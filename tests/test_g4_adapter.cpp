// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// G4MagFieldAdapter: unit conversion and composition with the placement
// helpers. Also the only place this repository compiles the adapter header.

#include "SHiP/FieldService/FieldMap.h"
#include "SHiP/FieldService/G4MagFieldAdapter.h"
#include "reference_maps.h"

#include <CLHEP/Units/SystemOfUnits.h>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace {

void checkField(SHiP::G4MagFieldAdapter const& adapter, std::array<double, 3> const& position,
                std::array<double, 3> const& expected_T) {
    using Catch::Matchers::WithinAbs;
    INFO("at (" << position[0] / CLHEP::mm << ", " << position[1] / CLHEP::mm << ", "
                << position[2] / CLHEP::mm << ") mm");
    G4double const point[4]{position[0], position[1], position[2], 0.0};
    G4double B[3]{};
    adapter.GetFieldValue(point, B);
    CHECK_THAT(B[0] / CLHEP::tesla, WithinAbs(expected_T[0], 1e-4));
    CHECK_THAT(B[1] / CLHEP::tesla, WithinAbs(expected_T[1], 1e-4));
    CHECK_THAT(B[2] / CLHEP::tesla, WithinAbs(expected_T[2], 1e-4));
}

}  // namespace

TEST_CASE("G4Adapter.ConvertsUnits", "[g4_adapter]") {
    SHiP::G4MagFieldAdapter const adapter{SHiP::makeFieldEvaluator(SHiP::test::uniformMap())};
    checkField(adapter, {10.0 * CLHEP::mm, -20.0 * CLHEP::mm, 30.0 * CLHEP::mm}, {0.0, -1.5, 0.0});
    // CLHEP's tesla is not 1; the field must come back in Geant4 units.
    G4double const origin[4]{};
    G4double B[3]{};
    adapter.GetFieldValue(origin, B);
    CHECK_THAT(B[1], Catch::Matchers::WithinRel(-1.5 * CLHEP::tesla, 1e-6));
}

TEST_CASE("G4Adapter.ComposesWithPlacement", "[g4_adapter]") {
    auto const map = SHiP::test::dipoleMap();
    SHiP::G4MagFieldAdapter const adapter{SHiP::translated(
        SHiP::zeroOutside(SHiP::makeFieldEvaluator(map), SHiP::extent(map)), {0.0, 0.0, 5000.0})};
    // Positions in metres must reach the evaluator as the same point in mm.
    checkField(adapter, {-0.03 * CLHEP::m, 0.01 * CLHEP::m, 5.12 * CLHEP::m},
               SHiP::test::dipoleField(-30.0, 10.0, 120.0));
    checkField(adapter, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0});
}
