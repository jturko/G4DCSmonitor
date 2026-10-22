// nearest_sensor_collapse.C -- collapse the whole-plate collection-efficiency
// field onto a single physical coordinate: the distance from the muon's entry
// point to the nearest collection site of the layout.
//
// Motivation
// ----------
// The whole-slab irradiation resolves the collection efficiency as a field
// eps(x,y) on the plate.  The report quotes a single number for the spread of
// that field (kappa_2D = max/min over 50 mm cells), but a contrast number
// carries no information about the *shape* of the field.  The maps show that
// the shape is not radial at all: for the contact-limited bare plate it is a
// checkerboard aligned with the sensor columns, and for the diffuse TiO2
// paint it is a bright frame around a dark central cross.  In both cases the
// cells that respond most strongly are the ones closest to a sensor.
//
// That suggests a single coordinate that organises the whole data set: the
// distance d from the muon entry point to the nearest collection site.  If
// the transport is not a perfect light mixer, the response should fall
// monotonically with d, and the *shape* of eps(d) should be the same for all
// four wrappers, while its amplitude (kappa_2D) differs.  The specular
// wrappers are near-perfect mixers and should show almost no dependence on d.
//
// Collection sites, in the report frame (top = +y, bottom = -y, left = -x,
// right = +x):
//   cfg1 : 8 side-mounted SiPMs, two per edge at u = +/-0.5
//   cfg2 : 4 side-mounted SiPMs, one per edge at the cfg1 offsets
//   cfg3 : 2 side-mounted SiPMs on the top (+y) edge at u = +/-0.5
//   cfg4 : the WLS fibre clamp, a line along the top (+y) edge; the light is
//          collected by the four fibres over their full length and then
//          transported to the single read-out SiPM at the top-left corner.
//          The collection aperture is therefore the line y = +125 mm, and the
//          distance to it is 125 - y.
//   cfg12: the corner-cut octagonal slab, with four SiPMs at the centres of
//          the 45-degree diagonal faces, (+/-119, +/-119) mm, all aimed
//          inward.  The distance is measured to those four sensor centres.
//
// Outputs
//   fig_nearest_sensor.pdf   eps/<eps> versus d, one panel per layout, one
//                            curve per reflector (shared linear y axis)
//   nearest_sensor.csv       config, reflector, d_lo, d_hi, events, eps,
//                            eps_norm
//   nearest_sensor_maps.txt  the 5x5 grid of 50 mm cell efficiencies for every
//                            (layout, reflector), for reference in the text
//
// Usage:
//   root -l -b -q 'macros/nearest_sensor_collapse.C("slurm/data_v6/whole","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TMarker.h>
#include <TPad.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions) and the PetroffPalette
// P6 categorical colours.  Located via ROOT_INCLUDE_PATH (set by
// macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

const double kHalf = 125.0;         // slab half-extent [mm]
const double kEdge = 62.5;          // +/-50% of half-length [mm]
const double kCorner = 119.0;       // corner-cut sensor centre, face distance [mm]
const int    kNBin = 10;            // 25 mm bins over 0-250 mm
const double kBinW = 25.0;
const int    kCellN = 5;            // 5x5 = 50 mm cells for the reference maps
const double kCellW = 2 * kHalf / kCellN;

const char* kReflName[4] = {"bare", "TiO2", "Al", "glossy"};
const int    kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};
const int    kStyle[4] = {1, 2, 3, 4};

// Panel order: configs 1, 2, 12 (corner-cut "Config 2 (mod)"), 3, 4.
const int kCfgOrder[5] = {1, 2, 12, 3, 4};
const int kNPad      = 5;       // five panels + one empty cell in a 2x3 grid

const char* cfgLabel(int cfg)
{
    switch (cfg) {
        case 1:  return "Config 1";
        case 2:  return "Config 2";
        case 12: return "Config 2 (mod)";
        case 3:  return "Config 3";
        case 4:  return "Config 4";
    }
    return "Config ?";
}

struct Site { double x, y; };

// Report-frame collection sites of the side-mounted layouts.
std::vector<Site> sites(int cfg)
{
    switch (cfg) {
        case 1:
            return {{ kHalf, -kEdge}, { kHalf,  kEdge}, {-kHalf, -kEdge}, {-kHalf,  kEdge},
                    {-kEdge,  kHalf}, { kEdge,  kHalf}, {-kEdge, -kHalf}, { kEdge, -kHalf}};
        case 2:
            return {{ kHalf, -kEdge}, {-kHalf,  kEdge}, { kEdge,  kHalf}, {-kEdge, -kHalf}};
        case 12:
            // Corner-cut (octagonal) slab: one SiPM at the centre of each
            // 45-degree diagonal face, all four aimed inward.  The distance
            // of each centre from the origin is 119 mm.
            return {{-kCorner, -kCorner}, {-kCorner,  kCorner},
                    { kCorner, -kCorner}, { kCorner,  kCorner}};
        case 3:
            return {{-kEdge,  kHalf}, { kEdge,  kHalf}};
        default:
            return {};
    }
}

// Distance from a report-frame point to the nearest collection site.
// For config 4 the aperture is the fibre line along the top edge.
double siteDistance(int cfg, double xr, double yr)
{
    if (cfg == 4) return kHalf - yr;          // distance to the line y = +125
    double best = 1e30;
    for (const Site& s : sites(cfg)) {
        const double dx = xr - s.x, dy = yr - s.y;
        best = std::min(best, std::sqrt(dx * dx + dy * dy));
    }
    return best;
}

// Per-event aggregation of one whole-slab file.
struct Event {
    double produced = 0.;   // optical photons produced in the slab
    double detected = 0.;   // photoelectrons over all SiPMs of the layout
    double xr = 0., yr = 0.;// muon entry point, report frame
};

std::map<int, Event> readEvents(const TString& fname)
{
    std::map<int, Event> ev;
    TFile* f = TFile::Open(fname);
    if (!f || f->IsZombie()) {
        printf("  MISSING %s\n", fname.Data());
        return ev;
    }
    TTree* t = (TTree*)f->Get("sipmHits");
    if (!t) { delete f; return ev; }
    int evt = 0, nd = 0, npr = 0;
    double mx = 0., my = 0.;
    t->SetBranchAddress("evtNb",   &evt);
    t->SetBranchAddress("nDetected", &nd);
    t->SetBranchAddress("nProduced", &npr);
    t->SetBranchAddress("muonX",   &mx);
    t->SetBranchAddress("muonY",   &my);
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        Event& e = ev[evt];
        e.detected += nd;
        e.produced  = npr;          // same for every row of an event
        e.xr = my;                  // report frame: (x,y) -> (y,-x)
        e.yr = -mx;
    }
    delete f;
    return ev;
}

void styleGraph(TGraph* g, int color, int style)
{
    g->SetLineColor(color);
    g->SetLineWidth(3);
    g->SetLineStyle(style);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(style + 20);
    g->SetMarkerSize(1.4);
}

}  // namespace

void nearest_sensor_collapse(const char* dir = "slurm/data_v6/whole",
                             const char* outdir = "report/figures")
{
    gSystem->mkdir(outdir, kTRUE);
    style(0.045);
    gStyle->SetOptTitle(0);

    // ---- data ------------------------------------------------------------
    // binned sums: [config][reflector][bin] of detected and produced
    std::vector<std::vector<std::vector<double>>> det(
        5, std::vector<std::vector<double>>(4, std::vector<double>(kNBin, 0.)));
    std::vector<std::vector<std::vector<double>>> pro(
        5, std::vector<std::vector<double>>(4, std::vector<double>(kNBin, 0.)));
    std::vector<std::vector<std::vector<int>>> nev(
        5, std::vector<std::vector<int>>(4, std::vector<int>(kNBin, 0)));
    std::vector<std::vector<double>> meanEff(5, std::vector<double>(4, 0.));
    std::vector<bool> have(5, false);

    FILE* fmap = fopen(TString::Format("%s/nearest_sensor_maps.txt", outdir), "w");
    if (fmap) fprintf(fmap,
        "# 5x5 grid of 50 mm cell efficiencies (x10^-3) in the report frame\n"
        "# (top = +y, bottom = -y, left = -x, right = +x); row 1 = y = +125..+75\n"
        "# config, reflector, then five rows of five cells\n");

    for (int slot = 0; slot < kNPad; ++slot) {
        const int cfg = kCfgOrder[slot];

        // Read all four reflectors first so a partial corner-cut production
        // run (missing or still-being-written files) never yields a
        // fabricated panel.
        std::vector<std::map<int, Event>> evs(4);
        for (int r = 0; r < 4; ++r)
            evs[r] = readEvents(
                TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, cfg, r));

        bool complete = true;
        for (int r = 0; r < 4; ++r) if (evs[r].empty()) complete = false;
        if (cfg == 12 && !complete) {
            printf("cfg12 (Config 2 (mod)): incomplete or missing data");
            for (int r = 0; r < 4; ++r) if (evs[r].empty()) printf(" r%d", r);
            printf(" -- omitting its panel and CSV rows\n");
            continue;
        }
        have[slot] = true;

        for (int r = 0; r < 4; ++r) {
            const std::map<int, Event>& ev = evs[r];
            double totD = 0., totP = 0.;
            std::vector<double> cellNum(kCellN * kCellN, 0.);
            std::vector<double> cellDen(kCellN * kCellN, 0.);
            for (const auto& kv : ev) {
                const Event& e = kv.second;
                totD += e.detected;
                totP += e.produced;
                const double d = siteDistance(cfg, e.xr, e.yr);
                int b = std::min(kNBin - 1, std::max(0, int(d / kBinW)));
                det[slot][r][b] += e.detected;
                pro[slot][r][b] += e.produced;
                nev[slot][r][b] += 1;
                const int ix = std::min(kCellN - 1, std::max(0, int((e.xr + kHalf) / kCellW)));
                const int iy = std::min(kCellN - 1, std::max(0, int((e.yr + kHalf) / kCellW)));
                cellNum[iy * kCellN + ix] += e.detected;
                cellDen[iy * kCellN + ix] += e.produced;
            }
            meanEff[slot][r] = (totP > 0.) ? totD / totP : 0.;
            printf("cfg%d %-6s events=%4zu  <eps>=%.5e  pe/evt=%.2f\n", cfg,
                   kReflName[r], ev.size(), meanEff[slot][r],
                   ev.empty() ? 0. : totD / (double)ev.size());
            if (fmap) {
                fprintf(fmap, "%d %s\n", cfg, kReflName[r]);
                for (int iy = kCellN - 1; iy >= 0; --iy) {
                    fprintf(fmap, "   ");
                    for (int ix = 0; ix < kCellN; ++ix) {
                        const double e = cellDen[iy * kCellN + ix] > 0.
                            ? 1e3 * cellNum[iy * kCellN + ix] / cellDen[iy * kCellN + ix] : 0.;
                        fprintf(fmap, "%9.3f", e);
                    }
                    fprintf(fmap, "\n");
                }
            }
        }
    }
    if (fmap) fclose(fmap);

    // ---- CSV -------------------------------------------------------------
    FILE* fp = fopen(TString::Format("%s/nearest_sensor.csv", outdir), "w");
    if (fp) {
        fprintf(fp, "config,reflector,d_lo_mm,d_hi_mm,events,eps,eps_norm\n");
        for (int slot = 0; slot < kNPad; ++slot) {
            if (!have[slot]) continue;
            const int cfg = kCfgOrder[slot];
            for (int r = 0; r < 4; ++r) {
                for (int b = 0; b < kNBin; ++b) {
                    const double e = pro[slot][r][b] > 0. ? det[slot][r][b] / pro[slot][r][b] : 0.;
                    const double en = meanEff[slot][r] > 0. ? e / meanEff[slot][r] : 0.;
                    fprintf(fp, "%d,%s,%.0f,%.0f,%d,%.6e,%.5f\n", cfg, kReflName[r],
                            b * kBinW, (b + 1) * kBinW, nev[slot][r][b], e, en);
                }
            }
        }
        fclose(fp);
    }

    // ---- figure ----------------------------------------------------------
    // 2 columns x 3 rows: five layouts (1, 2, 12, 3, 4) plus one empty cell.
    // Keeping the old 2x2 panel size gives a 1150x1425 canvas.  Identical
    // left margins in both columns: the rotated y-axis title of the left
    // column sits at the canvas edge and is clipped if its pad margin is
    // smaller than the right column's.  The legend of the top-left pad is
    // drawn in the TOP MARGIN, above the frame, so it cannot overlap the top
    // axis or the curves.
    TCanvas* c = new TCanvas("c_nearest", "", 1150, 1425);
    c->Divide(2, 3, 0.01, 0.01);

    double ymax = 1.0;
    for (int slot = 0; slot < kNPad; ++slot) {
        if (!have[slot]) continue;
        for (int r = 0; r < 4; ++r)
            for (int b = 0; b < kNBin; ++b)
                if (meanEff[slot][r] > 0. && pro[slot][r][b] > 0.)
                    ymax = std::max(ymax, det[slot][r][b] / pro[slot][r][b]
                                           / meanEff[slot][r]);
    }
    ymax = std::ceil(ymax * 1.06 * 10.) / 10.;

    // pad numbering of a Divide(2,3) grid runs left-to-right, top-to-bottom;
    // slot 2 (cfg12) and slot 5 (empty) may be blank.
    for (int slot = 0; slot < kNPad; ++slot) {
        if (!have[slot]) continue;
        const int cfg = kCfgOrder[slot];
        c->cd(slot + 1);
        gPad->SetLeftMargin(0.150);
        gPad->SetRightMargin(0.030);
        gPad->SetBottomMargin(0.140);
        gPad->SetTopMargin(0.190);

        TH1D* frame = new TH1D(TString::Format("fr_near_%d", cfg), "",
                               (int)(kNBin * kBinW), 0., kNBin * kBinW);
        frame->GetXaxis()->SetTitle("distance to nearest collection site  d [mm]");
        // <...> rather than #langle...#rangle: this ROOT build does not expand
        // those two TLatex symbols and prints them literally.
        frame->GetYaxis()->SetTitle("#varepsilon / <#varepsilon>");
        frame->GetXaxis()->SetTitleSize(0.045);
        frame->GetYaxis()->SetTitleSize(0.045);
        frame->GetXaxis()->SetLabelSize(0.042);
        frame->GetYaxis()->SetLabelSize(0.042);
        frame->GetXaxis()->SetTitleOffset(1.10);
        frame->GetYaxis()->SetTitleOffset(1.05);
        frame->GetXaxis()->CenterTitle(kTRUE);
        frame->GetYaxis()->CenterTitle(kTRUE);
        frame->GetYaxis()->SetRangeUser(0., ymax);
        frame->Draw("HIST");

        // Panel label drawn as a TLatex: a histogram title of a divided pad is
        // deleted before painting in batch mode (gPad->IsEditable() is false),
        // so gStyle->SetOptTitle(1) alone never shows it.
        TLatex* label = new TLatex(0.004, 0.900, cfgLabel(cfg));
        label->SetNDC(true);
        label->SetTextFont(42);
        label->SetTextSize(0.050);
        label->SetTextAlign(12);
        label->Draw();

        TLine* one = new TLine(0., 1., kNBin * kBinW, 1.);
        one->SetLineColor(kGray + 2);
        one->SetLineStyle(2);
        one->SetLineWidth(1);
        one->Draw("SAME");

        TLegend* leg = new TLegend(0.28, 0.825, 0.995, 0.970);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetFillColorAlpha(0., 0.);
        leg->SetTextSize(0.042);
        leg->SetNColumns(4);
        for (int r = 0; r < 4; ++r) {
            std::vector<double> xs, ys, es;
            for (int b = 0; b < kNBin; ++b) {
                if (pro[slot][r][b] <= 0.) continue;
                const double e = det[slot][r][b] / pro[slot][r][b];
                const double en = e / meanEff[slot][r];
                xs.push_back((b + 0.5) * kBinW);
                ys.push_back(en);
                es.push_back(en * std::sqrt(1. / det[slot][r][b] + 1. / pro[slot][r][b]));
            }
            if (xs.empty()) continue;
            TGraphErrors* g = new TGraphErrors((int)xs.size(), xs.data(), ys.data(), nullptr, es.data());
            styleGraph(g, kColor[r], kStyle[r]);
            g->Draw("PL SAME");
            if (slot == 0) leg->AddEntry(g, kReflName[r], "lp");
        }
        if (slot == 0) leg->Draw();

        // Geometry inset for the corner-cut layout: the octagonal slab and
        // the four diagonal-face SiPMs.  A true-scale 4 mm sensor is sub-point
        // at the print width, so the sensor marks are enlarged; the outline
        // and the 119 mm sensor centres are true scale.  The distance used on
        // the x axis is measured to these four centres, not to an edge.
        if (cfg == 12) {
            TPad* ins = new TPad(TString::Format("pad_oct_%d", slot), "",
                                 0.620, 0.360, 0.960, 0.773);
            ins->SetFillColor(kWhite);
            ins->SetFillStyle(1001);
            ins->SetBorderMode(1);
            ins->SetBorderSize(1);
            ins->SetTickx(0);
            ins->SetTicky(0);
            ins->Draw();
            ins->cd();

            TH1D* octFrame = new TH1D(TString::Format("fr_oct_%d", slot), "",
                                      1, -130., 130.);
            octFrame->SetStats(0);
            octFrame->GetXaxis()->SetNdivisions(0);
            octFrame->GetYaxis()->SetNdivisions(0);
            octFrame->GetXaxis()->SetLabelSize(0.);
            octFrame->GetYaxis()->SetLabelSize(0.);
            octFrame->GetXaxis()->SetTickLength(0.);
            octFrame->GetYaxis()->SetTickLength(0.);
            // The pad border is the inset frame: hide the histogram axes
            // entirely (their ticks would otherwise read as a scale).
            octFrame->GetXaxis()->SetAxisColor(kWhite);
            octFrame->GetYaxis()->SetAxisColor(kWhite);
            octFrame->SetLineColor(kWhite);
            octFrame->GetYaxis()->SetRangeUser(-130., 130.);
            octFrame->Draw("AXIS");

            // True octagon outline, closed.
            const double vx[9] = { 125.,  125.,  113., -113., -125.,
                                  -125., -113.,  113.,  125.};
            const double vy[9] = {-113.,  113.,  125.,  125.,  113.,
                                  -113., -125., -125., -113.};
            TGraph* oct = new TGraph(9, vx, vy);
            oct->SetLineColor(kBlack);
            oct->SetLineWidth(2);
            oct->Draw("L SAME");

            // Sensors: short marks on the 45-degree faces, oriented with
            // the face (perpendicular to the inward radial direction).
            for (const Site& s : sites(12)) {
                const double rr = std::sqrt(s.x * s.x + s.y * s.y);
                const double ux = -s.y / rr, uy = s.x / rr;
                const double h = 7.0;      // half-length of the (enlarged) mark
                TLine* ln = new TLine(s.x - h * ux, s.y - h * uy,
                                      s.x + h * ux, s.y + h * uy);
                ln->SetLineColor(kRed);
                ln->SetLineWidth(4);
                ln->Draw("SAME");
                TMarker* mk = new TMarker(s.x, s.y, 20);
                mk->SetMarkerColor(kRed);
                mk->SetMarkerSize(1.0);
                mk->Draw("SAME");
            }
            c->cd(slot + 1);
        }
    }

    c->SaveAs(TString::Format("%s/fig_nearest_sensor.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_nearest_sensor.png", outdir));
    printf("wrote %s/fig_nearest_sensor.{pdf,png} and nearest_sensor.csv\n", outdir);
}
