// opt_de.C -- detection efficiency versus threshold for the v7_opt detector
// designs (Phase 2, "detector optimisation").
//
// This is the design-orientated analogue of de_threshold.C: instead of a
// (config x reflector) grid, each entry of macros/opt_designs.h is one
// independent detector design with its own whole-plate file.  The observable
// is the same threshold-free statement of Rao et al. (J. Appl. Phys. 138,
// 114504 (2025)):
//
//     DE(N_thr) = P( sum_s N_s  >=  N_thr )
//
// i.e. the fraction of reference muons whose total number of detected
// photoelectrons clears the threshold.  The simulation has no electronics, so
// N_thr is in photoelectrons, not millivolts.
//
// Panel (a): DE(N_thr) for every design (log x, y 0..1) with guides at 80 %
//            and 99 %.
// Panel (b): the threshold required to reach DE = 0.90, one bar per design,
//            with the value printed and a dashed line at the control
//            (c1_same_1p55).
//
// Thresholds are interpolated linearly between the integer-threshold curve
// points, so they need not be integers.  A design that never reaches a level
// gets -1.
//
// Outputs (in outdir, default report/figures):
//   fig_opt_de.pdf/.png
//   opt_de.csv          tag,n_thr,de            (the full curves)
//   opt_de_summary.csv  tag,de_max,n_thr_at_de80,n_thr_at_de90,n_thr_at_de99,
//                       de_at_control_threshold
//
// `de_at_control_threshold` is each design's DE at the N_thr where the
// control (c1_same_1p55) first reaches DE = 0.99: the campaign-wide comparable
// "working threshold".
//
// A design whose file is missing or unreadable is skipped ENTIRELY (warning on
// stderr): it gets no curve, no bar and no CSV row, so the previous campaign's
// figures remain valid until a new design's data lands.
//
// Usage:
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     root -l -b -q 'macros/opt_de.C("DIR","report/figures")'

#include <TCanvas.h>
#include <TBox.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "opt_designs.h"

namespace {

const double kEps    = 1e-12;

// Continuous, distinguishable line styles.  Second member of each
// same-treatment pair gets a dashed or dash-dot line so that two designs
// drawn in one colour family never hide each other.  The colour stays
// opt::color(i); only the dash pattern changes.  No dotted style is used:
// on the steep part of a curve a dotted/dashed line at small width breaks
// up into isolated dots.
int lineStyle(int i)
{
    if (i == 1 || i == 5 || i == 8) return 2;   // dashed
    if (i == 3 || i == 6)           return 4;   // dash-dot
    return 1;                                   // solid
}

// Linear interpolation of the DE curve at a real threshold t.  The curve is
// sampled on integer thresholds; below/above the sampled range we return the
// end values.
double deAt(const std::vector<double>& de, double t)
{
    if (de.empty()) return 0.;
    if (t <= 0.) return de[0];
    const double xmax = (double)(de.size() - 1);
    if (t >= xmax) return de.back();
    const int    i    = (int)std::floor(t);
    const double frac = t - (double)i;
    return de[i] + frac * (de[i + 1] - de[i]);
}

// First threshold at which the (monotonically falling) DE curve reaches
// `level`, interpolated between the two bracketing integer thresholds.
// Returns -1 if the curve never falls to that level.
double firstCross(const std::vector<double>& de, double level)
{
    if (de.empty()) return -1.;
    if (de[0] < level) return 0.;
    for (size_t t = 1; t < de.size(); ++t) {
        if (de[t] <= level && de[t - 1] > level) {
            const double y0 = de[t - 1], y1 = de[t];
            if (std::fabs(y0 - y1) < kEps) return (double)(t - 1);
            return (double)(t - 1) + (y0 - level) / (y0 - y1);
        }
    }
    return -1.;
}

struct Result {
    TString              tag;
    std::vector<double>  de;       // de[N_thr], index = integer N_thr
    long long            nEvents = 0;
    double               meanPe  = 0.;
};

}  // namespace

void opt_de(const char* dir = "slurm/data_v7_opt/whole",
            const char* outdir = "report/figures")
{
    style(0.045);
    gSystem->mkdir(outdir, kTRUE);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);

    const std::vector<opt::Design> designs = opt::designs();
    const int ndes = (int)designs.size();

    std::vector<Result> res(ndes);
    std::vector<bool>   have(ndes, false);

    // ------------------------------------------------------------------
    // One single pass per file: per-event total photoelectrons.
    // ------------------------------------------------------------------
    for (int i = 0; i < ndes; ++i) {
        const opt::Design& d = designs[i];
        res[i].tag = d.tag;
        const TString fname = opt::file(dir, d);

        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie() || !f->IsOpen()) {
            fprintf(stderr, "  SKIP %-20s  (%s: missing or still being written)\n",
                    d.tag, fname.Data());
            if (f) { f->Close(); delete f; }
            continue;
        }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) {
            fprintf(stderr, "  SKIP %-20s  (no sipmHits tree)\n", d.tag);
            delete f;
            continue;
        }

        int evt = 0, nd = 0;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("nDetected", &nd);

        // Per-event totals keyed by evtNb.  The tree writes nch rows per event
        // (zero-detection rows included) only when the muon entered the slab;
        // a muon that misses it (a cut-away corner triangle of preset 12)
        // writes no rows and must not enter as a zero-p.e. event.  The event
        // count is therefore the number of DISTINCT evtNb values.
        std::map<long long, long long> tot;
        const Long64_t nent = t->GetEntries();
        for (Long64_t e = 0; e < nent; ++e) {
            t->GetEntry(e);
            if (evt < 0) continue;
            tot[evt] += nd;
        }
        delete f;

        const long long nEvents = (long long)tot.size();
        if (nEvents <= 0) {
            fprintf(stderr, "  SKIP %-20s  (no events)\n", d.tag);
            continue;
        }

        long long nTot = 0;
        for (const auto& kv : tot) nTot += kv.second;

        // Cap the curve at a sane maximum (an outlier tail must not allocate a
        // huge histogram); nothing below ~10^5 pe/event is physically relevant.
        long long maxPe = 0;
        for (const auto& kv : tot) maxPe = std::max(maxPe, kv.second);
        const int thrMax = (int)std::min<long long>(maxPe, 200000);

        std::vector<long long> hist((size_t)thrMax + 1, 0);
        for (const auto& kv : tot)
            hist[(size_t)std::min<long long>(kv.second, thrMax)]++;

        res[i].de.assign((size_t)thrMax + 1, 0.);
        long long above = 0;
        for (int thr = thrMax; thr >= 0; --thr) {
            above += hist[(size_t)thr];
            res[i].de[(size_t)thr] = (double)above / (double)nEvents;
        }
        res[i].nEvents = nEvents;
        res[i].meanPe  = (double)nTot / (double)nEvents;
        have[i] = true;

        const double de90 = firstCross(res[i].de, 0.90);
        printf("  %-20s events=%6lld  mean=%8.2f pe  N_thr(DE=0.90)=%.1f\n",
               d.tag, nEvents, res[i].meanPe, de90);
    }

    int nHave = 0;
    for (bool b : have) if (b) ++nHave;
    if (nHave == 0) {
        printf("opt_de: no readable files under %s -- nothing to do\n", dir);
        return;
    }

    // ------------------------------------------------------------------
    // The campaign-wide working threshold: control reaches DE = 0.99.
    // ------------------------------------------------------------------
    int iCtrl = -1;
    for (int i = 0; i < ndes; ++i)
        if (TString(designs[i].tag) == "c1_same_1p55") iCtrl = i;

    double ctrlThr = -1.;
    if (iCtrl >= 0 && have[iCtrl]) ctrlThr = firstCross(res[iCtrl].de, 0.99);
    if (ctrlThr < 0.) {
        // Fall back to the first available design so the column stays usable.
        for (int i = 0; i < ndes; ++i)
            if (have[i]) { ctrlThr = firstCross(res[i].de, 0.99); break; }
        printf("opt_de: warning: control c1_same_1p55 unavailable; using first "
               "available design as threshold reference (N_thr=%.1f)\n", ctrlThr);
    }
    printf("opt_de: campaign working threshold (control DE=0.99) = %.1f pe\n",
           ctrlThr);

    // ------------------------------------------------------------------
    // CSV: full curves
    // ------------------------------------------------------------------
    FILE* fc = fopen(TString::Format("%s/opt_de.csv", outdir), "w");
    if (fc) {
        fprintf(fc, "tag,n_thr,de\n");
        for (int i = 0; i < ndes; ++i) {
            if (!have[i]) continue;
            for (size_t thr = 0; thr < res[i].de.size(); ++thr)
                fprintf(fc, "%s,%zu,%.6f\n", designs[i].tag, thr,
                        res[i].de[thr]);
        }
        fclose(fc);
    } else {
        printf("opt_de: cannot write %s/opt_de.csv\n", outdir);
    }

    // ------------------------------------------------------------------
    // CSV: summary
    // ------------------------------------------------------------------
    FILE* fs = fopen(TString::Format("%s/opt_de_summary.csv", outdir), "w");
    if (fs) {
        fprintf(fs, "tag,de_max,n_thr_at_de80,n_thr_at_de90,n_thr_at_de99,"
                    "de_at_control_threshold\n");
        for (int i = 0; i < ndes; ++i) {
            if (!have[i]) continue;
            const std::vector<double>& de = res[i].de;
            const double deMax = *std::max_element(de.begin(), de.end());
            const double t80 = firstCross(de, 0.80);
            const double t90 = firstCross(de, 0.90);
            const double t99 = firstCross(de, 0.99);
            const double dc  = (ctrlThr >= 0.) ? deAt(de, ctrlThr) : -1.;
            fprintf(fs, "%s,%.6f,%.3f,%.3f,%.3f,%.6f\n", designs[i].tag,
                    deMax, t80, t90, t99, dc);
        }
        fclose(fs);
    } else {
        printf("opt_de: cannot write %s/opt_de_summary.csv\n", outdir);
    }

    // ------------------------------------------------------------------
    // Figure
    // ------------------------------------------------------------------
    double xHi = 10.;
    for (int i = 0; i < ndes; ++i)
        if (have[i]) xHi = std::max(xHi, (double)(res[i].de.size() - 1));
    xHi *= 1.1;

    // Threshold for DE = 0.90, and the compact list of available designs.
    std::vector<double> thr90(ndes, -1.);
    std::vector<int>    bars;
    double barMax = 1.;
    for (int i = 0; i < ndes; ++i) {
        if (!have[i]) continue;
        thr90[i] = firstCross(res[i].de, 0.90);
        if (thr90[i] >= 0.) bars.push_back(i);
        if (thr90[i] > barMax) barMax = thr90[i];
    }
    barMax *= 1.10;                    // just enough room for the value labels
    const int nBars = (int)bars.size();

    double ctrl90 = (iCtrl >= 0 && have[iCtrl]) ? thr90[iCtrl] : -1.;
    if (ctrl90 < 0.) {
        for (int i : bars) { ctrl90 = thr90[i]; break; }
    }

    TCanvas* c = new TCanvas("c_opt_de", "v7_opt detection efficiency",
                             2400, 1600);

    // One horizontal legend block above both panels.
    TPad* lp = new TPad("opt_de_leg", "", 0.0, 0.80, 1.0, 1.0);
    lp->Draw();
    TPad* pA = new TPad("opt_de_a", "", 0.0, 0.43, 1.0, 0.80);
    pA->Draw();
    TPad* pB = new TPad("opt_de_b", "", 0.0, 0.0, 1.0, 0.43);
    pB->Draw();

    // ---- panel (a): DE(N_thr) ----------------------------------------
    pA->cd();
    gStyle->SetOptTitle(1);
    pA->SetLogx();
    pA->SetLeftMargin(0.095);
    pA->SetRightMargin(0.025);
    pA->SetBottomMargin(0.19);
    pA->SetTopMargin(0.115);

    {
        TH1D* fr = (TH1D*)pA->DrawFrame(1., 0., xHi, 1.06);
        fr->SetTitle("");
        fr->GetXaxis()->SetTitle("threshold  N_{thr} [p.e. per event]");
        fr->GetYaxis()->SetTitle("detection efficiency  DE");
        fr->GetXaxis()->SetTitleSize(0.0665);
        fr->GetYaxis()->SetTitleSize(0.0665);
        fr->GetXaxis()->SetLabelSize(0.0654);
        fr->GetYaxis()->SetLabelSize(0.0654);
        fr->GetXaxis()->SetTitleOffset(1.40);   // clear of the tick labels
        fr->GetYaxis()->SetTitleOffset(0.80);
        fr->GetXaxis()->SetMoreLogLabels(kFALSE);   // decades only
        fr->GetXaxis()->SetNoExponent(kTRUE);       // 1, 10, 100, 1000
        fr->GetXaxis()->CenterTitle();              // ROOT right-aligns by default
        fr->GetYaxis()->CenterTitle();
    }
    {
        TLatex t;
        t.SetNDC();
        t.SetTextAlign(21);            // centred, bottom
        t.SetTextSize(0.0665);
        t.DrawLatexNDC(0.5, 0.905, "(a) detection efficiency versus threshold");
    }

    TLine* g80 = new TLine(1., 0.80, xHi, 0.80);
    g80->SetLineColor(kGray + 2);
    g80->SetLineStyle(2);
    g80->SetLineWidth(2);
    g80->Draw("SAME");
    TLine* g99 = new TLine(1., 0.99, xHi, 0.99);
    g99->SetLineColor(kGray + 2);
    g99->SetLineStyle(4);
    g99->SetLineWidth(2);
    g99->Draw("SAME");

    std::vector<TGraph*> graphs(ndes, nullptr);
    for (int i = 0; i < ndes; ++i) {
        if (!have[i]) continue;
        const std::vector<double>& de = res[i].de;
        const int n = (int)de.size() - 1;   // thresholds 1 .. max
        if (n < 1) continue;
        std::vector<double> xs, ys;
        xs.reserve(n);
        ys.reserve(n);
        for (int thr = 1; thr <= n; ++thr) {
            xs.push_back((double)thr);
            ys.push_back(de[(size_t)thr]);
        }
        TGraph* gr = new TGraph((int)xs.size(), xs.data(), ys.data());
        gr->SetLineColor(opt::color(i));
        gr->SetLineStyle(lineStyle(i));
        gr->SetLineWidth(3);
        gr->Draw("L SAME");
        graphs[i] = gr;
    }

    {
        TLatex lab;
        lab.SetTextSize(0.0665);
        lab.SetTextColor(kGray + 2);
        // Right-aligned and top-aligned, placed *below* its guide line so the
        // label neither touches the line nor the top of the frame.
        lab.SetTextAlign(33);
        lab.DrawLatex(0.94 * xHi, 0.780, "80 %");
        lab.DrawLatex(0.94 * xHi, 0.972, "99 %");
    }

    // ---- panel (b): threshold for DE = 0.90 ---------------------------
    // Horizontal bars: the manifest design labels are set horizontally in the
    // left margin, so nothing is rotated, nothing can overprint, and no label
    // can touch the frame.
    pB->cd();
    gStyle->SetOptTitle(1);
    pB->SetLeftMargin(0.34);
    pB->SetRightMargin(0.025);
    pB->SetBottomMargin(0.15);
    pB->SetTopMargin(0.12);

    {
        TH1D* fr = (TH1D*)pB->DrawFrame(0., 0., barMax, (double)nBars + 1.);
        fr->SetTitle("");
        fr->GetXaxis()->SetTitle("N_{thr} (DE = 0.90) [p.e.]");
        fr->GetXaxis()->SetTitleSize(0.058);
        fr->GetXaxis()->SetLabelSize(0.058);
        fr->GetXaxis()->SetTitleOffset(1.12);
        fr->GetXaxis()->CenterTitle();
        fr->GetYaxis()->SetLabelSize(0.);
        fr->GetYaxis()->SetTickLength(0.);
    }
    {
        TLatex t;
        t.SetNDC();
        t.SetTextAlign(21);            // centred, bottom
        t.SetTextSize(0.058);
        t.DrawLatexNDC(0.5, 0.905, "(b) threshold required for DE = 0.90");
    }

    // Dashed guide at the control's DE = 0.90 threshold.  Drawn before the
    // bars so the bars cover it where they cross; labelled once at the top.
    if (ctrl90 >= 0.) {
        TLine* lc = new TLine(ctrl90, 0., ctrl90, (double)nBars + 1.);
        lc->SetLineColor(kBlack);
        lc->SetLineStyle(2);
        lc->SetLineWidth(2);
        lc->Draw("SAME");
    }

    // First manifest entry on the top row.
    for (int k = 0; k < nBars; ++k) {
        const int i = bars[k];
        const double yc = (double)(nBars - k);
        TBox* b = new TBox(0., yc - 0.32, thr90[i], yc + 0.32);
        b->SetFillColor(opt::color(i));
        b->SetFillStyle(1001);
        b->SetLineColor(kBlack);
        b->SetLineWidth(1);
        b->Draw("SAME");

        TLatex v;
        v.SetTextSize(0.058);
        v.SetTextAlign(12);            // left, vertically centred
        v.DrawLatex(thr90[i] + 0.010 * barMax, yc,
                    TString::Format("%.1f", thr90[i]));
    }

    // Label the control guide at the top of the frame, clear of every bar.
    if (ctrl90 >= 0.) {
        TLatex cl;
        cl.SetTextSize(0.058);
        cl.SetTextAlign(12);           // left, vertically centred
        cl.DrawLatex(ctrl90 + 0.010 * barMax, (double)nBars + 0.34,
                     TString::Format("control %.1f", ctrl90));
    }

    // Manifest design labels in the left margin (horizontal, right-aligned).
    {
        const double lm = pB->GetLeftMargin();
        const double ybot = pB->GetBottomMargin();
        const double ytop = pB->GetTopMargin();
        for (int k = 0; k < nBars; ++k) {
            const double yc = (double)(nBars - k);
            const double yn = ybot + yc / ((double)nBars + 1.)
                                     * (1. - ybot - ytop);
            TLatex tl;
            tl.SetNDC();
            tl.SetTextSize(0.058);
            tl.SetTextAlign(32);       // right, vertically centred
            tl.DrawLatexNDC(lm - 0.012, yn, designs[bars[k]].label);
        }
    }

    // One legend for the whole figure, above both panels.  TLegend fills
    // row-major, so the entries are interleaved; reading each column downwards
    // then reproduces the manifest order.
    lp->cd();
    {
        TLegend* leg = new TLegend(0.22, 0.03, 0.78, 0.93);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetTextSize(0.1225);
        leg->SetNColumns(2);
        const int nRows = (ndes + 1) / 2;
        for (int row = 0; row < nRows; ++row) {
            for (int col = 0; col < 2; ++col) {
                const int i = col * nRows + row;
                if (i < ndes && graphs[i])
                    leg->AddEntry(graphs[i], designs[i].label, "l");
            }
        }
        leg->Draw();
    }

    for (const char* ex : {"pdf", "png"}) {
        c->SaveAs(TString::Format("%s/fig_opt_de.%s", outdir, ex));
    }
    printf("opt_de: wrote %s/fig_opt_de.{pdf,png}, opt_de.csv, "
           "opt_de_summary.csv (%d/%d designs)\n", outdir, nHave, ndes);
}
