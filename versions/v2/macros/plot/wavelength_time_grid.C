// wavelength_time_grid.C -- arrival time versus wavelength of the detected
// photoelectrons, one panel per SiPM configuration (1-4), from the whole-plate
// irradiation.
//
// Each detected photoelectron contributes one entry at
// (tDetected, wlDetected_nm) from the sipmHits tree; summing over every SiPM of
// the layout gives a two-dimensional occupancy:
//
//     x = arrival time [ns]
//     y = detected wavelength [nm]
//     z = photoelectrons (colour)
//
// Configurations 1-3 detect the blue scintillation spectrum directly, so the
// distribution should be a near-vertical band around ~460 nm whose vertical
// extent narrows with time as the slower, red-shifted (self-absorbed) light
// arrives; config 4 is green-shifted by the Y-11 WLS fibres (~485 nm), so its
// band sits at longer wavelengths.
//
// Usage (batch):
//   root -l -b -q 'wavelength_time_grid.C("slurm/data_v2.2/whole","report/figures",3)'
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

void wavelength_time_grid(const char* dir = "slurm/data_v2.2/whole",
                          const char* outdir = "report/figures",
                          int refl = 3)
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);      // style() hides titles; each panel is labelled
    gSystem->mkdir(outdir, kTRUE);

    const int    ntb  = 100;     // time: 0-100 ns, 1 ns/bin
    const double tmin = 0.0, tmax = 100.0;
    const int    nwb  = 130;     // wavelength: 360-620 nm, 2 nm/bin
    const double wmin = 360.0, wmax = 620.0;

    std::map<int, TH2D*> hMap;
    for (int cfg = 1; cfg <= 4; ++cfg) {
        TH2D* h = new TH2D(TString::Format("h_wltime_c%d", cfg),
                           TString::Format("%s;arrival time [ns];wavelength [nm]",
                                           kCfgName[cfg]),
                           ntb, tmin, tmax, nwb, wmin, wmax);
        h->SetDirectory(nullptr);
        hMap[cfg] = h;
    }

    for (int cfg = 1; cfg <= 4; ++cfg) {
        const TString fname = TString::Format("%s/muon_whole_out_c%d_r%d.root",
                                              dir, cfg, refl);
        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "wavelength_time_grid: cannot open " << fname << "\n";
            delete f;
            continue;
        }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }
        if (!t->GetBranch("tDetected") || !t->GetBranch("wlDetected_nm")) {
            std::cerr << "wavelength_time_grid: missing timing columns in "
                      << fname << " (regenerate the data)\n";
            delete f;
            continue;
        }

        int det = 0;
        std::vector<double>* times = nullptr;
        std::vector<double>* wls   = nullptr;
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("tDetected", &times);
        t->SetBranchAddress("wlDetected_nm", &wls);

        TH2D* h = hMap[cfg];
        long nPE = 0;
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0 || !times || !wls) continue;
            const std::size_t n = std::min(times->size(), wls->size());
            for (std::size_t p = 0; p < n; ++p) {
                h->Fill((*times)[p], (*wls)[p]);
                ++nPE;
            }
        }
        std::cout << "cfg" << cfg << ": " << nPE << " photoelectrons\n";
        delete f;
    }

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

    TCanvas* c = new TCanvas("c_wltime", "wavelength vs arrival time", 1100, 900);
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
        c->SaveAs(TString::Format("%s/fig_wavelength_time_grid.%s", outdir, ex));

    std::cout << "wavelength_time_grid: wrote fig_wavelength_time_grid.{pdf,png} to "
              << outdir << "\n";
}
