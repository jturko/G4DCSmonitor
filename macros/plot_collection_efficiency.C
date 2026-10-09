// plot_collection_efficiency.C -- light-collection-efficiency maps from the
// MuonScint sipmHits tree.
//
// The simulation writes, per (event, SiPM) row of the `sipmHits` tree:
//   muonX, muonY : primary muon first-entry position (slab-local, mm)
//   nProduced    : optical photons produced in the slab during the event
//   nDetected    : photoelectrons detected in that SiPM during the event
//   det, sipm    : slab index and SiPM index within the slab
// Zero-detection SiPMs are written too, so the tree is an unbiased sample of
// the muon interaction position.
//
// This macro bins (muonX, muonY) with a user-chosen cell size and, per
// (config, reflector), builds:
//   produced   : sum of nProduced per event (denominator)
//   collected  : sum of nDetected per event over the SELECTED SiPMs (numerator)
//   efficiency : collected / produced, bin by bin
//
// The SiPM selection can be:
//   ""            all SiPMs combined (default)
//   "1"           only SiPM 1
//   "0,2,5"       SiPMs 0, 2 and 5 combined
// `det` (slab index) is fixed to 0 for the single-slab geometry.
//
// Usage (batch):
//   root -l -b -q 'plot_collection_efficiency.C+("data_output_whole")'
//   root -l -b -q 'plot_collection_efficiency.C+("data_output_whole", 5.0, "", 125.0)'
//   root -l -b -q 'plot_collection_efficiency.C+("data_output_whole", 5.0, "1")'
//
// Args: dir (default "data_output_whole"), cellSize_mm (5), sipmList (all),
//       halfExtent_mm (125, the half-size of the 250x250 slab).
//
// Author: G4DCSmonitor analysis tooling.

#include <TCanvas.h>
#include <TFile.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TObjArray.h>
#include <TObjString.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TList.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};

// Parse "muon_whole_out_c<C>_r<R>" (extension stripped).
bool parseStem(const TString& stem, int& cfg, int& refl)
{
    TString s = stem;
    s.ReplaceAll("muon_whole_out_", "");
    s.ReplaceAll("muon_out_", "");   // tolerate the matrix naming too
    Ssiz_t p0 = s.Index("c");
    Ssiz_t p1 = s.Index("_r");
    if (p0 == kNPOS || p1 == kNPOS) return false;
    TString cTok = s(p0 + 1, p1 - p0 - 1);
    TString rTok = s(p1 + 2, s.Length());
    cfg  = cTok.Atoi();
    refl = rTok.Atoi();
    return (cfg > 0 && refl >= 0 && refl < 4);
}

std::set<int> parseSiPMList(const char* list)
{
    std::set<int> out;
    if (!list || !*list) return out;   // empty => all
    TString s(list);
    TObjArray* tok = s.Tokenize(",");
    for (int i = 0; i < tok->GetEntries(); ++i) {
        TString t(((TObjString*)tok->At(i))->GetString());
        t.ReplaceAll(" ", "");
        if (!t.IsNull()) out.insert(t.Atoi());
    }
    delete tok;
    return out;
}

struct PerEvent {
    double x = 0., y = 0.;
    int    produced = 0;
    int    collected = 0;
    bool   seen = false;
};

}  // namespace

void plot_collection_efficiency(const char* dir = "data_output_whole",
                                double cellSize = 5.0,
                                const char* sipmList = "",
                                double half = 125.0)
{
    gStyle->SetOptStat(0);

    if (cellSize <= 0.) cellSize = 5.0;
    const int nb = std::max(1, (int)(2. * half / cellSize) + ((int)(2. * half) % (int)cellSize ? 1 : 0));

    const std::set<int> sel = parseSiPMList(sipmList);
    const bool selectAll = sel.empty();

    TSystemDirectory sd(dir, dir);
    TList* files = sd.GetListOfFiles();
    if (!files) { std::cerr << "No such directory: " << dir << "\n"; return; }

    std::vector<TString> fileList;
    for (auto* o : *files) {
        TString fname = o->GetName();
        if (!fname.EndsWith(".root")) continue;
        const bool whole  = fname.BeginsWith("muon_whole_out_");
        const bool matrix = fname.BeginsWith("muon_out_");
        if (!whole && !matrix) continue;
        fileList.emplace_back(TString(dir) + "/" + fname);
    }
    std::sort(fileList.begin(), fileList.end());
    if (fileList.empty()) { std::cerr << "No muon[_whole]_out_*.root in " << dir << "\n"; return; }

    const std::string outDir = std::string(dir) + "/plots";
    gSystem->mkdir(outDir.c_str(), kTRUE);

    const TString selTag = selectAll ? "all" : TString(sipmList).ReplaceAll(",", "_");

    // cfg -> refl -> efficiency map
    std::map<int, std::map<int, TH2D*>> effMap;

    for (const auto& full : fileList) {
        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { std::cerr << "Cannot open " << full << "\n"; delete f; continue; }

        TTree* tree = (TTree*)f->Get("sipmHits");
        if (!tree) { std::cerr << "No sipmHits tree in " << full << "\n"; delete f; continue; }

        int cfg = 0, refl = -1;
        if (!parseStem(gSystem->BaseName(full), cfg, refl)) {
            std::cerr << "Cannot parse " << full << "; skipping\n";
            delete f;
            continue;
        }

        int    evt = 0, det = 0, sipm = 0, nDet = 0, nProd = 0;
        double mx = 0., my = 0.;
        tree->SetBranchAddress("evtNb",     &evt);
        tree->SetBranchAddress("det",       &det);
        tree->SetBranchAddress("sipm",      &sipm);
        tree->SetBranchAddress("nDetected", &nDet);
        tree->SetBranchAddress("muonX",     &mx);
        tree->SetBranchAddress("muonY",     &my);
        tree->SetBranchAddress("nProduced", &nProd);

        // Aggregate the (event, SiPM) rows into one entry per event: produced
        // is the event total (identical on every row), collected is the sum
        // over the selected SiPMs.
        std::map<int, PerEvent> perEvent;
        for (long i = 0; i < tree->GetEntries(); ++i) {
            tree->GetEntry(i);
            if (det != 0) continue;                       // single-slab geometry
            if (!selectAll && sel.find(sipm) == sel.end()) continue;

            PerEvent& pe = perEvent[evt];
            pe.seen     = true;
            pe.x        = mx;
            pe.y        = my;
            pe.produced = nProd;                          // same on all rows
            pe.collected += nDet;
        }

        TH2D* prod = new TH2D(TString::Format("h_prod_c%d_r%d_%s", cfg, refl, selTag.Data()),
                              TString::Format("produced #gamma, cfg%d %s (SiPM %s);x [mm];y [mm]",
                                              cfg, kReflName[refl], selTag.Data()),
                              nb, -half, half, nb, -half, half);
        TH2D* coll = new TH2D(TString::Format("h_coll_c%d_r%d_%s", cfg, refl, selTag.Data()),
                              TString::Format("collected p.e., cfg%d %s (SiPM %s);x [mm];y [mm]",
                                              cfg, kReflName[refl], selTag.Data()),
                              nb, -half, half, nb, -half, half);

        for (const auto& kv : perEvent) {
            const PerEvent& pe = kv.second;
            if (!pe.seen) continue;
            prod->Fill(pe.x, pe.y, pe.produced);
            coll->Fill(pe.x, pe.y, pe.collected);
        }

        TH2D* eff = (TH2D*)coll->Clone(TString::Format("h_eff_c%d_r%d_%s", cfg, refl, selTag.Data()));
        eff->SetDirectory(nullptr);
        eff->Divide(prod);
        eff->SetTitle(TString::Format("collection efficiency, cfg%d %s (SiPM %s);x [mm];y [mm]",
                                      cfg, kReflName[refl], selTag.Data()));
        eff->SetMinimum(0.);
        eff->SetMaximum(1.);
        eff->GetZaxis()->SetTitle("N_{pe} / N_{#gamma}");

        effMap[cfg][refl] = eff;

        const double totalProd = prod->Integral();
        const double totalColl = coll->Integral();
        const double overall = totalProd > 0. ? totalColl / totalProd : 0.;

        // Per-(cfg,refl) three-panel figure.
        TCanvas* c = new TCanvas(TString::Format("c_eff_c%d_r%d_%s", cfg, refl, selTag.Data()),
                                 "Light collection efficiency", 1500, 480);
        c->Divide(3, 1);
        c->cd(1); prod->Draw("COLZ");
        c->cd(2); coll->Draw("COLZ");
        c->cd(3); eff->Draw("COLZ");
        c->SaveAs(TString::Format("%s/light_eff_c%d_r%d_%s.png", outDir.c_str(), cfg, refl, selTag.Data()));
        c->SaveAs(TString::Format("%s/light_eff_c%d_r%d_%s.pdf", outDir.c_str(), cfg, refl, selTag.Data()));

        printf("cfg%d %-6s  SiPM %-7s  events=%zu  sum prod=%.4g  sum coll=%.4g  overall eff=%.5f\n",
               cfg, kReflName[refl], selTag.Data(), perEvent.size(),
               totalProd, totalColl, overall);

        delete f;
    }

    // One efficiency-grid canvas per config: 4 pads, one per reflector.
    std::ofstream csv(outDir + "/collection_efficiency_summary.csv");
    csv << "config,reflector,sipm_selection,overall_efficiency\n";
    for (auto& ckv : effMap) {
        const int cfg = ckv.first;

        TCanvas* cv = new TCanvas(TString::Format("c_effgrid_c%d_%s", cfg, selTag.Data()),
                                  TString::Format("efficiency, config %d (SiPM %s)", cfg, selTag.Data()),
                                  1600, 420);
        cv->Divide(4, 1);
        for (int r = 0; r < 4; ++r) {
            cv->cd(r + 1);
            auto it = ckv.second.find(r);
            if (it == ckv.second.end() || !it->second) {
                gPad->DrawFrame(-half, -half, half, half,
                                TString::Format("%s (no data)", kReflName[r]));
                continue;
            }
            it->second->Draw("COLZ");
        }
        cv->SaveAs(TString::Format("%s/efficiency_grid_cfg%d_%s.png", outDir.c_str(), cfg, selTag.Data()));
        cv->SaveAs(TString::Format("%s/efficiency_grid_cfg%d_%s.pdf", outDir.c_str(), cfg, selTag.Data()));

        for (int r = 0; r < 4; ++r) {
            auto it = ckv.second.find(r);
            if (it == ckv.second.end() || !it->second) continue;
            csv << cfg << "," << kReflName[r] << ","
                << (selectAll ? "all" : sipmList) << ","
                << it->second->GetMean() << "\n";   // bin-mean of the efficiency map
        }
    }
    csv.close();

    std::cout << "Plots written to " << outDir << "/  (SiPM selection: "
              << (selectAll ? "all" : sipmList) << ", " << nb << "x" << nb
              << " bins of " << cellSize << " mm)\n";
}
