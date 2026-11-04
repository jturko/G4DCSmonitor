// Simulated vs experimental CLYC2 spectra at 200 mm.
//
// The experimental spectra are the 2026-10-27 CNF files converted to ROOT:
//   Meas1_200_CS_Clyc2.root, Meas1_200_Co_Clyc2.root, Hintergrund.root
// The plots use the converter's already-calibrated counts_E histogram (one
// native ~1.96 keV bin per channel) and apply only a linear gain correction
// to the bin edges.  Rebinding the raw 1.96 keV channels into 2 keV bins by
// bin centre aliases the two grids and produces periodic single-bin spikes;
// that is avoided here.
//
// The simulation spectra are the 15 deg forward-cone runs:
//   clyc_eff_200mm_{661p64,1173p23,1332p51}keV_cone.root
// Each event is the sum of the CLYC hits with the same evtNb, smeared with
// the measured resolution FWHM(E) = (b/100)*sqrt(E), and scaled to the
// experimental live time and the decay-corrected source activity:
//   N_emit = A_exact * P_gamma * t_live / 100
//   scale  = N_emit * f_cone / N_gen,  f_cone = (1-cos(15 deg))/2
//
// The Co-60 simulation is the sum of the 1173.23 and 1332.51 keV runs, each
// scaled by its own branching ratio.  The 2505.74 keV sum peak is not present
// in these monoenergetic runs, and the experimental surroundings are not in
// the simulation, so the continuum comparison is qualitative.
//
// Run from the repository root:
//   root -l -b -q report/eff_clyc/scripts/make_exp_sim_figs.C
//
// Outputs:
//   report/eff_clyc/figures/fig_exp_sim_200mm_CS.pdf
//   report/eff_clyc/figures/fig_exp_sim_200mm_Co.pdf

#include <TFile.h>
#include <TH1D.h>
#include <TH2F.h>
#include <TTree.h>
#include <TGraphErrors.h>
#include <TCanvas.h>
#include <TF1.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TLine.h>
#include <TBox.h>
#include <TStyle.h>
#include <TString.h>
#include <TSystem.h>
#include <TMath.h>
#include <TRandom3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kConeHalfAngleDeg = 15.;
const double kConeFraction =
    0.5 * (1. - std::cos(kConeHalfAngleDeg * TMath::DegToRad()));

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

struct ExpRow {
    std::string source;
    int distance = 0;
    double energy = 0.;
    double aExact = 0.;
    double pGamma = 0.;
    double tLive = 0.;
    double fwhm = 0.;
    double calA = 0.;
    double calB = 1.;
};

std::vector<ExpRow> ReadPeakCsv(const char* path)
{
    std::vector<ExpRow> rows;
    std::ifstream in(path);
    if (!in.is_open()) {
        std::printf("ERROR: cannot open %s\n", path);
        return rows;
    }
    std::string line;
    std::getline(in, line);
    while (std::getline(in, line)) {
        if (Trim(line).empty()) continue;
        const auto f = Split(line);
        if (f.size() < 41) continue;
        ExpRow r;
        r.source = f[2];
        r.distance = std::atoi(f[1].c_str());
        r.energy = std::atof(f[3].c_str());
        r.aExact = std::atof(f[6].c_str());
        r.pGamma = std::atof(f[8].c_str());
        r.tLive = std::atof(f[9].c_str());
        r.fwhm = std::atof(f[35].c_str());
        r.calA = std::atof(f[39].c_str());
        r.calB = std::atof(f[40].c_str());
        rows.push_back(r);
    }
    return rows;
}

const ExpRow* FindRow(const std::vector<ExpRow>& rows, const std::string& source,
                      int distance, double energy)
{
    for (const auto& r : rows) {
        if (r.source == source && r.distance == distance &&
            std::fabs(r.energy - energy) < 0.01) {
            return &r;
        }
    }
    return nullptr;
}

double ReadResolutionB(const char* path)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        std::printf("ERROR: cannot open %s\n", path);
        return 0.;
    }
    std::string line;
    std::getline(in, line); // header
    if (!std::getline(in, line)) return 0.;
    const auto f = Split(line);
    return f.empty() ? 0. : std::atof(f[0].c_str());
}

// Read the converter's counts_E histogram and apply a linear gain correction
// to the bin edges.  The counts and their Poisson errors are unchanged.
TH1D* CorrectedCountsE(const char* file, double calA, double calB,
                       const char* name)
{
    TFile f(file);
    if (f.IsZombie()) {
        std::printf("ERROR: cannot open %s\n", file);
        return nullptr;
    }
    auto* h = static_cast<TH1D*>(f.Get("counts_E"));
    if (!h) {
        std::printf("ERROR: no counts_E in %s\n", file);
        return nullptr;
    }
    const int n = h->GetNbinsX();
    std::vector<double> edges(n + 1);
    for (int i = 0; i < n; ++i) {
        edges[i] = calA + calB * h->GetXaxis()->GetBinLowEdge(i + 1);
    }
    edges[n] = calA + calB * h->GetXaxis()->GetBinUpEdge(n);

    TH1D* out = new TH1D(name, "", n, edges.data());
    out->SetDirectory(nullptr);
    for (int i = 1; i <= n; ++i) {
        const double c = h->GetBinContent(i);
        out->SetBinContent(i, c);
        out->SetBinError(i, std::sqrt(c));
    }
    return out;
}

struct SimSpectrum {
    TH1D* hist = nullptr;
    double nPeak = 0.;
};

// Sum the hits tree per event, apply Gaussian resolution smearing, and fill a
// histogram with the same native energy bins as the experimental spectrum.
SimSpectrum BuildSim(const char* path, double Ngen, double scale, double b,
                     double eNom, TH1D* tmpl, unsigned seed, bool smear)

{
    SimSpectrum out;
    TFile f(path);
    if (f.IsZombie()) {
        std::printf("ERROR: cannot open %s\n", path);
        return out;
    }
    auto* t = static_cast<TTree*>(f.Get("hits"));
    if (!t) {
        std::printf("ERROR: no hits tree in %s\n", path);
        return out;
    }
    Double_t edep = 0., weight = 0., evt = 0.;
    Int_t det = -1;
    t->SetBranchAddress("edep", &edep);
    t->SetBranchAddress("weight", &weight);
    t->SetBranchAddress("det", &det);
    t->SetBranchAddress("evtNb", &evt);

    std::map<long long, double> eventEdep;
    for (Long64_t i = 0; i < t->GetEntries(); ++i) {
        t->GetEntry(i);
        if (det != 0 || weight <= 0.) continue;
        const long long ev = static_cast<long long>(std::llround(evt));
        eventEdep[ev] += edep * weight;
    }

    out.hist = static_cast<TH1D*>(tmpl->Clone(
        Form("hsim_%.2f_%u", eNom, seed)));
    out.hist->SetDirectory(nullptr);
    out.hist->Reset();
    out.hist->GetXaxis()->SetTitle("#font[132]{#it{E}}_{dep} [keV]");
    out.hist->GetYaxis()->SetTitle("counts / bin");

    TRandom3 rng(seed);
    const double lo = 0.98 * eNom;
    const double hi = 1.02 * eNom;
    for (const auto& kv : eventEdep) {
        const double e = 1000. * kv.second; // MeV -> keV
        if (e <= 0.) continue;
        const double fwhm = b / 100. * std::sqrt(e);
        const double sigma = fwhm / 2.354820045;
        const double es = smear ? rng.Gaus(e, sigma) : e;
        if (es < tmpl->GetXaxis()->GetXmin() ||
            es >= tmpl->GetXaxis()->GetXmax()) {
            continue;
        }
        out.hist->Fill(es, scale);
        if (es >= lo && es <= hi) out.nPeak += 1.;
    }
    out.nPeak *= scale;
    return out;
}

void Style()
{
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(0);
    gStyle->SetTextFont(42);
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetTitleSize(0.050, "XYZ");
    gStyle->SetLabelSize(0.045, "XYZ");
    gStyle->SetFrameLineWidth(1);
}

void StylePad(TVirtualPad* p)
{
    p->SetLeftMargin(0.16);
    p->SetRightMargin(0.04);
    p->SetBottomMargin(0.17);
    p->SetTopMargin(0.07);
    p->SetTickx(1);
    p->SetTicky(1);
}

struct SimDef {
    const char* file;
    double scale;
    double eNom;
    unsigned seed;
};

// Build one figure: top = raw, background-subtracted, smeared simulation,
// unsmeared simulation; bottom = background-subtracted / smeared simulation
// over the same x range.  The two pads share one x-axis.
void MakeFigure(const char* outPdf, const char* expFile, const char* bkgFile,
                const ExpRow& row, const std::vector<SimDef>& simDefs,
                double b, double x0, double x1, const char* label)
{
    Style();
    TH1D* raw = CorrectedCountsE(expFile, row.calA, row.calB,
                                 Form("hraw_%s", label));
    TH1D* bkg = CorrectedCountsE(bkgFile, row.calA, row.calB,
                                 Form("hbkg_%s", label));
    if (!raw || !bkg) return;

    TH1D* net = static_cast<TH1D*>(raw->Clone(Form("hnet_%s", label)));
    net->SetDirectory(nullptr);
    for (int i = 1; i <= net->GetNbinsX(); ++i) {
        const double nm = raw->GetBinContent(i);
        const double nbk = bkg->GetBinContent(i);
        net->SetBinContent(i, nm - nbk);
        net->SetBinError(i, std::sqrt(nm + std::max(0., nbk)));
    }

    std::vector<SimSpectrum> sims;
    std::vector<SimSpectrum> simsRaw;
    for (const auto& d : simDefs) {
        sims.push_back(BuildSim(d.file, 1e7, d.scale, b, d.eNom, raw,
                                d.seed, true));
        simsRaw.push_back(BuildSim(d.file, 1e7, d.scale, b, d.eNom, raw,
                                   d.seed + 1, false));
    }
    if (sims.empty() || !sims.front().hist) return;

    TH1D* sim = static_cast<TH1D*>(sims.front().hist->Clone(
        Form("hsim_%s", label)));
    sim->SetDirectory(nullptr);
    for (std::size_t k = 1; k < sims.size(); ++k) sim->Add(sims[k].hist, 1.);
    TH1D* simRaw = static_cast<TH1D*>(simsRaw.front().hist->Clone(
        Form("hsimraw_%s", label)));
    simRaw->SetDirectory(nullptr);
    for (std::size_t k = 1; k < simsRaw.size(); ++k) {
        simRaw->Add(simsRaw[k].hist, 1.);
    }

    raw->SetLineColor(kGray + 1);
    raw->SetLineWidth(1);
    net->SetLineColor(kBlack);
    net->SetLineWidth(2);
    sim->SetLineColor(kRed + 1);
    sim->SetLineWidth(2);
    sim->SetLineStyle(2);
    simRaw->SetLineColor(kBlue + 1);
    simRaw->SetLineWidth(2);
    simRaw->SetLineStyle(3);

    TCanvas c(Form("c_%s", label), "", 760, 720);
    c.Divide(1, 2, 0.0, 0.0);

    // Top: spectra.  The x-axis labels are on the lower pad only.
    c.cd(1);
    StylePad(gPad);
    gPad->SetTopMargin(0.08);
    gPad->SetBottomMargin(0.0);
    gPad->SetLogy(1);
    raw->SetTitle("");
    raw->GetXaxis()->SetTitle("");
    raw->GetXaxis()->SetLabelSize(0);
    raw->GetYaxis()->SetTitle("counts / bin");
    raw->GetXaxis()->CenterTitle();
    raw->GetYaxis()->CenterTitle();
    raw->GetYaxis()->SetTitleOffset(1.35);
    raw->GetXaxis()->SetRangeUser(x0, x1);
    double ymax = std::max(raw->GetMaximum(),
                           std::max(sim->GetMaximum(), simRaw->GetMaximum()));
    raw->SetMaximum(2.0 * ymax);
    raw->SetMinimum(0.5);
    raw->Draw("hist");
    net->Draw("hist same");
    sim->Draw("hist same");
    simRaw->Draw("hist same");

    const bool isCS = (std::string(label) == "CS");
    const TString expName = isCS ? "Meas1_200_CS_Clyc2.root"
                                 : "Meas1_200_Co_Clyc2.root";
    const TString simName = isCS ? "clyc_eff_200mm_661p64keV_cone.root"
                                 : "clyc_eff_200mm_1173+1332keV_cone.root";
    TLegend leg(0.18, 0.04, 0.72, 0.33);
    leg.SetBorderSize(0);
    leg.SetFillColor(kWhite);
    leg.SetFillStyle(0);
    leg.SetTextSize(0.040);
    leg.AddEntry(raw, Form("%s (raw)", expName.Data()), "l");
    leg.AddEntry(net, Form("%s #minus Hintergrund.root",
                           expName.Data()), "l");
    leg.AddEntry(sim, Form("%s (+ res.)",
                           simName.Data()), "l");
    leg.AddEntry(simRaw, Form("%s (no res.)",
                              simName.Data()), "l");
    leg.Draw();

    TLatex latTop;
    latTop.SetNDC();
    latTop.SetTextFont(62);
    latTop.SetTextSize(0.050);
    latTop.DrawLatex(0.05, 0.93, "(a)");

    // Bottom: ratio over the full x range, logarithmic y axis.
    c.cd(2);
    StylePad(gPad);
    gPad->SetTopMargin(0.0);
    gPad->SetLogy(1);
    TH1D* num = static_cast<TH1D*>(net->Clone(Form("hnum_%s", label)));
    TH1D* den = static_cast<TH1D*>(sim->Clone(Form("hden_%s", label)));
    num->SetDirectory(nullptr);
    den->SetDirectory(nullptr);
    TH1D* ratio = static_cast<TH1D*>(num->Clone(Form("hratio_%s", label)));
    ratio->SetDirectory(nullptr);
    ratio->Divide(num, den, 1., 1., "b");
    ratio->SetLineColor(kBlack);
    ratio->SetLineWidth(1);
    ratio->SetMarkerStyle(20);
    ratio->SetMarkerSize(0.3);
    ratio->SetTitle("");
    ratio->GetXaxis()->SetTitle("#font[132]{#it{E}}_{dep} [keV]");
    ratio->GetYaxis()->SetTitle("experiment / simulation");
    ratio->GetXaxis()->CenterTitle();
    ratio->GetYaxis()->CenterTitle();
    ratio->GetYaxis()->SetTitleOffset(1.35);
    ratio->GetXaxis()->SetRangeUser(x0, x1);

    // Omit bins with fewer than 10 simulated counts or a non-positive net
    // spectrum: the ratio is not meaningful there and would dominate the log
    // range.
    double rmax = 0., rmin = 1e9;
    for (int i = 1; i <= ratio->GetNbinsX(); ++i) {
        const double x = ratio->GetBinCenter(i);
        if (x < x0 || x >= x1) continue;
        const double ns = sim->GetBinContent(i);
        const double nn = net->GetBinContent(i);
        if (ns < 10. || nn <= 0.) {
            ratio->SetBinContent(i, 0.);
            ratio->SetBinError(i, 0.);
            continue;
        }
        const double r = nn / ns;
        if (r > rmax) rmax = r;
        if (r > 0. && r < rmin) rmin = r;
    }
    if (rmax <= 0.) rmax = 10.;
    const double ymaxR = std::pow(10., std::ceil(std::log10(rmax * 1.2))) * 1.3;
    double yminR = 0.1;
    if (rmin > 0. && rmin < 0.5) {
        yminR = std::pow(10., std::floor(std::log10(rmin / 1.2)));
    }
    if (yminR < 0.5) yminR = 0.5;
    ratio->SetMinimum(yminR);
    ratio->SetMaximum(ymaxR);
    ratio->Draw("E1");

    TBox band(x0, 0.9, x1, 1.1);
    band.SetFillColorAlpha(kGreen, 0.15);
    band.SetLineColor(kGreen + 2);
    band.SetLineWidth(1);
    band.Draw("same");
    TLine one(x0, 1., x1, 1.);
    one.SetLineColor(kGray + 2);
    one.SetLineStyle(2);
    one.Draw("same");
    ratio->Draw("E1 same");

    TLatex latBot;
    latBot.SetNDC();
    latBot.SetTextFont(62);
    latBot.SetTextSize(0.050);
    latBot.DrawLatex(0.05, 0.12, "(b)");

    const TString rawPdf = TString(outPdf).ReplaceAll(".pdf", "_raw.pdf");
    c.SaveAs(rawPdf);
    gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                       rawPdf.Data(), outPdf));
    gSystem->Unlink(rawPdf);

    const double eRef = simDefs.front().eNom;
    const int bLo = net->FindBin(0.98 * eRef);
    const int bHi = net->FindBin(1.02 * eRef);
    const double netPeak = net->Integral(bLo, bHi);
    const double simPeak = sim->Integral(bLo, bHi);
    std::printf("%s: net integral %.0f, smeared sim %.0f, no-reso sim %.0f, "
                "peak net/smeared %.3f (net %.0f, sim %.0f)\n",
                label, net->Integral(), sim->Integral(), simRaw->Integral(),
                simPeak > 0. ? netPeak / simPeak : 0., netPeak, simPeak);
}


TF1* FitGaussianBackground(TH1D* h, double E, double fwhm, const char* name,
                       Color_t colour, TF1*& peak, TF1*& bkg)
{
    const double lo = E - 1.5 * fwhm;
    const double hi = E + 1.5 * fwhm;
    TF1* total = new TF1(Form("f_%s", name), "gaus(0)+pol2(3)", lo, hi);
    const double sig = fwhm / 2.354820045;
    const double amp = h->GetBinContent(h->FindBin(E));
    auto avg = [&](double a, double b) {
        const int b1 = h->FindBin(a);
        const int b2 = h->FindBin(b);
        const int n = b2 - b1 + 1;
        return n > 0 ? h->Integral(b1, b2) / static_cast<double>(n) : 0.;
    };
    const double b0 = 0.5 * (avg(E - 1.8 * fwhm, E - 1.2 * fwhm) +
                             avg(E + 1.2 * fwhm, E + 1.8 * fwhm));
    total->SetParameters(amp, E, sig, b0, 0., 0.);
    total->SetParLimits(1, E - 0.03 * E, E + 0.03 * E);
    total->SetParLimits(2, 0.4 * sig, 2.5 * sig);
    total->SetLineColor(colour);
    total->SetLineWidth(3);
    total->SetLineStyle(2);
    h->Fit(total, "RQN0S", "", lo, hi);

    peak = new TF1(Form("f_%s_peak", name), "gaus", lo, hi);
    peak->SetParameters(total->GetParameter(0), total->GetParameter(1),
                        total->GetParameter(2));
    peak->SetLineColor(colour);
    peak->SetLineWidth(1);
    peak->SetLineStyle(1);

    bkg = new TF1(Form("f_%s_bkg", name), "pol2", lo, hi);
    bkg->SetParameters(total->GetParameter(3), total->GetParameter(4),
                        total->GetParameter(5));
    bkg->SetLineColor(colour);
    bkg->SetLineWidth(1);
    bkg->SetLineStyle(3);
    return total;
}

void MakeCoFitFigure(const char* outPdf, const char* expFile, const char* bkgFile,
                     const ExpRow& row1, const ExpRow& row2,
                     const std::vector<SimDef>& simDefs, double b)
{
    Style();
    TH1D* raw = CorrectedCountsE(expFile, row1.calA, row1.calB, "hraw_fit");
    TH1D* bkg = CorrectedCountsE(bkgFile, row1.calA, row1.calB, "hbkg_fit");
    if (!raw || !bkg) return;
    TH1D* net = static_cast<TH1D*>(raw->Clone("hnet_fit"));
    net->SetDirectory(nullptr);
    for (int i = 1; i <= net->GetNbinsX(); ++i) {
        const double nm = raw->GetBinContent(i);
        const double nbk = bkg->GetBinContent(i);
        net->SetBinContent(i, nm - nbk);
        net->SetBinError(i, std::sqrt(nm + std::max(0., nbk)));
    }

    std::vector<SimSpectrum> sims;
    for (const auto& d : simDefs) {
        sims.push_back(BuildSim(d.file, 1e7, d.scale, b, d.eNom, raw,
                                d.seed, true));
    }
    if (sims.empty() || !sims.front().hist) return;
    TH1D* sim = static_cast<TH1D*>(sims.front().hist->Clone("hsim_fit"));
    sim->SetDirectory(nullptr);
    for (std::size_t k = 1; k < sims.size(); ++k) sim->Add(sims[k].hist, 1.);

    TCanvas c("c_co_fits", "", 1000, 580);
    TPad* p1 = new TPad("p_fit1", "", 0.0, 0.25, 0.5, 1.0);
    TPad* p2 = new TPad("p_fit2", "", 0.5, 0.25, 1.0, 1.0);
    TPad* pLeg = new TPad("p_fitleg", "", 0.0, 0.0, 1.0, 0.25);
    p1->Draw();
    p2->Draw();
    pLeg->Draw();

    struct PeakDef {
        const ExpRow* row;
        const char* tag;
    };
    const PeakDef peaks[2] = {{&row1, "1173.23"}, {&row2, "1332.51"}};

    for (int i = 0; i < 2; ++i) {
        (i == 0 ? p1 : p2)->cd();
        gPad->SetLeftMargin(0.13);
        gPad->SetRightMargin(0.04);
        gPad->SetBottomMargin(0.16);
        gPad->SetTopMargin(0.08);
        gPad->SetLogy(0);
        gPad->SetTickx(1);
        gPad->SetTicky(1);

        const double E = peaks[i].row->energy;
        const double fwhm = peaks[i].row->fwhm;
        const double xlo = E - 2.2 * fwhm;
        const double xhi = E + 2.2 * fwhm;

        TH1D* netDraw = static_cast<TH1D*>(net->Clone(
            Form("hnet_draw_%d", i)));
        TH1D* simDraw = static_cast<TH1D*>(sim->Clone(
            Form("hsim_draw_%d", i)));
        netDraw->SetDirectory(nullptr);
        simDraw->SetDirectory(nullptr);
        netDraw->SetLineColor(kBlack);
        netDraw->SetLineWidth(2);
        simDraw->SetLineColor(kRed + 1);
        simDraw->SetLineStyle(2);
        simDraw->SetLineWidth(2);
        netDraw->GetXaxis()->SetRangeUser(xlo, xhi);
        netDraw->GetXaxis()->SetTitle("#font[132]{#it{E}}_{dep} [keV]");
        netDraw->GetYaxis()->SetTitle("counts / bin");
        netDraw->GetXaxis()->CenterTitle();
        netDraw->GetYaxis()->CenterTitle();
        netDraw->GetYaxis()->SetTitleOffset(1.25);

        double ymax = 0.;
        for (int bin = 1; bin <= net->GetNbinsX(); ++bin) {
            const double x = net->GetBinCenter(bin);
            if (x < xlo || x > xhi) continue;
            ymax = std::max(ymax, std::max(net->GetBinContent(bin),
                                           sim->GetBinContent(bin)));
        }
        netDraw->SetMinimum(0.);
        netDraw->SetMaximum(1.25 * ymax);
        netDraw->Draw("HIST");
        simDraw->Draw("HIST same");

        TF1 *eTot, *ePeak, *eBkg, *sTot, *sPeak, *sBkg;
        eTot = FitGaussianBackground(net, E, fwhm, Form("exp_%s", peaks[i].tag),
                                 kBlue + 1, ePeak, eBkg);
        sTot = FitGaussianBackground(sim, E, fwhm, Form("sim_%s", peaks[i].tag),
                                 kGreen + 2, sPeak, sBkg);
        eTot->SetLineColor(kMagenta + 1);
        eTot->SetLineStyle(1);
        eTot->SetLineWidth(3);
        sTot->SetLineColor(kOrange + 1);
        sTot->SetLineStyle(1);
        sTot->SetLineWidth(3);
        eTot->Draw("same");
        ePeak->Draw("same");
        eBkg->Draw("same");
        sTot->Draw("same");
        sPeak->Draw("same");
        sBkg->Draw("same");

        const double bw = net->GetBinWidth(1);
        auto area = [&](TF1* f) {
            return f->GetParameter(0) * f->GetParameter(2) *
                   TMath::Sqrt(2. * TMath::Pi()) / bw;
        };
        const double ea = area(ePeak);
        const double sa = area(sPeak);
        std::printf("FIT %s E=%.2f exp %.0f sim %.0f sim/exp %.3f\n",
                    peaks[i].tag, E, ea, sa, ea > 0. ? sa / ea : 0.);
        TLatex pl;
        pl.SetNDC();
        pl.SetTextFont(62);
        pl.SetTextSize(0.032);
        pl.DrawLatex(0.50, 0.93,
                     Form("(%c) %s keV", 'a' + i, peaks[i].tag));
    }

    pLeg->cd();
    pLeg->SetTopMargin(0.02);
    pLeg->SetBottomMargin(0.04);
    pLeg->SetLeftMargin(0.02);
    pLeg->SetRightMargin(0.02);

    TLegend* leg = new TLegend(0.01, 0.04, 0.99, 0.96);
    leg->SetNColumns(4);
    leg->SetBorderSize(0);
    leg->SetFillStyle(0);
    leg->SetTextFont(62);
    leg->SetTextSize(0.12);
    auto addLine = [&](Color_t colour, Style_t style, Width_t width,
                       const char* text) {
        TLine* line = new TLine(0., 0., 1., 0.);
        line->SetLineColor(colour);
        line->SetLineStyle(style);
        line->SetLineWidth(width);
        leg->AddEntry(line, text, "l");
    };
    // Row 1: Experiment items
    addLine(kBlack, 1, 2, "exp: data #minus bkg");
    addLine(kMagenta + 1, 1, 3, "exp: fit (peak + bkg)");
    addLine(kBlue + 1, 1, 2, "exp: fit (peak)");
    addLine(kBlue + 1, 2, 2, "exp: fit (bkg)");
    // Row 2: Simulation items
    addLine(kRed + 1, 2, 2, "sim: + resolution");
    addLine(kOrange + 1, 1, 3, "sim: fit (peak + bkg)");
    addLine(kGreen + 2, 1, 2, "sim: fit (peak)");
    addLine(kGreen + 2, 2, 2, "sim: fit (bkg)");
    leg->Draw();

    const TString rawPdf = TString(outPdf).ReplaceAll(".pdf", "_raw.pdf");
    c.SaveAs(rawPdf);
    gSystem->Exec(Form("pdfcrop --margins 2 %s %s >/dev/null 2>&1",
                       rawPdf.Data(), outPdf));
    gSystem->Unlink(rawPdf);
}
} // namespace

void make_exp_sim_figs(
    const char* expDir = "build/analysis/clyc_exp_2026",
    const char* simDir = "build/validation/clyc_eff",
    const char* peakCsv = "report/eff_clyc/data/exp_peak_integrals.csv",
    const char* modelCsv = "report/eff_clyc/data/exp_resolution_model.csv",
    const char* outDir = "report/eff_clyc/figures")
{
    const std::vector<ExpRow> rows = ReadPeakCsv(peakCsv);
    if (rows.empty()) return;
    const double b = ReadResolutionB(modelCsv);
    if (b <= 0.) {
        std::printf("ERROR: resolution model b is zero\n");
        return;
    }
    gSystem->mkdir(outDir, kTRUE);
    const TString dir(expDir);
    const TString sDir(simDir);
    const TString out(outDir);

    // --- Cs-137 at 200 mm ------------------------------------------------
    const ExpRow* cs = FindRow(rows, "CS", 200, 661.64);
    if (cs) {
        const double Nemit = cs->aExact * cs->pGamma / 100. * cs->tLive;
        const double scale = Nemit * kConeFraction / 1e7;
        const std::vector<SimDef> defs{
            {Form("%s/clyc_eff_200mm_661p64keV_cone.root", sDir.Data()),
             scale, 661.64, 20261027}};
        MakeFigure(Form("%s/fig_exp_sim_200mm_CS.pdf", out.Data()),
                   Form("%s/Meas1_200_CS_Clyc2.root", dir.Data()),
                   Form("%s/Hintergrund.root", dir.Data()), *cs, defs, b,
                   0., 710., "CS");
    }

    // --- Co-60 at 200 mm -------------------------------------------------
    const ExpRow* co1 = FindRow(rows, "Co", 200, 1173.23);
    const ExpRow* co2 = FindRow(rows, "Co", 200, 1332.51);
    if (co1 && co2) {
        const double Nemit1 = co1->aExact * co1->pGamma / 100. * co1->tLive;
        const double Nemit2 = co2->aExact * co2->pGamma / 100. * co2->tLive;
        const std::vector<SimDef> defs{
            {Form("%s/clyc_eff_200mm_1173p23keV_cone.root", sDir.Data()),
             Nemit1 * kConeFraction / 1e7, 1173.23, 20261027},
            {Form("%s/clyc_eff_200mm_1332p51keV_cone.root", sDir.Data()),
             Nemit2 * kConeFraction / 1e7, 1332.51, 20261028}};
        MakeFigure(Form("%s/fig_exp_sim_200mm_Co.pdf", out.Data()),
                   Form("%s/Meas1_200_Co_Clyc2.root", dir.Data()),
                   Form("%s/Hintergrund.root", dir.Data()), *co1, defs, b,
                   0., 1450., "Co");
        MakeCoFitFigure(Form("%s/fig_exp_sim_200mm_Co_fits.pdf", out.Data()),
                        Form("%s/Meas1_200_Co_Clyc2.root", dir.Data()),
                        Form("%s/Hintergrund.root", dir.Data()), *co1, *co2,
                        defs, b);
    }
}
