// optical_spectra.C -- the optical property tables actually fed to Geant4 in
// GeometryMuonScint.cc, plotted versus wavelength:
//
//   * scintillator (PVT / UPS-92S / EJ-200): SCINTILLATIONCOMPONENT1 emission
//     and bulk ABSLENGTH (attenuation length);
//   * WLS fibre core (Kuraray Y-11): the G4OpWLS WLSABSLENGTH absorption and
//     WLSCOMPONENT re-emission;
//   * SiPM (Cefla ASD-NUV4S-P-40): the photocathode EFFICIENCY = PDE(lambda).
//
// The numbers are copied verbatim from src/GeometryMuonScint.cc
// (BuildOpticalProperties, BuildOpticalSurfaces, BuildWLSFibres); the energy
// grids there are ascending in eV, and are converted here to vacuum wavelength
// via lambda[nm] = 1239.841984 / E[eV].  Geant4 linearly interpolates these
// tables, so the plotted straight segments are exactly what the transport uses.
//
// Usage:
//   root -l -b -q 'macros/optical_spectra.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TGraph.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TMultiGraph.h>

#include "style.h"

namespace {

const double kHc = 1239.841984;   // eV * nm

struct Spec { const char* name; int color; int marker; };

// Scintillator (N = 14), ascending eV.
const double scintE[14] = {1.771,2.067,2.156,2.250,2.339,2.450,2.594,2.755,
                           2.917,3.024,3.100,3.300,3.543,4.000};
const double scintEmis[14] = {0.00,0.02,0.05,0.10,0.18,0.32,0.55,0.85,
                              1.00,0.75,0.30,0.02,0.00,0.00};
const double scintAbs[14]  = {3.0,3.5,3.7,3.8,3.8,3.8,3.8,3.5,
                              2.5,1.0,0.30,0.05,0.005,0.0005};   // [m]

// WLS fibre core (N = 13).
const double wlsE[13] = {1.907,2.066,2.138,2.214,2.296,2.385,2.480,
                         2.583,2.695,2.818,2.952,3.100,3.263};
const double wlsAbsL[13] = {1e6,1e6,1e6,1e6,1e6,1e6,1e6,
                            0.60,0.20,0.05,0.005,0.0005,0.00005}; // [m]
const double wlsEmis[13] = {0.00,0.03,0.08,0.18,0.40,0.70,0.95,
                            1.00,0.55,0.00,0.00,0.00,0.00};

// SiPM photocathode (Np = 11).
const double sipmE[11]  = {2.067,2.156,2.250,2.339,2.450,2.594,
                           2.755,2.917,3.100,3.300,3.543};
const double sipmPDE[11] = {0.20,0.23,0.27,0.31,0.35,0.40,
                            0.43,0.43,0.40,0.30,0.18};

TGraph* makeGraph(const double* E, const double* v, int n,
                  int color, int marker)
{
    auto* g = new TGraph(n);
    for (int i = 0; i < n; ++i) g->SetPoint(i, kHc / E[i], v[i]);
    g->SetLineColor(color);
    g->SetLineWidth(2);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.9);
    return g;
}

const int kScint = PetroffPalette::getP6(0);   // blue
const int kWls   = PetroffPalette::getP6(4);   // grey-green
const int kSiPM  = PetroffPalette::getP6(2);   // red

}  // namespace

void optical_spectra(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_optical_spectra", "optical spectra", 1290, 520);
    c->Divide(3, 1, 0.004, 0.004);

    // ---- Panel 1: emission spectra (normalised) ------------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f1 = gPad->DrawFrame(310., 0., 700., 1.15,
                               ";wavelength [nm];emission (normalised)");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.06);  f1->GetXaxis()->SetTitleSize(0.06);
    TGraph* gScintE = makeGraph(scintE, scintEmis, 14, kScint, 20);
    TGraph* gWlsE   = makeGraph(wlsE,   wlsEmis,   13, kWls,   21);
    gScintE->Draw("LP SAME");
    gWlsE->Draw("LP SAME");
    auto* l1 = new TLegend(0.12, 0.79, 0.90, 0.97);
    l1->SetBorderSize(0);
    l1->SetFillStyle(0);
    l1->SetTextSize(0.05);
    l1->AddEntry(gScintE, "scintillator (PVT)", "lp");
    l1->AddEntry(gWlsE,   "WLS fibre Y-11", "lp");
    l1->Draw();
    TLine* peak1 = new TLine(425., 0., 425., 1.05);
    peak1->SetLineColor(kGray + 1); peak1->SetLineStyle(2); peak1->Draw("SAME");

    // ---- Panel 2: absorption / attenuation lengths (log) ---------------
    c->cd(2);
    gPad->SetLogy();
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f2 = gPad->DrawFrame(310., 1e-5, 700., 1e7,
                               ";wavelength [nm];attenuation length [m]");
    f2->GetYaxis()->SetTitleOffset(1.05);
    f2->GetYaxis()->SetTitleSize(0.06);  f2->GetXaxis()->SetTitleSize(0.06);
    TGraph* gScintA = makeGraph(scintE, scintAbs, 14, kScint, 20);
    TGraph* gWlsA   = makeGraph(wlsE,   wlsAbsL,  13, kWls,   21);
    gScintA->Draw("LP SAME");
    gWlsA->Draw("LP SAME");
    auto* l2 = new TLegend(0.05, 0.78, 0.97, 0.98);
    l2->SetBorderSize(0);
    l2->SetFillStyle(0);
    l2->SetTextSize(0.05);
    l2->AddEntry(gScintA, "scint. bulk absorption", "lp");
    l2->AddEntry(gWlsA,   "Y-11 WLS absorption", "lp");
    l2->Draw();
    TLine* onset = new TLine(480., 1e-5, 480., 1e7);
    onset->SetLineColor(kGray + 1); onset->SetLineStyle(2); onset->Draw("SAME");

    // ---- Panel 3: SiPM PDE ---------------------------------------------
    c->cd(3);
    gPad->SetLeftMargin(0.15); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f3 = gPad->DrawFrame(340., 0., 620., 0.50,
                               ";wavelength [nm];photodetection efficiency");
    f3->GetYaxis()->SetTitleOffset(1.15);
    f3->GetYaxis()->SetTitleSize(0.06);  f3->GetXaxis()->SetTitleSize(0.06);
    TGraph* gPDE = makeGraph(sipmE, sipmPDE, 11, kSiPM, 20);
    gPDE->Draw("LP SAME");
    auto* l3 = new TLegend(0.20, 0.81, 0.82, 0.96);
    l3->SetBorderSize(0);
    l3->SetFillStyle(0);
    l3->SetTextSize(0.05);
    l3->AddEntry(gPDE, "ASD-NUV4S-P-40", "lp");
    l3->Draw();
    TLine* peak3 = new TLine(420., 0., 420., 0.46);
    peak3->SetLineColor(kGray + 1); peak3->SetLineStyle(2); peak3->Draw("SAME");

    c->SaveAs(TString::Format("%s/optical_spectra.pdf", outdir));
    c->SaveAs(TString::Format("%s/optical_spectra.png", outdir));
    printf("optical_spectra: wrote %s/optical_spectra.{pdf,png}\n", outdir);
}
