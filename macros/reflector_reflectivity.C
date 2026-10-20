// reflector_reflectivity.C -- the tabulated reflectivity R(lambda) attached to
// each of the four reflector treatments in GeometryMuonScint.cc
// (BuildOpticalSurfaces), plotted on the shared ten-point 360-650 nm grid.
//
//   * Left:   R(lambda) for the three reflective wrappers (TiO2 paint,
//             aluminium foil, high-gloss photo paper).  The bare plate carries
//             no reflector and is drawn as the lossless-TIR guide the polished
//             plate becomes.
//   * Middle: R at the scintillation emission peak (425 nm), as a bar chart.
//   * Right:  R at the WLS fibre emission peak (476 nm), the wavelength that
//             matters for light re-emitted by the config-4 fibres.
//
// Usage:
//   root -l -b -q 'macros/reflector_reflectivity.C("report/figures")'

#include <TCanvas.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TText.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include "style.h"

namespace {

const double kHc = 1239.841984;   // eV * nm
const int N = 10;

// Shared grid, ascending eV (650 ... 360 nm).
const double E[N] = {1.907,2.066,2.254,2.480,2.638,2.818,2.952,3.100,3.263,3.444};
const double R_TiO2[N]   = {0.975,0.980,0.982,0.983,0.983,0.982,0.980,0.977,0.970,0.955};
const double R_Al[N]     = {0.904,0.911,0.916,0.919,0.921,0.923,0.924,0.924,0.925,0.925};
const double R_glossy[N] = {0.940,0.950,0.955,0.955,0.955,0.950,0.945,0.940,0.925,0.900};

// Linear interpolation of R at a target wavelength, grid must be wavelength-
// ascending; we build the wavelength array explicitly.
double RAt(const double* R, double lamTarget)
{
    double lam[N];
    for (int i = 0; i < N; ++i) lam[i] = kHc / E[N - 1 - i];   // 360 ... 650 nm
    for (int i = 0; i + 1 < N; ++i) {
        const double lo = lam[i], hi = lam[i + 1];
        if (lamTarget >= lo && lamTarget <= hi) {
            const double f = (lamTarget - lo) / (hi - lo);
            return R[N - 1 - i] + f * (R[N - 2 - i] - R[N - 1 - i]);
        }
    }
    return R[N - 1];
}

TGraph* mkR(const double* R, int color, int marker)
{
    auto* g = new TGraph(N);
    for (int i = 0; i < N; ++i) g->SetPoint(i, kHc / E[i], R[i]);
    g->SetLineColor(color);
    g->SetLineWidth(2);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.9);
    return g;
}

const int kTiO2   = PetroffPalette::getP6(5);   // purple
const int kAl     = PetroffPalette::getP6(1);   // orange
const int kGlossy = PetroffPalette::getP6(0);   // blue
const int kBare   = PetroffPalette::getP6(4);   // grey

// Draw the four-treatment bar chart at one wavelength.
void drawRbars(const char* name, double lam, const char* note)
{
    auto* h = new TH1F(name, ";treatment;reflectivity #font[132]{#it{R}}",
                       4, 0., 4.);
    h->SetDirectory(0);
    h->SetMinimum(0.85);
    h->SetMaximum(1.02);
    h->SetBinContent(1, 1.0);           // bare: TIR guide (not a reflector)
    h->SetBinContent(2, RAt(R_TiO2,   lam));
    h->SetBinContent(3, RAt(R_Al,     lam));
    h->SetBinContent(4, RAt(R_glossy, lam));
    h->GetXaxis()->SetBinLabel(1, "bare*");
    h->GetXaxis()->SetBinLabel(2, "TiO_{2}");
    h->GetXaxis()->SetBinLabel(3, "Al");
    h->GetXaxis()->SetBinLabel(4, "glossy");
    h->GetYaxis()->SetTitleOffset(1.30);
    h->GetYaxis()->SetTitleSize(0.060); h->GetXaxis()->SetTitleSize(0.060);
    h->GetYaxis()->SetLabelSize(0.050); h->GetXaxis()->SetLabelSize(0.065);
    h->GetXaxis()->SetLabelOffset(0.02);
    const int cols[4] = {kBare, kTiO2, kAl, kGlossy};
    for (int i = 0; i < 4; ++i) h->SetFillColor(cols[i]);
    h->SetBarWidth(0.6);
    h->SetBarOffset(0.2);
    h->SetMarkerSize(2.4);              // bar-value text size (TEXT0 option)
    h->SetMarkerColor(kBlack);
    gStyle->SetPaintTextFormat("3.3f");
    h->Draw("BAR");
    h->Draw("TEXT0 SAME");
    auto* t = new TLatex(0.16, 0.90, note);
    t->SetNDC(); t->SetTextSize(0.045); t->Draw("SAME");
}

}  // namespace

void reflector_reflectivity(const char* outdir = "report/figures")
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_refl", "reflector reflectivity", 1290, 520);
    c->Divide(3, 1, 0.004, 0.004);

    // ---- Panel 1: R(lambda) --------------------------------------------
    c->cd(1);
    gPad->SetLeftMargin(0.18); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.20); gPad->SetTopMargin(0.28);
    TH1F* f1 = gPad->DrawFrame(360., 0.88, 650., 1.008,
                               ";wavelength [nm];reflectivity #font[132]{#it{R}}");
    f1->GetYaxis()->SetTitleOffset(1.30);
    f1->GetYaxis()->SetTitleSize(0.060); f1->GetXaxis()->SetTitleSize(0.060);
    f1->GetXaxis()->SetLabelSize(0.050);
    f1->GetYaxis()->SetLabelSize(0.050);
    TLine* bare = new TLine(360., 1.0, 650., 1.0);
    bare->SetLineColor(kBare); bare->SetLineWidth(2); bare->SetLineStyle(2);
    bare->Draw("SAME");
    TGraph* gT = mkR(R_TiO2,   kTiO2,   20);
    TGraph* gA = mkR(R_Al,     kAl,     21);
    TGraph* gG = mkR(R_glossy, kGlossy, 22);
    gT->Draw("LP SAME");
    gA->Draw("LP SAME");
    gG->Draw("LP SAME");
    auto* l1 = new TLegend(0.03, 0.84, 0.99, 0.99);
    l1->SetBorderSize(0); l1->SetFillStyle(0); l1->SetTextSize(0.040);
    l1->SetNColumns(2);
    l1->AddEntry(gT,   "TiO_{2} paint", "lp");
    l1->AddEntry(gA,   "aluminium foil", "lp");
    l1->AddEntry(gG,   "glossy paper", "lp");
    l1->AddEntry(bare, "bare (TIR guide)", "l");
    l1->Draw();

    // ---- Panel 2: R at the 425 nm scintillation peak --------------------
    c->cd(2);
    gPad->SetLeftMargin(0.18); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.20); gPad->SetTopMargin(0.28);
    drawRbars("hR425", 425., "at 425 nm (scintillation)");

    // ---- Panel 3: R at the 476 nm WLS emission peak --------------------
    c->cd(3);
    gPad->SetLeftMargin(0.18); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.20); gPad->SetTopMargin(0.28);
    drawRbars("hR476", 476., "at 476 nm (Y-11 emission)");

    c->SaveAs(TString::Format("%s/reflector_reflectivity.pdf", outdir));
    c->SaveAs(TString::Format("%s/reflector_reflectivity.png", outdir));
    printf("reflector_reflectivity: wrote %s/reflector_reflectivity.{pdf,png}\n", outdir);
}
