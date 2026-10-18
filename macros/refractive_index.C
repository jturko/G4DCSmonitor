// refractive_index.C -- refractive-index structure of the optical model
// (GeometryMuonScint.cc).  Three views of the same input data:
//
//   * Left:   the index ladder of the bulk media.  Every medium is modelled
//             with a wavelength-flat index across the visible (a two-point
//             table in Geant4), so a single value per medium is complete.
//   * Middle: the radial refractive-index profile of a config-4 multi-clad
//             Y-11 fibre: core (n=1.59) / PMMA inner clad (n=1.49) /
//             fluoropolymer outer sheath (n=1.42), then the deliberate
//             0.05 mm air annulus (n=1.0003) before the n=1.55 clamp bulk.
//   * Right:  the critical angles theta_c = asin(n2/n1) of the successive
//             layer pairs, i.e. the total-internal-reflection thresholds
//             implied by the index staircase.
//
// Usage:
//   root -l -b -q 'macros/refractive_index.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TText.h>
#include <TLatex.h>
#include <TMath.h>
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

    TCanvas* c = new TCanvas("c_nidx", "refractive index", 1290, 520);
    c->Divide(3, 1, 0.004, 0.004);

    // ---- Panel 1: index ladder of the bulk media -----------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.28);

    auto* h = new TH1F("hNidx", ";medium;refractive index n", 6, 0., 6.);
    h->SetDirectory(0);
    h->SetMinimum(1.40);
    h->SetMaximum(1.62);
    h->SetBinContent(1, 1.59);   // WLS core (Y-11)
    h->SetBinContent(2, 1.58);   // scintillator (PVT)
    h->SetBinContent(3, 1.55);   // clamp / holder / epoxy
    h->SetBinContent(4, 1.49);   // PMMA inner clad
    h->SetBinContent(5, 1.46);   // optical grease
    h->SetBinContent(6, 1.42);   // fluoropolymer sheath
    h->GetXaxis()->SetBinLabel(1, "#splitline{WLS}{core}");
    h->GetXaxis()->SetBinLabel(2, "scint.");
    h->GetXaxis()->SetBinLabel(3, "#splitline{clamp/}{epoxy}");
    h->GetXaxis()->SetBinLabel(4, "#splitline{PMMA}{clad}");
    h->GetXaxis()->SetBinLabel(5, "grease");
    h->GetXaxis()->SetBinLabel(6, "#splitline{FP}{sheath}");
    h->GetYaxis()->SetTitleOffset(1.15);
    h->GetYaxis()->SetTitleSize(0.060); h->GetXaxis()->SetTitleSize(0.055);
    h->GetYaxis()->SetLabelSize(0.048); h->GetXaxis()->SetLabelSize(0.045);
    const int cols[6] = {kCore, kScint, kClamp, kPMMA, kGrease, kSheath};
    for (int i = 0; i < 6; ++i) h->SetFillColor(cols[i]);
    h->SetBarWidth(0.7);
    h->SetBarOffset(0.15);
    h->SetLineColor(kBlack);
    h->Draw("BAR");
    for (int i = 0; i < 6; ++i) {
        auto* v = new TLatex(i + 0.5, h->GetBinContent(i + 1) + 0.006,
                             Form("%.2f", h->GetBinContent(i + 1)));
        v->SetTextAlign(22); v->SetTextSize(0.040); v->SetTextFont(42);
        v->Draw("SAME");
    }
    auto* n1 = new TLatex(0.16, 0.90, "wavelength-flat (310-700 nm)");
    n1->SetNDC(); n1->SetTextSize(0.045); n1->Draw("SAME");

    // ---- Panel 2: multi-clad fibre radial index profile ----------------
    c->cd(2);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.28);
    TH1F* f2 = gPad->DrawFrame(0., 0.95, 0.62, 1.65,
                               ";radial position [mm];refractive index n");
    f2->GetYaxis()->SetTitleOffset(1.15);
    f2->GetYaxis()->SetTitleSize(0.060); f2->GetXaxis()->SetTitleSize(0.060);

    struct Layer { double x0, x1, n; int color; const char* name; };
    const Layer layers[] = {
        {0.00, 0.46, 1.5900, kCore,   "core  n = 1.59"},
        {0.46, 0.48, 1.4900, kPMMA,   "PMMA clad  n = 1.49"},
        {0.48, 0.50, 1.4200, kSheath, "FP sheath  n = 1.42"},
        {0.50, 0.55, 1.0003, kAir,    "air annulus  n = 1.0003"},
        {0.55, 0.62, 1.5500, kClamp,  "clamp  n = 1.55"},
    };
    auto* l2 = new TLegend(0.03, 0.73, 0.99, 0.995);
    l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextSize(0.040);
    l2->SetNColumns(2);
    for (const auto& L : layers) {
        auto* seg = new TLine(L.x0, L.n, L.x1, L.n);
        seg->SetLineColor(L.color); seg->SetLineWidth(3);
        seg->Draw("SAME");
        l2->AddEntry(seg, L.name, "l");
    }
    const double bx[4] = {0.46, 0.48, 0.50, 0.55};
    const double by0[4] = {1.49,  1.42,  1.0003, 1.0003};
    const double by1[4] = {1.59,  1.49,  1.42,   1.55};
    for (int i = 0; i < 4; ++i) {
        auto* v = new TLine(bx[i], by0[i], bx[i], by1[i]);
        v->SetLineColor(kBlack); v->SetLineWidth(1); v->SetLineStyle(2);
        v->Draw("SAME");
    }
    l2->Draw();

    // ---- Panel 3: TIR critical angles implied by the index stair --------
    c->cd(3);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.28);

    const double nPair[5][2] = {
        {1.59,   1.49  },   // core -> PMMA clad
        {1.49,   1.42  },   // PMMA clad -> FP sheath
        {1.42,   1.0003},   // FP sheath -> air annulus
        {1.58,   1.0003},   // scintillator -> air
        {1.58,   1.46  },   // scintillator -> grease
    };
    auto* hc = new TH1F("hThetaC", ";interface;critical angle #theta_{c} [deg]",
                        5, 0., 5.);
    hc->SetDirectory(0);
    hc->SetMinimum(0.);
    hc->SetMaximum(85.);
    for (int i = 0; i < 5; ++i)
        hc->SetBinContent(i + 1, TMath::ASin(nPair[i][1] / nPair[i][0]) * 180. / TMath::Pi());
    hc->GetXaxis()->SetBinLabel(1, "#splitline{core#rightarrow}{PMMA}");
    hc->GetXaxis()->SetBinLabel(2, "#splitline{PMMA#rightarrow}{FP}");
    hc->GetXaxis()->SetBinLabel(3, "#splitline{FP#rightarrow}{air}");
    hc->GetXaxis()->SetBinLabel(4, "#splitline{scint#rightarrow}{air}");
    hc->GetXaxis()->SetBinLabel(5, "#splitline{scint#rightarrow}{grease}");
    hc->GetYaxis()->SetTitleOffset(1.15);
    hc->GetYaxis()->SetTitleSize(0.060); hc->GetXaxis()->SetTitleSize(0.050);
    hc->GetYaxis()->SetLabelSize(0.048); hc->GetXaxis()->SetLabelSize(0.043);
    hc->SetFillColor(PetroffPalette::getP6(3));
    hc->SetBarWidth(0.6);
    hc->SetBarOffset(0.2);
    hc->SetLineColor(kBlack);
    hc->Draw("BAR");
    for (int i = 0; i < 5; ++i) {
        auto* v = new TLatex(i + 0.5, hc->GetBinContent(i + 1) + 1.5,
                             Form("%.1f", hc->GetBinContent(i + 1)));
        v->SetTextAlign(22); v->SetTextSize(0.040); v->SetTextFont(42);
        v->Draw("SAME");
    }
    auto* n3 = new TLatex(0.16, 0.90, "angles from the surface normal");
    n3->SetNDC(); n3->SetTextSize(0.045); n3->Draw("SAME");

    c->SaveAs(TString::Format("%s/refractive_index.pdf", outdir));
    c->SaveAs(TString::Format("%s/refractive_index.png", outdir));
    printf("refractive_index: wrote %s/refractive_index.{pdf,png}\n", outdir);
}
