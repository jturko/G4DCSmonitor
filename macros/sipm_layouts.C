// sipm_layouts.C -- schematic plan view of the four detector layouts used in
// the matrix scan, drawn on the 250x250 mm scintillator footprint.  One figure
// with four side-by-side panels (config 1/2/3/4) showing where the side-mounted
// SiPMs (or the Config 4 WLS fibre clamp) sit; produced as a vector PDF for the
// report and a PNG preview.
//
// Positions follow the geometry presets in src/GeometryMuonScint.cc and are
// drawn in the report frame (paper plan view: top=+y, bottom=-y, left=-x,
// right=+x). The internal simulation layout is this frame rotated 90 deg
// clockwise, hence config 3 sits on the top edge here:
//   cfg1 : 8 SiPMs, 2/side at u = +/-0.5
//   cfg2 : 4 SiPMs, one per side at the cfg1 offsets (cfg1 minus one/side):
//          +X u=-0.5, -X u=+0.5, +Y u=+0.5, -Y u=-0.5
//   cfg3 : 2 SiPMs, both on the +y (top) edge at u = -0.5, +0.5
//   cfg4 : single SiPM + WLS fibre clamp: green bar along the +y (top) edge,
//          with the black read-out SiPM at its left end (drawn as in
//          Fig. 3(b) of Rao et al.)
//
// Usage:
//   root -l -b -q 'macros/sipm_layouts.C("report/figures")'

#include <TCanvas.h>
#include <TBox.h>
#include <TLine.h>
#include <TLatex.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include <vector>
#include <cmath>

// Shared ROOT style (fonts, ticks, title conventions).  Located via
// ROOT_INCLUDE_PATH (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {
const double kHalf = 125.0;          // slab half-extent [mm]
const double kTile = 8.0;            // drawn SiPM square side [mm] (schematic)
const double kEdge = 0.5 * kHalf;    // +/-50% of half-length = +/-62.5 mm

struct Tile { double x, y; };

// Report-frame sensor name for a standard edge-mounted tile position.
// Numbering is clockwise from the top-right corner, "1" being the sensor
// nearest that corner on each edge:
//   T1 top-right    T2 top-left
//   R1 bottom-right R2 right-top
//   B1 bottom-left  B2 bottom-right
//   L1 left-top     L2 left-bottom
// These names are used throughout the report (e.g. the SiPM-sharing figure).
// Returns "" for a position that is not a standard tile (config-4 WLS read-out).
const char* sensorLabel(double x, double y)
{
    if (std::fabs(y - kHalf) < 1e-6) return (x > 0.) ? "T1" : "T2";
    if (std::fabs(y + kHalf) < 1e-6) return (x < 0.) ? "B1" : "B2";
    if (std::fabs(x - kHalf) < 1e-6) return (y < 0.) ? "R1" : "R2";
    if (std::fabs(x + kHalf) < 1e-6) return (y > 0.) ? "L1" : "L2";
    return "";
}

std::vector<Tile> tiles(int cfg)
{
    switch (cfg) {
        case 1:
            return {{ kHalf, -kEdge}, { kHalf,  kEdge}, {-kHalf, -kEdge}, {-kHalf,  kEdge},
                    {-kEdge,  kHalf}, { kEdge,  kHalf}, {-kEdge, -kHalf}, { kEdge, -kHalf}};
        case 2:
            return {{ kHalf, -kEdge}, {-kHalf,  kEdge}, { kEdge,  kHalf}, {-kEdge, -kHalf}};
        case 3:
            return {{-kEdge, kHalf}, { kEdge, kHalf}};   // both on the top (+y) edge
        case 4:
            return {{ kHalf, kHalf}};   // (unused; cfg4 draws a bar, see panel)
    }
    return {};
}

void panel(int cfg, const char* title)
{
    TH1F* frame = gPad->DrawFrame(-kHalf - 20, -kHalf - 20, kHalf + 20, kHalf + 20,
                                  TString::Format("%s;x [mm];y [mm]", title));
    frame->GetXaxis()->SetTitleSize(0.045);
    frame->GetYaxis()->SetTitleSize(0.045);
    frame->GetXaxis()->SetLabelSize(0.040);
    frame->GetYaxis()->SetLabelSize(0.040);
    frame->GetXaxis()->SetTitleOffset(1.05);
    frame->GetYaxis()->SetTitleOffset(1.05);
    TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
    slab->SetFillColorAlpha(kAzure - 9, 0.16);
    slab->SetLineColor(kGray + 2);
    slab->SetLineWidth(2);
    slab->Draw("SAME");

    TLine* hx = new TLine(-kHalf, 0., kHalf, 0.);
    TLine* vy = new TLine(0., -kHalf, 0., kHalf);
    hx->SetLineColor(kGray + 1); hx->SetLineStyle(2); hx->SetLineWidth(1);
    vy->SetLineColor(kGray + 1); vy->SetLineStyle(2); vy->SetLineWidth(1);
    hx->Draw("SAME"); vy->Draw("SAME");

    if (cfg == 4) {
        // WLS fibre clamp: a green bar outboard along the +y (top) edge, full
        // length in x, with the single read-out SiPM (black) butt-coupled flush
        // against its left (-x) end face, matching Fig. 3(b)/4(a) of Rao et al.
        const double by = kHalf + 8.0;           // bar centre, outboard of +y
        TBox* bar = new TBox(-kHalf, by - 0.5 * kTile, kHalf, by + 0.5 * kTile);
        bar->SetFillColor(kGreen + 2);
        bar->SetLineColor(kBlack);
        bar->SetLineWidth(1);
        bar->Draw("SAME");
        TBox* sipm = new TBox(-kHalf - kTile, by - 0.6 * kTile,
                              -kHalf,          by + 0.6 * kTile);
        sipm->SetFillColor(kBlack);
        sipm->SetLineColor(kBlack);
        sipm->Draw("SAME");
        return;
    }

    for (const auto& t : tiles(cfg)) {
        TBox* b = new TBox(t.x - 0.5 * kTile, t.y - 0.5 * kTile,
                           t.x + 0.5 * kTile, t.y + 0.5 * kTile);
        b->SetFillColor(kBlack);
        b->SetLineColor(kBlack);
        b->Draw("SAME");

        // Sensor name, nudged inwards off the edge so it sits clear of the
        // 8 mm SiPM box on the slab (text half-height is ~9 mm at this size).
        const char* lab = sensorLabel(t.x, t.y);
        if (lab[0]) {
            const double off = 15.0;                 // mm inwards from the edge
            double ax = t.x, ay = t.y;
            if (std::fabs(std::fabs(t.x) - kHalf) < 1e-6) ax -= (t.x > 0 ? off : -off);
            if (std::fabs(std::fabs(t.y) - kHalf) < 1e-6) ay -= (t.y > 0 ? off : -off);
            TLatex* tx = new TLatex(ax, ay, lab);
            tx->SetTextAlign(22);      // centred
            tx->SetTextSize(0.05);
            tx->SetTextColor(kBlack);
            tx->Draw("SAME");
        }
    }
}
}  // namespace

void sipm_layouts(const char* outdir = "report/figures")
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gSystem->mkdir(outdir, kTRUE);

    const int n[5] = {0, 8, 4, 2, 1};
    TCanvas* c = new TCanvas("c_sipm_layouts", "SiPM layouts", 1100, 1060);
    c->Divide(2, 2, 0.004, 0.004);

    for (int cfg = 1; cfg <= 4; ++cfg) {
        c->cd(cfg);
        gPad->SetLeftMargin(0.12);
        gPad->SetRightMargin(0.05);
        gPad->SetBottomMargin(0.13);
        gPad->SetTopMargin(0.10);
        if (cfg == 4)
            panel(cfg, "Config 4 (1 SiPM + WLS clamp)");
        else
            panel(cfg, TString::Format("Config %d (%d SiPMs)", cfg, n[cfg]));
    }

    c->SaveAs(TString::Format("%s/sipm_layouts.pdf", outdir));
    c->SaveAs(TString::Format("%s/sipm_layouts.png", outdir));
    printf("sipm_layouts: wrote %s/sipm_layouts.{pdf,png}\n", outdir);
}
