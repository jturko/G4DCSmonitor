// Recompute collection efficiencies and statistical uncertainties from data_v4.
// Usage: root -l -b -q 'paper/tools/compute_stats.C'
#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <TMath.h>
#include <cstdio>
#include <map>
#include <vector>
#include <cmath>

namespace {
struct Acc { long long det = 0; long long prod = 0; };

void one(const TString& fn, const TString& label)
{
  TFile f(fn);
  TTree* t = (TTree*)f.Get("sipmHits");
  if (!t) { printf("MISSING %s\n", fn.Data()); return; }

  Int_t evt = 0, nd = 0, np = 0;
  t->SetBranchAddress("evtNb", &evt);
  t->SetBranchAddress("nDetected", &nd);
  t->SetBranchAddress("nProduced", &np);

  std::map<int, Acc> m;
  const Long64_t n = t->GetEntries();
  for (Long64_t i = 0; i < n; ++i) {
    t->GetEntry(i);
    Acc& a = m[evt];
    a.det += nd;
    if (a.prod == 0) a.prod = np;
  }

  double D = 0.0, G = 0.0;
  std::vector<double> eps;
  eps.reserve(m.size());
  for (const auto& kv : m) {
    D += kv.second.det;
    G += kv.second.prod;
    eps.push_back(double(kv.second.det) / double(kv.second.prod));
  }

  const double ratio = D / G;
  const double mean = TMath::Mean(eps.size(), eps.data());
  const double rms = TMath::RMS(eps.size(), eps.data());
  const double se = rms / std::sqrt(double(eps.size()));
  const double pois = ratio / std::sqrt(D);

  printf("%-26s Nev=%5zu  D=%9.0f  G=%11.0f  eps=%.6e  se=%.2e  pois=%.2e  mean=%.6e\n",
         label.Data(), m.size(), D, G, ratio, se, pois, mean);
}
} // namespace

void compute_stats()
{
  const TString root = "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v4";
  const char* pos[3] = {"centre", "edge", "corner"};

  printf("=== MATRIX (2500 events/file) ===\n");
  for (int c = 1; c <= 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      for (int p = 0; p < 3; ++p) {
        const TString fn = Form("%s/matrix/muon_out_c%d_r%d_%s.root", root.Data(), c, r, pos[p]);
        one(fn, Form("c%d r%d %s", c, r, pos[p]));
      }
    }
  }

  printf("=== WHOLE PLATE (25000 events/file) ===\n");
  for (int c = 1; c <= 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      const TString fn = Form("%s/whole/muon_whole_out_c%d_r%d.root", root.Data(), c, r);
      one(fn, Form("c%d r%d", c, r));
    }
  }
}
