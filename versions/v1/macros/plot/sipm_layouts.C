// sipm_layouts.C -- schematic plan view of the three SiPM layouts used in the
// matrix scan, drawn on the 250x250 mm scintillator footprint.  One figure with
// three side-by-side panels (config 1/2/3) showing where the side-mounted SiPMs
// sit; produced as a vector PDF for the report and a PNG preview.
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

namespace {
const double kHalf = 125.0;          // slab half-extent [mm]
const double kTile = 8.0;            // drawn SiPM square side [mm] (schematic)
const double kEdge = 0.5 * kHalf;    // +/-50% of half-length = +/-62.5 mm

struct Tile { double x, y; };

std::vector<Tile> tiles(int cfg)
{
    switch (cfg) {
        case 1:
            return {{ kHalf, -kEdge}, { kHalf,  kEdge}, {-kHalf, -kEdge}, {-kHalf,  kEdge},
                    {-kEdge,  kHalf}, { kEdge,  kHalf}, {-kEdge, -kHalf}, { kEdge, -kHalf}};
        case 2:
            return {{ kHalf, 0.}, {-kHalf, 0.}, {0., kHalf}, {0., -kHalf}};
        case 3:
            return {{ kHalf, -kEdge}, { kHalf, kEdge}};
    }
    return {};
}

void panel(int cfg, const char* title)
{
    gPad->DrawFrame(-kHalf - 20, -kHalf - 20, kHalf + 20, kHalf + 20,
                    TString::Format("%s;x [mm];y [mm]", title));
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

    for (const auto& t : tiles(cfg)) {
        TBox* b = new TBox(t.x - 0.5 * kTile, t.y - 0.5 * kTile,
                           t.x + 0.5 * kTile, t.y + 0.5 * kTile);
        b->SetFillColor(kBlack);
        b->SetLineColor(kBlack);
        b->Draw("SAME");
    }
}
}  // namespace

void sipm_layouts(const char* outdir = "report/figures")
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gSystem->mkdir(outdir, kTRUE);

    const int n[4] = {0, 8, 4, 2};
    TCanvas* c = new TCanvas("c_sipm_layouts", "SiPM layouts", 1500, 520);
    c->Divide(3, 1, 0.004, 0.004);

    for (int cfg = 1; cfg <= 3; ++cfg) {
        c->cd(cfg);
        gPad->SetLeftMargin(0.11);
        gPad->SetRightMargin(0.05);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.10);
        panel(cfg, TString::Format("Config %d (%d SiPMs)", cfg, n[cfg]));
    }

    c->SaveAs(TString::Format("%s/sipm_layouts.pdf", outdir));
    c->SaveAs(TString::Format("%s/sipm_layouts.png", outdir));
    printf("sipm_layouts: wrote %s/sipm_layouts.{pdf,png}\n", outdir);
}
