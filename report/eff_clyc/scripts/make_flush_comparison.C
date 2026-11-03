// Compare the flush source-puck simulation with the printed CSV values.
//
// Reads the hits trees from
//   build/validation/clyc_sources_2cm/clyc_source_*_1mm.root
// and extracts the full-energy peak efficiency in the same +-2% window used
// by scripts/analyze_clyc_sources_2cm.C.  It writes
//   report/eff_clyc/figures/fig_flush_comparison.pdf
//   report/eff_clyc/data/flush_source_eff.csv
//
// The source definitions and the CSV efficiencies are the same as in
// analyze_clyc_sources_2cm.C.  The CSV column used here is the printed
// "Efficiency (%)" column of eff_clyc1.csv, not the separate
// "Efficiency Reference Value (%)" column.
//
// Usage (from the repository root):
//   root -l -b -q report/eff_clyc/scripts/make_flush_comparison.C
//
// The intrinsic efficiencies in the CSV and in panel (b) are in percent:
//   eps_int_pct = 100 * eps_pct / eps_Omega_pct.

#include <TCanvas.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
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
    double eff_csv_pct;
    const char* label;
};

struct Source {
    const char* name;
    const char* title;
    std::vector<Line> lines;
};

struct Peak {
    long long n;
    double eff_pct;
    double err_pct;
};

double geometricAcceptancePct(double d_mm, double r_mm = 12.5)
{
    return 100. * 0.5 * (1. - d_mm / std::sqrt(d_mm * d_mm + r_mm * r_mm));
}

// Same pulse definition as scripts/analyze_clyc_sources_2cm.C.  The hits
// tree stores one row per shower pulse; merge rows whose leading-edge times
// are separated by at most 1 us.  Valid only for unit-weight source runs.
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

TString SourceFileName(const char* inDir, const char* name, const char* tag)
{
    if (TString(name) == "Am241") {
        // Pulse-clustering study uses the archived 1e60-year Am-241 files.
        return TString::Format(
            "%s/superseded_thr1e60y/clyc_source_Am241_%s.root",
            inDir, tag);
    }
    return TString::Format("%s/clyc_source_%s_%s.root", inDir, name, tag);
}

std::vector<Source> MakeSources()
{
    return {
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
}

std::map<double, Peak> ReadSource(const char* fname, const Source& src,
                                  long long Ndecays)
{
    std::map<double, Peak> out;

    TFile* f = TFile::Open(fname, "READ");
    if (!f || f->IsZombie()) {
        std::printf("ERROR: cannot open %s\n", fname);
        if (f) f->Close();
        return out;
    }

    TTree* t = static_cast<TTree*>(f->Get("hits"));
    if (!t) {
        std::printf("ERROR: no hits tree in %s\n", fname);
        f->Close();
        return out;
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
                         fname, weight);
            std::exit(1);
        }
        const long long ev = static_cast<long long>(std::llround(evtNb));
        eventHits[ev].emplace_back(time, edep * weight);
    }

    std::map<long long, std::vector<double>> eventPulses;
    for (auto& kv : eventHits) {
        eventPulses[kv.first] = ClusterPulses(kv.second);
    }

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
        out[line.E_keV] = {nPeak, eff, err};
    }

    f->Close();
    return out;
}

void SetStyle()
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetTextFont(42);
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetTitleSize(0.050, "XYZ");
    gStyle->SetLabelSize(0.044, "XYZ");
    gStyle->SetFrameLineWidth(1);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
}

void StyleGraph(TGraph* g, int colour, int marker, double markerSize,
                bool withLine)
{
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(markerSize);
    g->SetMarkerColor(colour);
    g->SetLineColor(colour);
    g->SetLineWidth(withLine ? 2 : 0);
}

void SaveCrop(TCanvas* c, const char* out)
{
    TString raw(out);
    raw.ReplaceAll(".pdf", "_raw.pdf");
    c->SaveAs(raw);
    gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                       raw.Data(), out));
    gSystem->Unlink(raw);
}

} // namespace

void make_flush_comparison(long long Ndecays = 1000000)
{
    SetStyle();

    const char* inDir = "build/validation/clyc_sources_2cm";
    const char* outDir = "report/eff_clyc/figures";
    const char* dataDir = "report/eff_clyc/data";
    gSystem->mkdir(outDir, true);
    gSystem->mkdir(dataDir, true);

    const double dFlush = 3.0;
    const double accFlush = geometricAcceptancePct(dFlush); // 38.331 %

    const std::vector<Source> sources = MakeSources();

    struct Row {
        const Source* src;
        const Line* line;
        Peak pFlush;
    };
    std::vector<Row> rows;

    for (const auto& src : sources) {
        const TString ffl = SourceFileName(inDir, src.name, "1mm");
        const std::map<double, Peak> rfl = ReadSource(ffl, src, Ndecays);
        if (rfl.empty()) {
            std::printf("ERROR: missing input for %s; stopping.\n", src.name);
            return;
        }
        for (const auto& line : src.lines) {
            rows.push_back({&src, &line, rfl.at(line.E_keV)});
        }
    }

    std::sort(rows.begin(), rows.end(),
              [](const Row& a, const Row& b) {
                  return a.line->E_keV < b.line->E_keV;
              });

    // CSV summary used to fill the LaTeX table.
    const TString csvName = TString::Format("%s/flush_source_eff.csv", dataDir);
    FILE* fp = std::fopen(csvName, "w");
    if (!fp) {
        std::printf("ERROR: cannot write %s\n", csvName.Data());
        return;
    }
    std::fprintf(fp,
                 "source,E_keV,P_gamma,N_peak_flush,eps_sim_flush,"
                 "eps_CSV,eps_int_sim_flush,eps_int_CSV\n");

    const int n = static_cast<int>(rows.size());
    std::vector<double> x(n), yfl(n), ycsv(n);
    std::vector<double> eyfl(n);
    std::vector<double> yifl(n), yicsv(n);

    std::printf("\n%-6s %9s %8s %10s %12s %10s %12s\n",
                "source", "E_keV", "P_gamma", "N_peak_fl", "eps_fl(%)",
                "eps_CSV", "R_flush");

    for (int i = 0; i < n; ++i) {
        const Row& r = rows[i];
        x[i] = r.line->E_keV;
        yfl[i] = r.pFlush.eff_pct;
        ycsv[i] = r.line->eff_csv_pct;
        eyfl[i] = r.pFlush.err_pct;

        yifl[i] = 100. * r.pFlush.eff_pct / accFlush;
        yicsv[i] = 100. * r.line->eff_csv_pct / accFlush;

        const double ratio = r.line->eff_csv_pct > 0.
            ? r.pFlush.eff_pct / r.line->eff_csv_pct : 0.;

        std::fprintf(fp,
                     "%s,%.2f,%.4f,%lld,%.4f,%.2f,%.4f,%.4f\n",
                     r.src->name, r.line->E_keV, r.line->P_gamma,
                     r.pFlush.n, r.pFlush.eff_pct,
                     r.line->eff_csv_pct, yifl[i], yicsv[i]);

        std::printf("%-6s %9.2f %8.4f %10lld %12.4f %10.2f %12.3f\n",
                    r.src->name, r.line->E_keV, r.line->P_gamma,
                    r.pFlush.n, r.pFlush.eff_pct,
                    r.line->eff_csv_pct, ratio);
    }
    std::fclose(fp);
    std::printf("\nWrote %s\n", csvName.Data());

    TGraphErrors* gfl = new TGraphErrors(n, x.data(), yfl.data(), nullptr,
                                         eyfl.data());
    TGraph* gcsv = new TGraph(n, x.data(), ycsv.data());
    gfl->SetTitle(";#font[132]{#it{E}}_{#gamma} [keV];"
                  "#font[132]{#it{#epsilon}} [%]");
    StyleGraph(gfl, kGreen + 2, 21, 1.0, true);
    StyleGraph(gcsv, kRed + 1, 24, 1.15, false);

    TGraphErrors* gifl = new TGraphErrors(n, x.data(), yifl.data(), nullptr,
                                          eyfl.data());
    TGraph* gicsv = new TGraph(n, x.data(), yicsv.data());
    gifl->SetTitle(";#font[132]{#it{E}}_{#gamma} [keV];"
                   "#font[132]{#it{#epsilon}}_{int} [%]");
    StyleGraph(gifl, kGreen + 2, 21, 1.0, true);
    StyleGraph(gicsv, kRed + 1, 24, 1.15, false);

    TCanvas c("c_flush_comparison", "", 1500, 650);
    c.Divide(2, 1, 0.001, 0.001);

    auto preparePad = [](double left) {
        gPad->SetLogx();
        gPad->SetLogy();
        gPad->SetLeftMargin(left);
        gPad->SetRightMargin(0.04);
        gPad->SetBottomMargin(0.17);
        gPad->SetTopMargin(0.07);
    };

    // Panel (a): raw full-energy peak efficiency.
    c.cd(1);
    preparePad(0.14);
    gfl->GetXaxis()->SetLimits(20., 2000.);
    gfl->SetMinimum(0.3);
    gfl->SetMaximum(60.);
    gfl->GetXaxis()->CenterTitle();
    gfl->GetXaxis()->SetTitleOffset(1.20);
    gfl->GetYaxis()->SetTitleOffset(1.30);
    gfl->Draw("APL");
    gcsv->Draw("P same");

    TLine* lfl = new TLine(20., accFlush, 2000., accFlush);
    lfl->SetLineColor(kGray + 1);
    lfl->SetLineStyle(7);
    lfl->SetLineWidth(2);
    lfl->Draw("same");

    TLegend la(0.16, 0.19, 0.52, 0.36);
    la.SetBorderSize(0);
    la.SetFillStyle(0);
    la.SetTextSize(0.034);
    la.AddEntry(gfl, "#font[132]{#it{#epsilon}}_{sim,flush}", "pl");
    la.AddEntry(gcsv, "#font[132]{#it{#epsilon}}_{CSV}", "p");
    la.Draw();

    // Label the acceptance lines directly, below each line so that the
    // dashed line does not run through the text.  The labels sit in the
    // otherwise empty lower-left corner at low x.
    TLatex pl;
    pl.SetTextSize(0.028);
    pl.SetTextColor(kGray + 2);
    pl.SetTextAlign(13); // left, top
    pl.DrawLatex(26., accFlush * 0.92, Form("%.2f%%", accFlush));

    TLatex pa;
    pa.SetNDC();
    pa.SetTextSize(0.035);
    pa.SetTextAlign(33); // right, top
    pa.DrawLatex(0.94, 0.96, "(a)");

    // Panel (b): intrinsic efficiency.
    c.cd(2);
    preparePad(0.13);
    gifl->GetXaxis()->SetLimits(20., 2000.);
    gifl->SetMinimum(0.5);
    gifl->SetMaximum(120.);
    gifl->GetXaxis()->CenterTitle();
    gifl->GetXaxis()->SetTitleOffset(1.20);
    gifl->GetYaxis()->SetTitleOffset(1.30);
    gifl->Draw("APL");
    gicsv->Draw("P same");

    TLegend lb(0.15, 0.19, 0.55, 0.38);
    lb.SetBorderSize(0);
    lb.SetFillStyle(0);
    lb.SetTextSize(0.034);
    lb.AddEntry(gifl, "#font[132]{#it{#epsilon}}_{int,sim,flush}", "pl");
    lb.AddEntry(gicsv, "#font[132]{#it{#epsilon}}_{int,CSV}", "p");
    lb.Draw();

    TLatex pb;
    pb.SetNDC();
    pb.SetTextSize(0.035);
    pb.SetTextAlign(33); // right, top
    pb.DrawLatex(0.94, 0.88, "(b)");

    SaveCrop(&c, Form("%s/fig_flush_comparison.pdf", outDir));
    std::printf("Wrote %s/fig_flush_comparison.pdf\n", outDir);
}
