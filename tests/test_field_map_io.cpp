// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// MapIO tests: write/read round trips, rejection of files this reader must
// not interpret, and FieldMapSource.

#include "FieldService/FieldMapIO.h"
#include "reference_maps.h"

#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/// Temp file unique to a test, removed on scope exit.
struct TempFile {
    std::filesystem::path path;
    explicit TempFile(char const* name)
        : path(std::filesystem::temp_directory_path() /
               (std::string("field_service_") + name + ".root")) {}
    ~TempFile() { std::filesystem::remove(path); }
};

void requireEqual(ship::FieldMap const& a, ship::FieldMap const& b) {
    CHECK(a.name == b.name);
    CHECK(a.grid.min == b.grid.min);
    CHECK(a.grid.max == b.grid.max);
    CHECK(a.grid.n == b.grid.n);
    CHECK(a.symmetry.mirror == b.symmetry.mirror);
    CHECK(a.symmetry.parity == b.symmetry.parity);
    CHECK(a.bx == b.bx);
    CHECK(a.by == b.by);
    CHECK(a.bz == b.bz);
    CHECK(a.provenance.source == b.provenance.source);
    CHECK(a.provenance.comment == b.provenance.comment);
    CHECK(a.provenance.producer == b.provenance.producer);
    CHECK(a.provenance.created_utc == b.provenance.created_utc);
}

/// Values a hand-written file may deviate in.
struct RawOverrides {
    std::uint16_t format_major = ship::kFieldMapFormatMajor;
    std::string length_unit = "mm";
    std::string coordinate_system = "cartesian";
};

/// Write the uniform reference map with the v1 schema, bypassing
/// writeFieldMaps so the file can contain what it would refuse to write.
void writeRaw(std::filesystem::path const& path, RawOverrides const& o) {
    auto const m = ship::test::uniformMap();
    auto model = ROOT::RNTupleModel::Create();
    *model->MakeField<std::uint16_t>("format_major") = o.format_major;
    *model->MakeField<std::uint16_t>("format_minor") = 0;
    *model->MakeField<std::string>("name") = m.name;
    *model->MakeField<std::string>("coordinate_system") = o.coordinate_system;
    *model->MakeField<std::string>("length_unit") = o.length_unit;
    *model->MakeField<std::string>("field_unit") = "T";
    *model->MakeField<std::array<double, 3>>("axis_min") = m.grid.min;
    *model->MakeField<std::array<double, 3>>("axis_max") = m.grid.max;
    *model->MakeField<std::array<std::uint32_t, 3>>("axis_n") = m.grid.n;
    *model->MakeField<std::string>("index_order") = "xyz_z_fastest";
    *model->MakeField<std::vector<float>>("bx") = m.bx;
    *model->MakeField<std::vector<float>>("by") = m.by;
    *model->MakeField<std::vector<float>>("bz") = m.bz;
    *model->MakeField<std::array<bool, 3>>("mirror") = m.symmetry.mirror;
    *model->MakeField<std::array<std::int8_t, 9>>("parity") = m.symmetry.parity;
    *model->MakeField<std::string>("source") = "";
    *model->MakeField<std::string>("comment") = "";
    *model->MakeField<std::string>("producer") = "";
    *model->MakeField<std::string>("created_utc") = "";
    // A field this reader does not know, as a later minor version may add.
    *model->MakeField<float>("future_field") = 42.0f;
    auto writer =
        ROOT::RNTupleWriter::Recreate(std::move(model), ship::kFieldMapNTupleName, path.string());
    writer->Fill();
}

}  // namespace

TEST_CASE("FieldMapIO.RoundTripsSeveralMaps", "[field_map_io]") {
    TempFile tmp("roundtrip");
    auto const maps = ship::test::referenceMaps();
    ship::writeFieldMaps(tmp.path.string(), maps);

    auto const read = ship::readFieldMaps(tmp.path.string());
    REQUIRE(read.size() == maps.size());
    for (std::size_t i = 0; i < maps.size(); ++i)
        requireEqual(read[i], maps[i]);

    requireEqual(ship::readFieldMap(tmp.path.string(), "uniform"), maps[1]);
    CHECK_THROWS_AS(ship::readFieldMap(tmp.path.string(), "missing"), std::runtime_error);
}

TEST_CASE("FieldMapIO.FillsMissingProvenance", "[field_map_io]") {
    TempFile tmp("provenance");
    auto m = ship::test::uniformMap();
    m.provenance = {};
    ship::writeFieldMaps(tmp.path.string(), std::span{&m, 1});
    auto const read = ship::readFieldMap(tmp.path.string(), m.name);
    CHECK_THAT(read.provenance.producer, Catch::Matchers::StartsWith("SHiPFieldService "));
    CHECK(read.provenance.created_utc.size() == std::string("2026-09-29T00:00:00Z").size());
}

TEST_CASE("FieldMapIO.TruncatedValuesStayClose", "[field_map_io]") {
    TempFile tmp("truncated");
    auto const m = ship::test::dipoleMap();
    ship::writeFieldMaps(tmp.path.string(), std::span{&m, 1}, {.truncated_bits = 20});
    auto const read = ship::readFieldMap(tmp.path.string(), m.name);
    REQUIRE(read.by.size() == m.by.size());
    // 20 bits keep 11 of the 23 mantissa bits: relative error below 2^-11.
    for (std::size_t i = 0; i < m.by.size(); ++i)
        CHECK_THAT(read.by[i], Catch::Matchers::WithinRel(m.by[i], 1.0f / 2048.0f));
}

TEST_CASE("FieldMapIO.WriterRejectsBadInput", "[field_map_io]") {
    TempFile tmp("writer_rejects");
    std::vector<ship::FieldMap> dup{ship::test::uniformMap(), ship::test::uniformMap()};
    CHECK_THROWS_AS(ship::writeFieldMaps(tmp.path.string(), dup), std::invalid_argument);
    auto bad = ship::test::uniformMap();
    bad.bz.clear();
    CHECK_THROWS_AS(ship::writeFieldMaps(tmp.path.string(), std::span{&bad, 1}),
                    std::invalid_argument);
}

TEST_CASE("FieldMapIO.ReaderAcceptsUnknownFields", "[field_map_io]") {
    TempFile tmp("unknown_fields");
    writeRaw(tmp.path, {});
    auto const read = ship::readFieldMaps(tmp.path.string());
    REQUIRE(read.size() == 1u);
    CHECK(read[0].by == ship::test::uniformMap().by);
}

TEST_CASE("FieldMapIO.ReaderRejectsWhatItCannotInterpret", "[field_map_io]") {
    TempFile tmp("reader_rejects");
    using Catch::Matchers::ContainsSubstring;

    writeRaw(tmp.path, {.format_major = 2});
    CHECK_THROWS_WITH(ship::readFieldMaps(tmp.path.string()), ContainsSubstring("major version 2"));

    writeRaw(tmp.path, {.length_unit = "cm"});
    CHECK_THROWS_WITH(ship::readFieldMaps(tmp.path.string()), ContainsSubstring("length_unit"));

    writeRaw(tmp.path, {.coordinate_system = "cylindrical_rz"});
    CHECK_THROWS_WITH(ship::readFieldMaps(tmp.path.string()),
                      ContainsSubstring("coordinate_system"));

    CHECK_THROWS_AS(ship::readFieldMaps((tmp.path.parent_path() / "no_such_file.root").string()),
                    std::runtime_error);
}

TEST_CASE("FieldMapIO.SourceSharesMapsAndTranslates", "[field_map_io]") {
    TempFile tmp("source");
    auto const maps = ship::test::referenceMaps();
    ship::writeFieldMaps(tmp.path.string(), maps);

    ship::FieldMapSource src({
        {"MagA", "MuonShield", tmp.path.string(), "dipole_quadrant", {}},
        {"MagB", "Spectrometer", tmp.path.string(), "dipole_quadrant", {}},
        {"MagC", "Shifted", tmp.path.string(), "dipole_quadrant", {0.0, 0.0, 5000.0}},
    });
    auto const& regs = src.regions();
    REQUIRE(regs.size() == 3u);
    CHECK(regs[0].name == "MagA");
    CHECK(regs[1].volume_pattern == "Spectrometer");
    // Same (file, map): one evaluator, loaded once.
    CHECK(regs[0].field == regs[1].field);

    using namespace mp_units::si::unit_symbols;
    using Catch::Matchers::WithinAbs;
    auto const local = regs[0].field->at(-20.0 * mm, 10.0 * mm, 50.0 * mm);
    auto const global = regs[2].field->at(-20.0 * mm, 10.0 * mm, 5050.0 * mm);
    auto const expected = ship::test::dipoleField(-20.0, 10.0, 50.0);
    for (std::size_t c = 0; c < 3; ++c) {
        CHECK_THAT(local[c].numerical_value_in(T), WithinAbs(expected[c], 1e-4));
        CHECK_THAT(global[c].numerical_value_in(T), WithinAbs(expected[c], 1e-4));
    }
}
