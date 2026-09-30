// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Core (ROOT-free) tests: FieldMap validation and makeFieldEvaluator.

#include "FieldService/FieldMap.h"
#include "reference_maps.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <stdexcept>

namespace {

std::array<double, 3> evalAt(ship::IFieldEvaluator const& eval, double x, double y, double z) {
    using namespace mp_units::si::unit_symbols;
    auto const B = eval.at(x * mm, y * mm, z * mm);
    return {B[0].numerical_value_in(T), B[1].numerical_value_in(T), B[2].numerical_value_in(T)};
}

void checkField(ship::IFieldEvaluator const& eval, double x, double y, double z,
                std::array<double, 3> const& expected) {
    using Catch::Matchers::WithinAbs;
    INFO("at (" << x << ", " << y << ", " << z << ") mm");
    auto const B = evalAt(eval, x, y, z);
    CHECK_THAT(B[0], WithinAbs(expected[0], 1e-4));
    CHECK_THAT(B[1], WithinAbs(expected[1], 1e-4));
    CHECK_THAT(B[2], WithinAbs(expected[2], 1e-4));
}

}  // namespace

TEST_CASE("FieldMap.GridIndexIsZFastest", "[field_map]") {
    ship::FieldMapGrid const g{{0, 0, 0}, {1, 1, 1}, {2, 3, 4}};
    CHECK(g.size() == 24u);
    CHECK(g.index(0, 0, 1) == 1u);
    CHECK(g.index(0, 1, 0) == 4u);
    CHECK(g.index(1, 0, 0) == 12u);
    CHECK(g.node(2, 3) == 1.0);
}

TEST_CASE("FieldMap.ValidateRejectsMalformedMaps", "[field_map]") {
    auto m = ship::test::uniformMap();
    CHECK_NOTHROW(ship::validate(m));

    auto bad = m;
    bad.name.clear();
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.grid.n[1] = 1;
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.grid.max[2] = bad.grid.min[2];
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.by.pop_back();
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    // A mirrored axis must start at 0: the reflection plane is the origin.
    bad = m;
    bad.symmetry.mirror[0] = true;
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.symmetry.parity[4] = 0;
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.bx[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);

    bad = m;
    bad.bz.back() = std::numeric_limits<float>::infinity();
    CHECK_THROWS_AS(ship::validate(bad), std::invalid_argument);
}

TEST_CASE("FieldMap.EvaluatorInterpolatesAndClamps", "[field_map]") {
    // Without symmetry: a map of dipoleField over a box straddling the origin.
    ship::FieldMap m;
    m.name = "full";
    m.grid = {{-100, -50, -200}, {100, 50, 200}, {5, 5, 5}};
    for (std::size_t i = 0; i < 5; ++i)
        for (std::size_t j = 0; j < 5; ++j)
            for (std::size_t k = 0; k < 5; ++k) {
                auto const B = ship::test::dipoleField(m.grid.node(0, i), m.grid.node(1, j),
                                                       m.grid.node(2, k));
                m.bx.push_back(static_cast<float>(B[0]));
                m.by.push_back(static_cast<float>(B[1]));
                m.bz.push_back(static_cast<float>(B[2]));
            }
    auto const eval = ship::makeFieldEvaluator(m);
    REQUIRE(eval);

    // Between nodes: bilinear products are reproduced exactly.
    checkField(*eval, 12.5, -7.0, 33.0, ship::test::dipoleField(12.5, -7.0, 33.0));
    // On the corners, which must not read past the grid.
    checkField(*eval, 100, 50, 200, ship::test::dipoleField(100, 50, 200));
    checkField(*eval, -100, -50, -200, ship::test::dipoleField(-100, -50, -200));
    // Outside: clamped to the boundary value.
    checkField(*eval, 500, 0, 0, ship::test::dipoleField(100, 0, 0));
    checkField(*eval, 0, 0, -900, ship::test::dipoleField(0, 0, -200));
}

TEST_CASE("FieldMap.EvaluatorAppliesQuadrantSymmetry", "[field_map]") {
    auto const eval = ship::makeFieldEvaluator(ship::test::dipoleMap());
    REQUIRE(eval);
    // dipoleField has exactly the parities of quadrantDipole(), so the folded
    // evaluation must agree with the analytic field in all four quadrants.
    for (double const x : {-80.0, -30.0, 0.0, 30.0, 80.0})
        for (double const y : {-40.0, -10.0, 10.0, 40.0})
            for (double const z : {-150.0, 0.0, 120.0})
                checkField(*eval, x, y, z, ship::test::dipoleField(x, y, z));
}

TEST_CASE("FieldMap.EvaluatorOwnsItsData", "[field_map]") {
    std::shared_ptr<ship::IFieldEvaluator> eval;
    {
        auto m = ship::test::uniformMap();
        eval = ship::makeFieldEvaluator(m);
    }
    checkField(*eval, 1, 2, 3, {0.0, -1.5, 0.0});
}
