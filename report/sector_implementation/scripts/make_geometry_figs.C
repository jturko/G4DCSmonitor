// Geometry figures for the DCS-Monitor background-sector report.
// Run from the repository root:
//   root -l -b -q report/sector_implementation/scripts/make_geometry_figs.C

#include <cmath>
#include <vector>
#include <string>
#include "TCanvas.h"
#include "TBox.h"
#include "TEllipse.h"
#include "TGraph.h"
#include "TH2F.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TMarker.h"
#include "TArrow.h"
#include "TStyle.h"
#include "TColor.h"
#include "TMath.h"
#include "TString.h"
#include "TSystem.h"

namespace {

constexpr double kCaskBodyR = 1270.;
constexpr double kCaskFinR  = 1325.;
constexpr double kSectorRin = 1325.;
constexpr double kSectorRout = 1775.;
constexpr double kSectorZMin = -2040.;
constexpr double kSectorZMax = 2040.;
constexpr int    kNZLayers = 8;
constexpr double kSectorDz = (kSectorZMax - kSectorZMin) / kNZLayers;
constexpr double kTargetX = 1580.;
constexpr double kTargetY = 1580.;
constexpr double kDetR = 1350.;
constexpr double kDetZ = 150.;

const double kCaskX[12] = { 1580., 1580., -1580., -1580.,
                             6240., 6240.,  9400.,  9400.,
                            -6240.,-6240., -9400., -9400.};
const double kCaskY[12] = { 1580.,-1580.,  1580., -1580.,
                             1580.,-1580.,  1580., -1580.,
                             1580.,-1580.,  1580., -1580.};

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
    gStyle->SetLineWidth(1);
}

TH2F* Frame(const char* name, double x0, double x1, double y0, double y1,
            const char* xt, const char* yt)
{
    auto* h = new TH2F(name, "", 100, x0, x1, 100, y0, y1);
    h->SetStats(0);
    h->GetXaxis()->SetTitle(xt);
    h->GetYaxis()->SetTitle(yt);
    h->GetXaxis()->SetTitleOffset(1.15);
    h->GetYaxis()->SetTitleOffset(1.35);
    h->GetXaxis()->CenterTitle();
    h->GetYaxis()->CenterTitle();
    return h;
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

void PanelLabel(double x, double y, const char* text)
{
    auto* t = new TLatex();
    t->SetNDC();
    t->SetTextFont(42);
    t->SetTextSize(0.045);
    t->DrawLatex(x, y, text);
}

TGraph* Wedge(double cx, double cy, double rin, double rout,
              double a0deg, double a1deg)
{
    const int n = 24;
    std::vector<double> x, y;
    for (int i = 0; i <= n; ++i) {
        const double a = TMath::DegToRad() * (a0deg + (a1deg - a0deg) * i / n);
        x.push_back(cx + rin * std::cos(a));
        y.push_back(cy + rin * std::sin(a));
    }
    for (int i = n; i >= 0; --i) {
        const double a = TMath::DegToRad() * (a0deg + (a1deg - a0deg) * i / n);
        x.push_back(cx + rout * std::cos(a));
        y.push_back(cy + rout * std::sin(a));
    }
    auto* g = new TGraph((int)x.size(), x.data(), y.data());
    g->SetLineWidth(1);
    return g;
}

void DrawCask(double cx, double cy, int id, bool target)
{
    auto* body = new TEllipse(cx, cy, kCaskBodyR, kCaskBodyR);
    body->SetFillColor(target ? TColor::GetColor("#f4b6b6")
                              : TColor::GetColor("#c9ced6"));
    body->SetFillStyle(1001);
    body->SetLineColor(kBlack);
    body->SetLineWidth(1);
    body->Draw("F");
    body->Draw("L");

    auto* fin = new TEllipse(cx, cy, kCaskFinR, kCaskFinR);
    fin->SetFillStyle(0);
    fin->SetLineColor(kGray + 2);
    fin->SetLineStyle(2);
    fin->Draw("L");

    auto* lab = new TLatex(cx, cy, Form("%d", id));
    lab->SetTextAlign(22);
    lab->SetTextFont(62);
    lab->SetTextSize(0.028);
    lab->SetTextColor(target ? kRed + 2 : kBlack);
    lab->Draw();
}

void DrawDetectorPositions(double cx, double cy, bool labels)
{
    for (int i = 0; i < 8; ++i) {
        const double a = TMath::DegToRad() * 45. * i;
        const double x = cx + kDetR * std::cos(a);
        const double y = cy + kDetR * std::sin(a);
        auto* m = new TMarker(x, y, 20);
        m->SetMarkerSize(0.65);
        m->SetMarkerColor(kRed + 1);
        m->Draw();
        if (labels) {
            const double rl = kDetR + 170.;
            auto* t = new TLatex(cx + rl * std::cos(a), cy + rl * std::sin(a),
                                 Form("D%d", i));
            t->SetTextAlign(22);
            t->SetTextFont(42);
            t->SetTextSize(0.033);
            t->SetTextColor(kRed + 2);
            t->Draw();
        }
    }
}

void MakeHallTop()
{
    auto* c = new TCanvas("hall_top", "", 1500, 680);
    c->Divide(2, 1, 0.002, 0.002);

    c->cd(1);
    gPad->SetLeftMargin(0.11);
    gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.13);
    gPad->SetTopMargin(0.07);
    auto* h1 = Frame("h1", -11500, 11500, -6500, 6500, "x [mm]", "y [mm]");
    h1->Draw("AXIS");

    auto* floor = new TBox(-11000, -5660, 11000, 5660);
    floor->SetFillColor(TColor::GetColor("#eceff1"));
    floor->SetFillStyle(1001);
    floor->SetLineColor(kGray + 1);
    floor->Draw("F");
    floor->Draw("L");

    for (double y : {-5410., 5410.}) {
        auto* wall = new TLine(-11000, y, 11000, y);
        wall->SetLineColor(kGray + 3);
        wall->SetLineWidth(3);
        wall->Draw();
    }

    auto* zone = new TEllipse(kTargetX, kTargetY, kSectorRout, kSectorRout);
    zone->SetFillStyle(0);
    zone->SetLineColor(kAzure + 1);
    zone->SetLineWidth(2);
    zone->Draw("L");

    for (int i = 0; i < 12; ++i) DrawCask(kCaskX[i], kCaskY[i], i, i == 0);
    DrawDetectorPositions(kTargetX, kTargetY, false);

    auto* t = new TLatex();
    t->SetTextFont(42);
    t->SetTextSize(0.034);
    t->DrawLatex(-10300, 6000, "concrete wall (inner y = #pm5.41 m)");
    t->DrawLatex(1900, 6100, "target cask 0");
    auto* arrow = new TArrow(1850, 5900, 1600, 1900, 0.012, ">");
    arrow->SetLineColor(kRed + 2);
    arrow->SetFillColor(kRed + 2);
    arrow->Draw();

    t->SetTextColor(kGray + 3);
    t->SetTextSize(0.033);
    t->DrawLatex(-2200, 660, "central cluster");
    t->DrawLatex(6500, 660, "+x cluster");
    t->DrawLatex(-9200, 660, "#minusx cluster");

    t->SetTextColor(kRed + 2);
    t->DrawLatex(3000, 2450, "8 detector scan positions");
    auto* arrow2 = new TArrow(3050, 2320, 2700, 1760, 0.010, ">");
    arrow2->SetLineColor(kRed + 2);
    arrow2->SetFillColor(kRed + 2);
    arrow2->Draw();

    t->SetTextColor(kBlack);
    t->SetTextSize(0.030);
    t->DrawLatex(7200, -6100, "hall floor: 22.0 m #times 11.3 m");

    auto* leg = new TLegend(0.15, 0.72, 0.43, 0.88);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.032);
    auto* l1 = new TBox();
    l1->SetFillColor(TColor::GetColor("#f4b6b6"));
    l1->SetLineColor(kBlack);
    auto* l2 = new TBox();
    l2->SetFillColor(TColor::GetColor("#c9ced6"));
    l2->SetLineColor(kBlack);
    auto* l3 = new TMarker(0, 0, 20);
    l3->SetMarkerColor(kRed + 1);
    l3->SetMarkerSize(0.9);
    leg->AddEntry(l1, "target cask 0", "f");
    leg->AddEntry(l2, "background casks 1#minus11", "f");
    leg->AddEntry(l3, "CLYC position", "p");
    leg->Draw();

    PanelLabel(0.13, 0.94, "(a) storage-hall plan view");

    c->cd(2);
    gPad->SetLeftMargin(0.12);
    gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.13);
    gPad->SetTopMargin(0.07);
    auto* h2 = Frame("h2", -900, 4100, -900, 4100, "x [mm]", "y [mm]");
    h2->Draw("AXIS");

    const int sectorColors[8] = {
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2")
    };
    for (int s = 0; s < 8; ++s) {
        auto* g = Wedge(kTargetX, kTargetY, kSectorRin, kSectorRout,
                        45. * s, 45. * (s + 1));
        g->SetFillColor(sectorColors[s]);
        g->SetLineColor(kAzure + 1);
        g->Draw("F");
        g->Draw("L");

        const double am = TMath::DegToRad() * (45. * s + 22.5);
        auto* st = new TLatex(kTargetX + 1550. * std::cos(am),
                              kTargetY + 1550. * std::sin(am),
                              Form("%d", s));
        st->SetTextAlign(22);
        st->SetTextFont(62);
        st->SetTextSize(0.040);
        st->SetTextColor(kAzure + 3);
        st->Draw();
    }

    auto* body = new TEllipse(kTargetX, kTargetY, kCaskBodyR, kCaskBodyR);
    body->SetFillColor(TColor::GetColor("#c9ced6"));
    body->SetLineColor(kBlack);
    body->Draw("F");
    body->Draw("L");
    auto* fin = new TEllipse(kTargetX, kTargetY, kCaskFinR, kCaskFinR);
    fin->SetFillStyle(0);
    fin->SetLineColor(kGray + 2);
    fin->SetLineStyle(2);
    fin->Draw("L");
    auto* clab = new TLatex(kTargetX, kTargetY - 70, "CASTOR 0");
    clab->SetTextAlign(22);
    clab->SetTextFont(62);
    clab->SetTextSize(0.034);
    clab->Draw();

    for (int q = 0; q < 4; ++q) {
        const double a = TMath::DegToRad() * (90. * q + 45.);
        auto* ql = new TLatex(kTargetX + 1100. * std::cos(a),
                              kTargetY + 1100. * std::sin(a),
                              Form("Q%d", q));
        ql->SetTextAlign(22);
        ql->SetTextFont(42);
        ql->SetTextSize(0.026);
        ql->SetTextColor(kGray + 3);
        ql->Draw();
    }

    DrawDetectorPositions(kTargetX, kTargetY, true);

    auto* t2 = new TLatex();
    t2->SetTextFont(42);
    t2->SetTextSize(0.030);
    t2->DrawLatex(-650, 3850, "sector shell: 1.325 < r < 1.775 m, |z| < 2.04 m");
    t2->SetTextColor(kRed + 2);
    t2->DrawLatex(2450, 3450, "D0#minusD7: 45#circ steps at r = 1.35 m");
    t2->SetTextColor(kBlack);
    t2->DrawLatex(-650, -780, "top view; cask fins at r = 1.325 m (dashed)");

    PanelLabel(0.15, 0.94, "(b) sector scoring shell around cask 0");

    SaveCrop(c, "report/sector_implementation/figures/fig_hall_sectors.pdf");
    delete c;
}

void MakeSectorRZ()
{
    auto* c = new TCanvas("sector_rz", "", 900, 620);
    c->SetLeftMargin(0.13);
    c->SetRightMargin(0.04);
    c->SetBottomMargin(0.13);
    c->SetTopMargin(0.07);

    auto* h = Frame("rz", 0, 2050, -2250, 2250, "r [mm]", "z [mm]");
    h->Draw("AXIS");

    auto* cask = new TBox(0, -2040, kCaskBodyR, 2040);
    cask->SetFillColor(TColor::GetColor("#c9ced6"));
    cask->SetLineColor(kBlack);
    cask->Draw("F");
    cask->Draw("L");

    auto* shell = new TBox(kSectorRin, kSectorZMin, kSectorRout, kSectorZMax);
    shell->SetFillColor(TColor::GetColor("#c8dcf2"));
    shell->SetLineColor(kAzure + 2);
    shell->SetFillStyle(1001);
    shell->Draw("F");
    shell->Draw("L");

    for (int iz = 0; iz <= kNZLayers; ++iz) {
        const double z = kSectorZMin + iz * kSectorDz;
        auto* line = new TLine(kSectorRin, z, kSectorRout, z);
        line->SetLineColor(kAzure + 1);
        line->SetLineStyle(3);
        line->SetLineWidth(1);
        line->Draw();
    }

    auto* det = new TBox(kDetR, kDetZ - 29.4, kDetR + 125., kDetZ + 29.4);
    det->SetFillColor(kRed - 7);
    det->SetLineColor(kRed + 2);
    det->SetLineWidth(2);
    det->Draw("F");
    det->Draw("L");

    auto* floor = new TLine(0, -2040, 2000, -2040);
    floor->SetLineColor(kGray + 3);
    floor->SetLineWidth(3);
    floor->Draw();

    auto* t = new TLatex();
    t->SetTextFont(42);
    t->SetTextSize(0.038);
    t->DrawLatex(80, 1350, "CASTOR 440/84 body");
    t->SetTextColor(kAzure + 3);
    t->DrawLatex(1420, 1700, "sector shell");
    t->SetTextColor(kBlack);
    t->DrawLatex(1500, 300, "CLYC");
    t->SetTextColor(kBlack);
    t->SetTextSize(0.026);
    t->DrawLatex(30, -1930, "floor top (z = #minus2.04 m)");

    auto* d1 = new TLine(kSectorRin, kSectorZMin, kSectorRin, kSectorZMax);
    d1->SetLineStyle(2);
    d1->SetLineColor(kAzure + 2);
    d1->Draw();
    auto* d2 = new TLine(kSectorRout, kSectorZMin, kSectorRout, kSectorZMax);
    d2->SetLineStyle(2);
    d2->SetLineColor(kAzure + 2);
    d2->Draw();

    t->SetTextColor(kAzure + 3);
    t->SetTextSize(0.028);
    t->DrawLatex(30, 1850, "8 layers, dz = 510 mm");

    SaveCrop(c, "report/sector_implementation/figures/fig_sector_rz.pdf");
    delete c;
}

} // namespace

void make_geometry_figs()
{
    Style();
    gSystem->mkdir("report/sector_implementation/figures", kTRUE);
    MakeHallTop();
    MakeSectorRZ();
}
