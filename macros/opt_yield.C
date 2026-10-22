// opt_yield.C -- v7_opt detector-optimisation campaign: yield summary and
// per-event photoelectron spectra for the eleven whole-plate designs.
//
// For every design in macros/opt_designs.h this macro reads
//
//     <dir>/muon_whole_out_c<cfg>_r<refl>_<tag>.root      (tree sipmHits)
//
// and accumulates the per-event total photoelectrons (sum of nDetected over
// the SiPM channels).  Rows are per (event, SiPM), zero-detection rows
// included, but a muon that misses the slab writes no rows at all, so the
// number of events is the number of DISTINCT evtNb values -- NOT max(evtNb)+1
// and NOT the row count.
//
// Outputs (in <outdir>, default report/figures):
//
//   opt_yield.csv              one row per readable design
//   fig_opt_yield.{pdf,png}    horizontal yield bars, grouped by stem
//   fig_opt_spectra.{pdf,png}  4 x 3 grid of per-event total-p.e. spectra
//
// Files that are absent, still being written, or otherwise unreadable are
// skipped entirely (warning on stderr): the design gets no bar, no spectrum
// panel and no CSV row, so the previous campaign's figures stay valid until a
// new design's data lands.
//
// Usage:
//   env ROOT_INCLUDE_PATH="$HOME/root-work:$PWD/macros" \
//     $HOME/programs/root/build/bin/root -l -b -q \
//     'macros/opt_yield.C("/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole","report/figures")'
//
// Analysis-only: no Geant4 dependencies.  Deterministic, no timestamps.

#include "opt_designs.h"

#include <TBox.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TPad.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>
#include <TMath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

style(0.045);

namespace {

// ---- fixed plotting constants -------------------------------------------
const double kSpecXMax = 2400.0;   // common, design-blind spectrum ceiling
const int    kSpecBins = 120;      // 20 p.e. per bin
const double kSpecYMin = 1.e-6;
const double kSpecYMax = 0.50;

struct Yield {
    bool        ok      = false;
    long long   events  = 0;
    double      mean    = 0.;      // p.e. / event (all events)
    double      meanDet = 0.;      // p.e. / event with >= 1 p.e.
    double      p0      = 0.;      // fraction of events with zero p.e.
    double      sem     = 0.;      // statistical error of the mean
    double      meanProd = 0.;     // optical photons produced in the slab
    double      maxPe   = 0.;
    std::vector<double> npe;       // per-event totals, for the spectra
};

bool stemDiffers(const opt::Design& a, const opt::Design& b)
{
    return TString(a.stem) != TString(b.stem);
}

// Draw a TLatex string on a matching white background box.  The box is needed
// so that a value label is never struck through by a reference line; it is
// sized from ROOT's own text metrics, so the padding stays constant in pixels.
// align follows TLatex's convention (12 = left/centred, 32 = right/centred).
void drawTextWithWhiteBox(TLatex* tex, double x, double y,
                          const char* text, int align)
{
    tex->SetTextAlign(align);
    tex->SetNDC(false);

    UInt_t w = 0, h = 0;
    tex->GetTextExtent(w, h, text);

    const int px = gPad->XtoPixel(x);
    const int py = gPad->YtoPixel(y);
    const int xa = (align / 10 == 3) ? px + 3 : px - 3;
    const int xb = (align / 10 == 3) ? px - (int)w - 3 : px + (int)w + 3;
    const int ya = py - (int)h / 2 - 3;
    const int yb = py + (int)h / 2 + 3;

    double xlo = gPad->PixeltoX(xa), xhi = gPad->PixeltoX(xb);
    double ylo = gPad->PixeltoY(ya), yhi = gPad->PixeltoY(yb);
    if (xlo > xhi) std::swap(xlo, xhi);
    if (ylo > yhi) std::swap(ylo, yhi);

    TBox* bg = new TBox(xlo, ylo, xhi, yhi);
    bg->SetFillColor(kWhite);
    bg->SetFillStyle(1001);
    bg->SetLineColor(kWhite);
    bg->SetLineWidth(0);
    bg->Draw("SAME");

    tex->DrawLatex(x, y, text);
}

// Split a design label into at most two lines at its top-level comma (commas
// inside parentheses, e.g. "(8 ch, dual)", are not separators).  The full
// text is preserved; nothing is abbreviated.  Returns the number of lines.
// maxLen is the longest run that is left on one line.
int splitLabel(const char* label, TString& a, TString& b, int maxLen = 40)
{
    const TString s(label);
    if (s.Length() <= maxLen) { a = s; b = ""; return 1; }
    int depth = 0, split = -1;
    for (int i = 0; i < s.Length(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') { if (depth > 0) --depth; }
        else if (s[i] == ',' && depth == 0) split = i;
    }
    if (split < 0 && s.Length() > maxLen) {  // no comma: split at mid-space
        const int mid = s.Length() / 2;
        for (int i = 0; i < s.Length(); ++i) {
            if (s[i] != ' ') continue;
            if (split < 0 || std::abs(i - mid) < std::abs(split - mid)) split = i;
        }
    }
    if (split <= 0) { a = s; b = ""; return 1; }
    a = s(0, split);
    b = s(split + 1, s.Length());
    b = b.Strip(TString::kLeading);
    return 2;
}

// Read one whole-plate file.  Never throws: a missing / zombie / incomplete
// file simply returns ok = false.
Yield readDesign(const opt::Design& d, const char* dir)
{
    Yield r;
    const TString fname = opt::file(dir, d);

    if (gSystem->AccessPathName(fname, kFileExists)) {
        fprintf(stderr, "opt_yield: SKIP %-18s -- file does not exist (%s)\n",
                d.tag, fname.Data());
        return r;
    }

    TFile* f = TFile::Open(fname, "READ");
    if (!f || f->IsZombie()) {
        fprintf(stderr,
                "opt_yield: SKIP %-18s -- cannot open (probably still being "
                "written): %s\n", d.tag, fname.Data());
        if (f) delete f;
        return r;
    }

    TTree* t = (TTree*)f->Get("sipmHits");
    const char* needed[] = {"evtNb", "sipm", "nDetected", "nProduced",
                            "muonX", "muonY"};
    bool branchesOk = (t != nullptr);
    if (branchesOk)
        for (const char* b : needed)
            if (!t->GetBranch(b)) branchesOk = false;
    if (!branchesOk) {
        fprintf(stderr, "opt_yield: SKIP %-18s -- no usable sipmHits tree in %s\n",
                d.tag, fname.Data());
        delete f;
        return r;
    }

    int    evt = 0, sipm = 0, nd = 0, nprod = 0;
    double muonX = 0., muonY = 0.;
    t->SetBranchAddress("evtNb",     &evt);
    t->SetBranchAddress("sipm",      &sipm);
    t->SetBranchAddress("nDetected", &nd);
    t->SetBranchAddress("nProduced", &nprod);
    t->SetBranchAddress("muonX",     &muonX);
    t->SetBranchAddress("muonY",     &muonY);

    // Per-event accumulators keyed by evtNb.  A muon that misses the slab (it
    // can land in a cut-away corner triangle) writes no rows at all, so the
    // event count is the number of DISTINCT evtNb values -- not max(evtNb)+1
    // and not the row count.
    struct EvAcc { double npe = 0.; int produced = -1; };
    std::map<int, EvAcc> ev;

    const Long64_t nrows = t->GetEntries();
    for (Long64_t i = 0; i < nrows; ++i) {
        t->GetEntry(i);
        if (evt < 0) continue;

        EvAcc& a = ev[evt];
        a.npe += (double)nd;
        if (a.produced < 0) a.produced = nprod;   // constant within an event
    }
    delete f;

    if (ev.empty()) {
        fprintf(stderr, "opt_yield: SKIP %-18s -- tree has no events\n", d.tag);
        return r;
    }

    // Number of events = number of distinct evtNb values in the tree.
    const long long nev = (long long)ev.size();
    double sum = 0., sumsq = 0., sumProd = 0.;
    long long nz = 0, ndet = 0, nprodSeen = 0;

    r.npe.reserve((size_t)nev);
    for (const auto& kv : ev) {
        const double v = kv.second.npe;
        r.npe.push_back(v);
        sum += v;
        sumsq += v * v;
        if (v > r.maxPe) r.maxPe = v;
        if (v == 0.) ++nz; else ++ndet;
        if (kv.second.produced >= 0) {
            sumProd += kv.second.produced;
            ++nprodSeen;
        }
    }

    r.ok      = true;
    r.events  = nev;
    r.mean    = sum / (double)nev;
    r.meanDet = (ndet > 0) ? sum / (double)ndet : 0.;
    r.p0      = (double)nz / (double)nev;
    r.meanProd = (nprodSeen > 0) ? sumProd / (double)nprodSeen : 0.;

    if (nev > 1) {
        const double var = (sumsq - sum * sum / (double)nev) / (double)(nev - 1);
        r.sem = std::sqrt(std::max(0., var) / (double)nev);
    } else {
        r.sem = 0.;
    }

    return r;
}

// ---- figure 1: horizontal yield bars ------------------------------------
void drawYieldFigure(const std::vector<opt::Design>& ds,
                     const std::vector<Yield>& ys,
                     const std::vector<int>& active, const char* outdir)
{
    gStyle->SetOptTitle(0);

    const int n = (int)active.size();
    const double kText = 0.046;       // one size for every in-figure label

    double maxMean = 0.;
    for (const auto& y : ys)
        if (y.ok) maxMean = std::max(maxMean, y.mean + y.sem);

    // Nice common x ceiling: a little headroom for the printed values.
    double xmax = std::ceil(1.25 * std::max(maxMean, 1.) / 50.) * 50.;
    if (xmax < 800.) xmax = 800.;

    // Display order: contiguous stem groups (manifest order inside each).
    // The CSV and the colours keep the manifest index, so this only reorders
    // the rows on the page -- the numbers cannot move.
    const char* groupOrder[5] = {"position", "wls", "maxpe", "control",
                                 "corner"};
    std::vector<int> order;
    for (int g = 0; g < 5; ++g)
        for (int i : active)
            if (TString(ds[i].stem) == groupOrder[g]) order.push_back(i);
    if ((int)order.size() != n) {          // defensive: keep the table order
        order = active;
    }

    // Vertical placement: first design at the top, with extra space between
    // stem groups.
    const double gap = 0.90;
    std::vector<double> ypos(n, 0.);
    for (int j = 1; j < n; ++j) {
        const double step =
            1. + (stemDiffers(ds[order[j - 1]], ds[order[j]]) ? gap : 0.);
        ypos[j] = ypos[j - 1] - step;
    }
    const double ymin = ypos[n - 1] - 0.80;
    // Headroom above row 1 for the group header AND the transparent legend.
    // 3.2 keeps the legend clear of the first value label while the row pitch
    // (15.7 data units over the pad) still separates adjacent labels.
    const double ymax = 3.20;

    TCanvas* c = new TCanvas("c_opt_yield", "v7_opt yields", 1400, 700);
    // Enough left margin for the longest single-line label ("4-sided WLS
    // (8 ch, dual), glossy", 417 px) plus >= 8 px of canvas clearance.
    c->SetLeftMargin(0.335);
    c->SetRightMargin(0.055);
    c->SetTopMargin(0.06);
    c->SetBottomMargin(0.125);      // keeps the x-axis title bbox >= 8 px up

    // Invisible histogram used only for the frame and the x axis.
    TH1D* frame = new TH1D("h_opt_yield_frame", "", 1, 0., xmax);
    frame->SetMinimum(ymin);
    frame->SetMaximum(ymax);
    frame->SetStats(0);
    frame->GetYaxis()->SetLabelSize(0.);
    frame->GetYaxis()->SetTitleSize(0.);
    frame->GetYaxis()->SetTickLength(0.);
    frame->GetYaxis()->SetNdivisions(0);
    frame->GetXaxis()->SetTitle("Mean photoelectrons / event");
    frame->GetXaxis()->SetTitleSize(kText);
    frame->GetXaxis()->SetTitleOffset(1.05);
    frame->GetXaxis()->SetLabelSize(kText);
    frame->Draw("AXIS");

    // Reference lines from the v7 production campaign.  They are drawn FIRST,
    // behind the bars and labels.  The 140.7 line is shortened to the top block
    // of short bars, i.e. the part of the frame where that x value lies in the
    // empty space to the right of the bar; below it the long bars cover it.
    struct Ref { double value; const char* text; bool shorten;
                 int style; Color_t color; };
    const Ref refs[] = {
        {520.7, "v7 C1 glossy = 520.7", false, 2, kGray + 2},   // dashed
        {140.7, "v7 C3 glossy = 140.7", true,  4, kGray + 4}    // dash-dot
    };
    std::vector<TLine*> refLines;
    for (const auto& ref : refs) {
        const double ylo = (ref.shorten && n > 3) ? ypos[3] - 0.80 : ymin;
        TLine* l = new TLine(ref.value, ylo, ref.value, ymax);
        l->SetLineColor(ref.color);
        l->SetLineStyle(ref.style);
        l->SetLineWidth(2);
        l->Draw("SAME");
        refLines.push_back(l);
    }

    // Separators where a new stem group starts.
    for (int j = 1; j < n; ++j) {
        if (!stemDiffers(ds[order[j - 1]], ds[order[j]])) continue;
        const double ysep = 0.5 * (ypos[j - 1] + ypos[j]);
        TLine* s = new TLine(0., ysep, xmax, ysep);
        s->SetLineColor(kGray + 1);
        s->SetLineStyle(3);
        s->SetLineWidth(1);
        s->Draw("SAME");
    }

    // Bars and value labels.
    for (int j = 0; j < n; ++j) {
        const int idx = order[j];               // manifest index
        const auto& d = ds[idx];
        const auto& y = ys[idx];
        const Color_t col = opt::color(idx);    // manifest-order colour

        // Human label in the left margin (never the raw tag).
        TLatex* lab = new TLatex(-0.018 * xmax, ypos[j], d.label);
        lab->SetNDC(false);
        lab->SetTextAlign(32);         // right, vertically centred
        lab->SetTextSize(kText);
        lab->SetTextColor(y.ok ? kBlack : kGray + 2);
        lab->Draw("SAME");

        // One group header per stem, above the first row of the group.
        if (j == 0 || stemDiffers(ds[order[j - 1]], d)) {
            TLatex* hd = new TLatex(-0.018 * xmax, ypos[j] + 0.95,
                                    opt::stemLabel(d.stem));
            hd->SetNDC(false);
            hd->SetTextAlign(32);      // right, vertically centred
            hd->SetTextSize(kText);
            hd->SetTextColor(kBlack);
            hd->Draw("SAME");
        }

        if (!y.ok) {
            TLatex* miss = new TLatex(0.015 * xmax, ypos[j], "missing");
            miss->SetNDC(false);
            miss->SetTextAlign(12);
            miss->SetTextSize(kText);
            miss->SetTextColor(kGray + 2);
            miss->Draw("SAME");
            continue;
        }

        TBox* bar = new TBox(0., ypos[j] - 0.36, y.mean, ypos[j] + 0.36);
        bar->SetFillColor(col);
        bar->SetFillStyle(1001);
        bar->SetLineColor(kBlack);
        bar->SetLineWidth(1);
        bar->Draw("SAME");

        // No error-bar ticks: the statistical error is tiny and the caps were
        // both invisible and touching the label.  The +- value carries it.
        const TString value =
            TString::Format("%.1f #pm %.1f", y.mean, y.sem);
        TLatex* val = new TLatex();
        val->SetTextFont(42);
        val->SetTextSize(kText);
        val->SetTextColor(kBlack);

        // Value label: right of the bar.  If its text box would be struck by
        // one of the vertical reference lines, start it just to the right of
        // that line instead (a horizontal offset, not a mask).
        const double plotPx = gPad->GetWw() *
            (1. - gPad->GetLeftMargin() - gPad->GetRightMargin());
        const double unitsPerPx = xmax / std::max(1., plotPx);
        const double padX = 4. * unitsPerPx;
        UInt_t tw = 0, th = 0;
        val->GetTextExtent(tw, th, value);
        const double wData = (double)tw * unitsPerPx;
        double xlab = y.mean + 0.020 * xmax;
        for (const auto& ref : refs) {
            if (xlab < ref.value && xlab + wData + padX > ref.value)
                xlab = ref.value + padX;
        }
        drawTextWithWhiteBox(val, xlab, ypos[j], value, 12);
    }

    // Reference legend with visible line samples (dashed vs dash-dot).
    //
    // Coordinates are TCanvas NDC (a TLegend drawn on the canvas is NOT in pad
    // NDC).  Place it just right of the 520.7 reference line (computed from
    // xmax, which is data-driven) and inside the frame with a >= 15 px margin
    // on every side.  This region is free of bars and of all value labels;
    // the 140.7 line is far to the left.  The fill is fully transparent: it
    // must never mask the frame or a reference line.
    const double frameL = c->GetLeftMargin();
    const double frameR = 1. - c->GetRightMargin();
    const double frameT = 1. - c->GetTopMargin();
    const double legL = frameL + (520.7 / xmax) * (frameR - frameL) + 0.018;
    const double legR = frameR - 0.012;
    const double legT = frameT - 0.015;
    const double legB = legT - 0.130;      // 2 entries at text size 0.040
    TLegend* leg = new TLegend(legL, legB, legR, legT);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);           // transparent: no opaque white pad
    leg->SetLineColor(kWhite);
    leg->SetTextFont(42);
    leg->SetTextSize(0.040);
    leg->SetNColumns(1);
    for (size_t k = 0; k < refLines.size(); ++k)
        leg->AddEntry(refLines[k], refs[k].text, "l");
    leg->Draw();

    c->SaveAs(TString::Format("%s/fig_opt_yield.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_opt_yield.png", outdir));
    delete c;
}

// ---- figure 2: 4 x 3 grid of per-event p.e. spectra ---------------------
void drawSpectraFigure(const std::vector<opt::Design>& ds,
                       const std::vector<Yield>& ys,
                       const std::vector<int>& active, const char* outdir)
{
    gStyle->SetOptTitle(0);

    // 4 columns; the row count follows the number of readable designs (so a
    // partially populated campaign does not get empty rows of axis titles).
    const int nx = 4;
    const int nPanels = (int)active.size();
    const int ny = std::max(1, (nPanels + nx - 1) / nx);
    TCanvas* c = new TCanvas("c_opt_spectra", "v7_opt spectra", 1600, 1428);

    // Wider canvas-level left margin: it holds the shared, rotated y title.
    const double ml = 0.075, mr = 0.006, mt = 0.045, mb = 0.070;
    const double gx = 0.012, gy = 0.010;
    const double pw = (1. - ml - mr - (nx - 1) * gx) / nx;
    const double ph = (1. - mt - mb - (ny - 1) * gy) / ny;

    const double kTitle = 0.078;      // all panel titles, same size
    const double kLabel = 0.086;      // TGaxis sizes scale with frame height
    const double kAxis  = 0.088;

    for (int i = 0; i < nPanels; ++i) {
        const int col = i % nx;
        const int row = i / nx;

        const double x0 = ml + col * (pw + gx);
        const double y0 = 1. - mt - (row + 1) * ph - row * gy;

        // A panel with nothing below it (the two blanks of the last row) owns
        // its x tick labels and x title.  Its pad is stretched down over the
        // blank cell so ROOT can draw them in the bottom margin without being
        // clipped at the pad edge; top margin and frame height are chosen so
        // the frame itself stays exactly where it would be in a normal cell.
        const bool hasBelow = ((row + 1) * nx + col) < nPanels;
        const bool ownXAxis = !hasBelow;
        double padBottom = y0;
        double padH      = ph;
        if (ownXAxis && row != ny - 1) {
            padBottom = y0 - (ph + gy);
            padH      = 2. * ph + gy;
        }

        c->cd();
        TPad* p = new TPad(TString::Format("p_opt_spec_%02d", i), "", x0,
                           padBottom, x0 + pw, y0 + ph);
        p->SetLeftMargin(col == 0 ? 0.155 : 0.025);
        p->SetRightMargin(0.025);
        if (ownXAxis && row != ny - 1) {
            // frame top and bottom identical to the row above, the rest of the
            // tall pad is the label/title margin over the blank cell
            const double t = 0.32 * ph / padH;
            p->SetTopMargin(t);
            p->SetBottomMargin(1. - t - (1. - 0.32 - 0.025) * ph / padH);
        } else {
            p->SetBottomMargin(row == ny - 1 ? 0.22 : 0.025);
            p->SetTopMargin(0.32);    // title block (two lines) above the frame
        }
        p->SetTickx();
        p->SetTicky();
        p->SetLogy(1);
        p->Draw();
        p->cd();

        const int idx = active[i];
        const auto& d = ds[idx];
        const auto& y = ys[idx];

        TH1D* h = new TH1D(TString::Format("h_opt_spec_%02d", i), "",
                           kSpecBins, 0., kSpecXMax);
        h->SetStats(0);
        h->SetDirectory(0);
        h->SetMinimum(kSpecYMin);
        h->SetMaximum(kSpecYMax);
        h->GetXaxis()->SetTitle("photoelectrons / event");
        h->GetXaxis()->SetTitleSize(kAxis);
        h->GetXaxis()->SetLabelSize(kLabel);
        h->GetXaxis()->SetTitleOffset(1.0);
        h->GetXaxis()->SetNdivisions(4);
        h->GetYaxis()->SetTitle("");
        h->GetYaxis()->SetLabelSize(kLabel);
        h->GetYaxis()->SetMoreLogLabels(kFALSE);

        // Axis labels on the outer edges of the grid, and on every panel that
        // has no panel below it (so no panel is left without x tick labels).
        if (!ownXAxis) {
            h->GetXaxis()->SetLabelSize(0.);
            h->GetXaxis()->SetTitleSize(0.);
        }
        if (col != 0) {
            h->GetYaxis()->SetLabelSize(0.);
        }

        if (y.ok) {
            for (double v : y.npe) h->Fill(v);
            const double integral = h->Integral(0, kSpecBins + 1);
            if (integral > 0.) h->Scale(1. / integral);
            h->SetLineColor(opt::color(idx));
            h->SetLineWidth(3);
            h->Draw("HIST");
        } else {
            h->SetLineColor(kGray + 2);
            h->Draw("AXIS");
        }

        // Title block: full design label, wrapped at its top-level comma when
        // it would not fit a panel on one line, then the mean.  The y values
        // below are pad-NDC positions in a normal cell; for the stretched pads
        // they are converted to the same absolute height above the pad top.
        TString la, lb;
        const int nLines = splitLabel(d.label, la, lb, 25);
        auto titleY = [&](double yn) { return 1. - (1. - yn) * ph / padH; };

        TLatex* t1 = new TLatex();
        t1->SetNDC();
        t1->SetTextFont(42);
        t1->SetTextSize(kTitle);
        t1->SetTextAlign(22);           // centred, centred
        t1->SetTextColor(y.ok ? kBlack : kGray + 2);
        if (nLines == 2) {
            t1->DrawLatex(0.50, titleY(0.950), la);
            t1->DrawLatex(0.50, titleY(0.868), lb);
        } else {
            t1->DrawLatex(0.50, titleY(0.915), la);
        }

        TLatex* t2 = new TLatex();
        t2->SetNDC();
        t2->SetTextFont(42);
        t2->SetTextSize(kTitle);
        t2->SetTextAlign(22);
        t2->SetTextColor(y.ok ? kBlack : kGray + 2);
        const double yMean = titleY(0.770);
        if (y.ok)
            t2->DrawLatex(0.50, yMean,
                          TString::Format("#LTN_{pe}#GT = %.1f", y.mean));
        else
            t2->DrawLatex(0.50, yMean, "FILE MISSING OR UNREADABLE");
    }

    // One shared, rotated y title per row, at the row's vertical centre.
    c->cd();
    for (int row = 0; row < ny; ++row) {
        const double yc = 1. - mt - (row + 0.5) * ph - row * gy;
        TLatex* yt = new TLatex();
        yt->SetNDC();
        yt->SetTextFont(42);
        yt->SetTextSize(0.020);         // canvas-level text: ~20 px capitals
        yt->SetTextAngle(90.);
        yt->SetTextAlign(22);           // centred, centred
        yt->DrawLatex(0.058, yc, "Event fraction / 20 p.e.");
    }

    c->SaveAs(TString::Format("%s/fig_opt_spectra.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_opt_spectra.png", outdir));
    delete c;
}

}  // namespace

void opt_yield(const char* dir = "slurm/data_v7_opt/whole",
               const char* outdir = "report/figures")
{
    gSystem->mkdir(outdir, kTRUE);

    const std::vector<opt::Design> ds = opt::designs();
    std::vector<Yield> ys(ds.size());

    printf("opt_yield: reading %zu designs from %s\n", ds.size(), dir);
    for (size_t i = 0; i < ds.size(); ++i)
        ys[i] = readDesign(ds[i], dir);

    // Designs whose file is missing/unreadable are skipped ENTIRELY (warning
    // already printed on stderr): no bar, no spectrum panel, no CSV row.
    std::vector<int> active;
    for (size_t i = 0; i < ds.size(); ++i)
        if (ys[i].ok) active.push_back((int)i);
    if (active.empty()) {
        fprintf(stderr, "opt_yield: no readable files under %s -- nothing to do\n",
                dir);
        return;
    }

    // ---- CSV -------------------------------------------------------------
    const TString csv = TString::Format("%s/opt_yield.csv", outdir);
    FILE* fp = fopen(csv, "w");
    if (fp) {
        fprintf(fp, "tag,cfg,refl,preset,edge,clamp,sipm_mm,nch,events,"
                    "mean_pe_per_event,mean_pe_per_detected_event,p_zero,"
                    "sem_pe,mean_nproduced,note\n");
        for (int i : active) {
            const auto& d = ds[i];
            const auto& y = ys[i];
            fprintf(fp,
                    "%s,%d,%d,%d,%s,%s,%.1f,%d,%lld,%.4f,%.4f,%.6f,"
                    "%.4f,%.2f,ok\n",
                    d.tag, d.cfg, d.refl, d.preset, d.edge, d.clamp,
                    d.sipm_mm, d.nch, y.events, y.mean, y.meanDet, y.p0,
                    y.sem, y.meanProd);
        }
        fclose(fp);
        printf("opt_yield: wrote %s\n", csv.Data());
    } else {
        printf("opt_yield: ERROR cannot write %s\n", csv.Data());
    }

    // ---- figures ---------------------------------------------------------
    drawYieldFigure(ds, ys, active, outdir);
    drawSpectraFigure(ds, ys, active, outdir);
    printf("opt_yield: wrote %s/fig_opt_yield.{pdf,png}\n", outdir);
    printf("opt_yield: wrote %s/fig_opt_spectra.{pdf,png}\n", outdir);

    // ---- self-documenting stdout table -----------------------------------
    printf("\n%-18s %4s %4s %4s %7s %8s %11s %11s %9s %9s %10s %-7s\n",
           "tag", "cfg", "refl", "prst", "nch", "events",
           "<pe>/evt", "<pe>/det", "p_zero", "sem", "<nprod>", "note");
    printf("%.*s\n", 116, "-----------------------------------------------------"
                          "-----------------------------------------------------"
                          "--------------");
    for (int i : active) {
        const auto& d = ds[i];
        const auto& y = ys[i];
        printf("%-18s %4d %4d %4d %7d %8lld %11.4f %11.4f %9.5f %9.4f "
               "%10.2f %-7s\n",
               d.tag, d.cfg, d.refl, d.preset, d.nch, y.events,
               y.mean, y.meanDet, y.p0, y.sem, y.meanProd, "ok");
    }
    printf("\n");
}
