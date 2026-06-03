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
        G4float x, y, z;    // mm, cask-local frame (already transformed at load time)
        G4float px, py, pz; // unit vector
        G4float weight;     // statistical weight from step 1
        G4int   pid;        // PDG code
    };

    static SurfaceFluxSampler& Instance();

    // Called once from BuildForMaster(): no Geant4 worker may call this.
    // Selects only side-surface crossings (within a tolerance of the cask
    // outer radius) and only outgoing tracks (p . n_hat > 0).
    void Load(const std::string& filename,
              const std::string& treeName,
              G4double caskOuterRadius_mm,
              G4double caskHeight_mm,
              G4double surfaceTolerance_mm = 1.0);

    // Sample one crossing for a given particle (pid==0 ⇒ any). Returns false
    // if no entries exist for the requested species.
    bool Sample(G4int requestedPid,
                G4ThreeVector& posLocal,
                G4ThreeVector& dirLocal,
                G4double&      ekin,
                G4double&      weight,
                G4int&         pid) const;

    G4bool IsLoaded() const { return fLoaded; }
    G4int  NumEntries(G4int pid) const;

    // local coordinates
    //void SetSourceFile(const std::string& f) { fPendingFile = f; }
    //const std::string& GetSourceFile() const { return fPendingFile; }
    //void SetTreeName  (const std::string& t) { fPendingTree = t; }
    //void SetGeometryParameters(G4double R_mm, G4double H_mm, 
    //                           const G4ThreeVector& caskPos_mm, const G4RotationMatrix* caskRot,
    //                           G4double tol_mm = 2.0) {
    //    fPendingR_mm = R_mm; fPendingH_mm = H_mm; fPendingTol_mm = tol_mm;
    //    // for global coords in surfaceFlux:
    //    fPendingRot    = caskRot ? *caskRot : G4RotationMatrix();
    //    fPendingRotInv = fPendingRot.inverse();
    //    fPendingTol_mm = tol_mm;
    //}
    
    // global coordinates
    void SetSourceFile(const std::string& f) { fPendingFile = f; }
    const std::string& GetSourceFile() const { return fPendingFile; }
    void SetTreeName  (const std::string& t) { fPendingTree = t; }
    void SetGeometryParameters(G4double R_mm, G4double H_mm,
                               G4double tol_mm = 2.0) {
        fPendingR_mm   = R_mm;
        fPendingH_mm   = H_mm;
        fPendingTol_mm = tol_mm;
    }

    void SetMaxEntries(long int n) { fMaxEntries = n; }

    // for smearing
    void SetSmearing(G4double sigmaPhi_rad,
                     G4double sigmaZ_mm,
                     G4double sigmaAng_rad,
                     G4double sigmaEfrac) {
        fSmearPhi    = sigmaPhi_rad;
        fSmearZ      = sigmaZ_mm;
        fSmearAngle  = sigmaAng_rad;
        fSmearEfrac  = sigmaEfrac;
    }
    void SetSmearPhi   (G4double v) { fSmearPhi   = v; }
    void SetSmearZ     (G4double v) { fSmearZ     = v; }
    void SetSmearAngle (G4double v) { fSmearAngle = v; }
    void SetSmearEfrac (G4double v) { fSmearEfrac = v; }

    // for calculation of surface flux prob.
    void SetNumPrimaries(long int n) { fNumPrimaries = n; }
    long int GetNumPrimaries() { return fNumPrimaries; }
    G4double GetKeptSideWeight() { return fKeptSideWeight; }   

  private:
    SurfaceFluxSampler() = default;
    SurfaceFluxSampler(const SurfaceFluxSampler&) = delete;
    SurfaceFluxSampler& operator=(const SurfaceFluxSampler&) = delete;

    void EnsureLoaded() const;          // const-qualified; uses mutable members
    void DoLoad();                       // the real work, called exactly once

    mutable std::once_flag fLoadOnce;
    std::string fPendingFile;
    std::string fPendingTree = "surfaceFlux";
    G4double    fPendingR_mm   = -1.;
    G4double    fPendingH_mm   = -1.;
    G4double    fPendingTol_mm =  2.;
    G4bool fLoaded = false;

    struct Bucket {
        std::vector<Crossing> data;
        // Walker's alias method
        std::vector<G4double> prob;
        std::vector<G4int>    alias;
    };

    void BuildAlias(Bucket& b);
    const Bucket* GetBucket(G4int pid) const;

    //Bucket fNeutrons;
    //Bucket fGammas;
    Bucket fAll;     // fallback when caller asks pid==0


    // for smearing
    G4double fSmearPhi   = 0.;    // rad
    G4double fSmearZ     = 0.;    // mm
    G4double fSmearAngle = 0.;    // rad
    G4double fSmearEfrac = 0.;    // dimensionless
                                  
    // for calc. of surface flux prob.
    long int fMaxEntries = 0;
    long int fNumPrimaries = 0;
    long int fKeptSide = 0;
    G4double fKeptSideWeight = 0.;

};

#endif

