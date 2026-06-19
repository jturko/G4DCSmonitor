// make_assembly_heatmaps.C
//
// Build per-assembly hex-bin heatmaps of detector "hits" tree contributions
// from G4DCSmonitor output files
//   detector-response_globalFuelGG_<source>.root  (GG = 0..83, 5 sources)
//
// Distinguishes:
//   * Found file -> bin filled with measured count (0 is a legitimate value)
//   * Missing file -> bin left unfilled AND overdrawn with a grey hexagon
//
// Output:
//   assembly_heatmaps.root  -- TH1Ds (one per (fuel, source)), 6 TH2Polys, canvas
//   assembly_heatmaps.pdf
//   assembly_heatmaps.png
//
// Usage:
//   root -l -b -q 'make_assembly_heatmaps.C("./", "assembly_heatmaps.root")'

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TH2Poly.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TTree.h>
#include <TVector2.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "geometry_constraints.h"
using namespace geo;

namespace {

//// --- must match GeometryCASTOR440 -------------------------------------------
//constexpr double kPitch    = 147.0;                  // basket pitch [mm]
//constexpr int    kNRings   = 5;
//constexpr int    kNAssem   = 84;
//const     double kPhiStart = 30.0 * M_PI/180.;       // hex orientation [rad]
//
//// per-(fuel,source) TH1D edep axis
//constexpr int    kNbE   = 10000;
//constexpr double kEmin  = 0.0;
//constexpr double kEmax  = 10.0;                      // MeV
//
//enum DetectorType {
//    kCLYC,
//    kPlastic
//};


// Reproduces GeometryCASTOR440::GenerateFuelPositions().
std::vector<TVector2> BuildFuelPositions()
{
    const double dx = kPitch;
    const double dy = kPitch * std::sqrt(3.0) / 2.0;
    const int    R  = kNRings;
    std::vector<TVector2> pts; pts.reserve(kNAssem);
    for (int q = -R; q <= R; ++q) {
        const int r1 = std::max(-R, -q - R);
        const int r2 = std::min( R, -q + R);
        for (int r = r1; r <= r2; ++r) {
            const double x = dx * (q + r / 2.0);
            const double y = dy * r;
            if (std::hypot(x, y) < 1e-6) continue;
            const double rmax = R * kPitch;
            if (std::abs(std::hypot(x, y) - rmax) < 1e-3) {
                double ang = std::atan2(y, x) * 180.0/M_PI;
                if (ang < 0) ang += 360.0;
                bool isCorner = false;
                for (double a = 0.0; a < 360.0; a += 60.0)
                    if (std::abs(ang - a) < 1.0) { isCorner = true; break; }
                if (isCorner) continue;
            }
            pts.emplace_back(x, y);
        }
    }
    return pts;
}

TGraph* MakeHexPolygon(double cx, double cy, double scale = 1.0)
{
    const double R = scale * kPitch / std::sqrt(3.0);
    auto* g = new TGraph(7);
    for (int i = 0; i < 6; ++i) {
        const double ang = kPhiStart + i * 60.0 * M_PI/180.;
        g->SetPoint(i, cx + R * std::cos(ang), cy + R * std::sin(ang));
    }
    g->SetPoint(6, g->GetX()[0], g->GetY()[0]);
    return g;
}

TH2Poly* MakeAssemblyHexMap(const char* name, const char* title,
                            const std::vector<TVector2>& pts)
{
    double xmin = +1e9, xmax = -1e9, ymin = +1e9, ymax = -1e9;
    for (const auto& p : pts) {
        xmin = std::min(xmin, p.X()); xmax = std::max(xmax, p.X());
        ymin = std::min(ymin, p.Y()); ymax = std::max(ymax, p.Y());
    }
    const double pad = 1.2 * kPitch;
    auto* h = new TH2Poly(name, title,
                          xmin - pad, xmax + pad,
                          ymin - pad, ymax + pad);
    for (const auto& p : pts) h->AddBin(MakeHexPolygon(p.X(), p.Y()));
    h->GetXaxis()->SetTitle("x [mm]");
    h->GetYaxis()->SetTitle("y [mm]");
    h->GetZaxis()->SetTitle("counts");
    return h;
}

// Draw a slightly-shrunk grey hex on top of every assembly position whose
// source file was missing for this (sub)heatmap. Slightly shrinking (scale
// < 1) leaves a thin gap so adjacent missing bins remain visually separable.
void DrawMissingOverlay(const std::vector<TVector2>& pts,
                        const std::vector<bool>& missing,
                        Color_t fillCol = 16,
                        Style_t fillSty = 3354)
{
    for (size_t i = 0; i < pts.size(); ++i) {
        if (!missing[i]) continue;
        auto* g = MakeHexPolygon(pts[i].X(), pts[i].Y(), /*scale=*/0.97);
        //g->SetFillColor(fillCol);
        //g->SetFillStyle(fillSty);     // hatched
        g->SetFillColor(kWhite);
        g->SetFillStyle(1001); // solid
        g->SetLineColor(kBlack);
        g->SetLineWidth(1);
        g->Draw("F SAME");
        g->Draw("L SAME");
    }
}

} // anonymous

// ----------------------------------------------------------------------------
void make_assembly_heatmaps(const char* inputDir = ".",
                            string particle = "all", 
                            DetectorType thisDetectorType = kCLYC,
                            const char* outFile  = "assembly_heatmaps.root")
{
    if(particle != "neutron" &&
       particle != "gamma" &&
       particle != "all") {
        cout << "unknown particle type" << endl;
        return;
    }

    gStyle->SetOptStat(0);
    //gStyle->SetPalette(kViridis);
    gStyle->SetPalette(kBird);
    //gStyle->SetNumberContours(99);
    gStyle->SetNumberContours(256);

    const std::vector<std::string> sources = {
        "gamma_662keV",
        "gamma_1173keV",
        "gamma_1332keV",
        "isotope_Eu154",
        "neutron_Watt"
    };

    const auto positions = BuildFuelPositions();
    if ((int)positions.size() != kNAssem) {
        std::cerr << "[err] expected " << kNAssem
                  << " fuel positions, got " << positions.size() << std::endl;
        return;
    }

    TFile* fout = TFile::Open(outFile, "RECREATE");
    if (!fout || fout->IsZombie()) {
        std::cerr << "[err] cannot open output: " << outFile << std::endl;
        return;
    }

    std::vector<TH2Poly*> hMaps;
    for (const auto& s : sources) {
        hMaps.push_back(MakeAssemblyHexMap(
            ("h2_counts_" + s).c_str(),
            ("Hits-tree counts per assembly (" + s + ")").c_str(),
            positions));
    }
    auto* hSum = MakeAssemblyHexMap(
        "h2_counts_total",
        "Hits-tree counts per assembly (sum over all sources)",
        positions);

    // Missing masks: missing[source][fuel]; sum-map missing iff ANY source missing.
    std::vector<std::vector<bool>> missing(sources.size(),
                                           std::vector<bool>(kNAssem, false));
    std::vector<bool> missingAny(kNAssem, false);

    auto* edepDir = fout->mkdir("edep_per_assembly");

    long long nProcessed = 0, nMissing = 0, nFoundEmpty = 0;

    for (int g = 0; g < kNAssem; ++g) {
        for (size_t si = 0; si < sources.size(); ++si) {
            const std::string& src = sources[si];

            char fname[512];
            std::snprintf(fname, sizeof(fname),
                          //"%s/detector-response_cask0_globalFuel%02d_%s.root",
                          "%s/detector-response_globalFuel%02d_%s.root",
                          inputDir, g, src.c_str());

            const bool noFile = gSystem->AccessPathName(fname);
            std::unique_ptr<TFile> fin;
            TTree* t = nullptr;

            //if (!noFile) {
            //    fin.reset(TFile::Open(fname, "READ"));
            //    if (fin && !fin->IsZombie())
            //        t = (TTree*)fin->Get("hits");
            //}
            if (!noFile) {
                std::cerr << "[open] " << fname << std::endl;   // <-- add this
                // Silence ROOT's chatty error printing while probing a file
                // that may still be open for writing by a concurrent job.
                const Int_t prevIgnore = gErrorIgnoreLevel;
                gErrorIgnoreLevel = kFatal;
                fin.reset(TFile::Open(fname, "READ"));
                // A file that was not closed cleanly (i.e. still being written)
                // is auto-recovered by ROOT and has kRecovered set. Treat it as
                // missing -- reading its TTree usually segfaults mid-basket.
                const bool recovered = fin && !fin->IsZombie()
                                       && fin->TestBit(TFile::kRecovered);
                if (fin && !fin->IsZombie() && !recovered) {
                    t = (TTree*)fin->Get("hits");
                    if (t && t->GetEntries() < 0) t = nullptr;   // half-written
                }
                gErrorIgnoreLevel = prevIgnore;
                if (recovered)
                    std::cerr << "[skip] in-progress (recovered): "
                              << fname << std::endl;
            }

            // ---- treat anything that doesn't yield a usable 'hits' tree
            // ---- as MISSING. This covers: file absent, file zombie,
            // ---- 'hits' tree absent or unreadable.
            const bool isMissing = (noFile || !fin || fin->IsZombie() || !t);
            if (isMissing) {
                std::cerr << "[miss] " << fname << std::endl;
                missing[si][g]    = true;
                missingAny[g]     = true;
                ++nMissing;
                continue;   // do NOT touch the bin -> remains "unfilled"
            }

            // ---- file is present; build the per-pair edep TH1D
            edepDir->cd();
            char hname[128], htitle[256];
            std::snprintf(hname, sizeof(hname),
                          "h1_edep_globalFuel%02d_%s", g, src.c_str());
            std::snprintf(htitle, sizeof(htitle),
                          "edep, globalFuel=%d, source=%s [FOUND];edep [MeV];counts (weighted)",
                          g, src.c_str());
            auto* h1 = new TH1D(hname, htitle, kNbE, kEmin, kEmax);
            h1->SetDirectory(edepDir);

            const TString expr = TString::Format("edep>>%s", hname);

            double energyLow, energyHigh;
            auto format_expr2 = [particle, &energyLow, &energyHigh](string p, DetectorType detType) {
                string ret;
                if(detType == kCLYC) {
                    if(particle == "gamma") {
                        ret += "*(pid == 11 || pid == 22 || pid == -11)";
                        energyLow  = 0.500;
                        energyHigh = 0.740;
                    }
                    else if(particle == "neutron") {
                        ret += "*(pid == 1000020040 || pid == 2112)";
                        energyLow  = 4.6;
                        energyHigh = 5.2;
                    }
                    else {
                        energyLow  = 0.;
                        energyHigh = 10.;
                    }
                }
                else if(detType == kPlastic) {
                    if(particle == "gamma") {
                        ret += "*(pid == 11 || pid == 22 || pid == -11)";
                        energyLow  = 0.500;
                        energyHigh = 0.740;
                    }
                    else if(particle == "neutron") {
                        ret += "*(pid == 1000060120 || pid == 2112 || pid == 2212)";
                        energyLow  = 0.500;
                        energyHigh = 5.000;
                    }
                    else {
                        energyLow  = 0.;
                        energyHigh = 10.;
                    }
                }
                return ret;
            };
            const TString expr2 = TString::Format("weight%s", format_expr2(particle, thisDetectorType).c_str());
            t->Draw(expr, expr2, "goff");

            double total = h1->Integral(h1->FindBin(energyLow), h1->FindBin(energyHigh));
            if (total == 0.0) ++nFoundEmpty;

            // Bin content = real measured count (could legitimately be 0).
            hMaps[si]->SetBinContent(g + 1, total);
            hSum    ->SetBinContent(g + 1, hSum->GetBinContent(g + 1) + total);

            ++nProcessed;
        }
    }

    // For the SUM map we mark a bin as "missing" only if ALL sources for that
    // fuel are missing -- otherwise the partial sum is still meaningful.
    // (Switch the predicate to "ANY source missing" if you want the
    // strictest interpretation.)
    std::vector<bool> missingSum(kNAssem, false);
    for (int g = 0; g < kNAssem; ++g) {
        bool allMissing = true;
        for (size_t si = 0; si < sources.size(); ++si)
            if (!missing[si][g]) { allMissing = false; break; }
        missingSum[g] = allMissing;
    }

    // ---- shared colour scale across all 6 heatmaps -----------------------
    double zmin = +1e30, zmax = -1e30;
    auto scan = [&](TH2Poly* h) {
        for (int b = 1; b <= h->GetNumberOfBins(); ++b) {
            const double v = h->GetBinContent(b);
            if (v == 0.0) continue;          // skip empty/missing bins
            if (v < zmin) zmin = v;
            if (v > zmax) zmax = v;
        }
    };
    //zmax = 5000;
    zmin = 0;
    for (auto* h : hMaps) scan(h);
    scan(hSum);
    if (!(zmax > zmin)) { zmin = 0.0; zmax = 1.0; }   // pathological fallback
    for (auto* h : hMaps) { h->SetMinimum(zmin); h->SetMaximum(zmax); }
    hSum->SetMinimum(zmin); hSum->SetMaximum(zmax);

    // ------------------------------------------------------------------
    // Draw 2x3 canvas: 5 sources + sum.
    // ------------------------------------------------------------------
    fout->cd();
    auto* c = new TCanvas("c_assembly_heatmaps",
                          "Per-assembly hits-tree counts", 1400, 800);
    c->Divide(3, 2);

    auto drawOne = [&](TVirtualPad* pad, TH2Poly* h,
                       const std::vector<bool>& miss,
                       const char* label)
    {
        pad->cd();
        pad->SetRightMargin(0.15);
        pad->SetLeftMargin (0.12);
        h->Draw("TEXT COLZ L");
        //gPad->SetLogz();
        DrawMissingOverlay(positions, miss);

        // Tiny inline legend for the missing pattern.
        size_t nMiss = 0;
        for (bool m : miss) if (m) ++nMiss;
        if (nMiss > 0) {
            auto* tex = new TLatex();
            tex->SetNDC(true);
            tex->SetTextSize(0.028);
            tex->SetTextColor(kGray + 2);
            tex->DrawLatex(0.15, 0.92,
                Form("hatched grey = missing (%zu / %d)",
                     nMiss, kNAssem));
        }
        h->Write();
    };

    for (size_t i = 0; i < sources.size(); ++i)
        drawOne(c->cd(i + 1), hMaps[i], missing[i], sources[i].c_str());
    drawOne(c->cd(6), hSum, missingSum, "total");

    c->Write();
    c->SaveAs("assembly_heatmaps.pdf");
    c->SaveAs("assembly_heatmaps.png");

    fout->Write();
    fout->Close();

    std::cout << "[ok] processed " << nProcessed
              << " (fuel, source) pairs; "
              << nFoundEmpty << " found-but-zero, "
              << nMissing    << " missing.\n"
              << "[ok] wrote " << outFile
              << " and assembly_heatmaps.{pdf,png}" << std::endl;
}









