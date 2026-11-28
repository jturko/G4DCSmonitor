// run_all_wssa.C
//
// Read every record in the six supplied WSSA files and write full-statistics
// ROOT histograms.  This is the all-events counterpart of the 200k validation
// sample.  It is intentionally sequential: each job streams one file and fills
// the histograms in place, so the memory footprint stays small.
//
// Usage (from the repository root):
//
//   root -l -b -q 'scripts/wssa/run_all_wssa.C+'
//
// Output:
//   scripts/wssa/output_all/<tag>_all.root
//   scripts/wssa/output_all/<tag>_all_*.png / *.pdf
//
// The reader arguments are the same as read_wssa.C:
//   N=0, randomMode=false -> every record; job.particle selects the
//   ICRP 74 H*(10) coefficient table for the dose-weighted map.

#include "read_wssa.C"

#include "TStopwatch.h"

#include <iostream>
#include <string>
#include <vector>

void run_all_wssa(const char* outDir = "scripts/wssa/output_all")
{
    const std::string dataRoot = "/home/turko46/data/wti";
    const std::string outRoot(outDir);

    struct Job {
        const char* tag;
        const char* cask;
        const char* particle;
    };
    const std::vector<Job> jobs = {
        {"Cask1_neutrons", "Cask1", "neutrons"},
        {"Cask1_gamma",    "Cask1", "gamma"},
        {"Cask2_neutrons", "Cask2", "neutrons"},
        {"Cask2_gamma",    "Cask2", "gamma"},
        {"Cask3_neutrons", "Cask3", "neutrons"},
        {"Cask3_gamma",    "Cask3", "gamma"},
    };

    for (const auto& job : jobs) {
        const std::string in = dataRoot + "/" + job.cask + "/" +
                               job.tag + ".wssa";
        const std::string out = outRoot + "/" + job.tag + "_all";

        std::cout << "\n============================================================\n"
                  << "FULL READ: " << job.tag << "\n"
                  << "  input: " << in << "\n"
                  << "  output: " << out << "\n"
                  << "============================================================\n";

        TStopwatch timer;
        timer.Start();
        read_wssa(in.c_str(), out.c_str(),
                  0,      // N=0 -> all records in sequential mode
                  false,  // sequential
                  1,      // phiMode = clockwise from +y
                  true,   // weighted
                  36, 41, 4000.0,
                  120, 1e-12, 30.0,
                  12345,
                  job.particle);
        timer.Stop();
        std::cout << "FULL READ DONE: " << job.tag
                  << "  real=" << timer.RealTime()
                  << " s  cpu=" << timer.CpuTime() << " s\n";
    }
}
