// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/CovfieFieldSource.h"
#include "detail/covfie_chains.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kN = 3;
constexpr float kMin = -10.0f;
constexpr float kMax = 10.0f;

using FillFn = std::function<std::array<float, 3>(float x, float y, float z)>;

/// Write a field over the [-10, 10]³ mm box to `path`, sampling `fill` at each
/// grid node. Uses the canonical writer chain (nearest_neighbour, so
/// `view.at(...)` yields an assignable lvalue) from detail/covfie_chains.h.
void WriteField(std::filesystem::path const& path, FillFn const& fill) {
    ship::detail::GridSpec const spec{{kMin, kMin, kMin}, {kMax, kMax, kMax}, {kN, kN, kN}};
    auto field = ship::detail::make_writer_field(spec);
    ship::detail::writer_field_t::view_t view(field);
    for (std::size_t ix = 0; ix < kN; ++ix) {
        float const x = ship::detail::grid_pos(kMin, kMax, kN, ix);
        for (std::size_t iy = 0; iy < kN; ++iy) {
            float const y = ship::detail::grid_pos(kMin, kMax, kN, iy);
            for (std::size_t iz = 0; iz < kN; ++iz) {
                float const z = ship::detail::grid_pos(kMin, kMax, kN, iz);
                auto& p = view.at(x, y, z);
                auto const B = fill(x, y, z);
                p[0] = B[0];
                p[1] = B[1];
                p[2] = B[2];
            }
        }
    }
    std::ofstream out(path, std::ios::binary);
    field.dump(out);
}

/// Temp .cvf path unique to a test, removed on scope exit (also on a Catch2
/// REQUIRE that throws out of the test body).
struct TempCvf {
    std::filesystem::path path;
    explicit TempCvf(char const* name)
        : path(std::filesystem::temp_directory_path() /
               (std::string("field_service_") + name + ".cvf")) {}
    ~TempCvf() { std::filesystem::remove(path); }
};

}  // namespace

TEST_CASE("CovfieFieldSource.RoundtripConstantField", "[field_source]") {
    TempCvf tmp("roundtrip_constant");
    WriteField(tmp.path, [](float, float, float) { return std::array{0.0f, 1.5f, 0.0f}; });
    auto eval = ship::loadCovfieField(tmp.path.string());
    REQUIRE(eval);

    using namespace mp_units::si;
    using Catch::Matchers::WithinAbs;

    auto B = eval->at(0.0 * milli<metre>, 0.0 * milli<metre>, 0.0 * milli<metre>);
    CHECK_THAT(B[0].numerical_value_in(tesla), WithinAbs(0.0, 1e-6));
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.5, 1e-6));
    CHECK_THAT(B[2].numerical_value_in(tesla), WithinAbs(0.0, 1e-6));

    // Off-centre — same constant field everywhere inside the box.
    B = eval->at(3.0 * milli<metre>, -4.0 * milli<metre>, 7.0 * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.5, 1e-6));

    // Exactly on the box corners — a valid query, must not read past the grid.
    B = eval->at(kMax * milli<metre>, kMax * milli<metre>, kMax * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.5, 1e-6));
    B = eval->at(kMin * milli<metre>, kMin * milli<metre>, kMin * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.5, 1e-6));
}

TEST_CASE("CovfieFieldSource.ClampsToBoundaryValue", "[field_source]") {
    TempCvf tmp("clamps_to_boundary");
    // By varies linearly with x so out-of-bounds garbage cannot masquerade as
    // the expected value: By(x) = 0.1 * x Tesla, i.e. -1 T .. +1 T.
    WriteField(tmp.path, [](float x, float, float) { return std::array{0.0f, 0.1f * x, 0.0f}; });
    auto eval = ship::loadCovfieField(tmp.path.string());
    REQUIRE(eval);

    using namespace mp_units::si;
    using Catch::Matchers::WithinAbs;

    // Trilinear interpolation reproduces a linear field exactly (up to float).
    auto B = eval->at(5.0 * milli<metre>, 0.0 * milli<metre>, 0.0 * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(0.5, 1e-5));

    // Outside the box the query clamps to the boundary-plane value.
    B = eval->at(50.0 * milli<metre>, 0.0 * milli<metre>, 0.0 * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.0, 1e-5));
    B = eval->at(-50.0 * milli<metre>, 0.0 * milli<metre>, 0.0 * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(-1.0, 1e-5));
    B = eval->at(100.0 * milli<metre>, 100.0 * milli<metre>, -100.0 * milli<metre>);
    CHECK_THAT(B[1].numerical_value_in(tesla), WithinAbs(1.0, 1e-5));
}

TEST_CASE("CovfieFieldSource.SourceAggregatesRegions", "[field_source]") {
    TempCvf tmp("aggregates_regions");
    WriteField(tmp.path, [](float, float, float) { return std::array{0.0f, 1.5f, 0.0f}; });
    std::vector<ship::CovfieFieldSource::MagnetConfig> magnets = {
        {"MagA", "MuonShield", tmp.path.string()},
        {"MagB", "Spectrometer", tmp.path.string()},
    };
    ship::CovfieFieldSource src(std::move(magnets));
    auto const& regs = src.regions();
    REQUIRE(regs.size() == 2u);
    CHECK(regs[0].name == "MagA");
    CHECK(regs[0].volume_pattern == "MuonShield");
    CHECK(regs[1].volume_pattern == "Spectrometer");
    REQUIRE(regs[0].field);
    REQUIRE(regs[1].field);
}
