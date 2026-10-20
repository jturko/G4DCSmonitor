// de_threshold.C -- detection efficiency versus threshold, in the sense of
// Eq. (1) of Rao et al. (J. Appl. Phys. 138, 114504 (2025)).
//
// The experiment reports DE(u_thresh) = Ndet(u_thresh) / Nref: the fraction of
// reference (coincidence) muons that produce a trigger signal above the
// discriminator threshold.  The simulation contains no electronics, so the
// absolute threshold in millivolts cannot be reproduced, but the *shape* of
// the curve and its behaviour from one surface treatment to another can: the
// threshold enters only as a requirement that the total number of
// photoelectrons detected in the event exceed some number N_thr.
//
//   DE(N_thr) = P( sum_s N_s  >=  N_thr )
//
// The interesting statement is the *relative* one: because the bare plate
// collects about seven times fewer photoelectrons than glossy paper, the
// threshold at which glossy paper detects essentially every muon corresponds
// to a much larger fraction of the bare events falling below threshold.  This
// macro quantifies that.
//
// Outputs
//   fig_de_threshold.pdf   DE versus N_thr, one panel per layout, one curve
//                          per reflector
//   de_threshold.csv       config, reflector, n_thr, de
//   de_threshold_at_glossy.csv
//                          the threshold at which the glossy treatment of the
//                          same layout reaches DE = 0.99, and the DE of every
//                          other treatment at that threshold
//
// Usage:
//   root -l -b -q 'macros/de_threshold.C("slurm/data_v6/whole","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TLegend.h>
#include <TLine.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

#include "style.h"

namespace {

const char* kReflName[4] = {"bare", "TiO2", "Al", "glossy"};
const int    kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};
const int    kStyle[4] = {1, 2, 3, 4};
const int    kNThr = 700;      // thresholds 0 .. 700 photoelectrons

// Per-event total photoelectron count of one whole-slab file.
std::vector<int> eventTotals(const TString& fname)
{
    std::vector<int> tot;
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) { printf("  MISSING %s\n", fname.Data()); return tot; }
    TTree* t = (TTree*)f->Get("sipmHits");
    if (!t) { delete f; return tot; }
    int evt = 0, nd = 0;
    t->SetBranchAddress("evtNb", &evt);
    t->SetBranchAddress("nDetected", &nd);
    std::map<int, int> sum;
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        sum[evt] += nd;
    }
    tot.reserve(sum.size());
    for (const auto& kv : sum) tot.push_back(kv.second);
    delete f;
    return tot;
}

}  // namespace

void de_threshold(const char* dir = "slurm/data_v6/whole",
                  const char* outdir = "report/figures")
{
    gSystem->mkdir(outdir, kTRUE);
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);

    // de[cfg][refl][thr]
    std::vector<std::vector<std::vector<double>>> de(
        5, std::vector<std::vector<double>>(4, std::vector<double>(kNThr + 1, 0.)));
    std::vector<std::vector<int>> ntot(5, std::vector<int>(4, 0));

    for (int cfg = 1; cfg <= 4; ++cfg) {
        for (int r = 0; r < 4; ++r) {
            const std::vector<int> tot = eventTotals(
                TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, cfg, r));
            ntot[cfg][r] = (int)tot.size();
            if (tot.empty()) continue;
            // histogram of the per-event totals; everything at or above
            // kNThr goes into the last bin, so that DE(thr) = P(N >= thr)
            // is correct over the whole range
            std::vector<int> hist(kNThr + 1, 0);
            for (int v : tot) hist[std::min(v, kNThr)]++;
            int above = 0;
            for (int thr = kNThr; thr >= 0; --thr) {
                above += hist[thr];
                de[cfg][r][thr] = (double)above / (double)tot.size();
            }
            printf("cfg%d %-6s events=%5d  mean=%.1f pe  DE(N=0)=%.4f\n", cfg,
                   kReflName[r], (int)tot.size(),
                   [&] { long s = 0; for (int v : tot) s += v; return (double)s / tot.size(); }(),
                   de[cfg][r][0]);
        }
    }

    // ---- CSV: the full curves ------------------------------------------
    FILE* fp = fopen(TString::Format("%s/de_threshold.csv", outdir), "w");
    if (fp) {
        fprintf(fp, "config,reflector,n_thr,de\n");
        for (int cfg = 1; cfg <= 4; ++cfg)
            for (int r = 0; r < 4; ++r)
                for (int thr = 0; thr <= kNThr; ++thr)
                    fprintf(fp, "%d,%s,%d,%.6f\n", cfg, kReflName[r], thr, de[cfg][r][thr]);
        fclose(fp);
    }

    // ---- The threshold-free form of the statement ------------------------
    // Rao et al. report that at their working threshold (400 mV) every
    // configuration EXCEPT the bare one reaches a detection efficiency of
    // 80%, and that the bare one is the only one that falls short.  The
    // absolute threshold in millivolts cannot be reproduced here, but the
    // *statement* is threshold-free: it says that there exists a threshold at
    // which the three reflective treatments are above 80% and the bare one is
    // below.  We scan for that window.
    FILE* fg = fopen(TString::Format("%s/de_threshold_window.csv", outdir), "w");
    if (fg) {
        fprintf(fg, "config,thr_lo_pe,thr_hi_pe,de_bare_lo,de_bare_hi,"
                    "de_tio2_min,de_al_min,de_glossy_min\n");
        for (int cfg = 1; cfg <= 4; ++cfg) {
            int lo = -1, hi = -1;
            for (int thr = 0; thr <= kNThr; ++thr) {
                const bool refl = de[cfg][1][thr] >= 0.8 && de[cfg][2][thr] >= 0.8
                                  && de[cfg][3][thr] >= 0.8;
                const bool bare = de[cfg][0][thr] < 0.8;
                if (refl && bare) { if (lo < 0) lo = thr; hi = thr; }
            }
            if (lo < 0) { lo = hi = -1; }
            // the extremes of DE(bare) inside the window
            double dblo = 1., dbhi = 0., dtio = 1., dal = 1., dgl = 1.;
            if (lo >= 0) {
                dblo = 1.; dbhi = 0.; dtio = dal = dgl = 1.;
                for (int thr = lo; thr <= hi; ++thr) {
                    dblo = std::min(dblo, de[cfg][0][thr]);
                    dbhi = std::max(dbhi, de[cfg][0][thr]);
                    dtio = std::min(dtio, de[cfg][1][thr]);
                    dal = std::min(dal, de[cfg][2][thr]);
                    dgl = std::min(dgl, de[cfg][3][thr]);
                }
            }
            fprintf(fg, "%d,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n", cfg, lo, hi,
                    dblo, dbhi, dtio, dal, dgl);
            printf("cfg%d: window with the three reflective treatments above 80%% "
                   "and the bare one below: N_thr = %d..%d pe", cfg, lo, hi);
            if (lo >= 0)
                printf("  (DE(bare) = %.3f..%.3f there)", dblo, dbhi);
            printf("\n");
        }
        fclose(fg);
    }

    // ---- figure ----------------------------------------------------------
    TCanvas* c = new TCanvas("c_de", "", 1150, 950);
    c->Divide(2, 2, 0.005, 0.005);
    for (int cfg = 1; cfg <= 4; ++cfg) {
        c->cd(cfg);
        gStyle->SetOptTitle(1);
        gPad->SetLeftMargin((cfg % 2) ? 0.075 : 0.145);
        gPad->SetRightMargin(0.025);
        gPad->SetBottomMargin(0.13);
        gPad->SetTopMargin(0.13);

        TH1D* frame = new TH1D(TString::Format("fr_de_%d", cfg), "", kNThr, 0., kNThr);
        frame->SetTitle(TString::Format("Config %d", cfg));
        frame->GetXaxis()->SetTitle("threshold  N_{thr} [photoelectrons per event]");
        frame->GetYaxis()->SetTitle("detection efficiency  DE");
        frame->GetXaxis()->SetTitleSize(0.040);
        frame->GetYaxis()->SetTitleSize(0.040);
        frame->GetXaxis()->SetLabelSize(0.038);
        frame->GetYaxis()->SetLabelSize(0.038);
        frame->GetXaxis()->SetTitleOffset(1.10);
        frame->GetYaxis()->SetTitleOffset(1.15);
        frame->GetYaxis()->SetRangeUser(0., 1.0);
        frame->Draw("HIST");

        TLine* e80 = new TLine(0., 0.8, kNThr, 0.8);
        e80->SetLineColor(kGray + 2);
        e80->SetLineStyle(2);
        e80->SetLineWidth(1);
        e80->Draw("SAME");

        TLegend* leg = new TLegend(0.16, 0.79, 0.98, 0.97);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetFillColorAlpha(0., 0.);
        leg->SetTextSize(0.036);
        leg->SetNColumns(4);
        for (int r = 0; r < 4; ++r) {
            std::vector<double> xs, ys;
            for (int thr = 0; thr <= kNThr; ++thr) { xs.push_back(thr); ys.push_back(de[cfg][r][thr]); }
            TGraph* g = new TGraph((int)xs.size(), xs.data(), ys.data());
            g->SetMarkerStyle(0);
            g->SetLineColor(kColor[r]);
            g->SetLineWidth(3);
            g->SetLineStyle(kStyle[r]);
            g->SetMarkerColor(kColor[r]);
            g->SetMarkerStyle(kStyle[r] + 20);
            g->SetMarkerSize(1.0);
            g->Draw("L SAME");
            if (cfg == 1) leg->AddEntry(g, kReflName[r], "l");
        }
        if (cfg == 1) leg->Draw();
    }
    gStyle->SetOptTitle(0);
    c->SaveAs(TString::Format("%s/fig_de_threshold.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_de_threshold.png", outdir));
    printf("wrote %s/fig_de_threshold.{pdf,png} and de_threshold*.csv\n", outdir);
}
