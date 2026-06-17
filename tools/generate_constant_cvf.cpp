// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// generate_constant_cvf — write a .cvf containing a spatially-constant
// magnetic field over a box. Useful for closure tests and aegir smoke runs.
//
// Usage:
//   generate_constant_cvf <output.cvf> <Bx> <By> <Bz> \
//                         <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>
//
// All B values in Tesla, all positions in mm. The grid is 2×2×2 (the smallest
// trilinear box); inside the bounds the field is constant {Bx, By, Bz}.

#include <covfie/core/algebra/affine.hpp>
#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/nearest_neighbour.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using field_t = covfie::field<
    covfie::backend::affine<covfie::backend::nearest_neighbour<
        covfie::backend::strided<covfie::vector::size3,
                                 covfie::backend::array<covfie::vector::float3>>>>>;

int main(int argc, char** argv) {
    if (argc != 11) {
        std::cerr << "Usage: " << argv[0]
                  << " <out.cvf> <Bx> <By> <Bz> <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>\n"
                  << "       B in Tesla, positions in mm.\n";
        return 1;
    }
    std::string const out_path = argv[1];
    float const Bx = std::stof(argv[2]);
    float const By = std::stof(argv[3]);
    float const Bz = std::stof(argv[4]);
    float const xMin = std::stof(argv[5]);
    float const xMax = std::stof(argv[6]);
    float const yMin = std::stof(argv[7]);
    float const yMax = std::stof(argv[8]);
    float const zMin = std::stof(argv[9]);
    float const zMax = std::stof(argv[10]);

    constexpr std::size_t N = 2;

    auto translation = covfie::algebra::affine<3>::translation(-xMin, -yMin, -zMin);
    auto scaling = covfie::algebra::affine<3>::scaling(
        static_cast<float>(N - 1) / (xMax - xMin),
        static_cast<float>(N - 1) / (yMax - yMin),
        static_cast<float>(N - 1) / (zMax - zMin));

    field_t field(covfie::make_parameter_pack(
        field_t::backend_t::configuration_t(scaling * translation),
        field_t::backend_t::backend_t::configuration_t{},
        field_t::backend_t::backend_t::backend_t::configuration_t{N, N, N}));
    field_t::view_t view(field);

    for (std::size_t ix = 0; ix < N; ++ix) {
        float const x = (ix == 0) ? xMin : xMax;
        for (std::size_t iy = 0; iy < N; ++iy) {
            float const y = (iy == 0) ? yMin : yMax;
            for (std::size_t iz = 0; iz < N; ++iz) {
                float const z = (iz == 0) ? zMin : zMax;
                auto& p = view.at(x, y, z);
                p[0] = Bx;
                p[1] = By;
                p[2] = Bz;
            }
        }
    }

    std::ofstream out(out_path, std::ios::binary);
    if (!out.good()) {
        std::cerr << "Failed to open " << out_path << '\n';
        return 1;
    }
    field.dump(out);
    std::cout << "Wrote " << out_path << " (Bx=" << Bx << " By=" << By << " Bz=" << Bz
              << " T, box ["
              << xMin << "," << xMax << "] [" << yMin << "," << yMax << "] [" << zMin << ","
              << zMax << "] mm)\n";
    return 0;
}
