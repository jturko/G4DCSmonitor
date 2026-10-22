// matrix_positions.C -- position- and direction-resolved analysis of the
// three-patch matrix scan (centre / edge / corner).
//
// Motivation
// ----------
// The original summary reduced the three irradiation patches to a single
// "distance from the beam axis" (radius) curve.  That is misleading: radius
// is blind to azimuth, yet configs 1/2 mount sensors on all four sides while
// configs 3/4 instrument a single face.  For the single-sided layouts the
// relevant variable is the *signed* separation from the instrumented face,
// and the three patches (which all lie on the half of the plate opposite that
// face) carry a directional meaning that a radius average washes out.
//
// This macro keeps the three positions separate and produces:
//
//   fig_pos_layouts.pdf   efficiency vs {centre,edge,corner}, one panel per
//                         layout, four reflector curves (log y); five layouts
//                         (config 1, 2, 12, 3, 4) on a 2 x 3 grid
//   fig_pos_refl.pdf      efficiency vs {centre,edge,corner}, one panel per
//                         reflector, five layout curves (log y)
//   fig_uniformity.pdf    position contrast max/min across the three patches,
//                         one panel per layout, four reflector bars; five
//                         layouts on a 2 x 3 grid
//   fig_sipm_sharing.pdf  per-sensor share of collected light (direction-
//                         aware), cfg 1, Al vs glossy, three patches
//
// Config 12 (tag c12) is the corner-cut octagon, presented as "Config 2 (mod)"
// (short C2m) alongside configs 1-4.  Its sensors are on the four 45-degree
// faces at (+-119,+-119) mm; this macro plots no per-config sensor markers, so
// only the data/curve/CSV entries are added here.
//
// Report-frame convention (as plot_collection_efficiency.C): top = +y,
// bottom = -y, left = -x, right = +x.  The internal simulation frame for
// configs 1-3 is this view rotated 90 deg clockwise, so entry points are
// mapped through (x,y) -> (y,-x).  Config 4 is already in the report frame.
//
// Usage:
//   root -l -b -q 'matrix_positions.C("slurm/data_output","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLine.h>
#include <TMath.h>
#include <TPad.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions) and the PetroffPalette
// P6 categorical colours.  Located via ROOT_INCLUDE_PATH (set by
// macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};
const int   kStyle[4]    = {kSolid, kDashed, kDotted, kDashDotted};

// Layout order used throughout the report: configs 1, 2, 12 (= "Config 2
// (mod)", short C2m), 3, 4.
const int kCfgOrder[5] = {1, 2, 12, 3, 4};
const int kNConfig     = 5;

TString cfgLabel(int cfg)
{
    return (cfg == 12) ? TString("Config 2 (mod)")
                       : TString::Format("Config %d", cfg);
}
TString cfgShort(int cfg)
{
    return (cfg == 12) ? TString("C2m") : TString::Format("C%d", cfg);
}

// Config-1 SD order (geometry PlaceSiPMs preset): the eight SiPMs are written
// as, for each edge e = 0..3 (+X, -X, +Y, -Y), (e, u=-0.5) then (e, u=+0.5).
// Mapped into the report frame (top = +y, left = -x) via (x,y)->(y,-x) this is:
//   s0 bottom-left  s1 bottom-right  s2 top-left   s3 top-right
//   s4 right-upper  s5 right-lower   s6 left-upper s7 left-lower
// The sharing figure relabels and reorders them clockwise from the top-right
// corner, "1" nearest that corner on each edge, so the x axis reads
// T1 T2 R1 R2 B1 B2 L1 L2 (T/R/B/L = top/right/bottom/left):
//   T1=s3 T2=s2  R1=s5 R2=s4  B1=s0 B2=s1  L1=s6 L2=s7
const int   kShareOrder[8] = {3, 2, 5, 4, 0, 1, 6, 7};   // SD indices, clockwise
const char* kShareLabel[8] = {"T1", "T2", "R1", "R2", "B1", "B2", "L1", "L2"};

const char* kPosName[3]  = {"centre", "edge", "corner"};
const char* kPosLabel[3] = {"centre", "edge", "corner"};

// Sim frame -> report frame, applied to ALL configs (incl. config 4): a 90 deg
// clockwise rotation (x,y) -> (y,-x) (report frame: top=+y, left=-x).
inline void toReportFrame(double& x, double& y)
{
    const double xr = y, yr = -x;
    x = xr; y = yr;
}

struct PosData {
    double prod = 0., coll = 0.;
    double sx = 0., sy = 0.; int nev = 0;
    std::map<int, double> perSipm;
    double eff() const { return prod > 0. ? coll / prod : 0.; }
};

// A three-bin histogram is used purely as a categorical axis frame for the
// centre/edge/corner positions, so the graphs share a labelled x axis.
TH1D* makePosFrame(const char* name, double ylo, double yhi)
{
    TH1D* h = new TH1D(name, "", 3, 0, 3);
    for (int i = 0; i < 3; ++i) h->GetXaxis()->SetBinLabel(i + 1, kPosLabel[i]);
    h->GetYaxis()->SetRangeUser(ylo, yhi);
    // ROOT scales string (bin-label) text by 0.667 (TGaxis.cxx:1504), so a
    // categorical x label needs ~1.5x the numeric y size to render equal.
    h->GetXaxis()->SetLabelSize(0.072);
    h->GetXaxis()->SetTickLength(0.);
    h->GetYaxis()->SetTitleSize(0.048);
    h->GetYaxis()->SetTitleOffset(1.45);
    h->GetYaxis()->SetLabelSize(0.048);
    return h;
}

}  // namespace

void matrix_positions(const char* dir = "slurm/data_output",
                      const char* outdir = "report/figures")
{
    style(0.045);                 // fonts / ticks / title conventions
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);   // no horizontal caps on error bars
    gSystem->mkdir(outdir, kTRUE);

    // PetroffPalette P6 categorical colours, one per reflector
    // (none, TiO2, Al, glossy).
    const int kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};

    std::map<int, std::map<int, std::map<int, PosData>>> data;   // cfg/refl/pos

    for (int ci = 0; ci < kNConfig; ++ci) {
        const int cfg = kCfgOrder[ci];
        for (int r = 0; r < 4; ++r) {
            for (int p = 0; p < 3; ++p) {
                TString fn = TString::Format("%s/muon_out_c%d_r%d_%s.root",
                                             dir, cfg, r, kPosName[p]);
                TFile* f = TFile::Open(fn, "READ");
                if (!f || f->IsZombie()) { delete f; continue; }
                TTree* t = (TTree*)f->Get("sipmHits");
                if (!t) { delete f; continue; }

                int evt = 0, det = 0, sipm = 0, nDet = 0, nProd = 0;
                double mx = 0., my = 0.;
                t->SetBranchAddress("evtNb", &evt);
                t->SetBranchAddress("det", &det);
                t->SetBranchAddress("sipm", &sipm);
                t->SetBranchAddress("nDetected", &nDet);
                t->SetBranchAddress("muonX", &mx);
                t->SetBranchAddress("muonY", &my);
                t->SetBranchAddress("nProduced", &nProd);

                PosData& d = data[cfg][r][p];
                std::set<int> seen;
                for (long i = 0; i < t->GetEntries(); ++i) {
                    t->GetEntry(i);
                    if (det != 0) continue;
                    if (seen.insert(evt).second) {
                        d.prod += nProd;
                        double x = mx, y = my; toReportFrame(x, y);
                        d.sx += x; d.sy += y; ++d.nev;
                    }
                    d.coll += nDet;
                    d.perSipm[sipm] += nDet;
                }
                delete f;
            }
        }
    }

    const char* exts[2] = {"pdf", "png"};
    auto styleGraph = [](TGraphErrors* g, int color, int style) {
        g->SetLineColor(color); g->SetMarkerColor(color);
        g->SetLineStyle(style); g->SetLineWidth(2);
        g->SetMarkerStyle(20); g->SetMarkerSize(1.0);
    };
    auto axisRange = [](double& lo, double& hi, double v) {
        if (v <= 0.) return;
        if (lo <= 0. || v < lo) lo = v;
        if (v > hi) hi = v;
    };

    // A single efficiency range shared by every panel of both position figures,
    // so the tiles can be compared directly.  On the log axis a little headroom
    // is left below the smallest point; the top is raised slightly for the
    // legend-bearing panels later.
    double gLo = 1e30, gHi = 0.;
    for (int ci = 0; ci < kNConfig; ++ci)
        for (int r = 0; r < 4; ++r)
            for (int p = 0; p < 3; ++p)
                axisRange(gLo, gHi, data[kCfgOrder[ci]][r][p].eff());
    if (gLo <= 0.) gLo = 1e-6;
    if (gHi <= 0.) gHi = 1.;
    const double yLo = gLo * 0.5;
    const double yHi = gHi * 2.0;

    // ================= Fig A: efficiency vs position, per layout ===========
    // 2 x 3 grid at the original 525 x 450 px per panel: five layouts plus one
    // empty cell.  The canvas grows to 1050 x 1350 px rather than shrinking the
    // panels.
    {
        TCanvas* c = new TCanvas("c_pos_layouts", "", 1050, 1350);
        c->Divide(2, 3, 0.005, 0.005);
        for (int ci = 0; ci < kNConfig; ++ci) {
            const int cfg = kCfgOrder[ci];
            c->cd(ci + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.20);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.16);
            gPad->SetTopMargin(0.20);
            // Log y on every panel: the bare plate collects ~2 orders of
            // magnitude less than the reflective wrappers, so a linear axis
            // would flatten it onto zero and hide it entirely.
            gPad->SetLogy();

            // Identical range on every tile (shared with Fig B) so the panels
            // can be compared directly.
            TH1D* frame = makePosFrame(TString::Format("fr_layout_%d", cfg), yLo, yHi);
            frame->SetTitle(cfgLabel(cfg));
            frame->GetYaxis()->SetTitle(
                "collection efficiency  #varepsilon = "
                "#font[132]{#it{N}}_{pe}/#font[132]{#it{N}}_{#gamma}");
            frame->GetXaxis()->SetLabelSize(0.040);
            frame->GetYaxis()->SetLabelSize(0.040);
            frame->GetXaxis()->SetTitleSize(0.045);
            frame->GetYaxis()->SetTitleSize(0.045);
            frame->GetXaxis()->SetTitleOffset(1.10);
            frame->GetYaxis()->SetTitleOffset(1.10);
            frame->Draw("HIST");

            // Shared legend drawn only on the Config 1 (top-left) tile,
            // placed in the top-margin strip (above the data) so it never
            // sits over the plotted curves.  2 columns keep the strip shallow.
            TLegend* leg = new TLegend(0.20, 0.83, 0.97, 0.985);
            leg->SetBorderSize(0); leg->SetFillStyle(0);
            leg->SetFillColorAlpha(0., 0.);
            leg->SetTextSize(0.040);
            leg->SetNColumns(4);
            for (int r = 0; r < 4; ++r) {
                std::vector<double> x, y, ey;
                for (int p = 0; p < 3; ++p) {
                    const PosData& d = data[cfg][r][p];
                    if (d.prod <= 0.) continue;
                    x.push_back(p + 0.5); y.push_back(d.eff());
                    ey.push_back(d.coll > 0 ? std::sqrt(d.coll) / d.prod : 0.);
                }
                TGraphErrors* g = new TGraphErrors((int)x.size(), x.data(), y.data(), nullptr, ey.data());
                styleGraph(g, kColor[r], kStyle[r]);
                g->Draw("PL SAME");
                leg->AddEntry(g, kReflName[r], "lp");
            }
            if (cfg == 1) leg->Draw();
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_pos_layouts.%s", outdir, ex));
    }

    // ================= Fig B: efficiency vs position, per reflector ========
    {
        TCanvas* c = new TCanvas("c_pos_refl", "", 1050, 900);
        c->Divide(2, 2, 0.005, 0.005);
        for (int r = 0; r < 4; ++r) {
            c->cd(r + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.20);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.16);
            gPad->SetTopMargin(0.20);
            gPad->SetLogy();

            // Same shared range as Fig A, so the reflector panels are also
            // directly comparable to the layout panels.
            TH1D* frame = makePosFrame(TString::Format("fr_refl_%d", r), yLo, yHi);
            frame->SetTitle(kReflName[r]);
            frame->GetYaxis()->SetTitle(
                "collection efficiency  #varepsilon = "
                "#font[132]{#it{N}}_{pe}/#font[132]{#it{N}}_{#gamma}");
            frame->GetXaxis()->SetLabelSize(0.040);
            frame->GetYaxis()->SetLabelSize(0.040);
            frame->GetXaxis()->SetTitleSize(0.045);
            frame->GetYaxis()->SetTitleSize(0.045);
            frame->GetXaxis()->SetTitleOffset(1.10);
            frame->GetYaxis()->SetTitleOffset(1.10);
            frame->Draw("HIST");
            gPad->SetLogy();

            // Shared legend drawn only on the bare (top-left) tile, in the
            // top-margin strip to avoid covering the lower config 2/3/4 traces.
            // Five curves (1, 2, 12, 3, 4) in a 3-column x 2-row legend, kept
            // inside the 0.20 top strip but BELOW the panel title (the old
            // 0.82-0.985 box overlapped it).  Colours are P6(0..3) for configs
            // 1-4 and P6(4) for config 12; the latter also gets a square marker
            // so it stays distinguishable from config 1 (both solid).
            TLegend* leg = new TLegend(0.20, 0.805, 0.97, 0.925);
            leg->SetBorderSize(0); leg->SetFillStyle(0);
            leg->SetFillColorAlpha(0., 0.);
            leg->SetTextSize(0.034);
            leg->SetNColumns(3);
            const int cfgColor[5]  = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                                      PetroffPalette::getP6(4), PetroffPalette::getP6(2),
                                      PetroffPalette::getP6(3)};
            const int cfgStyle[5]  = {kSolid, kDashed, kSolid, kDotted, kDashDotted};
            const int cfgMarker[5] = {20, 20, 21, 20, 20};
            for (int ci = 0; ci < kNConfig; ++ci) {
                const int cfg = kCfgOrder[ci];
                std::vector<double> x, y, ey;
                for (int p = 0; p < 3; ++p) {
                    const PosData& d = data[cfg][r][p];
                    if (d.prod <= 0.) continue;
                    x.push_back(p + 0.5); y.push_back(d.eff());
                    ey.push_back(d.coll > 0 ? std::sqrt(d.coll) / d.prod : 0.);
                }
                TGraphErrors* g = new TGraphErrors((int)x.size(), x.data(), y.data(), nullptr, ey.data());
                styleGraph(g, cfgColor[ci], cfgStyle[ci]);
                g->SetMarkerStyle(cfgMarker[ci]);
                g->Draw("PL SAME");
                leg->AddEntry(g, cfgShort(cfg), "lp");
            }
            if (r == 0) leg->Draw();
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_pos_refl.%s", outdir, ex));
    }

    // ================= Fig C: position contrast (uniformity) ===============
    // 2 x 3 grid, five layouts + one empty cell (1050 x 1350 px keeps the
    // original 525 x 450 px panels).
    {
        TCanvas* c = new TCanvas("c_uniformity", "", 1050, 1350);
        c->Divide(2, 3, 0.005, 0.005);
        for (int ci = 0; ci < kNConfig; ++ci) {
            const int cfg = kCfgOrder[ci];
            c->cd(ci + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.18);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.18);
            gPad->SetTopMargin(0.12);

            TH1D* h = new TH1D(TString::Format("hu_c%d", cfg), "", 4, 0, 4);
            double ymax = 0.;
            for (int r = 0; r < 4; ++r) {
                double emax = 0., emin = 1e30;
                for (int p = 0; p < 3; ++p) {
                    const PosData& d = data[cfg][r][p];
                    if (d.prod <= 0.) continue;
                    emax = std::max(emax, d.eff());
                    emin = std::min(emin, d.eff());
                }
                double ratio = (emin > 0.) ? emax / emin : 0.;
                h->SetBinContent(r + 1, ratio);
                h->GetXaxis()->SetBinLabel(r + 1, kReflName[r]);
                ymax = std::max(ymax, ratio);
            }
            h->SetTitle(cfgLabel(cfg));
            h->GetYaxis()->SetTitle(
                "position contrast  #kappa = "
                "#font[132]{#it{#varepsilon}}_{max}/"
                "#font[132]{#it{#varepsilon}}_{min}");
            h->GetYaxis()->SetTitleSize(0.045);
            h->GetYaxis()->SetTitleOffset(1.20);
            h->GetYaxis()->SetLabelSize(0.045);
            h->GetXaxis()->SetLabelSize(0.065);
            h->GetYaxis()->SetRangeUser(0, std::max(1.2, ymax * 1.18));
            h->SetBarWidth(0.7); h->SetBarOffset(0.15);
            h->SetFillColor(kAzure);
            h->SetLineColor(kBlack);
            h->Draw("BAR");
            gPad->SetGridy();
            TLine* one = new TLine(0, 1, 4, 1);
            one->SetLineStyle(2); one->SetLineColor(kGray + 2);
            one->Draw("SAME");
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_uniformity.%s", outdir, ex));
    }

    // ================= Fig D: per-sensor light sharing (cfg 1) =============
    // For each irradiation patch the collected light is split across the eight
    // sensors.  This is the direction-aware view a radius summary cannot give:
    // the share pattern rotates with the patch.  One panel per reflector
    // (config 1, the only layout with enough sensors for an informative split);
    // within a panel the eight sensors are on the x axis and the three
    // irradiation patches are grouped bars, so the nearest-sensor concentration
    // and the growth of uniformity with better wrappers are read directly off
    // the bar heights.  The dashed line marks the uniform expectation of 1/8.
    {
        const int patchColor[3] = {PetroffPalette::getP6(0),
                                   PetroffPalette::getP6(1),
                                   PetroffPalette::getP6(2)};

        TCanvas* c = new TCanvas("c_sharing", "", 1100, 920);
        c->Divide(2, 2, 0.005, 0.005);
        for (int r = 0; r < 4; ++r) {
            c->cd(r + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.12);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.13);
            gPad->SetTopMargin(0.12);

            // y range: a little above the largest share in this panel.
            double ymax = 1. / 8.;
            for (int p = 0; p < 3; ++p) {
                const PosData& d = data[1][r][p];
                double tot = 0.; for (auto& kv : d.perSipm) tot += kv.second;
                for (auto& kv : d.perSipm)
                    ymax = std::max(ymax, (tot > 0.) ? kv.second / tot : 0.);
            }
            ymax *= 1.15;

            // Only the top-right panel (r == 1, TiO2) carries the legend.  In
            // that pad the two leftmost sensor slots (T1, T2) are short, so the
            // legend sits in the top-left corner; its right edge (NDC 0.38) is
            // kept left of the tall corner bar on R1 (pad-x ~0.40).
            TLegend* leg = (r == 1)
                ? new TLegend(0.15, 0.64, 0.38, 0.86) : nullptr;
            if (leg) {
                leg->SetBorderSize(0); leg->SetFillStyle(0);
                leg->SetFillColorAlpha(0., 0.);
                leg->SetTextSize(0.030);
            }

            for (int p = 0; p < 3; ++p) {
                TH1D* h = new TH1D(TString::Format("hsh_%d_%d", r, p), "", 8, 0, 8);
                const PosData& d = data[1][r][p];
                double tot = 0.; for (auto& kv : d.perSipm) tot += kv.second;
                for (int b = 0; b < 8; ++b) {
                    // Display the sensors reordered clockwise (T1 T2 R1 R2 B1 B2
                    // L1 L2) rather than in raw SD index order.
                    const int s = kShareOrder[b];
                    auto it = d.perSipm.find(s);
                    double v = (it != d.perSipm.end()) ? it->second : 0.;
                    double sh = (tot > 0.) ? v / tot : 0.;
                    h->SetBinContent(b + 1, sh);
                    // No SetBinError: allocating Sumw2 makes THistPainter draw
                    // error bars automatically ("if (fH->GetSumw2N() &&
                    // hdim==1) Hoption.Error = 2"), unwanted on these bars.
                    h->GetXaxis()->SetBinLabel(b + 1, kShareLabel[b]);
                }
                h->SetFillColor(patchColor[p]);
                h->SetFillStyle(1001);
                h->SetLineColor(kBlack);
                h->SetLineWidth(1);
                h->SetBarWidth(0.27);
                h->SetBarOffset(0.05 + 0.30 * p);
                if (p == 0) {
                    h->SetTitle(TString::Format("%s (config 1)", kReflName[r]));
                    h->GetYaxis()->SetTitle("share of collected light");
                    h->GetXaxis()->SetTitle("SiPM position");
                    h->GetYaxis()->SetTitleSize(0.048);
                    h->GetYaxis()->SetTitleOffset(1.15);
                    h->GetYaxis()->SetLabelSize(0.042);
                    h->GetXaxis()->SetTitleSize(0.048);
                    h->GetXaxis()->SetLabelSize(0.055);
                    h->GetYaxis()->SetRangeUser(0, ymax);
                    h->Draw("BAR");
                } else {
                    h->Draw("BAR SAME");
                }
                if (leg) leg->AddEntry(h, TString::Format("%s beam", kPosLabel[p]), "f");
            }
            if (leg) leg->Draw();
            // uniform 1/8 expectation
            TLine* u = new TLine(0, 1. / 8., 8, 1. / 8.);
            u->SetLineColor(kGray + 2); u->SetLineStyle(2); u->SetLineWidth(1);
            u->Draw("SAME");
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_sipm_sharing.%s", outdir, ex));
    }

    printf("matrix_positions: wrote figures to %s\n", outdir);

    // Plain-text dump so the report can quote exact values.
    FILE* fp = fopen(TString::Format("%s/position_efficiency.csv", outdir).Data(), "w");
    if (fp) {
        // Config 12 (C2m) rows are written with config = 12, in the report
        // layout order 1, 2, 12, 3, 4; the `label` column carries the human
        // name ("Config 2 (mod)" for 12), as in whole_yield.csv.
        fprintf(fp, "config,label,reflector,position,produced,collected,efficiency,contrast\n");
        for (int ci = 0; ci < kNConfig; ++ci) {
            const int cfg = kCfgOrder[ci];
            for (int r = 0; r < 4; ++r) {
                double emax = 0., emin = 1e30;
                for (int p = 0; p < 3; ++p) {
                    const PosData& d = data[cfg][r][p];
                    if (d.prod > 0.) { emax = std::max(emax, d.eff()); emin = std::min(emin, d.eff()); }
                }
                double ctr = (emin > 0.) ? emax / emin : 0.;
                for (int p = 0; p < 3; ++p) {
                    const PosData& d = data[cfg][r][p];
                    fprintf(fp, "%d,%s,%s,%s,%.6g,%.6g,%.6g,%.4f\n",
                            cfg, cfgLabel(cfg).Data(), kReflName[r], kPosName[p],
                            d.prod, d.coll, d.eff(), ctr);
                }
            }
        }
        fclose(fp);
    }
}
