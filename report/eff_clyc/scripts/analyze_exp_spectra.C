// Independent peak-integral validation and resolution extraction for the
// CLYC2 distance-variation spectra (2026-10-27).
//
// The CNF files are converted to ROOT with ~/programs/cnf2root/build/cnf2root
// (see convert_exp_spectra.sh).  This macro reads the converted ROOT files,
// subtracts Hintergrund.CNF bin by bin, fits each full-energy peak with a
// Gaussian plus a linear background, cross-checks the area with a local
// sideband integration, and compares the result with the CLYC2 rows of
// effDist_clyc1+2.csv.
//
// The 1173.23 keV Co-60 peak sits on the Compton continuum of the 1332.51 keV
// line.  Its area depends on the background model; a simultaneous two-peak fit
// is therefore also stored as a systematic cross-check.  The 661.64 and
// 1332.51 keV peaks are stable to about 2%.
//
// Resolution: R(E) = FWHM/E is extracted from the corrected peak centroids.
// The model R(E) = b/sqrt(E) is fitted to the two clean lines (661.64 and
// 1332.51 keV); the 1173.23 keV width is kept as a cross-check because its
// background model biases the fitted width low.
//
// Run from the repository root:
//   root -l -b -q report/eff_clyc/scripts/analyze_exp_spectra.C
//
// Optional arguments:
//   expDir     directory with the converted ROOT files
//   csvPath    effDist_clyc1+2.csv
//   outDataDir output directory for the CSV summaries
//
// Outputs:
//   report/eff_clyc/data/exp_peak_integrals.csv
//   report/eff_clyc/data/exp_resolution.csv
//   report/eff_clyc/data/exp_resolution_model.csv

#include <TFile.h>
#include <TH1D.h>
#include <TF1.h>
#include <TFitResult.h>
#include <TMatrixDSym.h>
#include <TTree.h>
#include <TMatrixD.h>
#include <TMath.h>
#include <TString.h>
#include <TSystem.h>
#include <TCanvas.h>
#include <TGraphErrors.h>
#include <TStyle.h>
#include <TLegend.h>
#include <TLine.h>
#include <TLatex.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kSqrtTwoPi = 2.5066282746310002;

std::string Trim(const std::string& s)
{
    const char* ws = " \t\r\n";
    const std::size_t first = s.find_first_not_of(ws);
    if (first == std::string::npos) return "";
    const std::size_t last = s.find_last_not_of(ws);
    return s.substr(first, last - first + 1);
}

std::vector<std::string> Split(const std::string& line)
{
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string token;
    while (std::getline(ss, token, ',')) out.push_back(Trim(token));
    return out;
}

double ToDouble(const std::string& s)
{
    return s.empty() ? 0. : std::atof(s.c_str());
}

int ToInt(const std::string& s)
{
    return s.empty() ? 0 : std::atoi(s.c_str());
}


struct CsvRow {
    std::string detector;
    int distance = 0;
    std::string nuclide;
    double activity = 0.;       // reference activity, Bq
    int refYear = 0, refMonth = 0, refDay = 0;
    double currentActivity = 0.; // rounded CSV current activity, Bq
    double halfLife = 0.;        // s
    double energy = 0.;          // keV
    double pGamma = 0.;          // percent
    double time = 0.;            // s
    double netPeak = 0.;
    double effPrinted = 0.;      // CSV "Efficiency (%)" column
    double effReference = 0.;    // CSV reference-value column
};
// Days since 1970-01-01 for a proleptic Gregorian date.  TDatime in ROOT
// 6.36 rejects years before 1995, and the source reference dates are 1973
// and 1985, so the Unix time is computed directly.
long DaysFromCivil(int y, int m, int d)
{
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u +
                         static_cast<unsigned>(d) - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return static_cast<long>(era) * 146097L + static_cast<long>(doe) - 719468L;
}

double RefUnix(const CsvRow& r)
{
    return 86400.0 * static_cast<double>(DaysFromCivil(r.refYear, r.refMonth,
                                                       r.refDay));
}



std::vector<CsvRow> ReadCsv(const char* path)
{
    std::vector<CsvRow> rows;
    std::ifstream in(path);
    if (!in.is_open()) {
        std::printf("ERROR: cannot open CSV %s\n", path);
        return rows;
    }
    std::string line;
    std::getline(in, line); // header
    while (std::getline(in, line)) {
        if (Trim(line).empty()) continue;
        const std::vector<std::string> f = Split(line);
        if (f.size() < 13) continue;
        CsvRow r;
        r.detector = f[0];
        r.distance = ToInt(f[1]);
        r.nuclide = f[2];
        r.activity = ToDouble(f[3]);
        std::sscanf(f[4].c_str(), "%d.%d.%d", &r.refDay, &r.refMonth, &r.refYear);
        r.currentActivity = ToDouble(f[5]);
        r.halfLife = ToDouble(f[6]);
        r.energy = ToDouble(f[7]);
        r.pGamma = ToDouble(f[8]);
        r.time = ToDouble(f[9]);
        r.netPeak = ToDouble(f[10]);
        r.effPrinted = ToDouble(f[11]);
        r.effReference = ToDouble(f[12]);
        rows.push_back(r);
    }
    return rows;
}

const CsvRow* FindRow(const std::vector<CsvRow>& rows, int distance,
                      const std::string& nuclide, double energy)
{
    for (const auto& r : rows) {
        if (r.detector == "CLYC2" && r.distance == distance &&
            r.nuclide == nuclide && std::fabs(r.energy - energy) < 0.01) {
            return &r;
        }
    }
    return nullptr;
}

struct Spectrum {
    TH1D* counts = nullptr;
    TH1D* countsNet = nullptr;
    double A0 = 0., A1 = 1.;
    double live = 0., real = 0.;
    ULong64_t startUnix = 0;
    ULong64_t total = 0;
};

bool ReadSpectrum(const char* path, Spectrum& s, bool makeNet,
                  const char* backgroundPath)
{
    TFile f(path);
    if (f.IsZombie()) {
        std::printf("ERROR: cannot open %s\n", path);
        return false;
    }
    auto* h = static_cast<TH1D*>(f.Get("counts_ch"));
    auto* t = static_cast<TTree*>(f.Get("meta"));
    if (!h || !t) {
        std::printf("ERROR: missing counts_ch/meta in %s\n", path);
        return false;
    }
    double A[4] = {0., 0., 0., 0.};
    t->SetBranchAddress("A", A);
    t->SetBranchAddress("live_time_s", &s.live);
    t->SetBranchAddress("real_time_s", &s.real);
    t->SetBranchAddress("start_unix", &s.startUnix);
    t->SetBranchAddress("total_counts", &s.total);
    t->GetEntry(0);
    s.A0 = A[0];
    s.A1 = A[1];
    s.counts = static_cast<TH1D*>(h->Clone(Form("%s_raw", h->GetName())));
    s.counts->SetDirectory(nullptr);

    if (!makeNet) return true;

    TFile fb(backgroundPath);
    if (fb.IsZombie()) {
        std::printf("ERROR: cannot open background %s\n", backgroundPath);
        return false;
    }
    auto* hb = static_cast<TH1D*>(fb.Get("counts_ch"));
    if (!hb) {
        std::printf("ERROR: missing counts_ch in background %s\n", backgroundPath);
        return false;
    }
    s.countsNet = static_cast<TH1D*>(s.counts->Clone(Form("%s_net", h->GetName())));
    s.countsNet->SetDirectory(nullptr);
    s.countsNet->Sumw2();
    const int n = s.counts->GetNbinsX();
    for (int b = 1; b <= n; ++b) {
        const double nm = s.counts->GetBinContent(b);
        const double nb = hb->GetBinContent(b);
        s.countsNet->SetBinContent(b, nm - nb);
        s.countsNet->SetBinError(b, std::sqrt(nm + nb));
    }
    return true;
}

struct PeakFit {
    bool ok = false;
    double meanRaw = 0.;
    double fwhmRaw = 0.;
    double fwhmRawErr = 0.;
    double area = 0.;
    double areaErr = 0.;
    double chi2 = 0.;
    int ndf = 0;
    double fitLo = 0.;
    double fitHi = 0.;
};

// Gaussian plus linear background over +/- windowFwhm * FWHM around the
// observed maximum.  The histogram x axis is channel number; one channel is
// one bin.
PeakFit FitPeak(TH1D* h, double eNom, double a0, double a1,
                double windowFwhm = 1.5)
{
    PeakFit r;
    const int n = h->GetNbinsX();
    int bmax = 0;
    double cmax = -1.;
    for (int b = 1; b <= n; ++b) {
        const double e = a0 + a1 * h->GetBinCenter(b);
        if (std::fabs(e - eNom) < 0.06 * eNom && h->GetBinContent(b) > cmax) {
            cmax = h->GetBinContent(b);
            bmax = b;
        }
    }
    if (bmax == 0 || cmax <= 0.) return r;

    const double half = 0.5 * cmax;
    int bl = bmax;
    int bh = bmax;
    while (bl > 1 && h->GetBinContent(bl) > half) --bl;
    while (bh < n && h->GetBinContent(bh) > half) ++bh;
    double fwhmCh = static_cast<double>(bh - bl);
    if (fwhmCh < 2.) fwhmCh = 0.05 * eNom / std::fabs(a1);

    const double mu0 = h->GetBinCenter(bmax);
    const double sig0 = fwhmCh / 2.354820045;
    double lo = bmax - windowFwhm * fwhmCh;
    double hi = bmax + windowFwhm * fwhmCh;
    if (lo < 1.) lo = 1.;
    if (hi > n) hi = n;

    TF1 f("fpeak", "gaus(0)+pol1(3)", lo, hi);
    f.SetParameters(cmax, mu0, sig0, 0., 0.);
    f.SetParLimits(1, mu0 - 0.03 * eNom / std::fabs(a1),
                   mu0 + 0.03 * eNom / std::fabs(a1));
    f.SetParLimits(2, 0.4 * sig0, 2.5 * sig0);
    TFitResultPtr fr = h->Fit(&f, "RQN0S", "", lo, hi);

    // One refit with the measured width.
    const double sig1 = std::fabs(f.GetParameter(2));
    if (sig1 > 0.) {
        lo = f.GetParameter(1) - windowFwhm * 2.354820045 * sig1;
        hi = f.GetParameter(1) + windowFwhm * 2.354820045 * sig1;
        if (lo < 1.) lo = 1.;
        if (hi > n) hi = n;
        fr = h->Fit(&f, "RQN0S", "", lo, hi);
    }

    const double amp = f.GetParameter(0);
    const double mu = f.GetParameter(1);
    const double sig = f.GetParameter(2);
    if (!(sig > 0.) || !(amp > 0.)) return r;

    const double bw = h->GetBinWidth(1);
    r.meanRaw = a0 + a1 * mu;
    r.fwhmRaw = 2.354820045 * std::fabs(a1) * sig;
    r.fwhmRawErr = 2.354820045 * std::fabs(a1) * f.GetParError(2);
    r.area = amp * sig * kSqrtTwoPi * bw;
    // Error propagation with the amplitude-sigma covariance.
    if (fr && fr->GetCovarianceMatrix().GetNrows() >= 3) {
        const TMatrixDSym cov = fr->GetCovarianceMatrix();
        const double va = cov(0, 0);
        const double vs = cov(2, 2);
        const double cas = cov(0, 2);
        const double dA_da = sig * kSqrtTwoPi * bw;
        const double dA_ds = amp * kSqrtTwoPi * bw;
        const double var = dA_da * dA_da * va + dA_ds * dA_ds * vs +
                           2. * dA_da * dA_ds * cas;
        r.areaErr = var > 0. ? std::sqrt(var) : 0.;
    } else {
        r.areaErr = std::fabs(sig * kSqrtTwoPi * bw) * f.GetParError(0);
    }
    r.chi2 = f.GetChisquare();
    r.ndf = f.GetNDF();
    r.fitLo = a0 + a1 * lo;
    r.fitHi = a0 + a1 * hi;
    r.ok = true;
    return r;
}

// Local sideband integration around the fitted peak.  Peak region is
// +/-peakFwhm * FWHM; sidebands are [1.2,1.8] FWHM on both sides.  This is a
// cross-check, not the nominal area.
PeakFit FitSideband(TH1D* h, double mean, double fwhm,
                    double a0, double a1)
{
    PeakFit r;
    if (!(fwhm > 0.)) return r;
    const double muCh = (mean - a0) / a1;
    const double fwCh = fwhm / std::fabs(a1);
    const int n = h->GetNbinsX();

    auto range = [&](double x0, double x1, double& sum, int& nbin) {
        sum = 0.;
        nbin = 0;
        for (int b = 1; b <= n; ++b) {
            const double x = h->GetBinCenter(b);
            if (x >= x0 && x <= x1) {
                sum += h->GetBinContent(b);
                ++nbin;
            }
        }
    };

    double peakSum = 0.;
    int peakN = 0;
    double side1 = 0., side2 = 0.;
    int side1N = 0, side2N = 0;
    range(muCh - 1.0 * fwCh, muCh + 1.0 * fwCh, peakSum, peakN);
    range(muCh - 1.8 * fwCh, muCh - 1.2 * fwCh, side1, side1N);
    range(muCh + 1.2 * fwCh, muCh + 1.8 * fwCh, side2, side2N);
    if (peakN <= 0 || side1N <= 0 || side2N <= 0) return r;

    const double b1 = side1 / side1N;
    const double b2 = side2 / side2N;
    const double bkg = 0.5 * (b1 + b2);
    const double varBkg =
        0.25 * (side1 / (side1N * side1N) + side2 / (side2N * side2N));
    const double area = peakSum - bkg * peakN;
    const double varArea = peakSum + peakN * peakN * varBkg;
    r.meanRaw = mean;
    r.fwhmRaw = fwhm;
    r.area = area;
    r.areaErr = varArea > 0. ? std::sqrt(varArea) : 0.;
    r.ok = true;
    return r;
}

// Simultaneous two-Gaussian fit with a quadratic background, used only for
// the Co-60 1173/1332 pair where the continuum under 1173 keV is not linear.
// The widths are not tied; the purpose is to expose the background-model
// spread in the area.
bool FitTwoPeaks(TH1D* h, double e1, double e2, double a0, double a1,
                 const PeakFit& p1, const PeakFit& p2,
                 PeakFit& out1, PeakFit& out2)
{
    const double loE = 1050.;
    const double hiE = 1420.;
    const double lo = (loE - a0) / a1;
    const double hi = (hiE - a0) / a1;
    TF1 f("ftwo", "gaus(0)+gaus(3)+pol2(6)", lo, hi);
    const double mu1 = (p1.meanRaw - a0) / a1;
    const double mu2 = (p2.meanRaw - a0) / a1;
    const double s1 = p1.fwhmRaw / 2.354820045 / std::fabs(a1);
    const double s2 = p2.fwhmRaw / 2.354820045 / std::fabs(a1);
    f.SetParameters(h->GetBinContent(h->FindBin(mu1)), mu1, s1,
                    h->GetBinContent(h->FindBin(mu2)), mu2, s2, 0., 0., 0.);
    f.SetParLimits(1, mu1 - 0.02 * e1 / std::fabs(a1),
                   mu1 + 0.02 * e1 / std::fabs(a1));
    f.SetParLimits(4, mu2 - 0.02 * e2 / std::fabs(a1),
                   mu2 + 0.02 * e2 / std::fabs(a1));
    f.SetParLimits(2, 0.5 * s1, 2.0 * s1);
    f.SetParLimits(5, 0.5 * s2, 2.0 * s2);
    TFitResultPtr fr = h->Fit(&f, "RQN0S", "", lo, hi);

    const double bw = h->GetBinWidth(1);
    auto extract = [&](int base, PeakFit& out) {
        const double amp = f.GetParameter(base);
        const double sig = f.GetParameter(base + 2);
        const double mu = f.GetParameter(base + 1);
        if (!(sig > 0.) || !(amp > 0.)) return;
        out.meanRaw = a0 + a1 * mu;
        out.fwhmRaw = 2.354820045 * std::fabs(a1) * sig;
        out.area = amp * sig * kSqrtTwoPi * bw;
        if (fr && fr->GetCovarianceMatrix().GetNrows() >= base + 3) {
            const TMatrixDSym cov = fr->GetCovarianceMatrix();
            const double va = cov(base, base);
            const double vs = cov(base + 2, base + 2);
            const double cas = cov(base, base + 2);
            const double dA_da = sig * kSqrtTwoPi * bw;
            const double dA_ds = amp * kSqrtTwoPi * bw;
            const double var = dA_da * dA_da * va + dA_ds * dA_ds * vs +
                               2. * dA_da * dA_ds * cas;
            out.areaErr = var > 0. ? std::sqrt(var) : 0.;
        }
        out.chi2 = f.GetChisquare();
        out.ndf = f.GetNDF();
        out.ok = true;
    };
    extract(0, out1);
    extract(3, out2);
    return out1.ok && out2.ok;
}

struct MeasDef {
    int distance;
    const char* source;
    const char* file;
};

struct ResultRow {
    int distance = 0;
    std::string source;
    double energy = 0.;
    const CsvRow* csv = nullptr;
    double aExact = 0.;
    double tLive = 0.;
    PeakFit gauss;
    PeakFit net;
    PeakFit side;
    PeakFit joint;
    double calA = 0.;
    double calB = 1.;
    double meanCorr = 0.;
    double fwhmCorr = 0.;
    double fwhmCorrErr = 0.;
};

} // namespace

void analyze_exp_spectra(const char* expDir = "build/analysis/clyc_exp_2026",
                         const char* csvPath =
                             "$HOME/data/CLYC_validation/effDist_clyc1+2.csv",
                         const char* outDataDir = "report/eff_clyc/data")
{
    const TString csvExpanded = gSystem->ExpandPathName(csvPath);
    const TString dir(expDir);
    const TString outDir(outDataDir);
    gSystem->mkdir(outDir, kTRUE);

    const std::vector<CsvRow> csv = ReadCsv(csvExpanded.Data());
    if (csv.empty()) return;

    const MeasDef meas[] = {
        {200, "CS", "Meas1_200_CS_Clyc2.root"},
        {300, "CS", "Meas1_300_CS_Clyc2.root"},
        {400, "CS", "Meas1_400_CS_Clyc2.root"},
        {200, "Co", "Meas1_200_Co_Clyc2.root"},
        {300, "Co", "Meas1_300_Co_Clyc2.root"},
        {400, "Co", "Meas1_400_Co_Clyc2.root"},
    };
    const char* bkg = Form("%s/Hintergrund.root", dir.Data());

    std::vector<ResultRow> results;
    std::printf("\n=== CLYC2 peak-integral validation ===\n");
    std::printf("%-3s %4s %8s %10s %10s %8s %8s %8s %8s\n",
                "src", "d", "E", "N_gauss", "N_CSV", "r_gauss", "r_net",
                "r_side", "r_joint");

    for (const auto& m : meas) {
        Spectrum s;
        const TString path = Form("%s/%s", dir.Data(), m.file);
        if (!ReadSpectrum(path, s, true, bkg)) continue;
        const std::string nucleus = (std::string(m.source) == "CS")
                                        ? "Cs-137" : "Co-60";
        const std::vector<double> lines =
            (std::string(m.source) == "CS")
                ? std::vector<double>{661.64}
                : std::vector<double>{1173.23, 1332.51};

        std::vector<ResultRow> local;
        for (double e : lines) {
            const CsvRow* row = FindRow(csv, m.distance, nucleus, e);
            if (!row) {
                std::printf("WARNING: no CSV row for %s d=%d E=%.2f\n",
                            nucleus.c_str(), m.distance, e);
                continue;
            }
            ResultRow r;
            r.distance = m.distance;
            r.source = m.source;
            r.energy = e;
            r.csv = row;
            const double dt = static_cast<double>(s.startUnix) -
                              RefUnix(*row);
            r.aExact = row->activity * std::exp(-std::log(2.) * dt /
                                                row->halfLife);
            r.tLive = s.live;
            r.gauss = FitPeak(s.counts, e, s.A0, s.A1);
            r.net = FitPeak(s.countsNet, e, s.A0, s.A1);
            if (r.gauss.ok) {
                r.side = FitSideband(s.counts, r.gauss.meanRaw,
                                     r.gauss.fwhmRaw, s.A0, s.A1);
            }
            local.push_back(r);
        }

        // Gain correction from the observed full-energy centroids.  Cs-137
        // has one line, so the correction is a pure scale.  Co-60 has two,
        // so a linear two-point calibration is used.  The correction is only
        // for plots and for correcting the FWHM to the true energy scale; the
        // net areas do not depend on it.
        double calA = 0., calB = 1.;
        if (std::string(m.source) == "CS" && local.size() == 1 && local[0].gauss.ok) {
            calB = local[0].energy / local[0].gauss.meanRaw;
        } else if (local.size() == 2 && local[0].gauss.ok && local[1].gauss.ok) {
            calB = (local[1].energy - local[0].energy) /
                   (local[1].gauss.meanRaw - local[0].gauss.meanRaw);
            calA = local[0].energy - calB * local[0].gauss.meanRaw;
        }

        for (auto& r : local) {
            r.calA = calA;
            r.calB = calB;
            if (r.gauss.ok) {
                r.meanCorr = calA + calB * r.gauss.meanRaw;
                r.fwhmCorr = calB * r.gauss.fwhmRaw;
                r.fwhmCorrErr = calB * r.gauss.fwhmRawErr;
            }
            if (std::string(m.source) == "Co" && local.size() == 2 &&
                local[0].gauss.ok && local[1].gauss.ok) {
                PeakFit j1, j2;
                if (FitTwoPeaks(s.counts, local[0].energy, local[1].energy,
                                s.A0, s.A1, local[0].gauss, local[1].gauss,
                                j1, j2)) {
                    if (std::fabs(r.energy - local[0].energy) < 0.01) r.joint = j1;
                    else r.joint = j2;
                }
            }
            results.push_back(r);
        }
    }

    // --- Write the peak-integral table -------------------------------------
    const TString peaksCsv = Form("%s/exp_peak_integrals.csv", outDir.Data());
    std::ofstream out(peaksCsv.Data());
    out << "detector,distance_mm,source,E_keV,"
           "A_ref_Bq,ref_date,A_exact_Bq,A_csv_Bq,P_gamma_pct,t_live_s,"
           "N_peak_gauss,N_peak_gauss_err,N_peak_net,N_peak_net_err,"
           "N_peak_sideband,N_peak_sideband_err,N_peak_joint,N_peak_joint_err,"
           "N_peak_csv,ratio_gauss_csv,ratio_net_csv,ratio_sideband_csv,"
           "ratio_joint_csv,"
           "eff_gauss_pct,eff_gauss_err_pct,eff_net_pct,eff_net_err_pct,"
           "eff_csv_implied_pct,eff_csv_printed_corrected_pct,"
           "ratio_eff_gauss_csv,ratio_eff_net_csv,"
           "mean_raw_keV,FWHM_raw_keV,FWHM_raw_err_keV,"
           "mean_corr_keV,FWHM_corr_keV,resolution_pct,resolution_err_pct,"
           "chi2_ndf,cal_a_keV,cal_b\n";

    auto eff = [](double n, double a, double p, double t) {
        return (a > 0. && p > 0. && t > 0.) ? 100. * n / (a * p / 100. * t) : 0.;
    };
    auto effErr = [](double n, double dN, double a, double p, double t) {
        return (a > 0. && p > 0. && t > 0.)
                   ? 100. * dN / (a * p / 100. * t) : 0.;
    };

    for (const auto& r : results) {
        const CsvRow& c = *r.csv;
        const double eg = eff(r.gauss.area, r.aExact, c.pGamma, r.tLive);
        const double egErr = effErr(r.gauss.area, r.gauss.areaErr,
                                    r.aExact, c.pGamma, r.tLive);
        const double en = eff(r.net.area, r.aExact, c.pGamma, r.tLive);
        const double enErr = effErr(r.net.area, r.net.areaErr,
                                    r.aExact, c.pGamma, r.tLive);
        const double ecsv = eff(c.netPeak, r.aExact, c.pGamma, r.tLive);
        const double eprinted = c.effPrinted / 1000.;
        const double res = (r.meanCorr > 0.) ? 100. * r.fwhmCorr / r.meanCorr : 0.;
        const double resErr = (r.meanCorr > 0.)
                                  ? 100. * r.fwhmCorrErr / r.meanCorr : 0.;
        const double chi2ndf = (r.gauss.ndf > 0)
                                   ? r.gauss.chi2 / r.gauss.ndf : 0.;

        out << "CLYC2," << r.distance << "," << r.source << ","
            << Form("%.2f", r.energy) << ","
            << Form("%.6g", c.activity) << ","
            << Form("%02d.%02d.%04d", c.refDay, c.refMonth, c.refYear) << ","
            << Form("%.6g", r.aExact) << ","
            << Form("%.6g", c.currentActivity) << ","
            << Form("%.2f", c.pGamma) << ","
            << Form("%.1f", r.tLive) << ","
            << Form("%.1f", r.gauss.area) << ","
            << Form("%.1f", r.gauss.areaErr) << ","
            << Form("%.1f", r.net.area) << ","
            << Form("%.1f", r.net.areaErr) << ","
            << Form("%.1f", r.side.area) << ","
            << Form("%.1f", r.side.areaErr) << ","
            << (r.joint.ok ? Form("%.1f", r.joint.area) : "")
            << "," << (r.joint.ok ? Form("%.1f", r.joint.areaErr) : "")
            << "," << Form("%.0f", c.netPeak) << ","
            << Form("%.4f", r.gauss.area / c.netPeak) << ","
            << Form("%.4f", r.net.area / c.netPeak) << ","
            << Form("%.4f", r.side.area / c.netPeak) << ","
            << (r.joint.ok ? Form("%.4f", r.joint.area / c.netPeak) : "")
            << "," << Form("%.6f", eg) << "," << Form("%.6f", egErr) << ","
            << Form("%.6f", en) << "," << Form("%.6f", enErr) << ","
            << Form("%.6f", ecsv) << "," << Form("%.6f", eprinted) << ","
            << Form("%.4f", eg / ecsv) << "," << Form("%.4f", en / ecsv) << ","
            << Form("%.2f", r.gauss.meanRaw) << ","
            << Form("%.3f", r.gauss.fwhmRaw) << ","
            << Form("%.3f", r.gauss.fwhmRawErr) << ","
            << Form("%.3f", r.meanCorr) << ","
            << Form("%.3f", r.fwhmCorr) << ","
            << Form("%.4f", res) << "," << Form("%.4f", resErr) << ","
            << Form("%.3f", chi2ndf) << ","
            << Form("%.4f", r.calA) << "," << Form("%.6f", r.calB) << "\n";

        std::printf("%-3s %4d %8.2f %10.0f %10.0f %8.4f %8.4f %8.4f %8s\n",
                    r.source.c_str(), r.distance, r.energy, r.gauss.area,
                    c.netPeak, r.gauss.area / c.netPeak,
                    r.net.area / c.netPeak,
                    r.side.ok ? r.side.area / c.netPeak : 0.,
                    r.joint.ok ? Form("%.4f", r.joint.area / c.netPeak) : "-");
    }
    out.close();
    std::printf("\nWrote %s\n", peaksCsv.Data());

    // --- Resolution summary and model --------------------------------------
    struct ResPoint {
        double E;
        double fwhm;
        double fwhmErr;
        double res;
        double resErr;
        int n;
        int used;
    };
    const double energies[3] = {661.64, 1173.23, 1332.51};
    std::vector<ResPoint> resPoints;
    for (double e : energies) {
        std::vector<double> fw, rw;
        for (const auto& r : results) {
            if (std::fabs(r.energy - e) < 0.01 && r.gauss.ok && r.meanCorr > 0.) {
                fw.push_back(r.fwhmCorr);
                rw.push_back(100. * r.fwhmCorr / r.meanCorr);
            }
        }
        if (fw.empty()) continue;
        double mf = 0., mr = 0.;
        for (double x : fw) mf += x;
        for (double x : rw) mr += x;
        mf /= fw.size();
        mr /= rw.size();
        double sf = 0., sr = 0.;
        for (double x : fw) sf += (x - mf) * (x - mf);
        for (double x : rw) sr += (x - mr) * (x - mr);
        sf = fw.size() > 1 ? std::sqrt(sf / (fw.size() - 1.)) : 0.;
        sr = rw.size() > 1 ? std::sqrt(sr / (rw.size() - 1.)) : 0.;
        ResPoint p;
        p.E = e;
        p.fwhm = mf;
        p.fwhmErr = sf / std::sqrt(static_cast<double>(fw.size()));
        p.res = mr;
        p.resErr = sr / std::sqrt(static_cast<double>(rw.size()));
        p.n = static_cast<int>(fw.size());
        p.used = (std::fabs(e - 1173.23) < 0.01) ? 0 : 1;
        resPoints.push_back(p);
    }

    // Fit b in R(E) = b / sqrt(E) to the two clean lines.  The 1173 keV
    // point is a cross-check because its fitted width is biased low by the
    // non-linear continuum under the peak.
    double b = 0., bErr = 0.;
    std::vector<double> bValues;
    for (const auto& p : resPoints) {
        if (p.used) bValues.push_back(p.res * std::sqrt(p.E));
    }
    if (!bValues.empty()) {
        for (double x : bValues) b += x;
        b /= bValues.size();
        double var = 0.;
        for (double x : bValues) var += (x - b) * (x - b);
        bErr = bValues.size() > 1
                   ? std::sqrt(var / (bValues.size() * (bValues.size() - 1.)))
                   : 0.;
        // Add the model spread as a systematic.
        bErr = std::sqrt(bErr * bErr + 0.25 * var / bValues.size());
    }

    const TString resCsv = Form("%s/exp_resolution.csv", outDir.Data());
    std::ofstream resOut(resCsv.Data());
    resOut << "# R(E) = FWHM(E)/E, R in percent, E in keV\n";
    resOut << "# fit R(E) = b/sqrt(E), b = " << Form("%.3f", b)
           << " +/- " << Form("%.3f", bErr)
           << " percent*sqrt(keV), fitted to the 661.64 and 1332.51 keV lines\n";
    resOut << "E_keV,n_distances,FWHM_mean_keV,FWHM_std_err_keV,"
              "R_mean_pct,R_std_err_pct,R_model_pct,residual_pct,used_in_fit\n";
    for (const auto& p : resPoints) {
        const double rmodel = (b > 0.) ? b / std::sqrt(p.E) : 0.;
        const double resid = (rmodel > 0.) ? 100. * (p.res - rmodel) / rmodel : 0.;
        resOut << Form("%.2f", p.E) << "," << p.n << ","
               << Form("%.3f", p.fwhm) << "," << Form("%.3f", p.fwhmErr) << ","
               << Form("%.4f", p.res) << "," << Form("%.4f", p.resErr) << ","
               << Form("%.4f", rmodel) << "," << Form("%.2f", resid) << ","
               << p.used << "\n";
        std::printf("RES E=%8.2f n=%d FWHM=%7.3f +/- %.3f keV  R=%6.3f +/- %.3f %%"
                    "  model=%6.3f %%  resid=%+5.1f %%  %s\n",
                    p.E, p.n, p.fwhm, p.fwhmErr, p.res, p.resErr, rmodel, resid,
                    p.used ? "fit" : "cross-check");
    }
    resOut.close();
    std::printf("Wrote %s\n", resCsv.Data());

    const TString modelCsv = Form("%s/exp_resolution_model.csv", outDir.Data());
    std::ofstream modelOut(modelCsv.Data());
    modelOut << "b_pct_sqrt_keV,b_err,n_lines,fit_energies_keV\n";
    modelOut << Form("%.4f", b) << "," << Form("%.4f", bErr) << ",2,"
             << "661.64;1332.51\n";
    modelOut.close();
    std::printf("Wrote %s\n", modelCsv.Data());
    std::printf("\nResolution model: FWHM(E) = (b/100)*sqrt(E) = (%.3f/100)*sqrt(E) keV\n", b);
    std::printf("                  R(E) = %.3f/sqrt(E) percent\n", b);

    // --- Resolution figure -------------------------------------------------
    if (!resPoints.empty()) {
        gStyle->SetOptStat(0);
        gStyle->SetOptTitle(0);
        gStyle->SetTextFont(42);
        gStyle->SetLabelFont(42, "XYZ");
        gStyle->SetTitleFont(42, "XYZ");
        gStyle->SetTitleSize(0.050, "XYZ");
        gStyle->SetLabelSize(0.045, "XYZ");

        const TString figDir = "report/eff_clyc/figures";
        gSystem->mkdir(figDir, kTRUE);
        TCanvas c("cres", "", 900, 420);
        c.Divide(2, 1, 0.001, 0.001);

        auto stylePad = [](TVirtualPad* p) {
            p->SetLeftMargin(0.14);
            p->SetBottomMargin(0.14);
            p->SetRightMargin(0.04);
            p->SetTopMargin(0.06);
            p->SetTickx(1);
            p->SetTicky(1);
        };

        // Panel (a): R(E)
        c.cd(1);
        stylePad(gPad);
        TH2F* frameR = new TH2F("frameR", "", 100, 600, 1400, 100, 2.8, 5.2);
        frameR->SetDirectory(nullptr);
        frameR->GetXaxis()->SetTitle("#font[132]{#it{E}}_{#gamma} [keV]");
        frameR->GetYaxis()->SetTitle("#font[132]{#it{R}} [%]");
        frameR->GetXaxis()->CenterTitle();
        frameR->GetYaxis()->CenterTitle();
        frameR->GetYaxis()->SetTitleOffset(1.30);
        frameR->Draw("axis");

        TF1 fR("fR", "[0]/sqrt(x)", 650, 1350);
        fR.SetParameter(0, b);
        fR.SetLineColor(kRed + 1);
        fR.SetLineStyle(2);
        fR.SetLineWidth(2);
        fR.Draw("same");

        TGraphErrors gR;
        int ip = 0;
        for (const auto& p : resPoints) {
            if (!p.used) continue;
            gR.SetPoint(ip, p.E, p.res);
            gR.SetPointError(ip, 0., p.resErr);
            ++ip;
        }
        gR.SetMarkerStyle(20);
        gR.SetMarkerSize(1.1);
        gR.SetMarkerColor(kBlack);
        gR.SetLineColor(kBlack);
        gR.SetLineWidth(1);
        gR.Draw("P same");

        // Mark the 1173 keV cross-check with an open marker.
        TGraphErrors gOpen;
        int io = 0;
        for (const auto& p : resPoints) {
            if (!p.used) {
                gOpen.SetPoint(io, p.E, p.res);
                gOpen.SetPointError(io, 0., p.resErr);
                ++io;
            }
        }
        gOpen.SetMarkerStyle(25);
        gOpen.SetMarkerSize(1.1);
        gOpen.SetMarkerColor(kBlack);
        gOpen.SetMarkerStyle(25);
        gOpen.Draw("P same");

        TLegend legR(0.54, 0.58, 0.94, 0.86);
        legR.SetBorderSize(1);
        legR.SetFillStyle(1001);
        legR.SetFillColor(kWhite);
        legR.SetTextSize(0.040);
        legR.AddEntry(&gR, "measured (fit)", "p");
        if (io > 0) legR.AddEntry(&gOpen, "1173 keV check", "p");
        legR.AddEntry(&fR, "#font[132]{#it{R}} = #font[132]{#it{b}}/#sqrt{#font[132]{#it{E}}}", "l");
        legR.Draw();
        TLatex latR;
        latR.SetNDC();
        latR.SetTextSize(0.045);
        latR.DrawLatex(0.28, 0.80, Form("#font[132]{#it{b}} = %.1f #pm %.1f", b, bErr));
        TLatex latPanel;
        latPanel.SetNDC();
        latPanel.SetTextFont(62);
        latPanel.SetTextSize(0.055);
        latPanel.DrawLatex(0.20, 0.86, "(a)");

        // Panel (b): FWHM(E)
        c.cd(2);
        stylePad(gPad);
        TH2F* frameF = new TH2F("frameF", "", 100, 600, 1400, 100, 28, 50);
        frameF->SetDirectory(nullptr);
        frameF->GetXaxis()->SetTitle("#font[132]{#it{E}}_{#gamma} [keV]");
        frameF->GetYaxis()->SetTitle("#font[132]{#it{#Delta E}}_{FWHM} [keV]");
        frameF->GetXaxis()->CenterTitle();
        frameF->GetYaxis()->CenterTitle();
        frameF->GetYaxis()->SetTitleOffset(1.30);
        frameF->Draw("axis");

        TF1 fF("fF", "[0]*sqrt(x)/100.", 650, 1350);
        fF.SetParameter(0, b);
        fF.SetLineColor(kRed + 1);
        fF.SetLineStyle(2);
        fF.SetLineWidth(2);
        fF.Draw("same");

        TGraphErrors gF;
        ip = 0;
        for (const auto& p : resPoints) {
            if (!p.used) continue;
            gF.SetPoint(ip, p.E, p.fwhm);
            gF.SetPointError(ip, 0., p.fwhmErr);
            ++ip;
        }
        gF.SetMarkerStyle(20);
        gF.SetMarkerSize(1.1);
        gF.SetMarkerColor(kBlack);
        gF.SetLineColor(kBlack);
        gF.Draw("P same");

        TGraphErrors gOpenF;
        io = 0;
        for (const auto& p : resPoints) {
            if (!p.used) {
                gOpenF.SetPoint(io, p.E, p.fwhm);
                gOpenF.SetPointError(io, 0., p.fwhmErr);
                ++io;
            }
        }
        gOpenF.SetMarkerStyle(25);
        gOpenF.SetMarkerSize(1.1);
        gOpenF.SetMarkerColor(kBlack);
        gOpenF.Draw("P same");

        TLegend legF(0.18, 0.56, 0.62, 0.80);
        legF.SetBorderSize(1);
        legF.SetFillStyle(1001);
        legF.SetFillColor(kWhite);
        legF.SetTextSize(0.040);
        legF.AddEntry(&gF, "measured (fit)", "p");
        if (io > 0) legF.AddEntry(&gOpenF, "1173 keV check", "p");
        legF.AddEntry(&fF, Form("#font[132]{#it{#Delta E}}_{FWHM} = %.2f#sqrt{#font[132]{#it{E}}}", b / 100.), "l");
        legF.Draw();

        c.cd(2);
        TLatex latPanel2;
        latPanel2.SetNDC();
        latPanel2.SetTextFont(62);
        latPanel2.SetTextSize(0.055);
        latPanel2.DrawLatex(0.20, 0.86, "(b)");

        const TString raw = Form("%s/fig_resolution_raw.pdf", figDir.Data());
        const TString out = Form("%s/fig_resolution.pdf", figDir.Data());
        c.SaveAs(raw);
        gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                           raw.Data(), out.Data()));
        gSystem->Unlink(raw);
        std::printf("Wrote %s\n", out.Data());
    }
}
