// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/CovfieFieldSource.h"

#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/linear.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ship {

namespace {

/// Canonical covfie chain for SHiP field maps: affine(mm→grid) ∘ trilinear ∘
/// strided ∘ float3 array. The converter (`fairship_to_cvf`) writes the same
/// chain so the binary format is symmetric.
using field_t = covfie::field<covfie::backend::affine<covfie::backend::linear<
    covfie::backend::strided<covfie::vector::size3,
                             covfie::backend::array<covfie::vector::float3>>>>>;

/// Filename resolution: bare names against `$SHIPFIELD_ROOT/share/field/`.
[[nodiscard]] std::string resolve_cvf_path(std::string const& path) {
    if (std::filesystem::exists(path)) return path;
    auto bare = std::filesystem::path(path).filename();
    if (auto const* root = std::getenv("SHIPFIELD_ROOT")) {
        auto resolved = std::filesystem::path(root) / "share" / "field" / bare;
        if (std::filesystem::exists(resolved)) return resolved.string();
    }
    throw std::runtime_error(
        "Cannot locate field map '" + path +
        "'; set SHIPFIELD_ROOT or provide an absolute path");
}

class CovfieEvaluator final : public IFieldEvaluator {
   public:
    explicit CovfieEvaluator(field_t field) : field_{std::move(field)} {}

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using namespace mp_units::si;
        // covfie views are thin handles around the backend; constructing per
        // call is cheap and avoids the thread_local-with-per-instance-state
        // pitfall. Field is immutable post-load, so this is thread-safe.
        field_t::view_t view{field_};
        auto const v = view.at(static_cast<float>(x.numerical_value_in(milli<metre>)),
                               static_cast<float>(y.numerical_value_in(milli<metre>)),
                               static_cast<float>(z.numerical_value_in(milli<metre>)));
        return {v[0] * tesla, v[1] * tesla, v[2] * tesla};
    }

   private:
    field_t field_;
};

}  // namespace

std::shared_ptr<IFieldEvaluator> loadCovfieField(std::string const& cvf_path) {
    auto resolved = resolve_cvf_path(cvf_path);
    std::ifstream is(resolved, std::ios::binary);
    if (!is.good())
        throw std::runtime_error("Failed to open field map: " + resolved);
    field_t field(is);
    return std::make_shared<CovfieEvaluator>(std::move(field));
}

CovfieFieldSource::CovfieFieldSource(std::vector<MagnetConfig> magnets) {
    regions_.reserve(magnets.size());
    for (auto& m : magnets) {
        FieldRegion r;
        r.name = std::move(m.name);
        r.volume_pattern = std::move(m.volume_pattern);
        r.field = loadCovfieField(m.cvf_file);
        regions_.push_back(std::move(r));
    }
}

}  // namespace ship
