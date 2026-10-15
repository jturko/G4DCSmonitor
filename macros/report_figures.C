// report_figures.C -- generate the figures used by the LaTeX matrix-scan report.
//
// Reads the matrix outputs (muon_out_c<C>_r<R>_{centre,edge,corner}.root) and
// writes vector PDFs (+ PNG previews) into `outdir`:
//
//   fig_yield.pdf         per-event optical-photon yield (produced) per reflector
//   fig_detected.pdf      per-event photoelectron count per reflector
//   fig_wavelength.pdf    detected-photon wavelength spectrum per reflector
//   fig_wavelength_cfg.pdf  detected-photon wavelength per reflector, cfg 1-4
//
// Position- and direction-resolved figures for the three-patch matrix scan
// live in macros/matrix_positions.C.
//
// Usage:
//   root -l -b -q 'report_figures.C("slurm/data_output","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TMultiGraph.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions) and the PetroffPalette
// P6 categorical colours.  Located via ROOT_INCLUDE_PATH (set by
// macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {
const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};
const int   kStyle[4]    = {kSolid, kDashed, kDotted, kDashDotted};
}  // namespace

void report_figures(const char* dir = "slurm/data_output",
                    const char* outdir = "report/figures")
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);   // no horizontal caps on error bars
    gSystem->mkdir(outdir, kTRUE);

    // PetroffPalette P6 (getP6(0..3) = blue, orange, red, purple), one colour
    // per reflector (none, TiO2, Al, glossy).
    const int kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};

    // ---- discover files -------------------------------------------------
    std::vector<TString> files;
    void* d = gSystem->OpenDirectory(dir);
    if (!d) { std::cerr << "No such directory: " << dir << "\n"; return; }
    const char* e;
    while ((e = gSystem->GetDirEntry(d))) {
        TString n(e);
        if (n.BeginsWith("muon_out_") && n.EndsWith(".root")) {
            files.emplace_back(TString(dir) + "/" + n);
        }
    }
    gSystem->FreeDirectory(d);
    if (files.empty()) { std::cerr << "No muon_out_*.root in " << dir << "\n"; return; }

    // ---- accumulators ---------------------------------------------------
    // per-(cfg=1,refl) per-event distributions
    std::map<int, TH1D*> hProduced, hDetected;
    // detected-photon wavelength: indexed by cfg*4 + refl, so both the
    // reflector comparison (fig_wavelength) and the config comparison
    // (fig_wavelength_cfg) can be drawn from the same filled histograms.
    std::map<int, TH1D*> hWave;
    auto waveKey = [](int cfg, int refl) { return cfg * 4 + refl; };

    for (int r = 0; r < 4; ++r) {
        // Yield spans up to ~7e4 photons/event; 50 photons/bin keeps the
        // narrow ~19000-photon peak smooth without a huge bin count.  The
        // photoelectron count is binned at 1 p.e./bin (up to a few thousand),
        // so the weakly-collecting bare case is resolved near zero.
        hProduced[r] = new TH1D(TString::Format("hp_r%d", r), "", 1400, 0, 70000);
        hDetected[r] = new TH1D(TString::Format("hd_r%d", r), "", 2400, 0, 2400);
        for (int cfg = 1; cfg <= 4; ++cfg)
            hWave[waveKey(cfg, r)] = new TH1D(TString::Format("hw_c%dr%d", cfg, r),
                                              "", 90, 360, 630);
    }

    for (const auto& full : files) {
        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { delete f; continue; }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }

        // parse cfg/refl from the stem
        TString stem = gSystem->BaseName(full);
        stem.ReplaceAll("muon_out_", "");
        int cfg = 0, refl = -1;
        Ssiz_t p0 = stem.Index("c"), p1 = stem.Index("_r");
        if (p0 == kNPOS || p1 == kNPOS) { delete f; continue; }
        cfg  = TString(stem(p0 + 1, p1 - p0 - 1)).Atoi();
        refl = TString(stem(p1 + 2, stem.Length())).Atoi();
        if (refl < 0 || refl > 3) { delete f; continue; }

        int evt = 0, det = 0, sipm = 0, nDet = 0, nProd = 0;
        double mx = 0., my = 0., wl = 0.;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("sipm", &sipm);
        t->SetBranchAddress("nDetected", &nDet);
        t->SetBranchAddress("muonX", &mx);
        t->SetBranchAddress("muonY", &my);
        t->SetBranchAddress("nProduced", &nProd);
        t->SetBranchAddress("meanWavelength_nm", &wl);

        // aggregate rows -> per event
        std::map<int, std::vector<double>> ev;  // evt -> {x,y,produced,collected}
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0) continue;
            auto& v = ev[evt];
            if (v.empty()) v = {mx, my, (double)nProd, 0.};
            v[3] += nDet;
            if (nDet > 0) hWave[waveKey(cfg, refl)]->Fill(wl, nDet);  // weight by p.e.
        }
        for (auto& kv : ev) {
            double pr = kv.second[2], co = kv.second[3];
            if (cfg == 1) {
                hProduced[refl]->Fill(pr);
                hDetected[refl]->Fill(co);
            }
        }
        delete f;
    }

    const char* exts[2] = {"pdf", "png"};

    // The plate-averaged efficiency figure (fig_eff_bars) is written by
    // macros/whole_slab_uniformity.C from the whole-slab run; the three-patch
    // matrix data pooled here would bias it by the choice of patches.

    // ================= Fig 2: produced yield (cfg1) ======================
    auto stackDraw = [&](std::map<int, TH1D*>& hs, const char* ytitle,
                         const char* xtitle, const char* name, bool density,
                         bool trimX, bool logX = false, bool logY = false,
                         double xTitleSize = 0.050,
                         double xLabelSize = 0.048, double xTitleOffset = 1.25,
                         double xLabelOffset = 0.008) {
        TCanvas* c = new TCanvas("c_tmp", "", 760, 560);
        c->SetLeftMargin(0.14);
        c->SetBottomMargin(0.13);
        if (logX) c->SetLogx();
        if (logY) c->SetLogy();
        // Trim the displayed x-range to the populated region (get rid of the
        // empty tail left by the generous fixed binning) and make sure the
        // largest bins are not pushed into the overflow bin.
        if (trimX) {
            double lo = 1e30, hi = -1e30;
            for (int r = 0; r < 4; ++r) {
                TH1D* h = hs[r];
                int b0 = h->FindFirstBinAbove(0);
                int b1 = h->FindLastBinAbove(0);
                if (b0 < 0) continue;
                lo = std::min(lo, h->GetXaxis()->GetBinLowEdge(b0));
                hi = std::max(hi, h->GetXaxis()->GetBinUpEdge(b1));
            }
            if (hi > lo) {
                double xlo, xhi;
                if (logX) {
                    // A log axis labels decades, so pad modestly (rather than
                    // snapping out to whole decades, which over-widens narrow
                    // distributions) and let SetMoreLogLabels show intermediate
                    // labels.  The quantities here are integer counts, so clamp
                    // the low end to 1: a range starting below 1 makes ROOT
                    // extend the axis through the sub-unity decades (labels
                    // like 10^-3) even though no count can fall there.
                    if (lo <= 0.) lo = hs[0]->GetBinWidth(1);
                    xlo = 0.8 * lo; if (xlo < 1.) xlo = 1.;
                    xhi = 1.25 * hi;
                } else {
                    const double margin = 0.05 * (hi - lo);
                    xlo = lo - margin; xhi = hi + margin;
                }
                for (int r = 0; r < 4; ++r)
                    hs[r]->GetXaxis()->SetRangeUser(xlo, xhi);
            }
        }
        double ymax = 0.;        for (auto& kv : hs) {
            TH1D* h = kv.second;
            // If asked, normalise to unit area (a probability density) so the
            // curves share a common, physically meaningful y scale.  All four
            // histograms hold the same number of events, so this is a uniform
            // rescale that does not alter their relative heights; the log-y
            // axis is what makes the broad reflective distributions visible
            // next to the concentrated bare peak.
            if (density) {
                double I = h->Integral();
                if (I > 0) h->Scale(1. / I / h->GetBinWidth(1));
            }
            // draw-time y maximum of the trimmed range
            int b0 = h->GetXaxis()->GetFirst();
            int b1 = h->GetXaxis()->GetLast();
            for (int b = b0; b <= b1; ++b) ymax = std::max(ymax, h->GetBinContent(b));
        }
        if (ymax > 0) ymax *= 1.35;            // headroom for the legend

        // Legend tucked into the lower-right corner, clear of the top of the
        // pad axis box (the pad frame top sits at NDC y ~0.90) and of the
        // distribution tails that rise there.  legBottom lifts the bottom edge
        // so the entries do not intrude into the curves (used for the detected
        // spectra, whose broad tails reach up into the legend region).
        TLegend* leg = new TLegend(0.60, 0.56, 0.89, 0.81);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);          // transparent (draw no fill)
        leg->SetFillColorAlpha(0, 0.);
        
        for (int r = 0; r < 4; ++r) {
            TH1D* h = hs[r];
            h->SetLineColor(kColor[r]); h->SetLineStyle(kStyle[r]); h->SetLineWidth(2);
            if (logX) h->GetXaxis()->SetMoreLogLabels();  // label intermediate decades
            h->GetYaxis()->SetTitleSize(xTitleSize);
            h->GetXaxis()->SetTitleSize(xTitleSize);
            h->GetYaxis()->SetLabelSize(xLabelSize);
            h->GetXaxis()->SetLabelSize(xLabelSize);
            h->GetYaxis()->SetTitleOffset(1.25);
            h->GetXaxis()->SetTitleOffset(xTitleOffset);
            h->GetXaxis()->SetLabelOffset(xLabelOffset);
            if (logY) {
                // Positively-populated bins span >2 decades; a log axis needs a
                // strictly positive floor, so open a little below the smallest
                // populated bin rather than at 0.
                double ymin = 0.;
                int fb = h->GetXaxis()->GetFirst();
                int lb = h->GetXaxis()->GetLast();
                for (int b = fb; b <= lb; ++b) {
                    double v = h->GetBinContent(b);
                    if (v > 0. && (ymin == 0. || v < ymin)) ymin = v;
                }
                if (ymin <= 0.) ymin = ymax * 1e-3;
                h->GetYaxis()->SetRangeUser(0.5 * ymin, ymax);
            } else if (ymax > 0) {
                h->GetYaxis()->SetRangeUser(0, ymax);
            }
            if (r == 0) { h->GetYaxis()->SetTitle(ytitle); h->GetXaxis()->SetTitle(xtitle); h->Draw("HIST"); }
            else        { h->Draw("HIST SAME"); }
            leg->AddEntry(h, kReflName[r], "l");
        }
        leg->Draw();
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/%s.%s", outdir, name, ex));
    };
    stackDraw(hProduced, "probability density [1/(event #gamma)]",
              "produced photons / event", "fig_yield", true, true, true, true,
              0.050, 0.048, 1.35, 0.006);
    stackDraw(hDetected, "probability density [1/(event p.e.)]",
              "photoelectrons / event", "fig_detected", true, true, true, true,
              0.050, 0.048, 1.35, 0.006);
    {
        std::map<int, TH1D*> hWave1;
        for (int r = 0; r < 4; ++r) hWave1[r] = hWave[waveKey(1, r)];
        stackDraw(hWave1, "photoelectrons / nm",
                  "detected photon wavelength [nm]", "fig_wavelength", true, false,
                  false, false, 0.045, 0.044, 1.25, 0.008);
    }

    // ============ Fig 3b: detected wavelength, configs 1-4 ================
    // 2x2 grid, one panel per reflector; each panel overlays configs 1-4 so
    // the WLS green-shift of config 4 is visible against the direct-coupled
    // layouts (which detect the blue scintillation spectrum).
    {
        const int cfgColor[5] = {0, PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                                 PetroffPalette::getP6(2), PetroffPalette::getP6(3)};
        const int cfgStyle[5] = {0, kSolid, kDashed, kDotted, kDashDotted};
        TCanvas* c = new TCanvas("c_wave_cfg", "wavelength by config", 1000, 900);
        c->Divide(2, 2, 0.004, 0.004);
        for (int r = 0; r < 4; ++r) {
            c->cd(r + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.15);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.15);
            gPad->SetTopMargin(0.12);
            double ymax = 0.;
            for (int cfg = 1; cfg <= 4; ++cfg) {
                TH1D* h = hWave[waveKey(cfg, r)];
                double I = h->Integral();
                if (I > 0) h->Scale(1. / I / h->GetBinWidth(1));
                for (int b = 1; b <= h->GetNbinsX(); ++b)
                    ymax = std::max(ymax, h->GetBinContent(b));
            }
            if (ymax > 0) ymax *= 1.30;
            // Only the bottom-right panel (r == 3) carries the legend; it is
            // pushed down and right so it clears the top of the axis box.
            TLegend* leg = (r == 3)
                ? new TLegend(0.55, 0.50, 0.89, 0.81) : nullptr;
            if (leg) {
                leg->SetBorderSize(0);
                leg->SetFillStyle(0);
                leg->SetFillColorAlpha(0, 0.);
            }
            
            for (int cfg = 1; cfg <= 4; ++cfg) {
                TH1D* h = hWave[waveKey(cfg, r)];
                h->SetLineColor(cfgColor[cfg]);
                h->SetLineStyle(cfgStyle[cfg]);
                h->SetLineWidth(2);
                h->SetTitle(kReflName[r]);
                h->GetXaxis()->SetTitle("detected photon wavelength [nm]");
                h->GetYaxis()->SetTitle("photoelectrons / nm");
                h->GetXaxis()->SetTitleSize(0.048);
                h->GetYaxis()->SetTitleSize(0.048);
                h->GetXaxis()->SetLabelSize(0.042);
                h->GetYaxis()->SetLabelSize(0.042);
                h->GetYaxis()->SetTitleOffset(1.25);
                h->GetYaxis()->SetRangeUser(0, ymax);
                if (cfg == 1) h->Draw("HIST");
                else          h->Draw("HIST SAME");
                if (leg) leg->AddEntry(h, TString::Format("config %d", cfg), "l");
            }
            if (leg) leg->Draw();
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_wavelength_cfg.%s", outdir, ex));
    }

    // Position-resolved and direction-aware analysis of the three-patch
    // matrix scan is produced separately by macros/matrix_positions.C
    // (fig_pos_layouts, fig_pos_refl, fig_uniformity, fig_sipm_sharing).

    printf("report_figures: wrote figures to %s\n", outdir);
}
