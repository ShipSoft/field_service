// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// fairship_to_fieldmap — convert FairShip's legacy ROOT-stored field-map
// format into the SHiP field-map format (docs/field_map_format.md).
//
// FairShip format (see FairShip/field/README.md):
//   TTree "Range": single-entry tree with float branches
//                  xMin, xMax, dx, yMin, yMax, dy, zMin, zMax, dz   (cm)
//   TTree "Data":  one entry per grid sample with float branches
//                  Bx, By, Bz                                       (Tesla)
//   binning order: (iX * Ny + iY) * Nz + iZ
//
// The field-map format uses the same node order, so values are copied through
// unchanged; only positions are converted from cm to mm. FairShip keeps
// symmetry outside the file, so it has to be given on the command line.
// Placement is not part of the format; it belongs to the consumer's
// configuration.

#include "FieldService/FieldMap.h"
#include "FieldService/FieldMapIO.h"

#include <TFile.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <input.root> <output.root>\n"
              << "Options:\n"
              << "  --name NAME          map name (default: input file stem)\n"
              << "  --symmetry SYM       none (default) or quadrant_dipole (FairShip's\n"
              << "                       SymFieldMap / quadSymm maps)\n"
              << "  --comment TEXT       free-text provenance comment\n"
              << "  --truncate-bits N    store values with N bits (10-31) instead of 32\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string name;
    std::string symmetry = "none";
    std::string comment;
    ship::FieldMapWriteOptions options;
    std::array<std::string, 2> paths;
    std::size_t n_paths = 0;
    try {
        for (int i = 1; i < argc; ++i) {
            std::string_view const arg = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc)
                    throw std::invalid_argument(std::string(arg) + " needs a value");
                return argv[++i];
            };
            if (arg == "--name")
                name = value();
            else if (arg == "--symmetry")
                symmetry = value();
            else if (arg == "--comment")
                comment = value();
            else if (arg == "--truncate-bits")
                options.truncated_bits = static_cast<unsigned>(std::stoul(value()));
            else if (arg.starts_with("-") || n_paths == paths.size())
                throw std::invalid_argument("unexpected argument '" + std::string(arg) + "'");
            else
                paths[n_paths++] = arg;
        }
        if (symmetry != "none" && symmetry != "quadrant_dipole")
            throw std::invalid_argument("unknown symmetry '" + symmetry + "'");
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << '\n';
        usage(argv[0]);
        return 1;
    }
    if (n_paths != 2) {
        usage(argv[0]);
        return 1;
    }
    auto const& in_path = paths[0];
    auto const& out_path = paths[1];

    std::unique_ptr<TFile> in_file{TFile::Open(in_path.c_str(), "READ")};
    if (!in_file || in_file->IsZombie()) {
        std::cerr << "Failed to open ROOT file: " << in_path << '\n';
        return 1;
    }

    // Read the single-entry Range tree.
    TTreeReader range_reader("Range", in_file.get());
    TTreeReaderValue<float> rxMin(range_reader, "xMin"), rxMax(range_reader, "xMax"),
        rdx(range_reader, "dx");
    TTreeReaderValue<float> ryMin(range_reader, "yMin"), ryMax(range_reader, "yMax"),
        rdy(range_reader, "dy");
    TTreeReaderValue<float> rzMin(range_reader, "zMin"), rzMax(range_reader, "zMax"),
        rdz(range_reader, "dz");
    if (!range_reader.Next()) {
        std::cerr << "Input file lacks a readable Range TTree\n";
        return 1;
    }

    // FairShip stores positions in cm; the field-map format uses mm.
    constexpr double kCmToMm = 10.0;
    std::array<double, 3> const min{*rxMin * kCmToMm, *ryMin * kCmToMm, *rzMin * kCmToMm};
    std::array<double, 3> const max{*rxMax * kCmToMm, *ryMax * kCmToMm, *rzMax * kCmToMm};
    std::array<double, 3> const spacing{*rdx * kCmToMm, *rdy * kCmToMm, *rdz * kCmToMm};

    // The Range tree is defined to hold exactly one entry. Extra entries would
    // be silently ignored and could describe a grid inconsistent with the one
    // we build below, so reject them outright.
    if (range_reader.Next()) {
        std::cerr << "Range tree has more than one entry; expected exactly one\n";
        return 1;
    }

    ship::FieldMap map;
    map.name = name.empty() ? std::filesystem::path(in_path).stem().string() : name;
    map.grid.min = min;
    map.grid.max = max;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(spacing[i] > 0.0) || !(max[i] > min[i])) {
            std::cerr << "Invalid Range tree: need max > min and spacing > 0 on every axis\n";
            return 1;
        }
        auto const steps = std::lround((max[i] - min[i]) / spacing[i]);
        if (steps < 1 || steps >= std::numeric_limits<std::uint32_t>::max()) {
            std::cerr << "Invalid Range tree: axis " << i << " has an unusable sample count\n";
            return 1;
        }
        // The extent must be an integer number of steps: otherwise lround()
        // silently snaps the sample count and every grid position drifts off
        // the values the Data tree was sampled at.
        double const spanned = static_cast<double>(steps) * spacing[i];
        if (std::abs(spanned - (max[i] - min[i])) > 1e-2 * spacing[i]) {
            std::cerr << "Invalid Range tree: axis " << i << " extent " << (max[i] - min[i])
                      << " mm is not an integer multiple of spacing " << spacing[i] << " mm\n";
            return 1;
        }
        map.grid.n[i] = static_cast<std::uint32_t>(steps + 1);
    }
    if (symmetry == "quadrant_dipole")
        map.symmetry = ship::FieldMapSymmetry::quadrantDipole();

    auto const total = map.grid.size();
    std::cout << "Grid: " << map.grid.n[0] << " x " << map.grid.n[1] << " x " << map.grid.n[2]
              << " samples\n"
              << "Range: x[" << min[0] << ", " << max[0] << "] y[" << min[1] << ", " << max[1]
              << "] z[" << min[2] << ", " << max[2] << "] (mm)\n";

    TTreeReader data_reader("Data", in_file.get());
    TTreeReaderValue<float> Bx(data_reader, "Bx"), By(data_reader, "By"), Bz(data_reader, "Bz");
    if (data_reader.GetEntries() != static_cast<Long64_t>(total)) {
        std::cerr << "Data tree has " << data_reader.GetEntries() << " entries, expected " << total
                  << '\n';
        return 1;
    }
    map.bx.reserve(total);
    map.by.reserve(total);
    map.bz.reserve(total);
    while (data_reader.Next()) {
        map.bx.push_back(*Bx);
        map.by.push_back(*By);
        map.bz.push_back(*Bz);
    }
    if (map.bx.size() != total) {
        std::cerr << "Data tree ended prematurely\n";
        return 1;
    }

    map.provenance.source = std::filesystem::absolute(in_path).string();
    map.provenance.comment = comment;
    map.provenance.producer = "fairship_to_fieldmap " SHIPFIELD_VERSION;

    try {
        ship::writeFieldMaps(out_path, std::span{&map, 1}, options);
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Wrote map '" << map.name << "' to " << out_path << '\n';
    return 0;
}
