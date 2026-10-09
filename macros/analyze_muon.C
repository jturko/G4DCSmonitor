// analyze_muon.C -- Npe / detection-efficiency analysis for the MuonScint scan.
//
// Reads the muon_out_c<C>_r<R>_<pos>.root files produced by
// macros/generate_muon_matrix.sh and, per (config, reflector, position),
// computes the per-event photoelectron count (sum of `nDetected` over all
// SiPMs, from the `sipmHits` tree) and the detection efficiency at a
// user-supplied Npe threshold.
//
// Usage (batch):
//   root -l -b -q 'analyze_muon.C+("macros/muon", 40)'
// where the 2nd argument is the Npe threshold (default 40).
//
// Output: printed table + muon_scan_summary.root/csv.

#include <TFile.h>
#include <TTree.h>
#include <TDirectory.h>
#include <TH1D.h>
#include <TSystemDirectory.h>
#include <TList.h>
#include <TString.h>
#include <TROOT.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

struct Result {
    TString cfg;
    TString refl;
    TString pos;
    double  meanNpe = 0.;
    double  eff     = 0.;
    long    nEvents = 0;
    TH1D*   hNpe     = nullptr;
};

static const char* reflName(int r)
{
    switch (r) {
        case 0: return "none";
        case 1: return "TiO2";
        case 2: return "Al";
        case 3: return "glossy";
        default: return "other";
    }
}

void analyze_muon(const char* dir = "macros/muon", int npeThresh = 40)
{
    TH1::AddDirectory(kFALSE);

    TSystemDirectory sd(dir, dir);
    TList* files = sd.GetListOfFiles();
    if (!files) { std::cerr << "No such directory: " << dir << "\n"; return; }

    std::vector<TString> fileList;
    for (auto* o : *files) {
        TString fname = o->GetName();
        if (!fname.BeginsWith("muon_out_") || !fname.EndsWith(".root")) continue;
        fileList.emplace_back(TString(dir) + "/" + fname);
    }
    std::sort(fileList.begin(), fileList.end());

    std::vector<Result> results;

    for (const auto& full : fileList) {
        TFile* f = TFile::Open(full, "READ");
        if (!f || f->IsZombie()) { std::cerr << "Cannot open " << full << "\n"; delete f; continue; }

        TTree* sipm = (TTree*)f->Get("sipmHits");
        TTree* opt  = (TTree*)f->Get("opticalStats");
        if (!sipm || !opt) { std::cerr << "Missing trees in " << full << "\n"; delete f; continue; }

        const long nEvents = opt->GetEntries();

        // Per-event Npe = sum of nDetected over all SiPMs of the event.
        std::map<int, int> npeByEvent;
        int evt = 0, nDet = 0;
        sipm->SetBranchAddress("evtNb", &evt);
        sipm->SetBranchAddress("nDetected", &nDet);
        for (long i = 0; i < sipm->GetEntries(); ++i) {
            sipm->GetEntry(i);
            npeByEvent[evt] += nDet;
        }

        // Parse the stem "c<C>_r<R>_<pos>".
        TString stem = full;
        stem.ReplaceAll(TString(dir) + "/", "");
        stem.ReplaceAll("muon_out_", "");
        stem.ReplaceAll(".root", "");

        Result r;
        r.cfg = stem;
        TString s = stem;
        Ssiz_t p0 = s.Index('_');
        Ssiz_t p1 = s.Index('_', p0 + 1);
        if (p0 != kNPOS && p1 != kNPOS) {
            TString cfgTok = s(0, p0);            // "c<C>"
            TString rTok   = s(p0 + 1, p1 - p0 - 1);  // "r<R>"
            TString posTok = s(p1 + 1, s.Length());   // "<pos>"
            cfgTok.ReplaceAll("c", "");
            rTok.ReplaceAll("r", "");
            r.cfg  = cfgTok;
            r.refl = reflName(rTok.Atoi());
            r.pos  = posTok;
        }

        TString hname = "h_" + stem;
        TString htitle = "Npe  cfg" + r.cfg + "  " + r.refl + "  " + r.pos;
        TH1D* h = new TH1D(hname, htitle, 400, 0, 4000);
        double sum = 0.; long nPass = 0;
        for (long e = 0; e < nEvents; ++e) {
            auto it = npeByEvent.find((int)e);
            const int npe = (it != npeByEvent.end()) ? it->second : 0;
            h->Fill(npe);
            sum += npe;
            if (npe >= npeThresh) ++nPass;
        }

        r.meanNpe = nEvents ? sum / nEvents : 0.;
        r.eff     = nEvents ? double(nPass) / double(nEvents) : 0.;
        r.nEvents = nEvents;
        r.hNpe    = h;

        results.push_back(r);
        delete f;
    }

    std::sort(results.begin(), results.end(),
              [](const Result& a, const Result& b) {
                  if (a.cfg  != b.cfg)  return a.cfg  < b.cfg;
                  if (a.refl != b.refl) return a.refl < b.refl;
                  return a.pos < b.pos;
              });

    std::cout << "\n=== MuonScint scan summary (threshold Npe >= "
              << npeThresh << ") ===\n";
    std::cout << "  cfg    refl     pos        events     meanNpe        eff\n";
    for (const auto& r : results) {
        std::cout << TString::Format("  %-5s  %-7s  %-8s  %8ld  %10.2f  %10.4f\n",
                                     r.cfg.Data(), r.refl.Data(), r.pos.Data(),
                                     r.nEvents, r.meanNpe, r.eff).Data();
    }

    std::ofstream csv("muon_scan_summary.csv");
    csv << "config,reflector,position,events,meanNpe,eff_at_" << npeThresh << "\n";
    TFile out("muon_scan_summary.root", "RECREATE");
    for (const auto& r : results) {
        csv << r.cfg << "," << r.refl << "," << r.pos << ","
            << r.nEvents << "," << r.meanNpe << "," << r.eff << "\n";
        if (r.hNpe) r.hNpe->Write();
    }
    csv.close();
    out.Close();

    std::cout << "\nWrote muon_scan_summary.csv and muon_scan_summary.root\n";
}
