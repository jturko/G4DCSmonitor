//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef MuonScintHit_h
#define MuonScintHit_h 1

#include "G4Allocator.hh"
#include "G4THitsCollection.hh"
#include "G4Threading.hh"
#include "G4ThreeVector.hh"
#include "G4VHit.hh"

/// Hit class for the MuonScint SiPM sensitive detector.
///
/// One hit per (event, SiPM) -- aggregates all detected photons that hit
/// that SiPM during the event. Mirrors the "shower container" pattern of
/// DCSMonitorSD: each hit accumulates running sums; EndOfEvent flushes
/// one row per hit to the analysis manager.

class MuonScintHit : public G4VHit
{
  public:
    MuonScintHit() = default;
    MuonScintHit(const MuonScintHit&) = default;
    ~MuonScintHit() override = default;

    MuonScintHit& operator=(const MuonScintHit&) = default;
    G4bool operator==(const MuonScintHit&) const;

    inline void* operator new(size_t);
    inline void  operator delete(void*);

    void Draw()  override;
    void Print() override;

    // ---- Setters / accumulators ----------------------------------------
    void SetDetNum   (G4int n)              { fDetNum  = n; }
    void SetSiPMNum  (G4int n)              { fSiPMNum = n; }
    void SetWeight   (G4double w)           { fWeight  = w; }

    // Add an incident photon (one that entered the SiPM volume but may or
    // may not have been detected).
    void AddIncident()                      { ++fNIncident; }

    // Add a detected photon. Updates running sums, first-arrival time, and
    // first-arrival position. Wavelength `lam` is in nm.
    void AddDetected(G4double t,
                     G4double lam_nm,
                     const G4ThreeVector& pos)
    {
        if (fNDetected == 0 || t < fTFirst) {
            fTFirst   = t;
            fPosFirst = pos;
        }
        ++fNDetected;
        fSumLam  += lam_nm;
        fSumLam2 += lam_nm * lam_nm;
    }

    // ---- Getters -------------------------------------------------------
    G4int    GetDetNum()    const { return fDetNum; }
    G4int    GetSiPMNum()   const { return fSiPMNum; }
    G4int    GetNDetected() const { return fNDetected; }
    G4int    GetNIncident() const { return fNIncident; }
    G4double GetTFirst()    const { return fTFirst; }
    G4ThreeVector GetPosFirst() const { return fPosFirst; }
    G4double GetWeight()    const { return fWeight; }

    G4double GetMeanWavelength() const {
        return (fNDetected > 0) ? (fSumLam / fNDetected) : 0.0;
    }
    G4double GetRMSWavelength() const {
        if (fNDetected < 2) return 0.0;
        const G4double mean = fSumLam / fNDetected;
        const G4double var  = fSumLam2 / fNDetected - mean * mean;
        return (var > 0.0) ? std::sqrt(var) : 0.0;
    }

  private:
    G4int         fDetNum    = -1;     // parent slab assembly copyNo
    G4int         fSiPMNum   = -1;     // SiPM copyNo within the parent slab
    G4int         fNDetected =  0;     // photons that PASSED the PDE Bernoulli
    G4int         fNIncident =  0;     // photons that entered the SiPM volume
    G4double      fTFirst    =  0.;    // time of first detected photon
    G4ThreeVector fPosFirst;           // global position of first detected photon
    G4double      fSumLam    =  0.;    // running sum of wavelengths [nm]
    G4double      fSumLam2   =  0.;    // running sum of wavelengths^2 [nm^2]
    G4double      fWeight    =  1.0;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

using MuonScintHitsCollection = G4THitsCollection<MuonScintHit>;

extern G4ThreadLocal G4Allocator<MuonScintHit>* MuonScintHitAllocator;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

inline void* MuonScintHit::operator new(size_t)
{
    if (!MuonScintHitAllocator) MuonScintHitAllocator = new G4Allocator<MuonScintHit>;
    return (void*)MuonScintHitAllocator->MallocSingle();
}

inline void MuonScintHit::operator delete(void* hit)
{
    MuonScintHitAllocator->FreeSingle((MuonScintHit*)hit);
}

#endif

