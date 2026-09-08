#ifndef GEOMETRYCAD_HH
#define GEOMETRYCAD_HH 1

#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "globals.hh"

class G4LogicalVolume;

// Imports ONE tessellated logical volume from a FreeCAD-exported GDML file and
// places it into a mother volume. The GDML contains its own worldVOL wrapper;
// we pull out ONLY the named part LV and re-place it into the 22 m fLWorld.
class GeometryCAD
{
  public:
    GeometryCAD();
    ~GeometryCAD();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo = 0);

    void SetFileName(const G4String& f)   { fFileName   = f; }
    const G4String& GetFileName() const { return fFileName; }

    void SetVolumeName(const G4String& v) { fVolumeName = v; }

    G4LogicalVolume* GetCADLog() const { return fCADLog; }

  private:
    G4String         fFileName   = "test_part.gdml";
    G4String         fVolumeName = "test_part";
    G4LogicalVolume* fCADLog     = nullptr;
};
#endif

