// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// plot_field_map — plot a covfie .cvf field map as ROOT histograms and PDFs.
//
// Usage:
//   plot_field_map <input.cvf> <out.root> <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>
//
// All positions in mm. Produces B_y vs z along the beam axis plus B_y in the
// xz-, yz- and central xy-planes, written to <out.root> with one PDF per plot
// alongside it. Plot ranges are command-line arguments because grid extents
// cannot be introspected from a loaded .cvf (see cvf_to_text.cpp).

#include "FieldService/CovfieFieldSource.h"
#include "FieldService/IFieldSource.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH2F.h>

#include <array>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <mp-units/systems/si.h>
#include <string>

namespace {

using ship::IFieldEvaluator;

/// B_y [T] at (x, y, z) [mm] through the public evaluator interface.
double byAt(IFieldEvaluator const& eval, double x_mm, double y_mm, double z_mm) {
    using namespace mp_units::si::unit_symbols;
    return eval.at(x_mm * mm, y_mm * mm, z_mm * mm)[1].numerical_value_in(T);
}

/// Fill a 2D histogram whose axes are in metres; `point_mm` maps a bin-centre
/// pair (X-axis value, Y-axis value) [mm] to the global (x, y, z) [mm] to query.
void fillBy(TH2F& h, IFieldEvaluator const& eval,
            std::function<std::array<double, 3>(double, double)> const& point_mm) {
    for (int iu = 1; iu <= h.GetNbinsX(); ++iu) {
        double const u_mm = h.GetXaxis()->GetBinCenter(iu) * 1000.0;
        for (int iv = 1; iv <= h.GetNbinsY(); ++iv) {
            double const v_mm = h.GetYaxis()->GetBinCenter(iv) * 1000.0;
            auto const p = point_mm(u_mm, v_mm);
            h.SetBinContent(iu, iv, byAt(eval, p[0], p[1], p[2]));
        }
    }
}

void usage(char const* prog) {
    std::cerr << "Usage: " << prog
              << " <input.cvf> <out.root> <xMin> <xMax> <yMin> <yMax> <zMin> <zMax>\n"
              << "       Positions in mm. Writes B_y plots to <out.root> and one PDF per\n"
              << "       plot next to it. Outside the mapped region the evaluator clamps\n"
              << "       to the boundary value — a plateau beyond the map bounds is a\n"
              << "       plotting artefact, not physical fringe field.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 9) {
        usage(argv[0]);
        return 1;
    }
    std::string const cvf_path = argv[1];
    std::string const out_path = argv[2];
    double xMin, xMax, yMin, yMax, zMin, zMax;
    try {
        xMin = std::stod(argv[3]);
        xMax = std::stod(argv[4]);
        yMin = std::stod(argv[5]);
        yMax = std::stod(argv[6]);
        zMin = std::stod(argv[7]);
        zMax = std::stod(argv[8]);
    } catch (std::exception const&) {
        usage(argv[0]);
        return 1;
    }
    if (xMin >= xMax || yMin >= yMax || zMin >= zMax) {
        std::cerr << "error: each min must be smaller than the corresponding max\n";
        return 1;
    }

    std::shared_ptr<IFieldEvaluator> eval;
    try {
        eval = ship::loadCovfieField(cvf_path);
    } catch (std::exception const& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }

    // ── 1D: By vs z at (x=0, y=0) ─────────────────────────────────────────
    constexpr int nZ1D = 1000;
    TGraph gBy_z(nZ1D);
    gBy_z.SetName("gBy_z");
    gBy_z.SetTitle("B_{y} along beam axis;z [m];B_{y} [T]");
    for (int i = 0; i < nZ1D; ++i) {
        double const z_mm = zMin + (i + 0.5) * (zMax - zMin) / nZ1D;
        gBy_z.SetPoint(i, z_mm / 1000.0, byAt(*eval, 0.0, 0.0, z_mm));
    }

    // ── 2D planes (axes in metres) ────────────────────────────────────────
    constexpr int nLong = 200, nTrans = 70;
    double const zC_mm = 0.5 * (zMin + zMax);

    TH2F hBy_xz("hBy_xz", "B_{y} in xz-plane (y=0);z [m];x [m];B_{y} [T]", nLong, zMin / 1000.0,
                zMax / 1000.0, nTrans, xMin / 1000.0, xMax / 1000.0);
    fillBy(hBy_xz, *eval, [](double z, double x) { return std::array{x, 0.0, z}; });

    TH2F hBy_yz("hBy_yz", "B_{y} in yz-plane (x=0);z [m];y [m];B_{y} [T]", nLong, zMin / 1000.0,
                zMax / 1000.0, nTrans, yMin / 1000.0, yMax / 1000.0);
    fillBy(hBy_yz, *eval, [](double z, double y) { return std::array{0.0, y, z}; });

    TH2F hBy_xy("hBy_xy", "B_{y} in xy-plane (z-centre);x [m];y [m];B_{y} [T]", nTrans,
                xMin / 1000.0, xMax / 1000.0, nTrans, yMin / 1000.0, yMax / 1000.0);
    fillBy(hBy_xy, *eval, [zC_mm](double x, double y) { return std::array{x, y, zC_mm}; });

    // ── Write ROOT file ───────────────────────────────────────────────────
    {
        TFile out(out_path.c_str(), "RECREATE");
        if (out.IsZombie()) {
            std::cerr << "error: failed to open " << out_path << '\n';
            return 1;
        }
        gBy_z.Write();
        hBy_xz.Write();
        hBy_yz.Write();
        hBy_xy.Write();
    }
    std::cout << "Wrote " << out_path << " (4 objects)\n";

    // ── PDF export ────────────────────────────────────────────────────────
    auto const out_fs = std::filesystem::path(out_path);
    auto const stem = (out_fs.parent_path() / out_fs.stem()).string();
    struct PdfPlot {
        TObject* obj;
        char const* suffix;
        char const* opt;
    };
    for (auto const& p : {PdfPlot{&gBy_z, "_by_z", "AL"}, PdfPlot{&hBy_xz, "_by_xz", "COLZ"},
                          PdfPlot{&hBy_yz, "_by_yz", "COLZ"}, PdfPlot{&hBy_xy, "_by_xy", "COLZ"}}) {
        TCanvas c("c", "", 800, 600);
        c.SetGrid();
        p.obj->Draw(p.opt);
        std::string const pdf = stem + p.suffix + ".pdf";
        c.SaveAs(pdf.c_str());
        std::cout << "Saved " << pdf << '\n';
    }

    return 0;
}
