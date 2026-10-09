// plot_muon.C -- figures for the MuonScint config/reflector/position scan.
//
// Reads the muon_out_c<C>_r<R>_<pos>.root files written into data_output/ by
// macros/run_muon_matrix.sh and produces, under <dir>/plots/:
//
//   npe_spectra_cfg<C>.png/pdf   per-event Npe spectra, reflectors overlaid,
//                                one pad per position (centre/edge/corner)
//   reflector_ranking.png/pdf    mean Npe vs reflector, one pad per config,
//                                one line per position  (tests none < TiO2 < Al ~ glossy)
//   position_trend.png/pdf       mean Npe vs position, one pad per config,
//                                one line per reflector    (tests centre > edge > corner)
//   wavelength_cfg<C>.png/pdf    detected-photon wavelength, reflectors overlaid
//   photon_budget_cfg<C>.png/pdf mean nScint/nCerenkov/nKilled/nDetected/
//                                nEscaped/nAlive, one pad per position
//   muon_summary.csv             tabular means for every (cfg,refl,pos)
//
// Usage (batch):
//   root -l -b -q 'plot_muon.C+("data_output")'
//
// Author: G4DCSmonitor analysis tooling.

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TList.h>
#include <TMultiGraph.h>
#include <TROOT.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TTree.h>
#include <TString.h>
#include <TMath.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

const char* kReflName[4] = {"none", "TiO2", "Al", "glossy"};
const int   kReflCol[4]  = {kBlack, kGreen + 2, kAzure + 2, kRed + 1};
const int   kReflMkr[4]  = {20, 21, 22, 23};
const char* kPosName[3]  = {"centre", "edge", "corner"};
const int   kPosCol[3]   = {kBlack, kRed + 1, kBlue + 2};
const int   kPosMkr[3]   = {20, 21, 22};

struct RunData {
    int         cfg     = 0;
    int         refl    = -1;
    int         pos     = -1;
    long        nEvents = 0;
    TH1D*       hNpe    = nullptr;   // per-event Npe
    TH1D*       hWl     = nullptr;   // detected-photon wavelength (weighted by Npe)
    double      mScint = 0., mCer = 0., mKilled = 0.;
    double      mDet = 0., mEsc = 0., mAlive = 0.;
};

int posIndex(const std::string& s)
{
    for (int i = 0; i < 3; ++i) if (s == kPosName[i]) return i;
    return -1;
}

// Parse "muon_out_c<C>_r<R>_<pos>" (root/.root already stripped).
bool parseStem(const TString& stem, int& cfg, int& refl, int& pos)
{
    TString s = stem;
    s.ReplaceAll("muon_out_", "");
    // tokens split by '_': c<C>, r<R>, <pos>
    const Ssiz_t p0 = s.Index('_');
    if (p0 == kNPOS) return false;
    const Ssiz_t p1 = s.Index('_', p0 + 1);
    if (p1 == kNPOS) return false;

    TString cTok = s(0, p0);                 // "c<C>"
    TString rTok = s(p0 + 1, p1 - p0 - 1);   // "r<R>"
    TString pTok = s(p1 + 1, s.Length());    // "<pos>"

    cTok.ReplaceAll("c", "");
    rTok.ReplaceAll("r", "");
    cfg  = cTok.Atoi();
    refl = rTok.Atoi();
    pos  = posIndex(pTok.Data());
    return (cfg > 0 && refl >= 0 && refl < 4 && pos >= 0);
}

}  // namespace

void plot_muon(const char* dir = "data_output")
{
    gStyle->SetOptStat(0);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
    TH1::AddDirectory(kFALSE);

    TSystemDirectory sd(dir, dir);
    TList* files = sd.GetListOfFiles();
    if (!files) { std::cerr << "No such directory: " << dir << "\n"; return; }

    const std::string outDir = std::string(dir) + "/plots";
    gSystem->mkdir(outDir.c_str(), kTRUE);

    // key = "cfg:refl:pos"
    std::map<std::string, RunData> runs;

    std::vector<TString> fileList;
    for (auto* o : *files) {
        TString fname = o->GetName();
        if (!fname.BeginsWith("muon_out_") || !fname.EndsWith(".root")) continue;
        fileList.emplace_back(TString(dir) + "/" + fname);
    }
    std::sort(fileList.begin(), fileList.end());

    for (const auto& full : fileList) {
        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { std::cerr << "Cannot open " << full << "\n"; delete f; continue; }

        TTree* opt  = (TTree*)f->Get("opticalStats");
        TTree* sipm = (TTree*)f->Get("sipmHits");
        if (!opt) { std::cerr << "Missing opticalStats in " << full << "\n"; delete f; continue; }

        TString stem = gSystem->BaseName(full);
        stem.ReplaceAll(".root", "");
        int cfg = 0, refl = -1, pos = -1;
        if (!parseStem(stem, cfg, refl, pos)) {
            std::cerr << "Cannot parse " << stem << " ; skipping\n";
            delete f;
            continue;
        }

        RunData r;
        r.cfg = cfg; r.refl = refl; r.pos = pos;
        r.nEvents = opt->GetEntries();

        r.hNpe = new TH1D(TString::Format("hNpe_%s", stem.Data()),
                          TString::Format("Npe  cfg%d  %s  %s",
                                          cfg, kReflName[refl], kPosName[pos]),
                          400, 0, 4000);
        r.hWl = new TH1D(TString::Format("hWl_%s", stem.Data()),
                         TString::Format("wavelength  cfg%d  %s  %s",
                                         cfg, kReflName[refl], kPosName[pos]),
                         120, 300, 600);

        int evt = 0, nScint = 0, nCer = 0, nKilled = 0, nDet = 0, nEsc = 0, nAlive = 0;
        opt->SetBranchAddress("evtNb",     &evt);
        opt->SetBranchAddress("nScint",    &nScint);
        opt->SetBranchAddress("nCerenkov", &nCer);
        opt->SetBranchAddress("nKilled",   &nKilled);
        opt->SetBranchAddress("nDetected", &nDet);
        opt->SetBranchAddress("nEscaped",  &nEsc);
        opt->SetBranchAddress("nAlive",    &nAlive);

        double sScint = 0, sCer = 0, sKilled = 0, sDet = 0, sEsc = 0, sAlive = 0;
        for (long i = 0; i < opt->GetEntries(); ++i) {
            opt->GetEntry(i);
            r.hNpe->Fill(nDet);
            sScint += nScint; sCer += nCer; sKilled += nKilled;
            sDet += nDet; sEsc += nEsc; sAlive += nAlive;
        }
        const double ne = (r.nEvents > 0) ? double(r.nEvents) : 1.;
        r.mScint = sScint / ne; r.mCer = sCer / ne; r.mKilled = sKilled / ne;
        r.mDet = sDet / ne; r.mEsc = sEsc / ne; r.mAlive = sAlive / ne;

        if (sipm) {
            int sevt = 0, sdet = 0, ssipm = 0, snDet = 0;
            double tFirst = 0, meanWl = 0, rmsWl = 0, weight = 0;
            sipm->SetBranchAddress("evtNb",           &sevt);
            sipm->SetBranchAddress("det",             &sdet);
            sipm->SetBranchAddress("sipm",            &ssipm);
            sipm->SetBranchAddress("nDetected",       &snDet);
            sipm->SetBranchAddress("tFirst",          &tFirst);
            sipm->SetBranchAddress("meanWavelength_nm", &meanWl);
            sipm->SetBranchAddress("rmsWavelength_nm",  &rmsWl);
            sipm->SetBranchAddress("weight",          &weight);
            for (long i = 0; i < sipm->GetEntries(); ++i) {
                sipm->GetEntry(i);
                if (snDet > 0 && meanWl > 0) r.hWl->Fill(meanWl, snDet);
            }
        }

        runs[std::string(TString::Format("%d:%d:%d", cfg, refl, pos).Data())] = r;
        delete f;
    }

    if (runs.empty()) {
        std::cerr << "No parsable muon_out_*.root files in " << dir << "\n";
        return;
    }

    auto find = [&](int c, int r, int p) -> RunData* {
        auto it = runs.find(std::string(TString::Format("%d:%d:%d", c, r, p).Data()));
        return (it == runs.end()) ? nullptr : &it->second;
    };
    auto meanNpe = [](RunData* r) { return r ? r->hNpe->GetMean() : 0.; };
    auto semNpe  = [](RunData* r) {
        if (!r) return 0.;
        const double n = r->hNpe->GetEntries();
        return (n > 0) ? r->hNpe->GetStdDev() / std::sqrt(n) : 0.;
    };

    // ------------------------------------------------------------------
    // F1: Npe spectra, one canvas per config, pad per position.
    // ------------------------------------------------------------------
    for (int c = 1; c <= 3; ++c) {
        TCanvas* cv = new TCanvas(TString::Format("c_npe_cfg%d", c),
                                  TString::Format("Npe spectra, config %d", c),
                                  1000, 900);
        cv->Divide(2, 2);
        for (int p = 0; p < 3; ++p) {
            cv->cd(p + 1);
            gPad->SetLogy(true);
            // pass 1: global maximum over all reflectors for this pad
            double ymax = 1.0;
            for (int r = 0; r < 4; ++r) {
                RunData* rd = find(c, r, p);
                if (rd) ymax = std::max(ymax, rd->hNpe->GetMaximum());
            }
            TH1D* frame = nullptr;
            for (int r = 0; r < 4; ++r) {
                RunData* rd = find(c, r, p);
                if (!rd) continue;
                TH1D* h = rd->hNpe;
                if (!frame) {
                    frame = (TH1D*)h->Clone(TString::Format("f_%d_%d", c, p));
                    frame->Reset();
                    frame->SetTitle(TString::Format(
                        "%s, config %d;N_{pe};events", kPosName[p], c));
                    frame->SetYTitle("events");
                    frame->SetMinimum(0.5);
                    frame->SetMaximum(ymax * 5.0);  // headroom for all curves
                    frame->Draw("axis");
                }
                h->SetLineColor(kReflCol[r]);
                h->SetLineWidth(2);
                h->Draw("hist same");
            }
            if (p == 0) {
                TLegend* leg = new TLegend(0.55, 0.55, 0.92, 0.92);
                leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextSize(0.035);
                for (int r = 0; r < 4; ++r) {
                    RunData* rd = find(c, r, p);
                    if (rd) leg->AddEntry(rd->hNpe, kReflName[r], "l");
                }
                leg->Draw();
            }
        }
        cv->SaveAs(TString::Format("%s/npe_spectra_cfg%d.png", outDir.c_str(), c));
        cv->SaveAs(TString::Format("%s/npe_spectra_cfg%d.pdf", outDir.c_str(), c));
    }

    // ------------------------------------------------------------------
    // F2: reflector ranking -- mean Npe vs reflector, pad per config.
    // ------------------------------------------------------------------
    {
        TCanvas* cv = new TCanvas("c_refl_ranking", "reflector ranking", 1200, 500);
        cv->Divide(3, 1);
        for (int c = 1; c <= 3; ++c) {
            cv->cd(c);
            TMultiGraph* mg = new TMultiGraph();
            for (int p = 0; p < 3; ++p) {
                TGraphErrors* g = new TGraphErrors(4);
                for (int r = 0; r < 4; ++r) {
                    RunData* rd = find(c, r, p);
                    g->SetPoint(r, r, meanNpe(rd));
                    g->SetPointError(r, 0, semNpe(rd));
                }
                g->SetLineColor(kPosCol[p]);
                g->SetMarkerColor(kPosCol[p]);
                g->SetMarkerStyle(kPosMkr[p]);
                g->SetLineWidth(2);
                g->SetTitle(kPosName[p]);
                mg->Add(g, "lp");
            }
            mg->SetTitle(TString::Format("config %d;reflector;mean N_{pe}/event", c));
            mg->Draw("a");
            mg->GetXaxis()->Set(4, -0.5, 3.5);  // bin centers at 0..3
            for (int r = 0; r < 4; ++r) {
                mg->GetXaxis()->SetBinLabel(r + 1, kReflName[r]);
            }
            gPad->Modified();
            gPad->Update();
            if (c == 1) {
                TLegend* leg = new TLegend(0.55, 0.60, 0.92, 0.92);
                leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextSize(0.05);
                for (int p = 0; p < 3; ++p) leg->AddEntry(mg->GetListOfGraphs()
                    ->At(p), kPosName[p], "lp");
                leg->Draw();
            }
        }
        cv->SaveAs(TString::Format("%s/reflector_ranking.png", outDir.c_str()));
        cv->SaveAs(TString::Format("%s/reflector_ranking.pdf", outDir.c_str()));
    }

    // ------------------------------------------------------------------
    // F3: position trend -- mean Npe vs position, pad per config.
    // ------------------------------------------------------------------
    {
        TCanvas* cv = new TCanvas("c_position", "position trend", 1200, 500);
        cv->Divide(3, 1);
        for (int c = 1; c <= 3; ++c) {
            cv->cd(c);
            TMultiGraph* mg = new TMultiGraph();
            for (int r = 0; r < 4; ++r) {
                TGraphErrors* g = new TGraphErrors(3);
                for (int p = 0; p < 3; ++p) {
                    RunData* rd = find(c, r, p);
                    g->SetPoint(p, p, meanNpe(rd));
                    g->SetPointError(p, 0, semNpe(rd));
                }
                g->SetLineColor(kReflCol[r]);
                g->SetMarkerColor(kReflCol[r]);
                g->SetMarkerStyle(kReflMkr[r]);
                g->SetLineWidth(2);
                g->SetTitle(kReflName[r]);
                mg->Add(g, "lp");
            }
            mg->SetTitle(TString::Format("config %d;position;mean N_{pe}/event", c));
            mg->Draw("a");
            mg->GetXaxis()->Set(3, -0.5, 2.5);  // bin centers at 0..2
            for (int p = 0; p < 3; ++p) {
                mg->GetXaxis()->SetBinLabel(p + 1, kPosName[p]);
            }
            gPad->Modified();
            gPad->Update();
            if (c == 1) {
                TLegend* leg = new TLegend(0.55, 0.55, 0.92, 0.92);
                leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextSize(0.05);
                for (int r = 0; r < 4; ++r) leg->AddEntry(mg->GetListOfGraphs()
                    ->At(r), kReflName[r], "lp");
                leg->Draw();
            }
        }
        cv->SaveAs(TString::Format("%s/position_trend.png", outDir.c_str()));
        cv->SaveAs(TString::Format("%s/position_trend.pdf", outDir.c_str()));
    }

    // ------------------------------------------------------------------
    // F4: detected-photon wavelength, per config, reflectors overlaid.
    // ------------------------------------------------------------------
    for (int c = 1; c <= 3; ++c) {
        TCanvas* cv = new TCanvas(TString::Format("c_wl_cfg%d", c),
                                  TString::Format("wavelength, config %d", c),
                                  900, 700);
        gPad->SetLogy(false);
        TH1D* frame = nullptr;
        TH1D* drawn[4] = {nullptr, nullptr, nullptr, nullptr};
        // pass 1: build per-reflector merged spectra and find the global maximum
        double ymax = 1.0;
        for (int r = 0; r < 4; ++r) {
            TH1D* merged = nullptr;
            for (int p = 0; p < 3; ++p) {
                RunData* rd = find(c, r, p);
                if (!rd) continue;
                if (!merged) {
                    merged = (TH1D*)rd->hWl->Clone(TString::Format("wl_%d_%d", c, r));
                    merged->Reset();
                }
                merged->Add(rd->hWl, 1.0);
            }
            if (!merged) continue;
            drawn[r] = merged;
            ymax = std::max(ymax, merged->GetMaximum());
        }
        // pass 2: frame sized to every curve, then overlay
        for (int r = 0; r < 4; ++r) {
            if (!drawn[r]) continue;
            if (!frame) {
                frame = (TH1D*)drawn[r]->Clone(TString::Format("wlframe_%d", c));
                frame->Reset();
                frame->SetTitle(TString::Format(
                    "config %d;wavelength / nm;detected photons (weighted)", c));
                frame->SetMinimum(0.5);
                frame->SetMaximum(ymax * 5.0);  // headroom for all curves
                frame->Draw("axis");
            }
            drawn[r]->SetLineColor(kReflCol[r]);
            drawn[r]->SetMarkerColor(kReflCol[r]);
            drawn[r]->SetMarkerStyle(kReflMkr[r]);
            drawn[r]->SetLineWidth(2);
            drawn[r]->Draw("hist same");
        }
        TLegend* leg = new TLegend(0.62, 0.55, 0.92, 0.92);
        leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextSize(0.035);
        for (int r = 0; r < 4; ++r)
            if (drawn[r]) leg->AddEntry(drawn[r], kReflName[r], "lp");
        leg->Draw();
        cv->SaveAs(TString::Format("%s/wavelength_cfg%d.png", outDir.c_str(), c));
        cv->SaveAs(TString::Format("%s/wavelength_cfg%d.pdf", outDir.c_str(), c));
    }

    // ------------------------------------------------------------------
    // F5: photon budget, per config, pad per position. x = process/category.
    // ------------------------------------------------------------------
    {
        const char* cat[6] = {"nScint", "nCerenkov", "nKilled",
                              "nDetected", "nEscaped", "nAlive"};
        for (int c = 1; c <= 3; ++c) {
            TCanvas* cv = new TCanvas(TString::Format("c_budget_cfg%d", c),
                                      TString::Format("photon budget, config %d", c),
                                      1300, 500);
            cv->Divide(3, 1);
            TGraph* drawn[4] = {nullptr, nullptr, nullptr, nullptr};
            for (int p = 0; p < 3; ++p) {
                cv->cd(p + 1);
                gPad->SetLogy(true);
                TMultiGraph* mg = new TMultiGraph();
                for (int r = 0; r < 4; ++r) {
                    RunData* rd = find(c, r, p);
                    if (!rd) continue;
                    double vals[6] = {rd->mScint, rd->mCer, rd->mKilled,
                                      rd->mDet, rd->mEsc, rd->mAlive};
                    TGraph* g = new TGraph(6);
                    for (int k = 0; k < 6; ++k)
                        g->SetPoint(k, k, std::max(vals[k], 0.5));  // log floor
                    g->SetLineColor(kReflCol[r]);
                    g->SetMarkerColor(kReflCol[r]);
                    g->SetMarkerStyle(kReflMkr[r]);
                    g->SetLineWidth(2);
                    g->SetTitle(kReflName[r]);
                    mg->Add(g, "lp");
                    if (p == 0) drawn[r] = g;  // same reflector across pads
                }
                mg->SetTitle(TString::Format("%s, config %d;category;photons/event",
                                             kPosName[p], c));
                mg->Draw("a");
                mg->GetYaxis()->SetTitle("photons/event (log)");
                mg->GetXaxis()->Set(6, -0.5, 5.5);  // bin centers at 0..5
                for (int k = 0; k < 6; ++k)
                    mg->GetXaxis()->SetBinLabel(k + 1, cat[k]);
                gPad->Modified();
                gPad->Update();
            }
            TLegend* leg = new TLegend(0.9, 0.15, 0.999, 0.9);  // right edge (pad 3)
            cv->cd(3);
            leg->SetBorderSize(0); leg->SetFillStyle(0); leg->SetTextSize(0.03);
            for (int r = 0; r < 4; ++r)
                if (drawn[r]) leg->AddEntry(drawn[r], kReflName[r], "lp");
            leg->Draw();
            cv->SaveAs(TString::Format("%s/photon_budget_cfg%d.png", outDir.c_str(), c));
            cv->SaveAs(TString::Format("%s/photon_budget_cfg%d.pdf", outDir.c_str(), c));
        }
    }

    // ------------------------------------------------------------------
    // CSV summary.
    // ------------------------------------------------------------------
    {
        std::ofstream csv(outDir + "/muon_summary.csv");
        csv << "config,reflector,position,events,meanNpe,semNpe,"
               "mean_nScint,mean_nCerenkov,mean_nKilled,mean_nDetected,"
               "mean_nEscaped,mean_nAlive\n";
        for (int c = 1; c <= 3; ++c)
        for (int r = 0; r < 4; ++r)
        for (int p = 0; p < 3; ++p) {
            RunData* rd = find(c, r, p);
            if (!rd) continue;
            csv << c << "," << kReflName[r] << "," << kPosName[p] << ","
                << rd->nEvents << "," << meanNpe(rd) << "," << semNpe(rd) << ","
                << rd->mScint << "," << rd->mCer << "," << rd->mKilled << ","
                << rd->mDet << "," << rd->mEsc << "," << rd->mAlive << "\n";
        }
        csv.close();
        std::cout << "Wrote " << outDir << "/muon_summary.csv\n";
    }

    std::cout << "Plots written to " << outDir << "/\n";
}
