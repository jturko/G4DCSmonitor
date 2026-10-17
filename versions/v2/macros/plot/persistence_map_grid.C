// persistence_map_grid.C -- oscilloscope-style "persistence" map of the SiPM
// arrival-time profile, one panel per SiPM configuration (1-4), from the
// whole-plate irradiation.
//
// For each event the per-SiPM arrival times (sipmHits.tDetected, one entry per
// detected photoelectron, global ns) are summed over all of the layout's SiPMs
// and histogrammed into fixed arrival-time bins.  A single event therefore gives
// a "trace" of photoelectron-count versus arrival time; stacking many events and
// recording, for every time bin, how many events showed a given count, yields the
// persistence map:
//
//     x = arrival time [ns]
//     y = photoelectrons in that 0.1 ns time bin
//     z = number of events (colour)
//
// This is the discrete-photon analogue of a scope persistence display: no pulse
// shape is convolved in, the map simply shows where the detected light lands in
// time and how the instantaneous rate is distributed event to event.  The
// absolute time is the Geant4 global time (the primary muon is launched from a
// fixed plane at a fixed energy, so the entry time is a common offset and the
// features are the propagation and scintillation delays).
//
// Both axes are linear.  The time axis runs 0-60 ns with 0.1 ns bins; all four
// panels share one photoelectron axis, 0-30 p.e. at 1 p.e./bin, so the
// amplitudes stay directly comparable.
//
// Usage (batch):
//   root -l -b -q 'persistence_map_grid.C("slurm/data_v2.2/whole","report/figures",3)'
//
// Args: dir (default "slurm/data_v2.2/whole"), outdir (default "report/figures"),
//       refl (default 3 = glossy paper).

#include <TCanvas.h>
#include <TFile.h>
#include <TH2D.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions).  Located via
// ROOT_INCLUDE_PATH (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {
const char* kCfgName[5] = {"", "config 1 (8 SiPMs)", "config 2 (4 SiPMs)",
                           "config 3 (2 SiPMs)", "config 4 (WLS + 1 SiPM)"};
}  // namespace

void persistence_map_grid(const char* dir = "slurm/data_v2.2/whole",
                          const char* outdir = "report/figures",
                          int refl = 3)
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);      // style() hides titles; each panel is labelled
    gSystem->mkdir(outdir, kTRUE);

    const int    ntb   = 600;    // arrival-time bins ...
    const double tmin  = 0.0;    // ... ns ...
    const double tmax  = 60.0;   // ... over 0-60 ns (0.1 ns/bin, linear)
    const double dt    = (tmax - tmin) / ntb;

    const int    nyb   = 30;     // photoelectron bins (1 p.e./bin) ...
    const double ymin  = 0.0;    // ...
    const double ymax  = 30.0;   // ... over 0-30 p.e. (linear, shared)

    // Per-event photoelectron count in each time bin, kept for every config so
    // all four maps can be booked after the read pass.
    std::map<int, std::map<int, std::vector<int>>> cfgEv;

    for (int cfg = 1; cfg <= 4; ++cfg) {
        const TString fname = TString::Format("%s/muon_whole_out_c%d_r%d.root",
                                              dir, cfg, refl);
        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "persistence_map_grid: cannot open " << fname << "\n";
            delete f;
            continue;
        }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }
        if (!t->GetBranch("tDetected")) {
            std::cerr << "persistence_map_grid: no tDetected branch in " << fname
                      << " (regenerate the data with the timing columns)\n";
            delete f;
            continue;
        }

        int evt = 0, det = 0;
        std::vector<double>* times = nullptr;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("tDetected", &times);

        // Accumulate, per event, the photoelectron count in each time bin
        // (summed over every SiPM of the layout).  Rows are written per
        // (event, SiPM) and events are contiguous, but a map keyed on the event
        // number keeps this robust to any ordering.
        std::map<int, std::vector<int>>& evBins = cfgEv[cfg];
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
        delete f;
    }

    // One 2-D map per config.  All four panels share a single linear photoelectron
    // axis, 0-30 p.e. at 1 p.e./bin, so the amplitudes are directly comparable.
    std::map<int, TH2D*> hMap;
    for (int cfg = 1; cfg <= 4; ++cfg) {
        TH2D* h = new TH2D(TString::Format("h_persist_c%d", cfg),
                           TString::Format("%s;arrival time [ns];p.e. / 0.1 ns bin",
                                           kCfgName[cfg]),
                           ntb, tmin, tmax, nyb, ymin, ymax);
        h->SetDirectory(nullptr);
        hMap[cfg] = h;
    }

    // One entry per (event, time bin) with a non-zero count: the vertical
    // position is the instantaneous photoelectron count, the weight is one
    // event.  Empty time bins are omitted (the baseline would otherwise
    // dominate); the map therefore shows where light actually arrives.
    for (int cfg = 1; cfg <= 4; ++cfg) {
        TH2D* h = hMap[cfg];
        long samples = 0;
        int peak = 0;
        for (const auto& kv : cfgEv[cfg]) {
            const std::vector<int>& bins = kv.second;
            for (int b = 0; b < ntb; ++b) {
                if (bins[b] <= 0) continue;
                h->Fill(tmin + (b + 0.5) * dt, bins[b]);
                if (bins[b] > peak) peak = bins[b];
                ++samples;
            }
        }
        std::cout << "cfg" << cfg << ": " << cfgEv[cfg].size() << " events, "
                  << samples << " (event,bin) samples, peak " << peak
                  << " p.e. / " << dt << " ns bin\n";
    }

    // Shared logarithmic z-range over the four maps (event counts span decades).
    double zMax = 0., zMinPos = 0.;
    for (int cfg = 1; cfg <= 4; ++cfg) {
        TH2D* h = hMap[cfg];
        for (int bx = 1; bx <= h->GetNbinsX(); ++bx)
            for (int by = 1; by <= h->GetNbinsY(); ++by) {
                const double v = h->GetBinContent(bx, by);
                if (v > zMax) zMax = v;
                if (v > 0. && (zMinPos == 0. || v < zMinPos)) zMinPos = v;
            }
    }
    if (zMax <= 0.) zMax = 1.;
    const double zMin = (zMinPos > 0.) ? zMinPos / 1.5 : zMax * 1e-3;

    for (int cfg = 1; cfg <= 4; ++cfg) {
        TH2D* h = hMap[cfg];
        h->SetMinimum(zMin);
        h->SetMaximum(zMax * 1.05);
        h->GetXaxis()->SetTitleSize(0.048);
        h->GetYaxis()->SetTitleSize(0.048);
        h->GetZaxis()->SetTitleSize(0.045);
        h->GetXaxis()->SetLabelSize(0.042);
        h->GetYaxis()->SetLabelSize(0.042);
        h->GetZaxis()->SetLabelSize(0.040);
        h->GetXaxis()->SetTitleOffset(1.15);
        h->GetYaxis()->SetTitleOffset(1.30);
        h->GetZaxis()->SetTitleOffset(1.40);
    }

    TCanvas* c = new TCanvas("c_persist", "arrival-time persistence", 1100, 900);
    c->Divide(2, 2, 0.004, 0.004);
    for (int cfg = 1; cfg <= 4; ++cfg) {
        c->cd(cfg);
        gPad->SetLeftMargin(0.12);
        gPad->SetRightMargin(0.16);
        gPad->SetBottomMargin(0.14);
        gPad->SetTopMargin(0.10);
        gPad->SetLogz();
        hMap[cfg]->Draw("COLZ");
    }
    for (auto ex : {"pdf", "png"})
        c->SaveAs(TString::Format("%s/fig_persistence_grid.%s", outdir, ex));

    std::cout << "persistence_map_grid: wrote fig_persistence_grid.{pdf,png} to "
              << outdir << "  (shared linear y axis, 0-30 p.e.)\n";
}
