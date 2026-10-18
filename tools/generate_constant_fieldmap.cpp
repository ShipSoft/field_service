// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// generate_constant_fieldmap — write a field-map file (docs/field_map_format.md)
// holding a spatially constant magnetic field over a box. Useful for closure
// tests and aegir smoke runs.
//
// Usage:
//   generate_constant_fieldmap <output.root> <name> <Bx> <By> <Bz> \
//                              <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>
//
// All B values in Tesla, all positions in mm. The grid is 2×2×2 (the smallest
// trilinear box); inside the bounds the field is constant {Bx, By, Bz}.

#include "SHiP/FieldService/FieldMap.h"
#include "SHiP/FieldService/FieldMapIO.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <print>
#include <span>
#include <stdexcept>
#include <string>

namespace {

void usage(const char* prog) {
    std::print(stderr,
               "Usage: {} <out.root> <name> <Bx> <By> <Bz> <xMin> <xMax> <yMin> <yMax> <zMin> "
               "<zMax>\n"
               "       B in Tesla, positions in mm.\n",
               prog);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 12) {
        usage(argv[0]);
        return 1;
    }
    std::string const out_path = argv[1];
    std::array<double, 9> args{};
    for (std::size_t i = 0; i < args.size(); ++i) {
        try {
            std::size_t consumed = 0;
            args[i] = std::stod(argv[i + 3], &consumed);
            if (argv[i + 3][consumed] != '\0')
                throw std::invalid_argument{"trailing characters"};
        } catch (std::exception const&) {
            std::println(stderr, "Invalid number: '{}'", argv[i + 3]);
            usage(argv[0]);
            return 1;
        }
    }

    SHiP::FieldMap map;
    map.name = argv[2];
    using SHiP::units::mm;
    map.grid.min = {args[3] * mm, args[5] * mm, args[7] * mm};
    map.grid.max = {args[4] * mm, args[6] * mm, args[8] * mm};
    map.grid.n = {2, 2, 2};
    auto const size = map.grid.size();
    map.bx.assign(size, static_cast<float>(args[0]));
    map.by.assign(size, static_cast<float>(args[1]));
    map.bz.assign(size, static_cast<float>(args[2]));
    map.provenance.source = "constant field";
    map.provenance.producer = "generate_constant_fieldmap " SHIPFIELD_VERSION;

    try {
        SHiP::writeFieldMaps(out_path, std::span{&map, 1});
    } catch (std::exception const& e) {
        std::println(stderr, "error: {}", e.what());
        usage(argv[0]);
        return 1;
    }
    std::println("Wrote map '{}' to {} (Bx={} By={} Bz={} T, box [{},{}] [{},{}] [{},{}] mm)",
                 map.name, out_path, args[0], args[1], args[2], args[3], args[4], args[5], args[6],
                 args[7], args[8]);
    return 0;
}
