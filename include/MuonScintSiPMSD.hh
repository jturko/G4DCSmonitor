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
class DetectorConstruction;

/// Sensitive detector for the SiPM volumes attached to a MuonScint slab.
///
/// Behaviour (per step):
///   * Filters to optical photons only (charged-particle steps are ignored).
///   * The grease<->SiPM boundary is a `dielectric_metal` optical surface with
///     EFFICIENCY = PDE(lambda) and REFLECTIVITY = 0. G4OpBoundaryProcess
///     performs the PDE Bernoulli draw internally and, on success, invokes
///     this SD with status Detection (non-zero energy deposit). ProcessHits is
///     therefore only called for detected photoelectrons.
///   * On detection: increments the per-SiPM photoelectron count and
///     accumulates running sums of wavelength & wavelength^2, capturing the
///     earliest time + position.
///   * KILLS the photon unconditionally (the SiPM is opaque).
///
/// Output (EndOfEvent): writes one row per (event, SiPM) of every slab the
/// primary muon entered -- including zero-detection SiPMs -- to the
/// analysis-manager NTuple at index `fNtupleId` (default 3 -- assumes
/// "sipmHits" is the 4th NTuple in HistoManager). Besides the per-SiPM
/// photoelectron count it writes the primary muon's first-entry slab-local
/// x-y (muonX/muonY) and the event's total optical photons produced in the
/// slab (nProduced), so the collection efficiency nDetected/nProduced can be
/// binned offline at any cell size and for any SiPM subset.
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

    // NTuple slot. Must match the index assigned in HistoManager::Book().
    void  SetNtupleId(G4int id)       { fNtupleId = id; }
    G4int GetNtupleId() const         { return fNtupleId; }

    // Detector construction, used to map each hit SiPM LV back to its slab
    // index so the per-slab collected-photon counters can be incremented.
    void SetDetector(DetectorConstruction* det) { fDetector = det; }

  private:
    MuonScintHitsCollection* fHitsCollection = nullptr;

    // Maps (det << 16 | sipm) -> hit index in fHitsCollection.
    // Lets ProcessHits find the existing aggregate hit in O(log N).
    std::map<G4int, G4int> fSiPMHitIndexMap;

    G4int fNtupleId = 3;        // 4th NTuple (after primary/hits/surfaceFlux)

    DetectorConstruction* fDetector = nullptr;
};

#endif

