// sipm_layouts.C -- schematic plan view of the five detector layouts used in
// the matrix scan, drawn on the 250x250 mm scintillator footprint.  One figure
// with five panels (config 1, 2, 2 (mod), 3, 4) in a 2x3 grid (the last cell is
// empty) showing where the side-mounted SiPMs (or the Config 4 WLS fibre clamp)
// sit; produced as a vector PDF for the report and a PNG preview.
//
// Positions follow the geometry presets in src/GeometryMuonScint.cc and are
// drawn in the report frame (paper plan view: top=+y, bottom=-y, left=-x,
// right=+x). The internal simulation layout is this frame rotated 90 deg
// clockwise, hence config 3 sits on the top edge here:
//   cfg1 : 8 SiPMs, 2/side at u = +/-0.5
//   cfg2 : 4 SiPMs, one per side at the cfg1 offsets (cfg1 minus one/side):
//          +X u=-0.5, -X u=+0.5, +Y u=+0.5, -Y u=-0.5
//   cfg12 ("Config 2 (mod)") : 4 SiPMs on the four 45-deg chamfers of the
//          corner-cut octagon.  Each square corner of the slab is truncated
//          with a 12 mm leg, leaving a 17.0 mm cut face centred at
//          (+/-119, +/-119) mm.  One 4x4 mm SiPM sits flush on each cut face;
//          report-frame channel order 0..3 = BR, TR, TL, BL.
//          Octagon: |x|+|y| <= 238 mm.
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
#include <TPolyLine.h>
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
const double kCutLeg = 12.0;         // 45-deg corner-cut leg [mm] (cfg 12)
const double kCutVertex = kHalf - kCutLeg;          // 113 mm (octagon vertex coord)
const double kCutFaceCentre = kHalf - 0.5 * kCutLeg; // 119 mm (chamfer centre)

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
        case 12:
            // One SiPM on each 45-deg chamfer, channel order 0..3 = BR/TR/TL/BL.
            return {{ kCutFaceCentre, -kCutFaceCentre},   // 0 BR (119,-119)
                    { kCutFaceCentre,  kCutFaceCentre},   // 1 TR (119,119)
                    {-kCutFaceCentre,  kCutFaceCentre},   // 2 TL (-119,119)
                    {-kCutFaceCentre, -kCutFaceCentre}};  // 3 BL (-119,-119)
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
    gStyle->SetTitleFontSize(0.045);
    gStyle->SetTitleSize(0.045, "t");

    if (cfg == 12) {
        // Corner-cut octagon: the four square corners are truncated by a 45-deg
        // cut of leg kCutLeg, so |x|+|y| <= 2*(kHalf - kCutLeg/2) = 238 mm.
        double ox[9] = { kHalf, kHalf, kCutVertex, -kCutVertex,
                        -kHalf, -kHalf, -kCutVertex, kCutVertex, kHalf};
        double oy[9] = {-kCutVertex, kCutVertex, kHalf, kHalf,
                         kCutVertex, -kCutVertex, -kHalf, -kHalf, -kCutVertex};
        TPolyLine* fill = new TPolyLine(9, ox, oy);
        fill->SetFillColorAlpha(kAzure - 9, 0.16);
        fill->Draw("F");
        TPolyLine* edge = new TPolyLine(9, ox, oy);
        edge->SetLineColor(kGray + 2);
        edge->SetLineWidth(2);
        edge->Draw("SAME");
    } else {
        TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
        slab->SetFillColorAlpha(kAzure - 9, 0.16);
        slab->Draw("F");
        // Match the cut-corner panel: every layout gets the same visible
        // outline around the scintillator footprint.
        double sx[5] = {-kHalf, kHalf, kHalf, -kHalf, -kHalf};
        double sy[5] = {-kHalf, -kHalf, kHalf, kHalf, -kHalf};
        TPolyLine* edge = new TPolyLine(5, sx, sy);
        edge->SetLineColor(kGray + 2);
        edge->SetLineWidth(2);
        edge->Draw("SAME");
    }

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

    if (cfg == 12) {
        // One diamond per chamfer.  The square is rotated 45 deg so its outer
        // side lies flush on the cut face; the face centre itself is the
        // midpoint of that side, hence the body grows inwards along -n.
        // Channel order 0..3 = BR, TR, TL, BL (same as tiles(12)).
        const char* names[4] = {"0 BR", "1 TR", "2 TL", "3 BL"};
        const double inv = 1.0 / std::sqrt(2.0);
        int idx = 0;
        for (const auto& s : tiles(cfg)) {
            const double nx = (s.x > 0 ? inv : -inv);   // outward face normal
            const double ny = (s.y > 0 ? inv : -inv);
            const double tx =  nx;                      // along the cut face
            const double ty = -ny;
            const double h  = 0.5 * kTile;              // half side

            double vx[4], vy[4];
            vx[0] = s.x + h * tx;                 // on-face side, + tangent end
            vy[0] = s.y + h * ty;
            vx[1] = s.x + kTile * nx + h * tx;    // outboard side, + tangent end
            vy[1] = s.y + kTile * ny + h * ty;
            vx[2] = s.x + kTile * nx - h * tx;    // outboard side, - tangent end
            vy[2] = s.y + kTile * ny - h * ty;
            vx[3] = s.x - h * tx;                 // on-face side, - tangent end
            vy[3] = s.y - h * ty;

            TPolyLine* d = new TPolyLine(4, vx, vy);
            d->SetFillColor(kBlack);
            d->SetLineColor(kBlack);
            d->SetLineWidth(1);
            d->Draw("F");
            d->Draw("SAME");

            // Label inside the plate, just in from the cut face, so it names
            // the outboard diamond without covering the plate footprint.
            const double off = 17.0;
            TLatex* lab = new TLatex(s.x - off * nx, s.y - off * ny, names[idx]);
            lab->SetTextAlign(22);       // centred
            lab->SetTextSize(0.034);
            lab->SetTextColor(kBlack);
            lab->Draw("SAME");
            ++idx;
        }
        return;
    }

    for (const auto& t : tiles(cfg)) {
        // Draw the sensor outboard of the slab, as mounted on the narrow face:
        // the square sits outside the plate boundary with its inner side on the
        // slab edge.  tiles() keeps the nominal on-face centre, which is what
        // the edge test and the sensor labels use.
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

        // Sensor name, inside the slab just off the edge, so it names the
        // outboard square without covering the plate footprint.
        const char* lab = sensorLabel(t.x, t.y);
        if (lab[0]) {
            const double off = 15.0;                 // mm inwards from the edge
            double ax = t.x, ay = t.y;
            if (std::fabs(std::fabs(t.x) - kHalf) < 1e-6) ax -= (t.x > 0 ? off : -off);
            if (std::fabs(std::fabs(t.y) - kHalf) < 1e-6) ay -= (t.y > 0 ? off : -off);
            TLatex* tx = new TLatex(ax, ay, lab);
            tx->SetTextAlign(22);      // centred
            tx->SetTextSize(0.040);
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

    const int n[13] = {0, 8, 4, 2, 1, 0, 0, 0, 0, 0, 0, 0, 4};
    const int cfgs[5] = {1, 2, 12, 3, 4};
    TCanvas* c = new TCanvas("c_sipm_layouts", "SiPM layouts", 1100, 1500);
    c->Divide(2, 3, 0.004, 0.004);

    for (int k = 0; k < 5; ++k) {
        const int cfg = cfgs[k];
        c->cd(k + 1);
        gPad->SetLeftMargin(0.12);
        gPad->SetRightMargin(0.05);
        gPad->SetBottomMargin(0.13);
        gPad->SetTopMargin(0.10);
        if (cfg == 4)
            panel(cfg, "Config 4 (1 SiPM + WLS clamp)");
        else if (cfg == 12)
            panel(cfg, TString::Format("Config 2 (mod) (%d SiPMs)", n[12]));
        else
            panel(cfg, TString::Format("Config %d (%d SiPMs)", cfg, n[cfg]));
    }
    // The sixth cell is intentionally empty (no frame drawn).
    c->cd(6);

    c->SaveAs(TString::Format("%s/sipm_layouts.pdf", outdir));
    c->SaveAs(TString::Format("%s/sipm_layouts.png", outdir));
    printf("sipm_layouts: wrote %s/sipm_layouts.{pdf,png}\n", outdir);
}
