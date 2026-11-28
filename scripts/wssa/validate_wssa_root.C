// validate_wssa_root.C
//
// ROOT-only validation of the full-statistics WSSA reader output against the
// supplied reference PNG maps.
//
// The macro:
//   * decodes each reference PNG with TASImage,
//   * detects the plot frame and the vertical colour bar,
//   * maps every heat-map pixel to the nearest colour-bar colour,
//   * averages the decoded values into the reference's native 72 x 80 grid
//     and a 36 x 41 orientation grid,
//   * compares those histograms with the full-statistics ROOT histograms,
//   * writes validation text/CSV and ROOT canvases.
//
// The CSV also stores a diagnostic high-pass 2D correlation: a Gaussian-smoothed
// component is subtracted from both maps before the correlation.  It is not a
// score in the report table; it is used to separate a smooth baseline from the
// fine structure.
//
// Usage (from the repository root, after run_all_wssa.C):
//
//   root -l -b -q 'scripts/wssa/validate_wssa_root.C+'
//
// The reference PNGs are decoded as an ordinal 0-1 colour-bar coordinate.  For
// display, the reference is scaled to the dose-rate range printed on its own
// colour bar; the reader map is a sum of MCNP weights and is normalised to its
// own maximum.

#include "TASImage.h"
#include "TCanvas.h"
#include "TColor.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPaveText.h"
#include "TMath.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int    kPhiBins    = 72;
constexpr int    kPhiNativeBins = 36;
constexpr int    kZFineBins  = 80;
constexpr int    kZNativeBins = 41;
constexpr double kZMaxMM     = 4000.0;
constexpr double kColorDist  = 35.0;
// Native 72 x 80 WSSA side-bin area from the report: R = 133.25 cm,
// dphi = 5 deg, dz = 5 cm.  H*(10) is stored as pSv cm^2 / s; dividing by
// this area and multiplying by 3600 / 1e6 gives dose rate in uSv/h.
constexpr double kWssaSideBinAreaCm2 = 58.141;
constexpr double kDoseRateScale =
    3600.0 / (1.0e6 * kWssaSideBinAreaCm2);

struct Rgb {
    int r = 0;
    int g = 0;
    int b = 0;
};

struct RefMap {
    TH2D* fine = nullptr;     // 72 x 80, reference native grid
    TH2D* native = nullptr;   // 36 x 41, orientation grid
    TH2D* display = nullptr;  // 72 x 80, figure grid
    std::vector<char> validFine;
    std::vector<char> validNative;
    int finiteFine = 0;
    int finiteNative = 0;
    double maxDose = 1.0;     // top of this image's colour bar [uSv/h]
    double medianDist = 0.0;
};

struct DataMap {
    std::string tag;
    std::string label;
    std::string cask;
    std::string input;
    TH2D* fine = nullptr;
    TH2D* fineDose = nullptr;
    TH2D* native = nullptr;
    TH2D* alt = nullptr;
    TH2D* display = nullptr;
    TH2D* displayDose = nullptr;
    TH1D* energy = nullptr;
    TH1D* energyLinear = nullptr;
    std::string info;
};

struct Result {
    DataMap data;
    RefMap ref;
    double corr2d = 0.0;
    double corr2dHighpass = 0.0;
    double corr2dDose = 0.0;
    double corr2dDoseHighpass = 0.0;
    double corrPhi1 = 0.0;
    double corrPhiDose = 0.0;
    double corrPhi0 = 0.0;
    double corrZ = 0.0;
    double corrZDose = 0.0;
    double zPeakImage = 0.0;
    double zPeakData = 0.0;
    double zPeakDose = 0.0;
    double phiPeakImage = 0.0;
    double phiPeakData = 0.0;
    double phiPeakDose = 0.0;
    double phiPeakAlt = 0.0;
    double sumDoseWeight = 0.0;
    double npAbs = 1.0;
    std::string doseParticle;
    double sumW = 0.0;
    double minE = 0.0;
    double maxE = 0.0;
    double minZ = 0.0;
    double maxZ = 0.0;
    long long nrsw = 0;
    long long processed = 0;
    long long nSide = 0;
    long long nTop = 0;
    long long nUnknown = 0;
};

Rgb rgbAt(const UInt_t* argb, int w, int x, int y)
{
    const UInt_t q = argb[y * w + x];
    Rgb c;
    c.r = static_cast<int>((q >> 16) & 0xff);
    c.g = static_cast<int>((q >> 8) & 0xff);
    c.b = static_cast<int>(q & 0xff);
    return c;
}

long long dist2(const Rgb& a, const Rgb& b)
{
    const long long dr = a.r - b.r;
    const long long dg = a.g - b.g;
    const long long db = a.b - b.b;
    return dr * dr + dg * dg + db * db;
}

TH2D* newMap(const std::string& name, int nphi, int nz, double zmax)
{
    auto* h = new TH2D(name.c_str(),
                       ";angle [deg];cask height [mm];map value",
                       nphi, 0.0, 360.0, nz, 0.0, zmax);
    h->SetDirectory(nullptr);
    h->SetStats(kFALSE);
    return h;
}

double referenceDoseMax(const std::string& tag)
{
    // Top of the colour bar printed on each supplied reference image, in uSv/h.
    if (tag == "Cask1_neutrons") return 60.0;
    if (tag == "Cask1_gamma")    return 5.0;
    if (tag == "Cask2_neutrons") return 6.0;
    if (tag == "Cask2_gamma")    return 1.4;
    if (tag == "Cask3_neutrons") return 35.0;
    if (tag == "Cask3_gamma")    return 2.5;
    return 1.0;
}

RefMap digitizeReference(const std::string& pngPath, const std::string& tag)
{
    RefMap out;
    TASImage img(pngPath.c_str());
    const int w = static_cast<int>(img.GetWidth());
    const int h = static_cast<int>(img.GetHeight());
    if (w <= 0 || h <= 0) {
        std::cerr << "ERROR: cannot open reference image " << pngPath << "\n";
        return out;
    }
    UInt_t* argb = img.GetArgbArray();
    if (!argb) {
        std::cerr << "ERROR: cannot decode reference image " << pngPath << "\n";
        return out;
    }

    std::vector<int> rowDark(h, 0);
    std::vector<int> colDark(w, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const Rgb c = rgbAt(argb, w, x, y);
            if (c.r < 80 && c.g < 80 && c.b < 80) {
                ++rowDark[y];
                ++colDark[x];
            }
        }
    }

    std::vector<int> rows;
    std::vector<int> cols;
    for (int y = 0; y < h; ++y)
        if (rowDark[y] > 0.5 * w) rows.push_back(y);
    for (int x = 0; x < w; ++x)
        if (colDark[x] > 0.5 * h) cols.push_back(x);

    if (rows.size() < 2 || cols.size() < 4) {
        std::cerr << "ERROR: cannot detect frame in " << pngPath << "\n";
        return out;
    }

    const int y0 = rows[0];
    const int y1 = rows[1];
    const int x0 = cols[0];
    const int x1 = cols[1];
    const int cbx0 = cols[2];
    const int cbx1 = cols[3];
    const int cbx = (cbx0 + cbx1) / 2;

    std::vector<Rgb> bar;
    std::vector<double> barValue;
    for (int y = y0 + 1; y < y1; ++y) {
        bar.push_back(rgbAt(argb, w, cbx, y));
        const int n = y1 - y0 - 1;
        const int k = y - (y0 + 1);
        barValue.push_back(1.0 - static_cast<double>(k) /
                                     static_cast<double>(n - 1));
    }
    if (bar.size() < 2) {
        std::cerr << "ERROR: empty colour bar in " << pngPath << "\n";
        return out;
    }

    out.fine = newMap(tag + "_refFine", kPhiBins, kZFineBins, kZMaxMM);
    out.native = newMap(tag + "_refNative", kPhiNativeBins, kZNativeBins, kZMaxMM);
    out.validFine.assign(out.fine->GetNcells(), 0);
    out.validNative.assign(out.native->GetNcells(), 0);
    out.maxDose = referenceDoseMax(tag);

    std::vector<double> sumFine(out.fine->GetNcells(), 0.0);
    std::vector<double> sumNative(out.native->GetNcells(), 0.0);
    std::vector<int> cntFine(out.fine->GetNcells(), 0);
    std::vector<int> cntNative(out.native->GetNcells(), 0);
    std::vector<double> distances;

    const int nx = x1 - x0 - 1;
    const int ny = y1 - y0 - 1;
    for (int i = 0; i < ny; ++i) {
        const int y = y0 + 1 + i;
        const double z = (ny - (i + 0.5)) / ny * kZMaxMM;
        for (int j = 0; j < nx; ++j) {
            const int x = x0 + 1 + j;
            const Rgb c = rgbAt(argb, w, x, y);
            if ((c.r > 245 && c.g > 245 && c.b > 245) ||
                (c.r < 50 && c.g < 50 && c.b < 50)) {
                continue;
            }

            long long best = dist2(c, bar[0]);
            int bestIndex = 0;
            for (std::size_t k = 1; k < bar.size(); ++k) {
                const long long d = dist2(c, bar[k]);
                if (d < best) {
                    best = d;
                    bestIndex = static_cast<int>(k);
                }
            }
            const double d = std::sqrt(static_cast<double>(best));
            distances.push_back(d);
            if (d > kColorDist) continue;

            const double value = barValue[bestIndex];
            const double phi = (j + 0.5) / nx * 360.0;

            const int bxF = out.fine->GetXaxis()->FindBin(phi);
            const int byF = out.fine->GetYaxis()->FindBin(z);
            const int gbF = out.fine->GetBin(bxF, byF);
            sumFine[gbF] += value;
            ++cntFine[gbF];

            const int bxN = out.native->GetXaxis()->FindBin(phi);
            const int byN = out.native->GetYaxis()->FindBin(z);
            const int gbN = out.native->GetBin(bxN, byN);
            sumNative[gbN] += value;
            ++cntNative[gbN];

        }
    }

    for (int bx = 1; bx <= out.fine->GetNbinsX(); ++bx) {
        for (int by = 1; by <= out.fine->GetNbinsY(); ++by) {
            const int gb = out.fine->GetBin(bx, by);
            if (cntFine[gb] > 0) {
                out.fine->SetBinContent(gb, sumFine[gb] / cntFine[gb]);
                out.validFine[gb] = 1;
                ++out.finiteFine;
            } else {
                out.fine->SetBinContent(gb, 0.0);
            }
        }
    }
    for (int bx = 1; bx <= out.native->GetNbinsX(); ++bx) {
        for (int by = 1; by <= out.native->GetNbinsY(); ++by) {
            const int gb = out.native->GetBin(bx, by);
            if (cntNative[gb] > 0) {
                out.native->SetBinContent(gb, sumNative[gb] / cntNative[gb]);
                out.validNative[gb] = 1;
                ++out.finiteNative;
            } else {
                out.native->SetBinContent(gb, 0.0);
            }
        }
    }
    out.display = dynamic_cast<TH2D*>(
        out.fine->Clone((tag + "_refDisplay").c_str()));
    if (out.display) out.display->SetDirectory(nullptr);

    if (!distances.empty()) {
        std::sort(distances.begin(), distances.end());
        out.medianDist = distances[distances.size() / 2];
    }

    std::cout << "  reference " << tag
              << ": frame x=" << x0 << ".." << x1
              << " y=" << y0 << ".." << y1
              << " colourbar x=" << cbx0 << ".." << cbx1
              << " finite 72x80=" << out.finiteFine << "/"
              << kPhiBins * kZFineBins
              << " 36x41=" << out.finiteNative << "/"
              << kPhiNativeBins * kZNativeBins
              << " median colour distance=" << out.medianDist << " px\n";
    return out;
}

TH2D* readMap(TFile& f, const char* name, const std::string& cloneName)
{
    auto* h = dynamic_cast<TH2D*>(f.Get(name));
    if (!h) {
        std::cerr << "ERROR: " << name << " not found in " << f.GetName() << "\n";
        return nullptr;
    }
    auto* c = dynamic_cast<TH2D*>(h->Clone(cloneName.c_str()));
    c->SetDirectory(nullptr);
    return c;
}

DataMap readData(const std::string& rootPath, const std::string& tag,
                 const std::string& label, const std::string& cask,
                 const std::string& input)
{
    DataMap d;
    d.tag = tag;
    d.label = label;
    d.cask = cask;
    d.input = input;

    TFile f(rootPath.c_str(), "READ");
    if (f.IsZombie()) {
        std::cerr << "ERROR: cannot open " << rootPath << "\n";
        return d;
    }
    d.native = readMap(f, "hPhiZ", tag + "_dataNative");
    d.alt = readMap(f, "hPhiZ_alt", tag + "_dataAlt");

    // The reader display histogram is 360 x 400, i.e. 5 x 5 samples per
    // 72 x 80 reference cell.  Grouping 5 x 5 gives exactly the reference cell
    // edges without interpolation.  The dose-weighted map uses the same
    // grouping; it is optional so older ROOT files still validate.
    auto readFine = [&](const char* name, const std::string& outName) -> TH2D* {
        auto* src = dynamic_cast<TH2D*>(f.Get(name));
        if (!src) return nullptr;
        auto* raw = dynamic_cast<TH2D*>(src->Clone((outName + "Raw").c_str()));
        if (!raw) return nullptr;
        raw->SetDirectory(nullptr);

        const int fx = raw->GetNbinsX() / kPhiBins;
        const int fy = raw->GetNbinsY() / kZFineBins;
        if (fx < 1 || fy < 1 ||
            fx * kPhiBins != raw->GetNbinsX() ||
            fy * kZFineBins != raw->GetNbinsY()) {
            std::cerr << "ERROR: " << name << " in " << rootPath
                      << " has " << raw->GetNbinsX() << " x "
                      << raw->GetNbinsY()
                      << " bins; cannot group to " << kPhiBins << " x "
                      << kZFineBins << "\n";
            delete raw;
            return nullptr;
        }
        if (fx == 1 && fy == 1) return raw;
        auto* out = dynamic_cast<TH2D*>(
            raw->Rebin2D(fx, fy, outName.c_str()));
        if (out) out->SetDirectory(nullptr);
        delete raw;
        return out;
    };

    d.fine = readFine("hPhiZ_fine", tag + "_dataFine");
    d.fineDose = readFine("hPhiZ_dose_fine", tag + "_dataFineDose");
    if (d.fine)
        d.display = dynamic_cast<TH2D*>(
            d.fine->Clone((tag + "_dataDisplay").c_str()));
    if (d.display) d.display->SetDirectory(nullptr);
    if (d.fineDose)
        d.displayDose = dynamic_cast<TH2D*>(
            d.fineDose->Clone((tag + "_dataDisplayDose").c_str()));
    if (d.displayDose) d.displayDose->SetDirectory(nullptr);
    auto* e = dynamic_cast<TH1D*>(f.Get("hE"));
    if (e) {
        d.energy = dynamic_cast<TH1D*>(e->Clone((tag + "_dataEnergy").c_str()));
        d.energy->SetDirectory(nullptr);
    }
    if (auto* el = dynamic_cast<TH1D*>(f.Get("hE_linear"))) {
        d.energyLinear = dynamic_cast<TH1D*>(
            el->Clone((tag + "_dataEnergyLinear").c_str()));
        if (d.energyLinear) d.energyLinear->SetDirectory(nullptr);
    }
    if (auto* n = dynamic_cast<TNamed*>(f.Get("wssa_info")))
        d.info = n->GetTitle();

    if (!d.fine || !d.native || !d.alt || !d.display) {
        std::cerr << "ERROR: missing histograms in " << rootPath << "\n";
    }
    return d;
}

std::map<std::string, std::string> parseInfo(const std::string& info)
{
    std::map<std::string, std::string> m;
    std::stringstream ss(info);
    std::string item;
    while (std::getline(ss, item, ';')) {
        const std::size_t p = item.find('=');
        if (p == std::string::npos) continue;
        std::string key = item.substr(0, p);
        std::string val = item.substr(p + 1);
        while (!key.empty() && key.front() == ' ') key.erase(key.begin());
        while (!key.empty() && key.back() == ' ') key.pop_back();
        m[key] = val;
    }
    return m;
}

double infoDouble(const std::map<std::string, std::string>& m,
                  const std::string& key, double fallback = 0.0)
{
    const auto it = m.find(key);
    if (it == m.end()) return fallback;
    try {
        return std::stod(it->second);
    } catch (...) {
        return fallback;
    }
}

long long infoLong(const std::map<std::string, std::string>& m,
                   const std::string& key, long long fallback = 0)
{
    const auto it = m.find(key);
    if (it == m.end()) return fallback;
    try {
        return std::stoll(it->second);
    } catch (...) {
        return fallback;
    }
}

double corr(const std::vector<double>& a, const std::vector<double>& b)
{
    const std::size_t n = std::min(a.size(), b.size());
    if (n < 3) return 0.0;
    double sx = 0.0, sy = 0.0, sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sx += a[i];
        sy += b[i];
        sxx += a[i] * a[i];
        syy += b[i] * b[i];
        sxy += a[i] * b[i];
    }
    const double cov = n * sxy - sx * sy;
    const double vx = n * sxx - sx * sx;
    const double vy = n * syy - sy * sy;
    if (vx <= 0.0 || vy <= 0.0) return 0.0;
    return cov / std::sqrt(vx * vy);
}

std::vector<double> profileY(const TH2D* h, const std::vector<char>* valid)
{
    std::vector<double> out(h->GetNbinsY(), 0.0);
    for (int by = 1; by <= h->GetNbinsY(); ++by) {
        double sum = 0.0;
        int n = 0;
        for (int bx = 1; bx <= h->GetNbinsX(); ++bx) {
            const int gb = h->GetBin(bx, by);
            if (valid && !(*valid)[gb]) continue;
            sum += h->GetBinContent(gb);
            ++n;
        }
        out[by - 1] = (n > 0) ? sum / n : 0.0;
    }
    return out;
}

std::vector<double> profileX(const TH2D* h, const std::vector<char>* valid)
{
    std::vector<double> out(h->GetNbinsX(), 0.0);
    for (int bx = 1; bx <= h->GetNbinsX(); ++bx) {
        double sum = 0.0;
        int n = 0;
        for (int by = 1; by <= h->GetNbinsY(); ++by) {
            const int gb = h->GetBin(bx, by);
            if (valid && !(*valid)[gb]) continue;
            sum += h->GetBinContent(gb);
            ++n;
        }
        out[bx - 1] = (n > 0) ? sum / n : 0.0;
    }
    return out;
}

std::vector<double> normalise(std::vector<double> v)
{
    double maxValue = 0.0;
    for (double x : v) maxValue = std::max(maxValue, x);
    if (maxValue > 0.0)
        for (double& x : v) x /= maxValue;
    return v;
}

double peakPosition(const std::vector<double>& v, double xmin, double xmax)
{
    if (v.empty()) return 0.0;
    const auto it = std::max_element(v.begin(), v.end());
    const int i = static_cast<int>(it - v.begin());
    const double width = (xmax - xmin) / v.size();
    return xmin + (i + 0.5) * width;
}

double corrMaps(const TH2D* ref, const TH2D* data,
                const std::vector<char>& valid)
{
    std::vector<double> a;
    std::vector<double> b;
    for (int bx = 1; bx <= ref->GetNbinsX(); ++bx) {
        for (int by = 1; by <= ref->GetNbinsY(); ++by) {
            const int gb = ref->GetBin(bx, by);
            if (!valid[gb]) continue;
            a.push_back(ref->GetBinContent(gb));
            b.push_back(data->GetBinContent(gb));
        }
    }
    return corr(a, b);
}

TH2D* smoothMap(const TH2D* src, double sigma, const std::string& name)
{
    auto* out = dynamic_cast<TH2D*>(src->Clone(name.c_str()));
    out->Reset();
    const int nx = src->GetNbinsX();
    const int ny = src->GetNbinsY();
    const int radius = std::max(1, static_cast<int>(std::ceil(3.0 * sigma)));
    std::vector<double> kernel(2 * radius + 1, 0.0);
    double norm = 0.0;
    for (int i = -radius; i <= radius; ++i) {
        const double w = std::exp(-0.5 * (i / sigma) * (i / sigma));
        kernel[i + radius] = w;
        norm += w;
    }
    for (double& w : kernel) w /= norm;

    for (int by = 1; by <= ny; ++by) {
        for (int bx = 1; bx <= nx; ++bx) {
            double sum = 0.0;
            for (int j = -radius; j <= radius; ++j) {
                const int yy = std::min(ny, std::max(1, by + j));
                for (int i = -radius; i <= radius; ++i) {
                    const int xx = std::min(nx, std::max(1, bx + i));
                    sum += src->GetBinContent(xx, yy) *
                           kernel[i + radius] * kernel[j + radius];
                }
            }
            out->SetBinContent(bx, by, sum);
        }
    }
    return out;
}

// Diagnostic only: subtract a smooth component and correlate the residual.
// A high value means the two maps differ mainly by a smooth baseline/contrast,
// not by the position of the fine structure.
double corrMapsHighPass(const TH2D* ref, const TH2D* data, double sigma)
{
    auto* refSmooth = smoothMap(ref, sigma, "ref_highpass_smooth");
    auto* dataSmooth = smoothMap(data, sigma, "data_highpass_smooth");
    std::vector<double> a;
    std::vector<double> b;
    for (int bx = 1; bx <= ref->GetNbinsX(); ++bx) {
        for (int by = 1; by <= ref->GetNbinsY(); ++by) {
            const int gb = ref->GetBin(bx, by);
            a.push_back(ref->GetBinContent(gb) - refSmooth->GetBinContent(gb));
            b.push_back(data->GetBinContent(gb) - dataSmooth->GetBinContent(gb));
        }
    }
    const double c = corr(a, b);
    delete refSmooth;
    delete dataSmooth;
    return c;
}

Result analyse(const DataMap& data, const RefMap& ref)
{
    Result r;
    r.data = data;
    r.ref = ref;

    const auto meta = parseInfo(data.info);
    r.nrsw = infoLong(meta, "nrsw");
    r.processed = infoLong(meta, "processed");
    r.nSide = infoLong(meta, "nSide");
    r.nTop = infoLong(meta, "nTop");
    r.nUnknown = infoLong(meta, "nUnknown");
    r.sumW = infoDouble(meta, "sumW");
    r.sumDoseWeight = infoDouble(meta, "sumDoseWeight");
    r.npAbs = infoDouble(meta, "npAbs");
    if (r.npAbs <= 1.0) {
        // Older Cask 2/3 outputs predate the npAbs metadata entry.
        if (data.tag == "Cask2_gamma") r.npAbs = 1.0e11;
        else if (data.tag == "Cask3_gamma") r.npAbs = 5.0e11;
        else if (data.tag == "Cask3_neutrons") r.npAbs = 1.0e8;
    }
    if (r.npAbs <= 0.0) r.npAbs = 1.0;
    if (auto it = meta.find("doseParticle"); it != meta.end())
        r.doseParticle = it->second;
    r.minE = infoDouble(meta, "minE");
    r.maxE = infoDouble(meta, "maxE");
    r.minZ = infoDouble(meta, "minZ");
    r.maxZ = infoDouble(meta, "maxZ");

    r.corr2d = corrMaps(ref.fine, data.fine, ref.validFine);
    r.corr2dHighpass = corrMapsHighPass(ref.fine, data.fine, 2.0);

    const auto refZ = profileY(ref.fine, &ref.validFine);
    const auto dataZ = profileY(data.fine, nullptr);
    const auto refPhi = profileX(ref.fine, &ref.validFine);
    const auto dataPhi = profileX(data.fine, nullptr);
    const auto refPhi0 = profileX(ref.native, &ref.validNative);
    const auto altPhi = profileX(data.alt, nullptr);

    r.corrZ = corr(refZ, dataZ);
    r.corrPhi1 = corr(refPhi, dataPhi);
    r.corrPhi0 = corr(refPhi0, altPhi);
    r.zPeakImage = peakPosition(refZ, 0.0, kZMaxMM);
    r.zPeakData = peakPosition(dataZ, 0.0, kZMaxMM);
    r.phiPeakImage = peakPosition(refPhi, 0.0, 360.0);
    r.phiPeakData = peakPosition(dataPhi, 0.0, 360.0);
    r.phiPeakAlt = peakPosition(altPhi, 0.0, 360.0);

    if (data.fineDose) {
        r.corr2dDose = corrMaps(ref.fine, data.fineDose, ref.validFine);
        r.corr2dDoseHighpass = corrMapsHighPass(ref.fine, data.fineDose, 2.0);
        const auto dataZDose = profileY(data.fineDose, nullptr);
        const auto dataPhiDose = profileX(data.fineDose, nullptr);
        r.corrZDose = corr(refZ, dataZDose);
        r.corrPhiDose = corr(refPhi, dataPhiDose);
        r.zPeakDose = peakPosition(dataZDose, 0.0, kZMaxMM);
        r.phiPeakDose = peakPosition(dataPhiDose, 0.0, 360.0);
    }

    return r;
}

void styleMapPad()
{
    if (!gPad) return;
    gPad->SetLeftMargin(0.22);
    gPad->SetRightMargin(0.17);
    gPad->SetBottomMargin(0.15);
    gPad->SetTopMargin(0.10);
    // Paint bins with no entries in the colour of zero, not in white.
    gPad->SetFrameFillColor(TColor::GetColor("#00008B"));
}

void styleProfilePad()
{
    if (!gPad) return;
    gPad->SetLeftMargin(0.21);
    gPad->SetRightMargin(0.04);
    gPad->SetBottomMargin(0.18);
    gPad->SetTopMargin(0.10);
}

TH2D* displayClone(const TH2D* src, const std::string& name,
                   const std::string& title, bool reference = false,
                   double maxDose = 1.0, bool normaliseData = true,
                   const char* dataZTitle = "normalised sum of weights",
                   double unitScale = 1.0)
{
    auto* h = dynamic_cast<TH2D*>(src->Clone(name.c_str()));
    if (!reference && unitScale != 1.0) h->Scale(unitScale);
    const double maxValue = h->GetMaximum();
    if (reference) {
        h->Scale(maxDose);
        h->SetMinimum(0.0);
        h->SetMaximum(maxDose);
        h->GetZaxis()->SetRangeUser(0.0, maxDose);
        h->GetZaxis()->SetTitle("dose rate [#muSv/h]");
    } else if (normaliseData) {
        if (maxValue > 0.0) h->Scale(1.0 / maxValue);
        h->SetMinimum(0.0);
        h->SetMaximum(1.0);
        h->GetZaxis()->SetRangeUser(0.0, 1.0);
        h->GetZaxis()->SetTitle(dataZTitle);
    } else {
        // Keep H*(10)-weighted maps in their native units.  No maximum
        // scaling is applied to a dose-weighted WSSA histogram.
        const double top = (maxValue > 0.0) ? maxValue : 1.0;
        h->SetMinimum(0.0);
        h->SetMaximum(top);
        h->GetZaxis()->SetRangeUser(0.0, top);
        h->GetZaxis()->SetTitle(dataZTitle);
    }
    h->SetTitle(title.c_str());
    h->SetStats(kFALSE);
    h->GetXaxis()->SetTitle("#font[132]{#it{#varphi}} [deg]");
    h->GetYaxis()->SetTitle("#font[132]{#it{z}} [mm]");
    h->GetXaxis()->CenterTitle();
    h->GetYaxis()->CenterTitle();
    h->GetXaxis()->SetNdivisions(4);
    h->GetYaxis()->SetNdivisions(4);
    h->GetXaxis()->SetLabelOffset(0.012);
    h->GetYaxis()->SetLabelOffset(0.012);
    h->GetXaxis()->SetTitleOffset(1.1);
    h->GetYaxis()->SetTitleOffset(2.0);
    h->GetZaxis()->SetTitleOffset(1.2);
    return h;
}

void drawR(double value, const char* text = "r_{2D}")
{
    TPaveText* pt = new TPaveText(0.05, 0.79, 0.43, 0.87, "NDC");
    pt->SetFillColor(kWhite);
    pt->SetBorderSize(1);
    pt->SetTextSize(0.038);
    pt->SetTextFont(42);
    pt->AddText(Form("%s = %.3f", text, value));
    pt->Draw();
}

void drawMapPair(const Result& r, const std::string& pdf,
                 const std::string& png, bool doseWeighted = false)
{
    TCanvas c(("c_map_" + r.data.tag).c_str(), "", 3000, 1800);
    c.Divide(2, 1);

    c.cd(1);
    styleMapPad();
    auto* ref = displayClone(r.ref.display, r.data.tag + "_refDraw",
                                Form("reference (digitised), %s",
                                     r.data.label.c_str()),
                                true, r.ref.maxDose);
    ref->Draw("COLZ");

    c.cd(2);
    styleMapPad();
    const TH2D* source = doseWeighted ? r.data.displayDose : r.data.display;
    if (!source) return;
    auto* data = displayClone(
        source, r.data.tag + (doseWeighted ? "_dataDoseDraw" : "_dataDraw"),
        doseWeighted
            ? Form("#it{H}^{*}(10)-weighted, %s (r_{2D}=%.3f)",
                   r.data.label.c_str(), r.corr2dDose)
            : Form("full data, %s (r_{2D}=%.3f)",
                   r.data.label.c_str(), r.corr2d),
        false, 1.0, !doseWeighted,
        doseWeighted ? "dose rate [#muSv/h]"
                     : "normalised sum of weights",
        doseWeighted ? kDoseRateScale / r.npAbs : 1.0);
    data->Draw("COLZ");

    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void drawMapQuad(const Result& g, const Result& n,
                 const std::string& pdf, const std::string& png,
                 bool doseWeighted = false)
{
    TCanvas c("c_map_cask3", "", 3200, 2000);
    c.Divide(2, 2);

    c.cd(1);
    styleMapPad();
    auto* r1 = displayClone(g.ref.display, g.data.tag + "_q_ref",
                               Form("reference (digitised), %s",
                                    g.data.label.c_str()),
                               true, g.ref.maxDose);
    r1->Draw("COLZ");

    c.cd(2);
    styleMapPad();
    const TH2D* gSource = doseWeighted ? g.data.displayDose : g.data.display;
    if (!gSource) return;
    auto* r2 = displayClone(
        gSource, g.data.tag + (doseWeighted ? "_q_dataDose" : "_q_data"),
        doseWeighted
            ? Form("#it{H}^{*}(10)-weighted, %s (r_{2D}=%.3f)",
                   g.data.label.c_str(), g.corr2dDose)
            : Form("full data, %s (r_{2D}=%.3f)",
                   g.data.label.c_str(), g.corr2d),
        false, 1.0, !doseWeighted,
        doseWeighted ? "dose rate [#muSv/h]"
                     : "normalised sum of weights",
        doseWeighted ? kDoseRateScale / g.npAbs : 1.0);
    r2->Draw("COLZ");

    c.cd(3);
    styleMapPad();
    auto* r3 = displayClone(n.ref.display, n.data.tag + "_q_ref",
                               Form("reference (digitised), %s",
                                    n.data.label.c_str()),
                               true, n.ref.maxDose);
    r3->Draw("COLZ");

    c.cd(4);
    styleMapPad();
    const TH2D* nSource = doseWeighted ? n.data.displayDose : n.data.display;
    if (!nSource) return;
    auto* r4 = displayClone(
        nSource, n.data.tag + (doseWeighted ? "_q_dataDose" : "_q_data"),
        doseWeighted
            ? Form("#it{H}^{*}(10)-weighted, %s (r_{2D}=%.3f)",
                   n.data.label.c_str(), n.corr2dDose)
            : Form("full data, %s (r_{2D}=%.3f)",
                   n.data.label.c_str(), n.corr2d),
        false, 1.0, !doseWeighted,
        doseWeighted ? "dose rate [#muSv/h]"
                     : "normalised sum of weights",
        doseWeighted ? kDoseRateScale / n.npAbs : 1.0);
    r4->Draw("COLZ");

    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void drawMapGrid(const std::vector<Result>& results,
                 bool reference, const std::string& pdf,
                 const std::string& png, bool doseWeighted = false)
{
    // Two columns (gamma, neutrons) by three rows (Cask 1, 2, 3).
    // 1800 x 1500 gives each 900 x 500 cell roughly the 2:1 shape of the
    // 1600 x 800 original PNGs when all three figures use \linewidth.
    TCanvas c(reference ? "c_ref_grid" : "c_data_grid", "", 3600, 3000);
    c.Divide(2, 3);
    for (std::size_t i = 0; i < results.size(); ++i) {
        c.cd(static_cast<int>(i) + 1);
        styleMapPad();
        const Result& r = results[i];
        const TH2D* source = doseWeighted ? r.data.displayDose : r.data.display;
        if (!source) continue;
        const std::string name = r.data.tag +
            (reference ? "_refGrid" : (doseWeighted ? "_dataDoseGrid" : "_dataGrid"));
        auto* h = reference
            ? displayClone(r.ref.display, name,
                              Form("reference (digitised), %s",
                                   r.data.label.c_str()),
                              true, r.ref.maxDose)
            : displayClone(
                  source, name,
                  doseWeighted
                      ? Form("#it{H}^{*}(10)-weighted, %s (r_{2D}=%.3f)",
                             r.data.label.c_str(), r.corr2dDose)
                      : Form("full data, %s (r_{2D}=%.3f)",
                             r.data.label.c_str(), r.corr2d),
                  false, 1.0, !doseWeighted,
                  doseWeighted ? "dose rate [#muSv/h]"
                               : "normalised sum of weights",
                  doseWeighted ? kDoseRateScale / r.npAbs : 1.0);
        h->Draw("COLZ");
    }
    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

TGraph* makeGraph(const std::vector<double>& y, double xmin, double xmax,
                  int color, int marker)
{
    auto* g = new TGraph(static_cast<int>(y.size()));
    const double width = (xmax - xmin) / y.size();
    for (std::size_t i = 0; i < y.size(); ++i) {
        g->SetPoint(static_cast<int>(i), xmin + (i + 0.5) * width, y[i]);
    }
    g->SetLineColor(color);
    g->SetMarkerColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(0.8);
    g->SetLineWidth(2);
    return g;
}

void drawProfiles(const std::vector<Result>& results, bool phi,
                  const std::string& pdf, const std::string& png,
                  bool doseWeighted = false)
{
    TCanvas c(phi ? "c_phi_profiles" : "c_z_profiles", "", 1800, 1180);
    c.Divide(3, 2);
    // Columns are Cask 1, 2, 3; top row gamma and bottom row neutrons.
    // The results vector is ordered by cask, then particle, so reorder the
    // pads to keep the casks together across the width.
    const std::size_t pads[6] = {0, 2, 4, 1, 3, 5};
    for (std::size_t i = 0; i < 6 && i < results.size(); ++i) {
        c.cd(static_cast<int>(i) + 1);
        styleProfilePad();
        const Result& r = results[pads[i]];
        const TH2D* dataFine = doseWeighted ? r.data.fineDose : r.data.fine;
        if (!dataFine) continue;

        const int nBins = phi ? kPhiBins : kZFineBins;
        const double xmin = 0.0;
        const double xmax = phi ? 360.0 : kZMaxMM;
        const double profileCorr = doseWeighted
            ? (phi ? r.corrPhiDose : r.corrZDose)
            : (phi ? r.corrPhi1 : r.corrZ);
        auto* frame = new TH1D(Form("frame_%s_%zu_%s", phi ? "phi" : "z", i,
                                    doseWeighted ? "dose" : "part"),
                               Form("%s%s (#font[132]{#it{r}}_{%s}=%.3f);%s;norm.",
                                    doseWeighted ? "#it{H}^{*}(10), " : "",
                                    r.data.label.c_str(),
                                    phi ? "#varphi" : "z",
                                    profileCorr,
                                    phi ? "#font[132]{#it{#varphi}} [deg]"
                                        : "#font[132]{#it{z}} [mm]"),
                               nBins, xmin, xmax);
        frame->SetMinimum(0.0);
        frame->SetMaximum(1.08);
        frame->SetStats(kFALSE);
        frame->GetYaxis()->CenterTitle();
        frame->GetXaxis()->CenterTitle();
        frame->GetXaxis()->SetNdivisions(4);
        frame->GetYaxis()->SetTitleOffset(1.3);
        frame->GetXaxis()->SetTitleOffset(1.1);
        frame->SetMaximum(1.08);
        frame->Draw("HIST");

        const auto ref = phi
            ? normalise(profileX(r.ref.fine, &r.ref.validFine))
            : normalise(profileY(r.ref.fine, &r.ref.validFine));
        const auto dat = phi
            ? normalise(profileX(dataFine, nullptr))
            : normalise(profileY(dataFine, nullptr));

        auto* gRef = makeGraph(ref, xmin, xmax, kGray + 2, 20);
        auto* gDat = makeGraph(dat, xmin, xmax, kAzure + 2, 21);
        gRef->Draw("L SAME");
        gDat->Draw("P SAME");

        if (phi && !doseWeighted) {
            const auto alt = normalise(profileX(r.data.alt, nullptr));
            auto* gAlt = makeGraph(alt, xmin, xmax, kRed + 1, 22);
            gAlt->Draw("L SAME");
        }

        if (phi) {
            frame->GetXaxis()->SetNdivisions(505);
        }
    }
    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void drawScores(const std::vector<Result>& results, const std::string& pdf,
                const std::string& png)
{
    TCanvas c("c_scores", "", 1700, 720);
    c.Divide(2, 1);

    const int n = static_cast<int>(results.size());
    c.cd(1);
    styleProfilePad();
    auto* f1 = new TH1D("score_frame", "mode 1: reference vs full data;file;Pearson correlation",
                        n, 0.0, static_cast<double>(n));
    f1->SetMinimum(0.0);
    f1->SetMaximum(1.10);
    f1->SetStats(kFALSE);
    for (int i = 0; i < n; ++i)
        f1->GetXaxis()->SetBinLabel(i + 1, results[i].data.label.c_str());
    f1->LabelsOption("h", "X");
    f1->Draw("AXIS");

    auto* g2 = new TGraph(n);
    auto* g2d = new TGraph(n);
    auto* gp = new TGraph(n);
    auto* gz = new TGraph(n);
    for (int i = 0; i < n; ++i) {
        const double x = i + 0.5;
        // Small horizontal offsets keep the four nearly equal scores visible.
        g2->SetPoint(i, x - 0.15, results[i].corr2d);
        g2d->SetPoint(i, x - 0.05, results[i].corr2dDose);
        gp->SetPoint(i, x + 0.05, results[i].corrPhi1);
        gz->SetPoint(i, x + 0.15, results[i].corrZ);
    }
    const int colors[4] = {kAzure + 2, kMagenta + 1, kRed + 1, kGreen + 2};
    const int markers[4] = {20, 20, 21, 22};
    TGraph* graphs[4] = {g2, g2d, gp, gz};
    const char* names[4] = {"2D particle/s", "2D #it{H}^{*}(10)",
                            "#varphi profile", "z profile"};
    for (int k = 0; k < 4; ++k) {
        graphs[k]->SetMarkerColor(colors[k]);
        graphs[k]->SetLineColor(colors[k]);
        graphs[k]->SetMarkerStyle(markers[k]);
        graphs[k]->SetMarkerSize(1.2);
        graphs[k]->Draw("P SAME");
    }
    auto* one = new TLine(0.0, 1.0, static_cast<double>(n), 1.0);
    one->SetLineStyle(2);
    one->SetLineColor(kGray + 1);
    one->Draw("SAME");
    TLegend* l1 = new TLegend(0.38, 0.45, 0.92, 0.68);
    l1->SetBorderSize(0);
    l1->SetFillColor(kWhite);
    l1->SetFillStyle(0);
    l1->SetTextSize(0.032);
    for (int k = 0; k < 4; ++k) l1->AddEntry(graphs[k], names[k], "p");
    l1->Draw();
    auto* pa = new TLatex(0.025, 0.94, "(a)");
    pa->SetNDC();
    pa->SetTextFont(62);
    pa->SetTextSize(0.055);
    pa->Draw();

    c.cd(2);
    styleProfilePad();
    auto* f2 = new TH1D("orient_frame", "orientation comparison;file;#varphi-profile correlation",
                        n, 0.0, static_cast<double>(n));
    f2->SetMinimum(-1.05);
    f2->SetMaximum(1.15);
    f2->SetStats(kFALSE);
    for (int i = 0; i < n; ++i)
        f2->GetXaxis()->SetBinLabel(i + 1, results[i].data.label.c_str());
    f2->LabelsOption("h", "X");
    f2->Draw("AXIS");

    auto* gm1 = new TGraph(n);
    auto* gm0 = new TGraph(n);
    for (int i = 0; i < n; ++i) {
        const double x = i + 0.5;
        gm1->SetPoint(i, x - 0.10, results[i].corrPhi1);
        gm0->SetPoint(i, x + 0.10, results[i].corrPhi0);
    }
    gm1->SetMarkerColor(kRed + 1);
    gm1->SetLineColor(kRed + 1);
    gm1->SetMarkerStyle(21);
    gm1->SetMarkerSize(1.2);
    gm0->SetMarkerColor(kAzure + 2);
    gm0->SetLineColor(kAzure + 2);
    gm0->SetMarkerStyle(22);
    gm0->SetMarkerSize(1.2);
    gm1->Draw("P SAME");
    gm0->Draw("P SAME");
    auto* zero = new TLine(0.0, 0.0, static_cast<double>(n), 0.0);
    zero->SetLineColor(kBlack);
    zero->SetLineWidth(1);
    zero->Draw("SAME");
    TLegend* l2 = new TLegend(0.23, 0.25, 0.47, 0.45);
    l2->SetBorderSize(0);
    l2->SetFillColor(kWhite);
    l2->SetFillStyle(1001);
    l2->SetTextSize(0.035);
    l2->AddEntry(gm1, "mode 1", "p");
    l2->AddEntry(gm0, "mode 0", "p");
    l2->Draw();
    auto* pb = new TLatex(0.025, 0.94, "(b)");
    pb->SetNDC();
    pb->SetTextFont(62);
    pb->SetTextSize(0.055);
    pb->Draw();

    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void drawEnergy(const DataMap& d, const std::string& pdf,
                const std::string& png)
{
    if (!d.energy) return;
    TCanvas c(("c_e_" + d.tag).c_str(), "", 1200, 800);
    c.SetLogx();
    c.SetLogy();
    c.SetLeftMargin(0.13);
    c.SetBottomMargin(0.15);
    auto* h = dynamic_cast<TH1D*>(d.energy->Clone((d.tag + "_eDraw").c_str()));
    h->SetStats(kFALSE);
    h->SetLineWidth(2);
    h->SetTitle(Form("%s;energy [MeV];sum of weights", d.label.c_str()));
    h->GetXaxis()->SetTitleOffset(1.2);
    h->Draw("HIST");
    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void drawEnergyAll(const std::vector<Result>& results,
                   const std::string& pdf, const std::string& png)
{
    // Three casks in columns; gamma (linear x) on the top row, neutrons
    // (logarithmic x) on the bottom row.
    TCanvas c("c_energy_all", "", 2100, 1200);
    c.Divide(3, 2, 0.002, 0.002);
    const int gammaIdx[3] = {0, 2, 4};
    const int neutronIdx[3] = {1, 3, 5};
    const char* panel[6] = {"(a)", "(b)", "(c)", "(d)", "(e)", "(f)"};

    // A common ordinate range per row makes the three casks directly
    // comparable; autoscaling each pad would hide the factor-of-ten difference
    // between Cask 1 and Cask 2 neutron spectra.
    double gammaMax = 0.0, neutronMax = 0.0;
    for (int col = 0; col < 3; ++col) {
        const TH1D* g = results[gammaIdx[col]].data.energyLinear;
        const TH1D* n = results[neutronIdx[col]].data.energy;
        if (g) gammaMax = std::max(gammaMax, g->GetMaximum());
        if (n) neutronMax = std::max(neutronMax, n->GetMaximum());
    }
    if (gammaMax <= 0.0) gammaMax = 1.0;
    if (neutronMax <= 0.0) neutronMax = 1.0;
    const double gammaMin = gammaMax * 1e-8;
    const double neutronMin = neutronMax * 1e-8;

    for (int col = 0; col < 3; ++col) {
        for (int row = 0; row < 2; ++row) {
            const int pad = row * 3 + col + 1;
            const int idx = (row == 0) ? gammaIdx[col] : neutronIdx[col];
            c.cd(pad);
            gPad->SetLeftMargin(0.15);
            gPad->SetRightMargin(0.035);
            gPad->SetBottomMargin(0.21);
            gPad->SetTopMargin(0.10);
            gPad->SetLogy(1);

            const bool gamma = (row == 0);
            const DataMap& d = results[idx].data;
            const TH1D* src = gamma ? d.energyLinear : d.energy;
            if (!src) src = d.energy;
            if (!src) continue;
            auto* h = dynamic_cast<TH1D*>(
                src->Clone(Form("eall_%d", pad)));
            h->SetDirectory(nullptr);
            h->SetStats(kFALSE);
            h->SetLineColor(kAzure + 2);
            h->SetLineWidth(2);
            h->SetMinimum(gamma ? gammaMin : neutronMin);
            h->SetMaximum((gamma ? gammaMax : neutronMax) * 1.6);
            if (gamma) {
                gPad->SetLogx(0);
                h->GetXaxis()->SetRangeUser(0.0, 4.0);
                h->SetTitle(Form("Cask %d #gamma;energy [MeV];sum of weights / 20 keV",
                                 col + 1));
            } else {
                gPad->SetLogx(1);
                h->GetXaxis()->SetRangeUser(1e-10, 30.0);
                h->SetTitle(Form("Cask %d n;energy [MeV];sum of weights / log bin",
                                 col + 1));
            }
            h->GetXaxis()->CenterTitle();
            h->GetYaxis()->CenterTitle();
            h->GetXaxis()->SetTitleOffset(1.15);
            h->GetYaxis()->SetTitleOffset(1.30);
            h->GetXaxis()->SetTitleSize(0.042);
            h->GetYaxis()->SetTitleSize(0.036);
            h->GetXaxis()->SetLabelSize(gamma ? 0.036 : 0.030);
            h->GetYaxis()->SetLabelSize(0.034);
            h->Draw("HIST");

            auto* lbl = new TLatex(0.045, 0.93, panel[pad - 1]);
            lbl->SetNDC();
            lbl->SetTextFont(62);
            lbl->SetTextSize(0.052);
            lbl->Draw();
        }
    }
    c.SaveAs(pdf.c_str());
    c.SaveAs(png.c_str());
}

void writeText(const std::vector<Result>& results,
               const std::string& reportDir)
{
    gSystem->mkdir((reportDir + "/data").c_str(), kTRUE);
    std::ofstream csv((reportDir + "/data/full_validation_summary.csv").c_str());
    csv << "tag,nrsw,processed,sumW,side,top,unknown,minE,maxE,minZ,maxZ,"
           "corr2D,corr2D_hp2,corrPhi1,corrPhi0,corrZ,zpeak_image,zpeak_data,"
           "phpeak_image,phpeak_data,phpeak_alt,finite72x80,finite36x41,"
           "ref_max_uSv_h,median_colour_dist,sumDoseWeight,corr2D_dose,"
           "corr2D_dose_hp2,corrPhi1_dose,corrZ_dose,zpeak_dose,phpeak_dose,"
           "doseParticle\n";
    csv << std::setprecision(8);

    for (const Result& r : results) {
        const std::string outPath =
            reportDir + "/data/" + r.data.tag + "_all_validation.txt";
        std::ofstream out(outPath.c_str());
        out << "input=" << r.data.input << "\n";
        out << "nrsw=" << r.nrsw << " processed=" << r.processed
            << " sumW=" << std::setprecision(12) << r.sumW << "\n";
        out << "side=" << r.nSide << " top=" << r.nTop
            << " unknown=" << r.nUnknown << "\n";
        out << "energy[MeV]=" << r.minE << ".." << r.maxE
            << " z[mm]=" << r.minZ << ".." << r.maxZ << "\n";
        out << "reference finite 72x80=" << r.ref.finiteFine << "/"
            << kPhiBins * kZFineBins
            << " 36x41=" << r.ref.finiteNative << "/"
            << kPhiNativeBins * kZNativeBins
            << " median_colour_distance=" << r.ref.medianDist << "\n";
        out << "reference colourbar max [uSv/h]=" << r.ref.maxDose << "\n";
        out << "H*(10) weight sum=" << std::setprecision(12) << r.sumDoseWeight
            << " particle=" << r.doseParticle << "\n";
        out << std::setprecision(6);
        out << "2D corr mode1 particle/s (72x80): " << r.corr2d << "\n";
        out << "2D corr mode1 H*(10) (72x80): " << r.corr2dDose << "\n";
        out << "2D high-pass corr particle/s (sigma=2 bins): "
            << r.corr2dHighpass << "\n";
        out << "2D high-pass corr H*(10) (sigma=2 bins): "
            << r.corr2dDoseHighpass << "\n";
        out << "phi profile corr mode1 particle/s (72x80): "
            << r.corrPhi1 << "\n";
        out << "phi profile corr mode1 H*(10) (72x80): "
            << r.corrPhiDose << "\n";
        out << "phi profile corr mode0 (36x41): " << r.corrPhi0 << "\n";
        out << "z profile corr particle/s: " << r.corrZ << "\n";
        out << "z profile corr H*(10): " << r.corrZDose << "\n";
        out << "z peak: image=" << r.zPeakImage << " mm, particle/s="
            << r.zPeakData << " mm, H*(10)=" << r.zPeakDose << " mm\n";
        out << "phi peak: image=" << r.phiPeakImage
            << " deg, particle/s mode1=" << r.phiPeakData
            << ", H*(10)=" << r.phiPeakDose
            << ", mode0=" << r.phiPeakAlt << "\n";

        const auto refZ = normalise(profileY(r.ref.fine, &r.ref.validFine));
        const auto datZ = normalise(profileY(r.data.fine, nullptr));
        const auto refPhi1 = normalise(profileX(r.ref.fine, &r.ref.validFine));
        const auto datPhi1 = normalise(profileX(r.data.fine, nullptr));
        const auto refPhi0 = normalise(profileX(r.ref.native, &r.ref.validNative));
        const auto altPhi = normalise(profileX(r.data.alt, nullptr));

        out << "\nz profile: z[mm] image ours\n";
        const double zWidth = kZMaxMM / refZ.size();
        for (std::size_t i = 0; i < refZ.size(); ++i) {
            out << std::fixed << std::setprecision(1)
                << (i + 0.5) * zWidth << " "
                << std::setprecision(4) << refZ[i] << " " << datZ[i] << "\n";
        }
        out << "\nphi profile mode1: phi[deg] image ours\n";
        const double phi1Width = 360.0 / refPhi1.size();
        for (std::size_t i = 0; i < refPhi1.size(); ++i) {
            out << std::fixed << std::setprecision(1)
                << (i + 0.5) * phi1Width << " "
                << std::setprecision(4) << refPhi1[i] << " "
                << datPhi1[i] << "\n";
        }
        out << "\nphi profile mode0: phi[deg] image ours\n";
        const double phi0Width = 360.0 / refPhi0.size();
        for (std::size_t i = 0; i < refPhi0.size(); ++i) {
            out << std::fixed << std::setprecision(1)
                << (i + 0.5) * phi0Width << " "
                << std::setprecision(4) << refPhi0[i] << " "
                << altPhi[i] << "\n";
        }

        csv << r.data.tag << "," << r.nrsw << "," << r.processed << ","
            << r.sumW << "," << r.nSide << "," << r.nTop << ","
            << r.nUnknown << "," << r.minE << "," << r.maxE << ","
            << r.minZ << "," << r.maxZ << "," << r.corr2d << ","
            << r.corr2dHighpass << "," << r.corrPhi1 << "," << r.corrPhi0 << "," << r.corrZ << ","
            << r.zPeakImage << "," << r.zPeakData << ","
            << r.phiPeakImage << "," << r.phiPeakData << ","
            << r.phiPeakAlt << "," << r.ref.finiteFine << ","
            << r.ref.finiteNative << "," << r.ref.maxDose << ","
            << r.ref.medianDist << "," << r.sumDoseWeight << ","
            << r.corr2dDose << "," << r.corr2dDoseHighpass << ","
            << r.corrPhiDose << "," << r.corrZDose << ","
            << r.zPeakDose << "," << r.phiPeakDose << ","
            << r.doseParticle << "\n";
    }
}

} // namespace

void validate_wssa_root(const char* outDir = "scripts/wssa/output_all",
                        const char* reportDir = "report/wti-binary-reader")
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gStyle->SetPalette(kRainBow);
    gStyle->SetNumberContours(99);
    gStyle->SetTitleFontSize(0.045);

    const std::string dataRoot = "/home/turko46/data/wti";
    const std::string out(outDir);
    const std::string report(reportDir);

    struct Tag {
        const char* tag;
        const char* label;
        const char* cask;
    };
    const std::vector<Tag> tags = {
        {"Cask1_gamma",    "C1 #gamma", "Cask1"},
        {"Cask1_neutrons", "C1 n", "Cask1"},
        {"Cask2_gamma",    "C2 #gamma", "Cask2"},
        {"Cask2_neutrons", "C2 n", "Cask2"},
        {"Cask3_gamma",    "C3 #gamma", "Cask3"},
        {"Cask3_neutrons", "C3 n", "Cask3"},
    };

    std::vector<Result> results;
    results.reserve(tags.size());

    for (const auto& t : tags) {
        const std::string tag(t.tag);
        const std::string rootPath = out + "/" + tag + "_all.root";
        const std::string pngPath = dataRoot + "/" + t.cask + "/" +
                                    tag + ".png";
        std::cout << "=== " << tag << " ===\n";
        DataMap data = readData(rootPath, tag, t.label, t.cask,
                                dataRoot + "/" + t.cask + "/" + tag + ".wssa");
        RefMap ref = digitizeReference(pngPath, tag);
        if (!data.fine || !data.native || !data.alt || !data.display ||
            !ref.fine || !ref.native || !ref.display) {
            std::cerr << "ERROR: incomplete inputs for " << tag << "\n";
            continue;
        }
        if (!data.fineDose)
            std::cerr << "WARNING: hPhiZ_dose_fine missing for " << tag
                      << "; re-run the WSSA reader for the H*(10) map.\n";
        Result r = analyse(data, ref);
        std::cout << std::fixed << std::setprecision(4)
                  << "  r2D=" << r.corr2d
                  << " r2Ddose=" << r.corr2dDose
                  << " rPhi1=" << r.corrPhi1
                  << " rPhi1dose=" << r.corrPhiDose
                  << " rPhi0=" << r.corrPhi0
                  << " rZ=" << r.corrZ
                  << " rZdose=" << r.corrZDose
                  << " zpeak=" << r.zPeakImage << "/" << r.zPeakData
                  << " phipeak=" << r.phiPeakImage << "/"
                  << r.phiPeakData << "/" << r.phiPeakAlt << "\n";
        results.push_back(r);
    }

    if (results.empty()) {
        std::cerr << "ERROR: no complete results\n";
        return;
    }

    gSystem->mkdir((report + "/figures").c_str(), kTRUE);
    gSystem->mkdir((report + "/data").c_str(), kTRUE);

    for (const Result& r : results) {
        const std::string src = dataRoot + "/" + r.data.cask + "/" +
                                r.data.tag + ".png";
        const std::string dst = report + "/figures/orig_" +
                                r.data.tag + ".png";
        gSystem->CopyFile(src.c_str(), dst.c_str(), kTRUE);
    }

    drawMapGrid(results, true, report + "/figures/fig_reference_maps_all.pdf",
                report + "/figures/fig_reference_maps_all.png");
    drawMapGrid(results, false, report + "/figures/fig_data_maps_all.pdf",
                report + "/figures/fig_data_maps_all.png");
    drawMapGrid(results, false,
                report + "/figures/fig_data_maps_all_dose.pdf",
                report + "/figures/fig_data_maps_all_dose.png", true);
    drawMapQuad(results[4], results[5],
                report + "/figures/fig_maps_Cask3.pdf",
                report + "/figures/fig_maps_Cask3.png");
    drawMapQuad(results[4], results[5],
                report + "/figures/fig_maps_Cask3_dose.pdf",
                report + "/figures/fig_maps_Cask3_dose.png", true);
    drawProfiles(results, false,
                 report + "/figures/fig_validation_z_profiles_all.pdf",
                 report + "/figures/fig_validation_z_profiles_all.png");
    drawProfiles(results, true,
                 report + "/figures/fig_validation_phi_profiles_all.pdf",
                 report + "/figures/fig_validation_phi_profiles_all.png");
    drawProfiles(results, false,
                 report + "/figures/fig_validation_z_profiles_dose_all.pdf",
                 report + "/figures/fig_validation_z_profiles_dose_all.png", true);
    drawProfiles(results, true,
                 report + "/figures/fig_validation_phi_profiles_dose_all.pdf",
                 report + "/figures/fig_validation_phi_profiles_dose_all.png", true);
    drawScores(results, report + "/figures/fig_validation_scores.pdf",
               report + "/figures/fig_validation_scores.png");
    drawEnergyAll(results, report + "/figures/fig_energy_all.pdf",
                  report + "/figures/fig_energy_all.png");

    writeText(results, report);
    std::cout << "Wrote ROOT figures and validation tables under "
              << report << "\n";
}
