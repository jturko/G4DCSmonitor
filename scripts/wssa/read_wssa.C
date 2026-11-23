// read_wssa.C
//
// Read an MCNP surface-source (.wssa) file and produce:
//   * a log-binned energy histogram (sum of statistical weights),
//   * a phi-vs-height histogram (sum of statistical weights),
//   * a phi-vs-height copy clipped to 0-4000 mm for comparison with the
//     reference .png files supplied with the data,
//   * side/top breakdowns and 1-D projections.
//
// The binary layout is described in LA-UR-24-29475 (Kulesza, 2024).
// The parser lives in include/WSSAReader.hh and is shared with the Geant4
// implementation.
//
// Usage (from the repository root):
//
//   root -l -b -q 'scripts/wssa/read_wssa.C("FILE.wssa","OUT_PREFIX",N,true)'
//
// Arguments:
//   FILE.wssa   input file
//   OUT_PREFIX  output prefix (empty -> derived from the input file name)
//   N           number of samples or records (default 5000)
//   randomMode  true  -> N random record indices across the whole file
//               false -> the first N records in file order
//   phiMode     1 = clockwise from +y (matches the reference PNG convention;
//                   default)
//               0 = atan2 counter-clockwise from +x
//               2 = clockwise from +x
//   weighted    true -> fill with wgt, false -> fill with 1 per record
//   nPhiBins, nZBins, zMaxMM, nEBins, eMinMeV, eMaxMeV
//
// The "alt" canvas always uses the complementary phi convention (0 <-> 1)
// so both orientations can be compared with the reference image.

#include "../../include/WSSAReader.hh"

#include "TCanvas.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLatex.h"
#include "TNamed.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TString.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::string DerivePrefix(const std::string& input)
{
    std::string base = input;
    const std::size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const std::size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return base;
}

void EnsureParentDirectory(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return;
    const std::string dir = path.substr(0, slash);
    if (!dir.empty()) gSystem->mkdir(dir.c_str(), kTRUE);
}

double PhiCCWFromX(double x, double y)
{
    double p = std::atan2(y, x) * 180.0 / kPi;
    if (p < 0.0) p += 360.0;
    return p;
}

double PhiCWFromY(double x, double y)
{
    double p = 90.0 - std::atan2(y, x) * 180.0 / kPi;
    p = std::fmod(p, 360.0);
    if (p < 0.0) p += 360.0;
    return p;
}

double PhiCWFromX(double x, double y)
{
    double p = -std::atan2(y, x) * 180.0 / kPi;
    p = std::fmod(p, 360.0);
    if (p < 0.0) p += 360.0;
    return p;
}

double PhiForMode(double x, double y, int mode)
{
    if (mode == 0) return PhiCCWFromX(x, y);
    if (mode == 1) return PhiCWFromY(x, y);
    return PhiCWFromX(x, y);
}

enum class SurfaceKind { Unknown, Side, Top };

SurfaceKind ClassifySurface(const wssa::Record& r,
                            const std::map<int, double>& coeffById)
{
    const auto it = coeffById.find(r.surfaceId());
    if (it == coeffById.end()) return SurfaceKind::Unknown;

    const double coeff = it->second;
    const double r_cm  = std::hypot(r.x, r.y);
    const double dCyl  = std::fabs(std::sqrt(std::max(0.0, coeff)) - r_cm);
    const double dPlane = std::fabs(coeff - r.z);

    if (dCyl < 0.5 && dCyl < dPlane) return SurfaceKind::Side;
    if (dPlane < 0.5)                return SurfaceKind::Top;
    return SurfaceKind::Unknown;
}

void StyleCanvas(TCanvas& c)
{
    c.SetRightMargin(0.18);
    c.SetLeftMargin(0.10);
    c.SetBottomMargin(0.13);
    c.SetTopMargin(0.08);
}

void DrawPhiZ(TH2D* h, const std::string& png, const std::string& pdf,
              const char* title)
{
    if (!h || h->GetEntries() == 0) return;
    TCanvas c("c_phiz", "phi vs z", 1600, 900);
    StyleCanvas(c);
    h->SetTitle(title);
    h->SetStats(kFALSE);
    h->GetXaxis()->SetTitle("angle [deg]");
    h->GetYaxis()->SetTitle("cask height [mm]");
    h->GetZaxis()->SetTitle("sum of weights");
    gStyle->SetPalette(kRainBow);
    h->Draw("COLZ");
    c.SaveAs(png.c_str());
    c.SaveAs(pdf.c_str());
}

struct DupDiagnostics {
    long long groups = 0;
    long long records = 0;
    std::map<int, long long> countByMultiplicity;
    std::map<int, double>    weightByMultiplicity;

    bool            havePrev = false;
    wssa::Record    prev;
    long long       run = 0;

    void add(const wssa::Record& r)
    {
        ++records;
        if (havePrev && r.sameAs(prev)) {
            ++run;
        } else {
            flush();
            prev = r;
            run = 1;
            havePrev = true;
        }
    }

    void finish() { flush(); }

    void flush()
    {
        if (run > 0) {
            ++groups;
            ++countByMultiplicity[static_cast<int>(run)];
            weightByMultiplicity[static_cast<int>(run)] += prev.wgt * run;
        }
        run = 0;
    }
};

} // namespace

// ---------------------------------------------------------------------------
void read_wssa(const char* input,
               const char* outPrefix = "",
               long long nEvents = 5000,
               bool randomMode = true,
               int phiMode = 1,
               bool weighted = true,
               int nPhiBins = 72,
               int nZBins = 82,
               double zMaxMM = 4100.0,
               int nEBins = 120,
               double eMinMeV = 1e-9,
               double eMaxMeV = 20.0,
               unsigned long long seed = 12345)
{
    gStyle->SetOptStat(0);
    gStyle->SetPalette(kRainBow);
    gStyle->SetNumberContours(99);

    const std::string in(input);
    std::string out = (outPrefix && outPrefix[0]) ? std::string(outPrefix)
                                                  : DerivePrefix(in);
    EnsureParentDirectory(out);

    std::string err;
    wssa::Reader reader;
    if (!reader.open(in, err)) {
        std::cerr << "ERROR: " << err << "\n";
        return;
    }
    if (!reader.readHeader(err)) {
        std::cerr << "ERROR: " << err << "\n";
        return;
    }

    const wssa::Header& h = reader.header();

    // Derived surface geometry (for classification and reporting only).
    double rSurfCm = 0.0;
    for (const auto& s : h.surfaces)
        rSurfCm = std::max(rSurfCm, std::sqrt(std::max(0.0, s.coeff)));
    double zTopCm = 0.0;
    for (const auto& s : h.surfaces) {
        const double r = std::sqrt(std::max(0.0, s.coeff));
        if (s.coeff > rSurfCm && r < rSurfCm)
            zTopCm = std::max(zTopCm, s.coeff);
    }

    std::map<int, double> coeffById;
    for (const auto& s : h.surfaces) coeffById[s.jss] = s.coeff;

    std::cout << "=== WSSA header ===\n"
              << "  identifier : " << reader.identifier() << "\n"
              << "  code       : " << h.codeName << "  version " << h.version
              << "\n"
              << "  aid        : " << h.aid << "\n"
              << "  nrsw       : " << h.nrsw << " particle records\n"
              << "  njss       : " << h.njss << " surface records, kq="
              << h.kq << ", nilw=" << h.nilw << ", mipt=" << h.mipt << "\n"
              << "  particles  : start byte " << h.particleStart
              << ", record size " << h.particleRecordBytes << " bytes\n"
              << "  geometry   : R_surface = " << rSurfCm * 10.0
              << " mm, top plane z = " << zTopCm * 10.0 << " mm\n";
    for (const auto& s : h.surfaces) {
        std::cout << "    surface jss=" << s.jss
                  << " type=" << s.type
                  << " n=" << s.n
                  << " coeff=" << s.coeff
                  << "  (sqrt=" << std::sqrt(std::max(0.0, s.coeff)) << ")\n";
    }

    // Histograms.  The energy histogram needs genuinely log-spaced edges;
    // setting the canvas to log-x on a linear-binned TH1D is not enough.
    if (nEBins < 1) nEBins = 1;
    if (eMinMeV <= 0.0) eMinMeV = 1e-12;
    if (eMaxMeV <= eMinMeV) eMaxMeV = 10.0 * eMinMeV;
    std::vector<double> eEdges(static_cast<std::size_t>(nEBins) + 1);
    const double logEmin = std::log10(eMinMeV);
    const double logEmax = std::log10(eMaxMeV);
    for (int i = 0; i <= nEBins; ++i) {
        const double t = static_cast<double>(i) / nEBins;
        eEdges[static_cast<std::size_t>(i)] =
            std::pow(10.0, logEmin + (logEmax - logEmin) * t);
    }
    auto* hE = new TH1D("hE", ";energy [MeV];sum of weights",
                        nEBins, eEdges.data());
    auto* hPhiZ = new TH2D("hPhiZ",
                           ";angle [deg];cask height [mm];sum of weights",
                           nPhiBins, 0.0, 360.0, nZBins, 0.0, zMaxMM);
    auto* hPhiZ_alt = new TH2D("hPhiZ_alt",
                               ";angle [deg];cask height [mm];sum of weights",
                               nPhiBins, 0.0, 360.0, nZBins, 0.0, zMaxMM);
    auto* hPhiZ_0_4000 = new TH2D("hPhiZ_0_4000",
                                  ";angle [deg];cask height [mm];sum of weights",
                                  nPhiBins, 0.0, 360.0, 80, 0.0, 4000.0);
    auto* hPhiZ_side = new TH2D("hPhiZ_side",
                                ";angle [deg];cask height [mm];sum of weights",
                                nPhiBins, 0.0, 360.0, nZBins, 0.0, zMaxMM);
    auto* hPhiZ_top = new TH2D("hPhiZ_top",
                               ";angle [deg];cask height [mm];sum of weights",
                               nPhiBins, 0.0, 360.0, nZBins, 0.0, zMaxMM);
    auto* hPhiZ_unknown = new TH2D("hPhiZ_unknown",
                                   ";angle [deg];cask height [mm];sum of weights",
                                   nPhiBins, 0.0, 360.0, nZBins, 0.0, zMaxMM);
    auto* hPhi = new TH1D("hPhi", ";angle [deg];sum of weights",
                          nPhiBins, 0.0, 360.0);
    auto* hZ = new TH1D("hZ", ";cask height [mm];sum of weights",
                        nZBins, 0.0, zMaxMM);

    hE->SetDirectory(nullptr);
    hPhiZ->SetDirectory(nullptr);
    hPhiZ_alt->SetDirectory(nullptr);
    hPhiZ_0_4000->SetDirectory(nullptr);
    hPhiZ_side->SetDirectory(nullptr);
    hPhiZ_top->SetDirectory(nullptr);
    hPhiZ_unknown->SetDirectory(nullptr);
    hPhi->SetDirectory(nullptr);
    hZ->SetDirectory(nullptr);

    const int altMode = (phiMode == 0) ? 1 : 0;

    long long nProcessed = 0;
    long long nSide = 0, nTop = 0, nUnknown = 0;
    long long nEnergyInRange = 0;
    double minE = 1e300, maxE = -1e300;
    double minZ = 1e300, maxZ = -1e300;
    double sumW = 0.0;

    DupDiagnostics dup;

    auto process = [&](const wssa::Record& r) {
        ++nProcessed;

        const double x = r.x;
        const double y = r.y;
        const double z_cm = r.z;
        const double E = r.erg;
        const double z_mm = z_cm * 10.0;   // MCNP z is floor-based, in cm
        const double w = weighted ? r.wgt : 1.0;

        if (E > 0.0) {
            minE = std::min(minE, E);
            maxE = std::max(maxE, E);
        }
        minZ = std::min(minZ, z_mm);
        maxZ = std::max(maxZ, z_mm);
        sumW += w;

        const double phi = PhiForMode(x, y, phiMode);
        const double phiAlt = PhiForMode(x, y, altMode);

        if (E > 0.0 && E >= eMinMeV && E <= eMaxMeV) {
            hE->Fill(E, w);
            ++nEnergyInRange;
        }

        hPhiZ->Fill(phi, z_mm, w);
        hPhiZ_alt->Fill(phiAlt, z_mm, w);
        hPhiZ_0_4000->Fill(phi, z_mm, w);
        hPhi->Fill(phi, w);
        hZ->Fill(z_mm, w);

        const SurfaceKind kind = ClassifySurface(r, coeffById);
        if (kind == SurfaceKind::Side) {
            ++nSide;
            hPhiZ_side->Fill(phi, z_mm, w);
        } else if (kind == SurfaceKind::Top) {
            ++nTop;
            hPhiZ_top->Fill(phi, z_mm, w);
        } else {
            ++nUnknown;
            hPhiZ_unknown->Fill(phi, z_mm, w);
        }
    };

    wssa::Record rec;

    if (randomMode) {
        long long nSamples = nEvents;
        if (nSamples <= 0) nSamples = 5000;
        nSamples = std::min<long long>(nSamples, static_cast<long long>(h.nrsw));
        if (h.particleRecordBytes <= 0) {
            std::cerr << "ERROR: no particle records\n";
            return;
        }
        std::mt19937_64 rng(seed);
        std::uniform_int_distribution<long long> dist(0, h.nrsw - 1);

        std::cout << "Sampling " << nSamples
                  << " random records out of " << h.nrsw << " ...\n";
        for (long long i = 0; i < nSamples; ++i) {
            const long long idx = dist(rng);
            if (!reader.readAt(idx, rec, err)) {
                std::cerr << "ERROR at random index " << idx << ": " << err
                          << "\n";
                break;
            }
            process(rec);
            if ((i + 1) % 100000 == 0)
                std::cout << "  " << (i + 1) << " / " << nSamples << "\n";
        }
    } else {
        long long limit = (nEvents <= 0) ? h.nrsw
                                         : std::min<long long>(nEvents, static_cast<long long>(h.nrsw));
        std::cout << "Reading the first " << limit << " records of "
                  << h.nrsw << " ...\n";

        while (nProcessed < limit) {
            if (!reader.next(rec, err)) {
                if (nProcessed < h.nrsw)
                    std::cerr << "ERROR after " << nProcessed
                              << " records: " << err << "\n";
                break;
            }
            process(rec);
            dup.add(rec);
            if (nProcessed % 1000000 == 0)
                std::cout << "  " << nProcessed << " / " << limit << "\n";
        }
        dup.finish();
    }

    // -----------------------------------------------------------------------
    // Report
    // -----------------------------------------------------------------------
    std::cout << "\n=== processed ===\n"
              << "  records            : " << nProcessed << "\n"
              << "  side               : " << nSide << "\n"
              << "  top                : " << nTop << "\n"
              << "  unknown surface    : " << nUnknown << "\n"
              << "  energy in range    : " << nEnergyInRange << "\n"
              << "  energy range [MeV] : " << minE << " ... " << maxE << "\n"
              << "  z range [mm]       : " << minZ << " ... " << maxZ << "\n"
              << "  sum of weights     : " << std::setprecision(10) << sumW
              << "\n";

    if (!randomMode) {
        std::cout << "\n=== contiguous identical-record groups ===\n"
                  << "  groups  : " << dup.groups << "\n"
                  << "  records : " << dup.records << "\n";
        std::vector<std::pair<long long, long long>> top;
        for (const auto& kv : dup.countByMultiplicity)
            top.push_back({kv.second, kv.first});
        std::sort(top.begin(), top.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        const std::size_t nmax = std::min<std::size_t>(top.size(), 12);
        for (std::size_t i = 0; i < nmax; ++i) {
            const long long mult = top[i].second;
            const long long cnt  = top[i].first;
            const double    wsum = dup.weightByMultiplicity[static_cast<int>(mult)];
            std::cout << "    multiplicity " << mult << " : " << cnt
                      << " groups, total weight " << wsum << "\n";
        }
    }

    // -----------------------------------------------------------------------
    // Draw and save
    // -----------------------------------------------------------------------
    if (hE->GetEntries() > 0) {
        TCanvas cE("c_energy", "energy", 1200, 800);
        cE.SetLeftMargin(0.12);
        cE.SetBottomMargin(0.16);
        cE.SetLogx();
        hE->SetLineWidth(2);
        hE->GetXaxis()->SetTitleOffset(1.25);
        hE->Draw("HIST");
        cE.SaveAs((out + "_energy.png").c_str());
        cE.SaveAs((out + "_energy.pdf").c_str());
    }

    DrawPhiZ(hPhiZ, out + "_phi_z.png", out + "_phi_z.pdf",
             "phi vs cask height");
    DrawPhiZ(hPhiZ_0_4000, out + "_phi_z_0_4000.png",
             out + "_phi_z_0_4000.pdf", "phi vs cask height (0-4000 mm)");
    DrawPhiZ(hPhiZ_alt, out + "_phi_z_alt.png", out + "_phi_z_alt.pdf",
             "phi vs cask height (alternate convention)");
    DrawPhiZ(hPhiZ_side, out + "_phi_z_side.png", out + "_phi_z_side.pdf",
             "phi vs cask height (side surface)");
    DrawPhiZ(hPhiZ_top, out + "_phi_z_top.png", out + "_phi_z_top.pdf",
             "phi vs cask height (top surface)");
    DrawPhiZ(hPhiZ_unknown, out + "_phi_z_unknown.png",
             out + "_phi_z_unknown.pdf", "phi vs cask height (unknown surface)");

    if (hPhi->GetEntries() > 0 && hZ->GetEntries() > 0) {
        TCanvas cProj("c_proj", "projections", 1400, 800);
        cProj.Divide(2, 1);
        cProj.cd(1);
        hPhi->SetLineWidth(2);
        hPhi->Draw("HIST");
        cProj.cd(2);
        hZ->SetLineWidth(2);
        hZ->Draw("HIST");
        cProj.SaveAs((out + "_projections.png").c_str());
        cProj.SaveAs((out + "_projections.pdf").c_str());
    }

    TFile f((out + ".root").c_str(), "RECREATE");
    hE->Write();
    hPhiZ->Write();
    hPhiZ_alt->Write();
    hPhiZ_0_4000->Write();
    hPhiZ_side->Write();
    hPhiZ_top->Write();
    hPhiZ_unknown->Write();
    hPhi->Write();
    hZ->Write();

    std::ostringstream meta;
    meta << "input=" << in << "; nrsw=" << h.nrsw
         << "; njss=" << h.njss << "; kq=" << h.kq
         << "; processed=" << nProcessed
         << "; randomMode=" << (randomMode ? 1 : 0)
         << "; phiMode=" << phiMode
         << "; weighted=" << (weighted ? 1 : 0);
    TNamed info("wssa_info", meta.str().c_str());
    info.Write();
    f.Close();

    std::cout << "\nWrote " << out << ".root and image files\n";
}
