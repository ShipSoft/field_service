// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// fairship_to_cvf — convert FairShip's legacy ROOT-stored field-map format
// into a covfie .cvf binary using the canonical SHiP backend chain
// (see src/detail/covfie_chains.h).
//
// FairShip format (see FairShip/field/README.md):
//   TTree "Range": single-entry tree with float branches
//                  xMin, xMax, dx, yMin, yMax, dy, zMin, zMax, dz   (cm)
//   TTree "Data":  one entry per grid sample with float branches
//                  Bx, By, Bz                                       (Tesla)
//   binning order: (iX * Ny + iY) * Nz + iZ
//
// We convert positions to mm (so the resulting covfie field matches what
// CovfieFieldSource expects) and keep B in Tesla.

#include "detail/covfie_chains.h"

#include <TFile.h>
#include <TTreeReader.h>
#include <TTreeReaderValue.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

namespace {

void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.root> <output.cvf>\n";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        usage(argv[0]);
        return 1;
    }
    std::string const in_path = argv[1];
    std::string const out_path = argv[2];

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

    // FairShip stores positions in cm; convert to mm here so the rest of the
    // toolchain works in a single length unit.
    constexpr float kCmToMm = 10.0f;
    std::array<float, 3> const min{*rxMin * kCmToMm, *ryMin * kCmToMm, *rzMin * kCmToMm};
    std::array<float, 3> const max{*rxMax * kCmToMm, *ryMax * kCmToMm, *rzMax * kCmToMm};
    std::array<float, 3> const spacing{*rdx * kCmToMm, *rdy * kCmToMm, *rdz * kCmToMm};

    // The Range tree is defined to hold exactly one entry. Extra entries would
    // be silently ignored and could describe a grid inconsistent with the one
    // we build below, so reject them outright.
    if (range_reader.Next()) {
        std::cerr << "Range tree has more than one entry; expected exactly one\n";
        return 1;
    }

    std::array<std::size_t, 3> n{};
    for (std::size_t i = 0; i < 3; ++i) {
        if (!(spacing[i] > 0.0f) || !(max[i] > min[i])) {
            std::cerr << "Invalid Range tree: need max > min and spacing > 0 on every axis\n";
            return 1;
        }
        n[i] = static_cast<std::size_t>(std::lround((max[i] - min[i]) / spacing[i])) + 1;
        if (n[i] < 2) {
            std::cerr << "Invalid Range tree: need at least 2 samples per axis\n";
            return 1;
        }
        // The extent must be an integer number of steps: otherwise lround()
        // silently snaps the sample count and every grid position drifts off
        // the values the Data tree was sampled at.
        float const spanned = static_cast<float>(n[i] - 1) * spacing[i];
        if (std::abs(spanned - (max[i] - min[i])) > 1e-2f * spacing[i]) {
            std::cerr << "Invalid Range tree: axis " << i << " extent " << (max[i] - min[i])
                      << " mm is not an integer multiple of spacing " << spacing[i] << " mm\n";
            return 1;
        }
    }
    auto const Nx = n[0];
    auto const Ny = n[1];
    auto const Nz = n[2];

    std::cout << "Grid: " << Nx << " x " << Ny << " x " << Nz << " samples\n"
              << "Range: x[" << min[0] << ", " << max[0] << "] y[" << min[1] << ", " << max[1]
              << "] z[" << min[2] << ", " << max[2] << "] (mm)\n";

    TTreeReader data_reader("Data", in_file.get());
    TTreeReaderValue<float> Bx(data_reader, "Bx"), By(data_reader, "By"), Bz(data_reader, "Bz");
    // Guard the sample-count product against std::size_t overflow before it is
    // used for the entry-count check: a wrapped value could slip below the real
    // entry count and pass validation, or drive a huge allocation.
    std::size_t total_samples = 1;
    for (std::size_t const ni : n) {
        if (ni > std::numeric_limits<std::size_t>::max() / total_samples) {
            std::cerr << "Grid too large: sample count overflows std::size_t\n";
            return 1;
        }
        total_samples *= ni;
    }
    if (total_samples > static_cast<std::size_t>(std::numeric_limits<Long64_t>::max())) {
        std::cerr << "Grid too large: sample count exceeds Long64_t\n";
        return 1;
    }
    auto const expected_entries = static_cast<Long64_t>(total_samples);
    if (data_reader.GetEntries() != expected_entries) {
        std::cerr << "Data tree has " << data_reader.GetEntries() << " entries, expected "
                  << expected_entries << '\n';
        return 1;
    }

    auto field = ship::detail::make_writer_field({min, max, n});
    ship::detail::writer_field_t::view_t view(field);

    // FairShip binning: (iX * Ny + iY) * Nz + iZ — matches sequential reads.
    for (std::size_t ix = 0; ix < Nx; ++ix) {
        float const x = ship::detail::grid_pos(min[0], max[0], Nx, ix);
        for (std::size_t iy = 0; iy < Ny; ++iy) {
            float const y = ship::detail::grid_pos(min[1], max[1], Ny, iy);
            for (std::size_t iz = 0; iz < Nz; ++iz) {
                float const z = ship::detail::grid_pos(min[2], max[2], Nz, iz);
                if (!data_reader.Next()) {
                    std::cerr << "Data tree ended prematurely\n";
                    return 1;
                }
                auto& p = view.at(x, y, z);
                p[0] = *Bx;
                p[1] = *By;
                p[2] = *Bz;
            }
        }
    }

    std::ofstream out(out_path, std::ios::binary);
    if (!out.good()) {
        std::cerr << "Failed to open output: " << out_path << '\n';
        return 1;
    }
    field.dump(out);
    std::cout << "Wrote " << out_path << '\n';
    return 0;
}
