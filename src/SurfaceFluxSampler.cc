#include "SurfaceFluxSampler.hh"

#include "Randomize.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"
#include "G4Threading.hh"
#include "G4UImanager.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

#include "ProgressBar.hh"

#include "TFile.h"
#include "TTree.h"
#include "TError.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <string>

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
        // Fallback to uniform if all weights are zero/negative
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

void SurfaceFluxSampler::Load(const std::string& filename,
                              const std::string& treeName,
                              G4double caskOuterRadius_mm,
                              G4double caskHeight_mm,
                              G4double surfaceTolerance_mm)
{
    if (fLoaded) return;

    std::unique_ptr<TFile> f(TFile::Open(filename.c_str(), "READ"));
    if (!f || f->IsZombie()) {
        G4Exception("SurfaceFluxSampler::Load", "OpenFail", FatalException,
                    ("Cannot open " + filename).c_str());
        return;
    }
    auto* t = dynamic_cast<TTree*>(f->Get(treeName.c_str()));
    if (!t) {
        G4Exception("SurfaceFluxSampler::Load", "NoTree", FatalException,
                    ("No tree " + treeName + " in " + filename).c_str());
        return;
    }

    // ===================================
    gErrorIgnoreLevel = kError;
    t->SetCacheSize(512LL * 1024 * 1024);  // 512 MB
    t->AddBranchToCache("*", kTRUE);
    t->StopCacheLearningPhase();
    // ===================================

    // ------------------------------------------------------------------
    // First pass: count species exactly. Only the 'pid' branch is read,
    // so this is essentially I/O-bound on a single column and very fast
    // even for trees with O(10^8) entries.
    // ------------------------------------------------------------------
    Double_t pid = 0, w = 0;
    t->SetBranchStatus("*", 0);
    t->SetBranchStatus("pid", 1);
    t->SetBranchStatus("weight", 1);
    t->SetBranchAddress ("pid", &pid);
    t->SetBranchAddress ("weight", &w);
    const Long64_t N = (fSurfaceSourceMaxEntriesLoadedFromTree > 0 &&
                    fSurfaceSourceMaxEntriesLoadedFromTree <= t->GetEntries())
                   ? fSurfaceSourceMaxEntriesLoadedFromTree
                   : t->GetEntries();

    Long64_t nNeutrons = 0, nGammas = 0, nOther = 0;
    Double_t wNeutrons = 0.,wGammas = 0.,wOther = 0.;
    ProgressBar prog(N);
    for (Long64_t i = 0; i < N; ++i) {
        if(i % 1000000 == (1000000-1)) prog.Print(i);
        t->GetEntry(i);
        const G4int p = static_cast<G4int>(pid);
        if      (p == 2112) { ++nNeutrons; wNeutrons += w; }
        else if (p == 22)   { ++nGammas;   wGammas += w; }
        else                { ++nOther;    wOther += w; }
    }
    G4cout << "[SurfaceFluxSampler] First pass: " << G4endl
           << "[SurfaceFluxSampler]     neutrons: " << nNeutrons << "\t, (sum_w = " << wNeutrons                  << ")" << G4endl
           << "[SurfaceFluxSampler]     gammas  : " << nGammas   << "\t, (sum_w = " << wGammas                    << ")" << G4endl
           << "[SurfaceFluxSampler]     other   : " << nOther    << "\t, (sum_w = " << wOther                     << ")" << G4endl
           << "[SurfaceFluxSampler]     total   : " << N         << "\t, (sum_w = " << (wNeutrons+wGammas+wOther) << ")" << G4endl;

    // ------------------------------------------------------------------
    // Second pass: full read. The tree is already stored in cask-local
    // coordinates, so we apply NO transform here -- only the side/outgoing
    // filter against the local cylinder (axis = z, radius = R).
    // ------------------------------------------------------------------
    t->SetBranchStatus("*", 1);
    Double_t ekin, tt, x, y, z, px, py, pz;
    t->SetBranchAddress("pid",    &pid);
    t->SetBranchAddress("ekin",   &ekin);
    t->SetBranchAddress("t",      &tt);
    t->SetBranchAddress("x",      &x);
    t->SetBranchAddress("y",      &y);
    t->SetBranchAddress("z",      &z);
    t->SetBranchAddress("px",     &px);
    t->SetBranchAddress("py",     &py);
    t->SetBranchAddress("pz",     &pz);
    t->SetBranchAddress("weight", &w);

    //fNeutrons.data.reserve(nNeutrons);
    //fGammas  .data.reserve(nGammas);
    fAll     .data.reserve(nNeutrons + nGammas);

    const G4double R     = fPendingR_mm;
    const G4double Hhalf = 0.5 * fPendingH_mm;
    const G4double tol   = fPendingTol_mm;

    ProgressBar prog2(N);
    Long64_t keptSide = 0, rejEndcap = 0, rejNotSurf = 0,
             rejIncoming = 0, rejUnknownPid = 0;
    Double_t keptSideWeight = 0, rejEndcapWeight = 0, rejNotSurfWeight = 0,
             rejIncomingWeight = 0, rejUnknownPidWeight = 0;

    for (Long64_t i = 0; i < N; ++i) {
        if(i % 1000000 == (1000000-1)) prog2.Print(i);
        t->GetEntry(i);

        // No transform -- input is already in cask-local frame.
        const G4double rLoc = std::hypot(x, y);

        const G4bool onSide   = (std::fabs(rLoc - R) < tol) &&
                                (std::fabs(z)        < Hhalf - tol);
        const G4bool onEndcap = (std::fabs(std::fabs(z) - Hhalf) < tol) &&
                                (rLoc < R + tol);

        if (!onSide) {
            if (onEndcap) { ++rejEndcap;  rejEndcapWeight += w; }
            else          { ++rejNotSurf; rejNotSurfWeight += w; }
            continue;
        }

        // Outward radial normal in local frame.
        const G4double nx = x / rLoc;
        const G4double ny = y / rLoc;

        const G4double pMag = std::sqrt(px*px + py*py + pz*pz);
        if (pMag <= 0.) { ++rejIncoming; rejIncomingWeight += w; continue; }

        const G4double pxh = px / pMag;
        const G4double pyh = py / pMag;
        const G4double mu  = pxh * nx + pyh * ny;   // p_hat . n_out
        if (mu <= 0.) { ++rejIncoming; rejIncomingWeight += w; continue; }  // outgoing only

        Crossing c;
        c.pid    = static_cast<G4int>(pid);
        c.ekin   = static_cast<G4float>(ekin);
        c.x      = (G4float)x;
        c.y      = (G4float)y;
        c.z      = (G4float)z;
        c.px     = (G4float)pxh;
        c.py     = (G4float)pyh;
        c.pz     = (G4float)(pz / pMag);
        c.weight = static_cast<G4float>(w);

        //if      (c.pid == 2112) fNeutrons.data.push_back(c);
        //else if (c.pid == 22)   fGammas  .data.push_back(c);
        //else                    { ++rejUnknownPid; rejUnknownPid += w; continue; }
        if( c.pid != 2112 && c.pid != 22 ) { ++rejUnknownPid; rejUnknownPidWeight += w; continue; }

        // Keep an "any species" pool too, so fRequestedPid==0 still works.
        fAll.data.push_back(c);
        ++keptSide;
        keptSideWeight += w;
    }

    G4cout << "[SurfaceFluxSampler] Load summary (cask-local input):\n"
           << "                         kept (side)        : " << keptSide      << ",\t weight = " << keptSideWeight       << '\n'
           << "                         rejected (endcap)  : " << rejEndcap     << ",\t weight = " << rejEndcapWeight      << '\n'
           << "                         rejected (not surf): " << rejNotSurf    << ",\t weight = " << rejNotSurfWeight     << '\n'
           << "                         rejected (incoming): " << rejIncoming   << ",\t weight = " << rejIncomingWeight    << '\n'
           << "                         rejected (other id): " << rejUnknownPid << ",\t weight = " << rejUnknownPidWeight  << G4endl;

    G4cout << "[SurfaceFluxSampler] For " << fNumPrimaries << " primaries, we have " << keptSideWeight << " weighted entries leaving the cask sides" << '\n'
           << "                     The surface flux to primaries ratio is: " << keptSideWeight/double(fNumPrimaries) << G4endl;
    t->PrintCacheStats();

    //BuildAlias(fNeutrons);
    //BuildAlias(fGammas);
    BuildAlias(fAll);

    fLoaded = true;
    
    fKeptSide = keptSide;
    fKeptSideWeight = keptSideWeight;

    gErrorIgnoreLevel = kWarning;
}

const SurfaceFluxSampler::Bucket*
SurfaceFluxSampler::GetBucket(G4int pid) const
{
    //if (pid == 2112) return &fNeutrons;
    //if (pid == 22)   return &fGammas;
    return &fAll;
}

G4int SurfaceFluxSampler::NumEntries(G4int pid) const
{
    auto* b = GetBucket(pid);
    return b ? static_cast<G4int>(b->data.size()) : 0;
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

    // Walker alias step: O(1) pick of a kernel centre.
    const std::size_t N = b->data.size();
    const G4double u    = G4UniformRand() * N;
    const std::size_t i = static_cast<std::size_t>(u);
    const G4double frac = u - i;
    const std::size_t k = (frac < b->prob[i]) ? i : b->alias[i];

    const auto& c = b->data[k];

    // ---------------------------------------------------------------
    // Position smearing -- stay on the cylinder side surface so the
    // emission point remains physically meaningful for step 2.
    //   * radial coordinate locked to the stored radius
    //   * (phi, z) smeared by independent Gaussians
    // ---------------------------------------------------------------
    G4double xL = c.x, yL = c.y, zL = c.z;
    if (fSmearPhi > 0. || fSmearZ > 0.) {
        const G4double rLoc = std::hypot(c.x, c.y);  // ~ fPendingR_mm
        G4double phi0 = std::atan2(c.y, c.x);

        if (fSmearPhi > 0.) phi0 += G4RandGauss::shoot(0., fSmearPhi);
        if (fSmearZ   > 0.) zL   += G4RandGauss::shoot(0., fSmearZ);

        // Clamp z to half-height minus tolerance so we don't accidentally
        // wander onto the endcap region.
        const G4double zMax = 0.5 * fPendingH_mm - fPendingTol_mm;
        if (zL >  zMax) zL =  zMax;
        if (zL < -zMax) zL = -zMax;

        xL = rLoc * std::cos(phi0);
        yL = rLoc * std::sin(phi0);
    }

    // ---------------------------------------------------------------
    // Direction smearing -- small-angle cone around the stored dir.
    // We sample (theta_smear, phi_smear) with theta_smear from a
    // half-Gaussian of width fSmearAngle, then rotate that cone-axis
    // vector into the frame whose z-axis is the stored direction.
    //
    // If the resulting direction is no longer outgoing (i.e. would go
    // into the cask), we reject and fall back to the unsmeared dir
    // rather than try to fix it up -- this preserves the angular PDF
    // on the outward hemisphere.
    // ---------------------------------------------------------------
    G4ThreeVector dir(c.px, c.py, c.pz);
    if (fSmearAngle > 0.) {
        const G4double thetaSmear = std::fabs(G4RandGauss::shoot(0., fSmearAngle));
        const G4double phiSmear   = 2.0 * M_PI * G4UniformRand();
        const G4double cs = std::cos(thetaSmear);
        const G4double sn = std::sin(thetaSmear);

        G4ThreeVector dDir(sn * std::cos(phiSmear),
                           sn * std::sin(phiSmear),
                           cs);
        dDir.rotateUz(dir);   // align cone axis with original direction

        // Reject if it would re-enter the cask (n_out . dir <= 0).
        const G4double invR = 1.0 / std::hypot(xL, yL);
        const G4double nx = xL * invR, ny = yL * invR;
        if (dDir.x() * nx + dDir.y() * ny > 0.) {
            dir = dDir;
        }
        // else: keep original `dir`, which is guaranteed outgoing.
    }

    // ---------------------------------------------------------------
    // Energy smearing -- fractional Gaussian, clamped strictly > 0.
    // ---------------------------------------------------------------
    G4double E = c.ekin;
    if (fSmearEfrac > 0.) {
        const G4double f = 1.0 + G4RandGauss::shoot(0., fSmearEfrac);
        if (f > 0.) E *= f;
        // If f<=0 we keep E unchanged (extremely rare for sensible sigmas).
    }

    posLocal = G4ThreeVector(xL, yL, zL);
    dirLocal = dir.unit();
    ekin     = E;
    weight   = 1.0;   // alias table already absorbs the input weights
    pid      = c.pid;
    return true;
}

void SurfaceFluxSampler::EnsureLoaded() const
{
    // Cast away const only on the once_flag/load path. The data itself
    // remains logically const after DoLoad() finishes.
    std::call_once(fLoadOnce, [this]() {
        const_cast<SurfaceFluxSampler*>(this)->DoLoad();
    });
}

void SurfaceFluxSampler::DoLoad()
{
    if (fPendingFile.empty()) {
        G4Exception("SurfaceFluxSampler::DoLoad", "NoFile", FatalException,
                    "No source file has been set. Call "
                    "/dcs-monitor/surf/sourceFile <path> before /run/beamOn.");
        return;
    }
    if (fPendingR_mm <= 0. || fPendingH_mm <= 0.) {
        G4Exception("SurfaceFluxSampler::DoLoad", "NoGeom", FatalException,
                    "Geometry parameters not configured. Did SetGeometryParameters()"
                    " get called from PrimaryGeneratorAction?");
        return;
    }

    G4cout << "[SurfaceFluxSampler] Lazy load triggered on thread "
           << G4Threading::G4GetThreadId()
           << " from file: " << fPendingFile << G4endl;

    Load(
        fPendingFile,
        fPendingTree,
        fPendingR_mm,
        fPendingH_mm,
        fPendingTol_mm
    );

    fLoaded = true;
    G4cout << "[SurfaceFluxSampler] Done loading! The run can start now..." << G4endl;
}

bool SurfaceFluxSampler::ForceLoad()
{
    EnsureLoaded();   // call_once does the work exactly once
    return fLoaded && fKeptSide > 0;
}

void SurfaceFluxSampler::StartRun(G4double measurement_time)
{
    // Step 0: trigger / verify the lazy load.
    if (!ForceLoad()) {
        G4Exception("SurfaceFluxSampler::StartRun", "LoadFail", FatalException,
                    "Sampler did not load any usable side-surface crossings. "
                    "Check the input file, geometry parameters, and PID filter.");
        return;
    }

    // Step 0.5: parameter sanity.
    if (fKeptSideWeight <= 0. || fNumPrimaries <= 0 ||
        fDecayRate <= 0. || measurement_time <= 0.) {
        G4ExceptionDescription ed;
        ed << "Invalid input(s): "
           << "keptSideWeight="     << fKeptSideWeight
           << ", numPrimaries="     << fNumPrimaries
           << ", decayRate[1/s]="   << fDecayRate
           << ", measTime[s]="      << measurement_time / CLHEP::s;
        G4Exception("SurfaceFluxSampler::StartRun", "BadParams",
                    FatalException, ed);
        return;
    }

    // Steps 1..3: compute the number of primaries.
    const G4double surfaceToPrimaryRatio =
        fKeptSideWeight / static_cast<G4double>(fNumPrimaries);
    const G4double numDecays    = (measurement_time / CLHEP::s) * fDecayRate;
    const G4double numPrimaries = numDecays * surfaceToPrimaryRatio;

    G4cout << "[SurfaceFluxSampler::StartRun] "
           << "ratio = " << surfaceToPrimaryRatio
           << "  ( " << fKeptSideWeight
           << " weighted side crossings / "
           << fNumPrimaries << " primaries )\n"
           << "                              "
           << "decays in " << measurement_time / CLHEP::s << " s = "
           << numDecays << '\n'
           << "                              "
           << "primaries to generate = " << numPrimaries << G4endl;

    // Step 4: launch the run.
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

    const long long N = static_cast<long long>(numPrimaries);
    G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn " + std::to_string(N));
}
