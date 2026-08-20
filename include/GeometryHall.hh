#ifndef GEOMETRYHALL_HH
#define GEOMETRYHALL_HH 1

#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

class G4AssemblyVolume;
class G4LogicalVolume;

// Experimental hall 
// Convention (matches upright CASTOR 440 placement, axis along +z):
//   x -> along the hall length   y -> transverse (wall to wall)   z -> up
//   casks sit on the floor; floor top defaults to -caskHeight/2 = -2040 mm.
class GeometryHall
{
  public:
    GeometryHall();
    ~GeometryHall();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo = 0);

    // geometry
    void SetHallLength(G4double v)        { fHallLength = v; }
    void SetFloorTopZ(G4double v)         { fFloorTopZ = v; }
    void SetFloorThickness(G4double v)    { fFloorThickness = v; }
    void SetWallInnerY(G4double v)        { fWallInnerY = v; }
    void SetWallThickness(G4double v)     { fWallThickness = v; }
    void SetWallHeight(G4double v)        { fWallHeight = v; }
    void SetCeilingThickness(G4double v)  { fCeilingThickness = v; }

    // toggles
    void SetUseFloor(G4bool v)            { fUseFloor = v; }
    void SetUseWalls(G4bool v)            { fUseWalls = v; }
    void SetUseCeiling(G4bool v)          { fUseCeiling = v; }

    // materials
    void SetFloorMaterialName(G4String n)   { fFloorMatName = n; }
    void SetWallMaterialName(G4String n)    { fWallMatName = n; }
    void SetCeilingMaterialName(G4String n) { fCeilingMatName = n; }

  private:
    void BuildMaterials();

    G4AssemblyVolume* fAssembly   = nullptr;
    G4LogicalVolume*  fFloorLog   = nullptr;
    G4LogicalVolume*  fWallLog    = nullptr;
    G4LogicalVolume*  fCeilingLog = nullptr;

    G4double fHallLength;
    G4double fFloorTopZ;
    G4double fFloorThickness;
    G4double fWallInnerY;
    G4double fWallThickness;
    G4double fWallHeight;
    G4double fCeilingThickness;

    G4bool   fUseFloor;
    G4bool   fUseWalls;
    G4bool   fUseCeiling;

    G4String fFloorMatName;
    G4String fWallMatName;
    G4String fCeilingMatName;

    G4Colour fFloorColour;
    G4Colour fWallColour;
    G4Colour fCeilingColour;
};

#endif

