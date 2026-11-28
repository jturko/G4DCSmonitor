// Re-run the three gamma files with the linear 20 keV hE_linear histogram.
// The neutron files keep the log-spaced hE and do not need hE_linear.
#include "read_wssa.C"
#include "TStopwatch.h"
#include <iostream>
#include <string>
#include <vector>

void run_gamma_all(const char* outDir = "scripts/wssa/output_all")
{
    const std::string dataRoot = "/home/turko46/data/wti";
    const std::string outRoot(outDir);
    const std::vector<std::pair<std::string, std::string>> jobs = {
        {"Cask1_gamma", "Cask1"},
        {"Cask2_gamma", "Cask2"},
        {"Cask3_gamma", "Cask3"},
    };
    for (const auto& job : jobs) {
        const std::string in = dataRoot + "/" + job.second + "/" +
                               job.first + ".wssa";
        const std::string out = outRoot + "/" + job.first + "_all";
        std::cout << "\n=== GAMMA FULL READ: " << job.first << " ===\n";
        TStopwatch timer;
        timer.Start();
        read_wssa(in.c_str(), out.c_str(), 0, false, 1, true,
                  36, 41, 4000.0, 120, 1e-12, 30.0, 12345, "gamma");
        timer.Stop();
        std::cout << "DONE: " << job.first << " real=" << timer.RealTime()
                  << " s\n";
    }
}
