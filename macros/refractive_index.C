// refractive_index.C -- refractive-index structure of the optical model
// (GeometryMuonScint.cc).  Two views of the same input data:
//
//   * Left:  n(lambda) for every optical material.  All are modelled with a
//            wavelength-flat index across the visible (a two-point table in
//            Geant4), which is why each material is a horizontal line.
//   * Right: the radial refractive-index profile of a config-4 multi-clad
//            Y-11 fibre: core (n=1.59) / PMMA inner clad (n=1.49) /
//            fluoropolymer outer sheath (n=1.42), then the deliberate
//            0.05 mm air annulus (n=1.0003) before the n=1.55 clamp bulk.
//            This low-index staircase is what sustains core-cladding TIR.
//
// Usage:
//   root -l -b -q 'macros/refractive_index.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TText.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include "style.h"

namespace {

const int kScint  = PetroffPalette::getP6(0);   // blue
const int kCore   = PetroffPalette::getP6(4);   // grey
const int kClamp  = PetroffPalette::getP6(1);   // orange
const int kPMMA   = PetroffPalette::getP6(2);   // red
const int kGrease = PetroffPalette::getP6(5);   // violet
const int kSheath = PetroffPalette::getP6(3);   // magenta
const int kAir    = 17;                          // light grey

}  // namespace

void refractive_index(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_nidx", "refractive index", 1120, 540);
    c->Divide(2, 1, 0.004, 0.004);

    // ---- Panel 1: n(lambda) of every material --------------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.28);
    TH1F* f1 = gPad->DrawFrame(350., 1.40, 700., 1.61,
                               ";wavelength [nm];refractive index n");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.060); f1->GetXaxis()->SetTitleSize(0.060);

    struct Mat { const char* label; double n; int color; };
    const Mat mats[] = {
        {"WLS core (Y-11)",            1.59,   kCore},
        {"scintillator (PVT)",         1.58,   kScint},
        {"clamp / holder / epoxy",     1.55,   kClamp},
        {"PMMA inner clad",            1.49,   kPMMA},
        {"optical grease",             1.46,   kGrease},
        {"fluoropolymer sheath",       1.42,   kSheath},
    };
    auto* l1 = new TLegend(0.03, 0.75, 0.99, 0.995);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.042);
    l1->SetNColumns(2);
    for (const auto& m : mats) {
        auto* ln = new TLine(350., m.n, 700., m.n);
        ln->SetLineColor(m.color); ln->SetLineWidth(2);
        ln->Draw("SAME");
        l1->AddEntry(ln, m.label, "l");
    }
    l1->Draw();

    // ---- Panel 2: multi-clad fibre radial index profile ----------------
    c->cd(2);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.28);
    TH1F* f2 = gPad->DrawFrame(0., 0.95, 0.62, 1.65,
                               ";radial position [mm];refractive index n");
    f2->GetYaxis()->SetTitleOffset(1.15);
    f2->GetYaxis()->SetTitleSize(0.060); f2->GetXaxis()->SetTitleSize(0.060);

    // Horizontal layer segments.
    struct Layer { double x0, x1, n; int color; const char* name; };
    const Layer layers[] = {
        {0.00, 0.46, 1.5900, kCore,   "core  n = 1.59"},
        {0.46, 0.48, 1.4900, kPMMA,   "PMMA clad  n = 1.49"},
        {0.48, 0.50, 1.4200, kSheath, "FP sheath  n = 1.42"},
        {0.50, 0.55, 1.0003, kAir,    "air annulus  n = 1.0003"},
        {0.55, 0.62, 1.5500, kClamp,  "clamp  n = 1.55"},
    };
    auto* l2 = new TLegend(0.03, 0.75, 0.99, 0.995);
    l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextSize(0.042);
    l2->SetNColumns(2);
    for (const auto& L : layers) {
        auto* seg = new TLine(L.x0, L.n, L.x1, L.n);
        seg->SetLineColor(L.color); seg->SetLineWidth(3);
        seg->Draw("SAME");
        l2->AddEntry(seg, L.name, "l");
    }
    // Vertical connectors at the layer boundaries.
    const double bx[4] = {0.46, 0.48, 0.50, 0.55};
    const double by0[4] = {1.49,  1.42,  1.0003, 1.0003};
    const double by1[4] = {1.59,  1.49,  1.42,   1.55};
    for (int i = 0; i < 4; ++i) {
        auto* v = new TLine(bx[i], by0[i], bx[i], by1[i]);
        v->SetLineColor(kBlack); v->SetLineWidth(1); v->SetLineStyle(2);
        v->Draw("SAME");
    }
    l2->Draw();

    c->SaveAs(TString::Format("%s/refractive_index.pdf", outdir));
    c->SaveAs(TString::Format("%s/refractive_index.png", outdir));
    printf("refractive_index: wrote %s/refractive_index.{pdf,png}\n", outdir);
}
