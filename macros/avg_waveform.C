// avg_waveform.C -- the average arrival-time waveform, one curve per SiPM
// configuration (1, 2, 12, 3, 4), from the whole-plate irradiation.  This is the
// persistence map of persistence_map_grid.C averaged over events: for every
// event the per-SiPM arrival times (sipmHits.tDetected) are summed over the
// layout's SiPMs and histogrammed into 0.1 ns bins; averaging those per-event
// "traces" over all events gives the mean photoelectron rate versus arrival
// time,
//
//     x = arrival time [ns]
//     y = mean p.e. in that 0.1 ns bin (per event)
//
// All five configurations are drawn on the same axes (left: linear ordinate,
// right: logarithmic ordinate, which exposes the WLS-shifted late tail of
// config 4).  cfg12 (C2m) keeps P6(4) and a dashed line so it stays
// identifiable where its tail overlaps the other curves.  Times are the
// Geant4 global times, so the common primary-muon launch offset is shared by
// every curve and the features are the propagation, scintillation and (for
// config 4) WLS re-emission delays.
//
// Usage (batch):
//   root -l -b -q 'macros/avg_waveform.C("slurm/data_v2.2/whole","report/figures",3)'
//
// Args: dir (default "slurm/data_v2.2/whole"), outdir (default "report/figures"),
//       refl (default 3 = glossy paper).

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TLegend.h>
#include <TH1F.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <cmath>
#include <iostream>
#include <map>
#include <vector>

#include "style.h"

namespace {
// Curve order: configs 1, 2, 12 (corner-cut "Config 2 (mod)"), 3, 4.
const int kCfgOrder[5] = {1, 2, 12, 3, 4};
const int kNPad      = 5;
// Compact labels: the legend has to carry five entries.  cfg12 uses the
// short form C2m; the report uses "Config 2 (mod)" in running text.
const char* kCfgName[5] = {"config 1 (8 SiPMs)", "config 2 (4 SiPMs)",
                           "C2m (4 diagonal SiPMs)", "config 3 (2 SiPMs)",
                           "config 4 (WLS + 1 SiPM)"};

// Config 1-4 keep P6(0..3); the corner-cut layout is P6(4).  Never call
// getP6(cfg - 1) with cfg = 12.
int cfgColor(int cfg) { return PetroffPalette::getP6(cfg == 12 ? 4 : cfg - 1); }
}  // namespace

void avg_waveform(const char* dir = "slurm/data_v2.2/whole",
                  const char* outdir = "report/figures",
                  int refl = 3)
{
    style(0.055, 0.13);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gSystem->mkdir(outdir, kTRUE);

    const int    ntb  = 600;    // arrival-time bins ...
    const double tmin = 0.0;    // ... ns ...
    const double tmax = 60.0;   // ... over 0-60 ns (0.1 ns/bin)
    const double dt   = (tmax - tmin) / ntb;

    std::map<int, std::vector<double>> mean;   // cfg -> mean p.e. per time bin
    std::map<int, long> nEvt;                  // cfg -> number of events

    for (int slot = 0; slot < kNPad; ++slot) {
        const int cfg = kCfgOrder[slot];
        const TString fname = TString::Format("%s/muon_whole_out_c%d_r%d.root",
                                              dir, cfg, refl);
        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "avg_waveform: cannot open " << fname
                      << " -- omitting its curve\n";
            delete f;
            continue;
        }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t || !t->GetBranch("tDetected")) {
            std::cerr << "avg_waveform: no tDetected branch in " << fname
                      << " -- omitting its curve\n";
            delete f;
            continue;
        }

        int evt = 0, det = 0;
        std::vector<double>* times = nullptr;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("tDetected", &times);

        // Accumulate, per event, the photoelectron count in each time bin,
        // summed over every SiPM of the layout; then sum over events.  Rows are
        // written per (event, SiPM) with det fixed at 0 (single slab).
        std::map<int, std::vector<int>> evBins;
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0 || !times) continue;
            std::vector<int>& bins = evBins[evt];
            if (bins.empty()) bins.assign(ntb, 0);
            for (double tt : *times) {
                int b = (int)((tt - tmin) / dt);
                if (b >= 0 && b < ntb) ++bins[b];
            }
        }
        std::vector<double> sum(ntb, 0.0);
        for (const auto& kv : evBins)
            for (int b = 0; b < ntb; ++b) sum[b] += kv.second[b];
        const long n = (long)evBins.size();
        nEvt[cfg] = n;
        std::vector<double>& m = mean[cfg];
        m.assign(ntb, 0.0);
        if (n > 0) for (int b = 0; b < ntb; ++b) m[b] = sum[b] / (double)n;

        double area = 0.0;
        for (int b = 0; b < ntb; ++b) area += m[b];
        std::cout << "cfg" << cfg << ": " << n << " events, mean yield "
                  << area << " p.e./event (integrated)" << std::endl;
        delete f;
    }

    // Build one graph per config over the bin centres.
    double yMax = 0.0;
    std::map<int, TGraph*> g;
    for (int slot = 0; slot < kNPad; ++slot) {
        const int cfg = kCfgOrder[slot];
        if (!mean.count(cfg) || nEvt[cfg] <= 0) continue;
        const std::vector<double>& m = mean[cfg];
        auto* gr = new TGraph(ntb);
        for (int b = 0; b < ntb; ++b) {
            const double y = m[b];
            gr->SetPoint(b, tmin + (b + 0.5) * dt, y);
            if (y > yMax) yMax = y;
        }
        gr->SetLineColor(cfgColor(cfg));
        gr->SetLineWidth(2);
        // A distinct style keeps the fifth curve identifiable in both the
        // linear and the logarithmic panel (colour alone is not enough once
        // the late tails overlap).
        if (cfg == 12) gr->SetLineStyle(2);
        g[cfg] = gr;
    }
    if (yMax <= 0.) yMax = 1.;

    TCanvas* c = new TCanvas("c_avg_waveform", "average arrival-time waveform",
                             1120, 540);
    c->Divide(2, 1, 0.004, 0.004);

    auto drawPanel = [&](int pad, bool logy) {
        c->cd(pad);
        if (logy) gPad->SetLogy();
        gPad->SetLeftMargin(0.13); gPad->SetRightMargin(0.03);
        gPad->SetBottomMargin(0.15); gPad->SetTopMargin(0.30);
        TH1F* fr = gPad->DrawFrame(
            0., logy ? 1e-4 : 0., 60., logy ? yMax * 2.0 : yMax * 1.1,
            ";arrival time [ns];"
            "mean p.e. / 0.1 ns bin / event");
        fr->GetYaxis()->SetTitleOffset(1.15);
        fr->GetYaxis()->SetTitleSize(0.050); fr->GetXaxis()->SetTitleSize(0.050);
        fr->GetXaxis()->SetLabelSize(0.040);
        fr->GetYaxis()->SetLabelSize(0.040);
        // Five entries in two columns, kept entirely above the frame top
        // (frame top sits at 1 - 0.30 = 0.70) so no trace is covered.
        auto* l = new TLegend(0.02, 0.735, 0.98, 0.995);
        l->SetBorderSize(0); l->SetFillStyle(0);
        l->SetTextSize(0.036);
        l->SetNColumns(2);
        for (int slot = 0; slot < kNPad; ++slot) {
            const int cfg = kCfgOrder[slot];
            if (!g.count(cfg)) continue;
            g[cfg]->Draw("L SAME");
            l->AddEntry(g[cfg], kCfgName[slot], "l");
        }
        l->Draw();
    };

    drawPanel(1, false);
    drawPanel(2, true);

    for (auto ex : {"pdf", "png"})
        c->SaveAs(TString::Format("%s/fig_avg_waveform.%s", outdir, ex));
    std::cout << "avg_waveform: wrote fig_avg_waveform.{pdf,png} to "
              << outdir << std::endl;
}
