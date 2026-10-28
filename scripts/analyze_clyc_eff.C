// CLYC full-energy peak efficiency from the per-pulse "hits" tree.
//
// The hits tree stores one row per pulse (DCSMonitorSD::EndOfEvent), so all
// rows with the same evtNb are summed to recover the total energy deposited in
// the crystal per event. The simulation has no detector resolution, so the
// full-energy peak is a delta; a +/-2% window captures it and excludes the
// 511 keV escape peaks.
//
// Usage:
//   root -l -b -q 'scripts/analyze_clyc_eff.C("file.root",661.64,10000000,0.00796)'
//
// Arguments:
//   fname        input ROOT file containing the "hits" tree
//   E_keV        gamma energy in keV
//   Ninc         number of generated gammas (/run/beamOn)
//   eff_ref_pct  corrected CSV efficiency in percent, or -1 to skip the ratio
//   win_frac     half-window as a fraction of E (default 0.02)
//   out_suffix   suffix for the output spectrum file (default "_eff")

#include <TFile.h>
#include <TH1D.h>
#include <TString.h>
#include <TTree.h>

#include <cmath>
#include <cstdio>
#include <map>

void analyze_clyc_eff(const char* fname,
                      double E_keV,
                      long long Ninc,
                      double eff_ref_pct = -1.0,
                      double win_frac = 0.02,
                      const char* out_suffix = "_eff")
{
    TFile* f = TFile::Open(fname, "READ");
    if (!f || f->IsZombie()) {
        std::printf("ERROR: cannot open %s\n", fname);
        return;
    }

    TTree* t = static_cast<TTree*>(f->Get("hits"));
    if (!t) {
        std::printf("ERROR: no 'hits' tree in %s\n", fname);
        f->Close();
        return;
    }

    Double_t edep = 0., evtNb = 0., weight = 0.;
    Int_t det = -1;
    t->SetBranchAddress("edep", &edep);
    t->SetBranchAddress("evtNb", &evtNb);
    t->SetBranchAddress("weight", &weight);
    t->SetBranchAddress("det", &det);

    std::map<long long, double> eventEdep;
    const Long64_t nEntries = t->GetEntries();
    for (Long64_t i = 0; i < nEntries; ++i) {
        t->GetEntry(i);
        if (det != 0) continue;          // CLYC is detector 0
        if (weight <= 0.) continue;
        const long long ev = static_cast<long long>(std::llround(evtNb));
        eventEdep[ev] += edep * weight;
    }

    const double E_MeV = E_keV / 1000.;
    const double lo = E_MeV * (1. - win_frac);
    const double hi = E_MeV * (1. + win_frac);

    long long nPeak = 0;
    double sumE = 0.;
    TH1D h("h_total_edep",
           TString::Format("total edep per event, %.2f keV;E_{dep} [MeV];events",
                           E_keV),
           600, 0., E_MeV * 1.5);

    for (const auto& kv : eventEdep) {
        const double e = kv.second;
        sumE += e;
        h.Fill(e);
        if (e >= lo && e <= hi) ++nPeak;
    }

    const long long nHit = static_cast<long long>(eventEdep.size());
    const double eff = (Ninc > 0) ? 100. * static_cast<double>(nPeak) /
                                    static_cast<double>(Ninc) : 0.;
    const double sig = (Ninc > 0) ? 100. * std::sqrt(static_cast<double>(nPeak)) /
                                    static_cast<double>(Ninc) : 0.;

    std::printf("\n=== CLYC efficiency: %s ===\n", fname);
    std::printf("E = %.2f keV, N_incident = %lld, N_hit_events = %lld, "
                "N_peak = %lld\n", E_keV, Ninc, nHit, nPeak);
    std::printf("window = [%.4f, %.4f] MeV (+/- %.2f%%)\n",
                lo, hi, 100. * win_frac);
    std::printf("eff_sim = %.6g %% +/- %.3g %% (stat)\n", eff, sig);
    if (eff_ref_pct > 0.) {
        std::printf("eff_ref = %.6g %%\n", eff_ref_pct);
        std::printf("eff_sim / eff_ref = %.4f\n", eff / eff_ref_pct);
        std::printf("difference = %+.2f %%\n",
                    100. * (eff / eff_ref_pct - 1.));
    }
    std::printf("mean total edep (hit events) = %.4f MeV\n",
                nHit ? sumE / static_cast<double>(nHit) : 0.);

    TString out = fname;
    if (out.EndsWith(".root")) out.ReplaceAll(".root", "");
    out += out_suffix;
    out += ".root";

    TFile* fo = TFile::Open(out, "RECREATE");
    if (fo && !fo->IsZombie()) {
        h.Write();
        fo->Close();
    }
    std::printf("spectrum written to %s\n", out.Data());

    f->Close();
}
