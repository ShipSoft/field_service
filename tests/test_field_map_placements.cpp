// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Placement manifest tests: reading a manifest, resolving it and its map
// files, building a FieldMapSource from it, and rejecting malformed input.

#include "SHiP/FieldService/FieldMapIO.h"
#include "reference_maps.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

/// A share/field directory unique to a test and to this run, holding the
/// reference maps, removed on scope exit.
struct FieldDir {
    std::filesystem::path root;
    std::filesystem::path dir;
    explicit FieldDir(char const* name)
        : root(std::filesystem::temp_directory_path() /
               (std::string("field_service_placements_") + name + "_" +
                std::to_string(std::random_device{}()))),
          dir(root / "share" / "field") {
        std::filesystem::create_directories(dir);
        SHiP::writeFieldMaps((dir / "reference.root").string(), SHiP::test::referenceMaps());
    }
    FieldDir(FieldDir const&) = delete;
    FieldDir& operator=(FieldDir const&) = delete;
    FieldDir(FieldDir&&) = delete;
    FieldDir& operator=(FieldDir&&) = delete;
    ~FieldDir() { std::filesystem::remove_all(root); }

    [[nodiscard]] std::string write(std::string const& content,
                                    char const* file = "placements.json") const {
        auto const path = dir / file;
        std::ofstream{path} << content;
        return path.string();
    }
};

/// Points SHIPFIELD_ROOT at `root` and changes into `cwd`; restores both on
/// scope exit, also when the test throws.
class ProcessStateGuard {
   public:
    ProcessStateGuard(std::filesystem::path const& root, std::filesystem::path const& cwd)
        : cwd_(std::filesystem::current_path()) {
        if (auto const* old = std::getenv("SHIPFIELD_ROOT"))
            root_ = old;
        ::setenv("SHIPFIELD_ROOT", root.c_str(), 1);
        std::filesystem::current_path(cwd);
    }
    ProcessStateGuard(ProcessStateGuard const&) = delete;
    ProcessStateGuard& operator=(ProcessStateGuard const&) = delete;
    ProcessStateGuard(ProcessStateGuard&&) = delete;
    ProcessStateGuard& operator=(ProcessStateGuard&&) = delete;
    ~ProcessStateGuard() {
        std::error_code ec;
        std::filesystem::current_path(cwd_, ec);
        if (root_)
            ::setenv("SHIPFIELD_ROOT", root_->c_str(), 1);
        else
            ::unsetenv("SHIPFIELD_ROOT");
    }

   private:
    std::filesystem::path cwd_;
    std::optional<std::string> root_;
};

std::string const kManifest = R"({
  "format_version": 1,
  "geometry": {"package": "shipgeometry", "version": "0.2.1"},
  "maps": {
    "dipole_quadrant": {"file": "reference.root", "symmetry": "quadrant_dipole"},
    "uniform": {"file": "reference.root"}
  },
  "magnets": [
    {"name": "Shield", "volume_pattern": "/SHiP/muon_shield",
     "map": "dipole_quadrant", "translation": [0, 0, 2040]},
    {"name": "Spectrometer", "volume_pattern": "/SHiP/magnet", "map": "uniform",
     "future_key": "ignored"}
  ],
  "future_key": {"also": "ignored"}
})";

/// `kManifest` with the first magnet entry replaced by `magnet`.
std::string withMagnet(std::string const& magnet) {
    return R"({"format_version": 1,
               "maps": {"dipole_quadrant": {"file": "reference.root"}},
               "magnets": [)" +
           magnet + "]}";
}

}  // namespace

TEST_CASE("Placements.ReadsMagnetsAndGeometry", "[placements]") {
    FieldDir const fd("read");
    auto const p = SHiP::readFieldMapPlacements(fd.write(kManifest));

    CHECK(p.geometry_package == "shipgeometry");
    CHECK(p.geometry_version == "0.2.1");
    REQUIRE(p.magnets.size() == 2u);
    CHECK(p.magnets[0].name == "Shield");
    CHECK(p.magnets[0].volume_pattern == "/SHiP/muon_shield");
    CHECK(p.magnets[0].map == "dipole_quadrant");
    CHECK(p.magnets[0].translation == SHiP::test::mm3(0.0, 0.0, 2040.0));
    // Map files are taken from the manifest's directory.
    auto const expected_file = std::filesystem::canonical(fd.dir / "reference.root");
    CHECK(std::filesystem::path(p.magnets[0].file) == expected_file);
    // Omitted translation means the origins coincide.
    CHECK(p.magnets[1].translation == SHiP::test::mm3(0.0, 0.0, 0.0));
}

TEST_CASE("Placements.ResolvesBareNameUnderShipfieldRoot", "[placements]") {
    FieldDir const fd("bare");
    (void)fd.write(kManifest);
    // Run from elsewhere, so the bare name can only be found via the env var.
    ProcessStateGuard const guard(fd.root, fd.root);
    auto const p = SHiP::readFieldMapPlacements("placements.json");

    REQUIRE(p.magnets.size() == 2u);
    CHECK(std::filesystem::exists(p.magnets[0].file));
}

TEST_CASE("Placements.FeedFieldMapSource", "[placements]") {
    FieldDir const fd("source");
    SHiP::FieldMapSource const src{SHiP::readFieldMapPlacements(fd.write(kManifest)).magnets};
    auto const& regs = src.regions();
    REQUIRE(regs.size() == 2u);
    CHECK(regs[0].name == "Shield");

    using namespace mp_units::si::unit_symbols;
    using Catch::Matchers::WithinAbs;
    auto const b = regs[0].field->at(-20.0 * mm, 10.0 * mm, 2090.0 * mm);
    auto const expected = SHiP::test::dipoleField(-20.0, 10.0, 50.0);
    for (std::size_t c = 0; c < 3; ++c)
        CHECK_THAT(b[c].numerical_value_in(T), WithinAbs(expected[c], 1e-4));
}

TEST_CASE("Placements.RejectsMalformedManifests", "[placements]") {
    FieldDir const fd("reject");
    using Catch::Matchers::ContainsSubstring;
    auto const rejects = [&](std::string const& content, std::string const& what) {
        CHECK_THROWS_WITH(SHiP::readFieldMapPlacements(fd.write(content)), ContainsSubstring(what));
    };

    rejects("{ not json", "not valid JSON");
    rejects("[]", "top level must be an object");
    rejects(R"({"maps": {}, "magnets": []})", "format_version");
    rejects(R"({"format_version": 4294967297, "maps": {}, "magnets": [{}]})",
            "unsupported format_version 4294967297");
    rejects(R"({"format_version": 2, "maps": {}, "magnets": [{}]})",
            "unsupported format_version 2");
    rejects(R"({"format_version": 1, "magnets": [{}]})", "'maps'");
    rejects(R"({"format_version": 1, "maps": {}, "magnets": []})", "'magnets'");
    rejects(R"({"format_version": 1, "geometry": "0.2.1", "maps": {}, "magnets": [{}]})",
            "'geometry' must be an object");

    auto const magnet = [](std::string const& extra) {
        return R"({"name": "M", "volume_pattern": "/V", "map": "dipole_quadrant")" + extra + "}";
    };
    rejects(withMagnet(R"({"volume_pattern": "/V", "map": "dipole_quadrant"})"), "'name'");
    rejects(withMagnet(R"({"name": "", "volume_pattern": "/V", "map": "dipole_quadrant"})"),
            "'name'");
    rejects(withMagnet(R"({"name": "M", "map": "dipole_quadrant"})"), "'volume_pattern'");
    rejects(withMagnet(R"({"name": "M", "volume_pattern": "/V", "map": "nope"})"),
            "map 'nope' is not listed");
    rejects(withMagnet(magnet(R"(, "translation": [0, 0])")), "needs 3 numbers");
    rejects(withMagnet(magnet(R"(, "translation": [0, "0", 0])")), "finite numbers");
    rejects(withMagnet(magnet(R"(, "translation": 5)")), "needs 3 numbers");
    rejects(withMagnet(magnet("") + "," + magnet("")), "duplicate magnet name 'M'");
    rejects(R"({"format_version": 1, "maps": {"dipole_quadrant": {}},
                "magnets": [{"name": "M", "volume_pattern": "/V", "map": "dipole_quadrant"}]})",
            "maps.dipole_quadrant: 'file'");

    CHECK_THROWS_AS(SHiP::readFieldMapPlacements((fd.dir / "missing.json").string()),
                    std::runtime_error);
}
