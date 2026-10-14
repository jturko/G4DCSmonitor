// pe_spectra_grid.C -- photoelectron-yield spectra in the layout of Fig. 10 of
// Rao et al. (J. Appl. Phys. 138, 114504), adapted to this simulation.
//
// Rao Fig. 10 is a grid of pulse-height (voltage) spectra: the three MCD
// measurement regions (centre, edge, corner) are the rows and the trigger
// detector configurations are the columns, with the four reflective materials
// overlaid in every cell.  Here the voltage pulse height is replaced by the
// number of photoelectrons per event (the simulation analogue), and the fourth
// configuration (single SiPM + WLS fibre clamp) is added as a fourth column.
//
// The result is a 3 (region) x 4 (config) grid; each cell overlays the four
// reflector treatments.  Spectra are probability densities (unit area) on
// logarithmic x/y axes, so the weakly collecting bare plate and the bright
// reflective wrappers are both resolved.
//
// Reads <dir>/muon_out_c<C>_r<R>_{centre,edge,corner}.root (tree "sipmHits").
//
// Usage:
//   root -l -b -q 'macros/pe_spectra_grid.C("slurm/data_v2.2/matrix","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

#include "style.h"

namespace {
const char* kReflName[4] = {"none (bare)", "TiO2", "Al", "glossy"};
const int   kStyle[4]    = {kSolid, kDashed, kDotted, kDashDotted};
const char* kLocName[3]  = {"centre", "edge", "corner"};

// Logarithmic bin edges: one [0,0.5) bin to absorb zero-p.e. events, then
// ~10%-wide log bins up to a generous maximum.  On the log-x display the
// zero bin is off-scale, i.e. "no pulse"; the spectra are normalised to unit
// area over the populated bins.
std::vector<double> logEdges()
{
    std::vector<double> e;
    e.push_back(0.0);
    e.push_back(0.5);
    const int    n  = 70;
    const double lo = 0.5, hi = 6000.0;
    for (int i = 1; i <= n; ++i)
        e.push_back(lo * std::pow(hi / lo, (double)i / n));
    return e;
}
}  // namespace

void pe_spectra_grid(const char* dir = "slurm/data_v2.2/matrix",
                     const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gSystem->mkdir(outdir, kTRUE);

    const int kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};

    const std::vector<double> edges = logEdges();
    const int nb = (int)edges.size() - 1;

    // h[cfg][loc][refl]  (cfg 1..4, loc 0..2, refl 0..3)
    std::map<int, std::map<int, std::map<int, TH1D*>>> h;
    for (int c = 1; c <= 4; ++c)
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r) {
                TString nm = TString::Format("hpe_c%d_%s_r%d", c, kLocName[l], r);
                h[c][l][r] = new TH1D(nm, "", nb, edges.data());
            }

    // ---- discover + reduce files ---------------------------------------
    std::vector<TString> files;
    void* d = gSystem->OpenDirectory(dir);
    if (!d) { std::cerr << "No such directory: " << dir << "\n"; return; }
    const char* e;
    while ((e = gSystem->GetDirEntry(d))) {
        TString n(e);
        if (n.BeginsWith("muon_out_") && n.EndsWith(".root"))
            files.emplace_back(TString(dir) + "/" + n);
    }
    gSystem->FreeDirectory(d);
    if (files.empty()) { std::cerr << "No muon_out_*.root in " << dir << "\n"; return; }

    for (const auto& full : files) {
        TString stem = gSystem->BaseName(full);
        stem.ReplaceAll("muon_out_", "");
        stem.ReplaceAll(".root", "");
        // stem = c<C>_r<R>_<loc>
        Ssiz_t p0 = stem.Index("c"), p1 = stem.Index("_r"), p2 = stem.Last('_');
        if (p0 == kNPOS || p1 == kNPOS || p2 == kNPOS) continue;
        int cfg  = TString(stem(p0 + 1, p1 - p0 - 1)).Atoi();
        int refl = TString(stem(p1 + 2, p2 - p1 - 2)).Atoi();
        TString loc = stem(p2 + 1, stem.Length());
        if (cfg < 1 || cfg > 4 || refl < 0 || refl > 3) continue;
        int li = -1;
        for (int l = 0; l < 3; ++l) if (loc == kLocName[l]) li = l;
        if (li < 0) continue;

        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { delete f; continue; }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }

        int evt = 0, det = 0, nDet = 0;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("nDetected", &nDet);

        // sum nDetected over the SiPMs -> photoelectrons per event
        std::map<int, double> ev;
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0) continue;
            ev[evt] += nDet;
        }
        for (auto& kv : ev) h[cfg][li][refl]->Fill(kv.second);
        delete f;
    }

    // ---- normalise all spectra, then fix SHARED x/y ranges -------------
    // Every cell uses the same axes so configs and regions can be compared
    // directly.  Only populated bins count; the zero-p.e. bin sits off the
    // logarithmic abscissa.
    double gxmin = 1e30, gxmax = -1e30, gymin = 1e30, gymax = 0.;
    for (int l = 0; l < 3; ++l)
        for (int cfg = 1; cfg <= 4; ++cfg)
            for (int r = 0; r < 4; ++r) {
                TH1D* hh = h[cfg][l][r];
                double I = hh->Integral();          // includes the zero bin
                if (I > 0) hh->Scale(1. / I);
                for (int b = 1; b <= hh->GetNbinsX(); ++b) {
                    double lo = hh->GetXaxis()->GetBinLowEdge(b);
                    double up = hh->GetXaxis()->GetBinUpEdge(b);
                    double v  = hh->GetBinContent(b);
                    if (v > 0 && lo >= 0.5) {
                        gxmin = std::min(gxmin, lo);
                        gxmax = std::max(gxmax, up);
                        gymin = std::min(gymin, v);
                        gymax = std::max(gymax, v);
                    }
                }
            }
    if (gxmin > gxmax) { gxmin = 0.5; gxmax = 200.; }
    if (gymin > gymax) { gymin = 1e-5; }
    const double xlo = 0.8 * gxmin, xhi = 1.3 * gxmax;
    const double ylo = 0.4 * gymin, yhi = 1.6 * gymax;

    // ---- draw ----------------------------------------------------------
    TCanvas* c = new TCanvas("c_pe_grid", "pe spectra grid", 1560, 1180);
    c->Divide(4, 3, 0.003, 0.003);

    for (int l = 0; l < 3; ++l) {
        for (int cc = 0; cc < 4; ++cc) {
            int cfg = cc + 1;
            c->cd(l * 4 + cc + 1);
            gPad->SetLogx();
            gPad->SetLogy();
            gPad->SetLeftMargin (cc == 0 ? 0.22 : 0.02);
            gPad->SetRightMargin(cc == 3 ? 0.04 : 0.025);
            gPad->SetBottomMargin(l == 2 ? 0.16 : 0.03);
            gPad->SetTopMargin(l == 0 ? 0.14 : 0.05);

            for (int r = 0; r < 4; ++r) {
                TH1D* hh = h[cfg][l][r];
                hh->SetLineColor(kColor[r]);
                hh->SetLineStyle(kStyle[r]);
                hh->SetLineWidth(2);
                hh->GetXaxis()->SetRangeUser(xlo, xhi);
                hh->GetYaxis()->SetRangeUser(ylo, yhi);
                hh->GetYaxis()->SetMoreLogLabels();
                if (cc == 0) {
                    hh->GetYaxis()->SetTitle("normalised entries");
                    hh->GetYaxis()->SetTitleSize(0.055);
                    hh->GetYaxis()->SetTitleOffset(1.15);
                }
                if (l == 2) {
                    hh->GetXaxis()->SetTitle("photoelectrons / event");
                    hh->GetXaxis()->SetTitleSize(0.055);
                    hh->GetXaxis()->SetTitleOffset(1.15);
                }
                if (r == 0) hh->Draw("HIST");
                else        hh->Draw("HIST SAME");
            }

            // column header (config) on the top row
            if (l == 0) {
                TLatex* tc = new TLatex();
                tc->SetNDC();
                tc->SetTextSize(0.075);
                tc->SetTextFont(62);
                tc->SetTextAlign(21);
                tc->DrawLatex(0.5, 0.955, TString::Format("Config %d", cfg));
            }
            // row label (region): rotated, off the axis to the left and centred
            // on the row so it never overlaps the frame.
            if (cc == 0) {
                TLatex* tx = new TLatex();
                tx->SetNDC();
                tx->SetTextSize(0.075);
                tx->SetTextFont(62);
                tx->SetTextAlign(22);
                tx->SetTextAngle(90);
                tx->DrawLatex(0.05, 0.5, kLocName[l]);
            }
            // shared legend in the top-right cell, top-right corner
            if (l == 0 && cc == 3) {
                TLegend* leg = new TLegend(0.60, 0.593, 0.995, 0.803);
                leg->SetBorderSize(0);
                leg->SetFillStyle(0);
                for (int r = 0; r < 4; ++r)
                    leg->AddEntry(h[cfg][l][r], kReflName[r], "l");
                leg->Draw();
            }
        }
    }

    c->SaveAs(TString::Format("%s/fig_pe_grid.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_pe_grid.png", outdir));
    printf("pe_spectra_grid: wrote %s/fig_pe_grid.{pdf,png}\n", outdir);
}
