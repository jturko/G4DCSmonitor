// position_reconstruction.C -- can the relative SiPM signal pattern localise
// the muon inside the plate?
//
// This macro answers the question "can the relative timing/amplitude
// information of the SiPM array be used to determine the interaction position
// of the incident muon?" for the whole-plate irradiation of data_v7 (25 000
// vertical 4 GeV muons uniformly covering the 250 x 250 mm plate, per reflector
// treatment).
//
// For every event the macro extracts, per SiPM, the observables that a real
// read-out chain would provide:
//
//   N        number of detected photoelectrons                    (pulse integral:
//             the total charge collected in the channel over the whole event,
//             NOT the amplitude of the pulse peak)
//   Tfirst   time of the first detected photoelectron             (0% onset)
//   Tcfd     12.5% optical constant-fraction (CFD) quantile time  (point quantile)
//   Tcfdm    12.5% optical CFD wavefront mean                     (L-estimator mean
//             of the earliest 12.5% of photoelectrons in the channel)
//   Tbar     mean arrival time of the detected photoelectrons
//   NP3      number of photoelectrons arriving before 3 ns         (prompt light)
//   Wbar     mean detected wavelength of the detected photoelectrons
//
// The ground truth is the muon's first-entry position in the slab
// (muonX/muonY, slab-local mm) that the sipmHits tree carries.  It is
// converted to the report plan view (top = +y, left = -x) with the usual
// (x,y) -> (y,-x) rotation, so that all the maps produced here are directly
// comparable with the collection-efficiency maps of Figs. 13-16.
//
// Two families of estimator are evaluated on a held-out test set:
//
//  (a) a transparent, purely analytic one: the "opposite-pair" observables
//      A_x = (N_right - N_left)/(N_right + N_left),
//      T_x = <tfirst>_right - <tfirst>_left, and
//      T_cfd,x = <tcfd_12.5%>_right - <tcfd_12.5%>_left,
//      whose linear response to the position is measured directly, so that
//      the achievable resolution is (noise)/(slope);
//
//  (b) a learned one: a D4-symmetry-equivariant, inverse-variance-weighted
//      tricube-kernel k-nearest-neighbour regression in the standardised
//      feature space.  Exploiting the 8-fold dihedral symmetry group D4 of the
//      square plate increases the effective training density 8-fold (from
//      20 000 to 160 000 training patterns), while block-relevance metric
//      weights prevent noisy feature subsets from diluting informative ones.
//
// Outputs (in outdir):
//   fig_recon_waveforms.{pdf,png}   per-SiPM waveforms, central vs corner muon
//   fig_recon_observables.{pdf,png} per-SiPM light share and arrival time
//   fig_recon_features.{pdf,png}    resolution per estimator and wrapper
//   fig_recon_response.{pdf,png}    position response (predicted vs true)
//   fig_recon_map.{pdf,png}         map of the position resolution
//   fig_recon_alpha_scan.{pdf,png}  resolution vs the constant fraction alpha
//   position_reconstruction.csv      estimator resolution table
//   position_observables.csv         opposite-pair linear-response table
//   position_alpha_scan.csv          constant-fraction scan table
//
// Usage (batch):
//   root -l -b -q 'macros/position_reconstruction.C("data_v7/whole","report/figures",25000,1,"",1)'
//
// Args: dir (default: the data_v7 whole tree), outdir (default "report/figures"),
//       maxEvents (default 25000 = all events in the file),
//       doAlphaScan (default 1 = also scan the constant fraction alpha),
//       alphaCsv (default "" = compute; otherwise redraw alpha scan from CSV),
//       preset (default 1 = Config 1; also supported: 8, 10, 11).
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
#include <TText.h>
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
#include <sstream>
#include <string>
#include <vector>

// Shared ROOT style (fonts, ticks, palette).  Located via ROOT_INCLUDE_PATH
// (set by macros/build_report_figures.sh to ${HOME}/root-work).
#include "style.h"

namespace {

// ---------------------------------------------------------------------------
// Geometry tables for several SiPM presets, in the REPORT plan view
// (top = +y, left = -x).  edge = 0..3 labels the side (bottom/right/top/left
// in report frame); u is the along-edge fractional coordinate; v is the
// across-edge coordinate (always +-kHalf).  The report-frame (x,y) are
// reconstructed from (edge,u) and are the positions after the usual
// slab-local -> report (x,y) -> (y,-x) rotation.
// ---------------------------------------------------------------------------
const double kHalf  = 125.0;    // slab half-width [mm]

struct SipmGeom { int edge; double u, v; double x, y; const char* name; };

std::vector<SipmGeom> GetSipmTable(int preset)
{
    std::vector<SipmGeom> s;
    if (preset == 1) {
        // Config 1: 8 SiPMs, 2/side at u = +-0.5
        s.push_back({0, -0.50, -kHalf, -62.5, -kHalf, "B1"});
        s.push_back({0, +0.50, -kHalf, +62.5, -kHalf, "B2"});
        s.push_back({1, -0.50, +kHalf, -62.5, +kHalf, "T2"});
        s.push_back({1, +0.50, +kHalf, +62.5, +kHalf, "T1"});
        s.push_back({2, +0.50, +kHalf, +kHalf, +62.5, "R2"});
        s.push_back({2, -0.50, +kHalf, +kHalf, -62.5, "R1"});
        s.push_back({3, +0.50, -kHalf, -kHalf, +62.5, "L1"});
        s.push_back({3, -0.50, -kHalf, -kHalf, -62.5, "L2"});
    } else if (preset == 8) {
        // Config 8: 8 SiPMs, 2/side at u = +-0.75
        s.push_back({0, -0.75, -kHalf, -93.75, -kHalf, "B1"});
        s.push_back({0, +0.75, -kHalf, +93.75, -kHalf, "B2"});
        s.push_back({1, -0.75, +kHalf, -93.75, +kHalf, "T2"});
        s.push_back({1, +0.75, +kHalf, +93.75, +kHalf, "T1"});
        s.push_back({2, +0.75, +kHalf, +kHalf, +93.75, "R2"});
        s.push_back({2, -0.75, +kHalf, +kHalf, -93.75, "R1"});
        s.push_back({3, +0.75, -kHalf, -kHalf, +93.75, "L1"});
        s.push_back({3, -0.75, -kHalf, -kHalf, -93.75, "L2"});
    } else if (preset == 10) {
        // Preset 10: 4 SiPMs, 1/side at one end of each WLS bar
        s.push_back({0, -0.75, -kHalf, -93.75, -kHalf, "B1"});
        s.push_back({1, +0.75, +kHalf, +93.75, +kHalf, "T1"});
        s.push_back({2, -0.75, +kHalf, +kHalf, -93.75, "R1"});
        s.push_back({3, +0.75, -kHalf, -kHalf, +93.75, "L1"});
    } else if (preset == 11) {
        // Preset 11: 8 SiPMs, 2/side at both ends of each WLS bar
        s.push_back({0, -0.75, -kHalf, -93.75, -kHalf, "B1"});
        s.push_back({0, +0.75, -kHalf, +93.75, -kHalf, "B2"});
        s.push_back({1, +0.75, +kHalf, +93.75, +kHalf, "T1"});
        s.push_back({1, -0.75, +kHalf, -93.75, +kHalf, "T2"});
        s.push_back({2, -0.75, +kHalf, +kHalf, -93.75, "R1"});
        s.push_back({2, +0.75, +kHalf, +kHalf, +93.75, "R2"});
        s.push_back({3, +0.75, -kHalf, -kHalf, +93.75, "L1"});
        s.push_back({3, -0.75, -kHalf, -kHalf, -93.75, "L2"});
    }
    return s;
}

// Globals set from the chosen preset in position_reconstruction().
static int gPreset = 1;
static int gNSiPM  = 8;
static std::vector<SipmGeom> gSipm = GetSipmTable(1);

// Compile-time maximum (Preset 9 deferred).
static const int kMaxSiPM = 12;


const char* kReflName[4] = {"bare", "TiO2", "Al (air)", "glossy"};
const char* kReflPlain[4] = {"bare", "TiO2", "Al (air)", "glossy"};

// Constant fractions scanned in the alpha-scan study.  The "alpha wavefront
// time" of a channel is the mean arrival time of the leading alpha fraction of
// the photoelectrons collected in that channel; the production results use
// kAlphaNom = 0.125.
const int    kNAlpha   = 9;
const double kAlpha[kNAlpha] = {0.03, 0.05, 0.075, 0.10, 0.125, 0.15, 0.20, 0.30, 0.50};
const int    kAlphaNom = 4;      // index of alpha = 0.125

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
    double x = 0, y = 0;                // truth: muon entry, REPORT frame mm
    int    n[kMaxSiPM]     = {0};       // detected photoelectrons per SiPM
    double tfirst[kMaxSiPM] = {0};      // first arrival time [ns] (0 = nothing seen)
    double tcfd[kMaxSiPM]   = {0};      // 12.5% optical quantile CFD time [ns] (point quantile)
    double tcfdm[kMaxSiPM]  = {0};      // 12.5% optical CFD wavefront mean [ns] (mean of earliest 12.5% of PEs)
    double tcfdp[kMaxSiPM]  = {0};      // 12.5% prompt-gated optical CFD wavefront time [ns] (t < 4.5 ns)
    // the same three timing observables for every scanned constant fraction
    double tcfdA[kNAlpha][kMaxSiPM]  = {};
    double tcfdmA[kNAlpha][kMaxSiPM] = {};
    double tcfdpA[kNAlpha][kMaxSiPM] = {};
    double tbar[kMaxSiPM]   = {0};      // mean arrival time [ns]
    int    np3[kMaxSiPM]    = {0};      // photoelectrons arriving before 3 ns
    double wbar[kMaxSiPM]   = {0};      // mean detected wavelength [nm]
};

struct WaveSet {
    std::vector<double> h[kNClass][kMaxSiPM];   // accumulated time histograms
    long n[kNClass] = {0, 0};
    WaveSet() {
        for (int c = 0; c < kNClass; ++c)
            for (int s = 0; s < kMaxSiPM; ++s) h[c][s].assign(kWaveBins, 0.0);
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

    const int nSiPM = (int)t->GetMaximum("sipm") + 1;

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
    std::vector<std::vector<double>> evBuf(kMaxSiPM, std::vector<double>(kWaveBins, 0.0));

    auto flushEvent = [&]() {
        if (!have) return;
        const int cls = Classify(cur);
        if (cls >= 0) {
            out.wave.n[cls]++;
            for (int s = 0; s < nSiPM; ++s)
                for (int b = 0; b < kWaveBins; ++b) out.wave.h[cls][s][b] += evBuf[s][b];
        }
        // prompt fraction of the collected light (PE arriving before 3 ns)
        for (int s = 0; s < nSiPM; ++s) { out.sumTot += cur.n[s]; out.sumP3 += cur.np3[s]; }
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
            for (int s = 0; s < nSiPM; ++s) evBuf[s].assign(kWaveBins, 0.0);
            have = true;
        }
        if (sipm < 0 || sipm >= nSiPM) continue;
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

            // Sort arrival times and extract the order-statistic timing
            // observables for every scanned constant fraction alpha: the
            // alpha-quantile time, the leading-alpha wavefront mean and the
            // tapered (Beta(1,3)) wavefront time.
            std::vector<double> st = tt;
            std::sort(st.begin(), st.end());
            cur.tfirst[sipm] = st[0];
            const size_t M = st.size();
            for (int ai = 0; ai < kNAlpha; ++ai) {
                const double alpha = kAlpha[ai];
                // (a) Interpolated alpha-quantile arrival time t_{alpha}
                const double u = std::max(0.0, alpha * (double)M - 1.0);
                const size_t j0 = std::min(M - 1, (size_t)std::floor(u));
                const size_t j1 = std::min(M - 1, j0 + 1);
                const double gam = u - (double)j0;
                cur.tcfdA[ai][sipm] = (1.0 - gam) * st[j0] + gam * st[j1];
                // (b) Truncated leading-alpha wavefront mean \bar{t}_{<= alpha}
                const double q = std::max(1.0, alpha * (double)M);
                const size_t m = std::min(M, (size_t)std::floor(q));
                const double rem = q - (double)m;
                double sLead = 0.0;
                for (size_t j = 0; j < m; ++j) sLead += st[j];
                if (rem > 0.0 && m < M) sLead += rem * st[m];
                cur.tcfdmA[ai][sipm] = sLead / q;
                // (c) Tapered alpha wavefront time: a Beta(1,3) kernel over
                //     the leading alpha quantile u in [0, alpha] that averages
                //     the earliest alpha of the photoelectrons while vanishing
                //     smoothly at u = alpha (eliminating integer-rank step
                //     discontinuities).
                const size_t m2 = std::min(M, (size_t)std::ceil(q));
                double sw = 0.0, stw = 0.0;
                for (size_t j = 0; j < m2; ++j) {
                    // integrate 3*(1 - u/q)^2 over u in [j, min(j+1, q)]
                    const double u0 = (double)j / q;
                    const double u1 = std::min(1.0, (double)(j + 1) / q);
                    const double a0 = 1.0 - u0, a1 = 1.0 - u1;
                    const double wj = (a0 * a0 * a0 - a1 * a1 * a1);
                    stw += wj * st[j];
                    sw  += wj;
                }
                cur.tcfdpA[ai][sipm] = (sw > 0.0) ? (stw / sw) : st[0];
            }
            // the production observables (alpha = 12.5 %)
            cur.tcfd[sipm]  = cur.tcfdA[kAlphaNom][sipm];
            cur.tcfdm[sipm] = cur.tcfdmA[kAlphaNom][sipm];
            cur.tcfdp[sipm] = cur.tcfdpA[kAlphaNom][sipm];
        }
    }
    flushEvent();
    f.Close();
    return true;
}

// ---------------------------------------------------------------------------
// D4 dihedral symmetry group of the square plate (8 elements: 4 rotations x 2 reflections)
// ---------------------------------------------------------------------------
struct D4Elem {
    double sx, sy;
    bool   swapXY;
    std::vector<int> perm;   // sensor s moves to perm[s] under T_g
};

std::array<D4Elem, 8> BuildD4Group(const std::vector<SipmGeom>& sipm)
{
    const int n = (int)sipm.size();
    std::array<D4Elem, 8> G{};
    for (int g = 0; g < 8; ++g) {
        G[g].sx     = (g & 1) ? -1.0 : +1.0;
        G[g].sy     = (g & 2) ? -1.0 : +1.0;
        G[g].swapXY = (g & 4) != 0;
        G[g].perm.assign(n, 0);
        for (int s = 0; s < n; ++s) {
            const double x0 = sipm[s].x, y0 = sipm[s].y;
            const double xp = G[g].swapXY ? (G[g].sx * y0) : (G[g].sx * x0);
            const double yp = G[g].swapXY ? (G[g].sy * x0) : (G[g].sy * y0);
            int dst = s;
            for (int q = 0; q < n; ++q) {
                if (std::fabs(sipm[q].x - xp) < 1e-3 && std::fabs(sipm[q].y - yp) < 1e-3) {
                    dst = q;
                    break;
                }
            }
            G[g].perm[s] = dst;
        }
    }
    return G;
}

// Build the list of SiPM indices on a given edge (report frame).
std::vector<int> GetEdgeGroup(int edge)
{
    std::vector<int> g;
    for (int s = 0; s < gNSiPM; ++s)
        if (gSipm[s].edge == edge) g.push_back(s);
    return g;
}

// ---------------------------------------------------------------------------
// Features
// ---------------------------------------------------------------------------
enum Feat { kAmp = 0, kTimeFirst = 1, kTimeCFD = 2, kPrompt = 3, kAll = 4, kNFeat = 5 };
const char* kFeatName[kNFeat] = {
    "pulse integral",
    "first arrival",
    "optical CFD (12.5%)",
    "prompt fraction",
    "combined"
};

void MakeFeatures(const std::vector<Event>& evs, int kind,
                  std::vector<std::vector<double>>& X,
                  int alphaIdx = kAlphaNom)
{
    const int N = gNSiPM;
    const size_t n = evs.size();
    X.assign(n, std::vector<double>());
    std::vector<double> amp(N), tf(N), tq(N), tm(N), pv(N);
    for (size_t e = 0; e < n; ++e) {
        const Event& ev = evs[e];

        // (i) pulse-integral shares (total collected charge per channel),
        //     centred on the event mean
        double tot = 0;
        for (int s = 0; s < N; ++s) tot += ev.n[s];
        double amean = 0;
        if (tot > 0) {
            for (int s = 0; s < N; ++s) { amp[s] = ev.n[s] / tot; amean += amp[s]; }
        }
        amean /= (double)N;
        for (int s = 0; s < N; ++s) amp[s] -= amean;

        // (ii) relative arrival times (common event offset removed):
        //      tf = first-photoelectron arrival time
        //      tq = 12.5% prompt-gated optical CFD wavefront time
        //      tm = 12.5% full-pulse optical CFD wavefront time
        auto relTime = [&](const double* tarr, std::vector<double>& out) {
            double tsum = 0;
            int tn = 0;
            for (int s = 0; s < N; ++s) if (tarr[s] > 0) { tsum += tarr[s]; ++tn; }
            for (int s = 0; s < N; ++s)
                out[s] = (tn > 0 && tarr[s] > 0) ? tarr[s] - tsum / tn : 0.0;
        };
        relTime(ev.tfirst, tf);
        relTime(ev.tcfdpA[alphaIdx], tq);
        relTime(ev.tcfdmA[alphaIdx], tm);

        // (iii) prompt fraction (photoelectrons before 3 ns), centred
        double ptot = 0;
        for (int s = 0; s < N; ++s) ptot += ev.np3[s];
        double pmean = 0;
        if (ptot > 0) {
            for (int s = 0; s < N; ++s) { pv[s] = ev.np3[s] / ptot; pmean += pv[s]; }
        }
        pmean /= (double)N;
        for (int s = 0; s < N; ++s) pv[s] -= pmean;

        std::vector<double>& f = X[e];
        f.clear();
        if (kind == kAmp       || kind == kAll) f.insert(f.end(), amp.begin(), amp.end());
        if (kind == kTimeFirst || kind == kAll) f.insert(f.end(), tf.begin(),  tf.end());
        if (kind == kTimeCFD) {
            f.insert(f.end(), tf.begin(), tf.end());
            f.insert(f.end(), tm.begin(), tm.end());
        }
        if (kind == kAll) {
            f.insert(f.end(), tq.begin(), tq.end());
            f.insert(f.end(), tm.begin(), tm.end());
        }
        if (kind == kPrompt    || kind == kAll) f.insert(f.end(), pv.begin(),  pv.end());

        // (iv) Preset 11: along-bar differential features for dual-ended WLS bars.
        //     Encoded as two nSiPM-wide antisymmetric blocks so the block
        //     structure of KnnPredict stays uniform: each bar's two ends get
        //     +/- the differential, preserving the "one feature per bar" intent
        //     while keeping every block nSiPM-wide.
        if (gPreset == 11 && (kind == kTimeCFD || kind == kAll)) {
            std::vector<double> dt(N, 0.0), da(N, 0.0);
            for (int edge = 0; edge < 4; ++edge) {
                std::vector<int> ids = GetEdgeGroup(edge);
                if ((int)ids.size() != 2) continue;
                // order by along-edge coordinate (x for horizontal, y for vertical)
                if (edge == 0 || edge == 1) {
                    if (gSipm[ids[0]].x > gSipm[ids[1]].x) std::swap(ids[0], ids[1]);
                } else {
                    if (gSipm[ids[0]].y > gSipm[ids[1]].y) std::swap(ids[0], ids[1]);
                }
                const int i1 = ids[0], i2 = ids[1];
                const double dtev = (ev.tcfd[i2] > 0 && ev.tcfd[i1] > 0) ? ev.tcfd[i2] - ev.tcfd[i1] : 0.0;
                const double ns = ev.n[i1] + ev.n[i2];
                const double daval = ns > 0 ? (double)(ev.n[i2] - ev.n[i1]) / ns : 0.0;
                dt[i1] = +dtev; dt[i2] = -dtev;
                da[i1] = +daval; da[i2] = -daval;
            }
            f.insert(f.end(), dt.begin(), dt.end());
            f.insert(f.end(), da.begin(), da.end());
        }
    }
}

// ---------------------------------------------------------------------------
// Symmetry-augmented, relevance-weighted kernel k-NN regression
// ---------------------------------------------------------------------------
void KnnPredict(const std::vector<std::vector<double>>& Xtr,
                const std::vector<double>& Yxtr,
                const std::vector<double>& Yytr,
                const std::vector<std::vector<double>>& Xte,
                int k,
                std::vector<double>& px, std::vector<double>& py,
                bool useSymmetry = true,
                const std::vector<double>& blockWeights = {},
                int nSiPM = -1)
{
    if (nSiPM < 0) nSiPM = gNSiPM;
    const size_t ntr = Xtr.size(), nte = Xte.size();
    if (ntr == 0 || nte == 0) return;
    const size_t dim = Xtr[0].size();
    const size_t nBlocks = dim / nSiPM;
    const auto G = BuildD4Group(gSipm);
    const int nSym = useSymmetry ? 8 : 1;

    // Standardise per nSiPM-sensor block so D4 permutations commute with scaling
    std::vector<double> mu(dim, 0.0), sd(dim, 0.0), wSqrt(dim, 1.0);
    for (size_t b = 0; b < nBlocks; ++b) {
        double s = 0, ss = 0;
        const size_t cnt = ntr * nSiPM;
        for (size_t i = 0; i < ntr; ++i)
            for (int j = 0; j < nSiPM; ++j) s += Xtr[i][b * nSiPM + j];
        const double m = s / cnt;
        for (size_t i = 0; i < ntr; ++i)
            for (int j = 0; j < nSiPM; ++j) {
                const double d = Xtr[i][b * nSiPM + j] - m;
                ss += d * d;
            }
        double sig = std::sqrt(ss / cnt);
        if (sig <= 0) sig = 1.0;
        const double wb = (b < blockWeights.size()) ? std::sqrt(std::max(0.0, blockWeights[b])) : 1.0;
        for (int j = 0; j < nSiPM; ++j) {
            mu[b * nSiPM + j]    = m;
            sd[b * nSiPM + j]    = sig;
            wSqrt[b * nSiPM + j] = wb;
        }
    }

    // Compact L3-resident training matrix (ntr x dim floats, ~1.9 MB)
    std::vector<float> A(ntr * dim, 0.0f);
    for (size_t i = 0; i < ntr; ++i)
        for (size_t j = 0; j < dim; ++j)
            A[i * dim + j] = (float)((Xtr[i][j] - mu[j]) / sd[j] * wSqrt[j]);

    std::vector<float> B(nte * dim, 0.0f);
    for (size_t t = 0; t < nte; ++t)
        for (size_t j = 0; j < dim; ++j)
            B[t * dim + j] = (float)((Xte[t][j] - mu[j]) / sd[j] * wSqrt[j]);

    px.assign(nte, 0.0);
    py.assign(nte, 0.0);

    const int kk = std::min<int>(k, (int)ntr);
    std::vector<std::pair<float, int>> d(ntr);
    std::vector<std::vector<float>> qSym(nSym, std::vector<float>(dim));
    for (size_t t = 0; t < nte; ++t) {
        const float* qt = &B[t * dim];
        // Precompute the 8 D4 symmetry permutations of the query vector in L1 cache:
        // ||Pi_g(z_i) - q_t||^2 == ||z_i - Pi_g^{-1}(q_t)||^2
        for (int g = 0; g < nSym; ++g) {
            for (size_t b = 0; b < nBlocks; ++b)
                for (int s = 0; s < nSiPM; ++s)
                    qSym[g][b * nSiPM + s] = qt[b * nSiPM + G[g].perm[s]];
        }

        for (size_t i = 0; i < ntr; ++i) {
            const float* ar = &A[i * dim];
            float bestD2 = 1e30f;
            int   bestG  = 0;
            for (int g = 0; g < nSym; ++g) {
                const float* qg = qSym[g].data();
                float s = 0.0f;
                for (size_t j = 0; j < dim; ++j) {
                    const float dd = ar[j] - qg[j];
                    s += dd * dd;
                }
                if (s < bestD2) { bestD2 = s; bestG = g; }
            }
            d[i] = {bestD2, (int)(i * nSym + (size_t)bestG)};
        }
        std::nth_element(d.begin(), d.begin() + kk, d.end());

        // Tricube distance-kernel weighting inside the k-nearest ball
        float maxD2 = 0.0f;
        for (int m = 0; m < kk; ++m) if (d[m].first > maxD2) maxD2 = d[m].first;
        const double dMax = std::sqrt(std::max(1e-12, (double)maxD2)) * 1.05;
        double sw = 0.0, sx = 0.0, sy = 0.0;
        for (int m = 0; m < kk; ++m) {
            const double u = std::min(1.0, std::sqrt(std::max(0.0, (double)d[m].first)) / dMax);
            const double t3 = 1.0 - u * u * u;
            const double w = useSymmetry ? (t3 * t3 * t3 + 1e-3) : 1.0;
            const size_t i = (size_t)(d[m].second / nSym);
            const int    g = d[m].second % nSym;
            const double x0 = Yxtr[i], y0 = Yytr[i];
            const double xg = G[g].swapXY ? (G[g].sx * y0) : (G[g].sx * x0);
            const double yg = G[g].swapXY ? (G[g].sy * x0) : (G[g].sy * y0);
            sx += w * xg;
            sy += w * yg;
            sw += w;
        }
        px[t] = sx / sw;
        py[t] = sy / sw;
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

// ---------------------------------------------------------------------------
// One row of the constant-fraction (alpha) scan
// ---------------------------------------------------------------------------
struct AlphaRow {
    int    refl = 0;
    double alpha = 0;
    double linSlope = 0, linNoise = 0, linSig = 0;  // wavefront mean, linear pair
    double linSigQ = 0;                             // point quantile, linear pair
    double knnSig = 0;                              // learned, optical CFD channel
    double knnSigAll = 0;                           // learned, combined channel
};

// wrapper index of a wrapper name ("bare", "TiO2", "Al (air)", "glossy")
int ReflIndex(const std::string& name)
{
    for (int r = 0; r < 4; ++r)
        if (name == kReflPlain[r]) return r;
    return -1;
}

// read a position_alpha_scan.csv written by this macro
std::vector<AlphaRow> ReadAlphaScan(const char* fname)
{
    std::vector<AlphaRow> rows;
    std::ifstream in(fname);
    if (!in) {
        std::cerr << "[position_reconstruction] cannot open " << fname << "\n";
        return rows;
    }
    std::string line;
    std::getline(in, line);   // header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> f;
        std::string tok;
        std::istringstream ss(line);
        while (std::getline(ss, tok, ',')) f.push_back(tok);
        if (f.size() < 8) continue;
        AlphaRow a;
        a.refl     = ReflIndex(f[0]);
        a.alpha    = std::atof(f[1].c_str());
        a.linSlope = std::atof(f[2].c_str());
        a.linNoise = std::atof(f[3].c_str());
        a.linSig   = std::atof(f[4].c_str());
        a.linSigQ  = std::atof(f[5].c_str());
        a.knnSig   = std::atof(f[6].c_str());
        a.knnSigAll= std::atof(f[7].c_str());
        rows.push_back(a);
    }
    return rows;
}

// ---------------------------------------------------------------------------
// The alpha-scan figure: the position resolution as a function of the constant
// fraction alpha, for the transparent linear read-out (left) and for the two
// learned estimators (right).
// ---------------------------------------------------------------------------
void DrawAlphaScan(const std::vector<AlphaRow>& ascan, const char* outdir)
{
    if (ascan.empty()) return;
    const int kColorA[4] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                            PetroffPalette::getP6(2), PetroffPalette::getP6(3)};
    TCanvas* c = new TCanvas("fig_recon_alpha_scan", "", 1180, 540);
    c->Divide(2, 1);
    for (int ip = 0; ip < 2; ++ip) {
        c->cd(ip + 1);
        gPad->SetLogx();
        gPad->SetLeftMargin(ip == 0 ? 0.25 : 0.19);
        gPad->SetRightMargin(0.30);
        gPad->SetBottomMargin(0.16);

        double ymax = 0;
        for (const AlphaRow& a : ascan) {
            if (ip == 0) ymax = std::max(ymax, a.linSig);
            else         ymax = std::max(ymax, std::max(a.knnSig, a.knnSigAll));
        }
        ymax *= 1.25;

        TH1D* frame = new TH1D(TString::Format("alphaf%d", ip), "", 100, 0.02, 0.6);
        frame->SetDirectory(nullptr);
        frame->SetTitle(ip == 0
                        ? "Linear opposite-pair read-out"
                        : "Learned multi-channel estimator");
        frame->GetXaxis()->SetTitle("constant fraction alpha");
        frame->GetYaxis()->SetTitle("rms error along one axis [mm]");
        // the shared style pushes the Y title off the canvas (2.5 title sizes),
        // so the offset is set explicitly here
        frame->GetYaxis()->SetTitleOffset(1.35);
        frame->GetXaxis()->SetMoreLogLabels(kTRUE);
        frame->GetXaxis()->SetLabelSize(0.030);
        frame->Draw();
        frame->GetXaxis()->SetRangeUser(0.02, 0.6);
        frame->GetYaxis()->SetRangeUser(0, ymax);

        // the nominal constant fraction, as a dashed vertical marker
        // (a TGraph, not a TLine: a TLine drawn "SAME" corrupts the range)
        TGraph* mark = new TGraph(2);
        mark->SetPoint(0, kAlpha[kAlphaNom], 0.0);
        mark->SetPoint(1, kAlpha[kAlphaNom], ymax);
        mark->SetLineColor(kGray + 1);
        mark->SetLineStyle(7);
        mark->SetLineWidth(1);
        mark->Draw("SAME");
        TText* nom = new TText(kAlpha[kAlphaNom] * 1.08, ymax * 0.93, "alpha = 12.5 %");
        nom->SetTextSize(0.032);
        nom->SetTextColor(kGray + 1);
        nom->Draw();

        TLegend* leg = new TLegend(0.72, 0.40, 0.98, 0.88);
        for (int r = 0; r < 4; ++r) {
            std::vector<double> ax, ay;
            for (const AlphaRow& a : ascan) {
                if (a.refl != r) continue;
                ax.push_back(a.alpha);
                ay.push_back(ip == 0 ? a.linSig : a.knnSig);
            }
            if (ax.empty()) continue;
            TGraph* g = new TGraph((int)ax.size());
            for (size_t i = 0; i < ax.size(); ++i) g->SetPoint((int)i, ax[i], ay[i]);
            g->SetMarkerStyle(20);
            g->SetMarkerSize(1.5);
            g->SetMarkerColor(kColorA[r]);
            g->SetLineColor(kColorA[r]);
            g->SetLineWidth(2);
            g->Draw("P SAME");
            leg->AddEntry(g, kReflName[r], "p");
            if (ip == 1) {
                // the combined estimator, dashed
                std::vector<double> bx, by;
                for (const AlphaRow& a : ascan) {
                    if (a.refl != r) continue;
                    bx.push_back(a.alpha);
                    by.push_back(a.knnSigAll);
                }
                TGraph* g2 = new TGraph((int)bx.size());
                for (size_t i = 0; i < bx.size(); ++i) g2->SetPoint((int)i, bx[i], by[i]);
                g2->SetMarkerStyle(21);
                g2->SetMarkerSize(1.5);
                g2->SetMarkerColor(kColorA[r]);
                g2->SetLineColor(kColorA[r]);
                g2->SetLineStyle(7);
                g2->SetLineWidth(2);
                g2->Draw("P SAME");
            }
        }
        if (ip == 1) {
            TGraph* s1 = new TGraph(1);
            s1->SetLineColor(kBlack);
            s1->SetLineWidth(2);
            leg->AddEntry(s1, "optical CFD", "l");
            TGraph* s2 = new TGraph(1);
            s2->SetLineColor(kBlack);
            s2->SetLineStyle(7);
            s2->SetLineWidth(2);
            leg->AddEntry(s2, "combined", "l");
        }
        leg->SetBorderSize(0);
        leg->Draw();
        frame->GetYaxis()->SetRangeUser(0, ymax);
    }
    c->SaveAs(TString::Format("%s/fig_recon_alpha_scan.pdf", outdir));
    c->SaveAs(TString::Format("%s/fig_recon_alpha_scan.png", outdir));
}

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
void position_reconstruction(const char* dir = "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7/whole",
                              const char* outdir = "report/figures",
                              int maxEvents = 25000,
                              int doAlphaScan = 1,
                              const char* alphaCsv = "",
                              int preset = 1)
{
    gPreset = preset;
    gSipm = GetSipmTable(preset);
    gNSiPM = (int)gSipm.size();

    style(0.045);
    gStyle->SetOptStat(0);
    gStyle->SetOptTitle(1);
    gSystem->mkdir(outdir, kTRUE);

    // fast path: redraw the alpha-scan figure from a csv written earlier
    if (alphaCsv && *alphaCsv) {
        const std::vector<AlphaRow> rows = ReadAlphaScan(alphaCsv);
        std::cout << "[position_reconstruction] redrawing the alpha scan from "
                  << alphaCsv << " (" << rows.size() << " rows)" << std::endl;
        DrawAlphaScan(rows, outdir);
        return;
    }

    const int    kK        = 40;        // neighbours of the k-NN regression
    const double kTestFrac = 0.2;       // 20 % of the events held out

    const char* cfgTag = (preset == 1) ? "c1" :
                         (preset == 8) ? "c8" :
                         (preset == 10) ? "c10" : "c11";

    // ---------------------------------------------------------------- read
    std::vector<WrapperData> wd(4);
    for (int r = 0; r < 4; ++r) {
        const TString fn = TString::Format("%s/muon_whole_out_%s_r%d.root", dir, cfgTag, r);
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

    // Build opposite-edge groups from the current geometry table.
    const std::vector<int> gBot = GetEdgeGroup(0);
    const std::vector<int> gTopEdge = GetEdgeGroup(1);
    const std::vector<int> gRgt = GetEdgeGroup(2);
    const std::vector<int> gLft = GetEdgeGroup(3);

    // ------------------------------------------------- opposite-pair analysis
    std::vector<ObsRow> obs;
    for (int r = 0; r < 4; ++r) {
        const std::vector<Event>& E = wd[r].evs;
        const size_t n = E.size();
        std::vector<double> Ax(n), Ay(n), Tx(n), Ty(n), Tqx(n), Tqy(n), Tmx(n), Tmy(n), Bx(n), By(n);
        for (size_t e = 0; e < n; ++e) {
            const Event& ev = E[e];
            auto grp = [&](const std::vector<int>& g) {
                double s = 0;
                for (int idx : g) s += ev.n[idx];
                return s;
            };
            const double bot = grp(gBot), top = grp(gTopEdge);
            const double rgt = grp(gRgt), lft = grp(gLft);
            Ax[e] = (rgt + lft > 0) ? (rgt - lft) / (rgt + lft) : 0.0;   // report x
            Ay[e] = (bot + top > 0) ? (bot - top) / (bot + top) : 0.0;   // report y
            auto meanPair = [&](const double* arr, const std::vector<int>& g) {
                double s = 0; int k = 0;
                for (int idx : g) if (arr[idx] > 0) { s += arr[idx]; ++k; }
                return k ? s / k : 0.0;
            };
            Tx[e]  = meanPair(ev.tfirst, gRgt) - meanPair(ev.tfirst, gLft);
            Ty[e]  = meanPair(ev.tfirst, gBot) - meanPair(ev.tfirst, gTopEdge);
            Tqx[e] = meanPair(ev.tcfd,   gRgt) - meanPair(ev.tcfd,   gLft);
            Tqy[e] = meanPair(ev.tcfd,   gBot) - meanPair(ev.tcfd,   gTopEdge);
            Tmx[e] = meanPair(ev.tcfdm,  gRgt) - meanPair(ev.tcfdm,  gLft);
            Tmy[e] = meanPair(ev.tcfdm,  gBot) - meanPair(ev.tcfdm,  gTopEdge);
            Bx[e] = ev.x; By[e] = ev.y;
        }
        obs.push_back({r, "A_x pulse-integral asymmetry (report x)", LinearFit(Ax, Bx)});
        obs.push_back({r, "A_y pulse-integral asymmetry (report y)", LinearFit(Ay, By)});
        obs.push_back({r, "T_x first-arrival difference (report x)", LinearFit(Tx, Bx)});
        obs.push_back({r, "T_y first-arrival difference (report y)", LinearFit(Ty, By)});
        obs.push_back({r, "T_cfd_x 12.5% quantile difference (report x)", LinearFit(Tqx, Bx)});
        obs.push_back({r, "T_cfd_y 12.5% quantile difference (report y)", LinearFit(Tqy, By)});
        obs.push_back({r, "T_cfdm_x 12.5% wavefront-mean difference (report x)", LinearFit(Tmx, Bx)});
        obs.push_back({r, "T_cfdm_y 12.5% wavefront-mean difference (report y)", LinearFit(Tmy, By)});
        std::cout << "   [linear r" << r << "] A_x: slope=" << obs[obs.size()-8].st.slope
                  << " noise=" << obs[obs.size()-8].st.noise << " sig=" << obs[obs.size()-8].st.sigmaPos
                  << " | T_first_x: slope=" << obs[obs.size()-6].st.slope
                  << " noise=" << obs[obs.size()-6].st.noise << " sig=" << obs[obs.size()-6].st.sigmaPos
                  << " | T_cfd_x: slope=" << obs[obs.size()-4].st.slope
                  << " noise=" << obs[obs.size()-4].st.noise << " sig=" << obs[obs.size()-4].st.sigmaPos
                  << " | T_cfdm_x: slope=" << obs[obs.size()-2].st.slope
                  << " noise=" << obs[obs.size()-2].st.noise << " sig=" << obs[obs.size()-2].st.sigmaPos
                  << std::endl;
    }

    // ------------------------------------------------------- learned estimators
    std::vector<std::vector<EstResult>> res(4);
    // per-wrapper radial errors of the four single-block estimators (amp, first
    // arrival, optical CFD, prompt fraction); re-used by the alpha scan to
    // weight the blocks of the combined estimator
    std::array<std::array<double, 4>, 4> chSrAll;
    for (auto& a : chSrAll) a = {100.0, 100.0, 100.0, 100.0};
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

        // the per-channel radial errors of the four single-block estimators; kept
        // outside the loop because the alpha scan re-uses them to weight the
        // blocks of the combined estimator
        std::array<double, 4>& chSr = chSrAll[r];
        for (int f = 0; f < kNFeat; ++f) {
            std::vector<std::vector<double>> X;
            MakeFeatures(E, f, X);
            std::vector<std::vector<double>> Xtr, Xte;
            std::vector<double> Yxtr, Yytr;
            for (size_t i : tr) { Xtr.push_back(X[i]); Yxtr.push_back(Yx[i]); Yytr.push_back(Yy[i]); }
            for (size_t i : te) { Xte.push_back(X[i]); }

            // For the combined estimator, weight each nSiPM-wide feature block
            // by its inverse variance (w_c = (sr_min / sr_c)^4) so noisy blocks
            // do not dilute the distance metric.
            std::vector<double> blkW;
            if (f == kAll) {
                const double srMin = std::min({chSr[0], chSr[1], chSr[2], chSr[3]});
                double w4[4] = {1.0, 1.0, 1.0, 1.0};
                for (int c = 0; c < 4; ++c) {
                    const double ratio = srMin / std::max(1.0, chSr[c]);
                    w4[c] = ratio * ratio * ratio * ratio;
                }
                if (gPreset == 11) {
                    // amp(8), tf(8), tq(8), tm(8), pv(8), dt(8), da(8)
                    blkW = {w4[0], w4[1], 0.7 * w4[2], 0.3 * w4[2], w4[3], 0.7 * w4[2], w4[0]};
                } else {
                    // kAll stacks: amp(N), tf(N), tq(N), tm(N), pv(N) -> 5 blocks
                    blkW = {w4[0], w4[1], 0.7 * w4[2], 0.3 * w4[2], w4[3]};
                }
                std::cout << "   [weights r" << r << "] w_amp=" << w4[0]
                          << " w_tfirst=" << w4[1] << " w_tcfd=" << w4[2]
                          << " w_prompt=" << w4[3] << std::endl;
            } else if (f == kTimeCFD) {
                if (gPreset == 11) {
                    // tf(N), tm(N), dt(N), da(N)
                    blkW = {0.7, 0.3, 0.7, 0.3};
                } else {
                    blkW = {0.7, 0.3};
                }
            }

            EstResult er;
            er.feat = f; er.k = kK; er.ntrain = (int)tr.size(); er.ntest = (int)te.size();
            KnnPredict(Xtr, Yxtr, Yytr, Xte, kK, er.px, er.py, true, blkW);
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
            if (f < 4) chSr[f] = er.sr;
            er.biasx = mean(dx); er.biasy = mean(dy);
            std::vector<double> sdr = dr;
            std::sort(sdr.begin(), sdr.end());
            er.r68 = sdr[(size_t)(0.68 * sdr.size())];
            res[r].push_back(er);
            std::cout << "   r" << r << " " << kFeatName[f] << ": sigma_x = " << er.sx
                      << " mm, sigma_y = " << er.sy << " mm, sigma_r = " << er.sr << " mm"
                      << " (r68 = " << er.r68 << " mm)" << std::endl;
        }
    }

    // ------------------------------------------------------------- alpha scan
    // The constant fraction of the optical CFD timing observable is a free
    // parameter of the reconstruction.  The scan below measures the position
    // resolution as a function of alpha, for the transparent linear observable
    // and for the two learned estimators, so that the nominal 12.5 % is chosen
    // from the data rather than from an argument.
    std::vector<AlphaRow> ascan;
    if (doAlphaScan) {
        for (int r = 0; r < 4; ++r) {
            const std::vector<Event>& E = wd[r].evs;
            const size_t n = E.size();
            std::vector<double> Yx(n), Yy(n);
            for (size_t e = 0; e < n; ++e) { Yx[e] = E[e].x; Yy[e] = E[e].y; }

            // the same train/test split as the production estimator
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::mt19937 rng(20251018u);
            std::shuffle(idx.begin(), idx.end(), rng);
            const size_t ntest = (size_t)(kTestFrac * n);
            std::vector<int> tr, te;
            for (size_t i = 0; i < n; ++i) { if (i < ntest) te.push_back(idx[i]); else tr.push_back(idx[i]); }

            auto meanPair = [&](const double* arr, const std::vector<int>& g) {
                double s = 0; int k = 0;
                for (int idx : g) if (arr[idx] > 0) { s += arr[idx]; ++k; }
                return k ? s / k : 0.0;
            };
            auto rms = [](const std::vector<double>& v) {
                double s = 0; for (double q : v) s += q * q; return std::sqrt(s / v.size());
            };

            for (int ai = 0; ai < kNAlpha; ++ai) {
                AlphaRow row;
                row.refl = r;
                row.alpha = kAlpha[ai];

                // (1) the transparent linear observable: the opposite-pair
                //     difference of the leading-alpha wavefront times
                std::vector<double> Tm(n), Tq(n);
                for (size_t e = 0; e < n; ++e) {
                    const Event& ev = E[e];
                    Tm[e] = meanPair(ev.tcfdmA[ai], gRgt)
                           - meanPair(ev.tcfdmA[ai], gLft);
                    Tq[e] = meanPair(ev.tcfdA[ai],  gRgt)
                           - meanPair(ev.tcfdA[ai],  gLft);
                }
                const PairStat pm = LinearFit(Tm, Yx);
                row.linSlope = pm.slope;
                row.linNoise = pm.noise;
                row.linSig   = pm.sigmaPos;
                row.linSigQ  = LinearFit(Tq, Yx).sigmaPos;

                // (2) the learned estimator, optical CFD channel
                {
                    std::vector<std::vector<double>> X, Xtr, Xte;
                    std::vector<double> Yxtr, Yytr;
                    MakeFeatures(E, kTimeCFD, X, ai);
                    for (size_t i : tr) { Xtr.push_back(X[i]); Yxtr.push_back(Yx[i]); Yytr.push_back(Yy[i]); }
                    for (size_t i : te) { Xte.push_back(X[i]); }
                    EstResult er;
                    std::vector<double> blkWTimeCFD = (gPreset == 11)
                        ? std::vector<double>{0.7, 0.3, 0.7, 0.3}
                        : std::vector<double>{0.7, 0.3};
                    KnnPredict(Xtr, Yxtr, Yytr, Xte, kK, er.px, er.py, true, blkWTimeCFD);
                    for (size_t i = 0; i < te.size(); ++i) { er.tx.push_back(Yx[te[i]]); er.ty.push_back(Yy[te[i]]); }
                    std::vector<double> dx, dy, dr;
                    for (size_t i = 0; i < te.size(); ++i) {
                        dx.push_back(er.tx[i] - er.px[i]);
                        dy.push_back(er.ty[i] - er.py[i]);
                        dr.push_back(std::sqrt(dx.back() * dx.back() + dy.back() * dy.back()));
                    }
                    row.knnSig = rms(dx);

                    // (3) the learned estimator, combined channel, with the
                    //     block weights of this alpha
                    std::array<double, 4> chSr = chSrAll[r];
                    chSr[2] = rms(dr);
                    const double srMin = std::min({chSr[0], chSr[1], chSr[2], chSr[3]});
                    double w4[4] = {1.0, 1.0, 1.0, 1.0};
                    for (int c = 0; c < 4; ++c) {
                        const double ratio = srMin / std::max(1.0, chSr[c]);
                        w4[c] = ratio * ratio * ratio * ratio;
                    }
                    std::vector<std::vector<double>> Xa, Xatr, Xate;
                    std::vector<double> Yxatr, Yyatr;
                    MakeFeatures(E, kAll, Xa, ai);
                    for (size_t i : tr) { Xatr.push_back(Xa[i]); Yxatr.push_back(Yx[i]); Yyatr.push_back(Yy[i]); }
                    for (size_t i : te) { Xate.push_back(Xa[i]); }
                    EstResult er2;
                    std::vector<double> blkWAll;
                    if (gPreset == 11) {
                        blkWAll = {w4[0], w4[1], 0.7 * w4[2], 0.3 * w4[2], w4[3], 0.7 * w4[2], w4[0]};
                    } else {
                        blkWAll = {w4[0], w4[1], 0.7 * w4[2], 0.3 * w4[2], w4[3]};
                    }
                    KnnPredict(Xatr, Yxatr, Yyatr, Xate, kK, er2.px, er2.py, true, blkWAll);
                    std::vector<double> dx2;
                    for (size_t i = 0; i < te.size(); ++i)
                        dx2.push_back(Yx[te[i]] - er2.px[i]);
                    row.knnSigAll = rms(dx2);
                }

                ascan.push_back(row);
                std::cout << "   [alpha scan r" << r << "] alpha = " << row.alpha
                          << ": linear sigma_x = " << row.linSig
                          << " mm, learned (CFD) = " << row.knnSig
                          << " mm, learned (combined) = " << row.knnSigAll
                          << " mm" << std::endl;
            }
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
    if (doAlphaScan) {
        std::ofstream csv(TString::Format("%s/position_alpha_scan.csv", outdir).Data());
        csv << "wrapper,alpha,linear_slope_per_mm,linear_rms_noise,linear_sigma_x_mm,"
               "linear_sigma_x_pointquantile_mm,learned_sigma_x_cfd_mm,learned_sigma_x_combined_mm\n";
        for (const AlphaRow& a : ascan)
            csv << kReflPlain[a.refl] << "," << a.alpha << ","
                << a.linSlope << "," << a.linNoise << "," << a.linSig << ","
                << a.linSigQ << "," << a.knnSig << "," << a.knnSigAll << "\n";
    }

    // ================================================================ figures
    const int kColor[kMaxSiPM] = {PetroffPalette::getP6(0), PetroffPalette::getP6(1),
                                  PetroffPalette::getP6(2), PetroffPalette::getP6(3),
                                  PetroffPalette::getP6(4), PetroffPalette::getP6(5),
                                  kMagenta, kTeal, kOrange, kViolet, kSpring, kCyan};

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
                for (int s = 0; s < gNSiPM; ++s) {
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
                    leg->AddEntry(h, gSipm[s].name, "l");
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
    //                 points] from the uniform expectation;
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
                std::vector<std::vector<double>> sumN(kNClass, std::vector<double>(gNSiPM, 0.0));
                std::vector<std::vector<double>> sumT(kNClass, std::vector<double>(gNSiPM, 0.0));
                std::vector<std::vector<double>> sumQ(kNClass, std::vector<double>(gNSiPM, 0.0));
                long   cnt[kNClass] = {0, 0};
                std::vector<std::vector<long>> cntT(kNClass, std::vector<long>(gNSiPM, 0));
                std::vector<std::vector<long>> cntQ(kNClass, std::vector<long>(gNSiPM, 0));
                for (const Event& ev : wd[refl].evs) {
                    const int k = Classify(ev);
                    if (k < 0) continue;
                    ++cnt[k];
                    for (int s = 0; s < gNSiPM; ++s) {
                        sumN[k][s] += ev.n[s];
                        if (ev.tfirst[s] > 0) { sumT[k][s] += ev.tfirst[s]; ++cntT[k][s]; }
                        if (ev.tcfdm[s]  > 0) { sumQ[k][s] += ev.tcfdm[s];  ++cntQ[k][s]; }
                    }
                }
                const int nBins = 2 * gNSiPM;
                TH1D* h0 = new TH1D(TString::Format("obsA%d_%d", refl, ic), "", nBins, -0.5, nBins - 0.5);
                TH1D* h1 = new TH1D(TString::Format("obsB%d_%d", refl, ic), "", nBins, -0.5, nBins - 0.5);
                h0->SetDirectory(nullptr); h1->SetDirectory(nullptr);
                double ymax = 0;
                std::vector<double> val0(gNSiPM, 0.0), val1(gNSiPM, 0.0),
                                    valQ0(gNSiPM, 0.0), valQ1(gNSiPM, 0.0);
                const double shareMean = 100.0 / (double)gNSiPM;
                for (int s = 0; s < gNSiPM; ++s) {
                    double v0 = 0, v1 = 0, q0 = 0, q1 = 0;
                    if (amp) {
                        double tot0 = 0, tot1 = 0;
                        for (int q = 0; q < gNSiPM; ++q) { tot0 += sumN[0][q]; tot1 += sumN[1][q]; }
                        v0 = (cnt[0] > 0 && tot0 > 0) ? 100.0 * sumN[0][s] / tot0 - shareMean : 0.0;
                        v1 = (cnt[1] > 0 && tot1 > 0) ? 100.0 * sumN[1][s] / tot1 - shareMean : 0.0;
                    } else {
                        double m0 = 0, m1 = 0; int k0 = 0, k1 = 0;
                        double mq0 = 0, mq1 = 0; int kq0 = 0, kq1 = 0;
                        for (int q = 0; q < gNSiPM; ++q) {
                            if (cntT[0][q] > 0) { m0  += sumT[0][q] / cntT[0][q]; ++k0; }
                            if (cntT[1][q] > 0) { m1  += sumT[1][q] / cntT[1][q]; ++k1; }
                            if (cntQ[0][q] > 0) { mq0 += sumQ[0][q] / cntQ[0][q]; ++kq0; }
                            if (cntQ[1][q] > 0) { mq1 += sumQ[1][q] / cntQ[1][q]; ++kq1; }
                        }
                        m0  = k0  ? m0  / k0  : 0.0;  m1  = k1  ? m1  / k1  : 0.0;
                        mq0 = kq0 ? mq0 / kq0 : 0.0;  mq1 = kq1 ? mq1 / kq1 : 0.0;
                        v0 = (cntT[0][s] > 0) ? (sumT[0][s] / cntT[0][s] - m0)  : 0.0;
                        v1 = (cntT[1][s] > 0) ? (sumT[1][s] / cntT[1][s] - m1)  : 0.0;
                        q0 = (cntQ[0][s] > 0) ? (sumQ[0][s] / cntQ[0][s] - mq0) : 0.0;
                        q1 = (cntQ[1][s] > 0) ? (sumQ[1][s] / cntQ[1][s] - mq1) : 0.0;
                    }
                    val0[s] = v0; val1[s] = v1; valQ0[s] = q0; valQ1[s] = q1;
                    ymax = std::max(ymax, std::max(std::fabs(v0), std::fabs(v1)));
                    if (!amp) ymax = std::max(ymax, std::max(std::fabs(q0), std::fabs(q1)));
                    if (ic == 0) {   // light share [percentage points from 12.5]
                        classObs.push_back({(double)refl, 0.0, (double)s, v0});
                        classObs.push_back({(double)refl, 1.0, (double)s, v1});
                    } else {         // first-arrival and 12.5% optical CFD offsets [ns]
                        classObs.push_back({(double)refl, 2.0, (double)s, v0});
                        classObs.push_back({(double)refl, 3.0, (double)s, v1});
                        classObs.push_back({(double)refl, 4.0, (double)s, q0});
                        classObs.push_back({(double)refl, 5.0, (double)s, q1});
                    }
                }
                // frame (creates the axis) + real bars anchored at zero
                h0->SetTitle(TString::Format("%s: %s", kReflName[refl],
                                             amp ? "light share per SiPM (deviation from uniform)"
                                                 : "arrival-time offset (first hit vs 12.5% optical CFD)"));
                for (int s = 0; s < gNSiPM; ++s) {
                    h0->GetXaxis()->SetBinLabel(s + 1, gSipm[s].name);
                    h0->GetXaxis()->SetBinLabel(s + gNSiPM + 1, gSipm[s].name);
                }
                h0->GetXaxis()->SetTitle("SiPM   (left group: central muon,  right group: corner muon)");
                h0->GetYaxis()->SetTitle(amp ? TString::Format("share - %.2f [%%]", shareMean).Data()
                                             : "arrival-time offset [ns]");
                h0->Draw();
                h0->GetYaxis()->SetRangeUser(std::min(0.0, -ymax * 1.25), ymax * (amp ? 1.25 : 1.42));
                h0->GetXaxis()->SetRangeUser(-0.6, nBins - 0.4);
                TBox* bar0 = nullptr; TBox* bar1 = nullptr; TBox* barQ = nullptr;
                for (int s = 0; s < gNSiPM; ++s) {
                    const double v0 = val0[s];
                    const double v1 = val1[s];
                    if (amp) {
                        TBox* b0 = new TBox(s - 0.40, std::min(0.0, v0), s + 0.40, std::max(0.0, v0));
                        b0->SetFillColor(kColor[0]); b0->SetFillStyle(1001);
                        b0->SetLineColor(kBlack); b0->Draw("SAME");
                        TBox* b1 = new TBox(s + gNSiPM - 0.40, std::min(0.0, v1), s + gNSiPM + 0.40, std::max(0.0, v1));
                        b1->SetFillColor(kColor[1]); b1->SetFillStyle(1001);
                        b1->SetLineColor(kBlack); b1->Draw("SAME");
                        if (!bar0) { bar0 = b0; bar1 = b1; }
                    } else {
                        const double q0 = valQ0[s];
                        const double q1 = valQ1[s];
                        TBox* b0 = new TBox(s - 0.38, std::min(0.0, v0), s - 0.02, std::max(0.0, v0));
                        b0->SetFillColor(kColor[0]); b0->SetFillStyle(1001);
                        b0->SetLineColor(kBlack); b0->Draw("SAME");
                        TBox* bq0 = new TBox(s + 0.02, std::min(0.0, q0), s + 0.38, std::max(0.0, q0));
                        bq0->SetFillColor(kColor[2]); bq0->SetFillStyle(1001);
                        bq0->SetLineColor(kBlack); bq0->Draw("SAME");
                        TBox* b1 = new TBox(s + gNSiPM - 0.38, std::min(0.0, v1), s + gNSiPM - 0.02, std::max(0.0, v1));
                        b1->SetFillColor(kColor[1]); b1->SetFillStyle(1001);
                        b1->SetLineColor(kBlack); b1->Draw("SAME");
                        TBox* bq1 = new TBox(s + gNSiPM + 0.02, std::min(0.0, q1), s + gNSiPM + 0.38, std::max(0.0, q1));
                        bq1->SetFillColor(kColor[2]); bq1->SetFillStyle(1001);
                        bq1->SetLineColor(kBlack); bq1->Draw("SAME");
                        if (!bar0) { bar0 = b0; bar1 = b1; barQ = bq1; }
                    }
                }
                TLegend* leg = new TLegend(0.18, amp ? 0.70 : 0.66, 0.50, 0.84);
                leg->SetTextSize(0.038);
                if (amp) {
                    leg->AddEntry(bar0, "central muon", "f");
                    leg->AddEntry(bar1, "corner muon", "f");
                } else {
                    leg->AddEntry(bar0, "central (first hit)", "f");
                    leg->AddEntry(bar1, "corner (first hit)", "f");
                    leg->AddEntry(barQ, "corner (12.5% CFD)", "f");
                }
                leg->SetBorderSize(0);
                leg->SetFillStyle(0);
                leg->Draw();
            }
        }
        c->SaveAs(TString::Format("%s/fig_recon_observables.pdf", outdir));
        c->SaveAs(TString::Format("%s/fig_recon_observables.png", outdir));
    }

    // --------------------------------------- F3: resolution per estimator
    {
        TCanvas* c = new TCanvas("fig_recon_features", "", 1080, 640);
        c->SetLeftMargin(0.16);
        c->SetRightMargin(0.26);
        c->SetBottomMargin(0.16);
        TH1D* frame = new TH1D("featframe", "", kNFeat, -0.5, kNFeat - 0.5);
        frame->SetDirectory(nullptr);
        double ymax = 0;
        for (int f = 0; f < kNFeat; ++f)
            for (int r = 0; r < 4; ++r) ymax = std::max(ymax, res[r][f].sr);
        frame->SetTitle(TString::Format("Position resolution of the %s read-out (whole-plate data)",
                                        cfgTag).Data());
        frame->GetYaxis()->SetTitle("rms radial error [mm]");
        frame->GetXaxis()->SetTitle("information channel used by the estimator");
        const char* kAxisLabel[kNFeat] = {
            "pulse integral", "first arrival", "optical CFD", "prompt fraction", "combined"
        };
        for (int f = 0; f < kNFeat; ++f)
            frame->GetXaxis()->SetBinLabel(f + 1, kAxisLabel[f]);
        frame->GetXaxis()->LabelsOption("h");
        frame->GetXaxis()->SetLabelSize(0.042);
        frame->GetXaxis()->SetTitleOffset(1.25);
        frame->Draw();
        frame->GetYaxis()->SetRangeUser(0, ymax * 1.15);
        TLegend* leg = new TLegend(0.76, 0.42, 0.99, 0.88);
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

    // --------------------------------------- F3b: constant-fraction scan
    if (doAlphaScan && !ascan.empty()) DrawAlphaScan(ascan, outdir);

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
            for (int s = 0; s < gNSiPM; ++s) {
                double cx = gSipm[s].x, cy = gSipm[s].y;
                const double tol = 1e-6;
                if      (std::fabs(gSipm[s].x - kHalf) < tol) cx =  kHalf + off;
                else if (std::fabs(gSipm[s].x + kHalf) < tol) cx = -kHalf - off;
                if      (std::fabs(gSipm[s].y - kHalf) < tol) cy =  kHalf + off;
                else if (std::fabs(gSipm[s].y + kHalf) < tol) cy = -kHalf - off;
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
        const char* shareLabel = (gPreset == 1) ? "light share - 12.5 [pp]"
                                                : "light share deviation [pp]";
        std::string pop0 = std::string("central muon, ") + shareLabel;
        std::string pop1 = std::string("corner muon, ") + shareLabel;
        const char* pop[6] = {pop0.c_str(), pop1.c_str(),
                              "central muon, first-arrival offset [ns]",
                              "corner muon, first-arrival offset [ns]",
                              "central muon, 12.5% optical CFD offset [ns]",
                              "corner muon, 12.5% optical CFD offset [ns]"};
        for (const auto& r : classObs)
            csv << kReflPlain[(int)r[0]] << "," << pop[(int)r[1]] << ","
                << gSipm[(int)r[2]].name << "," << r[3] << "\n";
    }

    std::cout << "[position_reconstruction] done" << std::endl;
}
