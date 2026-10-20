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
const int    kNBin = 10;            // 25 mm bins over 0-250 mm
const double kBinW = 25.0;
const int    kCellN = 5;            // 5x5 = 50 mm cells for the reference maps
const double kCellW = 2 * kHalf / kCellN;

const char* kReflName[4] = {"bare", "TiO2", "Al", "glossy"};
const int    kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};
const int    kStyle[4] = {1, 2, 3, 4};

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

    FILE* fmap = fopen(TString::Format("%s/nearest_sensor_maps.txt", outdir), "w");
    if (fmap) fprintf(fmap,
        "# 5x5 grid of 50 mm cell efficiencies (x10^-3) in the report frame\n"
        "# (top = +y, bottom = -y, left = -x, right = +x); row 1 = y = +125..+75\n"
        "# config, reflector, then five rows of five cells\n");

    for (int cfg = 1; cfg <= 4; ++cfg) {
        for (int r = 0; r < 4; ++r) {
            const TString fname =
                TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, cfg, r);
            const std::map<int, Event> ev = readEvents(fname);
            double tot = 0., totD = 0., totP = 0.;
            std::vector<double> cellNum(kCellN * kCellN, 0.);
            std::vector<double> cellDen(kCellN * kCellN, 0.);
            for (const auto& kv : ev) {
                const Event& e = kv.second;
                totD += e.detected;
                totP += e.produced;
                const double d = siteDistance(cfg, e.xr, e.yr);
                int b = std::min(kNBin - 1, std::max(0, int(d / kBinW)));
                det[cfg][r][b] += e.detected;
                pro[cfg][r][b] += e.produced;
                nev[cfg][r][b] += 1;
                const int ix = std::min(kCellN - 1, std::max(0, int((e.xr + kHalf) / kCellW)));
                const int iy = std::min(kCellN - 1, std::max(0, int((e.yr + kHalf) / kCellW)));
                cellNum[iy * kCellN + ix] += e.detected;
                cellDen[iy * kCellN + ix] += e.produced;
            }
            meanEff[cfg][r] = (totP > 0.) ? totD / totP : 0.;
            tot = meanEff[cfg][r];
            printf("cfg%d %-6s events=%4zu  <eps>=%.5e  pe/evt=%.2f\n", cfg,
                   kReflName[r], ev.size(), meanEff[cfg][r],
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
        for (int cfg = 1; cfg <= 4; ++cfg) {
            for (int r = 0; r < 4; ++r) {
                for (int b = 0; b < kNBin; ++b) {
                    const double e = pro[cfg][r][b] > 0. ? det[cfg][r][b] / pro[cfg][r][b] : 0.;
                    const double en = meanEff[cfg][r] > 0. ? e / meanEff[cfg][r] : 0.;
                    fprintf(fp, "%d,%s,%.0f,%.0f,%d,%.6e,%.5f\n", cfg, kReflName[r],
                            b * kBinW, (b + 1) * kBinW, nev[cfg][r][b], e, en);
                }
            }
        }
        fclose(fp);
    }

    // ---- figure ----------------------------------------------------------
    TCanvas* c = new TCanvas("c_nearest", "", 1150, 950);
    c->Divide(2, 2, 0.005, 0.005);

    double ymax = 1.0;
    for (int cfg = 1; cfg <= 4; ++cfg)
        for (int r = 0; r < 4; ++r)
            for (int b = 0; b < kNBin; ++b)
                if (meanEff[cfg][r] > 0. && pro[cfg][r][b] > 0.)
                    ymax = std::max(ymax, det[cfg][r][b] / pro[cfg][r][b] / meanEff[cfg][r]);
    ymax = std::ceil(ymax * 1.06 * 10.) / 10.;

    for (int cfg = 1; cfg <= 4; ++cfg) {
        c->cd(cfg);                       // 1 = top-left, 2 = top-right,
        gStyle->SetOptTitle(1);           // 3 = bottom-left, 4 = bottom-right
        gPad->SetLeftMargin((cfg % 2) ? 0.07 : 0.145);
        gPad->SetRightMargin(0.025);
        gPad->SetBottomMargin(0.13);
        gPad->SetTopMargin(0.17);

        TH1D* frame = new TH1D(TString::Format("fr_near_%d", cfg), "",
                               (int)(kNBin * kBinW), 0., kNBin * kBinW);
        frame->SetTitle(TString::Format("Config %d", cfg));
        frame->GetXaxis()->SetTitle("distance to nearest collection site  d [mm]");
        frame->GetYaxis()->SetTitle("#varepsilon / #langle#varepsilon#rangle");
        frame->GetXaxis()->SetTitleSize(0.042);
        frame->GetYaxis()->SetTitleSize(0.042);
        frame->GetXaxis()->SetLabelSize(0.038);
        frame->GetYaxis()->SetLabelSize(0.038);
        frame->GetXaxis()->SetTitleOffset(1.10);
        frame->GetYaxis()->SetTitleOffset(1.15);
        frame->GetYaxis()->SetRangeUser(0., ymax);
        frame->Draw("HIST");

        TLine* one = new TLine(0., 1., kNBin * kBinW, 1.);
        one->SetLineColor(kGray + 2);
        one->SetLineStyle(2);
        one->SetLineWidth(1);
        one->Draw("SAME");

        TLegend* leg = new TLegend(0.22, 0.875, 0.99, 0.985);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);
        leg->SetFillColorAlpha(0., 0.);
        leg->SetTextSize(0.036);
        leg->SetNColumns(4);
        for (int r = 0; r < 4; ++r) {
            std::vector<double> xs, ys, es;
            for (int b = 0; b < kNBin; ++b) {
                if (pro[cfg][r][b] <= 0.) continue;
                const double e = det[cfg][r][b] / pro[cfg][r][b];
                const double en = e / meanEff[cfg][r];
                xs.push_back((b + 0.5) * kBinW);
                ys.push_back(en);
                es.push_back(en * std::sqrt(1. / det[cfg][r][b] + 1. / pro[cfg][r][b]));
            }
            if (xs.empty()) continue;
            TGraphErrors* g = new TGraphErrors((int)xs.size(), xs.data(), ys.data(), nullptr, es.data());
            styleGraph(g, kColor[r], kStyle[r]);
            g->Draw("PL SAME");
            if (cfg == 1) leg->AddEntry(g, kReflName[r], "lp");
        }
        if (cfg == 1) leg->Draw();
    }

    gStyle->SetOptTitle(0);
    c->SaveAs(TString::Format("%s/fig_nearest_sensor.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_nearest_sensor.png", outdir));
    printf("wrote %s/fig_nearest_sensor.{pdf,png} and nearest_sensor.csv\n", outdir);
}
