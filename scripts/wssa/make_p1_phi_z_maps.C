// make_p1_phi_z_maps.C
//
// Sum the p1 per-assembly surface-flux files into one cask phi-z map for
// gammas and one for neutrons, in the same 72 x 80 binning and 0-4000 mm
// height range as the supplied WTI reference PNGs.
//
// The p1 files are per base fuel and per source.  The 84-assembly cask is
// obtained by rotating each base-fuel map by the six C6 hexant angles.  The
// p1 `z` is in the cask-body frame (centre at 0); the reference maps use
// z = 0 at the bottom, so this macro adds 2040 mm.
//
// The p1 trees are large: 8.2e9 entries and 529 GB in total.  Set
// P1_MAX_ENTRIES to read a leading prefix of each file and scale that shape
// to the sum of weights in g4_p1_surface_rates.csv.  Set 0 for all entries.
//
// Usage:
//   root -l -b -q 'scripts/wssa/make_p1_phi_z_maps.C+'
//
// Input:
//   report/wti-binary-reader/data/g4_p1_surface_rates.csv
//   P1_BASE_DIR overrides the stale /home/turko46/my-bigdata-dcs/SurfaceFlux_v3
//   prefix in the CSV paths.  P1_MAX_ENTRIES is the per-file entry limit
//   (0 = all entries; the Makefile default is 2000000).
//
// Output:
//   report/wti-binary-reader/data/p1_phi_z_maps.root
//   report/wti-binary-reader/data/p1_phi_z_maps_summary.csv

#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TNamed.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "ICRP74H10.hh"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kCaskHalfHeightMM = 2040.0;  // p1 z is body-centred
constexpr double kRcm = 127.5;                // p1 side radius
constexpr double kDphiDeg = 5.0;
constexpr double kDzCm = 5.0;
constexpr int kPhiBins = 72;
constexpr int kZBins = 80;
constexpr int kGammaEnergyBins = 200;
constexpr double kGammaEnergyMaxMeV = 4.0;
constexpr int kNeutronEnergyBins = 120;
constexpr double kNeutronEnergyMinMeV = 1.0e-12;
constexpr double kNeutronEnergyMaxMeV = 30.0;
constexpr double kAreaCm2 =
    kRcm * (kDphiDeg * kPi / 180.0) * kDzCm;  // 55.67 cm2
constexpr double kCapAreaPerPhiCm2 =
    kPi * kRcm * kRcm / kPhiBins;             // bottom/top cap per phi bin
constexpr double kRateToDose = 3600.0 / 1.0e6;  // pSv/s -> uSv/h
constexpr double kZMaxMM = 4000.0;
constexpr int kHexants = 6;

struct Row {
    std::string source;
    int base = -1;
    double nEvts = 0.0;
    double lambda = 0.0;
    double rGamma = 0.0;
    double rNeutron = 0.0;
    std::string path;
};

std::vector<std::string> SplitCsv(const std::string& line)
{
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, ',')) {
        if (!item.empty() && item.back() == '\r') item.pop_back();
        out.push_back(item);
    }
    return out;
}

double P1PhiCWFromY(double x, double y)
{
    double p = 90.0 - std::atan2(y, x) * 180.0 / kPi;
    p = std::fmod(p, 360.0);
    if (p < 0.0) p += 360.0;
    return p;
}

std::string ResolvePath(const std::string& csvPath)
{
    const char* root = std::getenv("P1_BASE_DIR");
    if (!root || !*root) root = std::getenv("P1_DATA_ROOT");
    if (!root || !*root) return csvPath;

    const std::string stale = "/home/turko46/my-bigdata-dcs/SurfaceFlux_v3";
    if (csvPath.rfind(stale, 0) == 0)
        return std::string(root) + csvPath.substr(stale.size());
    return csvPath;
}

std::string SummaryPath(const std::string& rootPath)
{
    const std::string suffix = ".root";
    if (rootPath.size() > suffix.size() &&
        rootPath.compare(rootPath.size() - suffix.size(), suffix.size(), suffix) == 0)
        return rootPath.substr(0, rootPath.size() - suffix.size()) + "_summary.csv";
    return rootPath + "_summary.csv";
}

void AddScaled(TH2D* target, TH2D* source, double scale)
{
    if (!target || !source) return;
    target->Add(source, scale);
}

void PrintMapStats(const char* label, TH2D* weightMap, TH2D* doseMap,
                   double expectedRate)
{
    const double rate = weightMap->Integral(1, weightMap->GetNbinsX(),
                                            1, weightMap->GetNbinsY());
    const double ratio = (expectedRate > 0.0) ? rate / expectedRate : 0.0;
    int bx = 1, by = 1, bz = 1;
    doseMap->GetMaximumBin(bx, by, bz);
    std::cout << std::left << std::setw(10) << label
              << " rate=" << std::scientific << std::setprecision(4) << rate
              << "  expected=" << expectedRate
              << "  ratio=" << std::fixed << std::setprecision(4) << ratio
              << " max=" << std::setprecision(4) << doseMap->GetMaximum()
              << " uSv/h at phi=" << doseMap->GetXaxis()->GetBinCenter(bx)
              << " deg, z=" << doseMap->GetYaxis()->GetBinCenter(by)
              << " mm\n";
}

// Fills one per-file map and returns the sampled sum of weights for each
// particle species.  Only the six branches needed by the map are read.
void FillFile(const std::string& path, int maxEntries,
              TH2D* gammaWeight, TH2D* gammaDose,
              TH2D* neutronWeight, TH2D* neutronDose,
              TH1D* gammaEnergy, TH1D* neutronEnergy,
              double& sampleGamma, double& sampleNeutron,
              long long& entriesRead)
{
    TFile file(path.c_str(), "READ");
    if (file.IsZombie())
        throw std::runtime_error("cannot open " + path);

    TTree* tree = dynamic_cast<TTree*>(file.Get("surfaceFlux"));
    if (!tree) throw std::runtime_error("no surfaceFlux tree in " + path);

    tree->SetBranchStatus("*", 0);
    for (const char* name : {"pid", "ekin", "x", "y", "z", "weight"})
        tree->SetBranchStatus(name, 1);

    double pid = 0.0;
    double ekin = 0.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double weight = 0.0;
    tree->SetBranchAddress("pid", &pid);
    tree->SetBranchAddress("ekin", &ekin);
    tree->SetBranchAddress("x", &x);
    tree->SetBranchAddress("y", &y);
    tree->SetBranchAddress("z", &z);
    tree->SetBranchAddress("weight", &weight);

    const long long nAll = tree->GetEntries();
    const long long n = (maxEntries > 0) ? std::min<long long>(nAll, maxEntries)
                                         : nAll;
    for (long long i = 0; i < n; ++i) {
        if (tree->GetEntry(i) <= 0) continue;

        const double phi0 = P1PhiCWFromY(x, y);
        const double zmm = z + kCaskHalfHeightMM;
        double phi[kHexants];
        for (int ih = 0; ih < kHexants; ++ih) {
            phi[ih] = std::fmod(phi0 - 60.0 * ih + 720.0, 360.0);
        }

        // The bottom and top caps use the cap area per phi bin; the side
        // uses the constant side-bin area.
        const double area =
            (z < -2039.0 || z > 2039.0) ? kCapAreaPerPhiCm2 : kAreaCm2;
        if (pid == 22.0) {
            sampleGamma += weight;
            const double dw =
                weight * icrp74h10::h10(ekin, icrp74h10::kPhoton) / area;
            if (ekin > 0.0 && ekin <= kGammaEnergyMaxMeV)
                gammaEnergy->Fill(ekin, weight);
            for (int ih = 0; ih < kHexants; ++ih) {
                gammaWeight->Fill(phi[ih], zmm, weight);
                gammaDose->Fill(phi[ih], zmm, dw);
            }
        } else if (pid == 2112.0) {
            sampleNeutron += weight;
            const double dw =
                weight * icrp74h10::h10(ekin, icrp74h10::kNeutron) / area;
            if (ekin >= kNeutronEnergyMinMeV && ekin <= kNeutronEnergyMaxMeV) {
                neutronEnergy->Fill(ekin, weight);
            }
            for (int ih = 0; ih < kHexants; ++ih) {
                neutronWeight->Fill(phi[ih], zmm, weight);
                neutronDose->Fill(phi[ih], zmm, dw);
            }
        }
    }
    entriesRead = n;
}

}  // namespace

void make_p1_phi_z_maps(
    const char* csvPath =
        "report/wti-binary-reader/data/g4_p1_surface_rates.csv",
    const char* outPath =
        "report/wti-binary-reader/data/p1_phi_z_maps.root",
    int maxRows = -1,
    int nThreads = 0,
    int maxEntries = -1)
{
    (void)nThreads;  // kept for command-line compatibility; this path is serial

    if (maxEntries < 0) {
        maxEntries = 0;
        if (const char* env = std::getenv("P1_MAX_ENTRIES")) {
            try { maxEntries = std::stoi(env); } catch (...) { maxEntries = 0; }
        }
    }
    if (maxEntries < 0) maxEntries = 0;

    std::ifstream in(csvPath);
    if (!in) throw std::runtime_error(std::string("cannot open ") + csvPath);

    std::string line;
    if (!std::getline(in, line))
        throw std::runtime_error("empty p1 rate CSV");

    std::vector<Row> rows;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        const auto f = SplitCsv(line);
        if (f.size() < 10) continue;
        Row row;
        row.source = f[0];
        row.base = std::stoi(f[1]);
        row.nEvts = std::stod(f[2]);
        row.lambda = std::stod(f[3]);
        row.rGamma = std::stod(f[7]);
        row.rNeutron = std::stod(f[8]);
        row.path = f[9];
        if (row.nEvts > 0.0 && row.lambda > 0.0) rows.push_back(row);
    }
    if (rows.empty()) throw std::runtime_error("no p1 rows in CSV");

    std::cout << "p1 aggregation: " << rows.size() << " source/base files\n"
              << "  bins: " << kPhiBins << " phi x " << kZBins << " z"
              << " (0-360 deg, 0-4000 mm)\n"
              << "  side area: " << kAreaCm2 << " cm2\n"
              << "  cap area per phi bin: " << kCapAreaPerPhiCm2 << " cm2\n"
              << "  z shift: +" << kCaskHalfHeightMM << " mm\n"
              << "  max entries/file: "
              << (maxEntries > 0 ? std::to_string(maxEntries) : "all") << "\n";

    TH2D* hGammaWeight = new TH2D(
        "hP1GammaWeight",
        ";#varphi [deg];cask height z [mm];p1 #gamma crossings [s^{-1}]",
        kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
    TH2D* hGammaDose = new TH2D(
        "hP1GammaDose",
        ";#varphi [deg];cask height z [mm];p1 #gamma dose rate [#muSv h^{-1}]",
        kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
    TH2D* hNeutronWeight = new TH2D(
        "hP1NeutronWeight",
        ";#varphi [deg];cask height z [mm];p1 neutron crossings [s^{-1}]",
        kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
    TH2D* hNeutronDose = new TH2D(
        "hP1NeutronDose",
        ";#varphi [deg];cask height z [mm];p1 neutron dose rate [#muSv h^{-1}]",
        kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);

    TH1D* hGammaEnergy = new TH1D(
        "hP1GammaEnergy", ";energy [MeV];p1 #gamma yield / 20 keV",
        kGammaEnergyBins, 0.0, kGammaEnergyMaxMeV);
    std::vector<double> neutronEdges(kNeutronEnergyBins + 1);
    const double logEmin = std::log10(kNeutronEnergyMinMeV);
    const double logEmax = std::log10(kNeutronEnergyMaxMeV);
    for (int i = 0; i <= kNeutronEnergyBins; ++i) {
        const double t = static_cast<double>(i) / kNeutronEnergyBins;
        neutronEdges[static_cast<std::size_t>(i)] =
            std::pow(10.0, logEmin + (logEmax - logEmin) * t);
    }
    TH1D* hNeutronEnergy = new TH1D(
        "hP1NeutronEnergy", ";energy [MeV];p1 n yield / log bin",
        kNeutronEnergyBins, neutronEdges.data());

    for (TH2D* h : {hGammaWeight, hGammaDose, hNeutronWeight, hNeutronDose})
        h->SetDirectory(nullptr);
    hGammaEnergy->SetDirectory(nullptr);
    hNeutronEnergy->SetDirectory(nullptr);

    int processed = 0;
    int missing = 0;
    double expectedGamma = 0.0;
    double expectedNeutron = 0.0;
    long long usedEntries = 0;

    for (const auto& row : rows) {
        if (maxRows > 0 && processed >= maxRows) break;

        const std::string path = ResolvePath(row.path);
        if (gSystem->AccessPathName(path.c_str())) {
            std::cerr << "missing p1 file: " << path << "\n";
            ++missing;
            continue;
        }

        TH2D gW("gw", "", kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
        TH2D gD("gd", "", kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
        TH2D nW("nw", "", kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
        TH2D nD("nd", "", kPhiBins, 0.0, 360.0, kZBins, 0.0, kZMaxMM);
        TH1D gE("ge", "", kGammaEnergyBins, 0.0, kGammaEnergyMaxMeV);
        TH1D nE("ne", "", kNeutronEnergyBins, neutronEdges.data());
        for (TH2D* h : {&gW, &gD, &nW, &nD}) h->SetDirectory(nullptr);
        gE.SetDirectory(nullptr);
        nE.SetDirectory(nullptr);

        double sampleGamma = 0.0;
        double sampleNeutron = 0.0;
        long long entriesRead = 0;
        FillFile(path, maxEntries, &gW, &gD, &nW, &nD, &gE, &nE,
                 sampleGamma, sampleNeutron, entriesRead);
        usedEntries += entriesRead;

        const double fullGamma = row.rGamma * row.nEvts;
        const double fullNeutron = row.rNeutron * row.nEvts;
        const double baseRate = row.lambda / row.nEvts;
        const double scaleGamma =
            baseRate * (sampleGamma > 0.0 ? fullGamma / sampleGamma : 0.0);
        const double scaleNeutron =
            baseRate * (sampleNeutron > 0.0 ? fullNeutron / sampleNeutron : 0.0);
        const double scaleGammaDose = scaleGamma * kRateToDose;
        const double scaleNeutronDose = scaleNeutron * kRateToDose;

        AddScaled(hGammaWeight, &gW, scaleGamma);
        AddScaled(hGammaDose, &gD, scaleGammaDose);
        AddScaled(hNeutronWeight, &nW, scaleNeutron);
        AddScaled(hNeutronDose, &nD, scaleNeutronDose);
        // The six C6 hexants have the same energy distribution; scale the
        // per-record spectrum by six as well as by the source-rate factor.
        hGammaEnergy->Add(&gE, kHexants * scaleGamma);
        hNeutronEnergy->Add(&nE, kHexants * scaleNeutron);

        std::cout << "  [" << std::setw(2) << processed << "] "
                  << std::left << std::setw(15) << row.source
                  << " base " << std::setw(2) << row.base
                  << " entries=" << entriesRead
                  << "  gamma_scale=" << std::scientific << std::setprecision(3)
                  << scaleGamma << "  " << path << "\n";

        ++processed;
        expectedGamma += kHexants * row.lambda * row.rGamma;
        expectedNeutron += kHexants * row.lambda * row.rNeutron;
    }

    if (processed == 0)
        throw std::runtime_error(
            "no p1 files were read; set P1_BASE_DIR to the SurfaceFlux_v3 root");

    PrintMapStats("gamma", hGammaWeight, hGammaDose, expectedGamma);
    PrintMapStats("neutron", hNeutronWeight, hNeutronDose, expectedNeutron);

    gSystem->mkdir(gSystem->DirName(outPath), kTRUE);
    TFile out(outPath, "RECREATE");
    if (out.IsZombie())
        throw std::runtime_error(std::string("cannot create ") + outPath);

    hGammaWeight->Write();
    hGammaDose->Write();
    hNeutronWeight->Write();
    hNeutronDose->Write();
    hGammaEnergy->Write();
    hNeutronEnergy->Write();

    const char* baseDir = std::getenv("P1_BASE_DIR");
    if (!baseDir || !*baseDir) baseDir = std::getenv("P1_DATA_ROOT");
    const TString infoTitle = TString::Format(
        "source=%s; rows_used=%d; rows_missing=%d; p1_base_dir=%s; "
        "phi=clockwise from +y; z=body z + %.0f mm; bins=%d x %d; "
        "side_area_cm2=%.6f; cap_area_per_phi_cm2=%.6f; h10=ICRP74/ICRU57; gamma_pid=22; neutron_pid=2112; "
        "hexants=%d; max_entries_per_file=%d; entries_read=%lld",
        csvPath, processed, missing, (baseDir && *baseDir) ? baseDir : "(csv paths)",
        kCaskHalfHeightMM, kPhiBins, kZBins, kAreaCm2, kCapAreaPerPhiCm2, kHexants, maxEntries,
        usedEntries);
    TNamed info("p1_map_info", infoTitle.Data());
    info.Write();
    out.Close();

    std::ofstream summary(SummaryPath(outPath));
    summary << "particle,files_used,files_missing,rate_per_s,expected_rate_per_s,"
               "ratio,map_max_uSv_h,max_phi_deg,max_z_mm\n";
    for (int i = 0; i < 2; ++i) {
        const char* label = (i == 0) ? "gamma" : "neutron";
        TH2D* w = (i == 0) ? hGammaWeight : hNeutronWeight;
        TH2D* d = (i == 0) ? hGammaDose : hNeutronDose;
        const double expected = (i == 0) ? expectedGamma : expectedNeutron;
        const double rate =
            w->Integral(1, w->GetNbinsX(), 1, w->GetNbinsY());
        int bx = 1, by = 1, bz = 1;
        d->GetMaximumBin(bx, by, bz);
        const double ratio = (expected > 0.0) ? rate / expected : 0.0;
        summary << label << ',' << processed << ',' << missing << ','
                << std::setprecision(10) << rate << ',' << expected << ','
                << ratio << ',' << d->GetMaximum() << ','
                << d->GetXaxis()->GetBinCenter(bx) << ','
                << d->GetYaxis()->GetBinCenter(by) << '\n';
    }
    summary.close();

    std::cout << "wrote " << outPath << "\n"
              << "wrote " << SummaryPath(outPath) << "\n";
}
