// check_absolute_scale.C
//
// Diagnostic for the absolute normalization of the WSSA surface source.
// It combines the existing full-statistics F1 map with a 10M-record sequential
// sample that also contains the F2 (1/|mu|) map.  F2 is estimated at the hot
// bin of the full F1 map from the local F2/F1 ratio in the sample; this avoids
// pretending that a sample maximum is a full-statistics maximum.
//
// Usage:
//   root -l -b -q 'scripts/wssa/check_absolute_scale.C()'
//
// Output:
//   report/wti-binary-reader/data/absolute_scale_diagnostic.csv

#include "TCanvas.h"
#include "TFile.h"
#include "TH2D.h"
#include "TNamed.h"
#include "TString.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRcm = 133.25;                  // cask side radius
constexpr double kNativeDphiDeg = 5.0;
constexpr double kNativeDzCm = 5.0;
constexpr double kAreaCm2 =
    kRcm * (kNativeDphiDeg * kPi / 180.0) * kNativeDzCm;
constexpr double kRateToDose = 3600.0 / 1.0e6;   // pSv/s -> uSv/h

struct Case {
    const char* tag;
    double reference;
};

double ReadNpAbs(TFile& f)
{
    auto* info = dynamic_cast<TNamed*>(f.Get("wssa_info"));
    if (!info) return 0.0;
    const std::string s = info->GetTitle();
    const std::string key = "npAbs=";
    const std::size_t p = s.find(key);
    if (p == std::string::npos) return 0.0;
    return std::fabs(std::stod(s.substr(p + key.size())));
}

TH2D* Native(TH2D* h)
{
    if (!h) return nullptr;
    return dynamic_cast<TH2D*>(h->Rebin2D(5, 5));
}

} // namespace

void check_absolute_scale(
    const char* outCsv = "report/wti-binary-reader/data/absolute_scale_diagnostic.csv",
    const char* sampleDir = "scripts/wssa/output_diag")
{
    const Case cases[] = {
        {"Cask1_neutrons", 60.0},
        {"Cask1_gamma",     5.0},
        {"Cask2_neutrons",  6.0},
        {"Cask2_gamma",     1.4},
        {"Cask3_neutrons", 35.0},
        {"Cask3_gamma",     2.5}
    };

    std::ofstream csv(outCsv);
    csv << "tag,np_abs,reference_uSv_h,"
           "F1_max_uSv_h,F1_per_np_uSv_h,F2_est_uSv_h,F2_per_np_uSv_h,"
           "F1_per_np_over_ref,F2_per_np_over_ref,F2_over_F1_local\n";

    std::cout << std::scientific << std::setprecision(5);
    std::cout << "area = " << kAreaCm2 << " cm2; 3600/1e6 = "
              << kRateToDose << "\n";
    std::cout << std::left << std::setw(16) << "tag"
              << std::right << std::setw(12) << "np_abs"
              << std::setw(14) << "F1_max"
              << std::setw(14) << "F1/np"
              << std::setw(14) << "F2_est"
              << std::setw(14) << "F2/np"
              << std::setw(10) << "ref"
              << std::setw(12) << "F1np/ref"
              << std::setw(12) << "F2np/ref"
              << std::setw(12) << "F2/F1" << "\n";

    for (const auto& c : cases) {
        const std::string fullName =
            std::string("scripts/wssa/output_all/") + c.tag + "_all.root";
        const std::string sampleName =
            std::string(sampleDir) + "/" + c.tag + "_seq10m.root";

        TFile ff(fullName.c_str());
        TFile fs(sampleName.c_str());
        if (ff.IsZombie() || fs.IsZombie()) {
            std::cerr << "missing input for " << c.tag << ": "
                      << fullName << " / " << sampleName << "\n";
            continue;
        }

        TH2D* hf = dynamic_cast<TH2D*>(ff.Get("hPhiZ_dose_fine"));
        TH2D* h1 = dynamic_cast<TH2D*>(fs.Get("hPhiZ_dose_fine"));
        TH2D* h2 = dynamic_cast<TH2D*>(fs.Get("hPhiZ_dose_flux_fine"));
        if (!hf || !h1 || !h2) {
            std::cerr << "missing histogram for " << c.tag << "\n";
            continue;
        }

        TH2D* hfN = Native(hf);
        TH2D* h1N = Native(h1);
        TH2D* h2N = Native(h2);
        if (!hfN || !h1N || !h2N) {
            std::cerr << "rebin failed for " << c.tag << "\n";
            continue;
        }

        const double npAbs = ReadNpAbs(fs);
        const double f1Max = hfN->GetMaximum();
        double f2Max = f1Max;

        int bx = 1, by = 1, bz = 1;
        hfN->GetMaximumBin(bx, by, bz);
        const double s1 = h1N->GetBinContent(bx, by);
        const double s2 = h2N->GetBinContent(bx, by);
        const double localRatio = (s1 > 0.0) ? s2 / s1 : 1.0;
        f2Max = f1Max * localRatio;

        const double f1Dose = f1Max * kRateToDose / kAreaCm2;
        const double f2Dose = f2Max * kRateToDose / kAreaCm2;
        const double f1PerNp = (npAbs > 0.0) ? f1Dose / npAbs : 0.0;
        const double f2PerNp = (npAbs > 0.0) ? f2Dose / npAbs : 0.0;

        std::cout << std::left << std::setw(16) << c.tag
                  << std::right << std::setw(12) << npAbs
                  << std::setw(14) << f1Dose
                  << std::setw(14) << f1PerNp
                  << std::setw(14) << f2Dose
                  << std::setw(14) << f2PerNp
                  << std::setw(10) << c.reference
                  << std::setw(12) << f1PerNp / c.reference
                  << std::setw(12) << f2PerNp / c.reference
                  << std::setw(12) << localRatio << "\n";

        csv << c.tag << ',' << npAbs << ',' << c.reference << ','
            << f1Dose << ',' << f1PerNp << ','
            << f2Dose << ',' << f2PerNp << ','
            << f1PerNp / c.reference << ','
            << f2PerNp / c.reference << ','
            << localRatio << '\n';
    }

    std::cout << "\nwrote " << outCsv << "\n";
}
