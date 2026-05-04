#ifndef GEOMETRYCLYC_HH
#define GEOMETRYCLYC_HH 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Colour.hh"
#include <vector>

class G4AssemblyVolume;
class G4VPhysicalVolume;
class G4LogicalVolume;

class GeometryCLYC
{
  public:
    GeometryCLYC();
    ~GeometryCLYC();

    G4int Build();
    void PlaceDetector(G4LogicalVolume* expHallLog, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo);
    //void PlaceDetectorByCrystalCenter(G4LogicalVolume* expHallLog, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo);
    G4ThreeVector GetCrystalCenterLocal() const;

    G4LogicalVolume* GetCLYCLog() { return fCLYCCrystalLog; }
    
    // Dimension Setters
    void SetCrystalRadius(G4double radius) { fCLYCCrystalRadius = radius; }
    void SetCrystalLength(G4double length) { fCLYCCrystalLength = length; }
    void SetAlumCasingThickness(G4double thickness) { fAlumCasingThickness = thickness; }
    
    void SetLiFCollimatorInnerRadius(G4double r) { fLiFCollimatorInnerRadius = r; }
    void SetLiFCollimatorOuterRadius(G4double r) { fLiFCollimatorOuterRadius = r; }
    void SetLiFCollimatorLength(G4double l) { fLiFCollimatorLength = l; }
    
    void SetPbCollimatorInnerRadius(G4double r) { fPbCollimatorInnerRadius = r; }
    void SetPbCollimatorOuterRadius(G4double r) { fPbCollimatorOuterRadius = r; }
    void SetPbCollimatorLength(G4double l) { fPbCollimatorLength = l; }
    
    void SetPEHDCollimatorInnerRadius(G4double r) { fPEHDCollimatorInnerRadius = r; }
    void SetPEHDCollimatorOuterRadius(G4double r) { fPEHDCollimatorOuterRadius = r; }
    void SetPEHDCollimatorLength(G4double l) { fPEHDCollimatorLength = l; }
    
    void SetPEPlugInnerRadius(G4double r)   { fPEPlugInnerRadius    = r; }
    void SetPEPlugLipRadius(G4double r)     { fPEPlugLipRadius      = r; }
    void SetPEPlugInnerLength(G4double l)   { fPEPlugInnerLength    = l; }
    void SetPEPlugLipLength(G4double l)     { fPEPlugLipLength      = l; }

    // Material Setters
    void SetCrystalMaterialName(G4String name) { fCLYCCrystalMatName = name; }
    void SetAlumMaterialName(G4String name) { fAlumMatName = name; }
    void SetLiFMaterialName(G4String name) { fLiFMatName = name; }
    void SetPbMaterialName(G4String name) { fPbMatName = name; }
    void SetPEHDMaterialName(G4String name) { fPEHDMatName = name; }

  private:
    void BuildMaterials();

    G4AssemblyVolume* fCLYCAssembly;

    G4LogicalVolume* fCLYCCrystalLog;
    G4LogicalVolume* fAlumCasingLog;
    G4LogicalVolume* fLiFCollimatorLog;
    G4LogicalVolume* fPbCollimatorLog;
    G4LogicalVolume* fPEHDCollimatorLog;
    G4LogicalVolume* fPEPlugLog;

    G4double fCLYCCrystalRadius;
    G4double fCLYCCrystalLength;
    G4double fAlumCasingThickness;
    
    G4double fLiFCollimatorInnerRadius;
    G4double fLiFCollimatorOuterRadius;
    G4double fLiFCollimatorLength;

    G4double fPbCollimatorInnerRadius;
    G4double fPbCollimatorOuterRadius;
    G4double fPbCollimatorLength;
    
    G4double fPEHDCollimatorInnerRadius;
    G4double fPEHDCollimatorOuterRadius;
    G4double fPEHDCollimatorLength;

    G4double fPEPlugInnerRadius;
    G4double fPEPlugLipRadius;
    G4double fPEPlugInnerLength;
    G4double fPEPlugLipLength;

    G4String fCLYCCrystalMatName;
    G4String fAlumMatName;
    G4String fLiFMatName;
    G4String fPbMatName;
    G4String fPEHDMatName;

    G4Colour fCLYCCrystalColour;
    G4Colour fAlumColour;
    G4Colour fLiFColour;
    G4Colour fPbColour;
    G4Colour fPEHDColour;
    G4Colour fPEPlugColour;
};

#endif
