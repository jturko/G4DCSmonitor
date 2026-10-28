#ifndef SectorScoringSD_h
#define SectorScoringSD_h 1

#include "G4VSensitiveDetector.hh"
#include "G4ThreeVector.hh"

class DetectorConstruction;
class G4Step;
class G4TouchableHistory;

class SectorScoringSD : public G4VSensitiveDetector
{
  public:
    SectorScoringSD(const G4String& name, DetectorConstruction* det);
    ~SectorScoringSD() override = default;

    G4bool ProcessHits(G4Step* step, G4TouchableHistory* history) override;

  private:
    G4int ClassifyFace(const G4ThreeVector& globalPos, G4int sector,
                       G4int zLayer) const;

    DetectorConstruction* fDetector = nullptr;
};

#endif
