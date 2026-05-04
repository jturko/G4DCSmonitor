#ifndef GeometryParallelBiasing_h
#define GeometryParallelBiasing_h 1

#include "G4VUserParallelWorld.hh"
#include "globals.hh"

class DetectorConstruction; // Need this to access dynamic CASTOR placements

class GeometryParallelBiasing : public G4VUserParallelWorld {
  public:
    GeometryParallelBiasing(G4String worldName, DetectorConstruction* det);
    ~GeometryParallelBiasing() override = default;

  protected:
    void Construct() override;

  private:
    DetectorConstruction*   fDetector;
};

#endif
