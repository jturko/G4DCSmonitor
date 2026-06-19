// make_assembly_spectra.C
//
// Per-assembly edep spectra. For each global fuel assembly G in 0..83:
//   * Sum the 5 per-(G, source) edep TH1Ds into a black "total" histogram.
//   * Overlay the 5 sources in distinct colours.
//   * Annotate which sources (if any) were missing for that assembly.
//
// Reads the histograms produced by make_assembly_heatmaps.C from the
// 'edep_per_assembly/' sub-directory of its output ROOT file.
//
// Outputs:
//   assembly_spectra.root   -- 84 TCanvases + per-assembly TH1D sums
//   assembly_spectra.pdf    -- multi-page PDF, one canvas per assembly
//
// Usage:
//   root -l -b -q 'make_assembly_spectra.C("assembly_heatmaps.root",
//                                          "assembly_spectra.root",
//                                          "assembly_spectra.pdf")'

#include "style.h"

#include <TCanvas.h>
#include <TDirectory.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TROOT.h>

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

//constexpr int kNAssem = 84;

struct SrcStyle {
    const char* tag;
    Color_t     color;
    Style_t     line;
};

// Distinct, print-friendly colours. The sum is drawn separately in kBlack.
const std::vector<SrcStyle> kSources = {
    { "gamma_662keV",   kRed     + 1, 1 },
    { "gamma_1173keV",  kGreen   + 2, 1 },
    { "gamma_1332keV",  kAzure   + 1, 1 },
    { "isotope_Eu154",  kMagenta + 1, 1 },
    { "neutron_Watt",   kOrange  + 7, 1 },
    //{ "gamma_662keV",  PetroffPalette::getP6(0) , 1 },
    //{ "gamma_1173keV", PetroffPalette::getP6(1) , 1 },
    //{ "gamma_1332keV", PetroffPalette::getP6(2) , 1 },
    //{ "isotope_Eu154", PetroffPalette::getP6(3) , 1 },
    //{ "neutron_Watt",  PetroffPalette::getP6(4) , 1 },
};

// Pretty label for legends (LaTeX-friendly).
const char* PrettyLabel(const std::string& tag) {
    if (tag == "gamma_662keV")   return "#gamma 662 keV (^{137}Cs)";
    if (tag == "gamma_1173keV")  return "#gamma 1173 keV (^{60}Co)";
    if (tag == "gamma_1332keV")  return "#gamma 1332 keV (^{60}Co)";
    if (tag == "isotope_Eu154")  return "^{154}Eu";
    if (tag == "neutron_Watt")   return "neutron (Watt)";
    return tag.c_str();
}

} // anonymous

#include "geometry_constraints.h"
using namespace geo;


void make_assembly_spectra(const char* inFile   = "assembly_heatmaps.root",
                           const char* outFile  = "assembly_spectra.root",
                           const char* pdfFile  = "assembly_spectra.pdf")
{
    gStyle->SetOptStat(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetLegendFillColor(0);

    std::unique_ptr<TFile> fin(TFile::Open(inFile, "READ"));
    if (!fin || fin->IsZombie()) {
        std::cerr << "[err] cannot open " << inFile << std::endl;
        return;
    }
    auto* edepDir = (TDirectory*)fin->Get("edep_per_assembly");
    if (!edepDir) {
        std::cerr << "[err] missing 'edep_per_assembly/' in " << inFile
                  << "  (did make_assembly_heatmaps.C run successfully?)"
                  << std::endl;
        return;
    }

    TFile* fout = TFile::Open(outFile, "RECREATE");
    if (!fout || fout->IsZombie()) {
        std::cerr << "[err] cannot open " << outFile << std::endl;
        return;
    }
    auto* sumDir = fout->mkdir("edep_sum_per_assembly");

    // Open the multi-page PDF.
    //auto* cInit = new TCanvas("c_init", "", 10, 10);  // suppressor canvas
    //cInit->Print(Form("%s[", pdfFile));               // opens the PDF
    //delete cInit;

    long long nMissingTotal = 0;
    TH1D* hSumAllAssemblies = nullptr;
    std::vector<TH1D*> hSumAllAssembliesSrc(kSources.size(), nullptr);
    for (int g = 0; g < kNAssem; ++g) {

        // -- Pull the 5 per-source histograms (may be missing for some sources)
        std::vector<TH1D*> hSrc(kSources.size(), nullptr);
        std::vector<bool>  missing(kSources.size(), false);

        for (size_t si = 0; si < kSources.size(); ++si) {
            char hname[128];
            std::snprintf(hname, sizeof(hname),
                          "h1_edep_globalFuel%02d_%s", g, kSources[si].tag);
            auto* h = (TH1D*)edepDir->Get(hname);
            if (!h) {
                missing[si] = true;
                ++nMissingTotal;
                continue;
            }
            // Detach from input file so we can keep using it after closing.
            h = (TH1D*)h->Clone();
            h->SetDirectory(nullptr);
            h->SetLineColor(kSources[si].color);
            h->SetLineWidth(1);
            h->SetLineStyle(kSources[si].line);
            hSrc[si] = h;
            
            // summed histograms across all assemblies
            // if we got here, we found a valid source assembly combo
            if(!hSumAllAssemblies) {
                hSumAllAssemblies = (TH1D*)h->Clone("hSumAllAssemblies");
                hSumAllAssemblies->SetLineColor(kBlack);
                hSumAllAssemblies->SetTitle(Form("edep, sum signal over all (assemblies,sources)"));
            }
            if(!hSumAllAssembliesSrc[si]) {
                hSumAllAssembliesSrc[si] = (TH1D*)h->Clone(Form("hSumAllAssemblies_%s", kSources[si].tag));
            }
            hSumAllAssemblies->Add(h);
            hSumAllAssembliesSrc[si]->Add(h);
        }

        // -- Find a non-null source to seed binning for the sum.
        TH1D* hSum = nullptr;
        for (auto* h : hSrc) {
            if (h) { 
                hSum = (TH1D*)h->Clone(); 
                break; 
            }
        }
        if (!hSum) {
            // Entire assembly has no data; emit a blank page so PDF page
            // numbering still matches the global fuel index.
            auto* c = new TCanvas(Form("c_spectra_globalFuel%02d", g),
                                  Form("globalFuel %d -- no data", g),
                                  1100, 750);
            auto* tex = new TLatex();
            tex->SetNDC(true);
            tex->SetTextAlign(22);
            tex->SetTextSize(0.05);
            tex->DrawLatex(0.5, 0.55, Form("globalFuel %d", g));
            tex->SetTextSize(0.035);
            tex->SetTextColor(kGray + 2);
            tex->DrawLatex(0.5, 0.45, "all 5 source files missing");
            //c->Print(pdfFile);
            c->Write();
            delete c;
            continue;
        }

        char sname[128];
        std::snprintf(sname, sizeof(sname),
                      "h1_edep_sum_globalFuel%02d", g);
        hSum->SetName(sname);
        hSum->SetTitle(Form("edep, globalFuel = %d;edep [MeV];counts (weighted)", g));
        hSum->SetDirectory(sumDir);
        hSum->Reset();
        for (auto* h : hSrc) if (h) hSum->Add(h);
        hSum->SetLineColor(kBlack);
        hSum->SetLineWidth(1);

        // -- Draw on one canvas.
        auto* c = new TCanvas(Form("c_spectra_globalFuel%02d", g),
                              Form("edep spectra, globalFuel = %d", g),
                              1100, 750);
        c->SetLogy();
        c->SetGrid();
        c->SetLeftMargin (0.10);
        c->SetRightMargin(0.04);
        c->SetTopMargin  (0.08);

        // Range: anchor to the sum, with a small headroom on log scale.
        double ymin = std::max(0.5, 0.5 * hSum->GetMinimum(1e-9));
        double ymax = 5.0 * hSum->GetMaximum();
        if (!(ymax > ymin)) { ymin = 0.5; ymax = 10.0; }
        hSum->SetMinimum(ymin);
        hSum->SetMaximum(ymax);

        hSum->Draw("HIST");
        for (auto* h : hSrc) if (h) h->Draw("HIST SAME");
        hSum->Draw("HIST SAME");   // re-draw on top so black is visible

        // -- Legend.
        auto* leg = new TLegend(0.62, 0.62, 0.96, 0.90);
        leg->SetTextSize(0.030);
        leg->AddEntry(hSum, "sum of all sources", "l");
        for (size_t si = 0; si < kSources.size(); ++si) {
            const char* lbl = PrettyLabel(kSources[si].tag);
            if (hSrc[si]) {
                leg->AddEntry(hSrc[si],
                              Form("%s  (#scale[0.8]{%.3g cts})",
                                   lbl, hSrc[si]->Integral()),
                              "l");
            } else {
                // Show the source in the legend with a "missing" tag, so the
                // viewer can tell apart "absent from plot because zero" vs
                // "absent because the file is missing".
                auto* dummy = new TH1D(Form("h1_missing_%02d_%zu", g, si),
                                       "", 1, 0, 1);
                dummy->SetLineColor(kSources[si].color);
                dummy->SetLineWidth(1);
                dummy->SetLineStyle(2);   // dashed to differentiate
                leg->AddEntry(dummy,
                              Form("%s  [missing file]", lbl), "l");
            }
        }
        leg->Draw();

        // -- Header text.
        auto* tex = new TLatex();
        tex->SetNDC(true);
        tex->SetTextSize(0.035);
        tex->DrawLatex(0.10, 0.945,
            Form("globalFuel = %d   (#scale[0.85]{total integral = %.3g})",
                 g, hSum->Integral()));

        // Count missing sources for inline diagnostic.
        size_t nMiss = 0;
        for (bool m : missing) if (m) ++nMiss;
        if (nMiss > 0) {
            tex->SetTextSize(0.028);
            tex->SetTextColor(kGray + 2);
            tex->DrawLatex(0.10, 0.91,
                Form("missing: %zu / %zu source files", nMiss, kSources.size()));
        }

        //c->Print(pdfFile);
        c->Write();
        delete c;
    }

    auto cSumAllAssemblies = new TCanvas("c_sumAllAssemblies", 
                                         "edep spectra, all assemblies",
                                         1100, 750);
    hSumAllAssemblies->Draw();
    for (size_t si = 0; si < kSources.size(); ++si) {
        hSumAllAssembliesSrc[si]->Draw("same");
    }
    cSumAllAssemblies->Write();
    delete cSumAllAssemblies;

    // Close the multi-page PDF.
    auto* cEnd = new TCanvas("c_end", "", 10, 10);
    //cEnd->Print(Form("%s]", pdfFile));
    delete cEnd;

    //hSumAllAssemblies->Write();
    //for (size_t si = 0; si < kSources.size(); ++si) {
    //    hSumAllAssembliesSrc[si]->Write();
    //}

    fout->Write();
    fout->Close();

    std::cout << "[ok] wrote " << outFile
              << " and " << pdfFile
              << " (" << nMissingTotal << " missing (fuel, source) entries across 84 assemblies)"
              << std::endl;
}

