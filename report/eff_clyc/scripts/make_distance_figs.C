// CLYC2 distance-comparison figures using only the peak-integration
// efficiencies from the 2026-10-27 spectra (Gaussian-plus-linear fit).
// No external efficiency CSV is used.
//
// Run from the repository root:
//   root -l -b -q report/eff_clyc/scripts/make_distance_figs.C

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include "TCanvas.h"
#include "TBox.h"
#include "TColor.h"
#include "TGraphErrors.h"
#include "TH2F.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"

namespace {

constexpr int kNDist = 3;
constexpr int kNE = 3;
const double kDist[kNDist] = {200., 300., 400.};
constexpr double kXStagger = 3.;
const double kE[kNE] = {661.64, 1173.23, 1332.51};
const char* kTag[kNE] = {"661p64", "1173p23", "1332p51"};
const int kColor[kNE] = {
    TColor::GetColor("#1f77b4"),
    TColor::GetColor("#d62728"),
    TColor::GetColor("#2ca02c")
};

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

struct ExpPoint {
    double eff = 0.;
    double err = 0.;
};

using Key = std::tuple<int, std::string, double>;

std::map<Key, ExpPoint> ReadExp(const char* path)
{
    std::map<Key, ExpPoint> out;
    std::ifstream in(path);
    if (!in.is_open()) {
        std::printf("ERROR: cannot open %s\n", path);
        return out;
    }
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> f;
        while (std::getline(ss, field, ',')) f.push_back(field);
        if (f.size() < 41) continue;
        if (f[0] != "CLYC2") continue;
        const int dist = std::atoi(f[1].c_str());
        const std::string src = f[2];
        const double e = std::atof(f[3].c_str());
        ExpPoint p;
        p.eff = std::atof(f[23].c_str());
        p.err = std::atof(f[24].c_str());
        out[Key(dist, src, e)] = p;
    }
    return out;
}

int EnergyIndex(double e)
{
    int best = 0;
    double diff = 1e9;
    for (int i = 0; i < kNE; ++i) {
        const double d = std::fabs(e - kE[i]);
        if (d < diff) { diff = d; best = i; }
    }
    return best;
}

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

void EfficiencyVsDistance(const std::map<Key, ExpPoint>& exp)
{
    auto* c = new TCanvas("c_eff_clcy2", "", 900, 700);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.06);

    auto* frame = Frame("frame_eff_clcy2", 180., 420., 4.e-4, 4.e-1,
                        "#font[132]{#it{d}}_{nom} [mm]",
                        "#font[132]{#it{#epsilon}} [%]");
    frame->Draw();
    c->SetLogy();

    auto* facc = new TF1("f_acc_clcy2",
        "100. * 0.5 * (1. - (x + 2.0) / sqrt((x + 2.0)*(x + 2.0) + 12.5*12.5))",
        180., 420.);
    facc->SetLineColor(kGray + 2);
    facc->SetLineStyle(2);
    facc->SetLineWidth(2);
    facc->Draw("SAME");

    // Offset the simulation and experiment markers horizontally by a small
    // amount so the overlapping points at each nominal distance are readable.
    // The offsets are graphical only; all quoted values use kDist.
    double xSim[kNDist], xExp[kNDist];
    for (int i = 0; i < kNDist; ++i) {
        xSim[i] = kDist[i] - kXStagger;
        xExp[i] = kDist[i] + kXStagger;
    }

    TGraphErrors* gsim[kNE];
    TGraphErrors* gexp[kNE];
    for (int j = 0; j < kNE; ++j) {
        double ys[kNDist], es[kNDist], ye[kNDist], ee[kNDist];
        for (int i = 0; i < kNDist; ++i) {
            ys[i] = kSim[i][j];
            es[i] = kSimErr[i][j];
            const std::string src = (j == 0 ? "CS" : "Co");
            const auto it = exp.find(Key(static_cast<int>(kDist[i]), src, kE[j]));
            ye[i] = (it == exp.end() ? 0. : it->second.eff);
            ee[i] = (it == exp.end() ? 0. : it->second.err);
        }
        gsim[j] = new TGraphErrors(kNDist, xSim, ys, nullptr, es);
        gsim[j]->SetLineColor(kColor[j]);
        gsim[j]->SetMarkerColor(kColor[j]);
        gsim[j]->SetMarkerStyle(20);
        gsim[j]->SetMarkerSize(1.1);
        gsim[j]->SetLineWidth(2);

        gexp[j] = new TGraphErrors(kNDist, xExp, ye, nullptr, ee);
        gexp[j]->SetLineColor(kColor[j]);
        gexp[j]->SetMarkerColor(kColor[j]);
        gexp[j]->SetMarkerStyle(24);
        gexp[j]->SetMarkerSize(1.15);
        gexp[j]->SetLineWidth(0);
    }

    for (int j = 0; j < kNE; ++j) {
        gsim[j]->Draw("LP SAME");
        gexp[j]->Draw("P SAME");
    }

    auto* leg = new TLegend(0.50, 0.70, 0.95, 0.93);
    leg->SetNColumns(1);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.030);
    for (int j = 0; j < kNE; ++j) {
        leg->AddEntry(gsim[j], Form("%.2f keV", kE[j]), "LP");
    }
    TGraph* gs = new TGraph();
    gs->SetMarkerStyle(20);
    gs->SetMarkerColor(kBlack);
    TGraph* ge = new TGraph();
    ge->SetMarkerStyle(24);
    ge->SetMarkerColor(kBlack);
    leg->AddEntry(gs, "simulation (filled)", "P");
    leg->AddEntry(ge, "experiment (open)", "P");
    leg->AddEntry(facc, "geometric bound", "L");
    leg->Draw();

    SaveCrop(c, "report/eff_clyc/figures/fig_eff_vs_distance.pdf");
}

void EfficiencyRatio(const std::map<Key, ExpPoint>& exp)
{
    auto* c = new TCanvas("c_ratio_clcy2", "", 900, 700);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.06);

    auto* frame = Frame("frame_ratio_clcy2", 180., 420., 0.7, 1.6,
                        "#font[132]{#it{d}}_{nom} [mm]",
                        "#font[132]{#it{R}} = #epsilon_{sim}/#epsilon_{exp}");
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

    TGraphErrors* grat[kNE];
    for (int j = 0; j < kNE; ++j) {
        double y[kNDist], ey[kNDist];
        for (int i = 0; i < kNDist; ++i) {
            const std::string src = (j == 0 ? "CS" : "Co");
            const auto it = exp.find(Key(static_cast<int>(kDist[i]), src, kE[j]));
            const double m = (it == exp.end() ? 0. : it->second.eff);
            const double em = (it == exp.end() ? 0. : it->second.err);
            y[i] = (m > 0. ? kSim[i][j] / m : 0.);
            ey[i] = (m > 0. ? y[i] * std::sqrt(
                std::pow(kSimErr[i][j] / kSim[i][j], 2) +
                std::pow(em / m, 2)) : 0.);
        }
        grat[j] = new TGraphErrors(kNDist, kDist, y, nullptr, ey);
        grat[j]->SetLineColor(kColor[j]);
        grat[j]->SetMarkerColor(kColor[j]);
        grat[j]->SetMarkerStyle(20);
        grat[j]->SetMarkerSize(1.1);
        grat[j]->SetLineWidth(2);
    }

    for (int j = 0; j < kNE; ++j) grat[j]->Draw("LP SAME");

    auto* leg = new TLegend(0.17, 0.76, 0.60, 0.93);
    leg->SetNColumns(1);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.032);
    for (int j = 0; j < kNE; ++j) {
        leg->AddEntry(grat[j], Form("%.2f keV", kE[j]), "LP");
    }
    leg->Draw();

    auto* t = new TLatex();
    t->SetNDC();
    t->SetTextFont(42);
    t->SetTextSize(0.032);
    t->DrawLatex(0.60, 0.30, "shaded band: #pm10%");
    t->DrawLatex(0.60, 0.24, "dashed line: ratio = 1");

    SaveCrop(c, "report/eff_clyc/figures/fig_eff_ratio.pdf");
}

} // namespace

void make_distance_figs(
    const char* peakCsv = "report/eff_clyc/data/exp_peak_integrals.csv")
{
    Style();
    const auto exp = ReadExp(peakCsv);
    if (exp.empty()) return;
    gSystem->mkdir("report/eff_clyc/figures", kTRUE);
    EfficiencyVsDistance(exp);
    EfficiencyRatio(exp);
    std::printf("wrote fig_eff_vs_distance.pdf and fig_eff_ratio.pdf\n");
}
