// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Maps stored in the frozen reference files under tests/data/. The fields are
// products of at most one coordinate per axis, which trilinear interpolation
// reproduces exactly, so readers can be checked at arbitrary points and not
// only at nodes.

#include "FieldService/FieldMap.h"

#include <array>
#include <cstddef>
#include <vector>

namespace ship::test {

/// Quadrant-symmetric dipole-like field, defined for all (x, y, z) in mm:
/// Bx is odd in x and y, By even, Bz even in x and odd in y, matching
/// FieldMapSymmetry::quadrantDipole().
inline std::array<double, 3> dipoleField(double x, double y, double z) {
    return {1e-4 * x * y, 1.0 + 2e-3 * z, 1e-4 * y * z};
}

/// Stored quadrant of dipoleField: x in [0, 100], y in [0, 50], z in [-200, 200].
inline FieldMap dipoleMap() {
    FieldMap m;
    m.name = "dipole_quadrant";
    m.grid = {{0.0, 0.0, -200.0}, {100.0, 50.0, 200.0}, {3, 3, 5}};
    m.symmetry = FieldMapSymmetry::quadrantDipole();
    for (std::size_t i = 0; i < m.grid.n[0]; ++i)
        for (std::size_t j = 0; j < m.grid.n[1]; ++j)
            for (std::size_t k = 0; k < m.grid.n[2]; ++k) {
                auto const B = dipoleField(m.grid.node(0, i), m.grid.node(1, j), m.grid.node(2, k));
                m.bx.push_back(static_cast<float>(B[0]));
                m.by.push_back(static_cast<float>(B[1]));
                m.bz.push_back(static_cast<float>(B[2]));
            }
    m.provenance = {"tests/reference_maps.h", "reference map for format 1.0",
                    "make_reference_field_map", "2026-09-29T00:00:00Z"};
    return m;
}

/// Uniform By = -1.5 T over [-1000, 1000]^3 mm, no symmetry.
inline FieldMap uniformMap() {
    FieldMap m;
    m.name = "uniform";
    m.grid = {{-1000.0, -1000.0, -1000.0}, {1000.0, 1000.0, 1000.0}, {2, 2, 2}};
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

}  // namespace ship::test
