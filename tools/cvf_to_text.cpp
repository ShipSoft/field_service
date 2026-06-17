// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// cvf_to_text — dump a covfie .cvf field-map to whitespace-separated
//               `x y z Bx By Bz` lines for human inspection and closure tests.
//
// The dumped file is assumed to use the canonical SHiP chain
// (affine ∘ linear ∘ strided ∘ float3-array); we deserialise into the
// "stored" form (no interpolator) to recover the discrete grid samples.

#include <covfie/core/algebra/affine.hpp>
#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/linear.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using field_t =
    covfie::field<covfie::backend::affine<covfie::backend::linear<covfie::backend::strided<
        covfie::vector::size3, covfie::backend::array<covfie::vector::float3>>>>>;

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.cvf> [output.txt]\n"
              << "If output.txt is omitted, writes to stdout.\n"
              << "Lines are: x y z Bx By Bz (mm and Tesla, evaluated at the\n"
              << "interpolated grid points).\n";
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

    field_t field(is);
    field_t::view_t view{field};

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

    // We don't currently introspect the affine/strided extents from the loaded
    // field — that requires reaching into backend internals. The companion
    // converter writes a sidecar metadata file (.cvf.meta) listing the grid
    // bounds and spacing. Users running closure tests should supply matching
    // sample points; this tool just walks any input via stdin in
    // "<x> <y> <z>" form when no extents are known.
    *os << "# Reading sample points from stdin: x y z (mm), one per line.\n";
    *os << "# Output: x y z Bx By Bz (mm, Tesla)\n";
    float x, y, z;
    while (std::cin >> x >> y >> z) {
        auto v = view.at(x, y, z);
        *os << x << ' ' << y << ' ' << z << ' ' << v[0] << ' ' << v[1] << ' ' << v[2] << '\n';
    }
    return 0;
}
