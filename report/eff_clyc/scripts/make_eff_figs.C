// Efficiency and spectrum figures for the CLYC distance-validation report.
//
// Simulation values are from scripts/analyze_clyc_eff.C (1e7 incident gammas
// per point, +/-2% total-energy window).  Measured values are the
// "Efficiency (%)" column of
// /home/turko46/data/CLYC_validation/effDist_clyc1+2.csv divided by 1000;
// their errors are Poisson errors on the net peak area.  Both detectors share
// the same simulated curve because they have the same crystal geometry.
//
// Run from the repository root:
//   root -l -b -q report/eff_clyc/scripts/make_eff_figs.C

#include <cmath>
#include <cstdio>

#include "TCanvas.h"
#include "TFile.h"
#include "TGraphErrors.h"
#include "TH1D.h"
#include "TH2F.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TBox.h"
#include "TColor.h"
#include "TF1.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TString.h"

namespace {

constexpr int kNDist = 3;
constexpr int kNE = 3;

const double kDist[kNDist] = {200., 300., 400.};
constexpr double kXStagger = 3.;
const double kE[kNE] = {661.64, 1173.23, 1332.51};

// Simulation: [distance][energy], percent.
const double kSim[kNDist][kNE] = {
    {0.00843, 0.00384, 0.00326},
    {0.00385, 0.00166, 0.00148},
    {0.00205, 0.00106, 0.00071}
};
const double kSimErr[kNDist][kNE] = {
    {0.00029, 0.00020, 0.00018},
    {0.00020, 0.00013, 0.00012},
    {0.00014, 0.00010, 0.00008}
};

// Measurement, CLYC1: [distance][energy], percent, after dividing the CSV by
// 1000.
const double kMeas1[kNDist][kNE] = {
    {0.00796, 0.00226, 0.00241},
    {0.00400, 0.00112, 0.00122},
    {0.00224, 0.00063, 0.00070}
};
const double kNetPeak1[kNDist][kNE] = {
    {148423., 153103., 163177.},
    {74617., 76163., 82693.},
    {41666., 42612., 47252.}
};

// Measurement, CLYC2: [distance][energy], percent, after dividing the CSV by
// 1000.  The printed 200 mm Cs-137 value is 0.83, a factor 10 below the value
// obtained from Eq. (eff-exp) with the other columns; it is treated as 8.27
// (i.e. 0.00827 after the /1000 correction) in the same way as all other rows.
const double kMeas2[kNDist][kNE] = {
    {0.00827, 0.00288, 0.00294},
    {0.00394, 0.00129, 0.00134},
    {0.00211, 0.00071, 0.00078}
};
const double kNetPeak2[kNDist][kNE] = {
    {23430., 20281., 20702.},
    {11158., 9073., 9478.},
    {5990., 5005., 5498.}
};

const int kColor[kNE] = {
    TColor::GetColor("#1f77b4"),
    TColor::GetColor("#d62728"),
    TColor::GetColor("#2ca02c")
};

const char* kTag[kNE] = {"661p64keV", "1173p23keV", "1332p51keV"};

void Style()
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetTextFont(42);
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetTitleSize(0.045, "XYZ");
    gStyle->SetLabelSize(0.040, "XYZ");
    gStyle->SetFrameLineWidth(1);
}

void SaveCrop(TCanvas* c, const char* out)
{
    TString raw(out);
    raw.ReplaceAll(".pdf", "_raw.pdf");
    c->SaveAs(raw);
    gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                       raw.Data(), out));
    gSystem->Unlink(raw);
}

TH2F* Frame(const char* name, double x0, double x1, double y0, double y1,
            const char* xt, const char* yt)
{
    auto* h = new TH2F(name, "", 100, x0, x1, 100, y0, y1);
    h->SetStats(0);
    h->GetXaxis()->SetTitle(xt);
    h->GetYaxis()->SetTitle(yt);
    h->GetXaxis()->SetTitleOffset(1.10);
    h->GetYaxis()->SetTitleOffset(1.35);
    h->GetXaxis()->CenterTitle();
    h->GetYaxis()->CenterTitle();
    return h;
}

TGraphErrors* MakeGraph(const double x[kNDist], const double y[kNDist],
                        const double ey[kNDist], int color, int marker,
                        int lineStyle)
{
    double ex[kNDist] = {0., 0., 0.};
    auto* g = new TGraphErrors(kNDist, x, y, ex, ey);
    g->SetLineColor(color);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(1.15);
    g->SetLineWidth(2);
    g->SetLineStyle(lineStyle);
    return g;
}

void EfficiencyVsDistance()
{
    auto* c = new TCanvas("c_eff", "", 900, 700);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.06);

    auto* frame = Frame("frame_eff", 180., 420., 4.e-4, 4.e-1,
                        "#font[132]{#it{d}}_{nom} [mm]",
                        "#font[132]{#it{#epsilon}} [%]");
    frame->Draw();
    c->SetLogy();

    // Geometric acceptance: crystal radius r = 12.5 mm, recessed by 2 mm (d = x + 2.0 mm)
    auto* facc = new TF1("f_acc",
        "100. * 0.5 * (1. - (x + 2.0) / sqrt((x + 2.0)*(x + 2.0) + 12.5*12.5))",
        180., 420.);
    facc->SetLineColor(kGray + 2);
    facc->SetLineStyle(2);
    facc->SetLineWidth(2);
    facc->Draw("SAME");

    TGraphErrors* gsim[kNE];
    TGraphErrors* gm1[kNE];
    TGraphErrors* gm2[kNE];

    // Small horizontal offsets for the two measurement sets.  They separate
    // the overlapping markers and are not part of the data; the simulation
    // and all numerical results use the nominal distance kDist.
    double x1[kNDist], x2[kNDist];
    for (int i = 0; i < kNDist; ++i) {
        x1[i] = kDist[i] - kXStagger;
        x2[i] = kDist[i] + kXStagger;
    }

    for (int j = 0; j < kNE; ++j) {
        double ys[kNDist], es[kNDist];
        double y1[kNDist], e1[kNDist], y2[kNDist], e2[kNDist];
        for (int i = 0; i < kNDist; ++i) {
            ys[i] = kSim[i][j];
            es[i] = kSimErr[i][j];
            y1[i] = kMeas1[i][j];
            e1[i] = kMeas1[i][j] / std::sqrt(kNetPeak1[i][j]);
            y2[i] = kMeas2[i][j];
            e2[i] = kMeas2[i][j] / std::sqrt(kNetPeak2[i][j]);
        }
        gsim[j] = MakeGraph(kDist, ys, es, kColor[j], 20, 1);
        gm1[j] = MakeGraph(x1, y1, e1, kColor[j], 24, 1);
        gm2[j] = MakeGraph(x2, y2, e2, kColor[j], 25, 1);
    }

    for (int j = 0; j < kNE; ++j) {
        gsim[j]->Draw("LP SAME");
        gm1[j]->Draw("P SAME");
        gm2[j]->Draw("P SAME");
    }

    auto* leg = new TLegend(0.47, 0.72, 0.77, 0.93);
    leg->SetNColumns(1);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.030);
    for (int j = 0; j < kNE; ++j) {
        leg->AddEntry(gsim[j], Form("%.2f keV", kE[j]), "LP");
    }
    leg->AddEntry(facc, "geom. acceptance", "L");
    leg->Draw();

    auto* legdet = new TLegend(0.73, 0.55, 0.94, 0.70);
    legdet->SetNColumns(1);
    legdet->SetBorderSize(0);
    legdet->SetFillStyle(0);
    legdet->SetTextSize(0.030);
    TGraph* gd1 = new TGraph();
    gd1->SetMarkerStyle(24);
    gd1->SetMarkerColor(kBlack);
    gd1->SetLineColor(kBlack);
    TGraph* gd2 = new TGraph();
    gd2->SetMarkerStyle(25);
    gd2->SetMarkerColor(kBlack);
    gd2->SetLineColor(kBlack);
    legdet->AddEntry(gd1, "CLYC1 meas.", "P");
    legdet->AddEntry(gd2, "CLYC2 meas.", "P");
    legdet->Draw();

    SaveCrop(c, "report/eff_clyc/figures/fig_eff_vs_distance.pdf");
}

void EfficiencyRatio()
{
    auto* c = new TCanvas("c_ratio", "", 900, 700);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.06);

    auto* frame = Frame("frame_ratio", 180., 420., 0.8, 2.05,
                        "#font[132]{#it{d}}_{nom} [mm]",
                        "#font[132]{#it{R}}");
    frame->Draw();

    auto* band = new TBox(180., 0.9, 420., 1.1);
    band->SetFillColorAlpha(kGreen - 10, 0.15);
    band->SetLineColor(kGreen - 7);
    band->SetLineStyle(2);
    band->Draw("F");
    band->Draw("L");

    auto* one = new TLine(180., 1.0, 420., 1.0);
    one->SetLineStyle(2);
    one->SetLineColor(kGray + 2);
    one->Draw();

    TGraphErrors* grat1[kNE];
    TGraphErrors* grat2[kNE];
    double x1[kNDist], x2[kNDist];
    for (int i = 0; i < kNDist; ++i) {
        x1[i] = kDist[i] - kXStagger;
        x2[i] = kDist[i] + kXStagger;
    }
    for (int j = 0; j < kNE; ++j) {
        double y1[kNDist], ey1[kNDist], y2[kNDist], ey2[kNDist];
        for (int i = 0; i < kNDist; ++i) {
            const double s = kSim[i][j];
            const double es = kSimErr[i][j];

            const double m1 = kMeas1[i][j];
            const double em1 = m1 / std::sqrt(kNetPeak1[i][j]);
            y1[i] = s / m1;
            ey1[i] = y1[i] * std::sqrt(std::pow(es / s, 2) +
                                      std::pow(em1 / m1, 2));

            const double m2 = kMeas2[i][j];
            const double em2 = m2 / std::sqrt(kNetPeak2[i][j]);
            y2[i] = s / m2;
            ey2[i] = y2[i] * std::sqrt(std::pow(es / s, 2) +
                                      std::pow(em2 / m2, 2));
        }
        grat1[j] = MakeGraph(x1, y1, ey1, kColor[j], 24, 1);
        grat2[j] = MakeGraph(x2, y2, ey2, kColor[j], 25, 2);
    }

    for (int j = 0; j < kNE; ++j) {
        grat1[j]->Draw("LP SAME");
        grat2[j]->Draw("LP SAME");
    }

    auto* leg = new TLegend(0.17, 0.76, 0.93, 0.92);
    leg->SetNColumns(2);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.032);
    for (int j = 0; j < kNE; ++j) {
        leg->AddEntry(grat1[j], Form("%.2f keV, CLYC1", kE[j]), "LP");
        leg->AddEntry(grat2[j], Form("%.2f keV, CLYC2", kE[j]), "LP");
    }
    leg->Draw();

    auto* t = new TLatex();
    t->SetNDC();
    t->SetTextFont(42);
    t->SetTextSize(0.032);
    t->DrawLatex(0.20, 0.21, "shaded band: #pm10%");
    t->DrawLatex(0.20, 0.16, "dashed line: ratio = 1");

    SaveCrop(c, "report/eff_clyc/figures/fig_eff_ratio.pdf");
}

void IntrinsicVsDistance()
{
    auto* c = new TCanvas("c_int", "", 900, 700);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.06);

    auto* frame = Frame("frame_int", 180., 420., 2., 20.,
                        "#font[132]{#it{d}}_{nom} [mm]",
                        "#font[132]{#it{#epsilon}}_{int} [%]");
    frame->Draw();
    c->SetLogy();

    auto acceptance = [](double dnom) {
        const double d = dnom + 2.0;
        return 100. * 0.5 *
               (1. - d / std::sqrt(d * d + 12.5 * 12.5));
    };

    TGraphErrors* gsim[kNE];
    TGraphErrors* gm1[kNE];
    TGraphErrors* gm2[kNE];

    double x1[kNDist], x2[kNDist];
    for (int i = 0; i < kNDist; ++i) {
        x1[i] = kDist[i] - kXStagger;
        x2[i] = kDist[i] + kXStagger;
    }

    for (int j = 0; j < kNE; ++j) {
        double ys[kNDist], es[kNDist];
        double y1[kNDist], e1[kNDist], y2[kNDist], e2[kNDist];
        for (int i = 0; i < kNDist; ++i) {
            const double a = acceptance(kDist[i]);
            ys[i] = 100. * kSim[i][j] / a;
            es[i] = 100. * kSimErr[i][j] / a;
            y1[i] = 100. * kMeas1[i][j] / a;
            e1[i] = 100. * (kMeas1[i][j] /
                            std::sqrt(kNetPeak1[i][j])) / a;
            y2[i] = 100. * kMeas2[i][j] / a;
            e2[i] = 100. * (kMeas2[i][j] /
                            std::sqrt(kNetPeak2[i][j])) / a;
        }
        gsim[j] = MakeGraph(kDist, ys, es, kColor[j], 20, 1);
        gm1[j] = MakeGraph(x1, y1, e1, kColor[j], 24, 1);
        gm2[j] = MakeGraph(x2, y2, e2, kColor[j], 25, 1);
    }

    for (int j = 0; j < kNE; ++j) {
        gsim[j]->Draw("LP SAME");
        gm1[j]->Draw("P SAME");
        gm2[j]->Draw("P SAME");
    }

    auto* leg = new TLegend(0.47, 0.72, 0.80, 0.93);
    leg->SetNColumns(1);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.030);
    for (int j = 0; j < kNE; ++j) {
        leg->AddEntry(gsim[j], Form("%.2f keV", kE[j]), "LP");
    }
    TGraph* gd1 = new TGraph();
    gd1->SetMarkerStyle(24);
    gd1->SetMarkerColor(kBlack);
    gd1->SetLineColor(kBlack);
    TGraph* gd2 = new TGraph();
    gd2->SetMarkerStyle(25);
    gd2->SetMarkerColor(kBlack);
    gd2->SetLineColor(kBlack);
    leg->AddEntry(gd1, "CLYC1 meas.", "P");
    leg->AddEntry(gd2, "CLYC2 meas.", "P");
    leg->Draw();

    SaveCrop(c, "report/eff_clyc/figures/fig_intrinsic_vs_distance.pdf");
}

void Spectra200mm()
{
    auto* c = new TCanvas("c_spec", "", 1500, 600);
    c->Divide(3, 1, 0.002, 0.002);

    for (int j = 0; j < kNE; ++j) {
        c->cd(j + 1);
        gPad->SetLogy();
        gPad->SetLeftMargin(0.15);
        gPad->SetRightMargin(0.035);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.12);

        TString fname = Form("build/validation/clyc_eff/"
                             "clyc_eff_200mm_%s_eff.root", kTag[j]);
        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie()) {
            std::printf("ERROR: cannot open %s\n", fname.Data());
            continue;
        }
        auto* h = static_cast<TH1D*>(f->Get("h_total_edep"));
        if (!h) {
            std::printf("ERROR: no h_total_edep in %s\n", fname.Data());
            f->Close();
            continue;
        }
        h->SetDirectory(nullptr);

        const double bw_keV = h->GetBinWidth(1) * 1000.;
        h->Scale(1.0 / bw_keV);  // per 1e7 incident per keV
        h->SetTitle(Form("200 mm, %.2f keV", kE[j]));
        h->GetXaxis()->SetTitle("#font[132]{#it{E}}_{dep} [MeV]");
        h->GetYaxis()->SetTitle(j == 0 ? "events / (10^{7} #times keV)" : "");
        h->GetXaxis()->SetTitleOffset(1.10);
        h->GetYaxis()->SetTitleOffset(1.45);
        h->GetXaxis()->CenterTitle();
        h->GetYaxis()->CenterTitle();
        h->SetLineColor(kBlack);
        h->SetLineWidth(1);
        h->SetStats(0);
        h->SetMinimum(0.1);
        h->SetMaximum(1000.);
        h->GetXaxis()->SetRangeUser(0., 1.5 * kE[j] / 1000.);

        const double EMeV = kE[j] / 1000.;
        auto* win = new TBox(EMeV * 0.98, 0.1, EMeV * 1.02, 1000.);
        win->SetFillStyle(0);
        win->SetLineColor(kAzure + 1);
        win->SetLineStyle(2);
        h->Draw("HIST");
        win->Draw("L SAME");

        auto* line = new TLine(EMeV, 0.1, EMeV, 1000.);
        line->SetLineColor(kRed + 1);
        line->SetLineStyle(2);
        line->Draw();

        auto* t = new TLatex();
        t->SetTextFont(42);
        t->SetTextSize(0.045);
        t->DrawLatex(EMeV * 1.05, 300., "#pm2%");

        auto* lab = new TLatex();
        lab->SetNDC();
        lab->SetTextFont(42);
        lab->SetTextSize(0.055);
        lab->DrawLatex(0.15, 0.90, Form("%.2f keV", kE[j]));

        f->Close();
    }

    SaveCrop(c, "report/eff_clyc/figures/fig_spectra_200mm.pdf");
}

}  // namespace

void make_eff_figs()
{
    Style();
    gSystem->mkdir("report/eff_clyc/figures", kTRUE);
    EfficiencyVsDistance();
    EfficiencyRatio();
    IntrinsicVsDistance();
    Spectra200mm();
    std::printf("wrote report/eff_clyc/figures/fig_eff_vs_distance.pdf\n");
    std::printf("wrote report/eff_clyc/figures/fig_eff_ratio.pdf\n");
    std::printf("wrote report/eff_clyc/figures/fig_intrinsic_vs_distance.pdf\n");
    std::printf("wrote report/eff_clyc/figures/fig_spectra_200mm.pdf\n");
}
