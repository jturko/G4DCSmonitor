// opt_layouts.C -- plan views of the three distinct sensor geometries used by
// the v7_opt detector-optimisation campaign (report Sec. 10).
//
// The ten designs differ in wrapper, clamp surface and sensor size, but they
// use only three visible geometries:
//   preset 1   8 SiPMs, 2 per side at u = +/-0.5  (62.5 mm from corner)
//   preset 8   8 SiPMs, 2 per side at u = +/-0.75 (31.25 mm from corner)
//   preset 10  four-sided WLS clamp, 4 read-out SiPMs, one per side
//
// Visual style matches macros/sipm_layouts.C:
//   - Azure-tinted scintillator plate with kGray+2 outline
//   - Schematic outboard black SiPM squares (kTile = 8 mm)
//   - Shared style(0.036), optStat(0), optTitle(1)
//   - 2x2 grid with empty fourth cell
//   - Single-line or compact two-line design subtitles below each frame
//   - Equal-aspect 1:1 geometry (frame and plate exactly square in pixels)
//
// Frame convention: report plan view, top = +y, right = +x, i.e. the simulation
// frame rotated 90 deg clockwise, (x,y) -> (y,-x).
//
// Usage:
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     $HOME/programs/root/build/bin/root -l -b -q \
//     'macros/opt_layouts.C("report/figures")'
//
// Analysis-only: no Geant4 dependencies. Deterministic, no timestamps.

#include <TCanvas.h>
#include <TBox.h>
#include <TH1F.h>
#include <TLine.h>
#include <TLatex.h>
#include <TPad.h>
#include <TPolyLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include <vector>
#include <cmath>
#include <cstdio>

#include "style.h"

namespace {
const double kHalf = 125.0; // slab half-extent [mm]
const double kTile = 8.0;   // drawn SiPM square side [mm] (schematic)

struct Tile { double x, y; };

// Report-frame sensor name for standard edge-mounted tile positions.
// Clockwise from top-right:
//   T1 top-right    T2 top-left
//   R1 bottom-right R2 right-top
//   B1 bottom-left  B2 bottom-right
//   L1 left-top     L2 left-bottom
const char* sensorLabel(double x, double y)
{
    if (std::fabs(y - kHalf) < 1e-6) return (x > 0.) ? "T1" : "T2";
    if (std::fabs(y + kHalf) < 1e-6) return (x < 0.) ? "B1" : "B2";
    if (std::fabs(x - kHalf) < 1e-6) return (y < 0.) ? "R1" : "R2";
    if (std::fabs(x + kHalf) < 1e-6) return (y > 0.) ? "L1" : "L2";
    return "";
}

std::vector<Tile> tiles(int preset)
{
    if (preset == 1) {
        const double u = 0.5 * kHalf;
        return {{ kHalf, -u}, { kHalf,  u}, {-kHalf, -u}, {-kHalf,  u},
                {-u,  kHalf}, { u,  kHalf}, {-u, -kHalf}, { u, -kHalf}};
    } else if (preset == 8) {
        const double u = 0.75 * kHalf;
        return {{ kHalf, -u}, { kHalf,  u}, {-kHalf, -u}, {-kHalf,  u},
                {-u,  kHalf}, { u,  kHalf}, {-u, -kHalf}, { u, -kHalf}};
    }
    return {};
}

void panel(int preset, const char* title, const std::vector<const char*>& subtitles)
{
    TH1F* frame = gPad->DrawFrame(-145.0, -145.0, 145.0, 145.0,
                                  TString::Format("%s;x [mm];y [mm]", title));
    frame->GetXaxis()->SetTitleSize(0.036);
    frame->GetYaxis()->SetTitleSize(0.036);
    frame->GetXaxis()->SetLabelSize(0.032);
    frame->GetYaxis()->SetLabelSize(0.032);
    frame->GetXaxis()->SetTitleOffset(1.05);
    frame->GetYaxis()->SetTitleOffset(1.05);
    gStyle->SetTitleFontSize(0.036);
    gStyle->SetTitleSize(0.036, "t");

    // Scintillator plate: square filled with kAzure-9 at alpha 0.16
    TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
    slab->SetFillColorAlpha(kAzure - 9, 0.16);
    slab->Draw("F");

    // Outline: kGray+2, line width 2
    double sx[5] = {-kHalf, kHalf, kHalf, -kHalf, -kHalf};
    double sy[5] = {-kHalf, -kHalf, kHalf, kHalf, -kHalf};
    TPolyLine* edge = new TPolyLine(5, sx, sy);
    edge->SetLineColor(kGray + 2);
    edge->SetLineWidth(2);
    edge->Draw("SAME");

    // Centre lines: dashed kGray+1, line width 1, style 2
    TLine* hx = new TLine(-kHalf, 0., kHalf, 0.);
    TLine* vy = new TLine(0., -kHalf, 0., kHalf);
    hx->SetLineColor(kGray + 1); hx->SetLineStyle(2); hx->SetLineWidth(1);
    vy->SetLineColor(kGray + 1); vy->SetLineStyle(2); vy->SetLineWidth(1);
    hx->Draw("SAME"); vy->Draw("SAME");

    if (preset == 10) {
        // Four-sided WLS clamp in config-4 visual language:
        // A green bar (kGreen+2 fill, black outline, line width 1) outboard and
        // flush along all four narrow faces, with one black read-out SiPM square
        // at one end of each bar. Rotational arrangement (top-left, right-top,
        // bottom-right, left-bottom) so sensors and bars do not overlap.

        // Top bar (+y) & top-left read-out SiPM
        TBox* barTop = new TBox(-kHalf, kHalf, kHalf, kHalf + kTile);
        barTop->SetFillColor(kGreen + 2);
        barTop->SetLineColor(kBlack);
        barTop->SetLineWidth(1);
        barTop->Draw("SAME");
        TBox* sipmTL = new TBox(-kHalf - kTile, kHalf, -kHalf, kHalf + kTile);
        sipmTL->SetFillColor(kBlack);
        sipmTL->SetLineColor(kBlack);
        sipmTL->SetLineWidth(1);
        sipmTL->Draw("SAME");

        // Right bar (+x) & right-top read-out SiPM
        TBox* barRight = new TBox(kHalf, -kHalf, kHalf + kTile, kHalf);
        barRight->SetFillColor(kGreen + 2);
        barRight->SetLineColor(kBlack);
        barRight->SetLineWidth(1);
        barRight->Draw("SAME");
        TBox* sipmRT = new TBox(kHalf, kHalf, kHalf + kTile, kHalf + kTile);
        sipmRT->SetFillColor(kBlack);
        sipmRT->SetLineColor(kBlack);
        sipmRT->SetLineWidth(1);
        sipmRT->Draw("SAME");

        // Bottom bar (-y) & bottom-right read-out SiPM
        TBox* barBottom = new TBox(-kHalf, -kHalf - kTile, kHalf, -kHalf);
        barBottom->SetFillColor(kGreen + 2);
        barBottom->SetLineColor(kBlack);
        barBottom->SetLineWidth(1);
        barBottom->Draw("SAME");
        TBox* sipmBR = new TBox(kHalf, -kHalf - kTile, kHalf + kTile, -kHalf);
        sipmBR->SetFillColor(kBlack);
        sipmBR->SetLineColor(kBlack);
        sipmBR->SetLineWidth(1);
        sipmBR->Draw("SAME");

        // Left bar (-x) & left-bottom read-out SiPM
        TBox* barLeft = new TBox(-kHalf - kTile, -kHalf, -kHalf, kHalf);
        barLeft->SetFillColor(kGreen + 2);
        barLeft->SetLineColor(kBlack);
        barLeft->SetLineWidth(1);
        barLeft->Draw("SAME");
        TBox* sipmLB = new TBox(-kHalf - kTile, -kHalf - kTile, -kHalf, -kHalf);
        sipmLB->SetFillColor(kBlack);
        sipmLB->SetLineColor(kBlack);
        sipmLB->SetLineWidth(1);
        sipmLB->Draw("SAME");
    } else {
        // Presets 1 and 8: schematic outboard black squares (kTile = 8 mm)
        const double labelSize = (preset == 8) ? 0.029 : 0.032;
        const double off = (preset == 8) ? 13.0 : 15.0; // mm inward from slab edge

        for (const auto& t : tiles(preset)) {
            double x0 = t.x - 0.5 * kTile, x1 = t.x + 0.5 * kTile;
            double y0 = t.y - 0.5 * kTile, y1 = t.y + 0.5 * kTile;
            if (std::fabs(t.x - kHalf) < 1e-6)       { x0 =  kHalf; x1 =  kHalf + kTile; }
            else if (std::fabs(t.x + kHalf) < 1e-6)  { x1 = -kHalf; x0 = -kHalf - kTile; }
            if (std::fabs(t.y - kHalf) < 1e-6)       { y0 =  kHalf; y1 =  kHalf + kTile; }
            else if (std::fabs(t.y + kHalf) < 1e-6)  { y1 = -kHalf; y0 = -kHalf - kTile; }

            TBox* b = new TBox(x0, y0, x1, y1);
            b->SetFillColor(kBlack);
            b->SetLineColor(kBlack);
            b->Draw("SAME");

            const char* lab = sensorLabel(t.x, t.y);
            if (lab[0]) {
                double ax = t.x, ay = t.y;
                if (std::fabs(std::fabs(t.x) - kHalf) < 1e-6) ax -= (t.x > 0 ? off : -off);
                if (std::fabs(std::fabs(t.y) - kHalf) < 1e-6) ay -= (t.y > 0 ? off : -off);
                TLatex* tx = new TLatex(ax, ay, lab);
                tx->SetTextAlign(22);      // centred
                tx->SetTextSize(labelSize);
                tx->SetTextColor(kBlack);
                tx->Draw("SAME");
            }
        }
    }

    // Design-group subtitle(s) below the x-axis title
    const double xCen = 0.5 * (gPad->GetLeftMargin() + (1.0 - gPad->GetRightMargin()));
    TLatex sub;
    sub.SetNDC();
    sub.SetTextAlign(22);
    sub.SetTextFont(42);
    sub.SetTextSize(0.029);
    sub.SetTextColor(kBlack);

    if (subtitles.size() == 1) {
        sub.DrawLatex(xCen, 0.065, subtitles[0]);
    } else if (subtitles.size() == 2) {
        sub.DrawLatex(xCen, 0.082, subtitles[0]);
        sub.DrawLatex(xCen, 0.040, subtitles[1]);
    }
}
} // namespace

void opt_layouts(const char* outdir = "report/figures")
{
    style(0.036);                 // shared style with scaled title/label fonts
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_opt_layouts", "Design layouts", 1100, 1250);
    c->Divide(2, 2, 0.004, 0.004);

    const double CW = c->GetWw();
    const double CH = c->GetWh();
    const double leftMargin   = 0.125;
    const double rightMargin  = 0.125;
    const double bottomMargin = 0.24;
    const double padW = (0.5 - 2.0 * 0.004) * CW;
    const double padH = (0.5 - 2.0 * 0.004) * CH;
    const double frameW = (1.0 - leftMargin - rightMargin) * padW;
    const double topMargin = 1.0 - bottomMargin - (frameW / padH);
    const double frameH = (1.0 - bottomMargin - topMargin) * padH;
    const double platePx = frameW * (250.0 / 290.0);

    printf("opt_layouts: canvas %.0f x %.0f px, pad %.1f x %.1f px\n", CW, CH, padW, padH);
    printf("opt_layouts: computed topMargin = %.4f (left=%.3f, right=%.3f, bottom=%.3f)\n",
           topMargin, leftMargin, rightMargin, bottomMargin);
    printf("opt_layouts: frame width = %.1f px, frame height = %.1f px (aspect = %.4f)\n",
           frameW, frameH, frameW / frameH);
    printf("opt_layouts: plate bounding box = %.1f x %.1f px\n", platePx, platePx);

    struct PanelInfo {
        int preset;
        const char* title;
        std::vector<const char*> subtitles;
    };

    const std::vector<PanelInfo> panels = {
        {1, "Preset 1 (8 SiPMs, u = #pm0.5)",
         {"C1 designs: black edges, diffuse edges, 6 mm, control"}},
        {8, "Preset 8 (8 SiPMs, u = #pm0.75)",
         {"C8 designs: black edges, 6 mm diffuse edges"}},
        {10, "Preset 10 (4-sided WLS clamp)",
         {"4-channel: TiO_{2}, glossy", "8-channel dual: second read-out at far end"}}
    };

    for (size_t k = 0; k < panels.size(); ++k) {
        c->cd(k + 1);
        gPad->SetLeftMargin(leftMargin);
        gPad->SetRightMargin(rightMargin);
        gPad->SetBottomMargin(bottomMargin);
        gPad->SetTopMargin(topMargin);
        panel(panels[k].preset, panels[k].title, panels[k].subtitles);
    }

    // Cell 4 intentionally empty
    c->cd(4);

    c->SaveAs(TString::Format("%s/fig_design_layouts.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_design_layouts.png", outdir));
    printf("opt_layouts: wrote %s/fig_design_layouts.{pdf,png}\n", outdir);
}
