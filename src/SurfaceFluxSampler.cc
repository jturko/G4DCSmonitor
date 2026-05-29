#include "SurfaceFluxSampler.hh"

#include "G4SystemOfUnits.hh"
#include "Randomize.hh"
#include "G4Exception.hh"
#include "G4Threading.hh"

#include "ProgressBar.hh"

#include "TFile.h"
#include "TTree.h"

#include <algorithm>
#include <cmath>
#include <queue>

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

    //const G4double R    = caskOuterRadius_mm;
    //const G4double Hhalf= 0.5 * caskHeight_mm;
    //const G4double tol  = surfaceTolerance_mm;

    // ------------------------------------------------------------------
    // First pass: count species exactly. Only the 'pid' branch is read,
    // so this is essentially I/O-bound on a single column and very fast
    // even for trees with O(10^8) entries.
    // ------------------------------------------------------------------
    Double_t pid = 0;
    t->SetBranchStatus("*", 0);
    t->SetBranchStatus("pid", 1);
    t->SetBranchAddress ("pid", &pid);
    const Long64_t N = t->GetEntries();
    Long64_t nNeutrons = 0, nGammas = 0, nOther = 0;
    ProgressBar prog(N);
    for (Long64_t i = 0; i < N; ++i) {
        prog.Print(i);
        t->GetEntry(i);
        const G4int p = static_cast<G4int>(pid);
        if      (p == 2112) ++nNeutrons;
        else if (p == 22)   ++nGammas;
        else                ++nOther;
    }
    G4cout << "[SurfaceFluxSampler] First pass: "
           << nNeutrons << " neutrons, "
           << nGammas   << " gammas, "
           << nOther    << " other (of " << N << " total)" << G4endl;

    // ------------------------------------------------------------------
    // Second pass: full read. The tree is already stored in cask-local
    // coordinates, so we apply NO transform here -- only the side/outgoing
    // filter against the local cylinder (axis = z, radius = R).
    // ------------------------------------------------------------------
    t->SetBranchStatus("*", 1);
    Double_t ekin, tt, x, y, z, px, py, pz, w;
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

    fNeutrons.data.reserve(nNeutrons);
    fGammas  .data.reserve(nGammas);
    fAll     .data.reserve(nNeutrons + nGammas);

    const G4double R     = fPendingR_mm;
    const G4double Hhalf = 0.5 * fPendingH_mm;
    const G4double tol   = fPendingTol_mm;

    ProgressBar prog2(N);
    Long64_t keptSide = 0, rejEndcap = 0, rejNotSurf = 0,
             rejIncoming = 0, rejUnknownPid = 0;

    for (Long64_t i = 0; i < N; ++i) {
        prog2.Print(i);
        t->GetEntry(i);

        // No transform -- input is already in cask-local frame.
        const G4double rLoc = std::hypot(x, y);

        const G4bool onSide   = (std::fabs(rLoc - R) < tol) &&
                                (std::fabs(z)        < Hhalf - tol);
        const G4bool onEndcap = (std::fabs(std::fabs(z) - Hhalf) < tol) &&
                                (rLoc < R + tol);

        if (!onSide) {
            if (onEndcap) ++rejEndcap;
            else          ++rejNotSurf;
            continue;
        }

        // Outward radial normal in local frame.
        const G4double nx = x / rLoc;
        const G4double ny = y / rLoc;

        const G4double pMag = std::sqrt(px*px + py*py + pz*pz);
        if (pMag <= 0.) { ++rejIncoming; continue; }

        const G4double pxh = px / pMag;
        const G4double pyh = py / pMag;
        const G4double mu  = pxh * nx + pyh * ny;   // p_hat . n_out
        if (mu <= 0.) { ++rejIncoming; continue; }  // outgoing only

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

        if      (c.pid == 2112) fNeutrons.data.push_back(c);
        else if (c.pid == 22)   fGammas  .data.push_back(c);
        else                    { ++rejUnknownPid; continue; }

        // Keep an "any species" pool too, so requestedPid==0 still works.
        fAll.data.push_back(c);
        ++keptSide;
    }

    G4cout << "[SurfaceFluxSampler] Load summary (cask-local input):\n"
           << "    kept (side)        : " << keptSide      << '\n'
           << "    rejected (endcap)  : " << rejEndcap     << '\n'
           << "    rejected (not surf): " << rejNotSurf    << '\n'
           << "    rejected (incoming): " << rejIncoming   << '\n'
           << "    rejected (other id): " << rejUnknownPid << G4endl;

    BuildAlias(fNeutrons);
    BuildAlias(fGammas);
    BuildAlias(fAll);

    fLoaded = true;
}

const SurfaceFluxSampler::Bucket*
SurfaceFluxSampler::GetBucket(G4int pid) const
{
    if (pid == 2112) return &fNeutrons;
    if (pid == 22)   return &fGammas;
    return &fAll;
}

G4int SurfaceFluxSampler::NumEntries(G4int pid) const
{
    auto* b = GetBucket(pid);
    return b ? static_cast<G4int>(b->data.size()) : 0;
}

bool SurfaceFluxSampler::Sample(G4int requestedPid,
                                G4ThreeVector& posLocal,
                                G4ThreeVector& dirLocal,
                                G4double&      ekin,
                                G4double&      weight,
                                G4int&         pid) const
{
    EnsureLoaded();

    const Bucket* b = GetBucket(requestedPid);
    if (!b || b->data.empty()) return false;

    // Walker alias step: O(1).
    const std::size_t N = b->data.size();
    const G4double u    = G4UniformRand() * N;
    const std::size_t i = static_cast<std::size_t>(u);
    const G4double frac = u - i;
    const std::size_t k = (frac < b->prob[i]) ? i : b->alias[i];

    const auto& c = b->data[k];
    posLocal  = G4ThreeVector(c.x, c.y, c.z);
    dirLocal  = G4ThreeVector(c.px, c.py, c.pz).unit();
    ekin      = c.ekin;
    weight    = 1.0; // weight is already absorbed into the alias-table probabilities
    pid       = c.pid;
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
                    "/dcs-monitor/gun/surfaceSourceFile <path> before /run/beamOn.");
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
    G4cout << "[SurfaceFluxSampler] Done the load!" << G4endl;
}
