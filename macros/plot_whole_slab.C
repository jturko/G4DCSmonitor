// plot_whole_slab.C -- full-plate light-collection-efficiency maps from the
// whole-slab (`data_output_whole`) irradiation, one panel per reflector.
//
// The whole-slab run uses a GPS planar beam covering the entire 250 x 250 mm^2
// plate, so every event's primary-muon first-entry position (muonX/muonY,
// slab-local mm) samples the full plate uniformly. The collection efficiency is
// reconstructed offline from the `sipmHits` tree, exactly as in
// plot_collection_efficiency.C:
//
//     efficiency(x,y) = sum_events nDetected(x,y) / sum_events nProduced(x,y)
//
// Unlike plot_collection_efficiency.C (which also emits produced/collected
// panels and per-(config,reflector) canvases), this macro produces a single
// publication figure: a row/2x2 grid of the efficiency map for each reflector
// present in the directory, all sharing one z-range, with the physical slab
// outline and the SiPM positions overlaid.
//
// Usage (batch):
//   root -l -b -q 'plot_whole_slab.C+("/path/to/data_output_whole", 1, 5.0, 125.0)'
//
// Args: dir (default "slurm/data_output_whole"), cfg (default 1),
//       cellSize_mm (default 5), halfExtent_mm (default 125).
//       cfg may be 1, 2, 12 (corner-cut, tag c12; "Config 2 (mod)"), 3 or 4.
//       Config 12 is the convex octagon (square corners truncated by a 12 mm
//       leg); its four SiPMs sit on the 45-degree faces at (+-119,+-119) mm and
//       are drawn as diamonds.  A missing/partial c12 file is skipped with a
//       warning: no panel is fabricated for it.
//
// Author: G4DCSmonitor analysis tooling.

#include <TBox.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TH2D.h>
#include <TPad.h>
#include <TPolyLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TList.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions).  Located via
// ROOT_INCLUDE_PATH (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};

// Config 12 (preset 12, tag c12): each square corner is truncated by a 45-deg
// cut with a 12 mm leg, leaving a convex octagon whose diagonal face centres
// sit at (+-119, +-119) mm.  The four SiPMs are flush on those faces.
const double kCornerLeg = 12.0;

std::vector<std::pair<double, double>> sipmTiles(int cfg, double half)
{
    const double H = half;
    const double u = 0.5 * H;
    std::vector<std::pair<double, double>> t;   // report frame (top=+y, left=-x)
    switch (cfg) {
        case 1:
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        case 2:
            t = {{ H, -u}, {-H,  u}, { u,  H}, {-u, -H}};
            break;
        case 12: {
            // Corner-cut octagon: one SiPM on each 45-degree face.  Report-frame
            // order = sipmHits channel order:
            //   0 BR (+119,-119)  1 TR (+119,+119)
            //   2 TL (-119,+119)  3 BL (-119,-119)
            const double c = H - 0.5 * kCornerLeg;   // 119 mm
            t = {{ c, -c}, { c,  c}, {-c,  c}, {-c, -c}};
            break;
        }
        case 3:
            t = {{-u, H}, { u, H}};   // both on the top (+y) edge
            break;
        case 4:
            // WLS fibre clamp: single read-out SiPM at the -Y end of the -X bar.
            t = {{-H, H}};
            break;
    }
    return t;
}

// Primary-muon entry point from the internal simulation frame into the report
// frame, applied to ALL configs (incl. config 4): (x,y) -> (y,-x), a 90 deg
// clockwise rotation (report frame: top=+y, left=-x).  The config-4 WLS clamp
// sits on the -x face with its read-out at slab-local -y, so under this same
// transform its bar is on the top edge and its SiPM at the top-left.
inline void toReportFrame(double& x, double& y)
{
    const double xr = y, yr = -x;
    x = xr; y = yr;
}

void stylePad(TVirtualPad* p)
{
    p->SetLeftMargin(0.17);
    p->SetRightMargin(0.24);
    p->SetBottomMargin(0.16);
    p->SetTopMargin(0.10);
}

// Bin edges aligned to the slab: the interior grid is an integer number of
// equal cells spanning exactly [-half, half], so no cell straddles the slab
// boundary, and the two outermost (partial) bins only pad the view out to
// +/-span so the SiPM markers drawn just outside the slab stay visible.
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

// Physical slab outline: the plain square for configs 1-4, the convex octagon
// (square corners cut by a 12 mm leg) for config 12.  The vertex order matches
// the preset-12 G4ExtrudedSolid in the report frame.
void drawSlabOutline(int cfg, double half)
{
    if (cfg == 12) {
        const double a = half - kCornerLeg;      // 113 mm
        double xs[8] = { half,  half,     a,    -a, -half, -half,    -a,     a};
        double ys[8] = {   -a,     a,  half,  half,     a,   -a, -half, -half};
        TPolyLine* oct = new TPolyLine(8, xs, ys);
        oct->SetFillStyle(0);
        oct->SetLineColor(kBlack);
        oct->SetLineWidth(2);
        oct->Draw("SAME");
        return;
    }
    TBox* slab = new TBox(-half, -half, half, half);
    slab->SetFillStyle(0);
    slab->SetLineColor(kBlack);
    slab->SetLineWidth(2);
    slab->Draw("SAME");
}

// Diamond marker: a square of side `side` rotated 45 deg, so its sides align
// with the corner-cut faces.  Vertices at (cx+-d, cy) and (cx, cy+-d).
void drawDiamond(double cx, double cy, double side = 8.0)
{
    const double d = 0.5 * side * std::sqrt(2.0);
    double xs[5] = {cx + d, cx, cx - d, cx, cx + d};
    double ys[5] = {cy, cy + d, cy, cy - d, cy};
    TPolyLine* m = new TPolyLine(5, xs, ys);
    m->SetFillColor(kBlack);
    m->SetFillStyle(1001);
    m->SetLineColor(kBlack);
    m->SetLineWidth(1);
    m->Draw("F");
    m->Draw("L SAME");
}

void drawOverlays(int cfg, double half, double side = 8.0, double gap = 2.0)
{
    drawSlabOutline(cfg, half);

    if (cfg == 4) {
        // Config 4 is not a set of discrete SiPMs but a WLS fibre clamp: a bar
        // running the full length of the top (+y) edge, outboard of the slab,
        // with the single read-out SiPM (black) butt-coupled at its left (-x)
        // end.  Same convention as Fig. 1 (sipm_layouts) and Fig. 3(b) of
        // Rao et al., so the fibres can be located relative to the read-out.
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

    if (cfg == 12) {
        // The four sensors are flush on the diagonal faces; draw a diamond at
        // each (+-119, +-119) mm, rotated 45 deg to match the face.
        for (const auto& t : sipmTiles(cfg, half))
            drawDiamond(t.first, t.second, side);
        return;
    }

    const double off = gap + 0.5 * side;
    const double tol = 1e-6;
    for (const auto& t : sipmTiles(cfg, half)) {
        double cx = t.first, cy = t.second;
        if      (std::fabs(t.first  - half) < tol) cx =  half + off;
        else if (std::fabs(t.first  + half) < tol) cx = -half - off;
        if      (std::fabs(t.second - half) < tol) cy =  half + off;
        else if (std::fabs(t.second + half) < tol) cy = -half - off;
        TBox* b = new TBox(cx - 0.5 * side, cy - 0.5 * side,
                           cx + 0.5 * side, cy + 0.5 * side);
        b->SetFillColor(kBlack);
        b->SetLineColor(kBlack);
        b->Draw("SAME");
    }
}

}  // namespace

void plot_whole_slab(const char* dir = "slurm/data_output_whole",
                     int cfg = 1,
                     double cellSize = 5.0,
                     double half = 125.0,
                     bool logZ = true)
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);        // style() hides titles; the panels are labelled by reflector

    if (cellSize <= 0.) cellSize = 5.0;
    const double margin = 18.0;
    const double span = half + margin;
    const std::vector<double> edges = makeEdges(span, half, cellSize);
    const int nb = (int)edges.size() - 1;

    TSystemDirectory sd(dir, dir);
    TList* files = sd.GetListOfFiles();
    if (!files) { std::cerr << "No such directory: " << dir << "\n"; return; }

    const TString prefix = TString::Format("muon_whole_out_c%d_r", cfg);
    std::vector<int> refls;
    std::map<int, TH2D*> prodMap, collMap;
    bool listed[4] = {false, false, false, false};

    for (auto* o : *files) {
        TString fname = o->GetName();
        if (!fname.EndsWith(".root") || !fname.BeginsWith(prefix)) continue;

        int refl = TString(fname(fname.Index("_r") + 2, 1)).Atoi();
        if (refl < 0 || refl > 3) continue;
        listed[refl] = true;

        TFile* f = TFile::Open(TString(dir) + "/" + fname, "READ");
        if (!f || f->IsZombie()) {
            std::cerr << "warning: cfg" << cfg << " " << kReflName[refl]
                      << ": unreadable/partial " << fname
                      << "; skipping (no panel)\n";
            delete f;
            continue;
        }
        TTree* tree = (TTree*)f->Get("sipmHits");
        if (!tree || tree->GetEntries() == 0) {
            std::cerr << "warning: cfg" << cfg << " " << kReflName[refl]
                      << ": no sipmHits entries in " << fname
                      << "; skipping (no panel)\n";
            delete f;
            continue;
        }

        int evt = 0, det = 0, nDet = 0, nProd = 0;
        double mx = 0., my = 0.;
        tree->SetBranchAddress("evtNb", &evt);
        tree->SetBranchAddress("det", &det);
        tree->SetBranchAddress("nDetected", &nDet);
        tree->SetBranchAddress("muonX", &mx);
        tree->SetBranchAddress("muonY", &my);
        tree->SetBranchAddress("nProduced", &nProd);

        std::map<int, int> evProd;
        std::map<int, std::pair<double, double>> evPos;
        std::map<int, int> evColl;
        for (long i = 0; i < tree->GetEntries(); ++i) {
            tree->GetEntry(i);
            if (det != 0) continue;
            if (!evProd.count(evt)) {
                double rx = mx, ry = my;
                toReportFrame(rx, ry);            // sim frame -> report frame
                evProd[evt] = nProd; evPos[evt] = {rx, ry};
            }
            evColl[evt] += nDet;
        }
        if (evProd.empty()) {
            // A tree with rows but no slab-0 (det==0) entry cannot produce a
            // meaningful map; treat it as partial rather than fabricating a
            // zero panel.
            std::cerr << "warning: cfg" << cfg << " " << kReflName[refl]
                      << ": no usable events in " << fname
                      << "; skipping (no panel)\n";
            delete f;
            continue;
        }
        if (!prodMap.count(refl)) {
            const TString nm = kReflName[refl];
            prodMap[refl] = new TH2D(TString::Format("h_wprod_c%d_r%d", cfg, refl),
                                     TString::Format("#gamma, cfg%d %s;x [mm];y [mm]", cfg, nm.Data()),
                                     nb, edges.data(), nb, edges.data());
            collMap[refl] = new TH2D(TString::Format("h_wcoll_c%d_r%d", cfg, refl),
                                     TString::Format("p.e., cfg%d %s;x [mm];y [mm]", cfg, nm.Data()),
                                     nb, edges.data(), nb, edges.data());
            prodMap[refl]->SetDirectory(nullptr);
            collMap[refl]->SetDirectory(nullptr);
            refls.push_back(refl);
        }
        TH2D* prod = prodMap[refl];
        TH2D* coll = collMap[refl];
        for (const auto& kv : evProd) {
            prod->Fill(evPos[kv.first].first, evPos[kv.first].second, kv.second);
            coll->Fill(evPos[kv.first].first, evPos[kv.first].second, evColl[kv.first]);
        }
        printf("cfg%d %-6s whole-slab events=%zu sum prod=%.4g sum coll=%.4g\n",
               cfg, kReflName[refl], evProd.size(), prod->Integral(), coll->Integral());
        delete f;
    }

    for (int r = 0; r < 4; ++r)
        if (!listed[r])
            std::cerr << "warning: cfg" << cfg << " " << kReflName[r]
                      << ": whole-slab file missing in " << dir
                      << "; no panel drawn\n";
    if (refls.empty()) {
        std::cerr << "No usable whole-slab file for cfg " << cfg << " in "
                  << dir << " (partial files are skipped; nothing written)\n";
        return;
    }
    std::sort(refls.begin(), refls.end());

    std::map<int, TH2D*> effMap;
    double zMax = 0., zMinPos = 0.;
    for (int r : refls) {
        TH2D* eff = (TH2D*)collMap[r]->Clone(TString::Format("h_weff_c%d_r%d", cfg, r));
        eff->SetDirectory(nullptr);
        eff->Divide(prodMap[r]);
        eff->SetTitle(TString::Format("%s;x [mm];y [mm]", kReflName[r]));
        eff->GetZaxis()->SetTitle(
            "#varepsilon(#font[132]{#it{x}},#font[132]{#it{y}}) = "
            "#font[132]{#it{N}}_{pe}/#font[132]{#it{N}}_{#gamma}");
        eff->GetZaxis()->SetMaxDigits(3);
        eff->GetXaxis()->SetTitleSize(0.040);
        eff->GetYaxis()->SetTitleSize(0.040);
        eff->GetZaxis()->SetTitleSize(0.038);
        eff->GetXaxis()->SetLabelSize(0.034);
        eff->GetYaxis()->SetLabelSize(0.034);
        eff->GetZaxis()->SetLabelSize(0.032);
        // Pull the titles in off the style default (which reserves room for the
        // much larger fonts used elsewhere); the right margin is wide enough for
        // the z-title to sit clear of its tick labels.
        eff->GetXaxis()->SetTitleOffset(1.30);
        eff->GetYaxis()->SetTitleOffset(1.55);
        eff->GetZaxis()->SetTitleOffset(1.70);
        for (int bx = 1; bx <= eff->GetNbinsX(); ++bx)
            for (int by = 1; by <= eff->GetNbinsY(); ++by) {
                const double v = eff->GetBinContent(bx, by);
                if (v > zMax) zMax = v;
                if (v > 0. && (zMinPos == 0. || v < zMinPos)) zMinPos = v;
            }
        effMap[r] = eff;
    }
    if (zMax <= 0.) zMax = 1.;
    zMax *= 1.05;
    // Log-z needs a strictly positive floor; use the smallest populated bin,
    // backed off a little so that floor stays visible. Linear mode starts at 0.
    double zMin = 0.;
    if (logZ) {
        zMin = (zMinPos > 0.) ? zMinPos / 1.5 : zMax * 1e-3;
        if (zMin >= zMax) zMin = zMax * 1e-3;
    }
    for (int r : refls) {
        effMap[r]->SetMinimum(zMin);
        effMap[r]->SetMaximum(zMax);
    }

    const int n = (int)refls.size();
    const int ncol = (n <= 1) ? 1 : 2;
    const int nrow = (n + ncol - 1) / ncol;
    // The x and y axes span the same range, so the map must be drawn with equal
    // mm-per-pixel on both axes (a square plate, as in the efficiency-grid maps).
    // ROOT stretches the frame to fill the pad, so the pad must be given a
    // slightly taller-than-wide shape that compensates for the margins: with the
    // stylePad margins below, padH/padW ~ 0.82 yields a square frame (calibrated
    // against TPad's absolute-pixel frame geometry).
    const int padW = 700;
    const int padH = (int)std::lround(padW * 0.817);
    TCanvas* cv = new TCanvas("c_whole_slab",
                              TString::Format("whole-slab collection efficiency, cfg %d", cfg),
                              padW * ncol, padH * nrow);
    cv->Divide(ncol, nrow, 0.004, 0.004);
    for (int i = 0; i < n; ++i) {
        cv->cd(i + 1);
        stylePad(gPad);
        TH2D* eff = (effMap[refls[i]]);
        if (logZ) gPad->SetLogz();
        eff->Draw("COLZ");
        drawOverlays(cfg, half);
    }

    const std::string outDir = std::string(dir) + "/plots";
    gSystem->mkdir(outDir.c_str(), kTRUE);
    const TString base = TString::Format("%s/whole_slab_cfg%d", outDir.c_str(), cfg);
    cv->SaveAs(base + ".pdf");
    cv->SaveAs(base + ".png");
    std::cout << "Wrote " << base << ".{pdf,png}  (" << nb << "x" << nb
              << " bins of " << cellSize << " mm)\n";
}
