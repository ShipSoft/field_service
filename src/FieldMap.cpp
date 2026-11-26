// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "SHiP/FieldService/FieldMap.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <mp-units/math.h>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace SHiP {

std::string resolveFieldMapPath(std::string const& path) {
    if (std::filesystem::exists(path)) {
        return std::filesystem::canonical(path).string();
    }
    auto const bare = std::filesystem::path(path).filename();
    if (auto const* root = std::getenv("SHIPFIELD_ROOT")) {
        auto const resolved = std::filesystem::path(root) / "share" / "field" / bare;
        if (std::filesystem::exists(resolved)) {
            return std::filesystem::canonical(resolved).string();
        }
    }
    throw std::runtime_error("Cannot locate field map '" + path +
                             "'; set SHIPFIELD_ROOT or provide an absolute path");
}

std::size_t FieldMapGrid::size() const {
    return static_cast<std::size_t>(n[0]) * n[1] * n[2];
}

namespace {

template <typename View, typename Values>
View view_of(FieldMapGrid const& grid, Values& values) {
    if (values.size() != grid.size()) {
        throw std::invalid_argument(std::format("FieldMapGrid::view: {} values for a grid of {}",
                                                values.size(), grid.size()));
    }
    return View{values.data(), grid.n[0], grid.n[1], grid.n[2]};
}

}  // namespace

FieldMapView FieldMapGrid::view(std::vector<float>& values) const {
    return view_of<FieldMapView>(*this, values);
}

ConstFieldMapView FieldMapGrid::view(std::vector<float> const& values) const {
    return view_of<ConstFieldMapView>(*this, values);
}

Length FieldMapGrid::node(std::size_t axis, std::size_t i) const {
    return min[axis] +
           static_cast<double>(i) * (max[axis] - min[axis]) / static_cast<double>(n[axis] - 1);
}

FieldMapSymmetry FieldMapSymmetry::quadrantDipole() {
    using enum Parity;
    FieldMapSymmetry s;
    s.mirror = {true, true, false};
    s.parity = {Row{odd, even, even}, Row{odd, even, odd}, kEven};
    return s;
}

namespace {

/// Check each axis of `map`'s grid; the total node count if they are fine.
std::expected<std::size_t, std::string> checkAxes(FieldMap const& map) {
    auto const& g = map.grid;
    std::size_t total = 1;
    for (auto const [axis, lo, hi, n, mirrored] :
         std::views::zip(std::string_view{"xyz"}, g.min, g.max, g.n, map.symmetry.mirror)) {
        if (n < 2) {
            return std::unexpected{std::format("axis {} needs at least 2 nodes", axis)};
        }
        if (!isfinite(lo) || !isfinite(hi) || !(hi > lo)) {
            return std::unexpected{std::format("axis {} needs finite bounds with max > min", axis)};
        }
        if (mirrored && lo != Length::zero()) {
            return std::unexpected{std::format("mirrored axis {} must start at 0", axis)};
        }
        if (total > std::numeric_limits<std::size_t>::max() / n) {
            return std::unexpected{"node count overflows"};
        }
        total *= n;
    }
    return total;
}

std::expected<void, std::string> checkParity(FieldMap const& map) {
    // Parity has a fixed underlying type, so any int8 value casts to it.
    for (auto const p : map.symmetry.parity | std::views::join) {
        if (p != Parity::odd && p != Parity::even) {
            return std::unexpected{"parity entries must be odd or even"};
        }
    }
    return {};
}

std::expected<void, std::string> checkFinite(char component, std::vector<float> const& values) {
    for (auto const [i, v] : std::views::enumerate(values)) {
        if (!std::isfinite(v)) {
            return std::unexpected{std::format("B{}[{}] is not finite", component, i)};
        }
    }
    return {};
}

/// Check that each component has `total` finite values.
std::expected<void, std::string> checkValues(FieldMap const& map, std::size_t total) {
    if (map.bx.size() != total || map.by.size() != total || map.bz.size() != total) {
        return std::unexpected{std::format("expected {} values per component, got {}/{}/{}", total,
                                           map.bx.size(), map.by.size(), map.bz.size())};
    }
    return checkFinite('x', map.bx)
        .and_then([&map] { return checkFinite('y', map.by); })
        .and_then([&map] { return checkFinite('z', map.bz); });
}

}  // namespace

std::expected<void, std::string> checkFieldMap(FieldMap const& map) {
    auto const named = [&map](std::string const& what) {
        return "field map '" + map.name + "': " + what;
    };
    if (map.name.empty()) {
        return std::unexpected{named("name must not be empty")};
    }
    return checkAxes(map)
        .and_then([&map](std::size_t total) {
            return checkParity(map).transform([total] { return total; });
        })
        .and_then([&map](std::size_t total) { return checkValues(map, total); })
        .transform_error(named);
}

void validate(FieldMap const& map) {
    if (auto const ok = checkFieldMap(map); !ok) {
        throw std::invalid_argument(ok.error());
    }
}

bool FieldBox::contains(Length x, Length y, Length z) const {
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

class TranslatedEvaluator final : public IFieldEvaluator {
   public:
    TranslatedEvaluator(std::shared_ptr<IFieldEvaluator> inner, Vec3<Length> const& offset)
        : inner_{std::move(inner)}, offset_{offset} {}

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        return inner_->at(x - offset_[0], y - offset_[1], z - offset_[2]);
    }

   private:
    std::shared_ptr<IFieldEvaluator> inner_;
    Vec3<Length> offset_;
};

class ZeroOutsideEvaluator final : public IFieldEvaluator {
   public:
    ZeroOutsideEvaluator(std::shared_ptr<IFieldEvaluator> inner, FieldBox const& box)
        : inner_{std::move(inner)}, box_{box} {}

    [[nodiscard]] std::array<field_q, 3> at(pos_q x, pos_q y, pos_q z) const override {
        using mp_units::si::tesla;
        if (box_.contains(x, y, z)) {
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
                                            Vec3<Length> const& offset) {
    if (!inner) {
        throw std::invalid_argument("translated: null evaluator");
    }
    for (auto const v : offset) {
        if (!isfinite(v)) {
            throw std::invalid_argument("translated: offset must be finite");
        }
    }
    if (offset == Vec3<Length>{}) {
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

}  // namespace SHiP
