// Data figures for the DCS-Monitor 64-cell background-sector report.
// Run from the repository root after the p1.5 and p2 macros:
//   root -l -b -q report/sector_implementation/scripts/make_data_figs.C

#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include "TCanvas.h"
#include "TFile.h"
#include "TTree.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include "TH1D.h"
#include "TH2F.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TMarker.h"
#include "TBox.h"
#include "TEllipse.h"
#include "TStyle.h"
#include "TColor.h"
#include "TMath.h"
#include "TString.h"
#include "TSystem.h"

namespace {

constexpr double kTargetX = 1580.;
constexpr double kTargetY = 1580.;
constexpr double kCaskBodyR = 1270.;
constexpr double kCaskFinR = 1325.;
constexpr double kSectorRin = 1325.;
constexpr double kSectorRout = 1775.;
constexpr double kSectorZMin = -2040.;
constexpr double kSectorZMax = 2040.;
constexpr int    kNSectors = 8;
constexpr int    kNZLayers = 8;
constexpr int    kNCells = kNSectors * kNZLayers;

struct Entry {
    double pid = 0., ekin = 0., t = 0., x = 0., y = 0., z = 0.;
    double px = 0., py = 0., pz = 0., w = 0.;
    double sid = 0., face = 0., zLayer = 0., cellId = 0.;
};

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

void PanelLabel(double x, double y, const char* text)
{
    auto* t = new TLatex();
    t->SetNDC();
    t->SetTextFont(42);
    t->SetTextSize(0.046);
    t->DrawLatex(x, y, text);
}

TH2F* Frame(const char* name, double x0, double x1, double y0, double y1,
            const char* xt, const char* yt)
{
    auto* h = new TH2F(name, "", 100, x0, x1, 100, y0, y1);
    h->SetStats(0);
    h->GetXaxis()->SetTitle(xt);
    h->GetYaxis()->SetTitle(yt);
    h->GetXaxis()->SetTitleOffset(1.10);
    h->GetYaxis()->SetTitleOffset(1.30);
    h->GetXaxis()->CenterTitle();
    h->GetYaxis()->CenterTitle();
    return h;
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

void DrawWedgeGeometry(double cx, double cy, bool labels)
{
    const int cols[8] = {
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2"),
        TColor::GetColor("#dce9f7"), TColor::GetColor("#c8dcf2")
    };
    for (int s = 0; s < kNSectors; ++s) {
        auto* g = Wedge(cx, cy, kSectorRin, kSectorRout,
                        45. * s, 45. * (s + 1));
        g->SetFillColor(cols[s]);
        g->SetLineColor(kAzure + 1);
        g->Draw("F");
        g->Draw("L");
        if (labels) {
            const double a = TMath::DegToRad() * (45. * s + 22.5);
            const double lx = cx + 1550. * std::cos(a);
            const double ly = cy + 1550. * std::sin(a);
            auto* box = new TBox(lx - 95, ly - 80, lx + 95, ly + 80);
            box->SetFillColor(kWhite);
            box->SetLineColor(kWhite);
            box->Draw("F");
            auto* t = new TLatex(lx, ly, Form("%d", s));
            t->SetTextAlign(22);
            t->SetTextFont(62);
            t->SetTextSize(0.042);
            t->SetTextColor(kBlack);
            t->Draw();
        }
    }
    auto* body = new TEllipse(cx, cy, kCaskBodyR, kCaskBodyR);
    body->SetFillColor(TColor::GetColor("#c9ced6"));
    body->SetLineColor(kBlack);
    body->Draw("F");
    body->Draw("L");
    auto* fin = new TEllipse(cx, cy, kCaskFinR, kCaskFinR);
    fin->SetFillStyle(0);
    fin->SetLineColor(kGray + 2);
    fin->SetLineStyle(2);
    fin->Draw("L");
}

void DrawWedgeLabels(double cx, double cy)
{
    for (int s = 0; s < kNSectors; ++s) {
        const double a = TMath::DegToRad() * (45. * s + 22.5);
        const double lx = cx + 1550. * std::cos(a);
        const double ly = cy + 1550. * std::sin(a);
        auto* box = new TBox(lx - 95, ly - 80, lx + 95, ly + 80);
        box->SetFillColor(kWhite);
        box->SetLineColor(kWhite);
        box->Draw("F");
        auto* t = new TLatex(lx, ly, Form("%d", s));
        t->SetTextAlign(22);
        t->SetTextFont(62);
        t->SetTextSize(0.042);
        t->SetTextColor(kBlack);
        t->Draw();
    }
}

bool OpenP15(const char* file, TFile*& f, TTree*& t)
{
    f = TFile::Open(file, "READ");
    if (!f || f->IsZombie()) {
        printf("WARNING: cannot open %s\n", file);
        return false;
    }
    t = dynamic_cast<TTree*>(f->Get("sectorFlux"));
    if (!t) {
        printf("WARNING: no sectorFlux in %s\n", file);
        return false;
    }
    return true;
}

void BindP15(TTree* t, Entry& e)
{
    t->SetBranchAddress("pid", &e.pid);
    t->SetBranchAddress("ekin", &e.ekin);
    t->SetBranchAddress("t", &e.t);
    t->SetBranchAddress("x", &e.x);
    t->SetBranchAddress("y", &e.y);
    t->SetBranchAddress("z", &e.z);
    t->SetBranchAddress("px", &e.px);
    t->SetBranchAddress("py", &e.py);
    t->SetBranchAddress("pz", &e.pz);
    t->SetBranchAddress("weight", &e.w);
    t->SetBranchAddress("sectorId", &e.sid);
    t->SetBranchAddress("face", &e.face);
    t->SetBranchAddress("zLayer", &e.zLayer);
    t->SetBranchAddress("cellId", &e.cellId);
}

void MakeP15Map(const char* file)
{
    TFile* f = nullptr;
    TTree* t = nullptr;
    if (!OpenP15(file, f, t)) return;
    Entry e;
    BindP15(t, e);

    const Long64_t n = t->GetEntries();

    // Subsample the x-y map for legibility. The full distribution is shown in
    // the cell-population and one-dimensional histograms.
    const Long64_t strideXY = std::max<Long64_t>(1, n / 10000);
    std::vector<double> gx[kNSectors], gy[kNSectors];
    for (Long64_t i = 0; i < n; i += strideXY) {
        t->GetEntry(i);
        const int s = (int)std::lround(e.sid);
        if (s < 0 || s >= kNSectors) continue;
        gx[s].push_back(e.x - kTargetX);
        gy[s].push_back(e.y - kTargetY);
    }

    // The z-phi panel uses more entries because it shows only the outer face.
    const Long64_t strideZPhi = std::max<Long64_t>(1, n / 30000);
    std::vector<double> gphi[kNSectors], gz[kNSectors];
    for (Long64_t i = 0; i < n; i += strideZPhi) {
        t->GetEntry(i);
        if (std::lround(e.face) != 1) continue;   // outer cylindrical face
        const int s = (int)std::lround(e.sid);
        if (s < 0 || s >= kNSectors) continue;
        gphi[s].push_back(std::atan2(e.y - kTargetY, e.x - kTargetX) *
                          TMath::RadToDeg());
        gz[s].push_back(e.z);
    }

    const int colors[8] = {kBlue + 1, kGray + 1, kGray + 1, kGray + 1,
                           kGray + 1, kGray + 1, kGray + 1, kRed + 1};

    auto* c = new TCanvas("p15_map", "", 1300, 650);
    c->Divide(2, 1, 0.002, 0.002);

    // (a) cask-local x-y entry map.
    c->cd(1);
    gPad->SetLeftMargin(0.16);
    gPad->SetRightMargin(0.07);
    gPad->SetBottomMargin(0.10);
    gPad->SetTopMargin(0.13);

    auto* hxy = Frame("mapxy", -1900., 1900., -1900., 1900.,
                      "x [mm], cask", "y [mm], cask");
    hxy->GetYaxis()->SetTitleOffset(1.55);
    hxy->GetXaxis()->SetTitleOffset(1.10);
    hxy->GetXaxis()->SetNdivisions(5);
    hxy->GetYaxis()->SetNdivisions(5);
    hxy->GetXaxis()->SetLabelSize(0.036);
    hxy->GetYaxis()->SetLabelSize(0.036);
    hxy->Draw("AXIS");
    DrawWedgeGeometry(0., 0., false);
    for (int s = 0; s < kNSectors; ++s) {
        if (gx[s].empty()) continue;
        auto* g = new TGraph((int)gx[s].size(), gx[s].data(), gy[s].data());
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.35);
        g->SetMarkerColor(colors[s]);
        g->Draw("P same");
    }
    DrawWedgeLabels(0., 0.);
    PanelLabel(0.15, 0.93, "(a) cask-local x-y");

    // (b) outer-face z-phi entry map.
    c->cd(2);
    gPad->SetLeftMargin(0.16);
    gPad->SetRightMargin(0.07);
    gPad->SetBottomMargin(0.10);
    gPad->SetTopMargin(0.13);

    const double zPad = 50.;
    auto* hzphi = Frame("mapzphi", -180., 180., kSectorZMin - zPad,
                        kSectorZMax + zPad, "#phi [deg], cask", "z [mm]");
    hzphi->GetYaxis()->SetTitleOffset(1.55);
    hzphi->GetXaxis()->SetTitleOffset(1.10);
    hzphi->GetXaxis()->SetNdivisions(4, kFALSE);
    hzphi->GetXaxis()->SetLabelSize(0.036);
    hzphi->Draw("AXIS");

    for (int k = 1; k < kNSectors; ++k) {
        const double phi = -180. + 45. * k;
        auto* line = new TLine(phi, kSectorZMin, phi, kSectorZMax);
        line->SetLineStyle(3);
        line->SetLineColor(kGray + 1);
        line->Draw();
    }
    for (int k = 1; k < kNZLayers; ++k) {
        const double zz = kSectorZMin +
            (kSectorZMax - kSectorZMin) * k / kNZLayers;
        auto* line = new TLine(-180., zz, 180., zz);
        line->SetLineStyle(3);
        line->SetLineColor(kGray + 1);
        line->Draw();
    }

    for (int s = 0; s < kNSectors; ++s) {
        if (gphi[s].empty()) continue;
        auto* g = new TGraph((int)gphi[s].size(), gphi[s].data(), gz[s].data());
        g->SetMarkerStyle(20);
        g->SetMarkerSize(0.35);
        g->SetMarkerColor(colors[s]);
        g->Draw("P same");
    }
    PanelLabel(0.15, 0.93, "(b) outer face z-#phi");

    SaveCrop(c, "report/sector_implementation/figures/fig_p15_map.pdf");
    delete c;
    f->Close();
}

void MakeP15Dist(const char* file)
{
    TFile* f = nullptr;
    TTree* t = nullptr;
    if (!OpenP15(file, f, t)) return;
    Entry e;
    BindP15(t, e);

    auto* hcell = new TH2F("hcell", "", 8, -0.5, 7.5, 8, -0.5, 7.5);
    hcell->GetXaxis()->SetTitle("sector");
    hcell->GetYaxis()->SetTitle("z layer");
    hcell->GetXaxis()->SetTitleFont(42);
    hcell->GetYaxis()->SetTitleFont(42);
    hcell->GetXaxis()->SetNdivisions(8);
    hcell->GetYaxis()->SetNdivisions(8);
    hcell->Scale(1e-3);
    hcell->GetZaxis()->SetTitle("entries / cell (#times 10^{3})");
    hcell->GetZaxis()->SetTitleFont(42);
    hcell->GetZaxis()->SetTitleSize(0.042);
    hcell->GetZaxis()->SetTitleOffset(1.60);
    hcell->GetZaxis()->SetLabelSize(0.035);
    hcell->GetZaxis()->SetNoExponent(kTRUE);
    hcell->GetZaxis()->CenterTitle();

    auto* hf = new TH1D("hf", "", 8, -0.5, 7.5);
    hf->GetXaxis()->SetTitle("face label");
    hf->GetYaxis()->SetTitle("entries / bin");
    hf->GetXaxis()->SetTitleOffset(1.15);
    hf->GetXaxis()->SetLabelSize(0.045);
    hf->GetXaxis()->SetNdivisions(8);
    for (int i = 0; i < 8; ++i) {
        hf->GetXaxis()->SetBinLabel(i + 1, Form("%d", i));
    }

    auto* he = new TH1D("he", "", 80, 0.60, 0.72);
    he->GetXaxis()->SetTitle("kinetic energy [MeV]");
    he->GetYaxis()->SetTitle("entries / bin");

    auto* hz = new TH1D("hz", "", 80, kSectorZMin, kSectorZMax);
    hz->GetXaxis()->SetTitle("z [mm]");
    hz->GetYaxis()->SetTitle("entries / bin");

    auto* hr = new TH1D("hr", "", 80, kSectorRin, kSectorRout);
    hr->GetXaxis()->SetTitle("r [mm]");
    hr->GetYaxis()->SetTitle("entries / bin");

    auto* ha = new TH1D("ha", "", 50, -1, 1);
    ha->GetXaxis()->SetTitle("cos #theta");
    ha->GetYaxis()->SetTitle("entries / bin");

    const Long64_t n = t->GetEntries();
    for (Long64_t i = 0; i < n; ++i) {
        t->GetEntry(i);
        hcell->Fill(e.sid, e.zLayer);
        hf->Fill(e.face);
        he->Fill(e.ekin);
        hz->Fill(e.z);
        hr->Fill(std::hypot(e.x - kTargetX, e.y - kTargetY));
        const double p = std::sqrt(e.px * e.px + e.py * e.py + e.pz * e.pz);
        if (p > 0.) ha->Fill(e.pz / p);
    }

    auto draw = [](TH1D* h) {
        gPad->SetLeftMargin(0.16);
        gPad->SetRightMargin(0.22);
        gPad->SetBottomMargin(0.21);
        gPad->SetTopMargin(0.08);
        h->SetLineColor(kAzure + 2);
        h->SetLineWidth(2);
        h->Scale(1e-3);
        h->GetYaxis()->SetTitle("entries / bin (#times 10^{3})");
        h->GetXaxis()->SetLabelSize(0.045);
        h->GetYaxis()->SetLabelSize(0.045);
        h->GetXaxis()->SetTitleSize(0.048);
        h->GetYaxis()->SetTitleSize(0.048);
        h->GetXaxis()->SetLabelOffset(0.020);
        h->GetXaxis()->SetTitleOffset(1.35);
        h->GetXaxis()->CenterTitle();
        h->GetYaxis()->CenterTitle();
        h->GetYaxis()->SetNoExponent(kTRUE);
        h->Draw("HIST");
    };

    auto* c = new TCanvas("p15_dist", "", 1600, 1400);
    c->Divide(2, 3, 0.004, 0.004);

    c->cd(1);
    gPad->SetLeftMargin(0.16);
    gPad->SetRightMargin(0.22);
    gPad->SetBottomMargin(0.21);
    gPad->SetTopMargin(0.08);
    hcell->GetXaxis()->CenterTitle();
    hcell->GetYaxis()->CenterTitle();
    hcell->Draw("COLZ");
    PanelLabel(0.15, 0.93, "(a) cell population");

    c->cd(2);
    hf->GetXaxis()->SetNdivisions(8);
    hf->GetXaxis()->SetLabelSize(0.045);
    hf->GetXaxis()->SetLabelOffset(0.025);
    hf->GetXaxis()->SetTitleOffset(1.45);
    draw(hf);
    PanelLabel(0.15, 0.94, "(b) entry face");
    c->cd(3);
    he->GetXaxis()->SetNdivisions(505);
    draw(he); PanelLabel(0.15, 0.94, "(c) energy");
    c->cd(4);
    hz->GetXaxis()->SetNdivisions(505);
    draw(hz); PanelLabel(0.15, 0.94, "(d) longitudinal position");
    c->cd(5);
    hr->GetXaxis()->SetNdivisions(505);
    draw(hr); PanelLabel(0.15, 0.94, "(e) radial position");
    c->cd(6);
    ha->GetXaxis()->SetNdivisions(505);
    draw(ha); PanelLabel(0.15, 0.94, "(f) polar angle");

    SaveCrop(c, "report/sector_implementation/figures/fig_p15_dist.pdf");
    delete c;
    f->Close();
}

TH1D* LoadHitSpectrum(const char* file, const char* name, double scale)
{
    TFile f(file, "READ");
    if (f.IsZombie()) {
        printf("WARNING: cannot open %s\n", file);
        return nullptr;
    }
    auto* t = dynamic_cast<TTree*>(f.Get("hits"));
    if (!t) {
        printf("WARNING: no hits tree in %s\n", file);
        return nullptr;
    }
    double edep = 0.;
    t->SetBranchAddress("edep", &edep);
    auto* h = new TH1D(name, "", 28, 0., 0.7);
    h->SetDirectory(nullptr);
    h->Sumw2();
    const Long64_t n = t->GetEntries();
    for (Long64_t i = 0; i < n; ++i) {
        t->GetEntry(i);
        h->Fill(edep, scale);
    }
    return h;
}

TGraphErrors* RatioToDirect(TH1D* h, TH1D* ref, int color, int marker)
{
    auto* g = new TGraphErrors();
    for (int i = 1; i <= h->GetNbinsX(); ++i) {
        const double n = h->GetBinContent(i);
        const double nd = ref->GetBinContent(i);
        if (n <= 0. || nd <= 0.) continue;
        const double r = n / nd;
        const double en = h->GetBinError(i);
        const double ed = ref->GetBinError(i);
        const double er = r * std::sqrt((en / n) * (en / n) +
                                        (ed / nd) * (ed / nd));
        const int k = g->GetN();
        g->SetPoint(k, h->GetBinCenter(i), r);
        g->SetPointError(k, 0., er);
    }
    g->SetMarkerColor(color);
    g->SetLineColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.8);
    return g;
}

void MakeGammaModels()
{
    const char* zlnFile = "/home/turko46/bigdata/DCS-Monitor-II/turko46/"
                          "ZLN_setup_v4/data/nominal/cask4/"
                          "detector-response_globalFuel00_gamma_662keV.root";
    auto* hz = LoadHitSpectrum(zlnFile, "hz", 0.2);
    auto* ha = LoadHitSpectrum(
        "build/validation/p2_gamma662_cask4_1Mevt_36s_z8.root", "ha", 1.);
    auto* h4 = LoadHitSpectrum(
        "build/validation/p2_gamma662_cask4_1Mevt_36s_4cell_z8.root", "h4", 1.);
    if (!hz || !ha) {
        printf("WARNING: gamma spectra incomplete; skipping figure\n");
        return;
    }

    const int cZ = kBlack;
    const int cA = kRed + 1;
    const int c4 = kAzure + 2;
    hz->SetLineColor(cZ); hz->SetMarkerColor(cZ);
    hz->SetMarkerStyle(20); hz->SetLineWidth(2);
    ha->SetLineColor(cA); ha->SetMarkerColor(cA);
    ha->SetMarkerStyle(21); ha->SetLineWidth(2);
    if (h4) {
        h4->SetLineColor(c4); h4->SetMarkerColor(c4);
        h4->SetMarkerStyle(22); h4->SetLineWidth(2);
    }

    auto* ga = RatioToDirect(ha, hz, cA, 21);
    TGraphErrors* g4 = h4 ? RatioToDirect(h4, hz, c4, 22) : nullptr;

    double rmin = 0.;
    double rmax = 2.5;
    auto scanRatio = [&](TGraphErrors* g) {
        if (!g) return;
        for (int i = 0; i < g->GetN(); ++i) {
            const double y = g->GetY()[i];
            const double e = g->GetEY()[i];
            rmax = std::max(rmax, y + e);
        }
    };
    scanRatio(ga);
    scanRatio(g4);
    rmax = 1.10 * rmax;

    double ymax = hz->GetMaximum();
    ymax = std::max(ymax, ha->GetMaximum());
    if (h4) ymax = std::max(ymax, h4->GetMaximum());
    ymax *= 1.25;

    auto* frame = new TH1D("frame_gamma", "", 100, 0., 0.7);
    frame->SetStats(0);
    frame->SetMinimum(0.);
    frame->SetMaximum(ymax);
    frame->GetXaxis()->SetTitle("energy deposition [MeV]");
    frame->GetYaxis()->SetTitle("hits / 25 keV / 36 s");
    frame->GetXaxis()->SetTitleOffset(1.10);
    frame->GetYaxis()->SetTitleOffset(1.35);
    frame->GetXaxis()->CenterTitle();
    frame->GetYaxis()->CenterTitle();

    auto* fr = new TH1D("frame_ratio", "", 100, 0., 0.7);
    fr->SetStats(0);
    fr->SetMinimum(rmin);
    fr->SetMaximum(rmax);
    fr->GetXaxis()->SetTitle("energy deposition [MeV]");
    fr->GetYaxis()->SetTitle("replay / direct ZLN");
    fr->GetXaxis()->SetTitleOffset(1.10);
    fr->GetYaxis()->SetTitleOffset(1.35);
    fr->GetXaxis()->CenterTitle();
    fr->GetYaxis()->CenterTitle();

    auto* c = new TCanvas("gamma_models", "", 1200, 560);
    c->Divide(2, 1, 0.002, 0.002);

    c->cd(1);
    gPad->SetLeftMargin(0.14); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.14); gPad->SetTopMargin(0.08);
    frame->Draw("AXIS");
    hz->Draw("HIST E SAME");
    ha->Draw("HIST E SAME");
    if (h4) h4->Draw("HIST E SAME");
    auto* l = new TLegend(0.43, 0.68, 0.92, 0.92);
    l->SetBorderSize(0);
    l->SetFillStyle(0);
    l->SetTextSize(0.031);
    l->AddEntry(hz, "direct ZLN, scaled to 36 s", "l");
    l->AddEntry(ha, "p2, cells 32+39", "l");
    if (h4) l->AddEntry(h4, "p2, cells 24+31+32+39", "l");
    l->Draw();
    PanelLabel(0.15, 0.93, "(a) energy-deposition spectrum");

    c->cd(2);
    gPad->SetLeftMargin(0.14); gPad->SetRightMargin(0.03);
    gPad->SetBottomMargin(0.14); gPad->SetTopMargin(0.08);
    fr->Draw("AXIS");
    auto* one = new TLine(0., 1., 0.7, 1.);
    one->SetLineColor(kGray + 2);
    one->SetLineStyle(2);
    one->Draw();
    ga->Draw("P SAME");
    if (g4) g4->Draw("P SAME");
    auto* l2 = new TLegend(0.19, 0.76, 0.58, 0.92);
    l2->SetBorderSize(0);
    l2->SetFillStyle(0);
    l2->SetTextSize(0.030);
    l2->AddEntry(ga, "cells 32+39", "p");
    if (g4) l2->AddEntry(g4, "cells 24+31+32+39", "p");
    l2->Draw();
    PanelLabel(0.15, 0.93, "(b) ratio to direct ZLN");

    SaveCrop(c, "report/sector_implementation/figures/fig_gamma_models.pdf");
    delete c;
}

} // namespace

void make_data_figs()
{
    Style();
    gSystem->mkdir("report/sector_implementation/figures", kTRUE);

    const char* envP15 = gSystem->Getenv("P15_FILE");
    const char* p15 = (envP15 && envP15[0])
        ? envP15
        : "build/validation/p15_gamma662_cask4_1Mevt_18s_z8_a.root";
    MakeP15Map(p15);
    MakeP15Dist(p15);
    MakeGammaModels();
}
