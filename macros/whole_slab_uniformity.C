// whole_slab_uniformity.C -- position-dependence analysis of the whole-slab
// (full-plate) irradiation, the full-plate analogue of the three-patch
// position contrast in matrix_positions.C.
//
// The whole-slab GPS beam irradiates the entire 250 x 250 mm^2 plate uniformly,
// so every primary-muon entry point samples the plate.  The SiPM layouts have
// NO continuous rotational symmetry (configs 1-2 are four-fold, configs 3-4 are
// single-sided), and the plate is square, so the efficiency is a function of
// BOTH radius and azimuth.  A radially-averaged contrast would be a geometric
// artefact (for r > 125 mm only the four corner wedges of the plate are
// populated, so a radial bin mixes a shrinking azimuth range).  We therefore do
// NOT use a radial metric.  Instead:
//
//   kappa_2d   2-D cell contrast: on a 5 x 5 grid of 50 mm cells,
//              kappa_2d = max_cell(eps) / min_cell(eps).  No symmetry assumed.
//   polar map  eps(r, phi) resolved in radius AND azimuth, so the directional
//              structure a radial average would destroy stays visible.  The
//              instrumented azimuths (SiPM positions, or the WLS clamp band)
//              are marked on each panel.
//
// Figures (all in the report frame, top = +y, left = -x):
//   fig_whole_polar.pdf          eps(r, phi) maps, rows = layout, columns =
//                                reflector (shared log z); instrumented
//                                azimuths marked.
//   fig_whole_uniformity_map_cfg<C>.pdf  normalised eps/<eps> maps, one figure
//                                per config, one panel per reflector (10 mm
//                                cells), with the slab outline and SiPM / WLS
//                                markers overlaid as in plot_whole_slab.C
//   whole_uniformity.csv         the kappa_2d numbers quoted in the report
//
// Usage:
//   root -l -b -q 'whole_slab_uniformity.C("slurm/data_output_whole","report/figures")'

#include <TBox.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TLine.h>
#include <TMath.h>
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
#include <map>
#include <numeric>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, title conventions) and the PetroffPalette
// P6 categorical colours.  Located via ROOT_INCLUDE_PATH (set by
// macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};

// Sim frame -> report frame, applied to all configs: 90 deg clockwise,
// (x,y) -> (y,-x) (report frame: top = +y, left = -x).
inline void toReportFrame(double& x, double& y)
{
    const double xr = y, yr = -x;
    x = xr; y = yr;
}

struct Event { double r, x, y, prod, coll; };

struct Result {
    // 2-D cell contrast (no symmetry assumed)
    double kappaCell = 0., kappaCellErr = 0.;
    double meanEff = 0.;                   // area-weighted <eps>
};

// Interior bin edges spanning exactly [-half, half] in nIn equal cells.
std::vector<double> makeEdges(int nIn, double half)
{
    std::vector<double> e;
    e.reserve(nIn + 1);
    for (int k = 0; k <= nIn; ++k) e.push_back(-half + 2.0 * half * k / nIn);
    return e;
}

// Map edges with an extra partial bin on each side out to +/-span, so the
// physical SiPM / WLS-clamp markers drawn just outside the slab (see
// drawOverlays) stay inside the frame.  Empty padding bins render as
// background, exactly as in plot_whole_slab.C.
std::vector<double> makeMapEdges(double span, double half, int nIn)
{
    const double step = 2.0 * half / nIn;
    std::vector<double> e;
    e.reserve(nIn + 3);
    e.push_back(-span);
    for (int k = 0; k <= nIn; ++k) e.push_back(-half + k * step);
    e.push_back(span);
    return e;
}

// SiPM tile centres (report frame, top = +y, left = -x); same layout as
// plot_whole_slab.C / sipm_layouts.C.  Config 4 is the WLS fibre clamp.
std::vector<std::pair<double, double>> sipmTiles(int cfg, double half)
{
    const double H = half;
    const double u = 0.5 * H;
    std::vector<std::pair<double, double>> t;
    switch (cfg) {
        case 1:
            t = {{ H, -u}, { H,  u}, {-H, -u}, {-H,  u},
                 {-u,  H}, { u,  H}, {-u, -H}, { u, -H}};
            break;
        case 2:
            t = {{ H, -u}, {-H,  u}, { u,  H}, {-u, -H}};
            break;
        case 3:
            t = {{-u, H}, { u, H}};   // both on the top (+y) edge
            break;
        case 4:
            t = {{-H, H}};            // read-out SiPM at the -x end of the +y bar
            break;
    }
    return t;
}

// Slab outline + SiPM markers (black), or for config 4 the full-length WLS
// fibre clamp (red bar, top +y edge) with its single read-out SiPM (black) at
// the top-left.  Identical convention to plot_whole_slab.C::drawOverlays, so
// the uniformity maps match Fig. 21 of the report.
void drawOverlays(int cfg, double half, double side = 8.0, double gap = 2.0)
{
    TBox* slab = new TBox(-half, -half, half, half);
    slab->SetFillStyle(0);
    slab->SetLineColor(kBlack);
    slab->SetLineWidth(2);
    slab->Draw("SAME");

    if (cfg == 4) {
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

// Poisson error on a ratio sum_coll / sum_prod.
inline double ratioErr(double coll, double prod)
{
    return prod > 0. ? std::sqrt(std::max(coll, 0.)) / prod : 0.;
}

// kappa = emax/emin with propagated error; emax/emin over the provided pairs.
void contrast(const std::vector<double>& eff, const std::vector<double>& err,
              double& kappa, double& kappaErr)
{
    double emax = 0., emin = 1e30, ermax = 0., ermin = 0.;
    for (size_t i = 0; i < eff.size(); ++i) {
        if (eff[i] <= 0.) continue;
        if (eff[i] > emax) { emax = eff[i]; ermax = err[i]; }
        if (eff[i] < emin) { emin = eff[i]; ermin = err[i]; }
    }
    if (emin <= 0. || emin >= 1e30) { kappa = 0.; kappaErr = 0.; return; }
    kappa = emax / emin;
    const double a = (emax > 0.) ? ermax / emax : 0.;
    const double b = (emin > 0.) ? ermin / emin : 0.;
    kappaErr = kappa * std::sqrt(a * a + b * b);
}

// Azimuth (deg) of every SiPM tile for a layout, for the polar-map overlay.
// Config 4 returns the single read-out SiPM; the clamp band is handled
// separately in the figure.
std::vector<double> sipmAzimuths(int cfg, double half)
{
    std::vector<double> az;
    for (const auto& t : sipmTiles(cfg, half))
        az.push_back(std::atan2(t.second, t.first) * 180.0 / TMath::Pi());
    return az;
}

}  // namespace

void whole_slab_uniformity(const char* dir = "slurm/data_output_whole",
                           const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);
    gSystem->mkdir(outdir, kTRUE);

    const double half = 125.0;
    const double rMax = half * std::sqrt(2.0);
    const int    kPolR    = 12;               // radial bins for the polar map
    const int    kPolPhi  = 12;               // 30-deg azimuth bins for the polar map
    const int    kCellN   = 5;                // 5x5 = 50 mm cells for kappa_2d
    const int    kMapN    = 50;               // 50x50 = 5 mm cells (match plot_whole_slab.C)
    const double mapSpan  = half + 18.0;      // pad frame so outboard markers fit
    const std::vector<double> cellEdges = makeEdges(kCellN, half);
    const std::vector<double> mapEdges  = makeMapEdges(mapSpan, half, kMapN);
    const int    nbMap   = (int)mapEdges.size() - 1;   // interior + 2 pad bins

    std::map<int, std::map<int, Result>> res;                 // cfg -> refl -> result
    std::map<int, std::map<int, TH2D*>> normMap;              // cfg -> refl -> eps/<eps>
    std::map<int, std::map<int, TH2D*>> polMap;               // cfg -> refl -> eps(r,phi)

    for (int cfg = 1; cfg <= 4; ++cfg) {
        for (int r = 0; r < 4; ++r) {
            TString fn = TString::Format("%s/muon_whole_out_c%d_r%d.root", dir, cfg, r);
            TFile* f = TFile::Open(fn, "READ");
            if (!f || f->IsZombie()) { delete f; continue; }
            TTree* t = (TTree*)f->Get("sipmHits");
            if (!t) { delete f; continue; }

            int evt = 0, det = 0, nDet = 0, nProd = 0;
            double mx = 0., my = 0.;
            t->SetBranchAddress("evtNb", &evt);
            t->SetBranchAddress("det", &det);
            t->SetBranchAddress("nDetected", &nDet);
            t->SetBranchAddress("muonX", &mx);
            t->SetBranchAddress("muonY", &my);
            t->SetBranchAddress("nProduced", &nProd);

            std::map<int, Event> em;
            for (long i = 0; i < t->GetEntries(); ++i) {
                t->GetEntry(i);
                if (det != 0) continue;
                auto it = em.find(evt);
                if (it == em.end()) {
                    double x = mx, y = my; toReportFrame(x, y);
                    em[evt] = {std::sqrt(x * x + y * y), x, y, (double)nProd, 0.};
                }
                em[evt].coll += nDet;
            }
            delete f;

            std::vector<Event> ev;
            ev.reserve(em.size());
            for (auto& kv : em) ev.push_back(kv.second);
            if (ev.empty()) continue;

            Result& R = res[cfg][r];

            // 2-D cell contrast on a coarse grid.
            TH2D hc(TString::Format("hc_%d_%d", cfg, r), "",
                    kCellN, cellEdges.data(), kCellN, cellEdges.data());
            TH2D hp(TString::Format("hp_%d_%d", cfg, r), "",
                    kCellN, cellEdges.data(), kCellN, cellEdges.data());
            double totProd = 0., totColl = 0.;
            for (auto& e : ev) {
                hc.Fill(e.x, e.y, e.coll);
                hp.Fill(e.x, e.y, e.prod);
                totProd += e.prod; totColl += e.coll;
            }
            R.meanEff = totProd > 0. ? totColl / totProd : 0.;
            std::vector<double> ce, cee;
            for (int bx = 1; bx <= kCellN; ++bx)
                for (int by = 1; by <= kCellN; ++by) {
                    const double p = hp.GetBinContent(bx, by);
                    if (p <= 0.) continue;
                    const double c = hc.GetBinContent(bx, by);
                    ce.push_back(c / p);
                    cee.push_back(ratioErr(c, p));
                }
            contrast(ce, cee, R.kappaCell, R.kappaCellErr);

            // Normalised 2-D map at 10 mm, for every config.
            {
                TH2D* hp2 = new TH2D(TString::Format("hp2_%d_%d", cfg, r), "produced",
                                     nbMap, mapEdges.data(), nbMap, mapEdges.data());
                TH2D* hc2 = new TH2D(TString::Format("hc2_%d_%d", cfg, r), "collected",
                                     nbMap, mapEdges.data(), nbMap, mapEdges.data());
                for (auto& e : ev) { hp2->Fill(e.x, e.y, e.prod); hc2->Fill(e.x, e.y, e.coll); }
                TH2D* hn = (TH2D*)hc2->Clone(TString::Format("hn_%d_%d", cfg, r));
                hn->SetDirectory(nullptr);
                hn->Divide(hp2);
                if (R.meanEff > 0.)
                    for (int bx = 1; bx <= nbMap; ++bx)
                        for (int by = 1; by <= nbMap; ++by) {
                            const double v = hn->GetBinContent(bx, by);
                            hn->SetBinContent(bx, by, v / R.meanEff);
                        }
                hn->SetTitle(TString::Format("%s;x [mm];y [mm]", kReflName[r]));
                normMap[cfg][r] = hn;
                delete hp2; delete hc2;
            }

            // Polar eps(r, phi): radius AND azimuth, so the directional
            // structure a radial average would wash out stays visible.
            {
                TH2D* hpP = new TH2D(TString::Format("hpP_%d_%d", cfg, r), "produced",
                                     kPolR, 0., rMax, kPolPhi, -180., 180.);
                TH2D* hcP = new TH2D(TString::Format("hcP_%d_%d", cfg, r), "collected",
                                     kPolR, 0., rMax, kPolPhi, -180., 180.);
                for (auto& e : ev) {
                    const double phi = std::atan2(e.y, e.x) * 180.0 / TMath::Pi();
                    hpP->Fill(e.r, phi, e.prod);
                    hcP->Fill(e.r, phi, e.coll);
                }
                TH2D* hP = (TH2D*)hcP->Clone(TString::Format("hP_%d_%d", cfg, r));
                hP->SetDirectory(nullptr);
                hP->Divide(hpP);
                hP->SetTitle(TString::Format("Config %d: %s;entry radius r [mm];azimuth #phi [deg]",
                                             cfg, kReflName[r]));
                for (int bx = 1; bx <= kPolR; ++bx)
                    for (int by = 1; by <= kPolPhi; ++by)
                        if (hpP->GetBinContent(bx, by) <= 0.) hP->SetBinContent(bx, by, 0.);
                polMap[cfg][r] = hP;
                delete hpP; delete hcP;
            }

            printf("cfg%d %-6s  <eps>=%.4e  kappa_2d=%.3f+-%.3f\n",
                   cfg, kReflName[r], R.meanEff, R.kappaCell, R.kappaCellErr);
        }
    }

    const char* exts[2] = {"pdf", "png"};

    // ============== Fig P: polar eps(r,phi) maps ===========================
    // Rows = layout, columns = reflector, shared log z.  The instrumented
    // azimuths are marked on every panel.
    {
        double zLo = 1e30, zHi = 0.;
        for (auto& c : polMap)
            for (auto& rr : c.second) {
                TH2D* h = rr.second;
                for (int bx = 1; bx <= h->GetNbinsX(); ++bx)
                    for (int by = 1; by <= h->GetNbinsY(); ++by) {
                        const double v = h->GetBinContent(bx, by);
                        if (v > 0.) { zLo = std::min(zLo, v); zHi = std::max(zHi, v); }
                    }
            }
        if (zLo >= 1e30) zLo = 1e-6;
        if (zHi <= 0.)  zHi = 1.;
        TCanvas* c = new TCanvas("c_ws_polar", "", 1500, 1300);
        c->Divide(4, 4, 0.001, 0.001);
        for (int cfg = 1; cfg <= 4; ++cfg) {
            for (int r = 0; r < 4; ++r) {
                if (!polMap[cfg].count(r)) continue;
                c->cd((cfg - 1) * 4 + r + 1);
                gStyle->SetOptTitle(1);
                gPad->SetLeftMargin(0.16);
                gPad->SetRightMargin(0.16);
                gPad->SetBottomMargin(0.16);
                gPad->SetTopMargin(0.14);
                gPad->SetLogz();
                TH2D* h = polMap[cfg][r];
                h->GetZaxis()->SetTitle("#varepsilon");
                h->GetZaxis()->SetTitleSize(0.05);
                h->GetZaxis()->SetTitleOffset(1.2);
                h->GetZaxis()->SetLabelSize(0.042);
                h->GetZaxis()->SetMaxDigits(3);
                h->GetXaxis()->SetTitleSize(0.05);
                h->GetYaxis()->SetTitleSize(0.05);
                h->GetXaxis()->SetLabelSize(0.045);
                h->GetYaxis()->SetLabelSize(0.045);
                h->SetMinimum(zLo);
                h->SetMaximum(zHi);
                h->Draw("COLZ");
                if (cfg == 4) {
                    // WLS clamp runs along the +y edge (phi 45..135); the
                    // read-out SiPM sits at the top-left corner (phi = 135).
                    TBox* band = new TBox(0., 45., rMax, 135.);
                    band->SetFillColor(kRed);
                    band->SetFillStyle(3004);
                    band->Draw("SAME");
                    TLine* ro = new TLine(0., 135., rMax, 135.);
                    ro->SetLineColor(kRed); ro->SetLineStyle(1); ro->SetLineWidth(2);
                    ro->Draw("SAME");
                } else {
                    for (double az : sipmAzimuths(cfg, half)) {
                        TLine* l = new TLine(0., az, rMax, az);
                        l->SetLineColor(kGray + 2); l->SetLineStyle(2); l->SetLineWidth(1);
                        l->Draw("SAME");
                    }
                }
            }
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_whole_polar.%s", outdir, ex));
    }

    // ============== Fig M: normalised uniformity maps (one per config) ======
    // SHARED LOGARITHMIC z range across all configs so the panels stay
    // comparable.  A linear range cannot hold the bare plate, whose response
    // spans several decades about its mean (edge-mid maximum / corner
    // minimum), while the reflective wrappers sit in a narrow band around
    // unity; the log scale resolves both.
    double gzLo = 1e30, gzHi = 0.;
    for (int cfg = 1; cfg <= 4; ++cfg)
        for (auto& rr : normMap[cfg]) {
            TH2D* h = rr.second;
            for (int bx = 1; bx <= h->GetNbinsX(); ++bx)
                for (int by = 1; by <= h->GetNbinsY(); ++by) {
                    const double v = h->GetBinContent(bx, by);
                    if (v <= 0.) continue;
                    gzLo = std::min(gzLo, v);
                    gzHi = std::max(gzHi, v);
                }
        }
    if (gzLo >= 1e30) gzLo = 1e-2;
    if (gzHi <= 0.)  gzHi = 2.;
    printf("normalised maps: eps/<eps> in [%.4g, %.4g]  ->  log z range [%.3g, %.3g]\n",
           gzLo, gzHi, gzLo, gzHi);
    for (int cfg = 1; cfg <= 4; ++cfg) {
        TCanvas* c = new TCanvas(TString::Format("c_ws_map_%d", cfg), "", 1150, 980);
        c->Divide(2, 2, 0.006, 0.006);
        for (int r = 0; r < 4; ++r) {
            if (!normMap[cfg].count(r)) continue;
            c->cd(r + 1);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.13);
            gPad->SetRightMargin(0.16);
            gPad->SetBottomMargin(0.13);
            gPad->SetTopMargin(0.12);
            TH2D* h = normMap[cfg][r];
            h->SetTitle(TString::Format("Config %d: %s;x [mm];y [mm]",
                                        cfg, kReflName[r]));
            h->GetZaxis()->SetTitle("#varepsilon / #LT#varepsilon#GT");
            h->GetZaxis()->SetTitleSize(0.038);
            h->GetZaxis()->SetTitleOffset(1.35);
            h->GetZaxis()->SetLabelSize(0.034);
            h->GetZaxis()->SetMaxDigits(3);
            h->GetXaxis()->SetTitleSize(0.040);
            h->GetYaxis()->SetTitleSize(0.040);
            h->GetXaxis()->SetLabelSize(0.034);
            h->GetYaxis()->SetLabelSize(0.034);
            gPad->SetLogz();
            h->SetMinimum(gzLo); h->SetMaximum(gzHi);
            h->Draw("COLZ");
            drawOverlays(cfg, half);
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_whole_uniformity_map_cfg%d.%s",
                                      outdir, cfg, ex));
    }

    // ============== Fig K: kappa_2D bar grid ===============================
    // The full-slab analogue of the three-patch contrast histogram: a 2 x 2
    // grid (one panel per layout), each panel a 4-bin bar chart of the
    // position contrast kappa_2D = eps_max/eps_min over the full plate, one bar
    // per reflector.  Linear y, each panel scaled to its own maximum, as in the
    // original matrix_positions.C three-patch figure.
    {
        TCanvas* c = new TCanvas("c_ws_kappa", "", 1050, 900);
        c->Divide(2, 2, 0.005, 0.005);
        for (int cfg = 1; cfg <= 4; ++cfg) {
            c->cd(cfg);
            gStyle->SetOptTitle(1);
            gPad->SetLeftMargin(0.15);
            gPad->SetRightMargin(0.04);
            gPad->SetBottomMargin(0.16);
            gPad->SetTopMargin(0.12);

            TH1D* h = new TH1D(TString::Format("hk_c%d", cfg), "", 4, 0, 4);
            double ymax = 0.;
            for (int r = 0; r < 4; ++r) {
                const Result& R = res[cfg][r];
                h->SetBinContent(r + 1, R.kappaCell);
                h->SetBinError(r + 1, R.kappaCellErr);
                h->GetXaxis()->SetBinLabel(r + 1, kReflName[r]);
                ymax = std::max(ymax, R.kappaCell);
            }
            h->SetTitle(TString::Format("Config %d", cfg));
            h->GetYaxis()->SetTitle("#kappa_{2D} = #varepsilon_{max}/#varepsilon_{min}");
            h->GetYaxis()->SetTitleSize(0.050);
            h->GetYaxis()->SetTitleOffset(1.25);
            h->GetYaxis()->SetLabelSize(0.044);
            h->GetXaxis()->SetLabelSize(0.070);
            h->GetYaxis()->SetRangeUser(0., std::max(1.2, ymax * 1.18));
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
            c->SaveAs(TString::Format("%s/fig_whole_contrast.%s", outdir, ex));
    }

    // ============== CSV ====================================================
    FILE* fp = fopen(TString::Format("%s/whole_uniformity.csv", outdir).Data(), "w");
    if (fp) {
        fprintf(fp, "config,reflector,mean_eff,kappa_2d,kappa_2d_err\n");
        for (int cfg = 1; cfg <= 4; ++cfg)
            for (int r = 0; r < 4; ++r) {
                const Result& R = res[cfg][r];
                fprintf(fp, "%d,%s,%.6g,%.4f,%.4f\n",
                        cfg, kReflName[r], R.meanEff, R.kappaCell, R.kappaCellErr);
            }
        fclose(fp);
    }

    printf("whole_slab_uniformity: wrote figures + whole_uniformity.csv to %s\n", outdir);
}
