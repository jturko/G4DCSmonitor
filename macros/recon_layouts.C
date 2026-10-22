// recon_layouts.C -- compare the position resolution of the Config 1,
// Config 2 and Config 2 (mod) read-outs with the same symmetry-augmented
// k-NN estimator.
//
// Reads the position_reconstruction.csv files produced by
// position_reconstruction.C+ for the three layouts (all four wrappers each)
// and draws the combined-estimator rms error along one axis.  The figure
// makes the cost of reducing the read-out from eight side sensors to four
// explicit, and separates the two four-sensor geometries: one sensor per side
// (Config 2, pinwheel, C4 symmetry) versus one sensor per 45-degree corner face
// (Config 2 (mod), fourfold symmetric, eight D4 operations).
//
// Usage:
//   root -l -b -q 'macros/recon_layouts.C("report/figures")'
//
// Defaults read <outdir>/position_reconstruction{,_c2,_c12}.csv.

#include <TCanvas.h>
#include <TBox.h>
#include <TGraph.h>
#include <TH1F.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TString.h>
#include <TStyle.h>
#include <TSystem.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "style.h"

namespace {

const char* kWrapper[4] = {"bare", "TiO2", "Al (air)", "glossy"};
const char* kWrapLabel[4] = {"bare", "TiO_{2}", "Al (air)", "glossy"};

const char* kLayout[3] = {"Config 1", "Config 2", "Config 2 (mod)"};
const int   kColor[3]  = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                          PetroffPalette::getP6(4)};

struct Result { double sx = 0., sr = 0., r68 = 0.; bool ok = false; };

// Locate the "combined" row of one wrapper in a position_reconstruction.csv.
Result readCombined(const char* fname, const char* wrapper)
{
    Result r;
    std::ifstream in(fname);
    if (!in) {
        std::cerr << "recon_layouts: cannot open " << fname << std::endl;
        return r;
    }
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (first) { first = false; continue; }   // header
        std::stringstream ss(line);
        std::string tok;
        std::vector<std::string> col;
        while (std::getline(ss, tok, ',')) col.push_back(tok);
        if (col.size() < 11) continue;
        if (col[0] == wrapper && col[1] == "combined") {
            r.sx  = std::atof(col[5].c_str());
            r.sr  = std::atof(col[7].c_str());
            r.r68 = std::atof(col[10].c_str());
            r.ok  = true;
            return r;
        }
    }
    return r;
}

}  // namespace

void recon_layouts(const char* outdir = "report/figures",
                   const char* c1csv  = "report/figures/position_reconstruction.csv",
                   const char* c2csv  = "report/figures/position_reconstruction_c2.csv",
                   const char* c12csv = "report/figures/position_reconstruction_c12.csv")
{
    const char* files[3] = {c1csv, c2csv, c12csv};
    Result res[3][4];
    for (int l = 0; l < 3; ++l)
        for (int w = 0; w < 4; ++w)
            res[l][w] = readCombined(files[l], kWrapper[w]);

    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);

    gSystem->mkdir(outdir, kTRUE);

    TCanvas* c = new TCanvas("c_recon_layouts", "", 1050, 620);
    c->SetLeftMargin(0.11);
    c->SetRightMargin(0.04);
    c->SetTopMargin(0.08);
    c->SetBottomMargin(0.15);

    TH1F* frame = new TH1F("frame_recon_layouts", "", 4, 0., 4.);
    for (int w = 0; w < 4; ++w) frame->GetXaxis()->SetBinLabel(w + 1, kWrapLabel[w]);
    frame->GetYaxis()->SetTitle("combined #sigma_{x} [mm]");
    frame->GetYaxis()->SetTitleOffset(1.05);
    frame->GetYaxis()->SetRangeUser(0., 40.);
    frame->GetXaxis()->SetLabelSize(0.045);
    frame->GetYaxis()->SetLabelSize(0.040);
    frame->GetXaxis()->SetTickLength(0.);
    frame->Draw("AXIS");

    const double bw = 0.20;                 // bar width in x-axis units
    const double off[3] = {-0.24, 0.0, +0.24};
    for (int w = 0; w < 4; ++w) {
        for (int l = 0; l < 3; ++l) {
            if (!res[l][w].ok) {
                std::cerr << "recon_layouts: missing " << kLayout[l] << " / "
                          << kWrapper[w] << std::endl;
                continue;
            }
            const double xc = w + 0.5 + off[l];
            TBox* b = new TBox(xc - 0.5 * bw, 0., xc + 0.5 * bw, res[l][w].sx);
            b->SetFillColor(kColor[l]);
            b->SetLineColor(kColor[l]);
            b->Draw("SAME");

            TLatex* v = new TLatex(xc, res[l][w].sx + 0.45,
                                   TString::Format("%.1f", res[l][w].sx));
            v->SetTextAlign(21);
            v->SetTextSize(0.030);
            v->SetTextColor(kColor[l]);
            v->Draw("SAME");
        }
    }

    TLegend* leg = new TLegend(0.16, 0.86, 0.86, 0.97);
    leg->SetNColumns(3);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.036);
    for (int l = 0; l < 3; ++l) {
        TGraph* sw = new TGraph();
        sw->SetMarkerStyle(21);          // filled square, like the bars
        sw->SetMarkerSize(1.6);
        sw->SetMarkerColor(kColor[l]);
        leg->AddEntry(sw, kLayout[l], "p");
    }
    leg->Draw("SAME");

    c->Print(TString::Format("%s/fig_recon_layouts.pdf", outdir));
    c->Print(TString::Format("%s/fig_recon_layouts.png", outdir));

    // Machine-readable copy of the plotted values.
    FILE* f = std::fopen(TString::Format("%s/recon_layouts.csv", outdir), "w");
    if (f) {
        std::fprintf(f, "layout,wrapper,sigma_x_mm,sigma_r_mm,r68_mm\n");
        for (int l = 0; l < 3; ++l)
            for (int w = 0; w < 4; ++w)
                if (res[l][w].ok)
                    std::fprintf(f, "%s,%s,%.4f,%.4f,%.4f\n",
                                 kLayout[l], kWrapper[w],
                                 res[l][w].sx, res[l][w].sr, res[l][w].r68);
        std::fclose(f);
    } else {
        std::cerr << "recon_layouts: cannot write recon_layouts.csv" << std::endl;
    }

    std::cout << "recon_layouts: wrote " << outdir
              << "/fig_recon_layouts.{pdf,png} and recon_layouts.csv" << std::endl;
}
