// wls_process.C -- the wavelength-shifting process of the config-4 fibre core
// (Kuraray Y-11), exactly as configured in GeometryMuonScint.cc / PhysicsList.cc.
//
//   * Left:  the digitized Kuraray Y-11 absorption (WLSABSLENGTH shape, peak
//            430 nm) and re-emission (WLSCOMPONENT, peak 476 nm) spectra,
//            normalised to 1 at their peaks.  The shaded overlap is what
//            G4OpWLS requires to be empty in energy: the secondary must satisfy
//            E_gamma' <= E_gamma.
//   * Right: the re-emission delay probability density.  The production model
//            uses G4OpticalParameters::SetWLSTimeProfile("exponential") with the
//            Y-11 decay constant tau = 7 ns; the historical "delta" profile
//            (a fixed 7 ns delay) is overlaid for comparison.
//
// Usage:
//   root -l -b -q 'macros/wls_process.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TBox.h>
#include <TF1.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TLatex.h>

#include "style.h"

namespace {

// Digitized Kuraray Y-11 absorption shape (indexed by nm), peak at 430 nm.
const int kNA = 37;
const double lamA[37] = {650,500,488,486,483,478,474,472,470,467,465,462,459,
                         457,453,450,448,444,442,440,437,435,430,427,423,420,
                         415,410,405,400,396,390,386,380,370,360,350};
const double relA[37] = {0.0,0.0,0.0,0.005,0.015,0.054,0.11,0.20,0.31,0.45,
                         0.59,0.71,0.80,0.85,0.88,0.85,0.81,0.77,0.79,0.84,
                         0.90,0.96,1.00,0.93,0.84,0.75,0.64,0.59,0.51,0.44,
                         0.37,0.29,0.24,0.15,0.075,0.045,0.03};

// Digitized Kuraray Y-11 emission shape (indexed by nm), peak at 476 nm.
const int kNE = 36;
const double lamE[36] = {650,640,620,600,590,580,570,560,550,540,531,526,520,
                         516,512,507,503,500,495,490,487,485,482,479,476,473,
                         471,469,466,463,461,458,456,452,449,444};
const double relE[36] = {0.0,0.0,0.005,0.02,0.03,0.05,0.085,0.13,0.19,0.25,
                         0.31,0.43,0.55,0.63,0.71,0.77,0.76,0.73,0.69,0.73,
                         0.80,0.85,0.92,0.98,1.00,0.92,0.82,0.67,0.50,0.32,
                         0.21,0.14,0.09,0.04,0.01,0.0};

TGraph* mk(const double* x, const double* y, int n, int color, int marker)
{
    auto* g = new TGraph(n);
    for (int i = 0; i < n; ++i) g->SetPoint(i, x[i], y[i]);
    g->SetLineColor(color);
    g->SetLineWidth(2);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.8);
    return g;
}

const int kAbs = PetroffPalette::getP6(2);   // red
const int kEmi = PetroffPalette::getP6(0);   // blue

}  // namespace

void wls_process(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_wls_process", "WLS process", 1120, 540);
    c->Divide(2, 1, 0.004, 0.004);

    // ---- Panel 1: Y-11 absorption and emission overlap -----------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.24);
    TH1F* f1 = gPad->DrawFrame(350., 0., 660., 1.15,
                               ";wavelength [nm];normalised amplitude");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.060); f1->GetXaxis()->SetTitleSize(0.060);
    // Shade the red-ward region where the emission lives (E_emit <= E_absorb).
    auto* box = new TBox(444., 0., 660., 1.15);
    box->SetFillColorAlpha(kEmi, 0.08);
    box->SetLineWidth(0);
    box->Draw("SAME");
    TGraph* gA = mk(lamA, relA, kNA, kAbs, 21);
    TGraph* gE = mk(lamE, relE, kNE, kEmi, 20);
    gE->Draw("LP SAME");
    gA->Draw("LP SAME");
    auto* l1 = new TLegend(0.10, 0.80, 0.99, 0.99);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.050);
    l1->AddEntry(gA, "Y-11 absorption (430 nm)", "lp");
    l1->AddEntry(gE, "Y-11 emission (476 nm)", "lp");
    l1->Draw();
    TLine* lp1 = new TLine(430., 0., 430., 1.05);
    lp1->SetLineColor(kGray + 1); lp1->SetLineStyle(2); lp1->Draw("SAME");
    TLine* lp2 = new TLine(476., 0., 476., 1.05);
    lp2->SetLineColor(kGray + 2); lp2->SetLineStyle(3); lp2->Draw("SAME");
    TLine* onset = new TLine(486., 0., 486., 1.15);
    onset->SetLineColor(kBlack); onset->SetLineStyle(7); onset->Draw("SAME");
    auto* t = new TLatex(0.60, 0.24,
                         "G4OpWLS: E_{#gamma'} #leq E_{#gamma}");
    t->SetNDC(); t->SetTextSize(0.048); t->Draw();

    // ---- Panel 2: re-emission delay profile ----------------------------
    c->cd(2);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.24);
    TH1F* f2 = gPad->DrawFrame(0., 0., 40., 1.12,
                               ";re-emission delay [ns];probability density (a.u.)");
    f2->GetYaxis()->SetTitleOffset(1.15);
    f2->GetYaxis()->SetTitleSize(0.060); f2->GetXaxis()->SetTitleSize(0.060);
    TF1* expo = new TF1("expoWLS", "exp(-x/7.0)", 0., 40.);
    expo->SetLineColor(kEmi);
    expo->SetLineWidth(2);
    expo->Draw("SAME");
    TLine* delta = new TLine(7., 0., 7., 1.0);
    delta->SetLineColor(kAbs);
    delta->SetLineWidth(2);
    delta->SetLineStyle(2);
    delta->Draw("SAME");
    auto* l2 = new TLegend(0.24, 0.80, 0.99, 0.99);
    l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextSize(0.050);
    l2->AddEntry(expo, "exponential, #tau = 7 ns (used)", "l");
    l2->AddEntry(delta, "delta @ 7 ns (historical)", "l");
    l2->Draw();

    c->SaveAs(TString::Format("%s/wls_process.pdf", outdir));
    c->SaveAs(TString::Format("%s/wls_process.png", outdir));
    printf("wls_process: wrote %s/wls_process.{pdf,png}\n", outdir);
}
