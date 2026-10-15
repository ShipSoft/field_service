// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// ActsFieldAdapter: unit conversion, composition with the placement helpers,
// and failure on anomalous lookups.

#include "SHiP/FieldService/ActsFieldAdapter.h"
#include "SHiP/FieldService/FieldMap.h"
#include "reference_maps.h"

#include <Acts/Definitions/Units.hpp>
#include <Acts/MagneticField/MagneticFieldContext.hpp>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using Acts::UnitConstants::mm;
using Acts::UnitConstants::T;

class ThrowingEvaluator final : public SHiP::IFieldEvaluator {
   public:
    [[nodiscard]] std::array<field_q, 3> at(pos_q /*x*/, pos_q /*y*/, pos_q /*z*/) const override {
        throw std::runtime_error("boom");
    }
};

class NaNEvaluator final : public SHiP::IFieldEvaluator {
   public:
    [[nodiscard]] std::array<field_q, 3> at(pos_q /*x*/, pos_q /*y*/, pos_q /*z*/) const override {
        using mp_units::si::tesla;
        auto const nan = std::numeric_limits<double>::quiet_NaN();
        return {nan * tesla, 0.0 * tesla, 0.0 * tesla};
    }
};

void checkField(SHiP::ActsFieldAdapter const& adapter, Acts::Vector3 const& position,
                std::array<double, 3> const& expected_T) {
    using Catch::Matchers::WithinAbs;
    INFO("at (" << position.transpose() / mm << ") mm");
    auto cache = adapter.makeCache(Acts::MagneticFieldContext{});
    auto const B = adapter.getField(position, cache);
    REQUIRE(B.ok());
    CHECK_THAT((*B)[0] / T, WithinAbs(expected_T[0], 1e-4));
    CHECK_THAT((*B)[1] / T, WithinAbs(expected_T[1], 1e-4));
    CHECK_THAT((*B)[2] / T, WithinAbs(expected_T[2], 1e-4));
}

bool lookupFails(SHiP::ActsFieldAdapter const& adapter, Acts::Vector3 const& position) {
    auto cache = adapter.makeCache(Acts::MagneticFieldContext{});
    return !adapter.getField(position, cache).ok();
}

}  // namespace

TEST_CASE("ActsAdapter.ConvertsUnits", "[acts_adapter]") {
    SHiP::ActsFieldAdapter const adapter{SHiP::makeFieldEvaluator(SHiP::test::uniformMap())};
    checkField(adapter, Acts::Vector3{10.0 * mm, -20.0 * mm, 30.0 * mm}, {0.0, -1.5, 0.0});
    // ACTS lengths are not in mm internally; a position given in metres must
    // reach the evaluator as the same point.
    checkField(adapter, Acts::Vector3{0.5 * Acts::UnitConstants::m, 0.0, 0.0}, {0.0, -1.5, 0.0});
}

TEST_CASE("ActsAdapter.ComposesWithPlacement", "[acts_adapter]") {
    auto const map = SHiP::test::dipoleMap();
    SHiP::ActsFieldAdapter const adapter{SHiP::translated(
        SHiP::zeroOutside(SHiP::makeFieldEvaluator(map), SHiP::extent(map)), {0.0, 0.0, 5000.0})};
    checkField(adapter, Acts::Vector3{-30.0 * mm, 10.0 * mm, 5120.0 * mm},
               SHiP::test::dipoleField(-30.0, 10.0, 120.0));
    // Past the grid: no field, and the lookup still succeeds.
    checkField(adapter, Acts::Vector3{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0});
}

TEST_CASE("ActsAdapter.ReportsTheFirstFailureOnce", "[acts_adapter]") {
    int reports = 0;
    std::string last;
    SHiP::ActsFieldAdapter const adapter{std::make_shared<ThrowingEvaluator>(),
                                         [&](std::string_view what) {
                                             ++reports;
                                             last = what;
                                         }};
    CHECK(lookupFails(adapter, Acts::Vector3::Zero()));
    CHECK(lookupFails(adapter, Acts::Vector3::Zero()));
    CHECK(reports == 1);
    CHECK(last == "boom");
}

TEST_CASE("ActsAdapter.FailsNonFiniteLookups", "[acts_adapter]") {
    SHiP::ActsFieldAdapter const nan_field{std::make_shared<NaNEvaluator>()};
    CHECK(lookupFails(nan_field, Acts::Vector3::Zero()));

    SHiP::ActsFieldAdapter const uniform{SHiP::makeFieldEvaluator(SHiP::test::uniformMap())};
    auto const nan = std::numeric_limits<double>::quiet_NaN();
    CHECK(lookupFails(uniform, Acts::Vector3{0.0, nan, 0.0}));
}

TEST_CASE("ActsAdapter.RejectsNullEvaluator", "[acts_adapter]") {
    CHECK_THROWS_AS(SHiP::ActsFieldAdapter{nullptr}, std::invalid_argument);
}
