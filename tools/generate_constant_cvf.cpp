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

#include "detail/covfie_chains.h"

#include <array>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " <out.cvf> <Bx> <By> <Bz> <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>\n"
              << "       B in Tesla, positions in mm.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 11) {
        usage(argv[0]);
        return 1;
    }
    std::string const out_path = argv[1];
    std::array<float, 9> args{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        try {
            std::size_t consumed = 0;
            args[i] = std::stof(argv[i + 2], &consumed);
            if (argv[i + 2][consumed] != '\0')
                throw std::invalid_argument{"trailing characters"};
        } catch (std::exception const&) {
            std::cerr << "Invalid number: '" << argv[i + 2] << "'\n";
            usage(argv[0]);
            return 1;
        }
    }
    std::array<float, 3> const B{args[0], args[1], args[2]};
    std::array<float, 3> const min{args[3], args[5], args[7]};
    std::array<float, 3> const max{args[4], args[6], args[8]};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(max[i] > min[i])) {
            std::cerr << "Invalid bounds: need max > min on every axis\n";
            usage(argv[0]);
            return 1;
        }
    }

    constexpr std::size_t N = 2;
    auto field = ship::detail::make_writer_field({min, max, {N, N, N}});
    ship::detail::writer_field_t::view_t view(field);

    for (std::size_t ix = 0; ix < N; ++ix) {
        float const x = ship::detail::grid_pos(min[0], max[0], N, ix);
        for (std::size_t iy = 0; iy < N; ++iy) {
            float const y = ship::detail::grid_pos(min[1], max[1], N, iy);
            for (std::size_t iz = 0; iz < N; ++iz) {
                float const z = ship::detail::grid_pos(min[2], max[2], N, iz);
                auto& p = view.at(x, y, z);
                p[0] = B[0];
                p[1] = B[1];
                p[2] = B[2];
            }
        }
    }

    std::ofstream out(out_path, std::ios::binary);
    if (!out.good()) {
        std::cerr << "Failed to open " << out_path << '\n';
        return 1;
    }
    field.dump(out);
    std::cout << "Wrote " << out_path << " (Bx=" << B[0] << " By=" << B[1] << " Bz=" << B[2]
              << " T, box [" << min[0] << "," << max[0] << "] [" << min[1] << "," << max[1] << "] ["
              << min[2] << "," << max[2] << "] mm)\n";
    return 0;
}
