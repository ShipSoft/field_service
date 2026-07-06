// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Canonical covfie backend chains for SHiP field maps. Internal — covfie is a
// PRIVATE dependency of the core library, so this header must not be included
// from installed public headers.
//
// The .cvf format is: affine (mm → fractional grid index) ∘ clamp (grid index
// to the mapped box) ∘ interpolator ∘ strided ∘ float3 array. The reader
// interpolates trilinearly; writers use nearest_neighbour instead so
// `view.at(...)` returns an assignable lvalue during construction. linear and
// nearest_neighbour serialise nothing of their own, so files written with the
// writer chain load with the reader chain.

#include <array>
#include <cmath>
#include <covfie/core/algebra/affine.hpp>
#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/clamp.hpp>
#include <covfie/core/backend/transformer/linear.hpp>
#include <covfie/core/backend/transformer/nearest_neighbour.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace ship::detail {

using stored_backend_t =
    covfie::backend::strided<covfie::vector::size3, covfie::backend::array<covfie::vector::float3>>;

using reader_field_t = covfie::field<
    covfie::backend::affine<covfie::backend::clamp<covfie::backend::linear<stored_backend_t>>>>;

using writer_field_t = covfie::field<covfie::backend::affine<
    covfie::backend::clamp<covfie::backend::nearest_neighbour<stored_backend_t>>>>;

/// Axis-aligned grid: `n[i]` samples spanning `[min[i], max[i]]` (mm).
struct GridSpec {
    std::array<float, 3> min;
    std::array<float, 3> max;
    std::array<std::size_t, 3> n;
};

/// Throw if `spec` cannot produce a well-formed grid: every axis needs at least
/// two samples (`n - 1` underflows for `n == 0` and divides by zero in
/// `grid_pos`/the affine scale for `n == 1`) and a positive extent (`max > min`,
/// otherwise the affine scale is non-positive or infinite).
inline void validate(GridSpec const& spec) {
    for (std::size_t i = 0; i < 3; ++i) {
        if (spec.n[i] < 2) {
            throw std::invalid_argument("GridSpec: axis " + std::to_string(i) +
                                        " needs at least 2 samples");
        }
        if (!(spec.max[i] > spec.min[i])) {
            throw std::invalid_argument("GridSpec: axis " + std::to_string(i) + " needs max > min");
        }
    }
}

/// Position of sample `i` on an axis with `n` samples spanning `[min, max]`.
/// Must be used by writers so fill positions land exactly on the grid indices
/// produced by the affine transform. `n >= 2` is a precondition (see
/// `validate`).
[[nodiscard]] inline float grid_pos(float min, float max, std::size_t n, std::size_t i) {
    return min + static_cast<float>(i) * (max - min) / static_cast<float>(n - 1);
}

/// Build an empty writer-chain field over `spec`, ready to be filled via
/// `writer_field_t::view_t::at(x, y, z)` at `grid_pos` sample positions.
[[nodiscard]] inline writer_field_t make_writer_field(GridSpec const& spec) {
    validate(spec);

    covfie::algebra::affine<3> translation =
        covfie::algebra::affine<3>::translation(-spec.min[0], -spec.min[1], -spec.min[2]);
    covfie::algebra::affine<3> scaling = covfie::algebra::affine<3>::scaling(
        static_cast<float>(spec.n[0] - 1) / (spec.max[0] - spec.min[0]),
        static_cast<float>(spec.n[1] - 1) / (spec.max[1] - spec.min[1]),
        static_cast<float>(spec.n[2] - 1) / (spec.max[2] - spec.min[2]));

    // Clamp fractional grid indices to the mapped box. The upper bound sits
    // one float ULP below n-1 because the trilinear interpolator always reads
    // cell i+1; nearest_neighbour still rounds it back to n-1, so writer
    // fills at the max corner are unaffected.
    using clamp_cfg_t = writer_field_t::backend_t::backend_t::configuration_t;
    clamp_cfg_t clamp_cfg{};
    for (std::size_t i = 0; i < 3; ++i) {
        clamp_cfg.min[i] = 0.0f;
        clamp_cfg.max[i] = std::nextafter(static_cast<float>(spec.n[i] - 1), 0.0f);
    }

    return writer_field_t(covfie::make_parameter_pack(
        writer_field_t::backend_t::configuration_t(scaling * translation), std::move(clamp_cfg),
        std::monostate{},
        writer_field_t::backend_t::backend_t::backend_t::backend_t::configuration_t{
            spec.n[0], spec.n[1], spec.n[2]}));
}

}  // namespace ship::detail
