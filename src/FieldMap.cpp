// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/FieldMap.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace ship {

std::string resolveFieldMapPath(std::string const& path) {
    if (std::filesystem::exists(path))
        return std::filesystem::canonical(path).string();
    auto bare = std::filesystem::path(path).filename();
    if (auto const* root = std::getenv("SHIPFIELD_ROOT")) {
        auto resolved = std::filesystem::path(root) / "share" / "field" / bare;
        if (std::filesystem::exists(resolved))
            return std::filesystem::canonical(resolved).string();
    }
    throw std::runtime_error("Cannot locate field map '" + path +
                             "'; set SHIPFIELD_ROOT or provide an absolute path");
}

std::size_t FieldMapGrid::size() const {
    return static_cast<std::size_t>(n[0]) * n[1] * n[2];
}

double FieldMapGrid::node(std::size_t axis, std::size_t i) const {
    return min[axis] +
           static_cast<double>(i) * (max[axis] - min[axis]) / static_cast<double>(n[axis] - 1);
}

FieldMapSymmetry FieldMapSymmetry::quadrantDipole() {
    FieldMapSymmetry s;
    s.mirror = {true, true, false};
    // Row x: Bx odd, By even, Bz even. Row y: Bx odd, By even, Bz odd.
    s.parity = {-1, 1, 1, -1, 1, -1, 1, 1, 1};
    return s;
}

void validate(FieldMap const& map) {
    auto fail = [&map](std::string const& what) {
        throw std::invalid_argument("field map '" + map.name + "': " + what);
    };
    if (map.name.empty())
        fail("name must not be empty");
    auto const& g = map.grid;
    std::size_t total = 1;
    for (std::size_t a = 0; a < 3; ++a) {
        auto const axis = std::to_string(a);
        if (g.n[a] < 2)
            fail("axis " + axis + " needs at least 2 nodes");
        if (!std::isfinite(g.min[a]) || !std::isfinite(g.max[a]) || !(g.max[a] > g.min[a]))
            fail("axis " + axis + " needs finite bounds with max > min");
        if (map.symmetry.mirror[a] && g.min[a] != 0.0)
            fail("mirrored axis " + axis + " must start at 0");
        if (total > std::numeric_limits<std::size_t>::max() / g.n[a])
            fail("node count overflows");
        total *= g.n[a];
    }
    for (auto const p : map.symmetry.parity)
        if (p != 1 && p != -1)
            fail("parity entries must be +1 or -1");
    if (map.bx.size() != total || map.by.size() != total || map.bz.size() != total)
        fail("expected " + std::to_string(total) + " values per component, got " +
             std::to_string(map.bx.size()) + "/" + std::to_string(map.by.size()) + "/" +
             std::to_string(map.bz.size()));
}

}  // namespace ship
