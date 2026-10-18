// optical_timing.C -- time structure of the two optical sources used in the
// simulation, plotted from the same parameters as GeometryMuonScint.cc:
//
//   * the PVT scintillation pulse
//       I(t) = (1 - exp(-t/tau_rise)) * exp(-t/tau_decay)
//     with tau_rise = 0.9 ns and tau_decay = 2.1 ns, shown on a linear
//     (left) and a logarithmic (middle) ordinate;
//   * the Kuraray Y-11 WLS re-emission delay (right), exponential with
//     tau = 7 ns as used in production, compared with the historical
//     fixed-delay ("delta") profile.
//
// The wavelength-dependent WLS input tables (absorption and emission) live in
// optical_spectra.C; this macro only carries the timing information.
//
// Usage:
//   root -l -b -q 'macros/optical_timing.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TF1.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TLatex.h>
#include <TMath.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include "style.h"

namespace {

const double kRise  = 0.9;    // ns
const double kDecay = 2.1;    // ns
const double kTMax  = 15.0;   // ns
const int    kN     = 1501;

double pulse(double t)
{
    return (1.0 - TMath::Exp(-t / kRise)) * TMath::Exp(-t / kDecay);
}

TGraph* makePulse(double tmin, double* tpeak, double* vmax)
{
    double peak = 0.0;
    for (int i = 0; i < kN; ++i) {
        const double t = tmin + (kTMax - tmin) * i / (kN - 1);
        const double v = pulse(t);
        if (v > peak) { peak = v; if (tpeak) *tpeak = t; }
    }
    auto* g = new TGraph(kN);
    for (int i = 0; i < kN; ++i) {
        const double t = tmin + (kTMax - tmin) * i / (kN - 1);
        g->SetPoint(i, t, pulse(t) / peak);
    }
    if (vmax) *vmax = peak;
    return g;
}

const int kScint  = PetroffPalette::getP6(0);   // blue
const int kWls    = PetroffPalette::getP6(4);   // grey-green
const int kLegacy = PetroffPalette::getP6(2);   // red

}  // namespace

void optical_timing(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_optical_timing", "optical timing", 1290, 520);
    c->Divide(3, 1, 0.004, 0.004);

    // ---- Panel 1: scintillation pulse, linear scale --------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    double tpeak = 0.0;
    TGraph* gLin = makePulse(0.0, &tpeak, nullptr);
    gLin->SetLineColor(kScint); gLin->SetLineWidth(2);
    TH1F* f1 = gPad->DrawFrame(0., 0., kTMax, 1.10,
                               ";time [ns];intensity (normalised)");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.06);  f1->GetXaxis()->SetTitleSize(0.06);
    gLin->Draw("L SAME");
    TLine* pl = new TLine(tpeak, 0., tpeak, 1.0);
    pl->SetLineColor(kGray + 1); pl->SetLineStyle(2); pl->Draw("SAME");
    auto* l1 = new TLegend(0.08, 0.79, 0.99, 0.99);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.047);
    l1->AddEntry(gLin, "PVT pulse (rise 0.9 / decay 2.1 ns)", "l");
    l1->Draw();
    auto* t1 = new TLatex(0.62, 0.67, Form("peak %.2f ns", tpeak));
    t1->SetNDC(); t1->SetTextSize(0.045); t1->Draw();

    // ---- Panel 2: scintillation pulse, logarithmic scale ---------------
    c->cd(2);
    gPad->SetLogy();
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TGraph* gLog = makePulse(0.02, nullptr, nullptr);
    gLog->SetLineColor(kScint); gLog->SetLineWidth(2);
    TH1F* f2 = gPad->DrawFrame(0., 1e-3, kTMax, 1.2,
                               ";time [ns];intensity (normalised)");
    f2->GetYaxis()->SetTitleOffset(1.15);
    f2->GetYaxis()->SetTitleSize(0.06);  f2->GetXaxis()->SetTitleSize(0.06);
    gLog->Draw("L SAME");
    auto* l2 = new TLegend(0.08, 0.84, 0.99, 0.99);
    l2->SetBorderSize(0); l2->SetFillStyle(0); l2->SetTextSize(0.047);
    l2->AddEntry(gLog, "same pulse, logarithmic ordinate", "l");
    l2->Draw();
    auto* t2 = new TLatex(0.50, 0.66, "tail #propto e^{-t/2.1 ns}");
    t2->SetNDC(); t2->SetTextSize(0.045); t2->Draw();

    // ---- Panel 3: WLS re-emission delay --------------------------------
    c->cd(3);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f3 = gPad->DrawFrame(0., 0., 40., 1.12,
                               ";re-emission delay [ns];probability density (a.u.)");
    f3->GetYaxis()->SetTitleOffset(1.15);
    f3->GetYaxis()->SetTitleSize(0.06);  f3->GetXaxis()->SetTitleSize(0.06);
    TF1* expo = new TF1("expoWLS", "exp(-x/7.0)", 0., 40.);
    expo->SetLineColor(kWls);
    expo->SetLineWidth(2);
    expo->Draw("SAME");
    TLine* delta = new TLine(7., 0., 7., 1.0);
    delta->SetLineColor(kLegacy);
    delta->SetLineWidth(2);
    delta->SetLineStyle(2);
    delta->Draw("SAME");
    auto* l3 = new TLegend(0.10, 0.78, 0.99, 0.99);
    l3->SetBorderSize(0); l3->SetFillStyle(0); l3->SetTextSize(0.047);
    l3->AddEntry(expo,  "exponential, #tau = 7 ns (used)", "l");
    l3->AddEntry(delta, "delta @ 7 ns (historical)", "l");
    l3->Draw();

    c->SaveAs(TString::Format("%s/optical_timing.pdf", outdir));
    c->SaveAs(TString::Format("%s/optical_timing.png", outdir));
    printf("optical_timing: wrote %s/optical_timing.{pdf,png}\n", outdir);
}
