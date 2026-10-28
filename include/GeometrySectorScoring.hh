#ifndef GeometrySectorScoring_h
#define GeometrySectorScoring_h 1

#include "G4VUserParallelWorld.hh"
#include "globals.hh"

#include <vector>

class DetectorConstruction;
class G4LogicalVolume;

// Eight non-overlapping 45-degree scoring wedges centred on target cask 0.
// The wedges live only in a parallel world; they have no material and do not
// alter the mass geometry. A sensitive detector records the first entry of
// each gamma/neutron into each wedge.
class GeometrySectorScoring : public G4VUserParallelWorld
{
  public:
    static constexpr G4int    kNumSectors    = 8;
    static constexpr G4double kInnerRadiusMm = 1325.;
    static constexpr G4double kOuterRadiusMm = 1775.;
    static constexpr G4int    kMaxZLayers    = 32;

    GeometrySectorScoring(G4String worldName, DetectorConstruction* det);
    ~GeometrySectorScoring() override = default;

    void Construct() override;
    void ConstructSD() override;

  private:
    DetectorConstruction* fDetector = nullptr;
    std::vector<G4LogicalVolume*> fSectorLogicals;
};

#endif
