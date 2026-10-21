// SPDX-FileCopyrightText: 2026 CERN for the benefit of the SHiP Collaboration
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// plot_field_map — plot a field map (docs/field_map_format.md) as ROOT
// histograms and PDFs.
//
// Usage:
//   plot_field_map <input.root> <map> <out.root> [<xMin> <xMax> <yMin> <yMax> <zMin> <zMax>]
//
// All positions in mm, in the map's own frame. Produces B_y vs z along the
// beam axis plus B_y in the xz-, yz- and central xy-planes, written to
// <out.root> with one PDF per plot alongside it. The plot range defaults to
// the map's grid, reflected on mirrored axes.

#include "SHiP/FieldService/FieldMap.h"
#include "SHiP/FieldService/FieldMapIO.h"
#include "SHiP/FieldService/IFieldSource.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH2F.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <mp-units/systems/si.h>
#include <print>
#include <stdexcept>
#include <string>

namespace {

using SHiP::IFieldEvaluator;

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

/// Parse all of `s` as a finite number; throw otherwise.
double parseFinite(char const* s) {
    std::size_t consumed = 0;
    double const v = std::stod(s, &consumed);
    if (s[consumed] != '\0' || !std::isfinite(v)) {
        throw std::invalid_argument{std::string("invalid number '") + s + "'"};
    }
    return v;
}

void usage(char const* prog) {
    std::print(stderr,
               "Usage: {} <input.root> <map> <out.root> [<xMin> <xMax> <yMin> <yMax> <zMin> "
               "<zMax>]\n"
               "       Positions in mm; the range defaults to the map's grid. Writes B_y\n"
               "       plots to <out.root> and one PDF per plot next to it. Outside the\n"
               "       mapped region the evaluator clamps to the boundary value — a\n"
               "       plateau beyond the map bounds is a plotting artefact, not physical\n"
               "       fringe field.\n",
               prog);
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 10) {
        usage(argv[0]);
        return 1;
    }
    std::string const map_path = argv[1];
    std::string const map_name = argv[2];
    std::string const out_path = argv[3];

    std::shared_ptr<IFieldEvaluator> eval;
    SHiP::FieldBox extent;
    try {
        auto const map = SHiP::readFieldMap(map_path, map_name);
        extent = SHiP::extent(map);
        eval = SHiP::makeFieldEvaluator(map);
    } catch (std::exception const& e) {
        std::println(stderr, "error: {}", e.what());
        return 1;
    }

    // The map's extent (mm), reflected on mirrored axes; overridden by the
    // arguments.
    std::array<double, 3> lo{}, hi{};
    for (std::size_t a = 0; a < 3; ++a) {
        lo.at(a) = extent.min.at(a).numerical_value_in(SHiP::units::mm);
        hi.at(a) = extent.max.at(a).numerical_value_in(SHiP::units::mm);
    }
    if (argc == 10) {
        try {
            for (std::size_t a = 0; a < 3; ++a) {
                lo[a] = parseFinite(argv[4 + (2 * a)]);
                hi[a] = parseFinite(argv[5 + (2 * a)]);
            }
        } catch (std::exception const& e) {
            std::println(stderr, "error: {}", e.what());
            usage(argv[0]);
            return 1;
        }
    }
    auto const [xMin, yMin, zMin] = lo;
    auto const [xMax, yMax, zMax] = hi;
    if (xMin >= xMax || yMin >= yMax || zMin >= zMax) {
        std::println(stderr, "error: each min must be smaller than the corresponding max");
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
            std::println(stderr, "error: failed to open {}", out_path);
            return 1;
        }
        gBy_z.Write();
        hBy_xz.Write();
        hBy_yz.Write();
        hBy_xy.Write();
    }
    std::println("Wrote {} (4 objects)", out_path);

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
        std::println("Saved {}", pdf);
    }

    return 0;
}
