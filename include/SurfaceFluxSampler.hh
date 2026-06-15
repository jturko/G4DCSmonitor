#ifndef SurfaceFluxSampler_h
#define SurfaceFluxSampler_h 1

#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "globals.hh"
#include <vector>
#include <string>
#include <mutex>

class SurfaceFluxSampler
{
  public:
    struct Crossing {
        G4float ekin;       // MeV
        G4float x, y, z;    // mm, cask-local frame
        G4float px, py, pz; // unit vector
        G4float weight;     // statistical weight from step 1
        G4int   pid;        // PDG code
        G4int   surf = 0;   // 0=side, 1=top cap, 2=bottom cap (classified inline)
    };

    static SurfaceFluxSampler& Instance();

    // Single-pass loader: reads EVERY entry of the step-1 'surfaceFlux' tree
    // (which is already pre-filtered to outgoing gammas/neutrons crossing the
    // cask body), classifies each crossing's surface inline, and builds the
    // alias table. No rejection, no second pass.
    void Load(const std::string& filename,
              const std::string& treeName,
              G4double caskOuterRadius_mm,
              G4double caskHeight_mm,
              G4double surfaceTolerance_mm = 2.0);

    bool Sample(G4ThreeVector& posLocal,
                G4ThreeVector& dirLocal,
                G4double&      ekin,
                G4double&      weight,
                G4int&         pid) const;

    G4bool IsLoaded() const { return fLoaded; }
    G4int  NumEntries(G4int pid) const;

    void SetSourceFile(const std::string& f) { fPendingFile = f; }
    const std::string& GetSourceFile() const { return fPendingFile; }
    void SetTreeName  (const std::string& t) { fPendingTree = t; }
    void SetGeometryParameters(G4double R_mm, G4double H_mm,
                               G4double tol_mm = 2.0) {
        fPendingR_mm   = R_mm;
        fPendingH_mm   = H_mm;
        fPendingTol_mm = tol_mm;
    }

    void  SetRequestedPid(G4int p) { fRequestedPid = p; }
    G4int GetRequestedPid() const  { return fRequestedPid; }

    void     SetSurfaceSourceMaxEntriesLoadedFromTree(long int n) { fSurfaceSourceMaxEntriesLoadedFromTree = n; }
    long int GetSurfaceSourceMaxEntriesLoadedFromTree() const     { return fSurfaceSourceMaxEntriesLoadedFromTree; }

    void SetSmearing(G4double sigmaPhi_rad, G4double sigmaZ_mm,
                     G4double sigmaAng_rad, G4double sigmaEfrac) {
        fSmearPhi = sigmaPhi_rad; fSmearZ = sigmaZ_mm;
        fSmearAngle = sigmaAng_rad; fSmearEfrac = sigmaEfrac;
    }
    void SetSmearPhi   (G4double v) { fSmearPhi   = v; }
    void SetSmearZ     (G4double v) { fSmearZ     = v; }
    void SetSmearAngle (G4double v) { fSmearAngle = v; }
    void SetSmearEfrac (G4double v) { fSmearEfrac = v; }

    void SetNumPrimaries(long int n) { fNumPrimaries = n; }
    void SetDecayRate(G4double rate) { fDecayRate = rate; }

    bool ForceLoad();
    void StartRun(G4double measurement_time);
    
    void     SetLoadThreads(long int n) { fLoadThreads = n; }   // 0 = all cores
    long int GetLoadThreads() const     { return fLoadThreads; }


  private:
    SurfaceFluxSampler() = default;
    SurfaceFluxSampler(const SurfaceFluxSampler&) = delete;
    SurfaceFluxSampler& operator=(const SurfaceFluxSampler&) = delete;

    long int fSurfaceSourceMaxEntriesLoadedFromTree = 0;

    void EnsureLoaded() const;
    void DoLoad();

    G4ThreeVector SmearDirectionCone(const G4ThreeVector& dir,
                                     G4double sigma) const;

    mutable std::once_flag fLoadOnce;
    std::string fPendingFile;
    std::string fPendingTree = "surfaceFlux";
    G4double    fPendingR_mm   = -1.;
    G4double    fPendingH_mm   = -1.;
    G4double    fPendingTol_mm =  2.;
    G4bool fLoaded = false;

    G4int fRequestedPid = 0;

    struct Bucket {
        std::vector<Crossing> data;
        std::vector<G4double> prob;
        std::vector<G4int>    alias;
    };
    void BuildAlias(Bucket& b);
    const Bucket* GetBucket(G4int pid) const;

    Bucket fAll;

    G4double fSmearPhi   = 0.;
    G4double fSmearZ     = 0.;
    G4double fSmearAngle = 0.;
    G4double fSmearEfrac = 0.;

    long int fNumPrimaries     = 0;
    G4double fDecayRate        = 0.;
    long int fKeptSide         = 0;
    G4double fKeptSideWeight   = 0.;
    G4double fKeptWeightTotal  = 0.;   // side + both caps
                                       
    long int fLoadThreads = 32;   // 0 => ROOT::EnableImplicitMT(0) uses all cores


};

#endif

