#include <TFile.h>
#include <TTree.h>
#include <TH1D.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TLegend.h>
#include <TLine.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TColor.h>
#include <TString.h>
#include <TMath.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

namespace {

struct RunSpec {
    std::string label;       // legend label
    std::string file;        // ROOT file name (without .root)
    bool        isUnbiased;  // true = reference (biased/unbiased denominator)
    int         color;       // line/marker colour
};

TH1D* MakeEdepHist(const std::string& rootFilePath,
                   const std::string& histName,
                   int nbins, double emin, double emax)
{
    TFile* f = TFile::Open(rootFilePath.c_str(), "READ");
    if (!f || f->IsZombie()) {
        std::cerr << "[ERROR] cannot open " << rootFilePath << std::endl;
        if (f) { f->Close(); delete f; }
        return nullptr;
    }

    TTree* t = dynamic_cast<TTree*>(f->Get("hits"));
    if (!t) {
        std::cerr << "[ERROR] no 'hits' TTree in " << rootFilePath << std::endl;
        f->Close(); delete f;
        return nullptr;
    }

    // Detach histogram from the file so it survives file->Close().
    TH1D* h = new TH1D(histName.c_str(),
                       ";#font[132]{#it{E}}_{dep} / MeV;weighted counts per 10^{8} primaries",
                       nbins, emin, emax);
    h->SetDirectory(nullptr);
    h->Sumw2();

    // Explicit branch addresses (matches HistoManager's DColumn types).
    Double_t edep   = 0.0;
    Double_t weight = 0.0;

    // Only enable the branches we actually need -- big I/O speedup.
    t->SetBranchStatus("*", 0);
    t->SetBranchStatus("edep",   1);
    t->SetBranchStatus("weight", 1);

    if (t->SetBranchAddress("edep",   &edep)   < 0) {
        std::cerr << "[ERROR] branch 'edep' not found in " << rootFilePath << std::endl;
        f->Close(); delete f; delete h; return nullptr;
    }
    if (t->SetBranchAddress("weight", &weight) < 0) {
        std::cerr << "[ERROR] branch 'weight' not found in " << rootFilePath << std::endl;
        f->Close(); delete f; delete h; return nullptr;
    }

    const Long64_t nEntries = t->GetEntries();
    for (Long64_t i = 0; i < nEntries; ++i) {
        t->GetEntry(i);
        h->Fill(edep, weight);
    }

    f->Close();
    delete f;
    return h;
}





} // anonymous namespace

void compare_bias_radii(const char* dir = "./data/")
{
    gStyle->SetOptStat(0);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);

    // ------------------------------------------------------------------
    // List of runs. The first entry MUST be the unbiased reference.
    // ------------------------------------------------------------------
    //// 1e9, cm
    //std::vector<RunSpec> runs = {
    //    {"unbiased",                               "fuel18_gamma-2MeV_unbiased_1e9",     true,  kBlack},
    //    {"biased, #font[132]{R}_{bound} = 50 cm",  "fuel18_gamma-2MeV_biased-50cm_1e9",  false, kRed+1},
    //};
    //// 1e8, cm, neutrons
    //std::vector<RunSpec> runs = {
    //    {"unbiased",                               "fuel18_neutron-2MeV_unbiased_1e8",     true,  kBlack},
    //    {"biased, #font[132]{R}_{bound} = 10 cm",  "fuel18_neutron-2MeV_biased-10cm_1e8",  false, kRed+1},
    //    {"biased, #font[132]{R}_{bound} = 25 cm",  "fuel18_neutron-2MeV_biased-25cm_1e8",  false, kOrange+7},
    ////    {"biased, #font[132]{R}_{bound} = 50 cm",  "fuel18_gamma-2MeV_biased-50cm_1e8",  false, kAzure+2},
    ////    {"biased, #font[132]{R}_{bound} = 100 cm", "fuel18_gamma-2MeV_biased-100cm_1e8", false, kGreen+2},
    //};
    // 1e8, cm
    std::vector<RunSpec> runs = {
        {"unbiased",                   "fuel18_gamma-2MeV_unbiased_1e8",     true,  kBlack},
        {"biased, #font[132]{R}_{bound} = 10 cm",  "fuel18_gamma-2MeV_biased-10cm_1e8",  false, kRed+1},
        {"biased, #font[132]{R}_{bound} = 25 cm",  "fuel18_gamma-2MeV_biased-25cm_1e8",  false, kOrange+7},
        {"biased, #font[132]{R}_{bound} = 50 cm",  "fuel18_gamma-2MeV_biased-50cm_1e8",  false, kAzure+2},
        {"biased, #font[132]{R}_{bound} = 100 cm", "fuel18_gamma-2MeV_biased-100cm_1e8", false, kGreen+2},
    };
    //// 1e7, mm
    //std::vector<RunSpec> runs = {
    //    {"unbiased",                   "fuel18_gamma-2MeV_unbiased_1e7",     true,  kBlack},
    //    {"biased, R_{bound} = 10 mm",  "fuel18_gamma-2MeV_biased-10mm_1e7",  false, kRed+1},
    //    {"biased, R_{bound} = 25 mm",  "fuel18_gamma-2MeV_biased-25mm_1e7",  false, kOrange+7},
    //    {"biased, R_{bound} = 50 mm",  "fuel18_gamma-2MeV_biased-50mm_1e7",  false, kAzure+2},
    //    {"biased, R_{bound} = 100 mm", "fuel18_gamma-2MeV_biased-100mm_1e7", false, kGreen+2},
    //};

    // Binning: 2 MeV gammas -> full-energy peak at 2 MeV; allow a small
    // overshoot for smeared/mis-reconstructed bins.
    const int    nbins = 60;
    const double emin  = 0.0;   // MeV
    const double emax  = 6.0;   // MeV

    // ------------------------------------------------------------------
    // Load histograms
    // ------------------------------------------------------------------
    std::vector<TH1D*> hists(runs.size(), nullptr);
    for (size_t i = 0; i < runs.size(); ++i) {
        const std::string path = std::string(dir) + "/" + runs[i].file + ".root";
        const std::string name = TString::Format("h_edep_%zu", i).Data();
        hists[i] = MakeEdepHist(path, name, nbins, emin, emax);
        if (!hists[i]) {
            std::cerr << "[FATAL] failed to load " << path
                      << " ; aborting." << std::endl;
            return;
        }
        hists[i]->SetLineColor(runs[i].color);
        hists[i]->SetMarkerColor(runs[i].color);
        hists[i]->SetLineWidth(2);
        hists[i]->SetTitle(runs[i].label.c_str());

        // Quick diagnostic:
        //   entries       : number of weighted fills
        //   Neff          : (Sum w)^2 / Sum w^2 -- the effective Poisson-
        //                   equivalent count (what drives the stat uncertainty)
        //   <w>           : mean statistical weight per hit
        //   integral sumW : physical expected count per 1e7 primaries
        //
        // Neff << entries means the sample is weight-dominated: a few high-weight
        // hits carry most of the information (typical for aggressive biasing).
        const double nEntries = hists[i]->GetEntries();
        const double sumW     = hists[i]->Integral();
        const double neff     = hists[i]->GetEffectiveEntries();
        std::cout << "  " << runs[i].label
                  << " : entries = "  << nEntries
                  << " , Neff = "     << neff
                  << " , <w> = "      << (nEntries > 0 ? sumW / nEntries : 0.0)
                  << " , integral (sum w) = " << sumW
                  << std::endl;
    }

    TH1D* hRef = hists.front();   // the unbiased reference

    // ------------------------------------------------------------------
    // Canvas: top pane (spectra) + bottom pane (ratios)
    // ------------------------------------------------------------------
    TCanvas* c = new TCanvas("c_compare_bias_radii",
                             "edep spectrum: bias-radius scan",
                             900, 800);
    c->cd();

    TPad* pTop = new TPad("pTop", "pTop", 0.0, 0.33, 1.0, 1.0);
    pTop->SetBottomMargin(0.02);
    pTop->SetLeftMargin(0.12);
    pTop->SetRightMargin(0.04);
    pTop->SetLogy(true);
    pTop->Draw();

    TPad* pBot = new TPad("pBot", "pBot", 0.0, 0.0,  1.0, 0.33);
    pBot->SetTopMargin(0.03);
    pBot->SetBottomMargin(0.30);
    pBot->SetLeftMargin(0.12);
    pBot->SetRightMargin(0.04);
    pBot->SetGridy(true);
    pBot->Draw();

    // ---------------- top pane: overlay spectra ----------------
    pTop->cd();

    // Style axes on the reference histogram (drawn first).
    hRef->GetXaxis()->SetLabelSize(0.0);     // hide x labels (bottom pane shows them)
    hRef->GetXaxis()->SetTitleSize(0.0);
    hRef->GetYaxis()->SetTitle("weighted counts / 10^{8} primaries");
    hRef->GetYaxis()->SetTitleSize(0.045);
    hRef->GetYaxis()->SetTitleOffset(1.25);
    hRef->GetYaxis()->SetLabelSize(0.04);

    // Set a sensible y-range covering all histograms.
    double ymax = 0.0;
    for (auto* h : hists) ymax = std::max(ymax, h->GetMaximum());
    hRef->SetMinimum(std::max(1e-3, ymax * 1e-6));   // avoid log(0)
    hRef->SetMaximum(ymax * 5.0);

    hRef->Draw("hist e");
    for (size_t i = 1; i < hists.size(); ++i) {
        hists[i]->Draw("hist e same");
    }

    TLegend* leg = new TLegend(0.58, 0.62, 0.95, 0.92);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextSize(0.035);
    for (size_t i = 0; i < hists.size(); ++i) {
        leg->AddEntry(hists[i], runs[i].label.c_str(), "l");
    }
    leg->Draw();

    TLatex tl;
    tl.SetNDC(true);
    tl.SetTextSize(0.04);
    tl.DrawLatex(0.14, 0.94,
                 "CASTOR fuel #18, 2 MeV #gamma, 10^{8} primaries, CLYC hits");

    // ---------------- bottom pane: biased / unbiased ----------------
    pBot->cd();

    bool  firstRatio = true;
    TH1D* rFirst     = nullptr;

    for (size_t i = 1; i < hists.size(); ++i) {
        TH1D* r = (TH1D*)hists[i]->Clone(TString::Format("ratio_%zu", i));
        r->SetDirectory(nullptr);
        r->SetTitle("");

        // Default TH1::Divide with Sumw2() on both histograms performs
        // Gaussian error propagation assuming numerator and denominator are
        // statistically independent, which is appropriate here since the
        // biased and unbiased runs come from independent primary streams.
        //r->Divide(hRef);

        for(int b=0; b<r->GetNbinsX(); b++) {
            const double rVal   = r->GetBinContent(b);
            const double refVal = hRef->GetBinContent(b);
            const double rErr   = r->GetBinError(b);
            const double refErr = hRef->GetBinError(b);
            
            if (refVal == 0.0) {
                r->SetBinContent(b, 0.0);   // or skip / use std::nan
                r->SetBinError  (b, 0.0);
                continue;
            }
            
            // Percent difference (relative to hRef)
            r->SetBinContent(b, 100.0 * (rVal - refVal) / refVal);
            
            // Propagated uncertainty on the percent difference
            const double term1 = rErr   / refVal;                 // d/dr
            const double term2 = rVal * refErr / (refVal*refVal); // d/dref
            r->SetBinError(b, 100.0 * std::sqrt(term1*term1 + term2*term2));


            //if(r->GetBinContent(b) != 0 && hRef->GetBinContent(b) != 0)
            //    r->SetBinContent(b, 100.*(r->GetBinContent(b)-hRef->GetBinContent(b))/hRef->GetBinContent(b));
            //else
            //    r->SetBinContent(b, 0.);
        }

        r->SetLineColor(runs[i].color);
        r->SetMarkerColor(runs[i].color);
        r->SetMarkerStyle(20);
        r->SetMarkerSize(0.6);
        r->SetLineWidth(2);

        if (firstRatio) {
            r->GetYaxis()->SetTitle("\% difference");
            r->GetYaxis()->SetNdivisions(505);
            r->GetYaxis()->SetTitleSize(0.10);
            r->GetYaxis()->SetTitleOffset(0.55);
            r->GetYaxis()->SetLabelSize(0.08);

            r->GetXaxis()->SetTitle("E_{dep} [MeV]");
            r->GetXaxis()->SetTitleSize(0.11);
            r->GetXaxis()->SetTitleOffset(1.10);
            r->GetXaxis()->SetLabelSize(0.09);

            r->SetMinimum(-110);
            r->SetMaximum(110);   // expected to cluster around 0
            r->Draw("e");
            rFirst     = r;
            firstRatio = false;
        } else {
            r->Draw("e same");
        }
    }

    // Horizontal reference line at y = 1.
    if (rFirst) {
        const double xlo = rFirst->GetXaxis()->GetXmin();
        const double xhi = rFirst->GetXaxis()->GetXmax();
        TLine* one = new TLine(xlo, 1.0, xhi, 1.0);
        one->SetLineColor(kGray+2);
        one->SetLineStyle(2);
        one->SetLineWidth(1);
        one->Draw();
    }

    c->cd();
    c->Modified();
    c->Update();

    // Save for the record.
    c->SaveAs("compare_bias_radii.png");
    c->SaveAs("compare_bias_radii.pdf");
}
