// plot_light_efficiency.C -- DEPRECATED forwarder.
//
// The per-slab produced/collected H2 maps (h2_optprod_xy_slab<i> /
// h2_optcollected_xy_slab<i>) were removed. The collection efficiency now
// lives in the sipmHits tree (muonX/muonY/nProduced/nDetected) and is plotted
// by macros/plot_collection_efficiency.C. This stub loads and calls it so
// older command lines keep working; prefer calling the new macro directly.
//
// Run from the repo root:
//   root -l -b -q 'plot_light_efficiency.C+("data_output_whole")'

#include <TSystem.h>
#include <TString.h>
#include <TROOT.h>

void plot_light_efficiency(const char* fileName = "data_output_whole",
                           const char* /*outDir*/ = nullptr)
{
    const TString mac = TString(gSystem->pwd()) + "/macros/plot_collection_efficiency.C+";
    gROOT->ProcessLine(TString::Format(".L %s", mac.Data()));
    gROOT->ProcessLine(TString::Format(
        "plot_collection_efficiency(\"%s\", 5.0, \"\", 125.0);", fileName).Data());
}
