// opt_maps.C -- v7_opt detector-design comparison: whole-plate collection-
// efficiency maps, the kappa_2D position contrast, and a summary CSV.
//
// The v7_opt campaign is eleven independent detector *designs* (see
// opt_designs.h, the single source of truth), each irradiated over the full
// 250 x 250 mm^2 plate.  This macro reads the `sipmHits` trees and produces
//
//   fig_opt_maps.pdf / .png     4 x 3 grid, one 5 mm-cell collection-efficiency
//                               map per design, shared log z, SiPM layout and
//                               slab outline overlaid.
//   fig_opt_maps_norm.pdf / .png  same grid, each panel divided by its own
//                               plate-averaged efficiency (eps/<eps>), shared
//                               log z 0.5-2.0.
//   fig_opt_contrast.pdf / .png bar chart of kappa_2D = eps_max / eps_min on
//                               the 5 x 5 grid of 50 mm cells, grouped by stem,
//                               with the v7 glossy reference lines.
//   opt_maps.csv                tag, events, eps_mean, kappa_2d, cell_min,
//                               cell_max, note.
//
// The efficiency definition, the sim->report frame mapping, the padded-bin
// trick, the SiPM tile helper and the kappa_2D cell contrast are copied
// (adapted, not included) from plot_whole_slab.C / whole_slab_uniformity.C:
//
//   efficiency(cell) = sum_events nDetected / sum_events nProduced
//
// with nProduced counted ONCE per event, never once per (event, SiPM) row.
// Events = the number of DISTINCT evtNb values in the tree: an event writes one
// row per channel (zero-detection rows included) only when the muon entered the
// slab, and a muon can land in a cut-away corner triangle of preset 12 and
// write no rows at all.
//
// A design whose file is missing or unreadable is skipped ENTIRELY (warning on
// stderr): it gets no map panel, no contrast bar and no CSV row, so the
// previous campaign's figures remain valid until a new design's data lands.
//
// Usage (batch):
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//       root -l -b -q 'macros/opt_maps.C("<whole_dir>","report/figures")'
//
// Author: G4DCSmonitor analysis tooling.

#include <TBox.h>
#include <TCanvas.h>
#include <TColor.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TMath.h>
#include <TPad.h>
#include <TPaletteAxis.h>
#include <TPolyLine.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// Design table + shared ROOT style.  ROOT_INCLUDE_PATH must contain this
// macros/ directory (see the usage line above).
#include "opt_designs.h"

namespace {

// ---------------------------------------------------------------------------
// geometry / frame conventions (identical to plot_whole_slab.C)
// ---------------------------------------------------------------------------
const double kHalf = 125.0;    // slab half-width [mm]
const double kCornerLeg = 12.0; // preset 12: corner truncation leg [mm]

// Simulation frame -> report frame, applied to every design:
// (x,y) -> (y,-x), a 90 deg clockwise rotation (report: top=+y, left=-x).
inline void toReportFrame(double& x, double& y)
{
    const double xr = y, yr = -x;
    x = xr; y = yr;
}

// Bin edges aligned to the slab: the interior grid is an integer number of
// equal cells spanning exactly [-half, half], so no cell straddles the slab
// boundary.  The two outermost (partial) bins only pad the view out to +/-span
// so the SiPM markers drawn just outside the slab remain visible.
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

// Edges of nIn equal cells spanning exactly [-half, half] (no padding), used
// for the coarse 5 x 5 kappa_2D grid.
std::vector<double> makeCellEdges(int nIn, double half)
{
    std::vector<double> e;
    e.reserve(nIn + 1);
    for (int k = 0; k <= nIn; ++k) e.push_back(-half + 2.0 * half * k / nIn);
    return e;
}

// SiPM tile centres in the report frame (top=+y, left=-x), for the presets
// used by this campaign: 1 and 8 (two per side at u = +-0.5 / +-0.75) and
// 10 / 11 (four-sided WLS read-out, single- / dual-ended).  The WLS positions
// are the analytic placement from GeometryMuonScint::PlaceWLSFibres(), copied
// from position_reconstruction.C::GetSipmTable().
std::vector<std::pair<double, double>> sipmTiles(int preset, double half)
{
    const double H = half;
    std::vector<std::pair<double, double>> t;
    switch (preset) {
        case 1: {
            const double u = 0.5 * H;
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        }
        case 8: {
            const double u = 0.75 * H;
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        }
        case 12: {
            // Convex octagon: each square corner truncated with a 12 mm leg, so
            // the 45-degree face is 17.0 mm wide and its centre lies 6 mm
            // inside the square corner, at (+-119, +-119) mm in the report
            // frame.  Order matches the sipmHits channel index:
            //   0 = (+x,+y) edge -> report (+119,-119)  bottom-right
            //   1 = (-x,+y) edge -> report (+119,+119)  top-right
            //   2 = (-x,-y) edge -> report (-119,+119)  top-left
            //   3 = (+x,-y) edge -> report (-119,-119)  bottom-left
            const double c = half - 0.5 * kCornerLeg;
            t = {{ c, -c}, { c,  c}, {-c,  c}, {-c, -c}};
            break;
        }
        case 10:
        case 11: {
            // Four-sided WLS read-out.  Slab-local SiPM centres, then the
            // usual slab-local -> report rotation (xr,yr) = (y_l,-x_l).
            const double kRFibre        = 0.5;    // WLS fibre radius [mm]
            const double kClampGap      = 0.10;   // slab <-> clamp grease film
            const double kFibreProtr    = 0.05;   // protrusion into the film
            const double kSiPMHalfX     = 2.0;    // 4 mm square SiPM
            const double kCouplingHalfZ = 0.05;   // read-out grease pad
            const double kSiPMHalfZ     = 0.3;    // SiPM half-thickness
            const double kPadClearance  = 0.05;
            const double kFourSidedTrim = 1.0;
            const double kHalfL   = half - kFourSidedTrim;
            const double faceInset = kClampGap + kRFibre - kFibreProtr;
            const double padShift  = kSiPMHalfX - faceInset + kPadClearance;
            const double padX      = -(half + faceInset) - padShift;
            const double ySiPM     = kHalfL + 2.0 * kCouplingHalfZ + kSiPMHalfZ;

            auto addEnd = [&](int side, int endSign) {
                const double ye = endSign * ySiPM;
                double xl = 0.0, yl = 0.0;
                switch (side) {
                    case 0:  xl =  padX; yl =  ye;   break;
                    case 1:  xl = -ye;   yl =  padX; break;
                    case 2:  xl = -padX; yl = -ye;   break;
                    default: xl =  ye;   yl = -padX; break;
                }
                t.push_back({yl, -xl});     // report frame
            };
            if (preset == 10) {
                for (int side = 0; side < 4; ++side) addEnd(side, -1);
            } else {
                for (int side = 0; side < 4; ++side) {
                    addEnd(side, -1);
                    addEnd(side, +1);
                }
            }
            break;
        }
        default:
            break;
    }
    return t;
}

// Slab outline + SiPM positions (black, hollow squares during the map drawing
// so the markers never hide the corner cells underneath them).  Configs 1/8
// are on-edge tiles, so their markers are pushed just outboard of the slab;
// the WLS read-out positions (10/11) already lie outside the slab faces and
// are drawn as-is.
void drawOverlays(int preset, double half, double side = 8.0, double gap = 2.0)
{
    if (preset == 12) {
        // Octagon outline (report frame; (x,y)->(y,-x) is a symmetry of the
        // square, so |x|+|y| = 2*half - kCornerLeg is unchanged).
        const double a = half - kCornerLeg;
        // Nine points: TPolyLine does not close the contour itself, so repeat
        // the first vertex -- else the bottom-right chamfer is left open.
        double xs[9] = { half,  half,    a,   -a, -half, -half,   -a,    a,  half};
        double ys[9] = {   -a,     a, half, half,    a,   -a, -half, -half,   -a};
        TPolyLine* oct = new TPolyLine(9, xs, ys);
        oct->SetFillStyle(0);
        oct->SetLineColor(kBlack);
        oct->SetLineWidth(2);
        oct->Draw("SAME");
    } else {
        TBox* slab = new TBox(-half, -half, half, half);
        slab->SetFillStyle(0);
        slab->SetLineColor(kBlack);
        slab->SetLineWidth(2);
        slab->Draw("SAME");
    }

    auto marker = [&](double cx, double cy) {
        TBox* b = new TBox(cx - 0.5 * side, cy - 0.5 * side,
                           cx + 0.5 * side, cy + 0.5 * side);
        b->SetFillStyle(0);             // hollow: do not mask the map
        b->SetLineColor(kBlack);
        b->SetLineWidth(2);
        b->Draw("SAME");
    };

    if (preset == 1 || preset == 8) {
        const double off = gap + 0.5 * side;
        const double tol = 1e-6;
        for (const auto& p : sipmTiles(preset, half)) {
            double cx = p.first, cy = p.second;
            if      (std::fabs(cx - half) < tol) cx =  half + off;
            else if (std::fabs(cx + half) < tol) cx = -half - off;
            if      (std::fabs(cy - half) < tol) cy =  half + off;
            else if (std::fabs(cy + half) < tol) cy = -half - off;
            marker(cx, cy);
        }
        return;
    }

    for (const auto& p : sipmTiles(preset, half)) marker(p.first, p.second);
}

// Fixed-size per-panel title, drawn as TLatex instead of the histogram title
// (ROOT auto-shrinks long histogram titles to fit the pad, which made the panel
// titles come out at different sizes).  Long manifest labels are split
// after their last comma into two lines of the same font size.
//
// x = 0.63 is the centre of the map frame (left margin 0.30, right margin 0.04
// -> frame spans 0.30-0.96).
void drawPanelTitle(const char* label)
{
    TLatex t;
    t.SetNDC();
    t.SetTextAlign(22);
    t.SetTextFont(42);
    t.SetTextSize(0.065);          // 0.065 * 721 px sub-pad -> 27 px capitals

    const TString s(label);
    int cut = -1;
    if (s.Length() > 20) {
        for (int i = s.Length() - 2; i >= 0; --i)
            if (s[i] == ',' && s[i + 1] == ' ') { cut = i; break; }
    }
    if (cut > 0) {
        const TString l1(s(0, cut));   // TSubString::Data() is not length-limited
        t.DrawLatex(0.63, 0.958, l1.Data());
        t.DrawLatex(0.63, 0.876, s.Data() + cut + 2);
    } else {
        t.DrawLatex(0.63, 0.930, s.Data());
    }
}

// Linear-interpolated percentile of a sorted copy.
double percentile(const std::vector<double>& v, double p)
{
    if (v.empty()) return 0.;
    std::vector<double> s(v);
    std::sort(s.begin(), s.end());
    const double idx = p * (double)(s.size() - 1);
    const size_t lo = (size_t)std::floor(idx);
    const size_t hi = (size_t)std::ceil(idx);
    return s[lo] + (s[hi] - s[lo]) * (idx - (double)lo);
}

// ---------------------------------------------------------------------------
// per-design result
// ---------------------------------------------------------------------------
struct Result {
    const opt::Design* d = nullptr;
    bool    ok = false;
    TString note;
    long long events = 0;       // distinct evtNb values in the tree
    double  meanEff = 0.;       // plate-averaged: sum coll / sum prod
    double  kappa = 0.;         // max cell eps / min cell eps (5 x 5)
    double  cellMin = 0.;
    double  cellMax = 0.;
    int     zeroCells = 0;      // 50 mm cells with zero produced photons
    TH2D*   eff = nullptr;      // 5 mm-cell efficiency map (owned)
};

}  // namespace

// ===========================================================================
void opt_maps(const char* dir =
                  "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole",
              const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);        // style() hides titles; each map panel is labelled
    gStyle->SetLegendBorderSize(0);
    gStyle->SetPalette(kBird);
    gStyle->SetNumberContours(99);
    gSystem->mkdir(outdir, kTRUE);

    const std::vector<opt::Design> designs = opt::designs();
    const int nd = (int)designs.size();

    const double cellSize = 5.0;             // fine-map cell [mm]
    const double margin   = 18.0;            // pad so outboard markers fit
    const double span     = kHalf + margin;
    const std::vector<double> edges = makeEdges(span, kHalf, cellSize);
    const int nb = (int)edges.size() - 1;    // interior 50 bins + 2 pad bins

    const int kCellN = 5;                    // 5 x 5 = 50 mm cells for kappa_2D
    const std::vector<double> cellEdges = makeCellEdges(kCellN, kHalf);

    std::vector<Result> res(nd);
    for (int i = 0; i < nd; ++i) { res[i].d = &designs[i]; res[i].note = "no data"; }

    // ---------------------------------------------------------------- read
    printf("\n[v7_opt] reading %d designs from %s\n", nd, dir);
    for (int i = 0; i < nd; ++i) {
        const opt::Design& d = designs[i];
        Result& R = res[i];

        const TString fn = opt::file(dir, d);
        TFile* f = TFile::Open(fn, "READ");
        if (!f || f->IsZombie()) {
            delete f;
            R.ok = false;
            R.note = "no data (file not ready: zombie/short file)";
            fprintf(stderr, "  %-18s SKIP  %s\n", d.tag, R.note.Data());
            continue;
        }
        TTree* tree = (TTree*)f->Get("sipmHits");
        if (!tree) {
            delete f;
            R.ok = false;
            R.note = "no data (no sipmHits tree)";
            fprintf(stderr, "  %-18s SKIP  %s\n", d.tag, R.note.Data());
            continue;
        }

        int evt = 0, det = 0, nDet = 0, nProd = 0;
        double mx = 0., my = 0.;
        tree->SetBranchAddress("evtNb", &evt);
        tree->SetBranchAddress("nDetected", &nDet);
        tree->SetBranchAddress("nProduced", &nProd);
        tree->SetBranchAddress("muonX", &mx);
        tree->SetBranchAddress("muonY", &my);
        const bool hasDet = (tree->GetBranch("det") != nullptr);
        if (hasDet) tree->SetBranchAddress("det", &det);

        struct EvRow { double x, y, prod, coll; };
        std::map<long long, EvRow> ev;   // distinct evtNb -> event totals
        int prodMismatch = 0;

        for (long long e = 0; e < tree->GetEntries(); ++e) {
            tree->GetEntry(e);
            if (hasDet && det != 0) continue;
            auto it = ev.find(evt);
            if (it == ev.end()) {
                double x = mx, y = my;
                toReportFrame(x, y);
                ev[evt] = EvRow{x, y, (double)nProd, 0.};
                it = ev.find(evt);
            } else if ((double)nProd != it->second.prod) {
                ++prodMismatch;
            }
            it->second.coll += nDet;
        }
        delete f;

        if (ev.empty()) {
            R.ok = false;
            R.note = "no data (tree has no events)";
            fprintf(stderr, "  %-18s SKIP  %s\n", d.tag, R.note.Data());
            continue;
        }

        // Number of events = number of distinct evtNb values present.  Events
        // whose muon missed the slab write no rows and must not enter as zero.
        R.events = (long long)ev.size();

        TH2D* hc = new TH2D(TString::Format("hc_%s", d.tag), "collected",
                            nb, edges.data(), nb, edges.data());
        TH2D* hp = new TH2D(TString::Format("hp_%s", d.tag), "produced",
                            nb, edges.data(), nb, edges.data());
        hc->SetDirectory(nullptr);
        hp->SetDirectory(nullptr);

        double totColl = 0., totProd = 0.;
        for (const auto& kv : ev) {
            const EvRow& r = kv.second;
            hc->Fill(r.x, r.y, r.coll);
            hp->Fill(r.x, r.y, r.prod);
            totColl += r.coll;
            totProd += r.prod;
        }
        R.meanEff = totProd > 0. ? totColl / totProd : 0.;

        // 5 mm efficiency map.
        TH2D* eff = (TH2D*)hc->Clone(TString::Format("eff_%s", d.tag));
        eff->SetDirectory(nullptr);
        eff->Divide(hp);
        eff->SetTitle(d.label);
        eff->SetXTitle("x [mm]");
        eff->SetYTitle("y [mm]");
        eff->SetZTitle("efficiency (n_det/n_produced)");
        R.eff = eff;

        // kappa_2D on the 5 x 5 grid of 50 mm cells, restricted to the plate
        // (the cell edges span exactly [-125,125], so every cell is on-plate).
        TH2D hc5("hc5", "", kCellN, cellEdges.data(), kCellN, cellEdges.data());
        TH2D hp5("hp5", "", kCellN, cellEdges.data(), kCellN, cellEdges.data());
        for (const auto& kv : ev) {
            hc5.Fill(kv.second.x, kv.second.y, kv.second.coll);
            hp5.Fill(kv.second.x, kv.second.y, kv.second.prod);
        }
        double emin = 0., emax = 0.;
        int nUsed = 0;
        R.zeroCells = 0;
        for (int bx = 1; bx <= kCellN; ++bx)
            for (int by = 1; by <= kCellN; ++by) {
                const double p = hp5.GetBinContent(bx, by);
                if (p <= 0.) { ++R.zeroCells; continue; }
                const double eps = hc5.GetBinContent(bx, by) / p;
                if (nUsed == 0 || eps < emin) emin = eps;
                if (nUsed == 0 || eps > emax) emax = eps;
                ++nUsed;
            }
        R.cellMin = emin;
        R.cellMax = emax;
        R.kappa   = (emin > 0.) ? emax / emin : 0.;

        R.ok = true;
        R.note = "ok";
        if (R.zeroCells > 0)
            R.note = TString::Format("ok; %d cell(s) with zero produced", R.zeroCells);
        if (prodMismatch > 0)
            R.note += TString::Format("; %d nProduced row mismatch(es)", prodMismatch);

        printf("  %-18s evts=%8lld  <eps>=%.4e  kappa_2D=%.3f  cells[%.3e, %.3e]\n",
               d.tag, R.events, R.meanEff, R.kappa, R.cellMin, R.cellMax);
    }

    // Designs with no readable file are skipped ENTIRELY: no map panel, no
    // contrast bar, no CSV row.
    std::vector<int> active;
    for (int i = 0; i < nd; ++i)
        if (res[i].ok) active.push_back(i);
    if (active.empty()) {
        fprintf(stderr, "[v7_opt] no readable files under %s -- nothing to do\n",
                dir);
        return;
    }

    // ============== shared robust log-z range ==============================
    // Collect every populated interior 5 mm cell over all designs, then crop to
    // the 1st-99th percentile so one bright/dead cell cannot flatten the scale.
    std::vector<double> zvals;
    for (int i : active) {
        const Result& R = res[i];
        if (!R.eff) continue;
        for (int bx = 2; bx <= nb - 1; ++bx)
            for (int by = 2; by <= nb - 1; ++by) {
                const double v = R.eff->GetBinContent(bx, by);
                if (v > 0.) zvals.push_back(v);
            }
    }
    double zLo, zHi;
    if (zvals.empty()) {
        zLo = 1e-4; zHi = 1.;
    } else {
        zLo = percentile(zvals, 0.01);
        zHi = percentile(zvals, 0.99);
        if (!(zLo > 0.)) zLo = *std::min_element(zvals.begin(), zvals.end());
        if (!(zHi > zLo)) zHi = *std::max_element(zvals.begin(), zvals.end());
        if (zHi <= zLo) zHi = zLo * 10.;
        // small visual margin so the cropped limits are not exactly on the data
        zLo *= 0.90;
        zHi *= 1.10;
    }
    printf("[v7_opt] shared log-z range (1st-99th percentile crop): [%.4g, %.4g]"
           " from %zu populated cells\n", zLo, zHi, zvals.size());

    // ============== Figs 1a/1b: 3 x 4 efficiency maps =====================
    // One drawing pass serves both the absolute and the normalised grid: every
    // panel is a clone of the histogram filled during the read loop, so the
    // trees are touched once.  `norm` divides each panel by its own plate-
    // averaged efficiency eps_mean and switches the shared z range to 0.5-2.0.
    //
    // Layout is driven by the print-size requirement: the 27 px capital target
    // needs S ~ 0.06, which only fits when the tick labels are shared rather
    // than repeated.  A 4-column grid also keeps each printed panel >= 3.5 cm
    // (451.7 pt / 4 = 112.9 pt); a 5-column grid would be 90.3 pt = 3.19 cm.
    // The colour bar is shared by all design panels and lives in its own bottom
    // pad, so the maps keep the full canvas width.
    //
    // Geometry (saved PNG 2592 x 2358): 4 x 3 pads of 644 x 721 px, frame
    // 425 x 425 px (square -> the 500 mm plate stays square), left/right/
    // bottom/top margins 0.30/0.04/0.24/0.17.
    auto drawMapGrid = [&](bool norm, const char* fname, const char* name) {
        const int ncol = 4, nrow = 3;
        const int cw = 2596, ch = 2387;        // PNG is 2592 x 2359
        TCanvas* cv = new TCanvas(name, fname, cw, ch);
        cv->cd();
        TPad* grid = new TPad(TString::Format("%s_grid", name), "",
                              0.0, 0.0793, 1.0, 1.0);
        grid->SetMargin(0., 0., 0., 0.);
        grid->SetBorderMode(0);
        grid->Draw();
        grid->Divide(ncol, nrow, 0.002, 0.002);

        const double zmin = norm ? 0.5 : zLo;
        const double zmax = norm ? 2.0 : zHi;

        // One text size for every label of the map grid: the sub-pad is 721 px
        // in the saved PNG, and ROOT renders Helvetica capitals at ~0.63 of
        // the nominal size, so 0.060 * 721 * 0.63 ~= 27 px.
        const double kText = 0.065;

        const int np = (int)active.size();
        for (int k = 0; k < np; ++k) {
            grid->cd(k + 1);
            const int col = k % ncol;
            const int row = k / ncol;
            gPad->SetLeftMargin(0.30);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.24);
            gPad->SetTopMargin(0.17);

            const Result& R = res[active[k]];
            // Share the tick labels: y on the left column, x on the bottom row.
            // Interior panels drop them (the frames stay identical, so the
            // plates keep the same size and stay square).
            const bool showX = (row == nrow - 1);
            const bool showY = (col == 0);
            const char* xtitle = showX ? "x [mm]" : "";
            const char* ytitle = showY ? "y [mm]" : "";

            const bool have = (R.ok && R.eff != nullptr);
            TH2D* h = nullptr;
            if (have) {
                h = (TH2D*)R.eff->Clone(TString::Format("draw_%s_%d", R.d->tag, (int)norm));
                h->SetDirectory(nullptr);
                if (norm && R.meanEff > 0.) h->Scale(1.0 / R.meanEff);
                // Zero / under-range interior cells: paint them with the bottom
                // colour of the palette ("no statistics") instead of leaving a
                // white hole.  Only the out-of-slab padding bins stay blank.
                // For the corner-cut octagon (preset 12) the 5 mm bins whose
                // centre lies outside |x|+|y| = 2*half-12 are outside the slab
                // and must stay blank rather than being painted at zmin.
                const bool octagon = (R.d->preset == 12);
                const double cut = 2.0 * kHalf - kCornerLeg;
                for (int bx = 2; bx <= nb - 1; ++bx) {
                    const double xc = h->GetXaxis()->GetBinCenter(bx);
                    for (int by = 2; by <= nb - 1; ++by) {
                        const double yc = h->GetYaxis()->GetBinCenter(by);
                        if (octagon && std::fabs(xc) + std::fabs(yc) > cut)
                            continue;
                        if (h->GetBinContent(bx, by) < zmin)
                            h->SetBinContent(bx, by, zmin);
                    }
                }
            } else {
                h = new TH2D(TString::Format("empty_%s_%d", R.d->tag, (int)norm), "",
                             nb, edges.data(), nb, edges.data());
                h->SetDirectory(nullptr);
            }
            h->SetTitle("");                 // panel title drawn by drawPanelTitle()
            h->SetXTitle(xtitle);
            h->SetYTitle(ytitle);

            h->GetXaxis()->SetTitleSize(kText);
            h->GetYaxis()->SetTitleSize(kText);
            h->GetXaxis()->SetLabelSize(showX ? kText : 0.);
            h->GetYaxis()->SetLabelSize(showY ? kText : 0.);
            h->GetXaxis()->SetTitleOffset(1.05);
            h->GetYaxis()->SetTitleOffset(0.80);
            // TGaxis right-adjusts titles by default; centre them on the frame.
            h->GetXaxis()->CenterTitle();
            h->GetYaxis()->CenterTitle();

            if (have) {
                gPad->SetLogz();
                h->SetMinimum(zmin);
                h->SetMaximum(zmax);
                h->Draw("COL");   // no per-panel palette: one shared bar below
                drawOverlays(R.d->preset, kHalf);
                gPad->Modified();
            } else {
                h->SetMinimum(0.);
                h->SetMaximum(1.);
                h->Draw("AXIS");
                TLatex t;
                t.SetNDC();
                t.SetTextAlign(22);
                t.SetTextSize(0.070);
                t.SetTextColor(kGray + 2);
                t.DrawLatex(0.63, 0.52, "no data");
                t.SetTextSize(0.045);
                t.DrawLatex(0.63, 0.42, "(file not ready)");
            }
            drawPanelTitle(R.d->label);
        }

        // ---- one shared colour bar, in its own pad below the grid ----------
        {
            cv->cd();                  // ppad must be a child of the canvas, not
                                       // of the last sub-pad (grid->cd above)
            // The pad is wider than the bar: TPaletteAxis centres the first
            // and last tick labels on the bar ends, so a pad ending exactly
            // at the bar clips half of each ("5x10^-1" -> "0^-1", "2" -> "2|").
            TPad* ppad = new TPad(TString::Format("%s_pal", name), "",
                                  0.02, 0.003, 0.98, 0.077);
            ppad->SetMargin(0., 0., 0., 0.);
            ppad->SetBorderMode(0);
            ppad->Draw();
            ppad->cd();
            ppad->SetLogz();

            TH2D* ph = new TH2D(TString::Format("palh_%s", name), "",
                                1, 0., 1., 1, 0., 1.);
            ph->SetDirectory(nullptr);
            ph->SetMinimum(zmin);
            ph->SetMaximum(zmax);
            ph->SetContour(99);
            ph->GetZaxis()->SetMoreLogLabels();

            // The pad is 175 px tall in the saved PNG; the palette axis labels
            // are pad-height relative, so 0.245 * 175 * 0.63 ~= 27 px.
            // The bar keeps its absolute extent (x = 0.12..0.88 of the canvas)
            // inside the wider pad, so the colour map and title do not move.
            const double kPal = 0.245;
            TPaletteAxis* pal = new TPaletteAxis(0.104167, 0.42, 0.895833, 0.60, ph);
            pal->SetLabelSize(kPal);
            pal->Draw();

            // Axis title above the bar (a TLatex is used instead of the TPalette
            // title so that the title never collides with the log labels: the
            // automatic horizontal title is drawn in the same band as them).
            TLatex zt;
            zt.SetNDC();
            zt.SetTextAlign(22);
            zt.SetTextFont(42);
            zt.SetTextSize(kPal);
            zt.DrawLatex(0.5, 0.90,
                         norm ? "#varepsilon / #LT#varepsilon#GT" : "#varepsilon");
        }

        for (const char* ex : {"pdf", "png"})
            cv->SaveAs(TString::Format("%s/%s.%s", outdir, fname, ex));
        printf("[v7_opt] wrote %s/%s.{pdf,png}\n", outdir, fname);
    };

    drawMapGrid(false, "fig_opt_maps", "c_opt_maps");
    drawMapGrid(true,  "fig_opt_maps_norm", "c_opt_maps_norm");

    // ============== Fig 2: kappa_2D bar chart, grouped by stem ============
    {
        // Group order: position, wls, maxpe, control; table order inside a group.
        // Headers use the same plain words as fig_opt_yield (opt::stemLabel),
        // not the raw stem keys.
        const char* groupOrder[5] = {"position", "wls", "maxpe", "control",
                                     "corner"};
        const char* groupLabel[5] = {"ballistic", "fibre read-out",
                                     "large sensor", "control",
                                     "corner read-out"};
        std::vector<int> order;
        std::vector<int> groupOf(nd, 0);
        for (int g = 0; g < 5; ++g) {
            for (int i : active) {
                if (TString(designs[i].stem) != groupOrder[g]) continue;
                groupOf[i] = g;
                order.push_back(i);
            }
        }

        // Bar centres with a gap between groups.
        std::vector<double> xpos(nd, 0.);
        double x = 0.8;
        int prevG = -1;
        for (int i : order) {
            if (prevG >= 0 && groupOf[i] != prevG) x += 0.6;
            xpos[i] = x;
            x += 1.0;
            prevG = groupOf[i];
        }
        const double xMax = x - 0.2;

        double kMax = 1.0;
        for (int i : active) kMax = std::max(kMax, res[i].kappa);
        const double yMax = kMax * 1.22;

        TCanvas* cv = new TCanvas("c_opt_contrast", "v7_opt kappa_2D", 1500, 820);
        gPad->SetLeftMargin(0.10);
        gPad->SetRightMargin(0.03);
        gPad->SetBottomMargin(0.32);
        gPad->SetTopMargin(0.20);      // reserved band for the legend

        TH1D* frame = new TH1D("frame_opt_kappa", "", 1, 0., xMax);
        frame->SetDirectory(nullptr);
        frame->SetStats(0);
        frame->SetTitle("");
        frame->GetYaxis()->SetTitle(
            "#kappa_{2D} = #varepsilon_{max}/#varepsilon_{min}");
        frame->GetYaxis()->SetTitleSize(0.040);
        frame->GetYaxis()->SetTitleOffset(0.95);
        // Frame-axis labels are rendered against the canvas width, not the
        // frame height: 0.031 x 1496 px x 0.349 ~= 16 px capitals.
        frame->GetYaxis()->SetLabelSize(0.031);
        frame->GetYaxis()->SetRangeUser(0., yMax);
        frame->GetXaxis()->SetLabelSize(0.);
        frame->GetXaxis()->SetTickLength(0.);
        frame->Draw("AXIS");

        // v7 reference lines (C1 glossy 1.07, C3 glossy 1.28), drawn BEFORE the
        // bars so the bars cover them, as neutral black/grey (red would be
        // nearly identical to one of the design colours).
        struct Ref { double y; Color_t c; int style; const char* txt; };
        const Ref refs[2] = {{1.07, kBlack, 2, "v7 C1 glossy 1.07"},
                             {1.28, kGray + 2, 7, "v7 C3 glossy 1.28"}};
        TLegend* leg = new TLegend(0.62, 0.83, 0.98, 0.95);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetTextSize(0.038);       // 0.038 * 792 px -> 18 px capitals
        for (const Ref& r : refs) {
            TGraph* g = new TGraph(2);
            g->SetPoint(0, 0., r.y);
            g->SetPoint(1, xMax, r.y);
            g->SetLineColor(r.c);
            g->SetLineStyle(r.style);
            g->SetLineWidth(2);
            g->Draw("L SAME");
            leg->AddEntry(g, r.txt, "l");
        }
        leg->Draw();

        const double barW = 0.68;
        for (int i : active) {
            const Result& R = res[i];
            const double xc = xpos[i];
            if (R.kappa <= 0.) continue;
            TBox* b = new TBox(xc - 0.5 * barW, 0., xc + 0.5 * barW, R.kappa);
            b->SetFillColor(opt::color(i));
            b->SetFillStyle(1001);
            b->SetLineColor(kBlack);
            b->SetLineWidth(1);
            b->Draw("SAME");

            // Value label on an opaque white box ABOVE the bar (not straddling
            // its top): the box bottom is kappa + 0.012*yMax, so the bar top
            // stays visible and a short bar is no longer optically shortened by
            // its own label.  The reference lines are drawn before the bars and
            // are hidden only where this box covers them, never across digits.
            const double boxLo = R.kappa + 0.012 * yMax;
            const double boxHi = boxLo + 0.065 * yMax;
            TBox* bg = new TBox(xc - 0.34, boxLo, xc + 0.34, boxHi);
            bg->SetFillColor(kWhite);
            bg->SetFillStyle(1001);
            bg->SetLineColor(kWhite);
            bg->Draw("SAME");

            TLatex v;
            v.SetTextAlign(22);        // centred horizontally AND vertically in the box
            v.SetTextSize(0.038);
            v.DrawLatex(xc, 0.5 * (boxLo + boxHi), Form("%.2f", R.kappa));
        }

        // Group labels (centred over each group).
        for (int g = 0; g < 5; ++g) {
            std::vector<double> xs;
            for (int i : order) if (groupOf[i] == g) xs.push_back(xpos[i]);
            if (xs.empty()) continue;
            const double gx = 0.5 * (xs.front() + xs.back());
            TLatex gl;
            gl.SetTextAlign(22);
            gl.SetTextSize(0.038);
            gl.SetTextFont(62);
            gl.DrawLatex(gx, yMax * 0.96, groupLabel[g]);
        }
        // separators at midpoints between groups
        {
            int prevGroup = -1;
            double prevX = 0.;
            for (int i : order) {
                if (prevGroup >= 0 && groupOf[i] != prevGroup) {
                    const double xb = 0.5 * (prevX + xpos[i]);
                    TGraph* s = new TGraph(2);
                    s->SetPoint(0, xb, 0.);
                    s->SetPoint(1, xb, yMax);
                    s->SetLineColor(kGray + 1);
                    s->SetLineStyle(3);
                    s->SetLineWidth(1);
                    s->Draw("L SAME");
                }
                prevGroup = groupOf[i];
                prevX = xpos[i];
            }
        }

        // Human-readable design labels under the bars, rotated 32 deg.
        for (int i : active) {
            TLatex t;
            t.SetTextAngle(32);
            t.SetTextAlign(32);
            t.SetTextSize(0.036);          // 0.036 * 792 px -> 17 px capitals
            t.SetTextColor(kBlack);
            t.DrawLatex(xpos[i] + 0.20, -0.020 * yMax, res[i].d->label);
        }

        for (const char* ex : {"pdf", "png"})
            cv->SaveAs(TString::Format("%s/fig_opt_contrast.%s", outdir, ex));
        printf("[v7_opt] wrote %s/fig_opt_contrast.{pdf,png}\n", outdir);
    }

    // ============== CSV + stdout table =====================================
    {
        const TString csv = TString::Format("%s/opt_maps.csv", outdir);
        FILE* fp = fopen(csv.Data(), "w");
        if (fp) {
            fprintf(fp, "tag,events,eps_mean,kappa_2d,cell_min,cell_max,note\n");
            for (int i : active) {
                const Result& R = res[i];
                fprintf(fp, "%s,%lld,%.6g,%.4f,%.6g,%.6g,%s\n",
                        R.d->tag, R.events, R.meanEff, R.kappa,
                        R.cellMin, R.cellMax, R.note.Data());
            }
            fclose(fp);
            printf("[v7_opt] wrote %s\n", csv.Data());
        }

        printf("\n%-18s %9s %12s %8s %12s %12s  %s\n",
               "tag", "events", "eps_mean", "kappa_2D", "cell_min", "cell_max", "note");
        for (int i : active) {
            const Result& R = res[i];
            printf("%-18s %9lld %12.4e %8.3f %12.4e %12.4e  %s\n",
                   R.d->tag, R.events, R.meanEff, R.kappa,
                   R.cellMin, R.cellMax, R.note.Data());
        }
    }
}
