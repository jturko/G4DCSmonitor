// scint_timing.C -- the scintillation light-pulse shape of the PVT scintillator
// as configured in GeometryMuonScint.cc (BuildOpticalProperties):
// SCINTILLATIONTIMECONSTANT1 = 2.1 ns (decay), SCINTILLATIONRISETIME1 = 0.9 ns.
// The pulse is I(t) = (1 - exp(-t/tau_rise)) * exp(-t/tau_decay), shown on a
// linear scale (left) and a logarithmic scale that exposes the rise (right).
//
// These constants only matter for timing studies (the arrival-time persistence
// and wavelength-vs-time maps); they do not affect the muon photoelectron yield.
//
// Usage:
//   root -l -b -q 'macros/scint_timing.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TText.h>
#include <TMath.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include "style.h"

namespace {

const double kRise  = 0.9;   // ns
const double kDecay = 2.1;   // ns
const double kTMax  = 15.0;  // ns
const int    kN     = 1501;

double pulse(double t)
{
    return (1.0 - TMath::Exp(-t / kRise)) * TMath::Exp(-t / kDecay);
}

}  // namespace

void scint_timing(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    // Build the normalised pulse.
    double vmax = 0.0;
    double tpeak = 0.0;
    for (int i = 0; i < kN; ++i) {
        const double t = kTMax * i / (kN - 1);
        const double v = pulse(t);
        if (v > vmax) { vmax = v; tpeak = t; }
    }
    auto* g = new TGraph(kN);
    for (int i = 0; i < kN; ++i) {
        const double t = kTMax * i / (kN - 1);
        g->SetPoint(i, t, pulse(t) / vmax);
    }
    const int kScint = PetroffPalette::getP6(0);   // blue

    TCanvas* c = new TCanvas("c_scint_timing", "scintillator timing", 1120, 540);
    c->Divide(2, 1, 0.004, 0.004);

    // ---- Panel 1: linear scale -----------------------------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.22);
    TH1F* f1 = gPad->DrawFrame(0., 0., kTMax, 1.10,
                               ";time [ns];intensity (normalised)");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.060); f1->GetXaxis()->SetTitleSize(0.060);
    auto* g1 = new TGraph(*g);
    g1->SetLineColor(kScint); g1->SetLineWidth(2);
    g1->Draw("L SAME");
    TLine* pl = new TLine(tpeak, 0., tpeak, 1.0);
    pl->SetLineColor(kGray + 1); pl->SetLineStyle(2); pl->Draw("SAME");
    auto* l1 = new TLegend(0.06, 0.81, 0.99, 0.99);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.048);
    l1->AddEntry(g1, "I(t), rise 0.9 ns / decay 2.1 ns", "l");
    l1->Draw();

    // ---- Panel 2: logarithmic scale ------------------------------------
    c->cd(2);
    gPad->SetLogy();
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.22);
    TH1F* f2 = gPad->DrawFrame(0., 1e-3, kTMax, 2.0,
                               ";time [ns];intensity (normalised)");
    f2->GetYaxis()->SetTitleOffset(1.15);
    f2->GetYaxis()->SetTitleSize(0.060); f2->GetXaxis()->SetTitleSize(0.060);
    auto* note2 = new TText(0.06, 0.87, "logarithmic ordinate");
    note2->SetNDC(); note2->SetTextSize(0.048); note2->Draw();
    auto* g2 = new TGraph(*g);
    g2->SetLineColor(kScint); g2->SetLineWidth(2);
    g2->Draw("L SAME");

    c->SaveAs(TString::Format("%s/scint_timing.pdf", outdir));
    c->SaveAs(TString::Format("%s/scint_timing.png", outdir));
    printf("scint_timing: wrote %s/scint_timing.{pdf,png}\n", outdir);
}
