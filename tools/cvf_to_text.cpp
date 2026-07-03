// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// cvf_to_text — evaluate a covfie .cvf field map at sample points read from
// stdin and write whitespace-separated `x y z Bx By Bz` lines for human
// inspection and closure tests.
//
// The map is loaded with the canonical SHiP reader chain (trilinear
// interpolation, clamped to the mapped box; see src/detail/covfie_chains.h),
// so the output matches what CovfieFieldSource returns at the same points.

#include "detail/covfie_chains.h"

#include <fstream>
#include <iostream>

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.cvf> [output.txt]\n"
              << "Reads sample points from stdin as '<x> <y> <z>' (mm), one per\n"
              << "line, and writes 'x y z Bx By Bz' (mm, Tesla) to output.txt,\n"
              << "or to stdout if output.txt is omitted.\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }

    std::ifstream is(argv[1], std::ios::binary);
    if (!is.good()) {
        std::cerr << "Failed to open " << argv[1] << '\n';
        return 1;
    }

    ship::detail::reader_field_t field(is);
    ship::detail::reader_field_t::view_t view{field};

    std::ofstream of;
    std::ostream* os = &std::cout;
    if (argc == 3) {
        of.open(argv[2]);
        if (!of.good()) {
            std::cerr << "Failed to open output " << argv[2] << '\n';
            return 1;
        }
        os = &of;
    }

    std::cerr << "Reading sample points from stdin: x y z (mm), one per line.\n"
              << "Output: x y z Bx By Bz (mm, Tesla)\n";
    float x, y, z;
    while (std::cin >> x >> y >> z) {
        auto v = view.at(x, y, z);
        *os << x << ' ' << y << ' ' << z << ' ' << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';
    }
    return 0;
}
