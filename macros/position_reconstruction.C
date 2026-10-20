// position_reconstruction.C -- can the relative SiPM signal pattern localise
// the muon inside the plate?
//
// This macro answers the question "can the relative timing/amplitude
// information of the eight SiPMs of config 1 be used to determine the
// interaction position of the incident muon?" for the whole-plate irradiation
// of data_v6 (25 000 vertical 4 GeV muons uniformly covering the 250 x 250 mm
// plate, per reflector treatment).
//
// For every event the macro extracts, per SiPM, the observables that a real
// read-out chain would provide:
//
//   N        number of detected photoelectrons                    (pulse integral:
//             the total charge collected in the channel over the whole event,
//             NOT the amplitude of the pulse peak)
//   Tfirst   time of the first detected photoelectron             (relative time)
//   Tbar     mean arrival time of the detected photoelectrons
//   NP3      number of photoelectrons arriving before 3 ns         (prompt light)
//   Wbar     mean wavelength of the detected photoelectrons
//
// The ground truth is the muon's first-entry position in the slab
// (muonX/muonY, slab-local mm) that the sipmHits tree carries.  It is
// converted to the report plan view (top = +y, left = -x) with the usual
// (x,y) -> (y,-x) rotation, so that all the maps produced here are directly
// comparable with the collection-efficiency maps of Figs. 20-23.
//
// Two families of estimator are evaluated on a held-out test set:
//
//  (a) a transparent, purely analytic one: the "opposite-pair" observables
//      A_x = (N_right - N_left)/(N_right + N_left) and
//      T_x = <tfirst>_right - <tfirst>_left, whose linear response to the
//      position is measured directly, so that the achievable resolution is
//      (noise)/(slope);
//
//  (b) a learned one: a k-nearest-neighbour regression in the standardised
//      feature space, i.e. the reconstructed position is the mean position of
//      the k most similar training events.  This is the "response function"
//      that a calibration scan would provide in a real experiment.
//
// Outputs (in outdir):
//   fig_recon_waveforms.{pdf,png}   per-SiPM waveforms, central vs corner muon
//   fig_recon_observables.{pdf,png} per-SiPM light share and arrival time
//   fig_recon_features.{pdf,png}    resolution per estimator and wrapper
//   fig_recon_response.{pdf,png}    position response (predicted vs true)
//   fig_recon_map.{pdf,png}         map of the position resolution
//   position_reconstruction.csv      estimator resolution table
//   position_observables.csv         opposite-pair linear-response table
//
// Usage (batch):
//   root -l -b -q 'macros/position_reconstruction.C("data_v6/whole","report/figures",25000)'
//
// Args: dir (default: the data_v6 whole tree), outdir (default "report/figures"),
//       maxEvents (default 25000 = all events in the file).
//
// NOTE: run this through ACLiC (.C+) -- the k-NN regression is compute-bound.

#include <TCanvas.h>
#include <TColor.h>
#include <TFile.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TLegend.h>
#include <TLine.h>
#include <TList.h>
#include <TMarker.h>
#include <TPad.h>
#include <TROOT.h>
#include <TSystem.h>
#include <TString.h>
#include <TTree.h>
#include <TH1D.h>
#include <TH2D.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, palette).  Located via ROOT_INCLUDE_PATH
// (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

// ---------------------------------------------------------------------------
// Config-1 geometry, in the REPORT plan view (top = +y, left = -x).
//
// The `sipm` column of the sipmHits tree is the placement index of
// src/GeometryMuonScint.cc:1959 (PresetConfig_8SiPM_AllSides): for each of the
// four edges e = 0(+X) 1(-X) 2(+Y) 3(-Y) the two sensors at u = -0.5 and
// u = +0.5 are placed in that order.  The names follow the report convention
// of macros/matrix_positions.C (clockwise T1 T2 R1 R2 B1 B2 L1 L2).
// The slab-local positions are (+-125, u*125) resp. (u*125, +-125); the
// report-frame positions below are that list after the (x,y) -> (y,-x) turn.
// ---------------------------------------------------------------------------
const double kHalf  = 125.0;    // slab half-width [mm]
const double kSipmU = 62.5;     // along-side offset of the config-1 SiPMs [mm]

struct SipmGeom { double x, y; const char* name; };
const SipmGeom kSipm[8] = {
    {-kSipmU, -kHalf, "B1"}, {+kSipmU, -kHalf, "B2"},
    {-kSipmU, +kHalf, "T2"}, {+kSipmU, +kHalf, "T1"},
    {+kHalf, +kSipmU, "R2"}, {+kHalf, -kSipmU, "R1"},
    {-kHalf, +kSipmU, "L1"}, {-kHalf, -kSipmU, "L2"},
};
// opposite-edge groups (report frame)
const int kBot[2] = {0, 1};   // bottom edge  (simulation +X face)
const int kTop[2] = {2, 3};   // top edge     (simulation -X face)
const int kRgt[2] = {4, 5};   // right edge   (simulation +Y face)
const int kLft[2] = {6, 7};   // left edge    (simulation -Y face)

const char* kReflName[4] = {"bare", "TiO2", "Al (air)", "glossy"};
const char* kReflPlain[4] = {"bare", "TiO2", "Al (air)", "glossy"};

// arrival-time bins used for the waveform figure
const int    kWaveBins = 200;      // 0.2 ns bins ...
const double kWaveTmax = 40.0;     // ... over 0-40 ns

// position classes used for the "extreme event" figure (report frame)
const int    kNClass   = 2;        // 0 = centre, 1 = top-right corner
const double kCentreR  = 15.0;    // |x|,|y| <= 15 mm
const double kCornerR  = 90.0;    // x >= 90 and y >= 90 mm

// ---------------------------------------------------------------------------
// Per-event data
// ---------------------------------------------------------------------------
struct Event {
    int    evt  = 0;
    double x = 0, y = 0;            // truth: muon entry, REPORT frame mm
    int    n[8]     = {0};          // detected photoelectrons per SiPM
    double tfirst[8] = {0};         // first arrival time [ns] (0 = nothing seen)
    double tbar[8]   = {0};         // mean arrival time [ns]
    int    np3[8]    = {0};         // photoelectrons arriving before 3 ns
    double wbar[8]   = {0};         // mean detected wavelength [nm]
};

struct WaveSet {
    std::vector<double> h[kNClass][8];   // accumulated time histograms
    long n[kNClass] = {0, 0};
    WaveSet() {
        for (int c = 0; c < kNClass; ++c)
            for (int s = 0; s < 8; ++s) h[c][s].assign(kWaveBins, 0.0);
    }
};

struct WrapperData {
    std::vector<Event> evs;
    WaveSet wave;
    // bookkeeping for the prompt fraction of the collected light (the share of
    // the photoelectrons that arrive before 3 ns), quoted in the report
    double sumTot = 0, sumP3 = 0;
};

int Classify(const Event& ev)
{
    if (std::fabs(ev.x) <= kCentreR && std::fabs(ev.y) <= kCentreR) return 0;
    if (ev.x >= kCornerR && ev.y >= kCornerR) return 1;
    return -1;
}

// ---------------------------------------------------------------------------
// Reader: one whole-slab config-1 file -> per-event features
// ---------------------------------------------------------------------------
bool LoadConfig1(const char* fname, int maxEvents, WrapperData& out)
{
    TFile f(fname);
    if (!f.IsOpen()) { std::cerr << "[position_reconstruction] cannot open " << fname << "\n"; return false; }
    TTree* t = (TTree*)f.Get("sipmHits");
    if (!t) { std::cerr << "[position_reconstruction] no sipmHits in " << fname << "\n"; f.Close(); return false; }

    int    evtNb = 0, det = 0, sipm = 0, nDet = 0;
    double tFirst = 0, meanW = 0, muonX = 0, muonY = 0;
    std::vector<double>* times = nullptr;
    t->SetBranchAddress("evtNb", &evtNb);
    t->SetBranchAddress("det", &det);
    t->SetBranchAddress("sipm", &sipm);
    t->SetBranchAddress("nDetected", &nDet);
    t->SetBranchAddress("tFirst", &tFirst);
    t->SetBranchAddress("meanWavelength_nm", &meanW);
    t->SetBranchAddress("muonX", &muonX);
    t->SetBranchAddress("muonY", &muonY);
    t->SetBranchAddress("tDetected", &times);

    const Long64_t nrows = t->GetEntries();

    Event cur;
    bool have = false;
    std::vector<double> evBuf[8];
    for (int s = 0; s < 8; ++s) evBuf[s].assign(kWaveBins, 0.0);

    auto flushEvent = [&]() {
        if (!have) return;
        const int cls = Classify(cur);
        if (cls >= 0) {
            out.wave.n[cls]++;
            for (int s = 0; s < 8; ++s)
                for (int b = 0; b < kWaveBins; ++b) out.wave.h[cls][s][b] += evBuf[s][b];
        }
        // prompt fraction of the collected light (PE arriving before 3 ns)
        for (int s = 0; s < 8; ++s) { out.sumTot += cur.n[s]; out.sumP3 += cur.np3[s]; }
        out.evs.push_back(cur);
    };

    for (Long64_t r = 0; r < nrows; ++r) {
        t->GetEntry(r);
        if (det != 0) continue;
        if (!have || evtNb != cur.evt) {
            flushEvent();
            if ((int)out.evs.size() >= maxEvents) break;
            cur = Event();
            cur.evt = evtNb;
            // slab-local -> report frame (top = +y, left = -x)
            cur.x =  muonY;
            cur.y = -muonX;
            for (int s = 0; s < 8; ++s) evBuf[s].assign(kWaveBins, 0.0);
            have = true;
        }
        if (sipm < 0 || sipm > 7) continue;
        cur.n[sipm]      = nDet;
        cur.tfirst[sipm] = tFirst;
        cur.wbar[sipm]   = meanW;
        if (times && !times->empty()) {
            const std::vector<double>& tt = *times;
            double sum = 0;
            int np3 = 0;
            for (size_t i = 0; i < tt.size(); ++i) {
                sum += tt[i];
                if (tt[i] < 3.0) ++np3;
                int b = (int)(tt[i] / (kWaveTmax / kWaveBins));
                if (b < 0) b = 0;
                if (b >= kWaveBins) b = kWaveBins - 1;
                evBuf[sipm][b] += 1.0;
            }
            cur.tbar[sipm] = sum / (double)tt.size();
            cur.np3[sipm]  = np3;
        }
    }
    flushEvent();
    f.Close();
    return true;
}

// ---------------------------------------------------------------------------
// Features
// ---------------------------------------------------------------------------
enum Feat { kAmp = 0, kTime = 1, kPrompt = 2, kAll = 3, kNFeat = 4 };
const char* kFeatName[kNFeat] = {"pulse integral", "arrival time", "prompt fraction", "combined"};

void MakeFeatures(const std::vector<Event>& evs, int kind,
                  std::vector<std::vector<double>>& X)
{
    const size_t n = evs.size();
    X.assign(n, std::vector<double>());
    std::vector<double> amp(8), tv(8), pv(8);
    for (size_t e = 0; e < n; ++e) {
        const Event& ev = evs[e];

        // (i) pulse-integral shares (total collected charge per channel),
        //     centred on the event mean
        double tot = 0;
        for (int s = 0; s < 8; ++s) tot += ev.n[s];
        double amean = 0;
        if (tot > 0) {
            for (int s = 0; s < 8; ++s) { amp[s] = ev.n[s] / tot; amean += amp[s]; }
        }
        amean /= 8.0;
        for (int s = 0; s < 8; ++s) amp[s] -= amean;

        // (ii) relative first-arrival times (common offset removed)
        double tsum = 0;
        int tn = 0;
        for (int s = 0; s < 8; ++s) if (ev.tfirst[s] > 0) { tsum += ev.tfirst[s]; ++tn; }
        for (int s = 0; s < 8; ++s)
            tv[s] = (tn > 0 && ev.tfirst[s] > 0) ? ev.tfirst[s] - tsum / tn : 0.0;

        // (iii) prompt fraction (photoelectrons before 3 ns), centred
        double ptot = 0;
        for (int s = 0; s < 8; ++s) ptot += ev.np3[s];
        double pmean = 0;
        if (ptot > 0) {
            for (int s = 0; s < 8; ++s) { pv[s] = ev.np3[s] / ptot; pmean += pv[s]; }
        }
        pmean /= 8.0;
        for (int s = 0; s < 8; ++s) pv[s] -= pmean;

        std::vector<double>& f = X[e];
        f.clear();
        if (kind == kAmp   || kind == kAll) f.insert(f.end(), amp.begin(), amp.end());
        if (kind == kTime  || kind == kAll) f.insert(f.end(), tv.begin(), tv.end());
        if (kind == kPrompt || kind == kAll) f.insert(f.end(), pv.begin(), pv.end());
    }
}

// ---------------------------------------------------------------------------
// k-nearest-neighbour regression (the "learned response function")
// ---------------------------------------------------------------------------
void KnnPredict(const std::vector<std::vector<double>>& Xtr,
                const std::vector<double>& Yxtr,
                const std::vector<double>& Yytr,
                const std::vector<std::vector<double>>& Xte,
                int k,
                std::vector<double>& px, std::vector<double>& py)
{
    const size_t ntr = Xtr.size(), nte = Xte.size();
    if (ntr == 0 || nte == 0) return;
    const size_t dim = Xtr[0].size();

    // standardise with the training-set mean and spread
    std::vector<double> mu(dim, 0.0), sd(dim, 0.0);
    for (size_t j = 0; j < dim; ++j) {
        double s = 0;
        for (size_t i = 0; i < ntr; ++i) s += Xtr[i][j];
        mu[j] = s / ntr;
        for (size_t i = 0; i < ntr; ++i) { const double d = Xtr[i][j] - mu[j]; sd[j] += d * d; }
        sd[j] = std::sqrt(sd[j] / ntr);
        if (sd[j] <= 0) sd[j] = 1.0;
    }
    std::vector<std::vector<double>> A(ntr, std::vector<double>(dim));
    std::vector<std::vector<double>> B(nte, std::vector<double>(dim));
    for (size_t i = 0; i < ntr; ++i)
        for (size_t j = 0; j < dim; ++j) A[i][j] = (Xtr[i][j] - mu[j]) / sd[j];
    for (size_t i = 0; i < nte; ++i)
        for (size_t j = 0; j < dim; ++j) B[i][j] = (Xte[i][j] - mu[j]) / sd[j];

    px.assign(nte, 0.0);
    py.assign(nte, 0.0);

    const int kk = std::min<int>(k, (int)ntr);
    std::vector<std::pair<double, int>> d(ntr);
    for (size_t t = 0; t < nte; ++t) {
        for (size_t i = 0; i < ntr; ++i) {
            double s = 0;
            for (size_t j = 0; j < dim; ++j) {
                const double dd = A[i][j] - B[t][j];
                s += dd * dd;
            }
            d[i] = {s, (int)i};
        }
        std::partial_sort(d.begin(), d.begin() + kk, d.end());
        double sx = 0, sy = 0;
        for (int m = 0; m < kk; ++m) { sx += Yxtr[d[m].second]; sy += Yytr[d[m].second]; }
        px[t] = sx / kk;
        py[t] = sy / kk;
    }
}

// ---------------------------------------------------------------------------
// Opposite-pair observables and their linear response to the position
// ---------------------------------------------------------------------------
struct PairStat { double slope = 0, intercept = 0, noise = 0, sigmaPos = 0; };

PairStat LinearFit(const std::vector<double>& v, const std::vector<double>& t)
{
    PairStat st;
    const size_t n = v.size();
    if (n < 2) return st;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (size_t i = 0; i < n; ++i) { sx += t[i]; sy += v[i]; sxx += t[i] * t[i]; sxy += t[i] * v[i]; }
    const double den = n * sxx - sx * sx;
    if (std::fabs(den) < 1e-12) return st;
    st.slope     = (n * sxy - sx * sy) / den;
    st.intercept = (sy - st.slope * sx) / n;
    double ss = 0;
    for (size_t i = 0; i < n; ++i) {
        const double r = v[i] - (st.intercept + st.slope * t[i]);
        ss += r * r;
    }
    st.noise = std::sqrt(ss / n);
    st.sigmaPos = (std::fabs(st.slope) > 1e-12) ? st.noise / std::fabs(st.slope) : 0.0;
    return st;
}

struct ObsRow { int refl; const char* obs; PairStat st; };

struct EstResult {
    int    feat = 0;
    int    k = 0;
    int    ntrain = 0, ntest = 0;
    double sx = 0, sy = 0, sr = 0;      // rms residuals per axis and radially
    double biasx = 0, biasy = 0;
    double r68 = 0;                     // 68 % containment radius
    std::vector<double> px, py, tx, ty; // test-set truth and prediction
};

}  // namespace

// ===========================================================================
// main entry
// ===========================================================================
void position_reconstruction(const char* dir = "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v6/whole",
                              const char* outdir = "report/figures",
                              int maxEvents = 25000)
{
    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gSystem->mkdir(outdir, kTRUE);

    const int    kK        = 40;        // neighbours of the k-NN regression
    const double kTestFrac = 0.2;       // 20 % of the events held out

    // ---------------------------------------------------------------- read
    std::vector<WrapperData> wd(4);
    for (int r = 0; r < 4; ++r) {
        const TString fn = TString::Format("%s/muon_whole_out_c1_r%d.root", dir, r);
        std::cout << "[position_reconstruction] reading " << fn << " ..." << std::endl;
        if (!LoadConfig1(fn, maxEvents, wd[r])) return;
        std::cout << "   " << wd[r].evs.size() << " events"
                  << "  (centre class " << wd[r].wave.n[0]
                  << ", corner class " << wd[r].wave.n[1] << ")" << std::endl;
        if (wd[r].sumTot > 0)
            std::cout << "   mean collected charge " << wd[r].sumTot / wd[r].evs.size()
                      << " p.e./event, of which " << 100.0 * wd[r].sumP3 / wd[r].sumTot
                      << " % arrives before 3 ns (prompt fraction)" << std::endl;
    }

    // ------------------------------------------------- opposite-pair analysis
    std::vector<ObsRow> obs;
    for (int r = 0; r < 4; ++r) {
        const std::vector<Event>& E = wd[r].evs;
        const size_t n = E.size();
        std::vector<double> Ax(n), Ay(n), Tx(n), Ty(n), Bx(n), By(n);
        for (size_t e = 0; e < n; ++e) {
            const Event& ev = E[e];
            auto grp = [&](const int* g) {
                double s = 0;
                s += ev.n[g[0]] + ev.n[g[1]];
                return s;
            };
            const double bot = grp(kBot), top = grp(kTop);
            const double rgt = grp(kRgt), lft = grp(kLft);
            Ax[e] = (rgt + lft > 0) ? (rgt - lft) / (rgt + lft) : 0.0;   // report x
            Ay[e] = (bot + top > 0) ? (bot - top) / (bot + top) : 0.0;   // report y
            auto meanFirst = [&](int a, int b) {
                double s = 0; int k = 0;
                if (ev.tfirst[a] > 0) { s += ev.tfirst[a]; ++k; }
                if (ev.tfirst[b] > 0) { s += ev.tfirst[b]; ++k; }
                return k ? s / k : 0.0;
            };
            Tx[e] = meanFirst(kRgt[0], kRgt[1]) - meanFirst(kLft[0], kLft[1]);
            Ty[e] = meanFirst(kBot[0], kBot[1]) - meanFirst(kTop[0], kTop[1]);
            Bx[e] = ev.x; By[e] = ev.y;
        }
        obs.push_back({r, "A_x pulse-integral asymmetry (report x)", LinearFit(Ax, Bx)});
        obs.push_back({r, "A_y pulse-integral asymmetry (report y)", LinearFit(Ay, By)});
        obs.push_back({r, "T_x first-arrival difference (report x)", LinearFit(Tx, Bx)});
        obs.push_back({r, "T_y first-arrival difference (report y)", LinearFit(Ty, By)});
    }

    // ------------------------------------------------------- learned estimators
    std::vector<std::vector<EstResult>> res(4);
    for (int r = 0; r < 4; ++r) {
        const std::vector<Event>& E = wd[r].evs;
        const size_t n = E.size();
        std::vector<double> Yx(n), Yy(n);
        for (size_t e = 0; e < n; ++e) { Yx[e] = E[e].x; Yy[e] = E[e].y; }

        std::vector<int> idx(n);
        std::iota(idx.begin(), idx.end(), 0);
        std::mt19937 rng(20251018u);
        std::shuffle(idx.begin(), idx.end(), rng);
        const size_t ntest = (size_t)(kTestFrac * n);
        std::vector<int> tr, te;
        for (size_t i = 0; i < n; ++i) { if (i < ntest) te.push_back(idx[i]); else tr.push_back(idx[i]); }

        for (int f = 0; f < kNFeat; ++f) {
            std::vector<std::vector<double>> X;
            MakeFeatures(E, f, X);
            std::vector<std::vector<double>> Xtr, Xte;
            std::vector<double> Yxtr, Yytr;
            for (size_t i : tr) { Xtr.push_back(X[i]); Yxtr.push_back(Yx[i]); Yytr.push_back(Yy[i]); }
            for (size_t i : te) { Xte.push_back(X[i]); }

            EstResult er;
            er.feat = f; er.k = kK; er.ntrain = (int)tr.size(); er.ntest = (int)te.size();
            KnnPredict(Xtr, Yxtr, Yytr, Xte, kK, er.px, er.py);
            for (size_t i = 0; i < te.size(); ++i) { er.tx.push_back(Yx[te[i]]); er.ty.push_back(Yy[te[i]]); }
            std::vector<double> dx, dy, dr;
            for (size_t i = 0; i < te.size(); ++i) {
                dx.push_back(er.tx[i] - er.px[i]);
                dy.push_back(er.ty[i] - er.py[i]);
                dr.push_back(std::sqrt(dx.back() * dx.back() + dy.back() * dy.back()));
            }
            auto rms = [&](const std::vector<double>& v) {
                double s = 0; for (double q : v) s += q * q; return std::sqrt(s / v.size());
            };
            auto mean = [&](const std::vector<double>& v) {
                double s = 0; for (double q : v) s += q; return s / v.size();
            };
            er.sx = rms(dx); er.sy = rms(dy); er.sr = rms(dr);
            er.biasx = mean(dx); er.biasy = mean(dy);
            std::vector<double> sdr = dr;
            std::sort(sdr.begin(), sdr.end());
            er.r68 = sdr[(size_t)(0.68 * sdr.size())];
            res[r].push_back(er);
            std::cout << "   r" << r << " " << kFeatName[f] << ": sigma_x = " << er.sx
                      << " mm, sigma_y = " << er.sy << " mm, sigma_r = " << er.sr << " mm"
                      << std::endl;
        }
    }

    // per-class observables (central vs corner muon), for the report text
    // columns: wrapper(0-3), class(0-1), SiPM, value
    std::vector<std::array<double, 4>> classObs;

    // ---------------------------------------------------------------- CSV
    {
        std::ofstream csv(TString::Format("%s/position_reconstruction.csv", outdir).Data());
        csv << "wrapper,estimator,neighbours,ntrain,ntest,sigma_x_mm,sigma_y_mm,sigma_r_mm,"
               "bias_x_mm,bias_y_mm,r68_mm\n";
        for (int r = 0; r < 4; ++r)
            for (const EstResult& e : res[r])
                csv << kReflPlain[r] << "," << kFeatName[e.feat] << "," << e.k << ","
                    << e.ntrain << "," << e.ntest << ","
                    << e.sx << "," << e.sy << "," << e.sr << ","
                    << e.biasx << "," << e.biasy << "," << e.r68 << "\n";
    }
    {
        std::ofstream csv(TString::Format("%s/position_observables.csv", outdir).Data());
        csv << "wrapper,observable,slope_per_mm,intercept,rms_noise,implied_sigma_mm\n";
        for (const ObsRow& o : obs)
            csv << kReflPlain[o.refl] << "," << o.obs << ","
                << o.st.slope << "," << o.st.intercept << ","
                << o.st.noise << "," << o.st.sigmaPos << "\n";
    }

    // ================================================================ figures
    const int kColor[8] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                           PetroffPalette::getP6(2), PetroffPalette::getP6(3),
                           PetroffPalette::getP6(4), PetroffPalette::getP6(5),
                           kMagenta, kTeal};

    // ------------------------------------------------- F1: waveform extremes
    {
        TCanvas* c = new TCanvas("fig_recon_waveforms", "", 1150, 820);
        c->Divide(2, 2);
        const int rows[2] = {3, 1};   // glossy, TiO2
        for (int ir = 0; ir < 2; ++ir) {
            const int refl = rows[ir];
            for (int ic = 0; ic < 2; ++ic) {
                const int cls = ic;  // 0 = centre, 1 = corner
                c->cd(ir * 2 + ic + 1);
                TLegend* leg = new TLegend(0.62, 0.40, 0.97, 0.92);
                double ymax = 0;
                TH1D* ax = nullptr;
                for (int s = 0; s < 8; ++s) {
                    const long n = wd[refl].wave.n[cls];
                    if (n <= 0) continue;
                    TH1D* h = new TH1D(TString::Format("w%d_%d_%d", refl, cls, s), "",
                                       kWaveBins, 0.0, kWaveTmax);
                    for (int b = 0; b < kWaveBins; ++b)
                        h->SetBinContent(b + 1, wd[refl].wave.h[cls][s][b] / (double)n);
                    h->SetLineColor(kColor[s]);
                    h->SetLineWidth(2);
                    h->SetDirectory(nullptr);
                    h->Draw("SAME");
                    if (!ax) ax = (TH1D*)gPad->GetListOfPrimitives()->At(0);
                    ymax = std::max(ymax, h->GetMaximum());
                    leg->AddEntry(h, kSipm[s].name, "l");
                }
                if (ax) {
                    ax->SetTitle(TString::Format("%s, %s muon (%ld events)",
                                                 kReflName[refl],
                                                 cls == 0 ? "central" : "corner",
                                                 wd[refl].wave.n[cls]));
                    ax->GetYaxis()->SetTitle("photoelectrons per 0.2 ns bin");
                    ax->GetXaxis()->SetTitle("arrival time [ns]");
                    ax->GetYaxis()->SetRangeUser(0, ymax * 1.12);
                    ax->GetXaxis()->SetRangeUser(0, kWaveTmax);
                }
                leg->SetBorderSize(0);
                leg->Draw();
            }
        }
        c->SaveAs(TString::Format("%s/fig_recon_waveforms.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_waveforms.png", outdir));
    }

    // ------------------------------------------- F2: per-SiPM observables
    //   left column : light share per SiPM, as the deviation [percentage
    //                 points] from the uniform 1/8 expectation;
    //   right column: first-arrival time per SiPM, relative to the mean
    //                 arrival time of the eight channels of the same event.
    // In both, the central-muon and the corner-muon populations are shown
    // side by side.
    {
        TCanvas* c = new TCanvas("fig_recon_observables", "", 1150, 820);
        c->Divide(2, 2);
        const int rows[2] = {3, 1};
        for (int ir = 0; ir < 2; ++ir) {
            const int refl = rows[ir];
            for (int ic = 0; ic < 2; ++ic) {
                c->cd(ir * 2 + ic + 1);
                const bool amp = (ic == 0);
                double sumN[kNClass][8] = {{0}}, sumT[kNClass][8] = {{0}};
                long   cnt[kNClass] = {0, 0};
                long   cntT[kNClass][8] = {{0}};
                for (const Event& ev : wd[refl].evs) {
                    const int k = Classify(ev);
                    if (k < 0) continue;
                    ++cnt[k];
                    for (int s = 0; s < 8; ++s) {
                        sumN[k][s] += ev.n[s];
                        if (ev.tfirst[s] > 0) { sumT[k][s] += ev.tfirst[s]; ++cntT[k][s]; }
                    }
                }
                TH1D* h0 = new TH1D(TString::Format("obsA%d_%d", refl, ic), "", 16, -0.5, 15.5);
                TH1D* h1 = new TH1D(TString::Format("obsB%d_%d", refl, ic), "", 16, -0.5, 15.5);
                h0->SetDirectory(nullptr); h1->SetDirectory(nullptr);
                double ymax = 0;
                double val0[8] = {0}, val1[8] = {0};
                for (int s = 0; s < 8; ++s) {
                    double v0 = 0, v1 = 0;
                    if (amp) {
                        double tot0 = 0, tot1 = 0;
                        for (int q = 0; q < 8; ++q) { tot0 += sumN[0][q]; tot1 += sumN[1][q]; }
                        v0 = (cnt[0] > 0 && tot0 > 0) ? 100.0 * sumN[0][s] / tot0 - 12.5 : 0.0;
                        v1 = (cnt[1] > 0 && tot1 > 0) ? 100.0 * sumN[1][s] / tot1 - 12.5 : 0.0;
                    } else {
                        double m0 = 0, m1 = 0; int k0 = 0, k1 = 0;
                        for (int q = 0; q < 8; ++q) {
                            if (cntT[0][q] > 0) { m0 += sumT[0][q] / cntT[0][q]; ++k0; }
                            if (cntT[1][q] > 0) { m1 += sumT[1][q] / cntT[1][q]; ++k1; }
                        }
                        m0 = k0 ? m0 / k0 : 0.0;  m1 = k1 ? m1 / k1 : 0.0;
                        v0 = (cntT[0][s] > 0) ? (sumT[0][s] / cntT[0][s] - m0) : 0.0;
                        v1 = (cntT[1][s] > 0) ? (sumT[1][s] / cntT[1][s] - m1) : 0.0;
                    }
                    val0[s] = v0; val1[s] = v1;
                    ymax = std::max(ymax, std::max(std::fabs(v0), std::fabs(v1)));
                    if (ic == 0) {   // light share [percentage points from 12.5]
                        classObs.push_back({(double)refl, 0.0, (double)s, v0});
                        classObs.push_back({(double)refl, 1.0, (double)s, v1});
                    } else {         // first-arrival offset [ns]
                        classObs.push_back({(double)refl, 2.0, (double)s, v0});
                        classObs.push_back({(double)refl, 3.0, (double)s, v1});
                    }
                }
                // frame (creates the axis) + real bars anchored at zero
                h0->SetTitle(TString::Format("%s: %s", kReflName[refl],
                                             amp ? "light share per SiPM (deviation from 1/8)"
                                                 : "first-arrival time (relative to the event mean)"));
                for (int s = 0; s < 8; ++s) {
                    h0->GetXaxis()->SetBinLabel(s + 1, kSipm[s].name);
                    h0->GetXaxis()->SetBinLabel(s + 9, kSipm[s].name);
                }
                h0->GetXaxis()->SetTitle("SiPM   (left group: central muon,  right group: corner muon)");
                h0->GetYaxis()->SetTitle(amp ? "share - 12.5 [%]" : "arrival-time offset [ns]");
                h0->Draw();
                h0->GetYaxis()->SetRangeUser(std::min(0.0, -ymax * 1.25), ymax * 1.25);
                h0->GetXaxis()->SetRangeUser(-0.6, 15.6);
                TBox* bar0 = nullptr; TBox* bar1 = nullptr;
                for (int s = 0; s < 8; ++s) {
                    const double v0 = val0[s];
                    const double v1 = val1[s];
                    TBox* b0 = new TBox(s - 0.40, std::min(0.0, v0), s + 0.40, std::max(0.0, v0));
                    b0->SetFillColor(kColor[0]); b0->SetFillStyle(1001);
                    b0->SetLineColor(kBlack); b0->Draw("SAME");
                    TBox* b1 = new TBox(s + 8 - 0.40, std::min(0.0, v1), s + 8 + 0.40, std::max(0.0, v1));
                    b1->SetFillColor(kColor[1]); b1->SetFillStyle(1001);
                    b1->SetLineColor(kBlack); b1->Draw("SAME");
                    if (!bar0) { bar0 = b0; bar1 = b1; }
                }
                TLegend* leg = new TLegend(0.66, 0.76, 0.95, 0.89);
                leg->AddEntry(bar0, "central muon", "f");
                leg->AddEntry(bar1, "corner muon", "f");
                leg->SetBorderSize(0);
                leg->Draw();
            }
        }
        c->SaveAs(TString::Format("%s/fig_recon_observables.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_observables.png", outdir));
    }

    // --------------------------------------- F3: resolution per estimator
    {
        TCanvas* c = new TCanvas("fig_recon_features", "", 1000, 640);
        c->SetLeftMargin(0.19);
        c->SetRightMargin(0.30);
        TH1D* frame = new TH1D("featframe", "", kNFeat, -0.5, kNFeat - 0.5);
        frame->SetDirectory(nullptr);
        double ymax = 0;
        for (int f = 0; f < kNFeat; ++f)
            for (int r = 0; r < 4; ++r) ymax = std::max(ymax, res[r][f].sr);
        frame->SetTitle("Position resolution of the config-1 read-out (whole-plate data)");
        frame->GetYaxis()->SetTitle("rms radial error [mm]");
        frame->GetXaxis()->SetTitle("information channel used by the estimator");
        for (int f = 0; f < kNFeat; ++f)
            frame->GetXaxis()->SetBinLabel(f + 1, kFeatName[f]);
        frame->Draw();
        frame->GetYaxis()->SetRangeUser(0, ymax * 1.15);
        TLegend* leg = new TLegend(0.74, 0.42, 0.99, 0.88);
        for (int r = 0; r < 4; ++r) {
            TGraph* g = new TGraph(kNFeat);
            for (int f = 0; f < kNFeat; ++f)
                g->SetPoint(f, f + 0.06 * (r - 1.5), res[r][f].sr);
            g->SetMarkerStyle(20);
            g->SetMarkerSize(1.6);
            g->SetMarkerColor(kColor[r]);
            g->SetLineColor(kColor[r]);
            g->SetLineWidth(2);
            g->Draw("P SAME");
            leg->AddEntry(g, kReflName[r], "p");
        }
        leg->SetBorderSize(0);
        leg->Draw();
        frame->GetYaxis()->SetRangeUser(0, ymax * 1.15);
        c->SaveAs(TString::Format("%s/fig_recon_features.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_features.png", outdir));
    }

    // ------------------------------------------- F4: position response curves
    std::vector<int> best(4);
    for (int r = 0; r < 4; ++r) {
        best[r] = 0;
        for (int f = 1; f < kNFeat; ++f)
            if (res[r][f].sr < res[r][best[r]].sr) best[r] = f;
    }
    {
        TCanvas* c = new TCanvas("fig_recon_response", "", 1400, 760);
        c->Divide(4, 2);
        const int nb = 10;                      // 25 mm bins
        for (int r = 0; r < 4; ++r) {
            const EstResult& e = res[r][best[r]];
            for (int ax = 0; ax < 2; ++ax) {
                c->cd(ax * 4 + r + 1);
                const std::vector<double>& T = (ax == 0) ? e.tx : e.ty;
                const std::vector<double>& P = (ax == 0) ? e.px : e.py;
                std::vector<double> ct(nb, 0), cp(nb, 0), cr(nb, 0);
                std::vector<int> cn(nb, 0);
                for (size_t i = 0; i < T.size(); ++i) {
                    int b = (int)std::floor((T[i] + kHalf) / (2 * kHalf / nb));
                    b = std::max(0, std::min(nb - 1, b));
                    ct[b] += T[i]; cp[b] += P[i]; ++cn[b];
                }
                for (int b = 0; b < nb; ++b) {
                    if (!cn[b]) continue;
                    ct[b] /= cn[b]; cp[b] /= cn[b];
                    double ss = 0;
                    for (size_t i = 0; i < T.size(); ++i) {
                        int bb = (int)std::floor((T[i] + kHalf) / (2 * kHalf / nb));
                        bb = std::max(0, std::min(nb - 1, bb));
                        if (bb == b) { const double d = P[i] - T[i]; ss += d * d; }
                    }
                    cr[b] = std::sqrt(ss / cn[b]);
                }
                TH1D* frame = new TH1D(TString::Format("respframe%d_%d", r, ax), "",
                                       2, -kHalf, kHalf);
                frame->SetDirectory(nullptr);
                frame->GetXaxis()->SetRangeUser(-kHalf, kHalf);
                frame->GetYaxis()->SetRangeUser(-kHalf, kHalf);
                frame->Draw();
                TGraphErrors* g = new TGraphErrors(nb);
                for (int b = 0; b < nb; ++b) {
                    if (!cn[b]) { g->SetPoint(b, 0, 0); continue; }
                    g->SetPoint(b, ct[b], cp[b]);
                    g->SetPointError(b, 0, cr[b]);
                }
                g->SetMarkerStyle(20);
                g->SetMarkerSize(1.2);
                g->SetLineColor(kColor[r]);
                g->SetLineWidth(2);
                g->Draw("P SAME");
                // shared y axis: only the left-most pad of each row carries the title
                if (r == 0) {
                    frame->GetYaxis()->SetTitle(ax == 0 ? "reconstructed x [mm]" : "reconstructed y [mm]");
                    ((TPad*)gPad)->SetLeftMargin(0.24);
                } else {
                    ((TPad*)gPad)->SetLeftMargin(0.14);
                }
                frame->GetXaxis()->SetTitle(ax == 0 ? "true x [mm]" : "true y [mm]");
                frame->SetTitle(TString::Format("%s, %s", kReflName[r], kFeatName[best[r]]));
                TGraph* diag = new TGraph(2);
                diag->SetPoint(0, -kHalf, -kHalf);
                diag->SetPoint(1, kHalf, kHalf);
                diag->SetLineColor(kGray + 2);
                diag->SetLineStyle(7);
                diag->Draw("L SAME");
            }
        }
        c->SaveAs(TString::Format("%s/fig_recon_response.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_response.png", outdir));
    }

    // ------------------------------------------ F5: map of the resolution
    {
        TCanvas* c = new TCanvas("fig_recon_map", "", 1150, 900);
        c->Divide(2, 2);
        const double margin = 18.0;
        const double span = kHalf + margin;
        const int    nIn  = 10;                  // 25 mm cells across the plate
        const double step = 2 * kHalf / nIn;
        std::vector<double> edges;
        edges.push_back(-span);
        for (int k = 0; k <= nIn; ++k) edges.push_back(-kHalf + k * step);
        edges.push_back(span);
        const int nb = (int)edges.size() - 1;
        for (int r = 0; r < 4; ++r) {
            c->cd(r + 1);
            const EstResult& e = res[r][best[r]];
            TH2D* h = new TH2D(TString::Format("map%d", r), "", nb, edges.data(),
                               nb, edges.data());
            h->SetDirectory(nullptr);
            std::vector<double> ss(nIn * nIn, 0.0);
            std::vector<int> cn(nIn * nIn, 0);
            for (size_t i = 0; i < e.tx.size(); ++i) {
                const double rx = e.tx[i], ry = e.ty[i];
                int ix = (int)std::floor((rx + kHalf) / step);   // 0 .. nIn-1
                int iy = (int)std::floor((ry + kHalf) / step);
                ix = std::max(0, std::min(nIn - 1, ix));
                iy = std::max(0, std::min(nIn - 1, iy));
                const double dx = e.px[i] - rx, dy = e.py[i] - ry;
                ss[iy * nIn + ix] += dx * dx + dy * dy;
                ++cn[iy * nIn + ix];
            }
            double vmax = 0;
            for (int iy = 0; iy < nIn; ++iy)
                for (int ix = 0; ix < nIn; ++ix) {
                    const int k = iy * nIn + ix;
                    if (cn[k] < 20) continue;
                    const double v = std::sqrt(ss[k] / cn[k]);
                    h->SetBinContent(ix + 2, iy + 2, v);   // +2: skip the margin bins
                    vmax = std::max(vmax, v);
                }
            h->SetTitle(TString::Format("%s, %s: rms radial error", kReflName[r],
                                        kFeatName[best[r]]));
            h->GetZaxis()->SetTitle("rms radial error [mm]");
            h->GetXaxis()->SetTitle("true x [mm] (report frame)");
            h->GetYaxis()->SetTitle("true y [mm] (report frame)");
            h->SetMaximum(vmax);
            h->Draw("COLZ");
            // slab outline and SiPM markers, drawn outboard as in Figs. 20-23
            TBox* slab = new TBox(-kHalf, -kHalf, kHalf, kHalf);
            slab->SetFillStyle(0);
            slab->SetLineColor(kBlack);
            slab->SetLineWidth(2);
            slab->Draw("SAME");
            const double side = 8.0, gap = 2.0, off = gap + 0.5 * side;
            for (int s = 0; s < 8; ++s) {
                double cx = kSipm[s].x, cy = kSipm[s].y;
                const double tol = 1e-6;
                if      (std::fabs(kSipm[s].x - kHalf) < tol) cx =  kHalf + off;
                else if (std::fabs(kSipm[s].x + kHalf) < tol) cx = -kHalf - off;
                if      (std::fabs(kSipm[s].y - kHalf) < tol) cy =  kHalf + off;
                else if (std::fabs(kSipm[s].y + kHalf) < tol) cy = -kHalf - off;
                TBox* b = new TBox(cx - 0.5 * side, cy - 0.5 * side,
                                   cx + 0.5 * side, cy + 0.5 * side);
                b->SetFillColor(kBlack);
                b->SetLineColor(kBlack);
                b->Draw("SAME");
            }
        }
        c->SaveAs(TString::Format("%s/fig_recon_map.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_map.png", outdir));
    }

    {
        std::ofstream csv(TString::Format("%s/position_class_observables.csv", outdir).Data());
        csv << "wrapper,population,sipm,value\n";
        const char* pop[4] = {"central muon, light share - 12.5 [pp]",
                              "corner muon, light share - 12.5 [pp]",
                              "central muon, first-arrival offset [ns]",
                              "corner muon, first-arrival offset [ns]"};
        for (const auto& r : classObs)
            csv << kReflPlain[(int)r[0]] << "," << pop[(int)r[1]] << ","
                << kSipm[(int)r[2]].name << "," << r[3] << "\n";
    }

    std::cout << "[position_reconstruction] done" << std::endl;
}
