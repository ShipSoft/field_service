// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// In-memory form of a SHiP field map (format specified in
// docs/field_map_format.md). This header has no ROOT or covfie dependency:
// file I/O lives in the MapIO component (SHiP/FieldService/FieldMapIO.h), and
// evaluation is built from a FieldMap via makeFieldEvaluator().

#include "SHiP/FieldService/IFieldSource.h"

#include <SHiP/Units.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <mdspan/mdspan.hpp>
#include <memory>
#include <string>
#include <vector>

namespace SHiP {

/// One field component's node values, indexed `[i, j, k]`. The layout_right
/// mapping is the format's node order: x-major, z fastest.
/// kokkos/mdspan rather than std::mdspan, which libstdc++ only has from GCC 16.
using FieldMapView = Kokkos::mdspan<float, Kokkos::dextents<std::size_t, 3>>;
using ConstFieldMapView = Kokkos::mdspan<float const, Kokkos::dextents<std::size_t, 3>>;

/// Equidistant Cartesian grid in the map's local frame. Axis `a` has `n[a]`
/// nodes; node `i` sits at `min[a] + i * (max[a] - min[a]) / (n[a] - 1)`, so
/// both ends are nodes.
struct FieldMapGrid {
    Vec3<Length> min{};
    Vec3<Length> max{};
    std::array<std::uint32_t, 3> n{};

    /// Number of nodes, `n[0] * n[1] * n[2]`.
    [[nodiscard]] std::size_t size() const;
    /// `values`, one component with an entry per node, viewed as a 3D array
    /// over this grid. Throws `std::invalid_argument` unless `values` has
    /// `size()` entries. The view does not own the values.
    [[nodiscard]] FieldMapView view(std::vector<float>& values) const;
    [[nodiscard]] ConstFieldMapView view(std::vector<float> const& values) const;
    /// A view of a temporary would dangle.
    void view(std::vector<float>&& values) const = delete;
    /// Position of node `i` on axis `axis`.
    [[nodiscard]] Length node(std::size_t axis, std::size_t i) const;
};

/// How a field component behaves under a reflection. The value is the factor
/// the component picks up, as stored in the file format.
enum class Parity : std::int8_t { odd = -1, even = 1 };

/// Reflection symmetry. A mirrored axis stores only coordinates >= 0 (its
/// grid starts at 0); a query at a negative coordinate on that axis is
/// evaluated at the reflected point and component `c` takes the sign of
/// `parity[axis][c]`. Rows of non-mirrored axes are ignored.
struct FieldMapSymmetry {
    using Row = std::array<Parity, 3>;
    /// Indexed [mirrored axis][component].
    using Matrix = std::array<Row, 3>;
    static constexpr Row kEven{Parity::even, Parity::even, Parity::even};

    std::array<bool, 3> mirror{};
    Matrix parity{kEven, kEven, kEven};

    [[nodiscard]] bool any() const { return mirror[0] || mirror[1] || mirror[2]; }

    /// FairShip's quadrant-symmetric dipole (ShipBFieldMap with
    /// isSymmetric): mirrored in x and y; Bx flips sign under both
    /// reflections, Bz flips under y, By is even.
    [[nodiscard]] static FieldMapSymmetry quadrantDipole();
};

/// Where a map came from. Informational only; never affects evaluation.
struct FieldMapProvenance {
    std::string source;       ///< Origin file or generator.
    std::string comment;      ///< Free text.
    std::string producer;     ///< Writing tool and version.
    std::string created_utc;  ///< ISO 8601 timestamp.
};

/// One field map: grid, symmetry, and per-node field components in Tesla,
/// stored x-major with z fastest. `grid.view(bx)[i, j, k]` is Bx at node
/// (i, j, k).
struct FieldMap {
    std::string name;
    FieldMapGrid grid;
    FieldMapSymmetry symmetry;
    std::vector<float> bx;
    std::vector<float> by;
    std::vector<float> bz;
    FieldMapProvenance provenance;
};

/// Throw `std::invalid_argument` unless `map` is well formed: non-empty name,
/// at least two nodes and a positive extent per axis, value arrays of
/// `grid.size()` finite entries, mirrored axes starting at 0, parities of +-1.
void validate(FieldMap const& map);

/// Build a thread-safe evaluator for `map`: trilinear interpolation, clamped to
/// the boundary value outside the grid, with `map.symmetry` applied. The
/// returned evaluator owns a copy of the values, so `map` may be discarded.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> makeFieldEvaluator(FieldMap const& map);

/// Axis-aligned box.
struct FieldBox {
    Vec3<Length> min{};
    Vec3<Length> max{};

    /// True if (x, y, z) lies inside the box or on its boundary; false if
    /// any coordinate is NaN.
    [[nodiscard]] bool contains(Length x, Length y, Length z) const;
};

/// Region `map` covers in its local frame. A mirrored axis covers
/// [-max, max], since its grid stores only the half at or above 0.
[[nodiscard]] FieldBox extent(FieldMap const& map);

/// Evaluator for `inner` placed with its origin at `offset`: the field at
/// p is `inner`'s field at p - offset. Returns `inner` itself for a zero
/// offset. Throws `std::invalid_argument` if `inner` is null or `offset` is
/// not finite.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> translated(std::shared_ptr<IFieldEvaluator> inner,
                                                          Vec3<Length> const& offset);

/// Evaluator that returns `inner`'s field inside `box`, given in `inner`'s
/// frame, and 0 outside it, where map evaluators would clamp. Use
/// `extent(map)` as the box to get the FairShip `ShipBFieldMap` behaviour.
/// Throws `std::invalid_argument` if `inner` is null.
[[nodiscard]] std::shared_ptr<IFieldEvaluator> zeroOutside(std::shared_ptr<IFieldEvaluator> inner,
                                                           FieldBox const& box);

/// Canonical path of a map file. A path that does not exist as given is
/// looked up by its bare filename in `$SHIPFIELD_ROOT/share/field/`; throws
/// `std::runtime_error` if neither exists.
[[nodiscard]] std::string resolveFieldMapPath(std::string const& path);

}  // namespace SHiP
