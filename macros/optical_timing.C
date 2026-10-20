// optical_timing.C -- time structure of the two optical sources used in the
// simulation, plotted from the same parameters as GeometryMuonScint.cc:
//
//   * the PVT scintillation pulse
//       I(t) = (1 - exp(-t/tau_rise)) * exp(-t/tau_decay)
//     with tau_rise = 0.9 ns and tau_decay = 2.1 ns, shown on a linear
//     ordinate (left) with a logarithmic-ordinate inset in the upper-right
//     corner so the same pulse carries both views without a separate panel;
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

    // Two panels: the PVT pulse (linear + log inset) and the WLS delay.
    // The earlier three-panel layout carried a redundant "same pulse, log
    // ordinate" legend on the middle panel; replacing the middle panel with
    // an inset in the linear view keeps both the absolute scale (linear)
    // and the dynamic range (log) on a single pulse drawing.
    TCanvas* c = new TCanvas("c_optical_timing", "optical timing", 1290, 520);
    c->Divide(2, 1, 0.004, 0.004);

    // ---- Panel 1: scintillation pulse (linear, with log inset) --------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.22);
    double tpeak = 0.0;
    TGraph* gLin = makePulse(0.0, &tpeak, nullptr);
    gLin->SetLineColor(kScint); gLin->SetLineWidth(2);
    TH1F* f1 = gPad->DrawFrame(0., 0., kTMax, 1.10,
                               ";time [ns];intensity (normalised)");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.060);  f1->GetXaxis()->SetTitleSize(0.060);
    f1->GetXaxis()->SetLabelSize(0.050);
    f1->GetYaxis()->SetLabelSize(0.050);
    gLin->Draw("L SAME");
    TLine* pl = new TLine(tpeak, 0., tpeak, 1.0);
    pl->SetLineColor(kGray + 1); pl->SetLineStyle(2); pl->Draw("SAME");

    // "peak X.XX ns" sits inside the pad, just to the right of the peak
    // marker on the curve so the reader can read the peak time without
    // leaving the plot area.
    auto* t1 = new TLatex(tpeak + 0.6, 0.92, Form("peak %.2f ns", tpeak));
    t1->SetTextSize(0.045); t1->Draw();

    // Legend in the top margin (lowered from the previous 0.30 top-margin
    // band; 0.22 leaves the legend just above the plot axes).
    auto* l1 = new TLegend(0.10, 0.83, 0.97, 0.97);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.045);
    l1->AddEntry(gLin, "PVT pulse (rise 0.9 / decay 2.1 ns)", "l");
    l1->Draw();

    // Inset: same pulse on a log-y axis, in the upper-right corner of the
    // plot area.  NDC (0.42, 0.25, 0.97, 0.80) makes the inset a bit larger
    // than the previous (0.46, 0.30, 0.96, 0.78) so the larger axis labels
    // and annotation text fit comfortably without overlapping the curve.
    TPad* inset = new TPad("inset_log", "log inset", 0.42, 0.25, 0.97, 0.80);
    inset->SetFillStyle(0);
    inset->SetFrameBorderSize(1);
    inset->SetLogy();
    inset->SetLeftMargin(0.13); inset->SetRightMargin(0.04);
    inset->SetBottomMargin(0.18); inset->SetTopMargin(0.04);
    inset->Draw();
    inset->cd();
    TGraph* gLog = makePulse(0.02, nullptr, nullptr);
    gLog->SetLineColor(kScint); gLog->SetLineWidth(2);
    gLog->Draw("AL");
    gLog->Draw("L SAME");
    // Make the inset's axis labels readable too: the default label size
    // (0.055 globally) is too small relative to the 0.065 annotation text.
    gLog->GetXaxis()->SetLabelSize(0.075);
    gLog->GetYaxis()->SetLabelSize(0.075);
    gLog->GetXaxis()->SetLabelOffset(0.02);
    gLog->GetYaxis()->SetLabelOffset(0.02);
    // The exponential-tail annotation lives inside the inset, in the
    // high-t region where the curve is roughly straight on the log axis.
    // Data coordinates keep the text clear of the y-axis labels and above
    // the curve.  At t=8 ns the log pulse is around 10^-3.4; y=0.5 sits
    // well above the curve.
    auto* t1b = new TLatex(8.0, 0.5, "tail #propto e^{-t/2.1 ns}");
    t1b->SetTextSize(0.090); t1b->Draw();

    // ---- Panel 2: WLS re-emission delay --------------------------------
    c->cd(2);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.22);
    TH1F* f3 = gPad->DrawFrame(0., 0., 40., 1.12,
                               ";re-emission delay [ns];probability density (a.u.)");
    f3->GetYaxis()->SetTitleOffset(1.15);
    f3->GetYaxis()->SetTitleSize(0.060);  f3->GetXaxis()->SetTitleSize(0.060);
    f3->GetXaxis()->SetLabelSize(0.050);
    f3->GetYaxis()->SetLabelSize(0.050);
    TF1* expo = new TF1("expoWLS", "exp(-x/7.0)", 0., 40.);
    expo->SetLineColor(kWls);
    expo->SetLineWidth(2);
    expo->Draw("SAME");
    TLine* delta = new TLine(7., 0., 7., 1.0);
    delta->SetLineColor(kLegacy);
    delta->SetLineWidth(2);
    delta->SetLineStyle(2);
    delta->Draw("SAME");

    // Annotations inside the pad: the tau label on the exponential tail
    // and a "fixed @ 7 ns" callout near the historical delta line.
    auto* t2a = new TLatex(15.0, 0.18, "#tau = 7 ns");
    t2a->SetTextSize(0.050); t2a->Draw();
    auto* t2b = new TLatex(9.0, 0.80, "fixed @ 7 ns");
    t2b->SetTextSize(0.045); t2b->Draw();

    auto* l3 = new TLegend(0.10, 0.83, 0.97, 0.97);
    l3->SetBorderSize(0); l3->SetFillStyle(0); l3->SetTextSize(0.045);
    l3->AddEntry(expo,  "exponential, #tau = 7 ns (used)", "l");
    l3->AddEntry(delta, "delta @ 7 ns (historical)", "l");
    l3->Draw();

    c->SaveAs(TString::Format("%s/optical_timing.pdf", outdir));
    c->SaveAs(TString::Format("%s/optical_timing.png", outdir));
    printf("optical_timing: wrote %s/optical_timing.{pdf,png}\n", outdir);
}
