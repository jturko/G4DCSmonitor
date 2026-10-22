// pe_spectra_grid.C -- photoelectron-yield spectra in the layout of Fig. 10 of
// Rao et al. (J. Appl. Phys. 138, 114504), adapted to this simulation.
//
// Rao Fig. 10 is a grid of pulse-height (voltage) spectra: the three MCD
// measurement regions (centre, edge, corner) are the rows and the trigger
// detector configurations are the columns, with the four reflective materials
// overlaid in every cell.  Here the voltage pulse height is replaced by the
// number of photoelectrons per event (the simulation analogue), with five
// configurations: 1, 2, the corner-cut "Config 2 (mod)" (12), 3 and 4.  The
// fourth standard configuration (single SiPM + WLS fibre clamp) is the last
// column.
//
// The result is a 3 (region) x 5 (config) grid; each cell overlays the four
// reflector treatments.  Spectra are probability densities (unit area).  The
// lower panel uses logarithmic axes with one shared range so the five configs
// can be compared directly; the upper panel gives every config column its own
// linear abscissa and ordinate, sized to that config's data, because the
// WLS-clamp layout (config 4) collects ~20x fewer photoelectrons than the
// directly-coupled layouts and would otherwise collapse into the first percent
// of a shared linear axis.
//
// Layout: the pads are placed by hand rather than with TCanvas::Divide.  Each
// pad's *frame* is an identical W x H NDC rectangle separated only by tiny
// inner gaps, so the panels are equal and carry no inner whitespace.  The
// generous outer margins live in the (larger) rectangles of the left-column and
// bottom-row pads, which hold the shared y/x tick labels; the axis titles,
// region identifiers and column headers are drawn once on the canvas itself.
//
// Reads <dir>/muon_out_c<C>_r<R>_{centre,edge,corner}.root (tree "sipmHits"),
// with C in {1,2,12,3,4}.  A missing/incomplete config simply leaves its column
// empty (the histograms are booked for every layout) and never crashes.
//
// Usage:
//   root -l -b -q 'macros/pe_spectra_grid.C("slurm/data_v2.2/matrix","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

#include "style.h"

namespace {
const char* kReflName[4] = {"none (bare)", "TiO2", "Al", "glossy"};
const int   kStyle[4]    = {kSolid, kDashed, kDotted, kDashDotted};
const char* kLocName[3]  = {"centre", "edge", "corner"};

// Config order everywhere: 1, 2, 12 (corner-cut "Config 2 (mod)"), 3, 4.
const int   kCfgVal[5]   = {1, 2, 12, 3, 4};
const char* kCfgLong[5]  = {"Config 1", "Config 2", "Config 2 (mod)",
                            "Config 3", "Config 4"};
const int   kNcfg        = 5;

// Logarithmic bin edges: one [0,0.5) bin to absorb zero-p.e. events, then
// ~10%-wide log bins up to a generous maximum.  On the log-x display the
// zero bin is off-scale, i.e. "no pulse"; the spectra are normalised to unit
// area over the populated bins.
std::vector<double> logEdges()
{
    std::vector<double> e;
    e.push_back(0.0);
    e.push_back(0.5);
    const int    n  = 70;
    const double lo = 0.5, hi = 6000.0;
    for (int i = 1; i <= n; ++i)
        e.push_back(lo * std::pow(hi / lo, (double)i / n));
    return e;
}
}  // namespace

void pe_spectra_grid(const char* dir = "slurm/data_v2.2/matrix",
                     const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gSystem->mkdir(outdir, kTRUE);

    const int kColor[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3)};

    const std::vector<double> edges = logEdges();
    const int nb = (int)edges.size() - 1;

    // h[cfg][loc][refl]  (cfg in kCfgVal, loc 0..2, refl 0..3) -- log bins
    std::map<int, std::map<int, std::map<int, TH1D*>>> h;
    for (int ci = 0; ci < kNcfg; ++ci) {
        const int c = kCfgVal[ci];
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r) {
                TString nm = TString::Format("hpe_c%d_%s_r%d", c, kLocName[l], r);
                h[c][l][r] = new TH1D(nm, "", nb, edges.data());
            }
    }

    // Raw pe/event samples per cell, kept so the linear-axes panel can use
    // linearly-spaced bins (log-spaced bins look wrong on a linear abscissa).
    std::map<int, std::map<int, std::map<int, std::vector<double>>>> pev;

    // ---- discover + reduce files ---------------------------------------
    std::vector<TString> files;
    void* d = gSystem->OpenDirectory(dir);
    if (!d) { std::cerr << "No such directory: " << dir << "\n"; return; }
    const char* e;
    while ((e = gSystem->GetDirEntry(d))) {
        TString n(e);
        if (n.BeginsWith("muon_out_") && n.EndsWith(".root"))
            files.emplace_back(TString(dir) + "/" + n);
    }
    gSystem->FreeDirectory(d);
    if (files.empty()) { std::cerr << "No muon_out_*.root in " << dir << "\n"; return; }

    for (const auto& full : files) {
        TString stem = gSystem->BaseName(full);
        stem.ReplaceAll("muon_out_", "");
        stem.ReplaceAll(".root", "");
        // stem = c<C>_r<R>_<loc>
        Ssiz_t p0 = stem.Index("c"), p1 = stem.Index("_r"), p2 = stem.Last('_');
        if (p0 == kNPOS || p1 == kNPOS || p2 == kNPOS) continue;
        int cfg  = TString(stem(p0 + 1, p1 - p0 - 1)).Atoi();
        int refl = TString(stem(p1 + 2, p2 - p1 - 2)).Atoi();
        TString loc = stem(p2 + 1, stem.Length());
        bool known = false;
        for (int ci = 0; ci < kNcfg; ++ci) if (cfg == kCfgVal[ci]) known = true;
        if (!known || refl < 0 || refl > 3) continue;
        int li = -1;
        for (int l = 0; l < 3; ++l) if (loc == kLocName[l]) li = l;
        if (li < 0) continue;

        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { delete f; continue; }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }

        int evt = 0, det = 0, nDet = 0;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("nDetected", &nDet);

        // sum nDetected over the SiPMs -> photoelectrons per event
        std::map<int, double> ev;
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0) continue;
            ev[evt] += nDet;
        }
        for (auto& kv : ev) {
            h[cfg][li][refl]->Fill(kv.second);
            pev[cfg][li][refl].push_back(kv.second);
        }
        delete f;
    }

    // ---- linearly-binned twins for the linear-axes panel ---------------
    // The linear panel gives every *config column* its own abscissa, sized to
    // that config's data: the WLS-clamp layout (config 4) collects far fewer
    // photoelectrons than the directly-coupled layouts, so one shared linear
    // range would crush config 4 into the first percent of the axis (and
    // over-spread the other configs).  Each column's range is the pooled
    // 99.5th percentile of all its regions and reflectors (rounded up); the
    // logarithmic panel keeps a single shared range so the configs stay
    // comparable there.
    const int nbl = 100;
    double xhi_lin[kNcfg];             // index = config column (0..4)
    std::map<int, std::map<int, std::map<int, TH1D*>>> hlin;
    for (int ci = 0; ci < kNcfg; ++ci) {
        const int c = kCfgVal[ci];
        std::vector<double> pool;
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r)
                for (double v : pev[c][l][r]) pool.push_back(v);
        std::sort(pool.begin(), pool.end());
        double xh = pool.empty() ? 100. : pool[(size_t)(0.995 * pool.size())];
        xh = std::ceil(xh / 100.0) * 100.0;
        if (xh < 100.) xh = 100.;
        xhi_lin[ci] = xh;

        std::vector<double> ledges(nbl + 1);
        for (int i = 0; i <= nbl; ++i) ledges[i] = xh * i / nbl;   // 0 .. xh
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r) {
                hlin[c][l][r] = new TH1D(
                    TString::Format("hplin_c%d_%s_r%d", c, kLocName[l], r),
                    "", nbl, ledges.data());
                for (double v : pev[c][l][r]) hlin[c][l][r]->Fill(v);
            }
    }

    // ---- normalise all spectra, then fix SHARED x/y ranges -------------
    // Every cell uses the same log axes so configs and regions can be compared
    // directly.  Only populated bins count; the zero-p.e. bin sits off the
    // logarithmic abscissa.  Empty (missing) configs contribute nothing, so a
    // partial data tree simply narrows the range instead of crashing.
    double gxmin = 1e30, gxmax = -1e30, gymin = 1e30, gymax = 0.;
    for (int ci = 0; ci < kNcfg; ++ci) {
        const int cfg = kCfgVal[ci];
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r) {
                TH1D* hh = h[cfg][l][r];
                double I = hh->Integral();          // includes the zero bin
                if (I > 0) hh->Scale(1. / I);
                for (int b = 1; b <= hh->GetNbinsX(); ++b) {
                    double lo = hh->GetXaxis()->GetBinLowEdge(b);
                    double up = hh->GetXaxis()->GetBinUpEdge(b);
                    double v  = hh->GetBinContent(b);
                    if (v > 0 && lo >= 0.5) {
                        gxmin = std::min(gxmin, lo);
                        gxmax = std::max(gxmax, up);
                        gymin = std::min(gymin, v);
                        gymax = std::max(gymax, v);
                    }
                }
            }
    }
    if (gxmin > gxmax) { gxmin = 0.5; gxmax = 200.; }
    if (gymin > gymax) { gymin = 1e-5; }
    const double xlo = 0.8 * gxmin, xhi = 1.3 * gxmax;
    const double ylo = 0.4 * gymin, yhi = 1.6 * gymax;

    // Linear-axes twin: normalise each cell to unit area over its own abscissa
    // and size every config column's ordinate to the tallest bin in that column
    // (all three regions and four reflectors), so each column uses its full
    // vertical range.  The ordinate is the bin content (normalised entries),
    // exactly as in panel~(b) -- NOT the probability density: ROOT plots the
    // content, and sizing the axis from content/bin-width is what clipped every
    // peak flat against the frame top.
    double yhi_lin[kNcfg];             // index = config column (0..4)
    for (int ci = 0; ci < kNcfg; ++ci) {
        const int c = kCfgVal[ci];
        double ymax = 0.;
        for (int l = 0; l < 3; ++l)
            for (int r = 0; r < 4; ++r) {
                TH1D* hh = hlin[c][l][r];
                double I = hh->Integral();
                if (I > 0) hh->Scale(1. / I);
                for (int b = 1; b <= hh->GetNbinsX(); ++b) {
                    double v = hh->GetBinContent(b);
                    if (v > ymax) ymax = v;
                }
            }
        // ~1.35x headroom rounded up to a half-decade, so the top tick label is
        // a round number (config 1 -> 0.03) and the tallest peak sits well clear
        // of the frame instead of being clipped flat against it.
        double yh = 1.35 * ymax;
        if (yh <= 0.) yh = 1.;         // empty (missing) config: harmless range
        double mag = std::pow(10.0, std::floor(std::log10(yh)));
        yh = std::ceil(yh / (mag / 2.0)) * (mag / 2.0);
        yhi_lin[ci] = yh;
    }

    // ---- layout + draw one 3 x 5 grid inside the NDC band [y0, y1] ------
    // Frames are all W x H, packed with small inner gaps; only the outer border
    // of the band is wide enough for the shared labels.  Pads are placed by hand
    // so the frames stay identical without introducing inner whitespace.
    //
    // A pad's *rectangle* is a little larger than its frame: ROOT clips a pad's
    // contents to that rectangle, and every tick label is centred on the frame
    // edge it annotates, so without a margin the labels sitting on a shared or
    // outer edge are sliced in half (each row's top "0.03"/"10^k", each
    // non-bottom row's bottom "0", and the "0"/"800" at a column's left/right
    // edge).  The margins below grow the pad just enough to hold those labels;
    // the frame stays exactly W x H and the axis ranges are untouched.  GY must
    // hold a top and a bottom label between two stacked frames (GY >= 2*yLabH)
    // and GX a left and a right one (GX >= xLabL + xLabR), so the enlarged pads
    // never overlap (an overlap would let a later pad's opaque background
    // repaint an earlier pad's label).
    const double ML = 0.105, MR = 0.020, GX = 0.024;
    const double MTb = 0.050, MBb = 0.052, GY = 0.021;   // band-edge margins
    const double xLabL = 0.006, xLabR = 0.016;  // x label half-width at edges
    const double yLabH = 0.009;                 // y label half-height (bottom)
    const double xRegionLbl = 0.018;                     // left-margin text x
    const double xYTitle    = 0.052;   // centred between region labels and the
                                       // (now wider) per-config y tick labels
    const double W  = (1.0 - ML - MR - (kNcfg - 1) * GX) / (double)kNcfg;

    // Wider than tall enough for five columns: at the report's
    // width=textwidth,height=0.73textheight the panels and their text keep
    // their physical size instead of shrinking with a fifth column.
    TCanvas* c = new TCanvas("c_pe_grid", "pe spectra grid", 1950, 2000);

    auto drawGrid = [&](std::map<int, std::map<int, std::map<int, TH1D*>>>& M,
                        double y0, double y1, bool useLog,
                        const double* xa, const double* xb,
                        const double* ya, const double* yb,
                        const char* tag) {
        const double H = ((y1 - y0) - MTb - MBb - 2.0 * GY) / 3.0;

        for (int l = 0; l < 3; ++l) {
            for (int cc = 0; cc < kNcfg; ++cc) {
                const int cfg = kCfgVal[cc];
                const double fx = ML + cc * (W + GX);          // frame left
                const double fy = y0 + MBb + (2 - l) * (H + GY);  // frame bottom

                // The pad rectangle is larger than the frame where the frame
                // carries boundary labels: the left column needs room for the y
                // labels (already inside px0=0); a non-bottom row's left-column
                // pad needs yLabH below the frame for the bottom "0"; and the
                // bottom row needs xLabL/xLabR at the frame's left/right edges.
                // The margins re-inset the frame to the shared W x H rectangle.
                // Left/right only where the frame carries an x tick label (the
                // bottom row); every pad gets a top margin (each row's top tick
                // label is centred on the frame top) and every non-bottom row a
                // bottom margin (its bottom label is centred on the frame
                // bottom).  All pads share the same top margin so the x tick
                // labels stay the same physical size (ROOT sizes them by pad
                // height).
                const double boxR = (l == 2) ? xLabR : 0.0;
                const double boxL = (l == 2 && cc != 0) ? xLabL : 0.0;
                const double px0 = (cc == 0) ? 0.0 : fx - boxL;
                const double py0 = (l == 2)  ? y0 : fy - yLabH;
                const double pw  = (fx + W + boxR) - px0;
                const double ph  = (fy + H + yLabH) - py0;

                c->cd();   // parent must be the canvas, not the previous pad
                TPad* p = new TPad(TString::Format("p_%s_%d_%d",
                                                   useLog ? "log" : "lin", l, cc),
                                   "", px0, py0, fx + W + boxR, fy + H + yLabH);
                p->SetLeftMargin((fx - px0) / pw);
                p->SetRightMargin(boxR / pw);
                p->SetBottomMargin((fy - py0) / ph);
                p->SetTopMargin(yLabH / ph);
                p->SetTickx();
                p->SetTicky();
                if (useLog) { p->SetLogx(); p->SetLogy(); }
                p->Draw();
                p->cd();

                for (int r = 0; r < 4; ++r) {
                    TH1D* hh = M[cfg][l][r];
                    hh->SetLineColor(kColor[r]);
                    hh->SetLineStyle(kStyle[r]);
                    hh->SetLineWidth(2);
                    hh->GetXaxis()->SetRangeUser(xa[cc], xb[cc]);
                    // Fix the y-range with SetMinimum/SetMaximum: for a TH1 the
                    // y-axis autoscales from the bin maximum and ignores a
                    // SetRangeUser on the y-axis, which was silently clipping the
                    // tallest peak flat against the frame top.
                    hh->SetMinimum(ya[cc]);
                    hh->SetMaximum(yb[cc]);
                    hh->GetYaxis()->SetMoreLogLabels(kFALSE);  // decade labels only
                    hh->GetYaxis()->SetTitle("");   // shared: drawn on canvas
                    // ROOT scales the label size by the pad height, and only the
                    // bottom row's pad carries the extra MBb margin, so divide
                    // it out to keep every row's labels the same physical size.
                    if (cc == 0) {
                        hh->GetYaxis()->SetLabelSize(0.065 * (H + MBb + yLabH) / ph);
                    } else {
                        hh->GetYaxis()->SetLabelSize(0);  // shared: left column
                    }
                    hh->GetXaxis()->SetTitle("");       // shared: drawn on canvas
                    if (l == 2) {
                        hh->GetXaxis()->SetLabelSize(0.065);
                    } else {
                        hh->GetXaxis()->SetLabelSize(0);  // shared: bottom row
                    }
                    if (r == 0) hh->Draw("HIST");
                    else        hh->Draw("HIST SAME");
                }

                // shared legend in the top-right cell (Config 4 column)
                if (l == 0 && cc == kNcfg - 1) {
                    // Clamp x2 to stay inside the pad (x2 > 1 puts the right
                    // edge outside the pad and flush with the canvas border).
                    TLegend* leg = new TLegend(0.500, 0.530, 0.985, 0.890);
                    leg->SetBorderSize(0);
                    leg->SetFillStyle(0);
                    leg->SetTextSize(0.040);
                    for (int r = 0; r < 4; ++r)
                        leg->AddEntry(M[cfg][l][r], kReflName[r], "l");
                    leg->Draw();
                }
            }
        }

        // ---- shared outer labels for this band (canvas NDC) -------------
        c->cd();   // canvas NDC, not the last pad's NDC
        for (int cc = 0; cc < kNcfg; ++cc) {                // column headers
            TLatex* tc = new TLatex();
            tc->SetNDC();
            // The corner-cut label ("Config 2 (mod)") is the longest; 0.021
            // keeps every header inside its own frame at this canvas width.
            tc->SetTextSize(0.021);
            tc->SetTextFont(42);
            tc->SetTextAlign(21);
            tc->DrawLatex(ML + cc * (W + GX) + W / 2.0, y1 - MTb * 0.45,
                          kCfgLong[cc]);
        }
        for (int l = 0; l < 3; ++l) {                       // region identifiers
            TLatex* tx = new TLatex();
            tx->SetNDC();
            tx->SetTextSize(0.024);
            tx->SetTextFont(42);
            tx->SetTextAlign(22);
            tx->SetTextAngle(90);
            tx->DrawLatex(xRegionLbl, y0 + MBb + (2 - l) * (H + GY) + H / 2.0,
                          kLocName[l]);
        }
        {
            TLatex* ty = new TLatex();                      // shared y title
            ty->SetNDC();
            ty->SetTextSize(0.024);
            ty->SetTextFont(42);
            ty->SetTextAlign(22);
            ty->SetTextAngle(90);
            ty->DrawLatex(xYTitle, (y0 + y1) / 2.0, "normalised entries");
        }
        {
            TLatex* txt = new TLatex();                     // shared x title
            txt->SetNDC();
            txt->SetTextSize(0.026);
            txt->SetTextFont(42);
            txt->SetTextAlign(22);
            txt->DrawLatex(ML + (1.0 - ML - MR) / 2.0, y0 + MBb * 0.38,
                           "photoelectrons / event");
        }
        {
            TLatex* tt = new TLatex();                      // panel-group tag
            tt->SetNDC();
            tt->SetTextSize(0.028);
            tt->SetTextFont(42);
            tt->SetTextAlign(13);   // left, top
            tt->DrawLatex(0.010, y1 - 0.006, tag);
        }
    };

    // (a) linear, truncated abscissa, linearly-binned; (b) logarithmic, full
    // range.  Both share the same reflector colours/styles and the same units.
    double lad_xlo[kNcfg], lad_xhi[kNcfg], lad_ylo[kNcfg], lad_yhi[kNcfg];
    double log_xlo[kNcfg], log_xhi[kNcfg], log_ylo[kNcfg], log_yhi[kNcfg];
    for (int ci = 0; ci < kNcfg; ++ci) {
        lad_xlo[ci] = 0.0;    lad_xhi[ci] = xhi_lin[ci];
        lad_ylo[ci] = 0.0;    lad_yhi[ci] = yhi_lin[ci];
        log_xlo[ci] = xlo;    log_xhi[ci] = xhi;
        log_ylo[ci] = ylo;    log_yhi[ci] = yhi;
    }
    drawGrid(hlin, 0.510, 0.995, false, lad_xlo, lad_xhi, lad_ylo, lad_yhi, "(a)");
    drawGrid(h,    0.005, 0.490, true,  log_xlo, log_xhi, log_ylo, log_yhi, "(b)");

    c->SaveAs(TString::Format("%s/fig_pe_grid.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_pe_grid.png", outdir));
    printf("pe_spectra_grid: wrote %s/fig_pe_grid.{pdf,png}\n", outdir);
}
