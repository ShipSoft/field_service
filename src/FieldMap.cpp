// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/FieldMap.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    auto const requireFinite = [&fail](char component, std::vector<float> const& values) {
        for (std::size_t i = 0; i < values.size(); ++i)
            if (!std::isfinite(values[i]))
                fail(std::string("B") + component + "[" + std::to_string(i) + "] is not finite");
    };
    requireFinite('x', map.bx);
    requireFinite('y', map.by);
    requireFinite('z', map.bz);
}

bool FieldBox::contains(double x, double y, double z) const {
    // Written so that NaN compares false.
    return x >= min[0] && x <= max[0] && y >= min[1] && y <= max[1] && z >= min[2] && z <= max[2];
}

FieldBox extent(FieldMap const& map) {
    FieldBox box{.min = map.grid.min, .max = map.grid.max};
    for (std::size_t a = 0; a < 3; ++a) {
        if (map.symmetry.mirror.at(a)) {
            box.min.at(a) = -box.max.at(a);
        }
    }
    return box;
}

namespace {

using mp_units::si::metre;
using mp_units::si::milli;

class TranslatedEvaluator final : public IFieldEvaluator {
   public:
    TranslatedEvaluator(std::shared_ptr<IFieldEvaluator> inner, std::array<double, 3> const& offset)
        : inner_{std::move(inner)},
          offset_{offset[0] * milli<metre>, offset[1] * milli<metre>, offset[2] * milli<metre>} {}

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        return inner_->at(x - offset_[0], y - offset_[1], z - offset_[2]);
    }

   private:
    std::shared_ptr<IFieldEvaluator> inner_;
    std::array<pos_q, 3> offset_;
};

class ZeroOutsideEvaluator final : public IFieldEvaluator {
   public:
    ZeroOutsideEvaluator(std::shared_ptr<IFieldEvaluator> inner, FieldBox const& box)
        : inner_{std::move(inner)}, box_{box} {}

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using mp_units::si::tesla;
        if (box_.contains(x.numerical_value_in(milli<metre>), y.numerical_value_in(milli<metre>),
                          z.numerical_value_in(milli<metre>))) {
            return inner_->at(x, y, z);
        }
        return {0.0 * tesla, 0.0 * tesla, 0.0 * tesla};
    }

   private:
    std::shared_ptr<IFieldEvaluator> inner_;
    FieldBox box_;
};

}  // namespace

std::shared_ptr<IFieldEvaluator> translated(std::shared_ptr<IFieldEvaluator> inner,
                                            std::array<double, 3> const& offset) {
    if (!inner) {
        throw std::invalid_argument("translated: null evaluator");
    }
    for (auto const v : offset) {
        if (!std::isfinite(v)) {
            throw std::invalid_argument("translated: offset must be finite");
        }
    }
    if (offset == std::array<double, 3>{}) {
        return inner;
    }
    return std::make_shared<TranslatedEvaluator>(std::move(inner), offset);
}

std::shared_ptr<IFieldEvaluator> zeroOutside(std::shared_ptr<IFieldEvaluator> inner,
                                             FieldBox const& box) {
    if (!inner) {
        throw std::invalid_argument("zeroOutside: null evaluator");
    }
    return std::make_shared<ZeroOutsideEvaluator>(std::move(inner), box);
}

}  // namespace ship
