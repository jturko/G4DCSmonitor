// macro_optical_spectra.C -- the optical property tables actually fed to Geant4 in
// GeometryMuonScint.cc, plotted versus wavelength:
//
//   * scintillator (PVT / UPS-92S / EJ-200): SCINTILLATIONCOMPONENT1 emission
//     and bulk ABSLENGTH (attenuation length);
//   * WLS fibre core (Kuraray Y-11): the digitized WLS absorption band
//     (WLSABSLENGTH) and the WLSCOMPONENT re-emission spectrum.  Both shapes
//     are drawn in the left panel so that the absorption/emission overlap is
//     visible; the absolute attenuation lengths are in the middle panel;
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
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include "style.h"

namespace {

const double kHc = 1239.841984;   // eV * nm

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

// Tables already indexed by wavelength [nm].
TGraph* makeGraphLam(const double* lam, const double* v, int n,
                     int color, int marker)
{
    auto* g = new TGraph(n);
    for (int i = 0; i < n; ++i) g->SetPoint(i, lam[i], v[i]);
    g->SetLineColor(color);
    g->SetLineWidth(2);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.9);
    return g;
}

// Scintillator (N = 14), ascending eV.
const double scintE[14] = {1.771,2.067,2.156,2.250,2.339,2.450,2.594,2.755,
                           2.917,3.024,3.100,3.300,3.543,4.000};
const double scintEmis[14] = {0.00,0.02,0.05,0.10,0.18,0.32,0.55,0.85,
                              1.00,0.75,0.30,0.02,0.00,0.00};
const double scintAbs[14]  = {3.0,3.5,3.7,3.8,3.8,3.8,3.8,3.5,
                              2.5,1.0,0.30,0.05,0.005,0.0005};   // [m]

// WLS fibre core (Kuraray Y-11), digitized Kuraray spectra, indexed by nm.
const int kNA = 37;
const double lamA[37] = {650,500,488,486,483,478,474,472,470,467,465,462,459,
                         457,453,450,448,444,442,440,437,435,430,427,423,420,
                         415,410,405,400,396,390,386,380,370,360,350};
const double relA[37] = {0.0,0.0,0.0,0.005,0.015,0.054,0.11,0.20,0.31,0.45,
                         0.59,0.71,0.80,0.85,0.88,0.85,0.81,0.77,0.79,0.84,
                         0.90,0.96,1.00,0.93,0.84,0.75,0.64,0.59,0.51,0.44,
                         0.37,0.29,0.24,0.15,0.075,0.045,0.03};
const int kNE = 36;
const double lamE[36] = {650,640,620,600,590,580,570,560,550,540,531,526,520,
                         516,512,507,503,500,495,490,487,485,482,479,476,473,
                         471,469,466,463,461,458,456,452,449,444};
const double relE[36] = {0.0,0.0,0.005,0.02,0.03,0.05,0.085,0.13,0.19,0.25,
                         0.31,0.43,0.55,0.63,0.71,0.77,0.76,0.73,0.69,0.73,
                         0.80,0.85,0.92,0.98,1.00,0.92,0.82,0.67,0.50,0.32,
                         0.21,0.14,0.09,0.04,0.01,0.0};
const double kL0mm = 1.0;   // WLS absorption length at the 430 nm peak [mm]

// SiPM photocathode (Np = 25), digitized AdvanSiD Fig. 8.
const double sipmE[25]  = {1.653,1.771,1.907,2.066,2.156,2.254,2.362,2.431,
                           2.480,2.530,2.583,2.638,2.695,2.755,2.818,2.883,
                           2.952,3.024,3.100,3.139,3.179,3.220,3.263,3.306,
                           3.443};
const double sipmPDE[25] = {0.086,0.110,0.146,0.178,0.197,0.224,0.247,0.266,
                            0.284,0.300,0.317,0.330,0.343,0.360,0.385,0.408,
                            0.426,0.431,0.418,0.380,0.345,0.280,0.210,0.150,
                            0.080};

const int kScint = PetroffPalette::getP6(0);   // blue
const int kWls   = PetroffPalette::getP6(4);   // grey-green
const int kAbs   = PetroffPalette::getP6(2);   // red
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

    // ---- Panel 1: spectral shapes (normalised) -------------------------
    c->cd(1);
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f1 = gPad->DrawFrame(310., 0., 700., 1.15,
                               ";wavelength [nm];normalised amplitude");
    f1->GetYaxis()->SetTitleOffset(1.15);
    f1->GetYaxis()->SetTitleSize(0.06);  f1->GetXaxis()->SetTitleSize(0.06);
    TGraph* gScintE = makeGraph(scintE, scintEmis, 14, kScint, 20);
    TGraph* gWlsE   = makeGraphLam(lamE, relE, kNE, kWls, 21);
    TGraph* gWlsA   = makeGraphLam(lamA, relA, kNA, kAbs, 22);
    gWlsA->Draw("LP SAME");
    gScintE->Draw("LP SAME");
    gWlsE->Draw("LP SAME");
    auto* l1 = new TLegend(0.10, 0.74, 0.99, 0.98);
    l1->SetBorderSize(0);
    l1->SetFillStyle(0);
    l1->SetTextSize(0.045);
    l1->AddEntry(gScintE, "scintillator emission (PVT)", "lp");
    l1->AddEntry(gWlsA,   "Y-11 absorption (430 nm)", "lp");
    l1->AddEntry(gWlsE,   "Y-11 emission (476 nm)", "lp");
    l1->Draw();
    TLine* peak1 = new TLine(425., 0., 425., 1.05);
    peak1->SetLineColor(kGray + 1); peak1->SetLineStyle(2); peak1->Draw("SAME");
    TLine* peak1b = new TLine(476., 0., 476., 1.05);
    peak1b->SetLineColor(kGray + 2); peak1b->SetLineStyle(3); peak1b->Draw("SAME");

    // ---- Panel 2: absorption / attenuation lengths (log) ---------------
    c->cd(2);
    gPad->SetLogy();
    gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f2 = gPad->DrawFrame(310., 1e-5, 700., 1e7,
                               ";wavelength [nm];attenuation length [m]");
    f2->GetYaxis()->SetTitleOffset(1.05);
    f2->GetYaxis()->SetTitleSize(0.06);  f2->GetXaxis()->SetTitleSize(0.06);
    double wlsAbsL[37];
    for (int i = 0; i < 37; ++i)
        wlsAbsL[i] = (relA[i] > 0.) ? (kL0mm * 1e-3 / relA[i]) : 1e6;
    TGraph* gScintA = makeGraph(scintE, scintAbs, 14, kScint, 20);
    TGraph* gWlsAbs   = makeGraphLam(lamA, wlsAbsL, kNA, kWls, 21);
    gScintA->Draw("LP SAME");
    gWlsAbs->Draw("LP SAME");
    auto* l2 = new TLegend(0.05, 0.78, 0.97, 0.98);
    l2->SetBorderSize(0);
    l2->SetFillStyle(0);
    l2->SetTextSize(0.05);
    l2->AddEntry(gScintA, "scint. bulk absorption", "lp");
    l2->AddEntry(gWlsAbs,   "Y-11 WLS absorption", "lp");
    l2->Draw();
    TLine* onset = new TLine(486., 1e-5, 486., 1e7);
    onset->SetLineColor(kGray + 1); onset->SetLineStyle(2); onset->Draw("SAME");

    // ---- Panel 3: SiPM PDE ---------------------------------------------
    c->cd(3);
    gPad->SetLeftMargin(0.15); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.16); gPad->SetTopMargin(0.24);
    TH1F* f3 = gPad->DrawFrame(350., 0., 760., 0.50,
                               ";wavelength [nm];photodetection efficiency");
    f3->GetYaxis()->SetTitleOffset(1.15);
    f3->GetYaxis()->SetTitleSize(0.06);  f3->GetXaxis()->SetTitleSize(0.06);
    TGraph* gPDE = makeGraph(sipmE, sipmPDE, 25, kSiPM, 20);
    gPDE->Draw("LP SAME");
    auto* l3 = new TLegend(0.20, 0.81, 0.82, 0.96);
    l3->SetBorderSize(0);
    l3->SetFillStyle(0);
    l3->SetTextSize(0.05);
    l3->AddEntry(gPDE, "ASD-NUV4S-P-40", "lp");
    l3->Draw();
    TLine* peak3 = new TLine(410., 0., 410., 0.46);
    peak3->SetLineColor(kGray + 1); peak3->SetLineStyle(2); peak3->Draw("SAME");

    c->SaveAs(TString::Format("%s/optical_spectra.pdf", outdir));
    c->SaveAs(TString::Format("%s/optical_spectra.png", outdir));
    printf("optical_spectra: wrote %s/optical_spectra.{pdf,png}\n", outdir);
}
