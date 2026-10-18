// Arrival-time and wavelength statistics from data_v4 whole-plate runs (glossy).
// Usage: root -l -b -q 'paper/tools/timing_stats.C'
#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <TMath.h>
#include <cstdio>
#include <vector>
#include <cmath>

namespace {
struct Res {
  double n = 0, n5 = 0, n10 = 0, n20 = 0;
  double wl = 0, wlEarly = 0, nEarly = 0, wlLate = 0, nLate = 0;
  double wl2 = 0;
};
}

void timing_stats()
{
  const TString root = "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v4";
  printf("cfg  Nev  Npe   f<5ns  f<10ns  f>20ns  <wl>  rms_wl  <wl early>  <wl late>  peak_t  peak_h\n");

  for (int c = 1; c <= 4; ++c) {
    const TString fn = Form("%s/whole/muon_whole_out_c%d_r3.root", root.Data(), c);
    TFile f(fn);
    TTree* t = (TTree*)f.Get("sipmHits");
    if (!t) { printf("missing %s\n", fn.Data()); continue; }

    std::vector<double>* td = nullptr;
    std::vector<double>* wl = nullptr;
    t->SetBranchAddress("tDetected", &td);
    t->SetBranchAddress("wlDetected_nm", &wl);

    Res r;
    const int nb = 700; // 0-70 ns, 0.1 ns
    std::vector<double> h(nb, 0.0);
    int maxCountEvent = 0;

    const Long64_t n = t->GetEntries();
    for (Long64_t i = 0; i < n; ++i) {
      t->GetEntry(i);
      if (!td || !wl) continue;
      std::vector<int> he(nb, 0);
      for (size_t k = 0; k < td->size(); ++k) {
        const double tt = (*td)[k];
        const double w = (k < wl->size()) ? (*wl)[k] : 0.0;
        r.n += 1.0;
        if (tt < 5.0)  { r.n5 += 1.0; r.wlEarly += w; r.nEarly += 1.0; }
        if (tt < 10.0) { r.n10 += 1.0; }
        if (tt > 20.0) { r.n20 += 1.0; r.wlLate += w; r.nLate += 1.0; }
        r.wl += w;
        r.wl2 += w * w;
        const int b = int(tt / 0.1);
        if (b >= 0 && b < nb) { h[b] += 1.0; he[b] += 1; if (he[b] > maxCountEvent) maxCountEvent = he[b]; }
      }
    }

    const double mean = r.wl / r.n;
    const double rms = std::sqrt(r.wl2 / r.n - mean * mean);
    int bmax = 0;
    for (int b = 1; b < nb; ++b) if (h[b] > h[bmax]) bmax = b;

    printf("%d  %5lld  %8.1f  %5.1f%%  %5.1f%%  %5.1f%%  %6.1f  %5.1f  %8.1f  %8.1f  %5.2f  %5d\n",
           c, (long long)t->GetMaximum("evtNb") + 1, r.n / (t->GetMaximum("evtNb") + 1),
           100.0 * r.n5 / r.n, 100.0 * r.n10 / r.n, 100.0 * r.n20 / r.n,
           mean, rms, r.wlEarly / r.nEarly, r.wlLate / r.nLate,
           (bmax + 0.5) * 0.1, maxCountEvent);
  }
}
