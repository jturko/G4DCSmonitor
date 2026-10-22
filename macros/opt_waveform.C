// opt_waveform.C -- average arrival-time waveform for the v7_opt detector
// designs (Phase 2, "detector optimisation").
//
// This is the design-orientated analogue of avg_waveform.C.  For every design
// the per-SiPM arrival times of the detected photoelectrons
// (sipmHits.tDetected) are summed over the layout's SiPMs and histogrammed in
// 0.1 ns bins; dividing the summed histogram by the number of events gives
//
//     x = arrival time [ns]
//     y = mean p.e. in that 0.1 ns bin (per event)
//
// exactly the ordinate of avg_waveform.C.  The left panel is linear over
// 0-25 ns (the prompt scintillation/Cerenkov light); the right panel is
// logarithmic over 0-100 ns, which exposes the ~7 ns exponential WLS tail of
// the four-sided fibre designs.  Both panels share one legend.
//
// The same single pass also extracts, per design:
//   npe              mean detected p.e. per event
//   prompt_frac_3ns  fraction of p.e. arriving before 3 ns
//   t_median         median arrival time
//   t90              time containing 90 % of the p.e.
//   mean_time        mean arrival time
//   f_wls, f_scint, f_cerenk
//                    creator mix of the detected p.e. from creatorDetected
//                    (1 = OpWLS, 2 = Scintillation, 3 = Cerenkov)
// and writes them to opt_waveform.csv.
//
// A design whose file is missing or unreadable is skipped ENTIRELY (warning on
// stderr): it gets no curve, no legend entry and no CSV row, so the previous
// campaign's figures remain valid until a new design's data lands.
//
// Usage:
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     root -l -b -q 'macros/opt_waveform.C("DIR","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <vector>

#include "opt_designs.h"

namespace {

const int    kNT   = 1000;   // arrival-time bins
const double kTmin = 0.0;    // ns
const double kTmax = 100.0;  // ns  (0.1 ns per bin)
const double kDt   = (kTmax - kTmin) / kNT;

struct Result {
    TString             tag;
    std::vector<double> wave;      // mean p.e. per bin per event
    long long           nEvents = 0;
    long long           npe     = 0;       // total detected p.e.
    long long           nIn     = 0;       // p.e. inside the time window
    long long           prompt  = 0;       // p.e. with t < 3 ns
    double              sumT    = 0.;
    double              tMedian = -1.;
    double              t90     = -1.;
    double              fWls = 0., fScint = 0., fCer = 0.;
    long long           cWls = 0, cScint = 0, cCer = 0, cUnknown = 0;
};

// Second member of each same-treatment pair is dashed; the colour is still
// opt::color(i), so a design keeps its colour in every figure.
int lineStyle(int i)
{
    return (i == 1 || i == 3 || i == 6 || i == 8) ? 2 : 1;
}

}  // namespace

void opt_waveform(const char* dir = "slurm/data_v7_opt/whole",
                  const char* outdir = "report/figures")
{
    style(0.045);
    gSystem->mkdir(outdir, kTRUE);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);

    const std::vector<opt::Design> designs = opt::designs();
    const int ndes = (int)designs.size();

    std::vector<Result> res(ndes);
    std::vector<bool>   have(ndes, false);

    // ------------------------------------------------------------------
    // One single pass per file.
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
        if (!t || !t->GetBranch("tDetected") || !t->GetBranch("creatorDetected")) {
            fprintf(stderr, "  SKIP %-20s  (no tDetected/creatorDetected branch)\n",
                    d.tag);
            delete f;
            continue;
        }

        int evt = 0, nd = 0;
        std::vector<double>* times  = nullptr;
        std::vector<int>*    creat  = nullptr;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("nDetected", &nd);
        t->SetBranchAddress("tDetected", &times);
        t->SetBranchAddress("creatorDetected", &creat);

        std::vector<double> sum((size_t)kNT, 0.);
        // Events are counted by their distinct evtNb values: an event whose
        // muon missed the slab (a cut-away corner triangle of preset 12)
        // writes no rows at all, so max(evtNb)+1 would over-count.
        std::set<int> seenEvt;
        long long npe = 0, nIn = 0, prompt = 0;
        long long cWls = 0, cScint = 0, cCer = 0, cUnknown = 0;
        double sumT = 0.;

        const Long64_t nent = t->GetEntries();
        for (Long64_t e = 0; e < nent; ++e) {
            t->GetEntry(e);
            if (evt >= 0) seenEvt.insert(evt);
            if (!times) continue;

            for (double tt : *times) {
                ++npe;
                sumT += tt;
                if (tt < 3.0) ++prompt;
                const int b = (int)((tt - kTmin) / kDt);
                if (b >= 0 && b < kNT) {
                    sum[(size_t)b] += 1.;
                    ++nIn;
                }
            }
            if (creat && creat->size() == times->size()) {
                for (int c : *creat) {
                    switch (c) {
                        case 1: ++cWls;     break;
                        case 2: ++cScint;   break;
                        case 3: ++cCer;     break;
                        default: ++cUnknown; break;
                    }
                }
            }
        }
        delete f;

        // Number of events = number of distinct evtNb values present.
        const long long nEvents = (long long)seenEvt.size();
        if (nEvents <= 0) {
            fprintf(stderr, "  SKIP %-20s  (no events)\n", d.tag);
            continue;
        }

        Result& r = res[i];
        r.nEvents = nEvents;
        r.npe     = npe;
        r.nIn     = nIn;
        r.prompt  = prompt;
        r.sumT    = sumT;
        r.cWls = cWls; r.cScint = cScint; r.cCer = cCer; r.cUnknown = cUnknown;

        r.wave.assign((size_t)kNT, 0.);
        for (int b = 0; b < kNT; ++b)
            r.wave[(size_t)b] = sum[(size_t)b] / (double)nEvents;

        // Median and 90 % containment from the binned cumulative distribution.
        long long cum = 0;
        r.tMedian = -1.; r.t90 = -1.;
        for (int b = 0; b < kNT; ++b) {
            cum += (long long)sum[(size_t)b];
            const double tc = kTmin + (b + 0.5) * kDt;
            if (r.tMedian < 0. && npe > 0 && cum >= 0.5 * (double)npe) r.tMedian = tc;
            if (r.t90 < 0. && npe > 0 && cum >= 0.9 * (double)npe) { r.t90 = tc; break; }
        }
        if (r.tMedian < 0.) r.tMedian = kTmax;
        if (r.t90 < 0.)     r.t90 = kTmax;

        if (npe > 0) {
            r.fWls   = (double)cWls   / (double)npe;
            r.fScint = (double)cScint / (double)npe;
            r.fCer   = (double)cCer   / (double)npe;
        }
        have[i] = true;

        printf("  %-20s events=%6lld  mean=%7.2f pe  prompt<3ns=%.3f  "
               "t90=%6.2f ns  f_WLS=%.3f\n",
               d.tag, nEvents, (double)npe / (double)nEvents,
               npe > 0 ? (double)prompt / (double)npe : 0., r.t90, r.fWls);
    }

    int nHave = 0;
    for (bool b : have) if (b) ++nHave;
    if (nHave == 0) {
        printf("opt_waveform: no readable files under %s -- nothing to do\n", dir);
        return;
    }

    // ------------------------------------------------------------------
    // CSV
    // ------------------------------------------------------------------
    FILE* fp = fopen(TString::Format("%s/opt_waveform.csv", outdir), "w");
    if (fp) {
        fprintf(fp, "tag,events,npe,prompt_frac_3ns,t_median,t90,mean_time,"
                    "f_wls,f_scint,f_cerenk\n");
        for (int i = 0; i < ndes; ++i) {
            if (!have[i]) continue;
            const Result& r = res[i];
            const double npeEv = (double)r.npe / (double)r.nEvents;
            const double pf   = r.npe > 0 ? (double)r.prompt / (double)r.npe : 0.;
            const double mt   = r.npe > 0 ? r.sumT / (double)r.npe : -1.;
            fprintf(fp, "%s,%lld,%.4f,%.5f,%.2f,%.2f,%.3f,%.5f,%.5f,%.5f\n",
                    r.tag.Data(), r.nEvents, npeEv, pf, r.tMedian, r.t90, mt,
                    r.fWls, r.fScint, r.fCer);
        }
        fclose(fp);
    } else {
        printf("opt_waveform: cannot write %s/opt_waveform.csv\n", outdir);
    }

    // ------------------------------------------------------------------
    // Figure
    // ------------------------------------------------------------------
    double yMax = 0.;
    for (int i = 0; i < ndes; ++i) {
        if (!have[i]) continue;
        for (int b = 0; b < kNT; ++b)
            if (b * kDt < 25.) yMax = std::max(yMax, res[i].wave[(size_t)b]);
    }
    if (yMax <= 0.) yMax = 1.;

    TCanvas* c = new TCanvas("c_opt_waveform", "v7_opt arrival-time waveform",
                             2400, 1200);

    // One legend shared by both panels, above them.
    TPad* lp = new TPad("opt_wave_leg", "", 0.0, 0.74, 1.0, 1.0);
    lp->Draw();
    TPad* p1 = new TPad("opt_wave_lin", "", 0.0, 0.0, 0.5, 0.74);
    p1->Draw();
    TPad* p2 = new TPad("opt_wave_log", "", 0.5, 0.0, 1.0, 0.74);
    p2->Draw();

    std::vector<TGraph*> graphs(ndes, nullptr);
    for (int i = 0; i < ndes; ++i) {
        if (!have[i]) continue;
        const std::vector<double>& w = res[i].wave;
        TGraph* gr = new TGraph(kNT);
        for (int b = 0; b < kNT; ++b)
            gr->SetPoint(b, kTmin + (b + 0.5) * kDt, w[(size_t)b]);
        gr->SetLineColor(opt::color(i));
        gr->SetLineStyle(lineStyle(i));
        gr->SetLineWidth(3);
        graphs[i] = gr;
    }

    auto drawPanel = [&](TPad* pad, bool logy, double xhi, const char* title) {
        pad->cd();
        gStyle->SetOptTitle(0);
        if (logy) pad->SetLogy();
        pad->SetLeftMargin(0.135);
        pad->SetRightMargin(0.03);
        pad->SetBottomMargin(0.16);
        pad->SetTopMargin(0.12);

        const double ylo = logy ? 1e-4 : 0.;
        const double yhi = logy ? yMax * 3.0 : yMax * 1.12;
        TH1D* fr = (TH1D*)pad->DrawFrame(0., ylo, xhi, yhi,
                                         ";arrival time [ns];"
                                         "mean p.e. / 0.1 ns / event");
        fr->SetTitle("");
        fr->GetXaxis()->SetTitleSize(0.0497);
        fr->GetYaxis()->SetTitleSize(0.0497);
        fr->GetXaxis()->SetLabelSize(0.0476);
        fr->GetYaxis()->SetLabelSize(0.0476);
        fr->GetXaxis()->SetTitleOffset(1.15);
        fr->GetYaxis()->SetTitleOffset(1.26);
        fr->GetXaxis()->CenterTitle();
        fr->GetYaxis()->CenterTitle();

        for (int i = 0; i < ndes; ++i)
            if (graphs[i]) graphs[i]->Draw("L SAME");

        TLatex t;
        t.SetNDC();
        t.SetTextAlign(21);            // centred, bottom
        t.SetTextSize(0.0519);
        t.DrawLatexNDC(0.5, 0.90, title);
    };

    drawPanel(p1, false, 25., "(a) linear ordinate, 0-25 ns");
    drawPanel(p2, true, 100., "(b) logarithmic ordinate, 0-100 ns");

    // One shared legend block, centred over the two panels.  TLegend fills
    // row-major, so the entries are interleaved; reading each column downwards
    // then reproduces the manifest order.
    lp->cd();
    {
        TLegend* leg = new TLegend(0.25, 0.02, 0.75, 0.94);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetTextSize(0.127);
        leg->SetMargin(0.10);
        leg->SetColumnSeparation(0.0);
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
        c->SaveAs(TString::Format("%s/fig_opt_waveform.%s", outdir, ex));
    }

    // ------------------------------------------------------------------
    // Compact table
    // ------------------------------------------------------------------
    printf("\n%-20s %7s %9s %8s %8s %8s %9s %7s %7s %7s\n",
           "design", "events", "pe/evt", "prompt", "t_med", "t90", "mean_t",
           "f_WLS", "f_scint", "f_cer");
    for (int i = 0; i < ndes; ++i) {
        if (!have[i]) continue;
        const Result& r = res[i];
        const double npeEv = (double)r.npe / (double)r.nEvents;
        const double pf   = r.npe > 0 ? (double)r.prompt / (double)r.npe : 0.;
        const double mt   = r.npe > 0 ? r.sumT / (double)r.npe : -1.;
        printf("%-20s %7lld %9.2f %8.3f %8.2f %8.2f %9.3f %7.3f %7.3f %7.3f\n",
               r.tag.Data(), r.nEvents, npeEv, pf, r.tMedian, r.t90, mt,
               r.fWls, r.fScint, r.fCer);
        if (r.cUnknown)
            printf("    note: %lld photoelectrons with unknown creator (code 0)\n",
                   r.cUnknown);
        if (r.nIn != r.npe)
            printf("    note: %lld p.e. outside the 0-100 ns window\n",
                   r.npe - r.nIn);
    }
    printf("\nopt_waveform: wrote %s/fig_opt_waveform.{pdf,png} and "
           "opt_waveform.csv (%d/%d designs)\n", outdir, nHave, ndes);
}
