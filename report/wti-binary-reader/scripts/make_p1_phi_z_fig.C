// Draw p1 aggregate phi-z dose maps with the same ROOT style as the WSSA maps.
//
// Input:  report/wti-binary-reader/data/p1_phi_z_maps.root
// Output: report/wti-binary-reader/figures/fig_p1_phi_z_maps.pdf/.png

#include <iostream>

#include "TCanvas.h"
#include "TColor.h"
#include "TFile.h"
#include "TH2D.h"
#include "TPad.h"
#include "TStyle.h"
#include "TString.h"
#include "TSystem.h"

namespace {

void styleMapPad()
{
  if (!gPad) return;
  gPad->SetLeftMargin(0.22);
  gPad->SetRightMargin(0.17);
  gPad->SetBottomMargin(0.15);
  gPad->SetTopMargin(0.10);
  gPad->SetFrameFillColor(TColor::GetColor("#00008B"));
}

void styleMap(TH2D* h, const char* title)
{
  h->SetStats(kFALSE);
  h->SetTitle(title);
  h->GetXaxis()->SetTitle("#font[132]{#it{#varphi}} [deg]");
  h->GetYaxis()->SetTitle("#font[132]{#it{z}} [mm]");
  h->GetZaxis()->SetTitle("dose rate [#muSv/h]");
  h->GetXaxis()->CenterTitle();
  h->GetYaxis()->CenterTitle();
  h->GetZaxis()->CenterTitle();
  h->GetXaxis()->SetNdivisions(4);
  h->GetYaxis()->SetNdivisions(4);
  h->GetXaxis()->SetLabelOffset(0.012);
  h->GetYaxis()->SetLabelOffset(0.012);
  h->GetXaxis()->SetTitleOffset(1.1);
  h->GetYaxis()->SetTitleOffset(2.0);
  h->GetZaxis()->SetTitleOffset(1.2);
  const double top = h->GetMaximum();
  h->SetMinimum(0.0);
  h->SetMaximum(top > 0.0 ? top : 1.0);
  h->GetZaxis()->SetRangeUser(0.0, top > 0.0 ? top : 1.0);
}

void drawMap(TH2D* h, const char* title)
{
  gPad->cd();
  styleMapPad();
  styleMap(h, title);
  h->Draw("COLZ");
}

}  // namespace

void make_p1_phi_z_fig(
    const char* rootPath = "report/wti-binary-reader/data/p1_phi_z_maps.root",
    const char* outPdf = "report/wti-binary-reader/figures/fig_p1_phi_z_maps.pdf")
{
  if (gSystem->AccessPathName(rootPath)) {
    std::cerr << "missing " << rootPath
              << "; run make_p1_phi_z_maps.C first" << std::endl;
    return;
  }

  TFile input(rootPath, "READ");
  if (input.IsZombie()) {
    std::cerr << "cannot open " << rootPath << std::endl;
    return;
  }

  TH2D* gamma = dynamic_cast<TH2D*>(input.Get("hP1GammaDose"));
  TH2D* neutron = dynamic_cast<TH2D*>(input.Get("hP1NeutronDose"));
  if (!gamma || !neutron) {
    std::cerr << "missing hP1GammaDose or hP1NeutronDose in "
              << rootPath << std::endl;
    return;
  }
  gamma->SetDirectory(nullptr);
  neutron->SetDirectory(nullptr);

  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(1);
  gStyle->SetPalette(kRainBow);
  gStyle->SetNumberContours(99);
  gStyle->SetTitleFontSize(0.045);
  gStyle->SetCanvasColor(kWhite);

  // Match one 1800 x 1000 cell of the 3 x 2 WSSA map grid.
  TCanvas canvas("p1_phi_z_maps", "p1 aggregate phi-z maps", 3600, 1000);
  canvas.Divide(2, 1);

  canvas.cd(1);
  drawMap(gamma, "p1 aggregate, #gamma");
  canvas.cd(2);
  drawMap(neutron, "p1 aggregate, n");

  TString outPng(outPdf);
  outPng.ReplaceAll(".pdf", ".png");
  canvas.Print(outPdf);
  canvas.Print(outPng);

  std::cout << "wrote " << outPdf << std::endl;
  std::cout << "wrote " << outPng << std::endl;
}
