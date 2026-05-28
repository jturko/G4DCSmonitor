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
    // Second pass: re-enable all branches and read everything. We now
    // have exact (upper-bound) sizes for the per-species buckets. The
    // surface-tolerance / outgoing-only cuts may reject some entries,
    // but reserving the count rather than the full N still avoids any
    // reallocation in the hot loop and uses the right amount of memory.
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
    fAll     .data.reserve(nNeutrons + nGammas);   // 'other' species ignored below

    // global coords:
    const G4double  R     = fPendingR_mm;
    const G4double  Hhalf = 0.5 * fPendingH_mm;
    const G4double  tol   = fPendingTol_mm;
    const G4ThreeVector& T   = fPendingPos_mm;
    const G4RotationMatrix& Rinv = fPendingRotInv;
    prog = ProgressBar(N);
    Long64_t keptSide = 0, rejEndcap = 0, rejNotSurf = 0,
             rejIncoming = 0, rejUnknownPid = 0;
    for (Long64_t i = 0; i < N; ++i) {
        prog.Print(i);
        t->GetEntry(i);

        // -- global -> cask-local --
        G4ThreeVector pGlob(x,  y,  z );
        G4ThreeVector mGlob(px, py, pz);
        G4ThreeVector pLoc = Rinv * (pGlob - T);
        G4ThreeVector mLoc = Rinv *  mGlob;

        // Local frame: cylinder axis along z, side at sqrt(x^2+y^2)=R,
        // endcaps at z=+/-Hhalf.
        const G4double rLoc = std::hypot(pLoc.x(), pLoc.y());

        // --- side-surface filter -----------------------------------------
        const G4bool onSide   = (std::fabs(rLoc -    R    ) < tol) &&
                                (std::fabs(pLoc.z())       < Hhalf - tol);
        const G4bool onEndcap = (std::fabs(std::fabs(pLoc.z()) - Hhalf) < tol) &&
                                (rLoc < R + tol);

        if (!onSide) {
            if (onEndcap) ++rejEndcap;
            else          ++rejNotSurf;
            continue;
        }
        // -----------------------------------------------------------------

        // Outward radial normal in local frame.
        const G4double nx = pLoc.x() / rLoc;
        const G4double ny = pLoc.y() / rLoc;

        const G4double pMag = mLoc.mag();
        if (pMag <= 0.) { ++rejIncoming; continue; }

        const G4double pxh = mLoc.x() / pMag;
        const G4double pyh = mLoc.y() / pMag;
        const G4double mu  = pxh * nx + pyh * ny;     // p . n_out
        if (mu <= 0.) { ++rejIncoming; continue; }    // keep outgoing only

        Crossing c;
        c.pid    = static_cast<G4int>(pid);
        c.ekin   = static_cast<G4float>(ekin);
        c.x  = (G4float)pLoc.x();
        c.y  = (G4float)pLoc.y();
        c.z  = (G4float)pLoc.z();
        c.px = (G4float)(mLoc.x() / pMag);   // store unit direction
        c.py = (G4float)(mLoc.y() / pMag);
        c.pz = (G4float)(mLoc.z() / pMag);
        c.weight = static_cast<G4float>(w);

        if      (c.pid == 2112) fNeutrons.data.push_back(c);
        else if (c.pid == 22)   fGammas  .data.push_back(c);
        else                    { ++rejUnknownPid; continue; }
        ++keptSide;
    }

    G4cout << "[SurfaceFluxSampler] Load summary:\n"
           << "    kept (side)        : " << keptSide      << '\n'
           << "    rejected (endcap)  : " << rejEndcap     << '\n'
           << "    rejected (not surf): " << rejNotSurf    << '\n'
           << "    rejected (incoming): " << rejIncoming   << '\n'
           << "    rejected (other id): " << rejUnknownPid << G4endl;



    //Long64_t kept = 0;
    //for (Long64_t i = 0; i < N; ++i) {
    //    t->GetEntry(i);

    //    // Reject anything that didn't come out the side.
    //    const G4double r = std::hypot(x, y);
    //    if (std::fabs(r - R) > tol)       continue;
    //    if (std::fabs(z)     >  Hhalf)    continue;

    //    // Outward radial normal in cask-local frame.
    //    const G4double nx = x / r, ny = y / r;
    //    const G4double mu = px*nx + py*ny;        // p . n
    //    if (mu <= 0.) continue;                   // keep outgoing only

    //    Crossing c;
    //    c.pid    = static_cast<G4int>(pid);
    //    c.ekin   = static_cast<G4float>(ekin);
    //    c.x = (G4float)x;  c.y = (G4float)y;  c.z = (G4float)z;
    //    c.px=(G4float)px;  c.py=(G4float)py;  c.pz=(G4float)pz;
    //    c.weight = static_cast<G4float>(w);

    //    fAll.data.push_back(c);
    //    if      (c.pid == 2112) fNeutrons.data.push_back(c);
    //    else if (c.pid == 22)   fGammas  .data.push_back(c);
    //    ++kept;
    //}
    
    //G4cout << "[SurfaceFluxSampler] Loaded " << kept << " / " << N
    //       << " side-outgoing crossings ("
    //       << fNeutrons.data.size() << " n, "
    //       << fGammas  .data.size() << " gamma)" << G4endl;

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
