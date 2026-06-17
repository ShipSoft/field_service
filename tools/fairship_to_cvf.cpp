// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// fairship_to_cvf — convert FairShip's legacy ROOT-stored field-map format
// into a covfie .cvf binary using the canonical SHiP backend chain
// (affine ∘ linear ∘ strided ∘ float3-array).
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

#include <TFile.h>
#include <TTree.h>

#include <covfie/core/algebra/affine.hpp>
#include <covfie/core/backend/primitive/array.hpp>
#include <covfie/core/backend/transformer/affine.hpp>
#include <covfie/core/backend/transformer/strided.hpp>
#include <covfie/core/field.hpp>
#include <covfie/core/field_view.hpp>
#include <covfie/core/parameter_pack.hpp>
#include <covfie/core/vector.hpp>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

// Writer chain uses nearest_neighbour so `view.at(...)` returns an lvalue ref
// we can assign through during construction. The binary serialisation is
// identical to the linear-interpolating reader chain in CovfieFieldSource.
#include <covfie/core/backend/transformer/nearest_neighbour.hpp>
using field_t = covfie::field<
    covfie::backend::affine<covfie::backend::nearest_neighbour<
        covfie::backend::strided<covfie::vector::size3,
                                 covfie::backend::array<covfie::vector::float3>>>>>;

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

    auto* range_tree = in_file->Get<TTree>("Range");
    auto* data_tree = in_file->Get<TTree>("Data");
    if (!range_tree || !data_tree) {
        std::cerr << "Input file lacks Range or Data TTree\n";
        return 1;
    }

    // Read the single-entry Range tree.
    float xMin, xMax, dx, yMin, yMax, dy, zMin, zMax, dz;
    range_tree->SetBranchAddress("xMin", &xMin);
    range_tree->SetBranchAddress("xMax", &xMax);
    range_tree->SetBranchAddress("dx", &dx);
    range_tree->SetBranchAddress("yMin", &yMin);
    range_tree->SetBranchAddress("yMax", &yMax);
    range_tree->SetBranchAddress("dy", &dy);
    range_tree->SetBranchAddress("zMin", &zMin);
    range_tree->SetBranchAddress("zMax", &zMax);
    range_tree->SetBranchAddress("dz", &dz);
    range_tree->GetEntry(0);

    // FairShip stores positions in cm; convert to mm here so the rest of the
    // toolchain works in a single length unit.
    constexpr float kCmToMm = 10.0f;
    xMin *= kCmToMm;
    xMax *= kCmToMm;
    dx *= kCmToMm;
    yMin *= kCmToMm;
    yMax *= kCmToMm;
    dy *= kCmToMm;
    zMin *= kCmToMm;
    zMax *= kCmToMm;
    dz *= kCmToMm;

    auto const Nx = static_cast<std::size_t>(std::lround((xMax - xMin) / dx)) + 1;
    auto const Ny = static_cast<std::size_t>(std::lround((yMax - yMin) / dy)) + 1;
    auto const Nz = static_cast<std::size_t>(std::lround((zMax - zMin) / dz)) + 1;

    std::cout << "Grid: " << Nx << " x " << Ny << " x " << Nz << " samples\n"
              << "Range: x[" << xMin << ", " << xMax << "] y[" << yMin << ", " << yMax
              << "] z[" << zMin << ", " << zMax << "] (mm)\n";

    auto const expected_entries = static_cast<Long64_t>(Nx * Ny * Nz);
    if (data_tree->GetEntries() != expected_entries) {
        std::cerr << "Data tree has " << data_tree->GetEntries() << " entries, expected "
                  << expected_entries << '\n';
        return 1;
    }

    // Affine maps real-world (x, y, z) in mm to fractional grid indices.
    covfie::algebra::affine<3> translation =
        covfie::algebra::affine<3>::translation(-xMin, -yMin, -zMin);
    covfie::algebra::affine<3> scaling = covfie::algebra::affine<3>::scaling(
        static_cast<float>(Nx - 1) / (xMax - xMin),
        static_cast<float>(Ny - 1) / (yMax - yMin),
        static_cast<float>(Nz - 1) / (zMax - zMin));

    field_t field(covfie::make_parameter_pack(
        field_t::backend_t::configuration_t(scaling * translation),
        field_t::backend_t::backend_t::configuration_t{},
        field_t::backend_t::backend_t::backend_t::configuration_t{Nx, Ny, Nz}));
    field_t::view_t view(field);

    float Bx, By, Bz;
    data_tree->SetBranchAddress("Bx", &Bx);
    data_tree->SetBranchAddress("By", &By);
    data_tree->SetBranchAddress("Bz", &Bz);

    // FairShip binning: (iX * Ny + iY) * Nz + iZ
    Long64_t entry = 0;
    for (std::size_t ix = 0; ix < Nx; ++ix) {
        float const x = xMin + ix * dx;
        for (std::size_t iy = 0; iy < Ny; ++iy) {
            float const y = yMin + iy * dy;
            for (std::size_t iz = 0; iz < Nz; ++iz) {
                float const z = zMin + iz * dz;
                data_tree->GetEntry(entry++);
                auto& p = view.at(x, y, z);
                p[0] = Bx;
                p[1] = By;
                p[2] = Bz;
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
