// whole_yield_table.C -- mean photoelectron yield per event and per *detected*
// event, for the whole-plate (full-slab) irradiations, for every layout
// (configs 1, 2, 2 (mod) [12], 3, 4) and reflector treatment
// (bare / TiO2 / Al / glossy).
//
// For each file the per-event total is the sum of sipmHits.nDetected over all
// SiPMs of the layout; an event is "detected" when that total is at least one
// photoelectron.  Reported per (config, wrapper):
//
//   N        number of events
//   Ndet     number of events with >= 1 detected photoelectron
//   eps      detection probability  Ndet / N
//   <pe>     mean photoelectrons per event (all events)
//   <pe>det  mean photoelectrons per detected event
//
// Config order is 1, 2, 12 ("Config 2 (mod)", the corner-cut layout), 3, 4.
// A missing or partial file (the c12 whole-plate run may still be in progress)
// is reported on stderr and its rows are skipped, never treated as zero yield.
//
// Writes <outdir>/whole_yield.csv and prints a LaTeX body.  The CSV keeps the
// numeric config id and adds a human label column, so the corner-cut layout
// appears as a "Config 2 (mod)" row.
//
// Usage:
//   root -l -b -q 'macros/whole_yield_table.C("slurm/data_v5/whole","report/figures")'

#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <TSystem.h>

#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

namespace {
const int   kCfgVal[5]   = {1, 2, 12, 3, 4};
const char* kCfgLong[5]  = {"Config 1", "Config 2", "Config 2 (mod)",
                            "Config 3", "Config 4"};
const int   kNcfg        = 5;
const char* kRefl[4]     = {"bare", "TiO2", "Al", "glossy"};
}  // namespace

void whole_yield_table(const char* dir = "slurm/data_v5/whole",
                       const char* outdir = "report/figures")
{
    gSystem->mkdir(outdir, kTRUE);

    double peDet[kNcfg][4], peAll[kNcfg][4], eps[kNcfg][4];
    double nEvt[kNcfg][4], nDet[kNcfg][4];
    bool   have[kNcfg][4];

    for (int i = 0; i < kNcfg; ++i) {
        const int c = kCfgVal[i];
        for (int r = 0; r < 4; ++r) {
            const TString fname =
                TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, c, r);
            double N = 0., Nd = 0., tot = 0.;
            bool ok = false;
            TFile* f = TFile::Open(fname, "READ");
            if (!f || f->IsZombie()) {
                std::cerr << "whole_yield_table: warning: missing/unreadable "
                          << fname << " -- skipping " << kCfgLong[i] << " "
                          << kRefl[r] << "\n";
                delete f;
            } else {
                std::map<int, double> sums;
                TTree* t = (TTree*)f->Get("sipmHits");
                if (!t || !t->GetBranch("nDetected")) {
                    std::cerr << "whole_yield_table: warning: no sipmHits/"
                                 "nDetected in " << fname
                              << " (partial file?) -- skipping "
                              << kCfgLong[i] << " " << kRefl[r] << "\n";
                } else {
                    int evt = 0, nd = 0;
                    t->SetBranchAddress("evtNb", &evt);
                    t->SetBranchAddress("nDetected", &nd);
                    for (long e = 0; e < t->GetEntries(); ++e) {
                        t->GetEntry(e);
                        sums[evt] += nd;
                    }
                    TTree* os = (TTree*)f->Get("opticalStats");
                    N = os ? (double)os->GetEntries() : (double)sums.size();
                    for (const auto& kv : sums) {
                        tot += kv.second;
                        if (kv.second > 0.) ++Nd;
                    }
                    ok = (N > 0.);
                    if (!ok)
                        std::cerr << "whole_yield_table: warning: no events in "
                                  << fname << " (partial file?) -- skipping "
                                  << kCfgLong[i] << " " << kRefl[r] << "\n";
                }
                delete f;
            }
            have[i][r]  = ok;
            nEvt[i][r]  = N;
            nDet[i][r]  = Nd;
            eps[i][r]   = (N > 0.) ? Nd / N : 0.;
            peAll[i][r] = (N > 0.) ? tot / N : 0.;
            peDet[i][r] = (Nd > 0.) ? tot / Nd : 0.;
        }
    }

    // CSV: numeric config + human label, one row per measured (config, wrapper).
    FILE* fp = fopen(TString::Format("%s/whole_yield.csv", outdir), "w");
    if (fp) {
        fprintf(fp, "config,label,wrapper,N,Ndet,eps,pe_all,pe_detected\n");
        for (int i = 0; i < kNcfg; ++i)
            for (int r = 0; r < 4; ++r) {
                if (!have[i][r]) continue;           // skip missing/partial
                fprintf(fp, "%d,%s,%s,%.0f,%.0f,%.4f,%.3f,%.3f\n",
                        kCfgVal[i], kCfgLong[i], kRefl[r],
                        nEvt[i][r], nDet[i][r], eps[i][r],
                        peAll[i][r], peDet[i][r]);
            }
        fclose(fp);
    }

    // Console summary
    printf("\n config |");
    for (int r = 0; r < 4; ++r) printf("   %-6s", kRefl[r]);
    printf("   (mean p.e./detected event)\n");
    for (int i = 0; i < kNcfg; ++i) {
        printf("   %-14s |", kCfgLong[i]);
        for (int r = 0; r < 4; ++r) {
            if (have[i][r]) printf(" %8.2f", peDet[i][r]);
            else            printf(" %8s", "n/a");
        }
        printf("\n");
    }
    printf("\n detection probability eps:\n");
    for (int i = 0; i < kNcfg; ++i) {
        printf("   %-14s |", kCfgLong[i]);
        for (int r = 0; r < 4; ++r) {
            if (have[i][r]) printf(" %8.4f", eps[i][r]);
            else            printf(" %8s", "n/a");
        }
        printf("\n");
    }
    printf("\n mean p.e./event (all events):\n");
    for (int i = 0; i < kNcfg; ++i) {
        printf("   %-14s |", kCfgLong[i]);
        for (int r = 0; r < 4; ++r) {
            if (have[i][r]) printf(" %8.2f", peAll[i][r]);
            else            printf(" %8s", "n/a");
        }
        printf("\n");
    }

    printf("\n%% LaTeX body\n");
    for (int i = 0; i < kNcfg; ++i) {
        printf("%s", kCfgLong[i]);
        for (int r = 0; r < 4; ++r) {
            if (have[i][r])
                printf(" & $%.2f$ & $%.3f$", peDet[i][r], eps[i][r]);
            else
                printf(" & n/a & n/a");
        }
        printf(" \\\\\n");
    }

    printf("\nwhole_yield_table: wrote %s/whole_yield.csv\n", outdir);
}
