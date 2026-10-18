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

#include "SHiP/FieldService/FieldMap.h"
#include "SHiP/FieldService/FieldMapIO.h"

#include <cstdio>
#include <exception>
#include <iostream>
#include <mp-units/systems/si.h>
#include <print>
#include <ranges>
#include <string_view>

namespace {

void usage(const char* prog) {
    std::print(stderr,
               "Usage: {0} <file.root>          print metadata of every map\n"
               "       {0} <file.root> <map>    evaluate <map> at points\n"
               "In the second form, reads '<x> <y> <z>' (mm) lines from stdin and writes\n"
               "'x y z Bx By Bz' (mm, Tesla) to stdout.\n",
               prog);
}

void print(SHiP::FieldMap const& m) {
    std::println("map '{}'", m.name);
    for (auto const [a, axis] : std::views::enumerate(std::string_view{"xyz"})) {
        std::print("  {}: [{}, {}], {} nodes", axis, m.grid.min.at(a), m.grid.max.at(a),
                   m.grid.n.at(a));
        if (m.symmetry.mirror.at(a)) {
            auto const names = m.symmetry.parity.at(a) | std::views::transform([](SHiP::Parity p) {
                                   return p == SHiP::Parity::odd ? "odd" : "even";
                               });
            std::print(", mirrored, parity (Bx, By, Bz) = ({:n:})", names);
        }
        std::println();
    }
    std::println("  source:   {}", m.provenance.source);
    std::println("  comment:  {}", m.provenance.comment);
    std::println("  producer: {}", m.provenance.producer);
    std::println("  created:  {}", m.provenance.created_utc);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }
    try {
        if (argc == 2) {
            for (auto const& m : SHiP::readFieldMaps(argv[1]))
                print(m);
            return 0;
        }
        auto const eval = SHiP::makeFieldEvaluator(SHiP::readFieldMap(argv[1], argv[2]));
        using namespace mp_units::si::unit_symbols;
        double x, y, z;
        while (std::cin >> x >> y >> z) {
            auto const B = eval->at(x * mm, y * mm, z * mm);
            // Shortest round-trip form: the 6 significant digits iostreams
            // default to are too coarse for closure tests.
            std::println("{} {} {} {} {} {}", x, y, z, B[0].numerical_value_in(T),
                         B[1].numerical_value_in(T), B[2].numerical_value_in(T));
        }
    } catch (std::exception const& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }
    return 0;
}
