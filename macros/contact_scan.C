// contact_scan.C -- analysis of the bare-holder contact-fraction scan.
//
// The scan varies the effective area fraction f of the bare plate that is in
// contact with the black holder (0 = clean 1.2 mm air gap, 1 = fully absorbing
// contact).  For each config 1-3, fraction f and irradiation position
// (centre / edge / corner) the per-event photoelectron yield is reconstructed
// from the `sipmHits` tree:
//
//     N_pe(event) = sum over SiPMs of nDetected
//     mean        = sum_events N_pe / nEvents
//     SEM         = sample_stddev(N_pe) / sqrt(nEvents)
//
// where nEvents is the number of entries in `opticalStats` (one row/event; the
// sipmHits tree has one row per event x SiPM, including zero-detection rows).
//
// The matrix scan supplies the TiO2 reference (muon_out_c<C>_r1_<pos>.root).
// For every case the macro reports the ratio to the f = 0 contact value (with
// propagated uncertainty) and the difference to the matrix TiO2 reference.
// The approximate crossing fraction where contact bare falls below matrix TiO2
// is obtained by linear interpolation between the two adjacent f samples that
// bracket the reference.
//
// Outputs (in outdir):
//   contact_scan.csv   config,fraction,position,events,mean_npe,sem_npe,
//                      ratio_to_f0,ratio_sem,tio2_ref,delta_to_tio2
//   contact_scan.pdf
//   contact_scan.png   2x2: cfg 1/2/3 absolute yields vs f (three positions,
//                      dashed same-colour TiO2 reference lines) and the
//                      relative yield Y(f)/Y(0) at centre for cfg 1-3.
//
// Usage (from the repository root):
//   root -l -b -q 'macros/contact_scan.C+("/path/to/contact","/path/to/matrix","report/figures")'
//
// Defaults point at the v5 production trees on this machine.

#include <TCanvas.h>
#include <TFile.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TList.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TString.h>
#include <TTree.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// Shared ROOT style and the PetroffPalette P6 colours.  The report build sets
// ROOT_INCLUDE_PATH to ${HOME}/root-work; the relative fallback keeps the
// documented `root -l -b -q 'macros/contact_scan.C+(...)'` command working
// from the repository root without extra environment setup.
#if __has_include("style.h")
#include "style.h"
#elif __has_include("../../../root-work/style.h")
#include "../../../root-work/style.h"
#else
#error "style.h not found: set ROOT_INCLUDE_PATH to the directory containing style.h"
#endif

namespace {

const int kNCfg = 3;
const char* kPosName[3] = {"centre", "edge", "corner"};
const int kPosMarker[3] = {20, 21, 22};   // filled circle / square / triangle

struct Stat {
    int n = 0;
    double mean = 0.;
    double sem = 0.;
    bool ok = false;
};

// Read one ROOT file: group sipmHits.nDetected by evtNb, then use the
// opticalStats entry count as the event count (this includes events whose
// per-event sum is zero).
bool readStat(const TString& fname, Stat& st)
{
    st = Stat();

    TFile* f = TFile::Open(fname, "READ");
    if (!f || f->IsZombie()) {
        std::cerr << "contact_scan: cannot open " << fname << "\n";
        delete f;
        return false;
    }

    TTree* hits = (TTree*)f->Get("sipmHits");
    TTree* opt = (TTree*)f->Get("opticalStats");
    if (!hits || !opt) {
        std::cerr << "contact_scan: missing sipmHits/opticalStats in "
                  << fname << "\n";
        delete f;
        return false;
    }

    std::map<int, double> perEventSum;
    int evt = 0, nDet = 0;
    hits->SetBranchAddress("evtNb", &evt);
    hits->SetBranchAddress("nDetected", &nDet);
    for (Long64_t i = 0; i < hits->GetEntries(); ++i) {
        hits->GetEntry(i);
        perEventSum[evt] += nDet;
    }

    std::vector<double> y;
    y.reserve(opt->GetEntries());
    opt->SetBranchAddress("evtNb", &evt);
    for (Long64_t i = 0; i < opt->GetEntries(); ++i) {
        opt->GetEntry(i);
        const auto it = perEventSum.find(evt);
        y.push_back(it == perEventSum.end() ? 0. : it->second);
    }

    st.n = (int)y.size();
    if (st.n <= 0) {
        delete f;
        return false;
    }

    long double sum = 0.;
    for (double v : y) sum += v;
    st.mean = (double)(sum / st.n);

    if (st.n > 1) {
        long double var = 0.;
        for (double v : y) {
            const long double d = v - st.mean;
            var += d * d;
        }
        var /= (st.n - 1);
        st.sem = std::sqrt((double)(var / st.n));
    }
    st.ok = true;

    delete f;
    return true;
}

// Parse a contact-case directory name "<cfg>_f<fraction>", e.g. c1_f0p015.
// 'p' is the decimal separator used in the production directory names.
bool parseCaseName(const char* cname, int& cfg, double& fraction)
{
    const std::string name(cname);
    if (name.size() < 3 || name[0] != 'c') return false;
    const std::size_t u = name.find("_f");
    if (u == std::string::npos || u <= 1) return false;

    const std::string cfgStr = name.substr(1, u - 1);
    char* end = nullptr;
    const long c = std::strtol(cfgStr.c_str(), &end, 10);
    if (!end || *end != '\0') return false;

    std::string fracStr = name.substr(u + 2);
    if (fracStr.empty()) return false;
    for (char& ch : fracStr) {
        if (ch == 'p' || ch == 'P') ch = '.';
    }
    char* fend = nullptr;
    const double frac = std::strtod(fracStr.c_str(), &fend);
    if (fend == fracStr.c_str()) return false;
    while (fend && *fend) {
        if (!std::isspace((unsigned char)*fend)) return false;
        ++fend;
    }
    if (frac < 0. || frac > 1.) return false;

    cfg = (int)c;
    fraction = frac;
    return true;
}

struct Result {
    Stat stat;
    double ratio = 0.;
    double ratioSem = 0.;
    double tio2 = 0.;
    double deltaToTio2 = 0.;
    bool hasTio2 = false;
};

struct Crossing {
    bool found = false;
    bool belowAtStart = false;   // already below TiO2 at the smallest f
    bool unavailable = false;    // no TiO2 reference or no scan points
    double f = 0.;
};

std::string crossingLabel(const Crossing& c, double fmax)
{
    if (c.found) return TString::Format("%.4f", c.f).Data();
    if (c.belowAtStart) return "at f=0";
    if (c.unavailable) return "n/a";
    return TString::Format(">%.3f", fmax).Data();
}

}  // namespace

void contact_scan(
    const char* contactDir =
        "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5/contact",
    const char* matrixDir =
        "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5/matrix",
    const char* outdir = "report/figures")
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);       // style() hides titles; panels are labelled below
    gStyle->SetLegendBorderSize(0);
    gStyle->SetEndErrorSize(0);   // no horizontal caps on error bars
    gSystem->mkdir(outdir, kTRUE);

    // ------------------------------------------------------------------
    // Discover the contact case directories (do not rely on the CWD).
    // ------------------------------------------------------------------
    std::map<int, std::map<double, std::string>> cases;   // cfg -> f -> dir
    TSystemDirectory sd(contactDir, contactDir);
    TList* entries = sd.GetListOfFiles();
    if (!entries) {
        std::cerr << "contact_scan: cannot list " << contactDir << "\n";
        return;
    }
    for (auto* obj : *entries) {
        TSystemFile* sf = dynamic_cast<TSystemFile*>(obj);
        if (!sf || !sf->IsDirectory()) continue;
        int cfg = 0;
        double frac = 0.;
        if (!parseCaseName(sf->GetName(), cfg, frac)) continue;
        if (cfg < 1 || cfg > kNCfg) continue;
        cases[cfg][frac] = std::string(contactDir) + "/" + sf->GetName();
    }

    int nCases = 0;
    for (const auto& c : cases) nCases += (int)c.second.size();
    std::cout << "contact_scan: found " << nCases << " contact cases ("
              << cases.size() << " configs) in " << contactDir << "\n";
    if (nCases == 0) {
        std::cerr << "contact_scan: no c<C>_f<fraction> directories found\n";
        return;
    }

    // ------------------------------------------------------------------
    // Read the contact scan and the matrix TiO2 references.
    // ------------------------------------------------------------------
    std::map<int, std::map<double, std::vector<Stat>>> contact;
    int nContactFiles = 0;
    for (const auto& c : cases) {
        const int cfg = c.first;
        for (const auto& f : c.second) {
            const double frac = f.first;
            contact[cfg][frac].assign(3, Stat());
            for (int p = 0; p < 3; ++p) {
                const TString fn = TString::Format(
                    "%s/muon_out_c%d_r0_%s.root", f.second.c_str(), cfg,
                    kPosName[p]);
                Stat st;
                if (readStat(fn, st)) {
                    contact[cfg][frac][p] = st;
                    ++nContactFiles;
                }
            }
        }
    }

    std::map<int, std::vector<Stat>> tio2;   // cfg -> position
    int nMatrixFiles = 0;
    for (int cfg = 1; cfg <= kNCfg; ++cfg) {
        tio2[cfg].assign(3, Stat());
        for (int p = 0; p < 3; ++p) {
            const TString fn = TString::Format("%s/muon_out_c%d_r1_%s.root",
                                               matrixDir, cfg, kPosName[p]);
            Stat st;
            if (readStat(fn, st)) {
                tio2[cfg][p] = st;
                ++nMatrixFiles;
            }
        }
    }

    std::cout << "contact_scan: read " << nContactFiles
              << " contact files and " << nMatrixFiles
              << " matrix TiO2 reference files\n";

    // ------------------------------------------------------------------
    // Ratios to f = 0 and deltas to the matrix TiO2 reference.
    // ------------------------------------------------------------------
    std::map<int, std::map<double, std::vector<Result>>> res;
    for (const auto& c : contact) {
        const int cfg = c.first;
        const auto it0 = c.second.find(0.0);
        const bool haveF0 = (it0 != c.second.end());

        for (const auto& f : c.second) {
            const double frac = f.first;
            res[cfg][frac].assign(3, Result());
            for (int p = 0; p < 3; ++p) {
                Result& r = res[cfg][frac][p];
                r.stat = f.second[p];
                r.hasTio2 = tio2[cfg][p].ok;
                r.tio2 = r.hasTio2 ? tio2[cfg][p].mean : std::nan("");
                r.deltaToTio2 = r.stat.mean - r.tio2;

                if (!haveF0) continue;
                const Stat& s0 = it0->second[p];
                if (frac == 0.0) {
                    // Same measurement: the ratio is exactly unity.
                    r.ratio = 1.;
                    r.ratioSem = 0.;
                } else if (s0.mean > 0. && r.stat.mean > 0.) {
                    r.ratio = r.stat.mean / s0.mean;
                    r.ratioSem = std::fabs(r.ratio) *
                                 std::sqrt(std::pow(r.stat.sem / r.stat.mean, 2) +
                                           std::pow(s0.sem / s0.mean, 2));
                }
            }
        }
        if (!haveF0) {
            std::cerr << "contact_scan: no f = 0 case for config " << cfg
                      << "; ratios left at zero\n";
        }
    }

    // ------------------------------------------------------------------
    // Crossing fractions: first adjacent pair with y(f_i) >= TiO2 >
    // y(f_{i+1}), linearly interpolated in f.
    // ------------------------------------------------------------------
    std::map<int, std::vector<Crossing>> crossings;
    double fmax = 0.;
    for (const auto& c : contact)
        for (const auto& f : c.second) fmax = std::max(fmax, f.first);

    for (const auto& c : contact) {
        const int cfg = c.first;
        crossings[cfg].assign(3, Crossing());

        std::vector<double> fs;
        fs.reserve(c.second.size());
        for (const auto& f : c.second) fs.push_back(f.first);
        std::sort(fs.begin(), fs.end());

        for (int p = 0; p < 3; ++p) {
            Crossing& cr = crossings[cfg][p];
            if (!tio2[cfg][p].ok || fs.size() < 2) {
                cr.unavailable = true;
                continue;
            }
            const double ref = tio2[cfg][p].mean;
            bool done = false;
            for (std::size_t i = 0; i + 1 < fs.size(); ++i) {
                const double y0 = c.second.at(fs[i])[p].mean;
                const double y1 = c.second.at(fs[i + 1])[p].mean;
                if (y0 >= ref && y1 < ref) {
                    double t = (y0 - ref) / (y0 - y1);
                    t = std::max(0., std::min(1., t));
                    cr.found = true;
                    cr.f = fs[i] + t * (fs[i + 1] - fs[i]);
                    done = true;
                    break;
                }
            }
            if (!done) {
                if (c.second.at(fs.front())[p].mean < ref)
                    cr.belowAtStart = true;
            }
        }
    }

    // ------------------------------------------------------------------
    // CSV.
    // ------------------------------------------------------------------
    const TString csvName = TString::Format("%s/contact_scan.csv", outdir);
    FILE* fp = std::fopen(csvName, "w");
    if (!fp) {
        std::cerr << "contact_scan: cannot write " << csvName << "\n";
    } else {
        std::fprintf(fp,
                     "config,fraction,position,events,mean_npe,sem_npe,"
                     "ratio_to_f0,ratio_sem,tio2_ref,delta_to_tio2,"
                     "crossing_fraction\n");
        for (const auto& c : res) {
            for (const auto& f : c.second) {
                for (int p = 0; p < 3; ++p) {
                    const Result& r = f.second[p];
                    std::fprintf(fp,
                                 "%d,%.3f,%s,%d,%.4f,%.4f,%.5f,%.5f,%.4f,"
                                 "%.4f,%s\n",
                                 c.first, f.first, kPosName[p], r.stat.n,
                                 r.stat.mean, r.stat.sem, r.ratio,
                                 r.ratioSem, r.tio2, r.deltaToTio2,
                                 crossingLabel(crossings[c.first][p], fmax)
                                     .c_str());
                }
            }
        }
        std::fclose(fp);
        std::cout << "contact_scan: wrote " << csvName << "\n";
    }

    // ------------------------------------------------------------------
    // Console summary: means at a few representative fractions.
    // ------------------------------------------------------------------
    const double keyF[4] = {0., 0.015, 0.020, 0.030};
    auto meanAt = [&](int cfg, double frac, int p) {
        const auto cfgIt = contact.find(cfg);
        if (cfgIt == contact.end()) return std::nan("");
        const auto fIt = cfgIt->second.find(frac);
        if (fIt == cfgIt->second.end()) return std::nan("");
        return fIt->second[p].mean;
    };

    std::printf("\n%-4s %-7s %8s %8s %8s %8s %8s  %s\n", "cfg", "pos",
                "f=0", "f=.015", "f=.020", "f=.030", "TiO2", "f_cross");
    for (int cfg = 1; cfg <= kNCfg; ++cfg) {
        for (int p = 0; p < 3; ++p) {
            std::printf("%-4d %-7s", cfg, kPosName[p]);
            for (double kf : keyF) std::printf(" %8.2f", meanAt(cfg, kf, p));
            if (tio2[cfg][p].ok)
                std::printf(" %8.2f", tio2[cfg][p].mean);
            else
                std::printf(" %8s", "n/a");
            std::printf("  %s\n",
                        crossingLabel(crossings[cfg][p], fmax).c_str());
        }
    }

    std::printf("\nCrossing fractions (bare contact falls below matrix TiO2):\n");
    for (int cfg = 1; cfg <= kNCfg; ++cfg)
        for (int p = 0; p < 3; ++p)
            std::printf("  cfg%d %-7s %s\n", cfg, kPosName[p],
                        crossingLabel(crossings[cfg][p], fmax).c_str());
    std::printf("\n");

    // ------------------------------------------------------------------
    // Figure: 2x2.  Panels 1-3 = absolute yield vs f for cfg 1-3;
    // panel 4 = relative yield at centre for all configs.
    // ------------------------------------------------------------------
    const int posColor[3] = {PetroffPalette::getP6(0),
                             PetroffPalette::getP6(1),
                             PetroffPalette::getP6(2)};
    const int cfgColor[kNCfg] = {PetroffPalette::getP6(0),
                                 PetroffPalette::getP6(1),
                                 PetroffPalette::getP6(2)};

    const double xlo = 0.;
    const double xhi = (fmax > 0.) ? fmax * 1.06 : 0.032;

    TCanvas* c = new TCanvas("c_contact_scan", "contact scan", 1150, 950);
    c->Divide(2, 2, 0.006, 0.006);

    for (int cfg = 1; cfg <= kNCfg; ++cfg) {
        c->cd(cfg);
        gPad->SetLeftMargin(0.17);
        gPad->SetRightMargin(0.04);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.12);

        double ymax = 0.;
        for (const auto& f : res[cfg])
            for (int p = 0; p < 3; ++p)
                if (f.second[p].stat.ok)
                    ymax = std::max(ymax, f.second[p].stat.mean +
                                               f.second[p].stat.sem);
        for (int p = 0; p < 3; ++p)
            if (tio2[cfg][p].ok) ymax = std::max(ymax, tio2[cfg][p].mean);
        if (ymax <= 0.) ymax = 1.;
        ymax *= 1.18;

        TH1D* frame = new TH1D(TString::Format("fr_cfg%d", cfg),
                               TString::Format("Config %d", cfg), 1, xlo, xhi);
        frame->SetMinimum(0.);
        frame->SetMaximum(ymax);
        frame->GetXaxis()->SetTitle("contact fraction f");
        frame->GetYaxis()->SetTitle("mean p.e. / event");
        frame->GetXaxis()->SetTitleSize(0.048);
        frame->GetYaxis()->SetTitleSize(0.048);
        frame->GetXaxis()->SetLabelSize(0.044);
        frame->GetYaxis()->SetLabelSize(0.044);
        frame->GetXaxis()->SetTitleOffset(1.15);
        frame->GetYaxis()->SetTitleOffset(1.35);
        frame->Draw("HIST");

        TLegend* leg = new TLegend(0.53, 0.60, 0.965, 0.86);
        leg->SetNColumns(2);
        leg->SetFillStyle(1001);
        leg->SetFillColor(kWhite);
        leg->SetTextSize(0.033);

        std::vector<TGraphErrors*> graphs(3, nullptr);
        for (int p = 0; p < 3; ++p) {
            std::vector<double> x, y, ey;
            for (const auto& f : res[cfg]) {
                const Result& r = f.second[p];
                if (!r.stat.ok) continue;
                x.push_back(f.first);
                y.push_back(r.stat.mean);
                ey.push_back(r.stat.sem);
            }
            if (x.empty()) continue;

            TGraphErrors* g = new TGraphErrors((int)x.size(), x.data(),
                                               y.data(), nullptr, ey.data());
            g->SetLineColor(posColor[p]);
            g->SetMarkerColor(posColor[p]);
            g->SetLineStyle(kSolid);
            g->SetLineWidth(2);
            g->SetMarkerStyle(kPosMarker[p]);
            g->SetMarkerSize(0.95);
            g->Draw("PL SAME");
            graphs[p] = g;

            if (tio2[cfg][p].ok) {
                TLine* line = new TLine(xlo, tio2[cfg][p].mean, xhi,
                                        tio2[cfg][p].mean);
                line->SetLineColor(posColor[p]);
                line->SetLineStyle(kDashed);
                line->SetLineWidth(2);
                line->Draw("SAME");
            }

            leg->AddEntry(g, kPosName[p], "lp");
        }
        for (int p = 0; p < 3; ++p) {
            if (!graphs[p] || !tio2[cfg][p].ok) continue;
            TLine* dummy = new TLine();
            dummy->SetLineColor(posColor[p]);
            dummy->SetLineStyle(kDashed);
            dummy->SetLineWidth(2);
            leg->AddEntry(dummy, TString::Format("TiO_{2} %s", kPosName[p]),
                          "l");
        }
        leg->Draw();

        TLatex* tex = new TLatex();
        tex->SetNDC();
        tex->SetTextSize(0.032);
        tex->SetTextAlign(11);
        for (int p = 0; p < 3; ++p) {
            tex->DrawLatex(0.20, 0.30 - 0.05 * p,
                           TString::Format("f_{cross} %s = %s", kPosName[p],
                                           crossingLabel(crossings[cfg][p],
                                                         fmax).c_str()));
        }
    }

    // Panel 4: relative yield Y(f)/Y(0) at centre, all configs.
    c->cd(4);
    gPad->SetLeftMargin(0.17);
    gPad->SetRightMargin(0.04);
    gPad->SetBottomMargin(0.16);
    gPad->SetTopMargin(0.12);

    double rmax = 1.;
    for (int cfg = 1; cfg <= kNCfg; ++cfg)
        for (const auto& f : res[cfg]) {
            const Result& r = f.second[0];
            if (r.stat.ok)
                rmax = std::max(rmax, r.ratio + r.ratioSem);
        }

    TH1D* frame4 = new TH1D("fr_relative", "Relative yield at centre", 1,
                            xlo, xhi);
    frame4->SetMinimum(0.);
    frame4->SetMaximum(std::max(1.10, rmax * 1.12));
    frame4->GetXaxis()->SetTitle("contact fraction f");
    frame4->GetYaxis()->SetTitle("Y(f) / Y(0)");
    frame4->GetXaxis()->SetTitleSize(0.048);
    frame4->GetYaxis()->SetTitleSize(0.048);
    frame4->GetXaxis()->SetLabelSize(0.044);
    frame4->GetYaxis()->SetLabelSize(0.044);
    frame4->GetXaxis()->SetTitleOffset(1.15);
    frame4->GetYaxis()->SetTitleOffset(1.35);
    frame4->Draw("HIST");

    TLine* unity = new TLine(xlo, 1., xhi, 1.);
    unity->SetLineColor(kGray + 2);
    unity->SetLineStyle(kDashed);
    unity->SetLineWidth(1);
    unity->Draw("SAME");

    TLegend* leg4 = new TLegend(0.50, 0.67, 0.965, 0.86);
    leg4->SetNColumns(2);
    leg4->SetFillStyle(1001);
    leg4->SetFillColor(kWhite);
    leg4->SetTextSize(0.033);
    leg4->AddEntry(unity, "unity", "l");

    for (int cfg = 1; cfg <= kNCfg; ++cfg) {
        std::vector<double> x, y, ey;
        for (const auto& f : res[cfg]) {
            const Result& r = f.second[0];
            if (!r.stat.ok) continue;
            x.push_back(f.first);
            y.push_back(r.ratio);
            ey.push_back(r.ratioSem);
        }
        if (x.empty()) continue;

        TGraphErrors* g = new TGraphErrors((int)x.size(), x.data(), y.data(),
                                           nullptr, ey.data());
        g->SetLineColor(cfgColor[cfg - 1]);
        g->SetMarkerColor(cfgColor[cfg - 1]);
        g->SetLineStyle(kSolid);
        g->SetLineWidth(2);
        g->SetMarkerStyle(kPosMarker[cfg - 1]);
        g->SetMarkerSize(0.95);
        g->Draw("PL SAME");
        leg4->AddEntry(g, TString::Format("config %d", cfg), "lp");
    }
    leg4->Draw();

    TLatex* tex4 = new TLatex();
    tex4->SetNDC();
    tex4->SetTextSize(0.032);
    tex4->SetTextAlign(11);
    tex4->DrawLatex(0.20, 0.25, "centre f_{cross}");
    tex4->DrawLatex(0.20, 0.20,
                    TString::Format(
                        "cfg1 %s   cfg2 %s   cfg3 %s",
                        crossingLabel(crossings[1][0], fmax).c_str(),
                        crossingLabel(crossings[2][0], fmax).c_str(),
                        crossingLabel(crossings[3][0], fmax).c_str()));

    for (const char* ext : {"pdf", "png"}) {
        const TString out = TString::Format("%s/contact_scan.%s", outdir, ext);
        c->SaveAs(out);
        std::cout << "contact_scan: wrote " << out << "\n";
    }
}
