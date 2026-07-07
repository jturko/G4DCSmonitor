#ifndef GEOMETRYHEMISHIELD_HH
#define GEOMETRYHEMISHIELD_HH 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Colour.hh"
#include <vector>

class G4AssemblyVolume;
class G4VPhysicalVolume;
class G4LogicalVolume;

class GeometryHemiShield
{
  public:
    GeometryHemiShield();
    ~GeometryHemiShield();

    G4int Build();
    void PlaceDetector(G4LogicalVolume* expHallLog, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo=0);


  private:
    void BuildMaterials();

    G4AssemblyVolume* fHemiShieldAssembly;

};

#endif

