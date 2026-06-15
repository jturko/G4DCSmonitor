#include "SurfaceFluxSampler.hh"

#include "Randomize.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4RunManager.hh"

#include "ProgressBar.hh"

#include "TFile.h"
#include "TTree.h"
#include "TError.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <memory>
#include <string>

#include "ROOT/RDataFrame.hxx"
#include "ROOT/RDF/RInterface.hxx"   // ROOT::RDF::RNode
#include "TROOT.h"                   // EnableImplicitMT / DisableImplicitMT / IsImplicitMTEnabled
#include "TStopwatch.h"
#include <thread>


SurfaceFluxSampler& SurfaceFluxSampler::Instance()
{
    static SurfaceFluxSampler s;
    return s;
}

// Walker's alias method, O(N) build, O(1) sample.
void SurfaceFluxSampler::BuildAlias(Bucket& b)
{
    const std::size_t N = b.data.size();
    b.prob.assign(N, 0.0);
    b.alias.assign(N, 0);
    if (N == 0) return;

    long double sum = 0.0L;
    for (auto& c : b.data) sum += std::max<G4float>(c.weight, 0.f);
    if (sum <= 0.0L) {
        for (auto& c : b.data) c.weight = 1.f;
        sum = static_cast<long double>(N);
    }

    std::vector<G4double> p(N);
    for (std::size_t i = 0; i < N; ++i)
        p[i] = (b.data[i].weight > 0.f ? b.data[i].weight : 0.) * N / sum;

    std::queue<std::size_t> small, large;
    for (std::size_t i = 0; i < N; ++i)
        (p[i] < 1.0 ? small : large).push(i);

    while (!small.empty() && !large.empty()) {
        std::size_t s = small.front(); small.pop();
        std::size_t l = large.front(); large.pop();
        b.prob[s]  = p[s];
        b.alias[s] = static_cast<G4int>(l);
        p[l] = (p[l] + p[s]) - 1.0;
        (p[l] < 1.0 ? small : large).push(l);
    }
    while (!large.empty()) { b.prob[large.front()] = 1.0; large.pop(); }
    while (!small.empty()) { b.prob[small.front()] = 1.0; small.pop(); }
}

//....oooOO0OOooo  SINGLE-PASS loader (no rejection, no preprocessing)  oooOO...

void SurfaceFluxSampler::Load(const std::string& filename,
                              const std::string& treeName,
                              G4double /*R_mm*/, G4double /*H_mm*/,
                              G4double /*tol_mm*/)
{
    if (fLoaded) return;

    const G4double R     = fPendingR_mm;
    const G4double Hhalf = 0.5 * fPendingH_mm;

    // maxEntries (Range) is incompatible with IMT in RDataFrame, so the capped
    // debug path runs single-threaded; the full production path runs MT.
    const bool capped = (fSurfaceSourceMaxEntriesLoadedFromTree > 0);

    gErrorIgnoreLevel = kError;
    TStopwatch sw; sw.Start();

    // --- configure ROOT implicit MT for this scan only ---
    bool weEnabledMT = false;
    if (!capped) {
        if (!ROOT::IsImplicitMTEnabled()) {
            const unsigned int req = (fLoadThreads <= 0)
                                   ? 0u   // 0 => use all hardware threads
                                   : static_cast<unsigned int>(fLoadThreads);
            ROOT::EnableImplicitMT(req);
            weEnabledMT = true;
        }
    } else if (ROOT::IsImplicitMTEnabled()) {
        ROOT::DisableImplicitMT();         // Range() needs IMT off
    }

    // cheap entry count (reads metadata only) for reserve + logging
    Long64_t nTotal = 0;
    {
        std::unique_ptr<TFile> probe(TFile::Open(filename.c_str(), "READ"));
        if (probe && !probe->IsZombie())
            if (auto* tt = dynamic_cast<TTree*>(probe->Get(treeName.c_str())))
                nTotal = tt->GetEntries();
    }
    const Long64_t N = capped
        ? std::min<Long64_t>(fSurfaceSourceMaxEntriesLoadedFromTree, nTotal)
        : nTotal;

    G4cout << "[SurfaceFluxSampler] Scanning " << N << " entries from '"
           << filename << "' using "
           << (capped ? 1u
                      : (ROOT::IsImplicitMTEnabled()
                            ? std::max(1u, std::thread::hardware_concurrency()) : 1u))
           << " thread(s)..." << G4endl;

    // --- build the (optionally capped) dataframe ---
    ROOT::RDataFrame df(treeName, filename);
    ROOT::RDF::RNode node = df;
    if (capped) node = df.Range(0, N);

    const unsigned int nSlots = std::max(1u, df.GetNSlots());
    std::vector<std::vector<Crossing>> perSlot(nSlots);
    {
        const Long64_t guess = (N > 0 ? N : 0) / nSlots + 16;
        for (auto& v : perSlot) v.reserve((std::size_t)guess);
    }

    // --- the parallel scan: each slot fills its own buffer, no locks ---
    node.ForeachSlot(
        [&perSlot, R, Hhalf](unsigned int slot,
                             double pid,  double ekin,
                             double x,    double y,    double z,
                             double px,   double py,   double pz,
                             double w)
        {
            const double pMag = std::sqrt(px*px + py*py + pz*pz);
            if (pMag <= 0.) return;                       // guard, not a rejection

            const double rLoc     = std::hypot(x, y);
            const double distSide = std::fabs(rLoc - R);
            const double distCap  = std::fabs(Hhalf - std::fabs(z));

            Crossing c;
            c.pid    = (G4int)llround(pid);
            c.ekin   = (G4float)ekin;
            c.x      = (G4float)x;  c.y = (G4float)y;  c.z = (G4float)z;
            c.px     = (G4float)(px/pMag);
            c.py     = (G4float)(py/pMag);
            c.pz     = (G4float)(pz/pMag);
            c.weight = (G4float)w;
            c.surf   = (distSide <= distCap) ? 0 : ((z >= 0.) ? 1 : 2);

            perSlot[slot].push_back(c);
        },
        {"pid","ekin","x","y","z","px","py","pz","weight"});

    // --- single-threaded merge + tally ---
    std::size_t total = 0;
    for (auto& v : perSlot) total += v.size();

    fAll.data.clear();
    fAll.data.reserve(total);

    Long64_t nSide=0, nTop=0, nBot=0, nNeutron=0, nGamma=0;
    Double_t wSide=0, wTop=0, wBot=0;
    for (auto& v : perSlot) {
        for (auto& c : v) {
            fAll.data.push_back(c);
            if      (c.surf==0){ ++nSide; wSide+=c.weight; }
            else if (c.surf==1){ ++nTop;  wTop +=c.weight; }
            else               { ++nBot;  wBot +=c.weight; }
            if      (c.pid==2112) ++nNeutron;
            else if (c.pid==22)   ++nGamma;
        }
        std::vector<Crossing>().swap(v);   // free slot buffer promptly
    }

    // restore ROOT to single-threaded for the rest of the Geant4 job
    if (weEnabledMT) ROOT::DisableImplicitMT();

    BuildAlias(fAll);

    fKeptSide        = nSide;
    fKeptSideWeight  = wSide;
    fKeptWeightTotal = wSide + wTop + wBot;
    fLoaded = true;
    gErrorIgnoreLevel = kWarning;

    sw.Stop();
    G4cout << "[SurfaceFluxSampler] Loaded " << fAll.data.size()
           << " crossings in " << sw.RealTime() << " s"
           << " (n=" << nNeutron << ", gamma=" << nGamma << ")\n"
           << "   surfaces: side=" << nSide << " (w=" << wSide << "), "
           << "top=" << nTop << " (w=" << wTop << "), "
           << "bottom=" << nBot << " (w=" << wBot << ")\n"
           << "   total kept weight = " << fKeptWeightTotal << G4endl;
}

//   void SurfaceFluxSampler::Load(const std::string& filename,
//                                 const std::string& treeName,
//                                 G4double /*R_mm*/, G4double /*H_mm*/,
//                                 G4double /*tol_mm*/)
//   {
//       if (fLoaded) return;
//   
//       std::unique_ptr<TFile> f(TFile::Open(filename.c_str(), "READ"));
//       if (!f || f->IsZombie()) {
//           G4Exception("SurfaceFluxSampler::Load", "OpenFail", FatalException,
//                       ("Cannot open " + filename).c_str());
//           return;
//       }
//       auto* t = dynamic_cast<TTree*>(f->Get(treeName.c_str()));
//       if (!t) {
//           G4Exception("SurfaceFluxSampler::Load", "NoTree", FatalException,
//                       ("No tree " + treeName + " in " + filename).c_str());
//           return;
//       }
//   
//       gErrorIgnoreLevel = kError;
//       t->SetCacheSize(512LL * 1024 * 1024);
//   
//       // Read only what we need: skip 't' and 'evtNb' to cut I/O.
//       Double_t pid, ekin, x, y, z, px, py, pz, w;
//       t->SetBranchStatus("*", 0);
//       for (auto br : {"pid","ekin","x","y","z","px","py","pz","weight"})
//           t->SetBranchStatus(br, 1);
//       t->AddBranchToCache("pid",1);   t->AddBranchToCache("ekin",1);
//       t->AddBranchToCache("x",1);     t->AddBranchToCache("y",1);
//       t->AddBranchToCache("z",1);     t->AddBranchToCache("px",1);
//       t->AddBranchToCache("py",1);    t->AddBranchToCache("pz",1);
//       t->AddBranchToCache("weight",1);
//       t->StopCacheLearningPhase();
//   
//       t->SetBranchAddress("pid",    &pid);
//       t->SetBranchAddress("ekin",   &ekin);
//       t->SetBranchAddress("x",      &x);
//       t->SetBranchAddress("y",      &y);
//       t->SetBranchAddress("z",      &z);
//       t->SetBranchAddress("px",     &px);
//       t->SetBranchAddress("py",     &py);
//       t->SetBranchAddress("pz",     &pz);
//       t->SetBranchAddress("weight", &w);
//   
//       const Long64_t N = (fSurfaceSourceMaxEntriesLoadedFromTree > 0 &&
//                           fSurfaceSourceMaxEntriesLoadedFromTree <= t->GetEntries())
//                        ? fSurfaceSourceMaxEntriesLoadedFromTree : t->GetEntries();
//   
//       fAll.data.clear();
//       fAll.data.reserve(N);
//   
//       const G4double R     = fPendingR_mm;
//       const G4double Hhalf = 0.5 * fPendingH_mm;
//   
//       Long64_t nSide=0, nTop=0, nBot=0, nNeutron=0, nGamma=0, nDegenerate=0;
//       Double_t wSide=0, wTop=0, wBot=0;
//   
//       ProgressBar prog(N);
//       for (Long64_t i = 0; i < N; ++i) {
//           if (i % 1000000 == (1000000-1)) prog.Print(i);
//           t->GetEntry(i);
//   
//           const G4double pMag = std::sqrt(px*px + py*py + pz*pz);
//           if (pMag <= 0.) { ++nDegenerate; continue; }   // not a rejection, just guard
//   
//           const G4double rLoc = std::hypot(x, y);
//   
//           // Classify by NEAREST surface (every entry is a real crossing, so we
//           // never drop one; this just decides how Sample() will smear it).
//           const G4double distSide = std::fabs(rLoc - R);
//           const G4double distCap  = std::fabs(Hhalf - std::fabs(z));
//           G4int surf;
//           if (distSide <= distCap) surf = 0;          // cylindrical side
//           else                     surf = (z >= 0.) ? 1 : 2;  // top / bottom cap
//   
//           Crossing c;
//           c.pid    = static_cast<G4int>(pid);
//           c.ekin   = static_cast<G4float>(ekin);
//           c.x      = (G4float)x;  c.y = (G4float)y;  c.z = (G4float)z;
//           c.px     = (G4float)(px/pMag);
//           c.py     = (G4float)(py/pMag);
//           c.pz     = (G4float)(pz/pMag);
//           c.weight = static_cast<G4float>(w);
//           c.surf   = surf;
//           fAll.data.push_back(c);
//   
//           if      (surf==0){ ++nSide; wSide+=w; }
//           else if (surf==1){ ++nTop;  wTop +=w; }
//           else             { ++nBot;  wBot +=w; }
//           if      (c.pid==2112) ++nNeutron;
//           else if (c.pid==22)   ++nGamma;
//       }
//   
//       BuildAlias(fAll);
//   
//       fKeptSide        = nSide;
//       fKeptSideWeight  = wSide;
//       fKeptWeightTotal = wSide + wTop + wBot;
//       fLoaded = true;
//       gErrorIgnoreLevel = kWarning;
//   
//       G4cout << "[SurfaceFluxSampler] Single-pass load of " << fAll.data.size()
//              << " crossings (n=" << nNeutron << ", gamma=" << nGamma
//              << (nDegenerate ? (", skipped degenerate=" + std::to_string(nDegenerate)) : std::string())
//              << ")\n"
//              << "   surfaces: side=" << nSide << " (w=" << wSide << "), "
//              << "top=" << nTop << " (w=" << wTop << "), "
//              << "bottom=" << nBot << " (w=" << wBot << ")\n"
//              << "   total kept weight = " << fKeptWeightTotal << G4endl;
//       t->PrintCacheStats();
//   }

const SurfaceFluxSampler::Bucket*
SurfaceFluxSampler::GetBucket(G4int /*pid*/) const { return &fAll; }

G4int SurfaceFluxSampler::NumEntries(G4int pid) const
{
    auto* b = GetBucket(pid);
    return b ? static_cast<G4int>(b->data.size()) : 0;
}

G4ThreeVector
SurfaceFluxSampler::SmearDirectionCone(const G4ThreeVector& dir,
                                       G4double sigma) const
{
    const G4double th = std::fabs(G4RandGauss::shoot(0., sigma));
    const G4double ph = 2.0 * M_PI * G4UniformRand();
    G4ThreeVector d(std::sin(th)*std::cos(ph),
                    std::sin(th)*std::sin(ph),
                    std::cos(th));
    d.rotateUz(dir);
    return d;
}

bool SurfaceFluxSampler::Sample(G4ThreeVector& posLocal,
                                G4ThreeVector& dirLocal,
                                G4double&      ekin,
                                G4double&      weight,
                                G4int&         pid) const
{
    EnsureLoaded();

    const Bucket* b = GetBucket(fRequestedPid);
    if (!b || b->data.empty()) return false;

    const std::size_t N = b->data.size();
    const G4double u    = G4UniformRand() * N;
    const std::size_t i = static_cast<std::size_t>(u);
    const G4double frac = u - i;
    const std::size_t k = (frac < b->prob[i]) ? i : b->alias[i];
    const auto& c = b->data[k];

    G4double xL = c.x, yL = c.y, zL = c.z;
    G4ThreeVector dir(c.px, c.py, c.pz);

    if (c.surf == 0) {
        // -------------------- cylindrical SIDE --------------------
        if (fSmearPhi > 0. || fSmearZ > 0.) {
            const G4double rLoc = std::hypot(c.x, c.y);
            G4double phi0 = std::atan2(c.y, c.x);
            if (fSmearPhi > 0.) phi0 += G4RandGauss::shoot(0., fSmearPhi);
            if (fSmearZ   > 0.) zL   += G4RandGauss::shoot(0., fSmearZ);
            const G4double zMax = 0.5*fPendingH_mm - fPendingTol_mm;
            if (zL >  zMax) zL =  zMax;
            if (zL < -zMax) zL = -zMax;
            xL = rLoc * std::cos(phi0);
            yL = rLoc * std::sin(phi0);
        }
        if (fSmearAngle > 0.) {
            G4ThreeVector d = SmearDirectionCone(dir, fSmearAngle);
            const G4double invR = 1.0 / std::hypot(xL, yL);
            if (d.x()*(xL*invR) + d.y()*(yL*invR) > 0.) dir = d;
        }
    } else {
        // ----------------- circular END-CAP (top=1, bottom=2) -----------------
        const G4double sign = (c.surf == 1) ? +1.0 : -1.0;
        zL = sign * (0.5 * fPendingH_mm);
        if (fSmearZ > 0.) {                       // reuse fSmearZ as in-plane sigma
            xL = c.x + G4RandGauss::shoot(0., fSmearZ);
            yL = c.y + G4RandGauss::shoot(0., fSmearZ);
            const G4double rMax = fPendingR_mm - fPendingTol_mm;
            const G4double rLoc = std::hypot(xL, yL);
            if (rLoc > rMax && rLoc > 0.) { xL *= rMax/rLoc; yL *= rMax/rLoc; }
        }
        if (fSmearAngle > 0.) {
            G4ThreeVector d = SmearDirectionCone(dir, fSmearAngle);
            if (sign * d.z() > 0.) dir = d;
        }
    }

    G4double E = c.ekin;
    if (fSmearEfrac > 0.) {
        const G4double fr = 1.0 + G4RandGauss::shoot(0., fSmearEfrac);
        if (fr > 0.) E *= fr;
    }

    posLocal = G4ThreeVector(xL, yL, zL);
    dirLocal = dir.unit();
    ekin     = E;
    weight   = 1.0;          // alias table already absorbs the input weights
    pid      = c.pid;
    return true;
}

void SurfaceFluxSampler::EnsureLoaded() const
{
    std::call_once(fLoadOnce, [this]() {
        const_cast<SurfaceFluxSampler*>(this)->DoLoad();
    });
}

void SurfaceFluxSampler::DoLoad()
{
    if (fPendingFile.empty()) {
        G4Exception("SurfaceFluxSampler::DoLoad", "NoFile", FatalException,
                    "No source file set. Use /dcs-monitor/surf/sourceFile <path>.");
        return;
    }
    if (fPendingR_mm <= 0. || fPendingH_mm <= 0.) {
        G4Exception("SurfaceFluxSampler::DoLoad", "NoGeom", FatalException,
                    "Geometry parameters not configured (SetGeometryParameters).");
        return;
    }

    G4cout << "[SurfaceFluxSampler] Lazy load on thread "
           << G4Threading::G4GetThreadId() << " : " << fPendingFile << G4endl;

    Load(fPendingFile, fPendingTree, fPendingR_mm, fPendingH_mm, fPendingTol_mm);

    fLoaded = true;
    G4cout << "[SurfaceFluxSampler] Done loading; run can start." << G4endl;
}

bool SurfaceFluxSampler::ForceLoad()
{
    EnsureLoaded();
    return fLoaded && !fAll.data.empty();
}

void SurfaceFluxSampler::StartRun(G4double measurement_time)
{
    if (!ForceLoad()) {
        G4Exception("SurfaceFluxSampler::StartRun", "LoadFail", FatalException,
                    "No usable surface crossings loaded.");
        return;
    }
    if (fKeptWeightTotal <= 0. || fNumPrimaries <= 0 ||
        fDecayRate <= 0. || measurement_time <= 0.) {
        G4ExceptionDescription ed;
        ed << "Invalid input(s): keptWeightTotal=" << fKeptWeightTotal
           << ", numPrimaries=" << fNumPrimaries
           << ", decayRate[1/s]=" << fDecayRate
           << ", measTime[s]=" << measurement_time / CLHEP::s;
        G4Exception("SurfaceFluxSampler::StartRun", "BadParams",
                    FatalException, ed);
        return;
    }

    const G4double ratio = fKeptWeightTotal / static_cast<G4double>(fNumPrimaries);
    const G4double numDecays    = (measurement_time / CLHEP::s) * fDecayRate;
    const G4double numPrimaries = numDecays * ratio;

    G4cout << "[SurfaceFluxSampler::StartRun] ratio = " << ratio
           << "  (" << fKeptWeightTotal << " weighted crossings / "
           << fNumPrimaries << " primaries)\n"
           << "  decays in " << measurement_time / CLHEP::s << " s = " << numDecays << '\n'
           << "  primaries to generate = " << numPrimaries << G4endl;

    if (numPrimaries < 1.0) {
        G4Exception("SurfaceFluxSampler::StartRun", "TooFew", JustWarning,
                    "Computed numPrimaries < 1; not starting a run.");
        return;
    }
    if (numPrimaries >= 1e9) {
        G4Exception("SurfaceFluxSampler::StartRun", "TooMany", FatalException,
                    "More than 1e9 primaries requested; check inputs.");
        return;
    }
    const long long Nrun = static_cast<long long>(numPrimaries);
    G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn " + std::to_string(Nrun));
}

