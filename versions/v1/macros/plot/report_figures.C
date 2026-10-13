// report_figures.C -- generate the figures used by the LaTeX matrix-scan report.
//
// Reads the matrix outputs (muon_out_c<C>_r<R>_{centre,edge,corner}.root) and
// writes vector PDFs (+ PNG previews) into `outdir`:
//
//   fig_eff_bars.pdf      overall collection efficiency, 4 reflectors x 3 configs
//   fig_yield.pdf         per-event optical-photon yield (produced) per reflector
//   fig_detected.pdf      per-event photoelectron count per reflector
//   fig_wavelength.pdf    detected-photon wavelength spectrum per reflector
//   fig_radius.pdf        collection efficiency vs distance from the beam axis
//
// Usage:
//   root -l -b -q 'report_figures.C("slurm/data_output","report/figures")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TLegend.h>
#include <TMultiGraph.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

namespace {
const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};
const int   kColor[4]    = {kBlack, kGreen + 2, kAzure + 1, kRed + 1};
const int   kStyle[4]    = {kSolid, kDashed, kDotted, kDashDotted};
}  // namespace

void report_figures(const char* dir = "slurm/data_output",
                    const char* outdir = "report/figures")
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetLegendFont(42);
    gSystem->mkdir(outdir, kTRUE);

    // ---- discover files -------------------------------------------------
    std::vector<TString> files;
    void* d = gSystem->OpenDirectory(dir);
    if (!d) { std::cerr << "No such directory: " << dir << "\n"; return; }
    const char* e;
    while ((e = gSystem->GetDirEntry(d))) {
        TString n(e);
        if (n.BeginsWith("muon_out_") && n.EndsWith(".root")) {
            files.emplace_back(TString(dir) + "/" + n);
        }
    }
    gSystem->FreeDirectory(d);
    if (files.empty()) { std::cerr << "No muon_out_*.root in " << dir << "\n"; return; }

    // ---- accumulators ---------------------------------------------------
    struct Tot { double prod = 0., coll = 0.; };
    std::map<int, std::map<int, Tot>> tot;            // cfg -> refl -> totals
    // per-(cfg=1,refl) per-event distributions
    std::map<int, TH1D*> hProduced, hDetected;
    std::map<int, TH1D*> hWave;
    // radius-binned efficiency data: cfg -> refl -> {sumprod, sumcoll} per r bin
    const int NR = 28; const double RMAX = 175.;     // 0..175 mm, ~6.25 mm bins
    std::map<int, std::map<int, std::vector<double>>> rProd, rColl;

    for (int r = 0; r < 4; ++r) {
        hProduced[r] = new TH1D(TString::Format("hp_r%d", r), "", 350, 0, 70000);
        hDetected[r] = new TH1D(TString::Format("hd_r%d", r), "", 120, 0, 2400);
        hWave[r]     = new TH1D(TString::Format("hw_r%d", r), "", 90, 360, 630);
        rProd[1][r].assign(NR, 0.); rColl[1][r].assign(NR, 0.);
    }

    for (const auto& full : files) {
        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { delete f; continue; }
        TTree* t = (TTree*)f->Get("sipmHits");
        if (!t) { delete f; continue; }

        // parse cfg/refl from the stem
        TString stem = gSystem->BaseName(full);
        stem.ReplaceAll("muon_out_", "");
        int cfg = 0, refl = -1;
        Ssiz_t p0 = stem.Index("c"), p1 = stem.Index("_r");
        if (p0 == kNPOS || p1 == kNPOS) { delete f; continue; }
        cfg  = TString(stem(p0 + 1, p1 - p0 - 1)).Atoi();
        refl = TString(stem(p1 + 2, stem.Length())).Atoi();
        if (refl < 0 || refl > 3) { delete f; continue; }

        int evt = 0, det = 0, sipm = 0, nDet = 0, nProd = 0;
        double mx = 0., my = 0., wl = 0.;
        t->SetBranchAddress("evtNb", &evt);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("sipm", &sipm);
        t->SetBranchAddress("nDetected", &nDet);
        t->SetBranchAddress("muonX", &mx);
        t->SetBranchAddress("muonY", &my);
        t->SetBranchAddress("nProduced", &nProd);
        t->SetBranchAddress("meanWavelength_nm", &wl);

        // aggregate rows -> per event
        std::map<int, std::vector<double>> ev;  // evt -> {x,y,produced,collected}
        for (long i = 0; i < t->GetEntries(); ++i) {
            t->GetEntry(i);
            if (det != 0) continue;
            auto& v = ev[evt];
            if (v.empty()) v = {mx, my, (double)nProd, 0.};
            v[3] += nDet;
            if (nDet > 0) hWave[refl]->Fill(wl, nDet);   // weight by p.e.
        }
        for (auto& kv : ev) {
            double x = kv.second[0], y = kv.second[1];
            double pr = kv.second[2], co = kv.second[3];
            tot[cfg][refl].prod += pr;
            tot[cfg][refl].coll += co;
            if (cfg == 1) {
                hProduced[refl]->Fill(pr);
                hDetected[refl]->Fill(co);
                int ib = (int)(std::hypot(x, y) / (RMAX / NR));
                if (ib >= NR) ib = NR - 1;
                rProd[1][refl][ib] += pr;
                rColl[1][refl][ib] += co;
            }
        }
        delete f;
    }

    const char* exts[2] = {"pdf", "png"};

    // ================= Fig 1: efficiency bars (config x reflector) ========
    {
        // Shared y-axis range over all three configs so the panels are
        // directly comparable.
        double ymaxAll = 0.;
        for (int cfg = 1; cfg <= 3; ++cfg)
            for (int r = 0; r < 4; ++r) {
                double p = tot[cfg][r].prod, co = tot[cfg][r].coll;
                double eff = p > 0 ? co / p : 0.;
                ymaxAll = std::max(ymaxAll, eff + (p > 0 ? std::sqrt(co) / p : 0.));
            }
        if (ymaxAll <= 0.) ymaxAll = 1.;

        TCanvas* c = new TCanvas("c_bars", "efficiency", 1600, 560);
        c->Divide(3, 1, 0.004, 0.004);
        for (int cfg = 1; cfg <= 3; ++cfg) {
            c->cd(cfg);
            gStyle->SetOptTitle(1);        // global style hides titles; show here
            gPad->SetLeftMargin(0.16);
            gPad->SetRightMargin(0.03);
            gPad->SetBottomMargin(0.16);
            gPad->SetTopMargin(0.12);
            TH1D* h = new TH1D(TString::Format("hb_c%d", cfg), "", 4, 0, 4);
            for (int r = 0; r < 4; ++r) {
                double p = tot[cfg][r].prod, co = tot[cfg][r].coll;
                double eff = p > 0 ? co / p : 0.;
                h->SetBinContent(r + 1, eff);
                h->SetBinError(r + 1, p > 0 ? std::sqrt(co) / p : 0.);
                h->GetXaxis()->SetBinLabel(r + 1, kReflName[r]);
            }
            h->SetTitle(TString::Format("Config %d", cfg));
            h->GetYaxis()->SetTitle("collection efficiency  N_{pe}/N_{#gamma}");
            h->GetYaxis()->SetTitleSize(0.050);
            h->GetYaxis()->SetTitleOffset(1.30);
            h->GetYaxis()->SetLabelSize(0.045);
            h->GetXaxis()->SetLabelSize(0.075);   // string bin labels
            h->GetYaxis()->SetRangeUser(0, ymaxAll * 1.15);
            h->SetBarWidth(0.7); h->SetBarOffset(0.15);
            h->SetFillColor(kAzure - 4);
            h->SetLineColor(kBlack);
            h->Draw("BAR2");
            gPad->SetGridy();
        }
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_eff_bars.%s", outdir, ex));
    }

    // ================= Fig 2: produced yield (cfg1) ======================
    auto stackDraw = [&](std::map<int, TH1D*>& hs, const char* ytitle,
                         const char* name, bool density, bool trimX) {
        TCanvas* c = new TCanvas("c_tmp", "", 760, 560);
        c->SetLeftMargin(0.14);
        c->SetBottomMargin(0.13);
        // Trim the displayed x-range to the populated region (get rid of the
        // empty tail left by the generous fixed binning) and make sure the
        // largest bins are not pushed into the overflow bin.
        if (trimX) {
            double lo = 1e30, hi = -1e30;
            for (int r = 0; r < 4; ++r) {
                TH1D* h = hs[r];
                int b0 = h->FindFirstBinAbove(0);
                int b1 = h->FindLastBinAbove(0);
                if (b0 < 0) continue;
                lo = std::min(lo, h->GetXaxis()->GetBinLowEdge(b0));
                hi = std::max(hi, h->GetXaxis()->GetBinUpEdge(b1));
            }
            if (hi > lo) {
                double margin = 0.05 * (hi - lo);
                for (int r = 0; r < 4; ++r)
                    hs[r]->GetXaxis()->SetRangeUser(lo - margin, hi + margin);
            }
        }
        double ymax = 0.;        for (auto& kv : hs) {
            TH1D* h = kv.second;
            if (density) {
                double I = h->Integral();
                if (I > 0) h->Scale(1. / I / h->GetBinWidth(1));
            }
            // draw-time y maximum of the trimmed range
            int b0 = h->GetXaxis()->GetFirst();
            int b1 = h->GetXaxis()->GetLast();
            for (int b = b0; b <= b1; ++b) ymax = std::max(ymax, h->GetBinContent(b));
        }
        if (ymax > 0) ymax *= 1.35;            // headroom for the legend

        TLegend* leg = new TLegend(0.58, 0.58, 0.90, 0.90);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);          // transparent (draw no fill)
        leg->SetFillColorAlpha(0, 0.);
        leg->SetTextFont(42);
        for (int r = 0; r < 4; ++r) {
            TH1D* h = hs[r];
            h->SetLineColor(kColor[r]); h->SetLineStyle(kStyle[r]); h->SetLineWidth(2);
            h->GetYaxis()->SetTitleSize(0.045);
            h->GetXaxis()->SetTitleSize(0.045);
            h->GetYaxis()->SetLabelSize(0.040);
            h->GetXaxis()->SetLabelSize(0.040);
            h->GetYaxis()->SetTitleOffset(1.25);
            if (ymax > 0) h->GetYaxis()->SetRangeUser(0, ymax);
            if (r == 0) { h->GetYaxis()->SetTitle(ytitle); h->Draw("HIST"); }
            else        { h->Draw("HIST SAME"); }
            leg->AddEntry(h, kReflName[r], "l");
        }
        leg->Draw();
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/%s.%s", outdir, name, ex));
    };
    stackDraw(hProduced, "events / bin", "fig_yield", false, true);
    stackDraw(hDetected, "events / bin", "fig_detected", false, true);
    stackDraw(hWave, "photoelectrons / nm", "fig_wavelength", true, false);

    // ================= Fig 4: efficiency vs radius =======================
    {
        TCanvas* c = new TCanvas("c_rad", "", 760, 560);
        c->SetLeftMargin(0.15);
        c->SetBottomMargin(0.13);
        c->SetLogy();
        // The bare curve sits near 2e-4 and the reflective ones near 3e-2, so
        // on the log axis the band between them is empty across all radii; the
        // legend goes there to avoid covering any points.
        TLegend* leg = new TLegend(0.17, 0.50, 0.46, 0.72);
        leg->SetBorderSize(0);
        leg->SetFillStyle(0);          // transparent (draw no fill)
        leg->SetFillColorAlpha(0, 0.);
        leg->SetTextFont(42);
        // A TMultiGraph computes its y-range from ALL member graphs, so on the
        // log axis the reflective curves are no longer clipped to the range of
        // whichever graph happened to be drawn first.
        TMultiGraph* mg = new TMultiGraph();
        for (int r = 0; r < 4; ++r) {
            std::vector<double> xx, yy, ex, ey;
            for (int i = 0; i < NR; ++i) {
                double p = rProd[1][r][i], co = rColl[1][r][i];
                if (p < 1) continue;
                xx.push_back((i + 0.5) * RMAX / NR);
                yy.push_back(co / p);
                ex.push_back(0.);
                ey.push_back(std::sqrt(co) / p);
            }
            TGraphErrors* g = new TGraphErrors((int)xx.size(), xx.data(), yy.data(),
                                               ex.data(), ey.data());
            g->SetLineColor(kColor[r]); g->SetMarkerColor(kColor[r]);
            g->SetLineStyle(kStyle[r]); g->SetLineWidth(2);
            g->SetMarkerStyle(20 + r); g->SetMarkerSize(0.8);
            g->SetTitle(TString::Format(";distance from beam axis r [mm];"
                                        "collection efficiency  N_{pe}/N_{#gamma}"));
            leg->AddEntry(g, kReflName[r], "lp");
            mg->Add(g, "PL");
        }
        mg->Draw("A");
        mg->GetXaxis()->SetTitleSize(0.045);
        mg->GetYaxis()->SetTitleSize(0.045);
        mg->GetXaxis()->SetLabelSize(0.040);
        mg->GetYaxis()->SetLabelSize(0.040);
        mg->GetYaxis()->SetTitleOffset(1.30);
        leg->Draw();
        for (auto ex : exts)
            c->SaveAs(TString::Format("%s/fig_radius.%s", outdir, ex));
    }

    printf("report_figures: wrote figures to %s\n", outdir);
}
