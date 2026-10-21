// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Maps stored in the frozen reference files under tests/data/. The fields are
// products of at most one coordinate per axis, which trilinear interpolation
// reproduces exactly, so readers can be checked at arbitrary points and not
// only at nodes.

#include "SHiP/FieldService/FieldMap.h"

#include <array>
#include <cstddef>
#include <ranges>
#include <vector>

namespace SHiP::test {

/// (x, y, z) mm as lengths.
inline Vec3<Length> mm3(double x, double y, double z) {
    return {x * units::mm, y * units::mm, z * units::mm};
}

/// Quadrant-symmetric dipole-like field, defined for all (x, y, z) in mm:
/// Bx is odd in x and y, By even, Bz even in x and odd in y, matching
/// FieldMapSymmetry::quadrantDipole().
inline std::array<double, 3> dipoleField(double x, double y, double z) {
    return {1e-4 * x * y, 1.0 + 2e-3 * z, 1e-4 * y * z};
}

/// Fill `m`'s values with `field(x, y, z)` (mm -> T) sampled at every node.
template <typename Field>
void sampleNodes(FieldMap& m, Field const& field) {
    for (auto* values : {&m.bx, &m.by, &m.bz}) {
        values->assign(m.grid.size(), 0.0f);
    }
    auto const bx = m.grid.view(m.bx);
    auto const by = m.grid.view(m.by);
    auto const bz = m.grid.view(m.bz);
    auto const nodes = [&bx](std::size_t axis) { return std::views::iota(0uz, bx.extent(axis)); };
    auto const node_mm = [&m](std::size_t axis, std::size_t n) {
        return m.grid.node(axis, n).numerical_value_in(units::mm);
    };
    for (auto const [i, j, k] : std::views::cartesian_product(nodes(0), nodes(1), nodes(2))) {
        auto const B = field(node_mm(0, i), node_mm(1, j), node_mm(2, k));
        bx[i, j, k] = static_cast<float>(B[0]);
        by[i, j, k] = static_cast<float>(B[1]);
        bz[i, j, k] = static_cast<float>(B[2]);
    }
}

/// Stored quadrant of dipoleField: x in [0, 100], y in [0, 50], z in [-200, 200].
inline FieldMap dipoleMap() {
    FieldMap m;
    m.name = "dipole_quadrant";
    m.grid = {.min = mm3(0.0, 0.0, -200.0), .max = mm3(100.0, 50.0, 200.0), .n = {3, 3, 5}};
    m.symmetry = FieldMapSymmetry::quadrantDipole();
    sampleNodes(m, dipoleField);
    m.provenance = {"tests/reference_maps.h", "reference map for format 1.0",
                    "make_reference_field_map", "2026-09-29T00:00:00Z"};
    return m;
}

/// Uniform By = -1.5 T over [-1000, 1000]^3 mm, no symmetry.
inline FieldMap uniformMap() {
    FieldMap m;
    m.name = "uniform";
    m.grid = {
        .min = mm3(-1000.0, -1000.0, -1000.0), .max = mm3(1000.0, 1000.0, 1000.0), .n = {2, 2, 2}};
    m.bx.assign(8, 0.0f);
    m.by.assign(8, -1.5f);
    m.bz.assign(8, 0.0f);
    m.provenance = {"tests/reference_maps.h", "", "make_reference_field_map",
                    "2026-09-29T00:00:00Z"};
    return m;
}

inline std::vector<FieldMap> referenceMaps() {
    return {dipoleMap(), uniformMap()};
}

}  // namespace SHiP::test
