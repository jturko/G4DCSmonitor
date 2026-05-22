//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef MuonScintSiPMSD_h
#define MuonScintSiPMSD_h 1

#include "MuonScintHit.hh"

#include "G4VSensitiveDetector.hh"
#include "globals.hh"

#include <map>

class G4Step;
class G4HCofThisEvent;

/// Sensitive detector for the SiPM volumes attached to a MuonScint slab.
///
/// Behaviour (per step):
///   * Filters to optical photons only (charged-particle steps are ignored).
///   * Triggers on geometry-boundary entry into the SiPM volume.
///   * Increments the per-SiPM "incident" counter, then applies a Bernoulli
///     draw against fPDE (default 43% from Rao et al. [[12]]).
///   * On detection: increments "detected" counter, accumulates running
///     sums of wavelength & wavelength^2, captures earliest time + position.
///   * KILLS the photon unconditionally (the SiPM is opaque).
///
/// Output (EndOfEvent): writes one row per (event, SiPM) with non-zero
/// detected counts to the analysis-manager NTuple at index `fNtupleId`
/// (default 3 -- assumes "sipmHits" is the 4th NTuple in HistoManager).
///
/// One hit per SiPM per event => bounded I/O even at very high light yield.

class MuonScintSiPMSD : public G4VSensitiveDetector
{
  public:
    MuonScintSiPMSD(const G4String& name,
                    const G4String& hitsCollectionName);
    ~MuonScintSiPMSD() override = default;

    void Initialize  (G4HCofThisEvent* hce) override;
    G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;
    void EndOfEvent  (G4HCofThisEvent* hce) override;

    // Configuration
    void     SetPDE(G4double pde)     { fPDE = pde; }
    G4double GetPDE() const           { return fPDE; }

    // NTuple slot. Must match the index assigned in HistoManager::Book().
    void  SetNtupleId(G4int id)       { fNtupleId = id; }
    G4int GetNtupleId() const         { return fNtupleId; }

  private:
    MuonScintHitsCollection* fHitsCollection = nullptr;

    // Maps (det << 16 | sipm) -> hit index in fHitsCollection.
    // Lets ProcessHits find the existing aggregate hit in O(log N).
    std::map<G4int, G4int> fSiPMHitIndexMap;

    G4double fPDE      = 0.43;     // Rao 2025 Table II [[12]]
    G4int    fNtupleId = 3;        // 4th NTuple (after primary/hits/surfaceFlux)
};

#endif

