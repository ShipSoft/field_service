// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// fieldmap_dump — inspect a field-map file (docs/field_map_format.md).
//
// Without a map name, prints each map's metadata. With one, reads sample
// points from stdin and writes whitespace-separated `x y z Bx By Bz` lines, as
// the library evaluator returns them (trilinear interpolation, symmetry
// applied, clamped at the boundary), for human inspection and closure tests.

#include "FieldService/FieldMap.h"
#include "FieldService/FieldMapIO.h"

#include <exception>
#include <iostream>
#include <mp-units/systems/si.h>
#include <string>

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <file.root>          print metadata of every map\n"
              << "       " << prog << " <file.root> <map>    evaluate <map> at points\n"
              << "In the second form, reads '<x> <y> <z>' (mm) lines from stdin and writes\n"
              << "'x y z Bx By Bz' (mm, Tesla) to stdout.\n";
}

void print(ship::FieldMap const& m) {
    static constexpr char const* kAxis = "xyz";
    std::cout << "map '" << m.name << "'\n";
    for (std::size_t a = 0; a < 3; ++a) {
        std::cout << "  " << kAxis[a] << ": [" << m.grid.min[a] << ", " << m.grid.max[a] << "] mm, "
                  << m.grid.n[a] << " nodes";
        if (m.symmetry.mirror[a])
            std::cout << ", mirrored, parity (" << int{m.symmetry.parity[3 * a]} << ", "
                      << int{m.symmetry.parity[3 * a + 1]} << ", "
                      << int{m.symmetry.parity[3 * a + 2]} << ")";
        std::cout << '\n';
    }
    std::cout << "  source:   " << m.provenance.source << '\n'
              << "  comment:  " << m.provenance.comment << '\n'
              << "  producer: " << m.provenance.producer << '\n'
              << "  created:  " << m.provenance.created_utc << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }
    try {
        if (argc == 2) {
            for (auto const& m : ship::readFieldMaps(argv[1]))
                print(m);
            return 0;
        }
        auto const eval = ship::makeFieldEvaluator(ship::readFieldMap(argv[1], argv[2]));
        using namespace mp_units::si::unit_symbols;
        double x, y, z;
        while (std::cin >> x >> y >> z) {
            auto const B = eval->at(x * mm, y * mm, z * mm);
            std::cout << x << ' ' << y << ' ' << z << ' ' << B[0].numerical_value_in(T) << ' '
                      << B[1].numerical_value_in(T) << ' ' << B[2].numerical_value_in(T) << '\n';
        }
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
