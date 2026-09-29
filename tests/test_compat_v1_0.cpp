// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Reads the frozen format-1.0 reference file with the current reader. If this
// fails, the change broke files that already exist: fix the reader, never the
// reference file.

#include "FieldService/FieldMapIO.h"
#include "reference_maps.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <string>

namespace {

std::string const kReference = SHIPFIELD_TEST_DATA_DIR "/field_map_v1.0.root";

}  // namespace

TEST_CASE("Compat.v1_0.MapsReadBackUnchanged", "[compat]") {
    auto const maps = ship::readFieldMaps(kReference);
    auto const expected = ship::test::referenceMaps();
    REQUIRE(maps.size() == expected.size());
    for (std::size_t i = 0; i < maps.size(); ++i) {
        auto const& m = maps[i];
        auto const& e = expected[i];
        CHECK(m.name == e.name);
        CHECK(m.grid.min == e.grid.min);
        CHECK(m.grid.max == e.grid.max);
        CHECK(m.grid.n == e.grid.n);
        CHECK(m.symmetry.mirror == e.symmetry.mirror);
        CHECK(m.symmetry.parity == e.symmetry.parity);
        CHECK(m.bx == e.bx);
        CHECK(m.by == e.by);
        CHECK(m.bz == e.bz);
        CHECK(m.provenance.source == e.provenance.source);
        CHECK(m.provenance.comment == e.provenance.comment);
    }
}

TEST_CASE("Compat.v1_0.EvaluatesAsSpecified", "[compat]") {
    auto const eval = ship::makeFieldEvaluator(ship::readFieldMap(kReference, "dipole_quadrant"));
    using namespace mp_units::si::unit_symbols;
    using Catch::Matchers::WithinAbs;
    for (double const x : {-75.0, 25.0})
        for (double const y : {-35.0, 5.0})
            for (double const z : {-110.0, 60.0}) {
                auto const B = eval->at(x * mm, y * mm, z * mm);
                auto const e = ship::test::dipoleField(x, y, z);
                for (std::size_t c = 0; c < 3; ++c)
                    CHECK_THAT(B[c].numerical_value_in(T), WithinAbs(e[c], 1e-4));
            }
}
