#ifndef DCSMonitorSD_h
#define DCSMonitorSD_h 1

#include "DCSMonitorHit.hh"

#include "G4VSensitiveDetector.hh"
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"

#include <vector>
#include <map>

class G4Step;
class G4HCofThisEvent;
class G4LogicalVolume;

class DCSMonitorSD : public G4VSensitiveDetector
{
  public:
    DCSMonitorSD(const G4String& name, const G4String& hitsCollectionName);
    ~DCSMonitorSD() override = default;

    void   Initialize(G4HCofThisEvent* hitCollection) override;
    G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;
    void   EndOfEvent(G4HCofThisEvent* hitCollection) override;

    // Register a sensitive logical volume <-> detector ID. Called once per
    // detector from DetectorConstruction::ConstructSDandField().
    void SetDetectorID(const G4LogicalVolume* lv, G4int id) { fDetMap[lv] = id; }

    // Digitizer pulse-pair resolving time. Deposits in the SAME shower that
    // are separated by more than this gap form distinct pulses.
    void     SetResolvingTime(G4double t) { fResolvingTime = t; }
    G4double GetResolvingTime() const     { return fResolvingTime; }

  private:
    // A single raw energy deposit (not yet merged).
    struct Deposit {
        G4double      t    = 0.;   // global time
        G4double      edep = 0.;   // deposited energy
        G4ThreeVector pos;         // post-step position
        G4int         pid  = 0;    // PDG code of the depositing track
    };

    // One statistically-independent shower: a single track that entered the
    // crystal from outside, plus its secondaries created inside. It carries
    // ONE weight (the entry weight). Deposits from DIFFERENT showers -- e.g.
    // distinct geometric-biasing clones of the same primary -- must NEVER be
    // summed together.
    struct Shower {
        G4int                det    = -1;
        G4double             weight = 1.0;
        std::vector<Deposit> deps;
    };

    DCSMonitorHitsCollection* fHitsCollection = nullptr;

    std::map<G4int, G4int>    fTrackShowerMap;   // trackID -> index in fShowers
    std::vector<Shower>       fShowers;

    std::map<const G4LogicalVolume*, G4int> fDetMap;  // logical volume -> det id

    G4double fResolvingTime = 1.0 * CLHEP::us;
};

#endif

