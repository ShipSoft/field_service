// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Core (ROOT-free) tests: FieldMap validation and makeFieldEvaluator.

#include "SHiP/FieldService/FieldMap.h"
#include "reference_maps.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

std::array<double, 3> evalAt(SHiP::IFieldEvaluator const& eval, double x, double y, double z) {
    using namespace mp_units::si::unit_symbols;
    auto const B = eval.at(x * mm, y * mm, z * mm);
    return {B[0].numerical_value_in(T), B[1].numerical_value_in(T), B[2].numerical_value_in(T)};
}

void checkField(SHiP::IFieldEvaluator const& eval, double x, double y, double z,
                std::array<double, 3> const& expected) {
    using Catch::Matchers::WithinAbs;
    INFO("at (" << x << ", " << y << ", " << z << ") mm");
    auto const B = evalAt(eval, x, y, z);
    CHECK_THAT(B[0], WithinAbs(expected[0], 1e-4));
    CHECK_THAT(B[1], WithinAbs(expected[1], 1e-4));
    CHECK_THAT(B[2], WithinAbs(expected[2], 1e-4));
}

}  // namespace

TEST_CASE("FieldMap.GridViewIsZFastest", "[field_map]") {
    SHiP::FieldMapGrid const g{{0, 0, 0}, {1, 1, 1}, {2, 3, 4}};
    CHECK(g.size() == 24u);
    CHECK(g.node(2, 3) == 1.0);

    std::vector<float> values(g.size());
    auto const v = g.view(values);
    CHECK(v.extent(0) == 2u);
    CHECK(v.extent(1) == 3u);
    CHECK(v.extent(2) == 4u);
    CHECK(&v[0, 0, 1] == &values[1]);
    CHECK(&v[0, 1, 0] == &values[4]);
    CHECK(&v[1, 0, 0] == &values[12]);

    std::vector<float> wrong(g.size() - 1);
    CHECK_THROWS_AS(g.view(wrong), std::invalid_argument);
}

TEST_CASE("FieldMap.ValidateRejectsMalformedMaps", "[field_map]") {
    auto m = SHiP::test::uniformMap();
    CHECK_NOTHROW(SHiP::validate(m));

    auto bad = m;
    bad.name.clear();
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    bad.grid.n[1] = 1;
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    bad.grid.max[2] = bad.grid.min[2];
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    bad.by.pop_back();
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    // A mirrored axis must start at 0: the reflection plane is the origin.
    bad = m;
    bad.symmetry.mirror[0] = true;
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    // Deliberately out of range, as a corrupt file could deliver.
    bad.symmetry.parity[1][1] =
        static_cast<SHiP::Parity>(0);  // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    bad.bx[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);

    bad = m;
    bad.bz.back() = std::numeric_limits<float>::infinity();
    CHECK_THROWS_AS(SHiP::validate(bad), std::invalid_argument);
}

TEST_CASE("FieldMap.EvaluatorInterpolatesAndClamps", "[field_map]") {
    // Without symmetry: a map of dipoleField over a box straddling the origin.
    SHiP::FieldMap m;
    m.name = "full";
    m.grid = {{-100, -50, -200}, {100, 50, 200}, {5, 5, 5}};
    SHiP::test::sampleNodes(m, SHiP::test::dipoleField);
    auto const eval = SHiP::makeFieldEvaluator(m);
    REQUIRE(eval);

    // Between nodes: bilinear products are reproduced exactly.
    checkField(*eval, 12.5, -7.0, 33.0, SHiP::test::dipoleField(12.5, -7.0, 33.0));
    // On the corners, which must not read past the grid.
    checkField(*eval, 100, 50, 200, SHiP::test::dipoleField(100, 50, 200));
    checkField(*eval, -100, -50, -200, SHiP::test::dipoleField(-100, -50, -200));
    // Outside: clamped to the boundary value.
    checkField(*eval, 500, 0, 0, SHiP::test::dipoleField(100, 0, 0));
    checkField(*eval, 0, 0, -900, SHiP::test::dipoleField(0, 0, -200));
}

TEST_CASE("FieldMap.EvaluatorAppliesQuadrantSymmetry", "[field_map]") {
    auto const eval = SHiP::makeFieldEvaluator(SHiP::test::dipoleMap());
    REQUIRE(eval);
    // dipoleField has exactly the parities of quadrantDipole(), so the folded
    // evaluation must agree with the analytic field in all four quadrants.
    for (double const x : {-80.0, -30.0, 0.0, 30.0, 80.0})
        for (double const y : {-40.0, -10.0, 10.0, 40.0})
            for (double const z : {-150.0, 0.0, 120.0})
                checkField(*eval, x, y, z, SHiP::test::dipoleField(x, y, z));
}

TEST_CASE("FieldMap.EvaluatorOwnsItsData", "[field_map]") {
    std::shared_ptr<SHiP::IFieldEvaluator> eval;
    {
        auto m = SHiP::test::uniformMap();
        eval = SHiP::makeFieldEvaluator(m);
    }
    checkField(*eval, 1, 2, 3, {0.0, -1.5, 0.0});
}

TEST_CASE("FieldMap.ExtentCoversMirroredAxes", "[field_map]") {
    auto const plain = SHiP::extent(SHiP::test::uniformMap());
    CHECK(plain.min == std::array<double, 3>{-1000.0, -1000.0, -1000.0});
    CHECK(plain.max == std::array<double, 3>{1000.0, 1000.0, 1000.0});

    // dipoleMap stores x in [0, 100] and y in [0, 50], mirrored in both.
    auto const mirrored = SHiP::extent(SHiP::test::dipoleMap());
    CHECK(mirrored.min == std::array<double, 3>{-100.0, -50.0, -200.0});
    CHECK(mirrored.max == std::array<double, 3>{100.0, 50.0, 200.0});
}

TEST_CASE("FieldMap.BoxContainsItsBoundaryButNotNaN", "[field_map]") {
    SHiP::FieldBox const box{.min = {-1.0, -2.0, -3.0}, .max = {1.0, 2.0, 3.0}};
    CHECK(box.contains(0.0, 0.0, 0.0));
    CHECK(box.contains(1.0, -2.0, 3.0));
    CHECK_FALSE(box.contains(1.001, 0.0, 0.0));
    CHECK_FALSE(box.contains(0.0, 0.0, -3.001));
    auto const nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_FALSE(box.contains(nan, 0.0, 0.0));
    CHECK_FALSE(box.contains(0.0, 0.0, nan));
}

TEST_CASE("FieldMap.ZeroOutsideReplacesClamping", "[field_map]") {
    auto const map = SHiP::test::dipoleMap();
    auto const eval = SHiP::zeroOutside(SHiP::makeFieldEvaluator(map), SHiP::extent(map));
    // Inside, in every quadrant and on the boundary: the map's field.
    checkField(*eval, -80.0, 40.0, 120.0, SHiP::test::dipoleField(-80.0, 40.0, 120.0));
    checkField(*eval, 100.0, -50.0, -200.0, SHiP::test::dipoleField(100.0, -50.0, -200.0));
    // Outside, where the bare evaluator would clamp: 0.
    checkField(*eval, 100.5, 0.0, 0.0, {0.0, 0.0, 0.0});
    checkField(*eval, 0.0, -60.0, 0.0, {0.0, 0.0, 0.0});
    checkField(*eval, 0.0, 0.0, 900.0, {0.0, 0.0, 0.0});
    CHECK_THROWS_AS(SHiP::zeroOutside(nullptr, SHiP::extent(map)), std::invalid_argument);
}

TEST_CASE("FieldMap.TranslatedShiftsTheOrigin", "[field_map]") {
    auto const local = SHiP::makeFieldEvaluator(SHiP::test::dipoleMap());
    CHECK(SHiP::translated(local, {0.0, 0.0, 0.0}) == local);

    auto const placed = SHiP::translated(local, {10.0, -5.0, 5000.0});
    checkField(*placed, -10.0, 15.0, 5050.0, SHiP::test::dipoleField(-20.0, 20.0, 50.0));

    // Composed the way a consumer places a map with 0 outside it.
    auto const map = SHiP::test::dipoleMap();
    auto const bounded =
        SHiP::translated(SHiP::zeroOutside(local, SHiP::extent(map)), {0.0, 0.0, 5000.0});
    checkField(*bounded, 0.0, 0.0, 5100.0, SHiP::test::dipoleField(0.0, 0.0, 100.0));
    checkField(*bounded, 0.0, 0.0, 100.0, {0.0, 0.0, 0.0});

    auto const inf = std::numeric_limits<double>::infinity();
    CHECK_THROWS_AS(SHiP::translated(local, {0.0, inf, 0.0}), std::invalid_argument);
    CHECK_THROWS_AS(SHiP::translated(nullptr, {1.0, 0.0, 0.0}), std::invalid_argument);
}

TEST_CASE("FieldMap.EvaluatorReturnsNaNForNaNPositions", "[field_map]") {
    // A NaN coordinate passes the mirror fold and covfie's clamp unchanged, so
    // it must be caught before it becomes a grid index.
    auto const nan = std::numeric_limits<double>::quiet_NaN();
    for (auto const& map : {SHiP::test::uniformMap(), SHiP::test::dipoleMap()}) {
        INFO("map " << map.name);
        auto const eval = SHiP::makeFieldEvaluator(map);
        for (auto const& p :
             {std::array{nan, 0.0, 0.0}, std::array{0.0, -nan, 0.0}, std::array{0.0, 0.0, nan}}) {
            auto const B = evalAt(*eval, p[0], p[1], p[2]);
            CHECK(std::isnan(B[0]));
            CHECK(std::isnan(B[1]));
            CHECK(std::isnan(B[2]));
        }
    }
}
