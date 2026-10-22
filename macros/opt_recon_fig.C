// opt_recon_fig.C -- position-reconstruction resolution of the v7_opt detector
// designs, and the resolution-versus-yield trade-off.
//
// Companion of macros/run_opt_recon.sh, which runs the proven
// position_reconstruction.C+ on one scratch directory per design and stores
// the r0 ("bare") rows of each position_reconstruction.csv in a combined raw
// table:
//
//   tag,cfg,preset,wrapper,estimator,neighbours,ntrain,ntest,
//   sigma_x_mm,sigma_y_mm,sigma_r_mm,bias_x_mm,bias_y_mm,r68_mm
//
// All four wrapper blocks in the per-design CSV are the same file (the driver
// symlinks one campaign file to r0..r3), so only r0 is carried over.  The
// five estimators are, in the order position_reconstruction.C writes them:
//
//   pulse integral | first arrival | optical CFD (12.5%) |
//   prompt fraction | combined
//
// Outputs (in outdir, default report/figures):
//   opt_recon.csv        one row per design: tag,cfg,preset,ntrain,ntest and
//                        sigma_x/sigma_y/sigma_r/r68 for all five estimators
//   fig_opt_recon.pdf    (a) combined sigma_x per design, with the two
//   fig_opt_recon.png        same-layout C1 references (v6p5: glossy 15.9,
//                            TiO2 11.9 mm) as guides
//                        (b) sigma_x versus mean photoelectrons/event (log x),
//                            filled = v7_opt design, faint horizontal lines =
//                            the same two C1 references (no yield pairing),
//                            grey line = Pareto lower envelope
//
// Light yields are read from opt_yield.csv (tag -> mean_pe_per_event), the
// same file opt_de.C / opt_yield.C write.  A design with no rows in the raw
// table (missing/unreadable input, or the reconstruction was skipped) is
// skipped ENTIRELY -- warning on stderr, no bar, no point, no table row -- so
// the previous campaign's figures remain valid until its data lands.
//
// Usage:
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     root -l -b -q 'macros/opt_recon_fig.C("RAW_CSV","report/figures","report/figures/opt_yield.csv")'
//
// Args (all optional):
//   rawCsv   combined raw table from run_opt_recon.sh
//            (default /tmp/opencode/recon/opt_recon_raw.csv)
//   outdir   output directory (default report/figures)
//   yieldCsv tag,mean_pe_per_event table (default report/figures/opt_yield.csv)

#include <TCanvas.h>
#include <TBox.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TMath.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "opt_designs.h"

namespace {

// Estimator keys in the exact spelling of position_reconstruction.C.
const char* kEst[5] = {
    "pulse integral",
    "first arrival",
    "optical CFD (12.5%)",
    "prompt fraction",
    "combined"
};
// Short column stems for opt_recon.csv.
const char* kEstStem[5] = {
    "pulse_integral",
    "first_arrival",
    "cfd_12p5",
    "prompt_fraction",
    "combined"
};

// Same-layout references: the config-1 reconstruction of the v6p5 campaign
// (eight SiPMs, 25 000 events), i.e. exactly opt::v7References().  The report
// has no config-3/config-4 reconstruction, so no such reference exists.  The
// v6p5 *yields* belong to a different campaign and are deliberately not used
// as abscissae in panel (b).
struct Ref { const char* what; double sigmaX; int style; };

struct EstRow {
    int    ntrain = 0, ntest = 0;
    double sx = 0, sy = 0, sr = 0, r68 = 0;
    bool   have = false;
};

struct DesignResult {
    opt::Design              d;
    std::map<std::string, EstRow> est;   // key = estimator name
    double                   meanPe = -1.;
    bool                     have = false;
};

std::vector<std::string> Split(const std::string& line, char sep = ',')
{
    std::vector<std::string> out;
    std::string part;
    std::stringstream ss(line);
    while (std::getline(ss, part, sep)) out.push_back(part);
    return out;
}

// Read the raw table written by run_opt_recon.sh.
bool ReadRaw(const char* fname, std::vector<DesignResult>& res)
{
    std::ifstream in(fname);
    if (!in) {
        std::fprintf(stderr, "opt_recon_fig: cannot open %s\n", fname);
        return false;
    }
    // Index designs by tag.
    std::map<std::string, int> idx;
    for (size_t i = 0; i < res.size(); ++i) idx[res[i].d.tag] = (int)i;

    std::string line;
    std::getline(in, line);   // header
    int nrows = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = Split(line);
        if (f.size() < 14) {
            std::printf("opt_recon_fig: malformed row: %s\n", line.c_str());
            continue;
        }
        const std::string tag = f[0];
        auto it = idx.find(tag);
        if (it == idx.end()) {
            std::printf("opt_recon_fig: unknown tag '%s' in the raw table\n", tag.c_str());
            continue;
        }
        DesignResult& r = res[it->second];
        if (f[3] != "bare") continue;   // r0 only (all four are the same file)
        EstRow e;
        e.ntrain = std::atoi(f[6].c_str());
        e.ntest  = std::atoi(f[7].c_str());
        e.sx     = std::atof(f[8].c_str());
        e.sy     = std::atof(f[9].c_str());
        e.sr     = std::atof(f[10].c_str());
        e.r68    = std::atof(f[13].c_str());
        e.have   = true;
        r.est[f[4]] = e;
        r.have = true;
        ++nrows;
    }
    std::printf("opt_recon_fig: read %d r0 estimator rows from %s\n", nrows, fname);
    return nrows > 0;
}

// tag -> mean photoelectrons/event from opt_yield.csv.
bool ReadYields(const char* fname, std::map<std::string, double>& pe)
{
    std::ifstream in(fname);
    if (!in) {
        std::fprintf(stderr,
                     "opt_recon_fig: warning: cannot open %s (panel b will be "
                     "empty)\n", fname);
        return false;
    }
    std::string line;
    std::getline(in, line);   // header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const std::vector<std::string> f = Split(line);
        if (f.size() < 10) continue;
        pe[f[0]] = std::atof(f[9].c_str());   // mean_pe_per_event
    }
    std::printf("opt_recon_fig: read %zu light yields from %s\n", pe.size(), fname);
    return true;
}

EstRow GetEst(const DesignResult& r, int i)
{
    auto it = r.est.find(kEst[i]);
    if (it == r.est.end()) return EstRow{};
    return it->second;
}

}  // namespace


// Wrap a long human label into two lines with TLatex's #splitline, so the
// rotated axis labels keep one common font size instead of overlapping.
static TString wrapLabel(const char* label, int maxChars = 18)
{
    TString s(label);
    if (s.Length() <= maxChars) return s;
    const int mid = s.Length() / 2;
    int best = -1;
    for (int i = 0; i < s.Length(); ++i) {
        if (s[i] != ' ') continue;
        if (best < 0 || std::abs(i - mid) < std::abs(best - mid)) best = i;
    }
    if (best <= 0) return s;
    return TString::Format("#splitline{%s}{%s}", s(0, best).Data(),
                           s(best + 1, s.Length()).Data());
}

// Panel (a) only: one-line short name, built from the design fields (the
// full d.label is kept for the table and for panel (b)).
static TString shortLabelA(const opt::Design& d)
{
    if (d.cfg >= 10) {
        const TString refl = (d.refl == 1) ? "TiO2" :
                             (d.refl == 3) ? "gloss" : opt::reflName(d.refl);
        return TString::Format("WLS%d %s", d.nch, refl.Data());
    }
    TString s = TString::Format("C%d", d.cfg);
    if (d.sipm_mm > 5.0) s += " 6 mm";
    else if (TString(d.edge) == "black") s += " black";
    else if (TString(d.edge) == "diffuse") s += " diffuse";
    else s += " control";
    return s;
}

// Split the full label at a space near its middle, so each line is at most
// about maxChars long.  The concatenation of the two lines is exactly the
// original d.label (only a space becomes the line break).
static int splitFullLabel(const char* label, int maxChars,
                          TString& a, TString& b)
{
    const TString s(label);
    if (s.Length() <= maxChars) { a = s; b = ""; return 1; }
    int best = -1;
    for (int i = 0; i < s.Length(); ++i) {
        if (s[i] != ' ') continue;
        if (best < 0 ||
            std::abs(i - s.Length() / 2) < std::abs(best - s.Length() / 2))
            best = i;
    }
    if (best <= 0) { a = s; b = ""; return 1; }
    a = s(0, best);
    b = s(best + 1, s.Length());
    b = b.Strip(TString::kLeading);
    return 2;
}

void opt_recon_fig(const char* rawCsv = "/tmp/opencode/recon/opt_recon_raw.csv",
                   const char* outdir = "report/figures",
                   const char* yieldCsv = "report/figures/opt_yield.csv")
{
    style(0.045);
    gSystem->mkdir(outdir, kTRUE);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);

    std::vector<DesignResult> res;
    for (const opt::Design& d : opt::designs()) {
        DesignResult r;
        r.d = d;
        res.push_back(r);
    }

    if (!ReadRaw(rawCsv, res)) {
        std::fprintf(stderr, "opt_recon_fig: no usable rows -- nothing to do\n");
        return;
    }

    std::map<std::string, double> peMap;
    ReadYields(yieldCsv, peMap);
    for (DesignResult& r : res) {
        auto it = peMap.find(r.d.tag);
        if (it != peMap.end()) r.meanPe = it->second;
    }

    // The only valid references are the two config-1 reconstruction entries
    // of opt::v7References() (v6p5, same eight-SiPM layout): glossy 15.9 and
    // TiO2 11.9 mm.  There is no config-3/config-4 reconstruction.
    std::vector<Ref> refs;
    for (const opt::Reference& v : opt::v7References()) {
        const TString what(v.what);
        if (!what.Contains("sigma_x")) continue;
        const bool glossy = what.Contains("glossy");
        refs.push_back({glossy ? "v6p5, C1 glossy" : "v6p5, C1 TiO2",
                        v.value, glossy ? 2 : 3});   // dashed / dotted
    }
    const int nRef = (int)refs.size();

    // ---------------------------------------------------------------- CSV
    int nHave = 0;
    for (const DesignResult& r : res) if (r.have) ++nHave;
    if (nHave == 0) {
        std::printf("opt_recon_fig: no complete design -- nothing to do\n");
        return;
    }

    {
        FILE* f = std::fopen(TString::Format("%s/opt_recon.csv", outdir), "w");
        if (f) {
            std::fprintf(f, "tag,cfg,preset,ntrain,ntest");
            for (int k = 0; k < 5; ++k)
                std::fprintf(f, ",sigma_x_%s_mm,sigma_y_%s_mm,sigma_r_%s_mm,r68_%s_mm",
                             kEstStem[k], kEstStem[k], kEstStem[k], kEstStem[k]);
            std::fprintf(f, "\n");
            for (const DesignResult& r : res) {
                if (!r.have) continue;
                const EstRow any = GetEst(r, 0);
                std::fprintf(f, "%s,%d,%d,%d,%d", r.d.tag, r.d.cfg, r.d.preset,
                             any.ntrain, any.ntest);
                for (int k = 0; k < 5; ++k) {
                    const EstRow e = GetEst(r, k);
                    std::fprintf(f, ",%.4f,%.4f,%.4f,%.4f", e.sx, e.sy, e.sr, e.r68);
                }
                std::fprintf(f, "\n");
            }
            std::fclose(f);
            std::printf("opt_recon_fig: wrote %s/opt_recon.csv (%d designs)\n",
                        outdir, nHave);
        } else {
            std::printf("opt_recon_fig: cannot write %s/opt_recon.csv\n", outdir);
        }
    }

    // ---------------------------------------------------------------- figure
    const int ndes = (int)res.size();

    // Designs with at least one estimator row; everything else is skipped
    // entirely (no missing bar / point / label / table row).
    std::vector<int> act;
    for (int i = 0; i < ndes; ++i)
        if (res[i].have) act.push_back(i);
    const int nAct = (int)act.size();

    // A wider canvas is needed for two readable panels: panel (a) gets a
    // horizontal-bar layout (labels are horizontal, never rotated), panel (b)
    // gets enough room for full wrapped design names.
    TCanvas* c = new TCanvas("c_opt_recon", "v7_opt position reconstruction",
                             1900, 820);

    // ---- panel (a): combined sigma_x per design, horizontal bars ----------
    // Give panel (a) the width needed for the full design labels; panel (b)
    // gives up the difference.
    c->cd();
    TPad* pa = new TPad("p_opt_recon_a", "", 0.0, 0.0, 0.520, 1.0);
    pa->SetLeftMargin(0.450);
    pa->SetRightMargin(0.045);
    pa->SetBottomMargin(0.130);
    pa->SetTopMargin(0.240);
    pa->Draw();
    pa->cd();
    gStyle->SetOptTitle(0);

    double barMax = 1.;
    for (int i : act) {
        const EstRow e = GetEst(res[i], 4);
        if (e.have) barMax = std::max(barMax, e.sx);
    }
    const double yMaxA = std::max(30., barMax * 1.12);

    {
        TH2D* fr = new TH2D("fr_opt_recon_a", "", 1, 0., yMaxA,
                            1, 0.5, (double)nAct + 0.5);
        fr->SetStats(0);
        fr->GetXaxis()->SetTitle("combined #sigma_{x} [mm]");
        fr->GetYaxis()->SetTitle("");
        fr->GetYaxis()->SetLabelSize(0.);
        fr->GetYaxis()->SetTickLength(0.);
        fr->GetXaxis()->SetTitleSize(0.040);
        fr->GetXaxis()->SetLabelSize(0.036);
        fr->GetXaxis()->SetTitleOffset(1.10);
        fr->Draw("AXIS");
    }

    // Reference guides first, behind the bars.
    std::vector<TLine*> refLinesA;
    TLegend* legA = new TLegend(0.020, 0.820, 0.980, 0.925);
    legA->SetBorderSize(0);
    legA->SetFillStyle(0);
    legA->SetTextSize(0.036);
    legA->SetNColumns(2);
    for (int k = 0; k < nRef; ++k) {
        TLine* l = new TLine(refs[k].sigmaX, 0.5, refs[k].sigmaX,
                             (double)nAct + 0.5);
        l->SetLineColor(kGray + 2);
        l->SetLineStyle(refs[k].style);
        l->SetLineWidth(2);
        l->Draw("SAME");
        refLinesA.push_back(l);
        legA->AddEntry(l, TString::Format("%s  %.1f mm", refs[k].what,
                                          refs[k].sigmaX), "l");
    }

    for (int k = 0; k < nAct; ++k) {
        const int i = act[k];
        const double yc = (double)k + 1.;
        TLatex* nm = new TLatex(-0.018 * yMaxA, yc, res[i].d.label);
        nm->SetNDC(false);
        nm->SetTextAlign(32);            // right, vertically centred
        nm->SetTextSize(0.036);
        nm->SetTextColor(kBlack);
        nm->Draw("SAME");

        const EstRow e = GetEst(res[i], 4);
        if (!e.have) continue;

        TBox* b = new TBox(0., yc - 0.34, e.sx, yc + 0.34);
        b->SetFillColor(opt::color(i));
        b->SetFillStyle(1001);
        b->SetLineColor(kBlack);
        b->SetLineWidth(1);
        b->Draw("SAME");

        // Value labels: keep every one clear of both reference lines and
        // the right frame.  Bars shorter than the longest reference carry
        // the value inside (right-aligned); long bars carry it outside,
        // falling back to the inside when the outside text would reach the
        // frame.
        const TString vt = TString::Format("%.1f", e.sx);
        TLatex probeV;
        probeV.SetTextFont(42);
        probeV.SetTextSize(0.036);
        UInt_t vw = 0, vh = 0;
        probeV.GetTextExtent(vw, vh, vt);
        const double framePx = gPad->GetWw() * gPad->GetAbsWNDC() *
                               (1. - gPad->GetLeftMargin() -
                                gPad->GetRightMargin());
        const double pxPerX = std::max(1e-6, framePx / yMaxA);
        const double tw = (double)vw / pxPerX;
        const double padX = 8. / pxPerX;

        double refMin = 1e30, refMax = -1e30;
        for (const Ref& r : refs) {
            refMin = std::min(refMin, r.sigmaX);
            refMax = std::max(refMax, r.sigmaX);
        }

        TLatex* v = new TLatex();
        v->SetNDC(false);
        v->SetTextAlign(32);             // right, vertically centred
        v->SetTextSize(0.036);
        v->SetTextColor(kBlack);
        if (e.sx < refMax) {
            // Inside the bar, and left of the lower reference when needed.
            double xr = std::min(e.sx - padX, refMin - padX);
            if (xr - tw < padX) xr = tw + padX;
            v->SetX(xr);
        } else {
            const double xl = e.sx + padX;
            if (xl + tw > yMaxA - padX) {
                double xr = e.sx - padX;
                if (xr - tw < padX) xr = tw + padX;
                v->SetX(xr);
            } else {
                v->SetTextAlign(12);     // left, vertically centred
                v->SetX(xl);
            }
        }
        // A label placed inside the bar must stay legible on its fill:
        // white on dark fills, black on light ones.
        if (v->GetX() < e.sx) {
            Float_t rr = 1.F, gg = 1.F, bb = 1.F;
            TColor* tc = gROOT->GetColor(opt::color(i));
            if (tc) {
                tc->GetRGB(rr, gg, bb);
                const double lum = 0.2126 * rr + 0.7152 * gg + 0.0722 * bb;
                v->SetTextColor(lum < 0.5 ? kWhite : kBlack);
            }
        }
        v->DrawLatex(v->GetX(), yc, vt);
    }
    legA->Draw();

    TLatex* titleA = new TLatex(0.020, 0.978,
        "(a) position resolution, combined estimator");
    titleA->SetNDC();
    titleA->SetTextFont(42);
    titleA->SetTextAlign(13);            // left, top
    titleA->SetTextSize(0.036);
    titleA->Draw();

    // ---- panel (b): resolution versus light yield -------------------------
    c->cd();
    TPad* pb = new TPad("p_opt_recon_b", "", 0.526, 0.0, 1.0, 1.0);
    pb->SetLogx();
    pb->SetLeftMargin(0.170);
    pb->SetRightMargin(0.035);
    pb->SetBottomMargin(0.160);
    pb->SetTopMargin(0.240);
    pb->Draw();
    pb->cd();
    gStyle->SetOptTitle(0);

    double xLo = 1e30, xHi = -1e30;
    double yMaxB = 1.;
    for (const DesignResult& r : res) {
        if (!r.have || r.meanPe <= 0.) continue;
        const EstRow e = GetEst(r, 4);
        if (!e.have) continue;
        xLo = std::min(xLo, r.meanPe);
        xHi = std::max(xHi, r.meanPe);
        yMaxB = std::max(yMaxB, e.sx);
    }
    for (int k = 0; k < nRef; ++k)
        yMaxB = std::max(yMaxB, refs[k].sigmaX);
    if (xHi < 0.) { xLo = 50.; xHi = 1000.; }
    xLo /= 1.35;
    xHi *= 1.35;
    yMaxB = std::max(65., yMaxB * 1.30);

    {
        TH1D* fr = new TH1D("fr_opt_recon_b", "", 100, xLo, xHi);
        fr->SetStats(0);
        fr->GetXaxis()->SetTitle("");
        fr->GetYaxis()->SetTitle("combined #sigma_{x} [mm]");
        fr->GetYaxis()->SetRangeUser(0., yMaxB);
        fr->GetXaxis()->SetTitleSize(0.);
        fr->GetXaxis()->SetLabelSize(0.);   // custom log labels below
        fr->GetYaxis()->SetTitleSize(0.052);
        fr->GetYaxis()->SetLabelSize(0.052);
        fr->GetYaxis()->SetTitleOffset(1.45);
        fr->GetXaxis()->SetNdivisions(0);
        fr->Draw("AXIS");
    }

    // Pareto lower envelope over the design points (minimise pe, minimise
    // sigma_x).  Sort by yield ascending and keep every new lower record.
    std::vector<int> order;
    for (int i = 0; i < ndes; ++i) {
        if (!res[i].have || res[i].meanPe <= 0.) continue;
        if (!GetEst(res[i], 4).have) continue;
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return res[a].meanPe < res[b].meanPe;
    });
    std::vector<double> px, py;
    double bestY = 1e30;
    for (int i : order) {
        const double y = GetEst(res[i], 4).sx;
        if (y < bestY) {
            bestY = y;
            px.push_back(res[i].meanPe);
            py.push_back(y);
        }
    }
    TGraph* par = nullptr;
    if (px.size() >= 2) {
        par = new TGraph((int)px.size(), px.data(), py.data());
        par->SetLineColor(kGray + 2);
        par->SetLineWidth(2);
        par->SetLineStyle(1);
        par->Draw("L SAME");
    }

    std::vector<TLine*> refLinesB;
    for (int k = 0; k < nRef; ++k) {
        TLine* l = new TLine(xLo, refs[k].sigmaX, xHi, refs[k].sigmaX);
        l->SetLineColor(kGray + 1);
        l->SetLineStyle(refs[k].style);
        l->SetLineWidth(2);
        l->Draw("SAME");
        refLinesB.push_back(l);
    }

    // Custom log-x tick labels: ROOT drops most of them in this narrow range,
    // and the 6 mm designs at ~700 pe need a visible tick.
    {
        const double ml = gPad->GetLeftMargin();
        const double mr = gPad->GetRightMargin();
        const double logLo = TMath::Log10(xLo);
        const double logHi = TMath::Log10(xHi);
        const double vals[] = {50., 70., 100., 200., 300., 500., 700., 900.};
        TLatex tx;
        tx.SetNDC();
        tx.SetTextFont(42);
        tx.SetTextAlign(21);             // centred, top
        tx.SetTextSize(0.035);
        tx.SetTextColor(kBlack);
        for (double v : vals) {
            if (v < xLo * 1.02 || v > xHi * 0.98) continue;
            const double f = (TMath::Log10(v) - logLo) / (logHi - logLo);
            const double xn = ml + f * (1. - ml - mr);
            tx.DrawLatex(xn, 0.114, TString::Format("%g", v));
        }
        TLatex xt;
        xt.SetNDC();
        xt.SetTextFont(42);
        xt.SetTextAlign(21);
        xt.SetTextSize(0.036);
        xt.DrawLatex(ml + 0.5 * (1. - ml - mr), 0.052,
                     "mean photoelectrons per event");
    }

    // Filled design markers plus full d.label names.  Labels are placed by a
    // small candidate search in pad NDC: no label may touch a marker, either
    // reference line, the frame, another label, or the Pareto polyline.
    const double mlB = gPad->GetLeftMargin();
    const double mrB = gPad->GetRightMargin();
    const double mbB = gPad->GetBottomMargin();
    const double mtB = gPad->GetTopMargin();
    const double W = std::max(1., (double)gPad->GetWw() *
                                   gPad->GetAbsWNDC());
    const double H = std::max(1., (double)gPad->GetWh() *
                                   gPad->GetAbsHNDC());
    const double logLo = TMath::Log10(xLo);
    const double logHi = TMath::Log10(xHi);
    auto xToNdc = [&](double x) {
        return mlB + (TMath::Log10(x) - logLo) / (logHi - logLo) *
                     (1. - mlB - mrB);
    };
    auto yToNdc = [&](double y) {
        return mbB + y / yMaxB * (1. - mtB - mbB);
    };

    struct LabBox { double x1, x2, y1, y2; };
    struct Seg { double x1, y1, x2, y2; };
    struct LabRec {
        int i;
        double xm, ym;
        LabBox box;
        TString la, lb;
        int nl;
    };
    std::vector<LabBox> placed;
    std::vector<LabRec> recs;
    std::vector<double> mx, my;
    for (const DesignResult& r : res) {
        if (!r.have || r.meanPe <= 0.) continue;
        const EstRow e = GetEst(r, 4);
        if (!e.have) continue;
        mx.push_back(xToNdc(r.meanPe));
        my.push_back(yToNdc(e.sx));
    }
    auto overlaps = [](const LabBox& a, const LabBox& b, double m) {
        return !(a.x2 + m < b.x1 || b.x2 + m < a.x1 ||
                 a.y2 + m < b.y1 || b.y2 + m < a.y1);
    };
    // Sampled segment/box test for the dashed leader lines: a leader must
    // not cross any glyph box, including labels placed later.
    auto segHitsBox = [](const Seg& s, const LabBox& b,
                         double mxMargin, double myMargin) {
        for (int k = 0; k <= 80; ++k) {
            const double f = k / 80.;
            const double x = s.x1 + (s.x2 - s.x1) * f;
            const double y = s.y1 + (s.y2 - s.y1) * f;
            if (x > b.x1 - mxMargin && x < b.x2 + mxMargin &&
                y > b.y1 - myMargin && y < b.y2 + myMargin) return true;
        }
        return false;
    };

    const double ts = 0.0350;
    for (int i = 0; i < ndes; ++i) {
        const DesignResult& r = res[i];
        if (!r.have || r.meanPe <= 0.) continue;
        const EstRow e = GetEst(r, 4);
        if (!e.have) continue;

        const double xm = xToNdc(r.meanPe);
        const double ym = yToNdc(e.sx);
        TGraph* g = new TGraph(1, &r.meanPe, &e.sx);
        g->SetMarkerStyle(20);
        g->SetMarkerSize(1.6);
        g->SetMarkerColor(opt::color(i));
        g->SetLineColor(opt::color(i));
        g->Draw("P SAME");

        TString la, lb;
        const int nl = splitFullLabel(r.d.label, 18, la, lb);
        TLatex probe;
        probe.SetTextFont(42);
        probe.SetTextSize(ts);
        UInt_t wa = 0, ha = 0, wb = 0, hb = 0;
        probe.GetTextExtent(wa, ha, la);
        if (nl > 1) probe.GetTextExtent(wb, hb, lb);
        const double tw = 1.12 * std::max((double)wa, (double)wb) / W;
        const double lh = 1.55 * ts;      // one text line, in pad NDC
        const double th = nl * lh * 1.05;

        // Global grid search: choose the free position closest to the
        // marker.  A leader is drawn when the label lands far away.
        LabBox best{};
        double bestScore = 1e30;
        bool found = false;
        const double gx = 0.007;
        const double gy = 0.006;
        const double xlo = mlB + 5. / W;
        const double xhi = 1. - mrB - 5. / W;
        const double ylo = mbB + 18. / H;
        const double yhi = 1. - mtB - 12. / H;
        for (double yy = ylo; yy + th < yhi; yy += gy) {
            for (double xx = xlo; xx + tw < xhi; xx += gx) {
                LabBox b{xx, xx + tw, yy, yy + th};
                bool bad = false;
                for (size_t k = 0; k < mx.size() && !bad; ++k) {
                    const double emx = 14. / W, emy = 14. / H;
                    if (mx[k] > b.x1 - emx && mx[k] < b.x2 + emx &&
                        my[k] > b.y1 - emy && my[k] < b.y2 + emy) bad = true;
                }
                for (int k = 0; k < nRef && !bad; ++k) {
                    const double ry = yToNdc(refs[k].sigmaX);
                    if (ry > b.y1 - 8./H && ry < b.y2 + 8./H) bad = true;
                }
                for (const LabBox& q : placed)
                    if (overlaps(b, q, 8. / W)) { bad = true; break; }

                if (par && !bad) {
                    for (int k = 0; k + 1 < par->GetN() && !bad; ++k) {
                        const double x1 = xToNdc(par->GetX()[k]);
                        const double y1 = yToNdc(par->GetY()[k]);
                        const double x2 = xToNdc(par->GetX()[k+1]);
                        const double y2 = yToNdc(par->GetY()[k+1]);
                        const double sx1 = std::min(x1, x2) - 3./W;
                        const double sx2 = std::max(x1, x2) + 3./W;
                        const double sy1 = std::min(y1, y2) - 3./H;
                        const double sy2 = std::max(y1, y2) + 3./H;
                        if (b.x2 < sx1 || b.x1 > sx2 ||
                            b.y2 < sy1 || b.y1 > sy2) continue;
                        for (int s = 0; s <= 30; ++s) {
                            const double xc = x1 + (x2 - x1) * s / 30.;
                            const double yc = y1 + (y2 - y1) * s / 30.;
                            if (xc > b.x1 - 2./W && xc < b.x2 + 2./W &&
                                yc > b.y1 - 2./H && yc < b.y2 + 2./H) {
                                bad = true; break;
                            }
                        }
                    }
                }
                if (bad) continue;
                const double cx = 0.5 * (b.x1 + b.x2);
                const double cy = 0.5 * (b.y1 + b.y2);
                const double score = (cx - xm) * (cx - xm) +
                                     (cy - ym) * (cy - ym);
                if (score < bestScore) {
                    best = b; bestScore = score; found = true;
                }
            }
        }
        if (!found) {
            best = {mlB + 10./W, mlB + 10./W + tw,
                    mbB + 10./H, mbB + 10./H + th};
        }
        placed.push_back(best);

        recs.push_back({i, xm, ym, best, la, lb, nl});

        TLatex lab;
        lab.SetNDC();
        lab.SetTextFont(42);
        lab.SetTextSize(ts);
        lab.SetTextColor(kBlack);        // keep the colour on marker/leader
        lab.SetTextAlign(22);               // centred, centred
        // Re-draw at the bbox we reserved, keeping both lines in that box.
        const double ymid = 0.5 * (best.y1 + best.y2);
        const double step = lh;
        if (nl > 1) {
            lab.DrawLatex(0.5 * (best.x1 + best.x2),
                          ymid + 0.5 * step, la);
            lab.DrawLatex(0.5 * (best.x1 + best.x2),
                          ymid - 0.5 * step, lb);
        } else {
            lab.DrawLatex(0.5 * (best.x1 + best.x2), ymid, la);
        }
    }

    // Leaders are drawn after all label boxes are known, and are allowed to
    // use an elbow when the direct path would cross a glyph box.  If no
    // clear route exists the leader is omitted rather than drawn through
    // text.
    {
        auto pointOnBox = [](double px, double py, const LabBox& b) {
            return std::pair<double,double>{
                std::max(b.x1, std::min(px, b.x2)),
                std::max(b.y1, std::min(py, b.y2))};
        };
        auto clearSeg = [&](const Seg& s, const LabRec& self) {
            for (const LabRec& q : recs) {
                if (&q == &self) continue;
                if (segHitsBox(s, q.box, 2. / W, 2. / H)) return false;
            }
            return true;
        };
        for (const LabRec& r : recs) {
            const auto near = pointOnBox(r.xm, r.ym, r.box);
            // Only the label side facing the marker is a safe endpoint;
            // routing to the far side would cross the label's own glyphs.
            std::vector<std::pair<double,double>> ends = {near};
            std::vector<std::vector<Seg>> routes;
            for (const auto& e : ends) {
                const double dx = std::fabs(e.first - r.xm) * W;
                const double dy = std::fabs(e.second - r.ym) * H;
                if (dx + dy <= 34.) continue;
                routes.push_back({{r.xm, r.ym, e.first, e.second}});
                routes.push_back({{r.xm, r.ym, e.first, r.ym},
                                  {e.first, r.ym, e.first, e.second}});
                routes.push_back({{r.xm, r.ym, r.xm, e.second},
                                  {r.xm, e.second, e.first, e.second}});
                // Three-segment detours around a blocking label box:
                // travel in a clear vertical or horizontal corridor.
                for (const LabRec& q : recs) {
                    if (&q == &r) continue;
                    const double xs[2] = {q.box.x1 - 4. / W,
                                          q.box.x2 + 4. / W};
                    for (double xc : xs) {
                        if (xc < mlB + 8. / W || xc > 1. - mrB - 8. / W)
                            continue;
                        routes.push_back({{r.xm, r.ym, xc, r.ym},
                                          {xc, r.ym, xc, e.second},
                                          {xc, e.second, e.first, e.second}});
                    }
                    const double ys[2] = {q.box.y1 - 4. / H,
                                          q.box.y2 + 4. / H};
                    for (double yc : ys) {
                        if (yc < mbB + 8. / H || yc > 1. - mtB - 8. / H)
                            continue;
                        routes.push_back({{r.xm, r.ym, r.xm, yc},
                                          {r.xm, yc, e.first, yc},
                                          {e.first, yc, e.first, e.second}});
                    }
                }
            }
            std::sort(routes.begin(), routes.end(),
                      [](const std::vector<Seg>& a,
                         const std::vector<Seg>& b) {
                          auto len = [](const std::vector<Seg>& v) {
                              double l = 0.;
                              for (const Seg& s : v)
                                  l += std::hypot(s.x2 - s.x1, s.y2 - s.y1);
                              return l;
                          };
                          return len(a) < len(b);
                      });
            for (const auto& route : routes) {
                bool ok = true;
                // Keep every vertex off the frame: a leader hugging the spine
                // reads as a stray line over the tick marks.
                for (const Seg& s : route) {
                    for (const double xv : {s.x1, s.x2})
                        if (xv < mlB + 8. / W || xv > 1. - mrB - 8. / W)
                            ok = false;
                    for (const double yv : {s.y1, s.y2})
                        if (yv < mbB + 8. / H || yv > 1. - mtB - 8. / H)
                            ok = false;
                }
                if (!ok) continue;
                for (const Seg& s : route)
                    if (!clearSeg(s, r)) { ok = false; break; }
                if (!ok) continue;
                for (const Seg& s : route) {
                    TLine* lead = new TLine(s.x1, s.y1, s.x2, s.y2);
                    lead->SetNDC();
                    lead->SetLineColor(opt::color(r.i));
                    lead->SetLineWidth(1);
                    lead->SetLineStyle(2);
                    lead->Draw("SAME");
                }
                break;
            }

        }
    }

    {
        // Hand-drawn legend: TLegend's fixed row pitch is smaller than the
        // 0.038 text height and would overlap the three entries.
        const double lx1 = 0.022, lx2 = 0.072, tx = 0.084;
        const double yy[3] = {0.910, 0.857, 0.804};
        for (int k = 0; k < nRef; ++k) {
            TLine* sl = new TLine(lx1, yy[k], lx2, yy[k]);
            sl->SetNDC();
            sl->SetLineColor(kGray + 1);
            sl->SetLineStyle(refs[k].style);
            sl->SetLineWidth(2);
            sl->Draw();
            TLatex* le = new TLatex(tx, yy[k], refs[k].what);
            le->SetNDC();
            le->SetTextFont(42);
            le->SetTextSize(0.038);
            le->SetTextAlign(12);
            le->Draw();
        }
        if (par) {
            TLine* sl = new TLine(lx1, yy[2], lx2, yy[2]);
            sl->SetNDC();
            sl->SetLineColor(kGray + 2);
            sl->SetLineStyle(1);
            sl->SetLineWidth(2);
            sl->Draw();
            TLatex* le = new TLatex(tx, yy[2],
                                    "grey line: Pareto lower envelope");
            le->SetNDC();
            le->SetTextFont(42);
            le->SetTextSize(0.038);
            le->SetTextAlign(12);
            le->Draw();
        }
    }

    TLatex* titleB = new TLatex(0.020, 0.978,
        "(b) resolution versus collected light");
    titleB->SetNDC();
    titleB->SetTextFont(42);
    titleB->SetTextAlign(13);
    titleB->SetTextSize(0.038);
    titleB->Draw();

    gStyle->SetOptTitle(0);
    for (const char* ex : {"pdf", "png"})
        c->SaveAs(TString::Format("%s/fig_opt_recon.%s", outdir, ex));
    std::printf("opt_recon_fig: wrote %s/fig_opt_recon.{pdf,png}\n", outdir);

    // ---------------------------------------------------------------- table
    std::printf("\n%-18s %9s %9s %9s %9s %9s %9s\n",
                "design", "pe/evt", "pulse", "1st arr", "CFD", "prompt",
                "combined");
    std::printf("%s\n", std::string(84, '-').c_str());
    for (int i : act) {
        const DesignResult& r = res[i];
        std::printf("%-18s %9.1f", r.d.tag, r.meanPe);
        for (int k = 0; k < 5; ++k) {
            const EstRow e = GetEst(r, k);
            std::printf(" %9.2f", e.have ? e.sx : -1.);
        }
        std::printf("\n");
    }
    std::printf("\n");
}
