// al_backing.C -- aluminium-foil backing (grease vs air) vs position and layout.
//
// Usage (from the repository root):
//   root -l -b -q 'macros/al_backing.C+("/path/to/al","/path/to/matrix","report/figures")'
//
// Defaults point at the v5 data trees:
//   al      .../data_v5/al
//   matrix  .../data_v5/matrix
//   outdir  report/figures
//
// Writes <outdir>/al_backing.csv, al_backing.pdf and al_backing.png.

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

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#if __has_include("style.h")
#include "style.h"
#elif __has_include("../../../root-work/style.h")
#include "../../../root-work/style.h"
#else
#error "style.h not found; add its directory to ROOT_INCLUDE_PATH"
#endif

namespace {

const char* kPosName[3]  = {"centre", "edge", "corner"};
const char* kPosLabel[3] = {"centre", "edge", "corner"};
const char* kBackName[2] = {"grease", "air"};

struct Stat {
    double mean = 0.;
    double sem  = 0.;
    int    n    = 0;
    bool   ok   = false;
};

Stat readStat(const TString& fname)
{
    Stat s;
    TFile* f = TFile::Open(fname, "READ");
    if (!f || f->IsZombie()) { delete f; return s; }

    TTree* hits = (TTree*)f->Get("sipmHits");
    TTree* stat = (TTree*)f->Get("opticalStats");
    if (!hits) { delete f; return s; }

    int evt = 0, nDet = 0;
    hits->SetBranchAddress("evtNb", &evt);
    hits->SetBranchAddress("nDetected", &nDet);

    std::map<int, double> sums;
    for (long i = 0; i < hits->GetEntries(); ++i) {
        hits->GetEntry(i);
        sums[evt] += nDet;
    }

    std::vector<double> pe;
    if (stat) {
        int sevt = 0;
        stat->SetBranchAddress("evtNb", &sevt);
        pe.reserve(stat->GetEntries());
        for (long i = 0; i < stat->GetEntries(); ++i) {
            stat->GetEntry(i);
            auto it = sums.find(sevt);
            pe.push_back(it == sums.end() ? 0. : it->second);
        }
    } else {
        for (const auto& kv : sums) pe.push_back(kv.second);
    }

    delete f;

    s.n = (int)pe.size();
    if (s.n == 0) return s;

    double sum = 0.;
    for (double v : pe) sum += v;
    s.mean = sum / s.n;

    if (s.n > 1) {
        double ss = 0.;
        for (double v : pe) ss += (v - s.mean) * (v - s.mean);
        s.sem = std::sqrt(ss / (s.n - 1) / s.n);
    }

    s.ok = true;
    return s;
}

TH1D* makePosFrame(const char* name, double ylo, double yhi)
{
    TH1D* h = new TH1D(name, "", 3, 0, 3);
    for (int i = 0; i < 3; ++i) h->GetXaxis()->SetBinLabel(i + 1, kPosLabel[i]);
    h->GetYaxis()->SetRangeUser(ylo, yhi);
    h->GetXaxis()->SetLabelSize(0.075);
    h->GetXaxis()->SetTickLength(0.);
    h->GetYaxis()->SetTitleSize(0.048);
    h->GetYaxis()->SetTitleOffset(1.45);
    h->GetYaxis()->SetLabelSize(0.047);
    return h;
}

}  // namespace

void al_backing(const char* alDir =
                    "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5/al",
                const char* matrixDir =
                    "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5/matrix",
                const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);
    gSystem->mkdir(outdir, kTRUE);

    const int kColorGrease = PetroffPalette::getP6(0);
    const int kColorAir    = PetroffPalette::getP6(1);
    const int kColorTiO2   = PetroffPalette::getP6(2);
    const int kColorGlossy = PetroffPalette::getP6(3);
    const int kColorCfg[4] = {0, PetroffPalette::getP6(0),
                              PetroffPalette::getP6(1),
                              PetroffPalette::getP6(2)};
    const int kStyleCfg[4] = {0, kSolid, kDashed, kDotted};

    Stat al[4][2][3];
    Stat tio2[4][3], glossy[4][3], matrixAl[4][3];

    int alRead = 0, refRead = 0, alRefRead = 0;

    for (int cfg = 1; cfg <= 3; ++cfg) {
        for (int b = 0; b < 2; ++b) {
            for (int p = 0; p < 3; ++p) {
                const TString fn = TString::Format(
                    "%s/c%d_%s/muon_out_c%d_r2_%s.root",
                    alDir, cfg, kBackName[b], cfg, kPosName[p]);
                al[cfg][b][p] = readStat(fn);
                if (al[cfg][b][p].ok) ++alRead;
            }
        }
        for (int p = 0; p < 3; ++p) {
            const TString base = TString::Format("%s/muon_out_c%d_r%%d_%s.root",
                                                 matrixDir, cfg, kPosName[p]);
            tio2[cfg][p]     = readStat(TString::Format(base, 1));
            glossy[cfg][p]   = readStat(TString::Format(base, 3));
            matrixAl[cfg][p] = readStat(TString::Format(base, 2));
            if (tio2[cfg][p].ok) ++refRead;
            if (glossy[cfg][p].ok) ++refRead;
            if (matrixAl[cfg][p].ok) ++alRefRead;
        }
    }

    printf("\nal_backing: read %d/18 al files, %d/18 matrix TiO2+glossy refs, "
           "%d/9 matrix Al-grease files\n", alRead, refRead, alRefRead);

    double ratio[4][3], ratioSem[4][3];
    for (int cfg = 1; cfg <= 3; ++cfg) {
        for (int p = 0; p < 3; ++p) {
            const Stat& g = al[cfg][0][p];
            const Stat& a = al[cfg][1][p];
            if (g.ok && a.ok && g.mean > 0.) {
                ratio[cfg][p] = a.mean / g.mean;
                ratioSem[cfg][p] = ratio[cfg][p] *
                    std::sqrt(std::pow(a.sem / a.mean, 2) +
                              std::pow(g.sem / g.mean, 2));
            } else {
                ratio[cfg][p] = 0.;
                ratioSem[cfg][p] = 0.;
            }
        }
    }

    const TString csv = TString::Format("%s/al_backing.csv", outdir);
    FILE* fp = fopen(csv, "w");
    if (fp) {
        fprintf(fp, "config,backing,position,events,mean_npe,sem_npe,"
                    "ratio_air_over_grease,ratio_sem,tio2_ref,glossy_ref,"
                    "matrix_al_grease\n");
        for (int cfg = 1; cfg <= 3; ++cfg) {
            for (int p = 0; p < 3; ++p) {
                for (int b = 0; b < 2; ++b) {
                    const Stat& s = al[cfg][b][p];
                    fprintf(fp, "%d,%s,%s,%d,%.4f,%.4f,%.5f,%.5f,%.4f,%.4f,%.4f\n",
                            cfg, kBackName[b], kPosName[p], s.n,
                            s.mean, s.sem, ratio[cfg][p], ratioSem[cfg][p],
                            tio2[cfg][p].mean, glossy[cfg][p].mean,
                            matrixAl[cfg][p].mean);
                }
            }
        }
        fclose(fp);
    }

    printf("\n cfg pos     grease [pe]        air [pe]        air/grease"
           "     TiO2     glossy   matrix Al   Al-matrix\n");
    printf(" -------------------------------------------------------------------------"
           "----------------------------\n");
    for (int cfg = 1; cfg <= 3; ++cfg) {
        for (int p = 0; p < 3; ++p) {
            const Stat& g = al[cfg][0][p];
            const Stat& a = al[cfg][1][p];
            const Stat& m = matrixAl[cfg][p];
            const double dm = (m.mean > 0. && g.ok)
                ? 100. * (g.mean - m.mean) / m.mean : 0.;
            printf("  %d  %-6s %7.2f +- %5.2f   %7.2f +- %5.2f   "
                   "%5.3f +- %.3f   %6.2f   %6.2f    %7.2f   %+6.2f%%\n",
                   cfg, kPosName[p],
                   g.mean, g.sem, a.mean, a.sem,
                   ratio[cfg][p], ratioSem[cfg][p],
                   tio2[cfg][p].mean, glossy[cfg][p].mean,
                   m.mean, dm);
        }
    }

    double cfgMax[4] = {0., 0., 0., 0.};
    for (int cfg = 1; cfg <= 3; ++cfg) {
        for (int b = 0; b < 2; ++b)
            for (int p = 0; p < 3; ++p)
                cfgMax[cfg] = std::max(cfgMax[cfg], al[cfg][b][p].mean);
        for (int p = 0; p < 3; ++p)
            cfgMax[cfg] = std::max(cfgMax[cfg],
                std::max(tio2[cfg][p].mean, glossy[cfg][p].mean));
    }

    TCanvas* c = new TCanvas("c_al_backing", "", 1200, 980);
    c->Divide(2, 2, 0.006, 0.006);

    TLegend* legRef = nullptr;

    for (int cfg = 1; cfg <= 3; ++cfg) {
        c->cd(cfg);
        gStyle->SetOptTitle(1);
        gPad->SetLeftMargin(0.17);
        gPad->SetRightMargin(0.04);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.18);

        TH1D* frame = makePosFrame(TString::Format("fr_al_c%d", cfg),
                                   0., cfgMax[cfg] * 1.15);
        frame->SetTitle(TString::Format("Config %d", cfg));
        frame->GetYaxis()->SetTitle("mean photoelectrons / event");
        frame->GetXaxis()->SetLabelSize(0.044);
        frame->GetYaxis()->SetLabelSize(0.044);
        frame->GetXaxis()->SetTitleSize(0.050);
        frame->GetYaxis()->SetTitleSize(0.050);
        frame->GetXaxis()->SetTitleOffset(1.15);
        frame->GetYaxis()->SetTitleOffset(1.35);
        frame->Draw("HIST");

        std::vector<double> xg, yg, eg, xa, ya, ea;
        for (int p = 0; p < 3; ++p) {
            xg.push_back(p + 0.42);
            yg.push_back(al[cfg][0][p].mean);
            eg.push_back(al[cfg][0][p].sem);
            xa.push_back(p + 0.58);
            ya.push_back(al[cfg][1][p].mean);
            ea.push_back(al[cfg][1][p].sem);
        }

        TGraphErrors* gG = new TGraphErrors(3, xg.data(), yg.data(),
                                            nullptr, eg.data());
        gG->SetLineColor(kColorGrease);
        gG->SetMarkerColor(kColorGrease);
        gG->SetMarkerStyle(20);
        gG->SetMarkerSize(1.15);
        gG->SetLineWidth(2);
        gG->Draw("P SAME");

        TGraphErrors* gA = new TGraphErrors(3, xa.data(), ya.data(),
                                            nullptr, ea.data());
        gA->SetLineColor(kColorAir);
        gA->SetMarkerColor(kColorAir);
        gA->SetMarkerStyle(21);
        gA->SetMarkerSize(1.15);
        gA->SetLineWidth(2);
        gA->Draw("P SAME");

        for (int p = 0; p < 3; ++p) {
            const double x1 = p + 0.27, x2 = p + 0.73;
            TLine* lt = new TLine(x1, tio2[cfg][p].mean,
                                  x2, tio2[cfg][p].mean);
            lt->SetLineColor(kColorTiO2);
            lt->SetLineStyle(kDashed);
            lt->SetLineWidth(2);
            lt->Draw("SAME");

            TLine* lg = new TLine(x1, glossy[cfg][p].mean,
                                  x2, glossy[cfg][p].mean);
            lg->SetLineColor(kColorGlossy);
            lg->SetLineStyle(kDotted);
            lg->SetLineWidth(2);
            lg->Draw("SAME");
        }

        if (cfg == 1) {
            // Legend in the top margin.  With top margin = 0.18 the legend at
            // y = 0.78..0.97 sits above the frame top (~0.82).  Use 4 columns
            // so the long "TiO2 (matrix r1)" / "glossy (matrix r3)" labels each
            // get their own column and don't overlap the marker entries.
            legRef = new TLegend(0.10, 0.78, 0.98, 0.97);
            legRef->SetBorderSize(0);
            legRef->SetFillStyle(0);
            legRef->SetTextSize(0.034);
            legRef->SetNColumns(4);
            legRef->AddEntry(gG, "Al grease", "p");
            legRef->AddEntry(gA, "Al air", "p");
            TLine* dT = new TLine();
            dT->SetLineColor(kColorTiO2);
            dT->SetLineStyle(kDashed);
            dT->SetLineWidth(2);
            legRef->AddEntry(dT, "TiO_{2} (matrix r1)", "l");
            TLine* dG = new TLine();
            dG->SetLineColor(kColorGlossy);
            dG->SetLineStyle(kDotted);
            dG->SetLineWidth(2);
            legRef->AddEntry(dG, "glossy (matrix r3)", "l");
            legRef->Draw();
        }
    }

    c->cd(4);
    gStyle->SetOptTitle(1);
    gPad->SetLeftMargin(0.17);
    gPad->SetRightMargin(0.04);
    gPad->SetBottomMargin(0.16);
    gPad->SetTopMargin(0.30);

    double rLo = 1e30, rHi = -1e30;
    for (int cfg = 1; cfg <= 3; ++cfg) {
        for (int p = 0; p < 3; ++p) {
            if (ratio[cfg][p] <= 0.) continue;
            rLo = std::min(rLo, ratio[cfg][p] - ratioSem[cfg][p]);
            rHi = std::max(rHi, ratio[cfg][p] + ratioSem[cfg][p]);
        }
    }
    if (rHi < rLo) { rLo = 0.; rHi = 1.; }
    rLo = std::max(0., rLo - 0.05);
    rHi += 0.05;

    TH1D* rframe = makePosFrame("fr_al_ratio", rLo, rHi);
    rframe->SetTitle("Air / grease enhancement");
    rframe->GetYaxis()->SetTitle("air / grease");
    rframe->GetXaxis()->SetLabelSize(0.044);
    rframe->GetYaxis()->SetLabelSize(0.044);
    rframe->GetXaxis()->SetTitleSize(0.050);
    rframe->GetYaxis()->SetTitleSize(0.050);
    rframe->GetXaxis()->SetTitleOffset(1.15);
    rframe->GetYaxis()->SetTitleOffset(1.35);
    rframe->Draw("HIST");

    // Move the legend to the top margin (the lower-right of the data area
    // was overlapping the cfg 1/2/3 markers and their error bars in the
    // y=2.3..2.6 band).
    TLegend* legRatio = new TLegend(0.55, 0.83, 0.97, 0.97);
    legRatio->SetBorderSize(0);
    legRatio->SetFillStyle(0);
    legRatio->SetTextSize(0.038);

    for (int cfg = 1; cfg <= 3; ++cfg) {
        std::vector<double> x, y, ey;
        for (int p = 0; p < 3; ++p) {
            if (ratio[cfg][p] <= 0.) continue;
            x.push_back(p + 0.5);
            y.push_back(ratio[cfg][p]);
            ey.push_back(ratioSem[cfg][p]);
        }
        TGraphErrors* g = new TGraphErrors((int)x.size(), x.data(), y.data(),
                                           nullptr, ey.data());
        g->SetLineColor(kColorCfg[cfg]);
        g->SetMarkerColor(kColorCfg[cfg]);
        g->SetLineStyle(kStyleCfg[cfg]);
        g->SetLineWidth(2);
        g->SetMarkerStyle(20 + cfg - 1);
        g->SetMarkerSize(1.1);
        g->Draw("PL SAME");
        legRatio->AddEntry(g, TString::Format("config %d", cfg), "lp");
    }
    legRatio->Draw();

    for (const char* ex : {"pdf", "png"})
        c->SaveAs(TString::Format("%s/al_backing.%s", outdir, ex));

    printf("\nal_backing: wrote %s/al_backing.{csv,pdf,png}\n", outdir);
}
