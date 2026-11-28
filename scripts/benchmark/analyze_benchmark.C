#include "TCanvas.h"
#include "TFile.h"
#include "TH2D.h"
#include "TMath.h"
#include "TStyle.h"
#include "TTree.h"

#include <cstring>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr double kPromptCutNs = 1.0e6;
constexpr double kPhiMin = -7.5;
constexpr double kPhiMax = 367.5;
constexpr int    kNPhi = 25;

constexpr int kNZ = 16;
constexpr double kZEdges[kNZ + 1] = {
    330.0,  530.0,  705.0,  880.0,
    1080.0, 1280.0, 1480.0, 1680.0,
    1880.0, 2080.0, 2280.0, 2480.0,
    2680.0, 2880.0, 3080.0, 3280.0,
    3480.0
};

struct DetPoint {
    double phi = -9999.0;
    double z = -9999.0;
};

struct Cut {
    bool gamma = false;
    bool neutron = false;
    bool all = false;
    double eLo = 0.0;
    double eHi = 0.0;
};

Cut MakeCut(const std::string& particle)
{
    Cut c;
    if (particle == "gamma") {
        c.gamma = true; c.eLo = 0.500; c.eHi = 0.740;
    } else if (particle == "neutron") {
        c.neutron = true; c.eLo = 4.6; c.eHi = 5.2;
    } else {
        c.all = true; c.eLo = 0.0; c.eHi = 1.0e9;
    }
    return c;
}

bool PassPid(const Cut& c, long long pid)
{
    if (c.all) return true;
    if (c.gamma) return pid == 11 || pid == 22 || pid == -11;
    return pid == 2112 || pid == 1000020040 || pid == 1000010030;
}

std::map<int, DetPoint> ReadMeta(TFile& f)
{
    std::map<int, DetPoint> out;
    auto* t = dynamic_cast<TTree*>(f.Get("meta"));
    if (!t) return out;

    int det_id = -1;
    double phi = -9999.0;
    double z = -9999.0;
    if (!t->GetBranch("det_id") || !t->GetBranch("scan_phi_deg") ||
        !t->GetBranch("scan_z_mm")) {
        return out;
    }
    t->SetBranchAddress("det_id", &det_id);
    t->SetBranchAddress("scan_phi_deg", &phi);
    t->SetBranchAddress("scan_z_mm", &z);
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        if (phi > -9000.0 && z > -9000.0) out[det_id] = {phi, z};
    }
    return out;
}

TH2D* MakeMap(const char* name, const char* title)
{
    auto* h = new TH2D(name, title, kNPhi, kPhiMin, kPhiMax, kNZ, kZEdges);
    h->SetDirectory(nullptr);
    h->GetXaxis()->SetTitle("angle [deg]");
    h->GetYaxis()->SetTitle("cask height [mm]");
    h->GetZaxis()->SetTitle("counts");
    h->SetStats(false);
    return h;
}

void DuplicateZero(TH2D& h)
{
    const int bx0 = h.GetXaxis()->FindBin(0.0);
    const int bx360 = h.GetXaxis()->FindBin(360.0);
    for (int by = 1; by <= h.GetNbinsY(); ++by) {
        h.SetBinContent(bx360, by, h.GetBinContent(bx0, by));
    }
}

void DrawMap(TH2D& h, const std::string& stem)
{
    gStyle->SetOptStat(0);
    TCanvas c("c_phi_z", "phi vs z", 1600, 900);
    c.SetLeftMargin(0.11);
    c.SetRightMargin(0.17);
    c.SetBottomMargin(0.13);
    c.SetTopMargin(0.08);
    gStyle->SetPalette(kRainBow);
    h.Draw("COLZ");
    c.SaveAs((stem + ".png").c_str());
    c.SaveAs((stem + ".pdf").c_str());
}

double MapCorrelation(const TH2D& a, const TH2D& b)
{
    double sa = 0.0, sb = 0.0, saa = 0.0, sbb = 0.0, sab = 0.0;
    int n = 0;
    for (int bx = 1; bx <= 24; ++bx) {
        for (int by = 1; by <= kNZ; ++by) {
            const double x = a.GetBinContent(bx, by);
            const double y = b.GetBinContent(bx, by);
            sa += x; sb += y; saa += x*x; sbb += y*y; sab += x*y;
            ++n;
        }
    }
    const double den = std::sqrt((n*saa - sa*sa) * (n*sbb - sb*sb));
    return den > 0.0 ? (n*sab - sa*sb) / den : 0.0;
}

} // namespace

void analyze_benchmark(const char* input,
                       const char* particle = "gamma",
                       const char* outPrefix = "",
                       const char* wssaRoot = "")
{
    TFile fin(input, "READ");
    if (fin.IsZombie()) {
        std::cerr << "cannot open " << input << "\n";
        return;
    }

    auto* hits = dynamic_cast<TTree*>(fin.Get("hits"));
    if (!hits) {
        std::cerr << "no hits tree in " << input << "\n";
        return;
    }

    const auto detMap = ReadMeta(fin);
    const Cut cut = MakeCut(particle);
    std::string stem = outPrefix[0] ? outPrefix : input;
    if (stem.size() > 5 && stem.compare(stem.size() - 5, 5, ".root") == 0) {
        stem.erase(stem.size() - 5);
    }
    stem += "_maps";

    auto* h = MakeMap("hPhiZ", "CLYC detector counts");
    auto* hNorm = MakeMap("hPhiZ_norm", "CLYC detector counts (normalised)");

    double pid = 0.0;
    double edep = 0.0;
    double t = 0.0;
    int det = -1;
    double weight = 1.0;

    hits->SetBranchAddress("pid", &pid);
    hits->SetBranchAddress("edep", &edep);
    hits->SetBranchAddress("t", &t);
    hits->SetBranchAddress("det", &det);
    hits->SetBranchAddress("weight", &weight);

    Long64_t used = 0;
    Long64_t missingMeta = 0;
    for (Long64_t i = 0; i < hits->GetEntries(); ++i) {
        hits->GetEntry(i);
        if (t > kPromptCutNs) continue;
        if (!PassPid(cut, static_cast<long long>(pid))) continue;
        if (edep < cut.eLo || edep > cut.eHi) continue;

        auto it = detMap.find(det);
        if (it == detMap.end()) {
            ++missingMeta;
            continue;
        }
        h->Fill(it->second.phi, it->second.z, weight);
        ++used;
    }

    DuplicateZero(*h);
    hNorm->Add(h);
    const double hmax = hNorm->GetMaximum();
    if (hmax > 0.0) hNorm->Scale(1.0 / hmax);
    DuplicateZero(*hNorm);

    TH2D* hWssa = nullptr;
    double corr = 0.0;
    if (wssaRoot[0]) {
        TFile wf(wssaRoot, "READ");
        auto* fine = dynamic_cast<TH2D*>(wf.Get("hPhiZ_fine"));
        if (fine) {
            hWssa = MakeMap("hPhiZ_wssa", "WSSA expected counts");
            for (const auto& kv : detMap) {
                const double phi = kv.second.phi;
                const double z = kv.second.z;
                hWssa->Fill(phi, z, fine->GetBinContent(
                    fine->GetXaxis()->FindBin(phi),
                    fine->GetYaxis()->FindBin(z)));
            }
            DuplicateZero(*hWssa);
            corr = MapCorrelation(*hNorm, *hWssa);
            const double wmax = hWssa->GetMaximum();
            if (wmax > 0.0) hWssa->Scale(1.0 / wmax);
        } else {
            std::cerr << "no hPhiZ_fine in " << wssaRoot << "\n";
        }
    }

    TFile fout((stem + ".root").c_str(), "RECREATE");
    h->Write();
    hNorm->Write();
    if (hWssa) hWssa->Write();
    fout.Close();

    DrawMap(*h, stem + "_phi_z");
    DrawMap(*hNorm, stem + "_phi_z_norm");
    if (hWssa) DrawMap(*hWssa, stem + "_phi_z_wssa");

    std::cout << "input          : " << input << "\n"
              << "particle       : " << particle << "\n"
              << "meta points    : " << detMap.size() << "\n"
              << "hits           : " << hits->GetEntries() << "\n"
              << "selected       : " << used << "\n"
              << "missing meta   : " << missingMeta << "\n"
              << "integral       : " << h->Integral() << "\n";
    if (hWssa) std::cout << "WSSA corr      : " << corr << "\n";
    std::cout << "output         : " << stem << ".root\n";
}
