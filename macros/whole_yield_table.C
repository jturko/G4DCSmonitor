// whole_yield_table.C -- mean photoelectron yield per event and per *detected*
// event, for the whole-plate (full-slab) irradiations, for every layout
// (config 1-4) and reflector treatment (bare / TiO2 / Al / glossy).
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
// Writes <outdir>/whole_yield.csv and prints a LaTeX body.
//
// Usage:
//   root -l -b -q 'macros/whole_yield_table.C("slurm/data_v2.2/whole","report/figures")'

#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <TSystem.h>

#include <cstdio>
#include <map>
#include <vector>

namespace {
const char* kCfg[5]  = {"", "1", "2", "3", "4"};
const char* kRefl[4] = {"bare", "TiO2", "Al", "glossy"};
}  // namespace

void whole_yield_table(const char* dir = "slurm/data_v2.2/whole",
                       const char* outdir = "report/figures")
{
    gSystem->mkdir(outdir, kTRUE);

    double peDet[5][4], peAll[5][4], eps[5][4];
    double nEvt[5][4], nDet[5][4];

    for (int c = 1; c <= 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            const TString fname =
                TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, c, r);
            double N = 0., Nd = 0., tot = 0.;
            TFile* f = TFile::Open(fname, "READ");
            if (f && !f->IsZombie()) {
                TTree* t = (TTree*)f->Get("sipmHits");
                if (t && t->GetBranch("nDetected")) {
                    int evt = 0, nd = 0;
                    t->SetBranchAddress("evtNb", &evt);
                    t->SetBranchAddress("nDetected", &nd);
                    double cur = -1., sum = 0.;
                    for (long i = 0; i < t->GetEntries(); ++i) {
                        t->GetEntry(i);
                        if ((double)evt != cur) {
                            if (cur >= 0.) { ++N; if (sum > 0.) ++Nd; tot += sum; }
                            cur = evt; sum = 0.;
                        }
                        sum += nd;
                    }
                    if (cur >= 0.) { ++N; if (sum > 0.) ++Nd; tot += sum; }
                }
                delete f;
            }
            nEvt[c][r] = N;
            nDet[c][r] = Nd;
            eps[c][r]   = (N > 0.) ? Nd / N : 0.;
            peAll[c][r] = (N > 0.) ? tot / N : 0.;
            peDet[c][r] = (Nd > 0.) ? tot / Nd : 0.;
        }
    }

    // CSV
    FILE* fp = fopen(TString::Format("%s/whole_yield.csv", outdir), "w");
    if (fp) {
        fprintf(fp, "config,wrapper,N,Ndet,eps,pe_all,pe_detected\n");
        for (int c = 1; c <= 4; ++c)
            for (int r = 0; r < 4; ++r)
                fprintf(fp, "%d,%s,%.0f,%.0f,%.4f,%.3f,%.3f\n",
                        c, kRefl[r], nEvt[c][r], nDet[c][r],
                        eps[c][r], peAll[c][r], peDet[c][r]);
        fclose(fp);
    }

    // Console summary
    printf("\n config |");
    for (int r = 0; r < 4; ++r) printf("   %-6s", kRefl[r]);
    printf("   (mean p.e./detected event)\n");
    for (int c = 1; c <= 4; ++c) {
        printf("   %s    |", kCfg[c]);
        for (int r = 0; r < 4; ++r) printf(" %8.2f", peDet[c][r]);
        printf("\n");
    }
    printf("\n detection probability eps:\n");
    for (int c = 1; c <= 4; ++c) {
        printf("   %s    |", kCfg[c]);
        for (int r = 0; r < 4; ++r) printf(" %8.4f", eps[c][r]);
        printf("\n");
    }
    printf("\n mean p.e./event (all events):\n");
    for (int c = 1; c <= 4; ++c) {
        printf("   %s    |", kCfg[c]);
        for (int r = 0; r < 4; ++r) printf(" %8.2f", peAll[c][r]);
        printf("\n");
    }

    printf("\n%% LaTeX body\n");
    for (int c = 1; c <= 4; ++c) {
        printf("%s", kCfg[c]);
        for (int r = 0; r < 4; ++r)
            printf(" & $%.2f$ & $%.3f$", peDet[c][r], eps[c][r]);
        printf(" \\\\\n");
    }

    printf("\nwhole_yield_table: wrote %s/whole_yield.csv\n", outdir);
}
