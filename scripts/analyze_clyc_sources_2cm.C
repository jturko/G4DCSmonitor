// CLYC source spectra and full-energy peak efficiencies at a fixed distance.
//
// Reads the hits tree from the radioactive-decay runs, sums the energy
// deposited in the CLYC crystal (det == 0) per event, writes one spectrum per
// source and a combined spectrum figure, and extracts the full-energy peak
// efficiency for the lines listed in eff_clyc1.csv.
//
// Usage:
//   root -l -b -q 'scripts/analyze_clyc_sources_2cm.C(10000000,"20mm")'
//   root -l -b -q 'scripts/analyze_clyc_sources_2cm.C(1000000,"1mm",3.0,"_flush")'
//
// Arguments:
//   Ndecays     number of ion decays per source (default 1e7)
//   distTag     distance tag used in the input file names (default "20mm")
//   dCrystalMM  source-to-crystal-front distance in mm (default 22.0)
//   outSuffix   suffix appended to the output figure names (default "")

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TH1D.h>
#include <TLatex.h>
#include <TLegend.h>
#include <TLine.h>
#include <TStyle.h>
#include <TString.h>
#include <TSystem.h>
#include <TTree.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <utility>
#include <vector>

namespace {

struct Line {
    double E_keV;
    double P_gamma;
    double eff_meas_pct;
    const char* label;
};

struct Source {
    const char* name;
    const char* title;
    std::vector<Line> lines;
};

struct Point {
    double E_keV;
    double eff_sim_pct;
    double eff_sim_err_pct;
    double eff_meas_pct;
};

double geometricAcceptancePct(double d_mm, double r_mm = 12.5)
{
    return 100. * 0.5 * (1. - d_mm / std::sqrt(d_mm * d_mm + r_mm * r_mm));
}

// DCSMonitorSD uses 1 us for CLYC (DetectorConstruction.cc:599).  The hits
// tree stores one row per shower pulse; merge rows whose leading-edge times
// are separated by at most this gap, so that coincident photons from one
// decay are summed into one detector pulse.
constexpr double kResolvingTimeNs = 1000.0;

std::vector<double> ClusterPulses(std::vector<std::pair<double, double>>& hits)
{
    std::sort(hits.begin(), hits.end(),
              [](const std::pair<double, double>& a,
                 const std::pair<double, double>& b) { return a.first < b.first; });

    std::vector<double> pulses;
    std::size_t i = 0;
    while (i < hits.size()) {
        double sumE = hits[i].second;
        double tPrev = hits[i].first;
        ++i;
        while (i < hits.size() &&
               (hits[i].first - tPrev) <= kResolvingTimeNs) {
            sumE += hits[i].second;
            tPrev = hits[i].first;
            ++i;
        }
        pulses.push_back(sumE);
    }
    return pulses;
}

void SaveCrop(TCanvas* c, const char* out)
{
    TString raw(out);
    raw.ReplaceAll(".pdf", "_raw.pdf");
    c->SaveAs(raw);
    gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                       raw.Data(), out));
    gSystem->Unlink(raw);
    TString alt(out);
    if (alt.Contains("report/eff_clyc/figures/")) {
        alt.ReplaceAll("report/eff_clyc/figures/",
                       "report/eff_sources_2cm/figures/");
        gSystem->CopyFile(out, alt, kTRUE);
    }
}

void SetStyle()
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetTextFont(42);
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetTitleSize(0.045, "XYZ");
    gStyle->SetLabelSize(0.040, "XYZ");
    gStyle->SetFrameLineWidth(1);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
}

} // namespace

void analyze_clyc_sources_2cm(long long Ndecays = 10000000,
                              const char* distTag = "20mm",
                              double dCrystalMM = 22.0,
                              const char* outSuffix = "")
{
    SetStyle();

    const char* inDir = "build/validation/clyc_sources_2cm";
    const char* outDir = "report/eff_clyc/figures";
    gSystem->mkdir(outDir, true);
    gSystem->mkdir("report/eff_sources_2cm/figures", true);

    const std::vector<Source> sources = {
        {"Cs137", "^{137}Cs", {{32.00, 0.0900, 18.50, "32.0"},
                               {661.64, 0.8510, 1.87, "661.6"}}},
        {"Pb210", "^{210}Pb", {{46.54, 0.0425, 23.85, "46.5"}}},
        {"Am241", "^{241}Am", {{59.54, 0.3530, 23.80, "59.5"}}},
        {"Eu152", "^{152}Eu", {{121.78, 0.2880, 15.40, "121.8"},
                               {344.28, 0.2650, 4.87, "344.3"},
                               {1408.01, 0.2100, 0.40, "1408"}}},
        {"Co60", "^{60}Co", {{1173.23, 0.9997, 0.51, "1173"},
                             {1332.51, 0.9999, 0.56, "1333"}}}
    };

    std::vector<Point> points;
    std::map<TString, TH1D*> spectra;

    for (const auto& src : sources) {
        TString fname = TString::Format("%s/clyc_source_%s_%s.root",
                                        inDir, src.name, distTag);
        // The Am-241 files in the main directory are the 1e4-year
        // cross-check.  This pulse-clustering study uses the archived
        // 1e60-year Am-241 files, as requested.
        if (TString(src.name) == "Am241") {
            fname = TString::Format(
                "%s/superseded_thr1e60y/clyc_source_Am241_%s.root",
                inDir, distTag);
        }
        TFile* f = TFile::Open(fname, "READ");
        if (!f || f->IsZombie()) {
            std::printf("ERROR: cannot open %s\n", fname.Data());
            continue;
        }

        TTree* t = static_cast<TTree*>(f->Get("hits"));
        if (!t) {
            std::printf("ERROR: no hits tree in %s\n", fname.Data());
            f->Close();
            continue;
        }

        Double_t edep = 0., evtNb = 0., weight = 0., time = 0.;
        Int_t det = -1;
        t->SetBranchAddress("edep", &edep);
        t->SetBranchAddress("evtNb", &evtNb);
        t->SetBranchAddress("weight", &weight);
        t->SetBranchAddress("det", &det);
        t->SetBranchAddress("t", &time);

        std::map<long long, std::vector<std::pair<double, double>>> eventHits;
        const Long64_t nEntries = t->GetEntries();
        for (Long64_t i = 0; i < nEntries; ++i) {
            t->GetEntry(i);
            if (det != 0) continue;
            if (weight <= 0.) continue;
            if (std::abs(weight - 1.0) > 1e-12) {
                std::fprintf(stderr,
                             "ERROR: %s has det==0 weight %.12g; "
                             "cross-shower pulse merging is only valid for "
                             "unit-weight source runs.\n",
                             fname.Data(), weight);
                std::exit(1);
            }
            const long long ev = static_cast<long long>(std::llround(evtNb));
            eventHits[ev].emplace_back(time, edep * weight);
        }

        std::map<long long, std::vector<double>> eventPulses;
        for (auto& kv : eventHits) {
            eventPulses[kv.first] = ClusterPulses(kv.second);
        }

        TH1D* h = new TH1D(TString::Format("h_%s", src.name),
                           TString::Format("%s;#font[132]{#it{E}}_{dep} [MeV];"
                                           "pulses / 1 keV", src.title),
                           3000, 0., 3.0);
        for (const auto& kv : eventPulses) {
            for (double pulseE : kv.second) h->Fill(pulseE);
        }
        h->SetDirectory(nullptr);   // do not let TFile::Close delete the histogram
        spectra[src.name] = h;

        // Individual spectrum.
        const double ymax = h->GetBinContent(h->GetMaximumBin()) * 10.;
        TCanvas c(Form("c_%s", src.name), "", 900, 550);
        c.SetLogy();
        c.SetLeftMargin(0.13);
        c.SetTopMargin(0.11);
        h->SetLineColor(kBlue + 1);
        h->SetMinimum(0.5);
        h->SetMaximum(ymax);
        h->GetXaxis()->CenterTitle();
        h->Draw("HIST");

        for (const auto& line : src.lines) {
            const double E = line.E_keV / 1000.;
            TLine* l = new TLine(E, 0.5, E, ymax);
            l->SetLineColor(kGray + 2);
            l->SetLineStyle(2);
            l->Draw("same");

            TLatex lab;
            lab.SetTextSize(0.028);
            lab.SetTextAngle(90);
            lab.SetTextAlign(32);
            lab.SetTextColor(kGray + 3);
            lab.DrawLatex(E + 0.05, ymax * 0.8, line.label);
        }

        TLatex name;
        name.SetNDC();
        name.SetTextSize(0.045);
        name.DrawLatex(0.55, 0.81, src.title);

        TLatex info;
        info.SetNDC();
        info.SetTextSize(0.030);
        info.DrawLatex(0.55, 0.73, Form("#font[132]{#it{N}}_{decays} = %lld", Ndecays));
        info.DrawLatex(0.55, 0.67,
                       Form("source puck, d_{nom} = %.0f mm",
                            dCrystalMM - 2.0));
        info.DrawLatex(0.55, 0.61, "1 #mus pulse clustering");
        SaveCrop(&c, Form("%s/fig_spectrum_%s%s.pdf", outDir, src.name,
                          outSuffix));

        // Peak counts and efficiencies for the lines in eff_clyc1.csv.
        for (const auto& line : src.lines) {
            const double E_MeV = line.E_keV / 1000.;
            const double lo = E_MeV * 0.98;
            const double hi = E_MeV * 1.02;
            long long nPeak = 0;
            for (const auto& kv : eventPulses) {
                for (double pulseE : kv.second) {
                    if (pulseE >= lo && pulseE <= hi) ++nPeak;
                }
            }

            const double denom = static_cast<double>(Ndecays) * line.P_gamma;
            const double eff = denom > 0. ? 100. * nPeak / denom : 0.;
            const double err = denom > 0.
                ? 100. * std::sqrt(static_cast<double>(nPeak)) / denom : 0.;

            std::printf("%-6s %8.2f keV  P=%.4f  N_peak=%8lld  "
                        "eff_sim=%8.4f +/- %.4f %%  eff_csv=%6.2f %%\n",
                        src.name, line.E_keV, line.P_gamma, nPeak,
                        eff, err, line.eff_meas_pct);

            points.push_back({line.E_keV, eff, err, line.eff_meas_pct});
        }

        f->Close();
    }

    // Combined spectra figure.
    TCanvas call("c_source_spectra", "", 1500, 1200);
    call.Divide(2, 3, 0.01, 0.01);
    int iPad = 1;
    for (const auto& src : sources) {
        auto it = spectra.find(src.name);
        if (it == spectra.end()) continue;
        call.cd(iPad++);
        gPad->SetLogy();
        gPad->SetLeftMargin(0.17);
        gPad->SetRightMargin(0.05);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.15);
        TH1D* h = it->second;
        const double ymax = h->GetBinContent(h->GetMaximumBin()) * 10.;
        // The isotope label is drawn in the top margin below, not as the
        // histogram title (which would be centred on the top frame line).
        h->SetTitle(";#font[132]{#it{E}}_{dep} [MeV];pulses / 1 keV");
        h->SetLineColor(kBlue + 1);
        h->SetMinimum(0.5);
        h->SetMaximum(ymax);
        h->GetXaxis()->SetTitleSize(0.070);
        h->GetYaxis()->SetTitleSize(0.070);
        h->GetXaxis()->SetLabelSize(0.060);
        h->GetYaxis()->SetLabelSize(0.060);
        h->GetXaxis()->SetTitleOffset(1.05);
        h->GetYaxis()->SetTitleOffset(1.05);
        h->GetXaxis()->CenterTitle();
        h->GetYaxis()->CenterTitle();
        h->Draw("HIST");
        for (const auto& line : src.lines) {
            const double E = line.E_keV / 1000.;
            TLine* l = new TLine(E, 0.5, E, ymax);
            l->SetLineColor(kGray + 2);
            l->SetLineStyle(2);
            l->Draw("same");

            // Label inside the frame, to the right of the marker line and
            // ending just below the top frame line (text runs downward).
            TLatex lab;
            lab.SetTextSize(0.055);
            lab.SetTextAngle(90);
            lab.SetTextAlign(32);
            lab.SetTextColor(kGray + 3);
            lab.DrawLatex(E + 0.065, ymax * 0.8, line.label);
        }
        // Isotope label in the top margin, centred over the frame.
        TLatex name;
        name.SetNDC();
        name.SetTextSize(0.085);
        name.SetTextAlign(21);
        name.DrawLatex(0.17 + 0.5 * (1. - 0.17 - 0.05), 0.865, src.title);
    }

    // Sixth cell: notes.
    call.cd(6);
    {
        TLatex note;
        note.SetNDC();
        note.SetTextAlign(12);
        note.SetTextSize(0.062);
        note.DrawLatex(0.12, 0.74, "1 #mus pulse clustering");
        note.DrawLatex(0.12, 0.60,
                       Form("#font[132]{#it{N}}_{decays} = %.0f#times10^{%d}",
                            (double)Ndecays / std::pow(10., std::floor(std::log10((double)Ndecays))),
                            (int)std::floor(std::log10((double)Ndecays))));
        note.DrawLatex(0.12, 0.46,
                       Form("source puck, #font[132]{#it{d}}_{nom} = %.0f mm",
                            dCrystalMM - 2.0));
        note.SetTextSize(0.052);
        note.SetTextColor(kGray + 2);
        note.DrawLatex(0.12, 0.30, "dashed lines: #gamma-ray energies [keV]");
    }
    SaveCrop(&call, Form("%s/fig_source_spectra%s.pdf", outDir, outSuffix));

    // Efficiency curve, sorted by energy.
    std::sort(points.begin(), points.end(),
              [](const Point& a, const Point& b) { return a.E_keV < b.E_keV; });

    std::vector<double> x, y, ey, ym;
    for (const auto& p : points) {
        x.push_back(p.E_keV);
        y.push_back(p.eff_sim_pct);
        ey.push_back(p.eff_sim_err_pct);
        ym.push_back(p.eff_meas_pct);
    }

    TGraphErrors* gsim = new TGraphErrors(static_cast<int>(x.size()),
                                          x.data(), y.data(), nullptr, ey.data());
    gsim->SetTitle(";#font[132]{#it{E}}_{#gamma} [keV];#font[132]{#it{#epsilon}} [%]");
    gsim->SetMarkerStyle(20);
    gsim->SetMarkerSize(1.0);
    gsim->SetMarkerColor(kBlue + 1);
    gsim->SetLineColor(kBlue + 1);
    gsim->SetLineWidth(2);

    TGraph* gmeas = new TGraph(static_cast<int>(x.size()), x.data(), ym.data());
    gmeas->SetMarkerStyle(24);
    gmeas->SetMarkerSize(1.1);
    gmeas->SetMarkerColor(kRed + 1);
    gmeas->SetLineColor(kRed + 1);

    TCanvas ceff("c_eff_2cm", "", 900, 650);
    ceff.SetLogx();
    ceff.SetLogy();
    ceff.SetLeftMargin(0.14);
    ceff.SetRightMargin(0.04);
    ceff.SetBottomMargin(0.13);
    ceff.SetTopMargin(0.06);
    ceff.SetFrameLineWidth(1);

    gsim->GetXaxis()->SetTitleOffset(1.15);
    gsim->GetYaxis()->SetTitleOffset(1.30);
    gsim->GetXaxis()->SetLimits(20., 2000.);
    gsim->SetMinimum(0.05);
    gsim->SetMaximum(50.);
    gsim->Draw("APL");
    gmeas->Draw("P same");

    const double acc = geometricAcceptancePct(dCrystalMM);
    TLine* gacc = new TLine(20., acc, 2000., acc);
    gacc->SetLineColor(kGray + 2);
    gacc->SetLineStyle(2);
    gacc->SetLineWidth(2);
    gacc->Draw("same");

    TLegend leg(0.51, 0.73, 0.91, 0.90);
    leg.SetBorderSize(0);
    leg.SetFillStyle(0);
    leg.SetTextSize(0.032);
    leg.AddEntry(gsim, "#font[132]{#it{#epsilon}}_{sim}", "p");
    leg.AddEntry(gmeas, "#font[132]{#it{#epsilon}}_{CSV}", "p");
    leg.AddEntry(gacc, Form("#font[132]{#it{#epsilon}}_{#Omega} = %.2f%%", acc), "l");
    leg.Draw();

    SaveCrop(&ceff, Form("%s/fig_eff_vs_energy_2cm%s.pdf", outDir, outSuffix));
}
