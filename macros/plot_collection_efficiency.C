// plot_collection_efficiency.C -- light-collection-efficiency maps from the
// MuonScint sipmHits tree.
//
// ORIENTATION: the report presents the plate in the paper's plan view, with
//   top = +y, bottom = -y, left = -x, right = +x.
// The internal simulation frame is this view rotated 90 deg clockwise
// (x_sim, y_sim) = (-y_report, x_report), so both the entry points and the SiPM
// markers are mapped through (x,y) -> (y,-x) before plotting -- for ALL configs,
// including config 4.  Config 3 (both SiPMs on one face) therefore appears on
// the top (+y) edge, and the edge/corner beams on the bottom (-y) edge, matching
// Fig. 3 of Rao et al.  Configs 1 and 2 are four-fold symmetric, so the rotation
// only moves their irradiation sites, not the sensor markers.  The config-4 WLS
// clamp sits on the -x narrow face with its read-out at slab-local -y, so it too
// lands with its bar on the top edge and its single SiPM at the top-left.
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

#include <TBox.h>
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
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions).  Located via
// ROOT_INCLUDE_PATH (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

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

// SiPM centre positions for the three presets, in the REPORT (x,y) frame, for a
// 250x250 slab (half-extent 125 mm).  The internal simulation layout is rotated
// 90 deg clockwise into this frame (see the orientation note at the top), so the
// shapes here are the report-frame layouts: config 1 = 2/edge, config 2 = 1/edge
// at the config-1 offsets, config 3 = 2 SiPMs on the top (+y) edge.
// The SiPM active area is 4x4 mm (fSiPMHalfX = fSiPMHalfY = 2 mm); when a
// sensor is mounted on a side face its in-plane footprint is 4 mm along the
// edge and (visually, looking down the beam) zero-thickness, so on the x-y map
// it appears as a small square straddling that edge.
struct SiPMTile { double x, y; };
std::vector<SiPMTile> sipmTiles(int cfg, double half)
{
    std::vector<SiPMTile> t;
    const double H = half;       // slab half-extent [mm]
    const double u = 0.5 * H;    // +/-50% of the half-length = +/-62.5 mm
    switch (cfg) {
        case 1:  // 8 SiPMs: two per side
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        case 2:  // 4 SiPMs: one per side at the cfg1 offsets (cfg1 minus one/side)
            t = {{ H, -u}, {-H, u}, { u, H}, {-u, -H}};
            break;
        case 3:  // 2 SiPMs: both on the top +y edge
            t = {{-u, H}, { u, H}};
            break;
        case 4:  // single read-out SiPM at the -y end of the -x WLS fibre clamp
            t = {{-H, H}};
            break;
    }
    return t;
}

// Map a primary-muon entry point from the internal simulation frame into the
// report frame: a 90 deg clockwise rotation, (x,y) -> (y,-x) (report frame:
// top=+y, left=-x).  This applies to ALL configs, including config 4: the WLS
// clamp is placed on the -x narrow face with its read-out at slab-local -y, so
// under this same transform its bar lands on the top edge and its SiPM at the
// top-left, matching the marker convention in sipmTiles() above.
inline void toReportFrame(double& x, double& y)
{
    const double xr = y, yr = -x;
    x = xr; y = yr;
}

// Overlay the SiPM positions on a 2D map as filled black squares.  The sensors
// are physically mounted on the side faces, i.e. right on the slab boundary at
// x = +/-half or y = +/-half.  On the map this would overlap the outermost data
// bins, so the markers are pushed just *outside* the active area (inner edge at
// half+gap) purely for legibility; they still mark the correct edge and the
// correct position along it.  `side` is the drawn square side in mm.
void drawSiPMSquares(int cfg, double half, double side = 8.0, double gap = 2.0)
{
    if (cfg == 4) {
        // Config 4 is a WLS fibre clamp rather than discrete SiPMs: draw the bar
        // running the full length of the top (+y) edge, outboard of the slab,
        // with the single read-out SiPM (black) butt-coupled at its left (-x)
        // end, so the fibres can be located relative to the read-out.  Same
        // convention as Fig. 1 (sipm_layouts) and Fig. 3(b) of Rao et al.
        const double y0 = half + gap;
        const double y1 = half + gap + side;
        TBox* bar = new TBox(-half, y0, half, y1);
        bar->SetFillColor(kRed);
        bar->SetFillStyle(1001);
        bar->SetLineColor(kBlack);
        bar->SetLineWidth(1);
        bar->Draw("SAME");
        TBox* sipm = new TBox(-half - side, y0, -half, y1);
        sipm->SetFillColor(kBlack);
        sipm->SetFillStyle(1001);
        sipm->SetLineColor(kBlack);
        sipm->SetLineWidth(1);
        sipm->Draw("SAME");
        return;
    }

    const std::vector<SiPMTile> tiles = sipmTiles(cfg, half);
    const double off = gap + 0.5 * side;   // shift from the boundary to the centre
    const double tol = 1e-6;
    for (const auto& t : tiles) {
        double cx = t.x, cy = t.y;
        if      (std::fabs(t.x - half) < tol) cx =  half + off;
        else if (std::fabs(t.x + half) < tol) cx = -half - off;
        if      (std::fabs(t.y - half) < tol) cy =  half + off;
        else if (std::fabs(t.y + half) < tol) cy = -half - off;
        TBox* b = new TBox(cx - 0.5 * side, cy - 0.5 * side,
                           cx + 0.5 * side, cy + 0.5 * side);
        b->SetFillColor(kBlack);
        b->SetFillStyle(1001);
        b->SetLineColor(kBlack);
        b->SetLineWidth(1);
        b->Draw("SAME");
    }
}

// Draw a bold, unfilled rectangle marking the physical slab active area
// [-half,half] x [-half,half] mm, to anchor the zoomed-out view.
void drawSlabOutline(double half)
{
    TBox* slab = new TBox(-half, -half, half, half);
    slab->SetFillStyle(0);
    slab->SetLineColor(kBlack);
    slab->SetLineWidth(2);
    slab->Draw("SAME");
}

// Bin edges aligned to the slab: an integer number of equal cells spans exactly
// [-half, half] so no cell straddles the slab boundary; the two outermost
// (partial) bins merely pad the view to +/-span for the outside SiPM markers.
std::vector<double> makeEdges(double span, double half, double cellSize)
{
    const int nIn = std::max(1, (int)std::lround(2.0 * half / cellSize));
    const double step = 2.0 * half / nIn;
    std::vector<double> e;
    e.reserve(nIn + 3);
    e.push_back(-span);
    for (int k = 0; k <= nIn; ++k) e.push_back(-half + k * step);
    e.push_back(span);
    return e;
}

}  // namespace

void plot_collection_efficiency(const char* dir = "data_output_whole",
                                double cellSize = 5.0,
                                const char* sipmList = "",
                                double half = 125.0)
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);        // style() hides titles; the panels are labelled by reflector
    gStyle->SetLegendBorderSize(0);

    if (cellSize <= 0.) cellSize = 5.0;
    // The 50x50 mm GPS beam is placed fully on the slab (centre, bottom-edge and
    // bottom-right patches all lie inside [-half,half] in the report frame), and
    // the whole-slab run irradiates the full plate, so the entry points span
    // [-half,half].
    // Zoom out enough to show the bold slab outline AND the SiPM markers, which
    // are drawn just outside the boundary (see drawSiPMSquares) so they never
    // overlap the data.
    const double margin = 18.0;
    const double span   = half + margin;
    const std::vector<double> edges = makeEdges(span, half, cellSize);
    const int nb = (int)edges.size() - 1;

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

    // cfg -> refl -> accumulated histograms.  Each (config, reflector) is written
    // as THREE files (centre/edge/corner); accumulate them before building the
    // efficiency map and the overall totals, otherwise only the last file counts.
    std::map<int, std::map<int, TH2D*>> prodMap;
    std::map<int, std::map<int, TH2D*>> collMap;

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
            toReportFrame(pe.x, pe.y);                    // sim frame -> report frame
            pe.produced = nProd;                          // same on all rows
            pe.collected += nDet;
        }

        if (!prodMap[cfg].count(refl)) {
            prodMap[cfg][refl] = new TH2D(TString::Format("h_prod_c%d_r%d_%s", cfg, refl, selTag.Data()),
                                           TString::Format("produced #gamma, cfg%d %s (SiPM %s);x [mm];y [mm]",
                                                           cfg, kReflName[refl], selTag.Data()),
                                           nb, edges.data(), nb, edges.data());
            collMap[cfg][refl] = new TH2D(TString::Format("h_coll_c%d_r%d_%s", cfg, refl, selTag.Data()),
                                           TString::Format("collected p.e., cfg%d %s (SiPM %s);x [mm];y [mm]",
                                                           cfg, kReflName[refl], selTag.Data()),
                                           nb, edges.data(), nb, edges.data());
            // These outlive the TFile they are created under; detach them so
            // delete f does not destroy them.
            prodMap[cfg][refl]->SetDirectory(nullptr);
            collMap[cfg][refl]->SetDirectory(nullptr);
        }
        TH2D* prod = prodMap[cfg][refl];
        TH2D* coll = collMap[cfg][refl];

        for (const auto& kv : perEvent) {
            const PerEvent& pe = kv.second;
            if (!pe.seen) continue;
            prod->Fill(pe.x, pe.y, pe.produced);
            coll->Fill(pe.x, pe.y, pe.collected);
        }

        printf("cfg%d %-6s  SiPM %-7s  %-6s  events=%zu  sum prod=%.4g  sum coll=%.4g\n",
               cfg, kReflName[refl], selTag.Data(), gSystem->BaseName(full),
               perEvent.size(), prod->Integral(), coll->Integral());

        delete f;
    }

    // Display range: the slab plus the beam footprint, so the marker of the
    // physical 250x250 plate is visible inside the wider field.
    const double xLo = -span;
    const double xHi =  span;
    const double yLo = -span;
    const double yHi =  span;
    auto styleFrame = [](TH2D* h) {
        h->GetXaxis()->SetTitleSize(0.045);
        h->GetYaxis()->SetTitleSize(0.045);
        h->GetZaxis()->SetTitleSize(0.042);
        h->GetXaxis()->SetLabelSize(0.040);
        h->GetYaxis()->SetLabelSize(0.040);
        h->GetZaxis()->SetLabelSize(0.040);
        h->GetXaxis()->SetTitleOffset(1.00);
        h->GetYaxis()->SetTitleOffset(1.05);
        h->GetZaxis()->SetTitleOffset(1.35);
        h->GetZaxis()->SetMaxDigits(3);
    };
    auto stylePad = [](TVirtualPad* p) {
        p->SetLeftMargin(0.13);
        p->SetRightMargin(0.16);
        p->SetBottomMargin(0.15);
        p->SetTopMargin(0.10);
    };

    // ---- pass 1: build the efficiency maps and the totals ----------------
    std::map<int, std::map<int, TH2D*>> effMap;
    // cfg -> refl -> {sum produced, sum collected} for the true overall efficiency
    std::map<int, std::map<int, std::pair<double, double>>> effTotals;
    double globalEffMax = 0.;

    for (auto& ckv : prodMap) {
        const int cfg = ckv.first;
        for (auto& rkv : ckv.second) {
            const int refl = rkv.first;
            TH2D* prod = rkv.second;
            TH2D* coll = collMap[cfg][refl];

            TH2D* eff = (TH2D*)coll->Clone(TString::Format("h_eff_c%d_r%d_%s", cfg, refl, selTag.Data()));
            eff->SetDirectory(nullptr);
            eff->Divide(prod);
            eff->SetTitle(TString::Format("%s (cfg %d);x [mm];y [mm]",
                                          kReflName[refl], cfg));
            eff->SetMinimum(0.);
            eff->GetZaxis()->SetTitle("#varepsilon(x,y) = N_{#font[42]{pe}}/N_{#gamma}");
            styleFrame(eff);
            effMap[cfg][refl] = eff;

            // Largest populated bin drives the shared z-range.
            double mx = eff->GetBinContent(eff->GetMaximumBin());
            if (mx > globalEffMax) globalEffMax = mx;

            const double totalProd = prod->Integral();
            const double totalColl = coll->Integral();
            const double overall = totalProd > 0. ? totalColl / totalProd : 0.;
            effTotals[cfg][refl] = {totalProd, totalColl};

            printf("cfg%d %-6s  SiPM %-7s  ALL-POS  sum prod=%.4g  sum coll=%.4g  overall eff=%.5f\n",
                   cfg, kReflName[refl], selTag.Data(), totalProd, totalColl, overall);
        }
    }
    if (globalEffMax <= 0.) globalEffMax = 1.;
    const double zMax = globalEffMax * 1.05;
    std::cout << "Shared efficiency z-range: [0, " << zMax << "]\n";

    // ---- pass 2: draw per-(cfg,reflector) produced/collected/efficiency ---
    for (auto& ckv : effMap) {
        const int cfg = ckv.first;
        for (auto& rkv : ckv.second) {
            const int refl = rkv.first;
            TH2D* prod = prodMap[cfg][refl];
            TH2D* coll = collMap[cfg][refl];
            TH2D* eff  = rkv.second;
            eff->SetMaximum(zMax);

            TCanvas* c = new TCanvas(TString::Format("c_eff_c%d_r%d_%s", cfg, refl, selTag.Data()),
                                     "Light collection efficiency", 1500, 560);
            c->Divide(3, 1);
            c->cd(1); stylePad(gPad); styleFrame(prod); prod->Draw("COLZ"); drawSlabOutline(half); drawSiPMSquares(cfg, half, 8.0);
            c->cd(2); stylePad(gPad); styleFrame(coll); coll->Draw("COLZ"); drawSlabOutline(half); drawSiPMSquares(cfg, half, 8.0);
            c->cd(3); stylePad(gPad); styleFrame(eff);  eff->Draw("COLZ"); drawSlabOutline(half); drawSiPMSquares(cfg, half, 8.0);
            c->SaveAs(TString::Format("%s/light_eff_c%d_r%d_%s.png", outDir.c_str(), cfg, refl, selTag.Data()));
            c->SaveAs(TString::Format("%s/light_eff_c%d_r%d_%s.pdf", outDir.c_str(), cfg, refl, selTag.Data()));
        }
    }

    // ---- pass 3: one 2x2 efficiency-grid canvas per config ----------------
    std::ofstream csv(outDir + "/collection_efficiency_summary.csv");
    csv << "config,reflector,sipm_selection,overall_efficiency\n";
    for (auto& ckv : effMap) {
        const int cfg = ckv.first;

        TCanvas* cv = new TCanvas(TString::Format("c_effgrid_c%d_%s", cfg, selTag.Data()),
                                  TString::Format("collection efficiency, config %d (SiPM %s);", cfg, selTag.Data()),
                                  1100, 1000);
        cv->Divide(2, 2, 0.005, 0.005);
        for (int r = 0; r < 4; ++r) {
            cv->cd(r + 1);
            stylePad(gPad);
            auto it = ckv.second.find(r);
            if (it == ckv.second.end() || !it->second) {
                gPad->DrawFrame(xLo, yLo, xHi, yHi,
                                TString::Format("%s (no data);x [mm];y [mm]", kReflName[r]));
                continue;
            }
            TH2D* eff = it->second;
            eff->SetMaximum(zMax);
            eff->GetXaxis()->SetRangeUser(xLo, xHi);
            eff->GetYaxis()->SetRangeUser(yLo, yHi);
            eff->Draw("COLZ");
            drawSlabOutline(half);
            drawSiPMSquares(cfg, half, 8.0);
        }
        cv->SaveAs(TString::Format("%s/efficiency_grid_cfg%d_%s.png", outDir.c_str(), cfg, selTag.Data()));
        cv->SaveAs(TString::Format("%s/efficiency_grid_cfg%d_%s.pdf", outDir.c_str(), cfg, selTag.Data()));

        for (int r = 0; r < 4; ++r) {
            auto it = ckv.second.find(r);
            if (it == ckv.second.end() || !it->second) continue;
            const auto& tot = effTotals[cfg][r];
            const double eff = tot.first > 0. ? tot.second / tot.first : 0.;
            csv << cfg << "," << kReflName[r] << ","
                << (selectAll ? "all" : sipmList) << ","
                << eff << "\n";   // overall efficiency = sum(collected)/sum(produced)
        }
    }
    csv.close();

    std::cout << "Plots written to " << outDir << "/  (SiPM selection: "
              << (selectAll ? "all" : sipmList) << ", " << nb << "x" << nb
              << " bins of " << cellSize << " mm)\n";
}
