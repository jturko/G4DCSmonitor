// Compare the normalized energy shapes of WSSA Cask 1 with the Geant4 p1
// surface source.  Histograms are normalized to unit area; this is a shape
// comparison, not an absolute rate comparison.
//
// Usage:
//   root -l -b -q 'report/wti-binary-reader/scripts/make_p1_energy_compare.C+'

#include <TCanvas.h>
#include <TFile.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TStyle.h>
#include <TSystem.h>

#include <iostream>

namespace {

void StylePad(TVirtualPad* p)
{
    p->SetLeftMargin(0.14);
    p->SetRightMargin(0.04);
    p->SetBottomMargin(0.16);
    p->SetTopMargin(0.12);
    p->SetTickx(1);
    p->SetTicky(1);
}

TH1D* UnitArea(const TH1D* src, const char* name)
{
    auto* h = dynamic_cast<TH1D*>(src->Clone(name));
    if (!h) return nullptr;
    h->SetDirectory(nullptr);
    const double integral = h->Integral();
    if (integral > 0.0) h->Scale(1.0 / integral);
    h->SetStats(false);
    h->SetLineWidth(2);
    h->GetYaxis()->SetTitle("normalised yield / bin");
    h->GetYaxis()->SetTitleSize(0.045);
    h->GetYaxis()->SetLabelSize(0.038);
    h->GetXaxis()->SetTitle("#font[132]{#it{E}} [MeV]");
    h->GetXaxis()->SetTitleSize(0.045);
    h->GetXaxis()->SetLabelSize(0.038);
    return h;
}

}  // namespace

void make_p1_energy_compare(
    const char* wssaDir = "scripts/wssa/output_all",
    const char* p1File = "report/wti-binary-reader/data/p1_phi_z_maps.root",
    const char* outDir = "report/wti-binary-reader/figures")
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);

    TFile fwG(Form("%s/Cask1_gamma_all.root", wssaDir), "READ");
    TFile fwN(Form("%s/Cask1_neutrons_all.root", wssaDir), "READ");
    TFile fp(p1File, "READ");
    if (fwG.IsZombie() || fwN.IsZombie() || fp.IsZombie()) {
        std::cerr << "cannot open one of the input files\n";
        return;
    }

    auto* wg = dynamic_cast<TH1D*>(fwG.Get("hE_linear"));
    auto* wn = dynamic_cast<TH1D*>(fwN.Get("hE"));
    auto* pg = dynamic_cast<TH1D*>(fp.Get("hP1GammaEnergy"));
    auto* pn = dynamic_cast<TH1D*>(fp.Get("hP1NeutronEnergy"));
    if (!wg || !wn || !pg || !pn) {
        std::cerr << "missing energy histogram\n";
        return;
    }

    TH1D* wgN = UnitArea(wg, "wgNorm");
    TH1D* pgN = UnitArea(pg, "pgNorm");
    TH1D* wnN = UnitArea(wn, "wnNorm");
    TH1D* pnN = UnitArea(pn, "pnNorm");

    for (TH1D* h : {wgN, pgN, wnN, pnN}) {
        h->SetMarkerStyle(20);
        h->SetMarkerSize(0.6);
    }
    wgN->SetLineColor(kBlack);
    wgN->SetMarkerColor(kBlack);
    pgN->SetLineColor(kRed + 1);
    pgN->SetMarkerColor(kRed + 1);
    wnN->SetLineColor(kBlack);
    wnN->SetMarkerColor(kBlack);
    pnN->SetLineColor(kRed + 1);
    pnN->SetMarkerColor(kRed + 1);

    TCanvas c("c_p1_energy_compare", "", 1800, 800);
    c.Divide(2, 1, 0.002, 0.002);

    // Gamma spectrum, linear energy axis.
    c.cd(1);
    StylePad(gPad);
    gPad->SetLogy(1);
    wgN->GetXaxis()->SetRangeUser(0.0, 4.0);
    wgN->GetYaxis()->SetRangeUser(1.0e-6, 1.0);
    wgN->Draw("HIST");
    pgN->Draw("HIST SAME");
    TLatex labG;
    labG.SetNDC();
    labG.SetTextSize(0.055);
    labG.SetTextFont(42);
    labG.DrawLatex(0.16, 0.93, "Cask 1 gamma");
    TLegend lg(0.55, 0.67, 0.93, 0.87);
    lg.SetBorderSize(0);
    lg.SetTextSize(0.038);
    lg.AddEntry(wgN, "WSSA Cask 1", "l");
    lg.AddEntry(pgN, "Geant4 p1", "l");
    lg.Draw();

    // Neutron spectrum, logarithmic energy axis.
    c.cd(2);
    StylePad(gPad);
    gPad->SetLogx(1);
    gPad->SetLogy(1);
    wnN->GetXaxis()->SetRangeUser(1.0e-8, 30.0);
    wnN->GetYaxis()->SetRangeUser(1.0e-7, 1.0);
    wnN->Draw("HIST");
    pnN->Draw("HIST SAME");
    TLatex labN;
    labN.SetNDC();
    labN.SetTextSize(0.055);
    labN.SetTextFont(42);
    labN.DrawLatex(0.16, 0.93, "Cask 1 neutrons");
    TLegend ln(0.55, 0.67, 0.93, 0.87);
    ln.SetBorderSize(0);
    ln.SetTextSize(0.038);
    ln.AddEntry(wnN, "WSSA Cask 1", "l");
    ln.AddEntry(pnN, "Geant4 p1", "l");
    ln.Draw();

    gSystem->mkdir(outDir, kTRUE);
    c.SaveAs(Form("%s/fig_p1_energy_compare.pdf", outDir));
    c.SaveAs(Form("%s/fig_p1_energy_compare.png", outDir));
    std::cout << "wrote " << outDir << "/fig_p1_energy_compare.{pdf,png}\n";
}
