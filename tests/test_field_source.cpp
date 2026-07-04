// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "FieldService/CovfieFieldSource.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <covfie/core/algebra/affine.hpp>
#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/nearest_neighbour.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>

// Writer chain uses nearest_neighbour so `view.at(...)` returns an lvalue ref
// we can assign through during construction. The file is binary-compatible
// with CovfieFieldSource's linear-interpolating reader chain.
using field_t = covfie::field<
    covfie::backend::affine<covfie::backend::nearest_neighbour<covfie::backend::strided<
        covfie::vector::size3, covfie::backend::array<covfie::vector::float3>>>>>;

namespace {

// Write a tiny constant field map to a temp file and return the path.
std::filesystem::path WriteTinyConstantField() {
    constexpr std::size_t N = 3;
    constexpr float xMin = -10.0f, xMax = 10.0f;
    constexpr float yMin = -10.0f, yMax = 10.0f;
    constexpr float zMin = -10.0f, zMax = 10.0f;

    auto translation = covfie::algebra::affine<3>::translation(-xMin, -yMin, -zMin);
    auto scaling = covfie::algebra::affine<3>::scaling(static_cast<float>(N - 1) / (xMax - xMin),
                                                       static_cast<float>(N - 1) / (yMax - yMin),
                                                       static_cast<float>(N - 1) / (zMax - zMin));

    field_t field(covfie::make_parameter_pack(
        field_t::backend_t::configuration_t(scaling * translation),
        field_t::backend_t::backend_t::configuration_t{},
        field_t::backend_t::backend_t::backend_t::configuration_t{N, N, N}));
    field_t::view_t view(field);
    for (std::size_t ix = 0; ix < N; ++ix) {
        float const x = xMin + ix * (xMax - xMin) / (N - 1);
        for (std::size_t iy = 0; iy < N; ++iy) {
            float const y = yMin + iy * (yMax - yMin) / (N - 1);
            for (std::size_t iz = 0; iz < N; ++iz) {
                float const z = zMin + iz * (zMax - zMin) / (N - 1);
                auto& p = view.at(x, y, z);
                p[0] = 0.0f;
                p[1] = 1.5f;
                p[2] = 0.0f;
            }
        }
    }

    auto path = std::filesystem::temp_directory_path() / "ship_field_service_tinyconst.cvf";
    std::ofstream out(path, std::ios::binary);
    field.dump(out);
    return path;
}

}  // namespace

TEST_CASE("CovfieFieldSource.RoundtripConstantField", "[field_source]") {
    auto path = WriteTinyConstantField();
    auto eval = ship::loadCovfieField(path.string());
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

    std::filesystem::remove(path);
}

TEST_CASE("CovfieFieldSource.SourceAggregatesRegions", "[field_source]") {
    auto path = WriteTinyConstantField();
    std::vector<ship::CovfieFieldSource::MagnetConfig> magnets = {
        {"MagA", "MuonShield", path.string()},
        {"MagB", "Spectrometer", path.string()},
    };
    ship::CovfieFieldSource src(std::move(magnets));
    auto const& regs = src.regions();
    REQUIRE(regs.size() == 2u);
    CHECK(regs[0].name == "MagA");
    CHECK(regs[0].volume_pattern == "MuonShield");
    CHECK(regs[1].volume_pattern == "Spectrometer");
    REQUIRE(regs[0].field);
    REQUIRE(regs[1].field);

    std::filesystem::remove(path);
}
