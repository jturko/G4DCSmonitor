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
    G4ThreeVector GetCrystalCenterLocal() const;

    G4LogicalVolume* GetCLYCLog() { return fCrystalLog; }
    
    // Dimension Setters
    void SetCrystalRadius(G4double radius) { fCrystalRadius = radius; }
    void SetCrystalLength(G4double length) { fCrystalLength = length; }
    void SetCasingThickness(G4double thickness) { fCasingThickness = thickness; }
    
    void SetLiFCollimatorInnerRadius(G4double r){ fLiFCollimatorInnerRadius = r; }
    void SetLiFCollimatorOuterRadius(G4double r){ fLiFCollimatorOuterRadius = r; }
    void SetLiFCollimatorLength(G4double l)     { fLiFCollimatorLength = l; }
    
    void SetPbCollimatorInnerRadius(G4double r) { fPbCollimatorInnerRadius = r; }
    void SetPbCollimatorOuterRadius(G4double r) { fPbCollimatorOuterRadius = r; }
    void SetPbCollimatorLength(G4double l)      { fPbCollimatorLength = l; }
    
    void SetPECollimatorInnerRadius(G4double r) { fPECollimatorInnerRadius = r; }
    void SetPECollimatorOuterRadius(G4double r) { fPECollimatorOuterRadius = r; }
    void SetPECollimatorLength(G4double l)      { fPECollimatorLength = l; }
    
    void SetPEPlugInnerRadius(G4double r)       { fPEPlugInnerRadius    = r; }
    void SetPEPlugLipRadius(G4double r)         { fPEPlugLipRadius      = r; }
    void SetPEPlugInnerLength(G4double l)       { fPEPlugInnerLength    = l; }
    void SetPEPlugLipLength(G4double l)         { fPEPlugLipLength      = l; }

    // Material Setters
    void SetCrystalMaterialName(G4String name)      { fCrystalMatName = name; }
    void SetCasingMaterialName(G4String name)       { fCasingMatName = name; }
    void SetLiFColMaterialName(G4String name)       { fLiFColMatName = name; }
    void SetPbColMaterialName(G4String name)        { fPbColMatName = name; }
    void SetPEColMaterialName(G4String name)        { fPEColMatName = name; }
    void SetPEPlugMaterialName(G4String name)       { fPEPlugMatName = name; }

  private:
    void BuildMaterials();

    G4AssemblyVolume* fCLYCAssembly;

    G4LogicalVolume* fCrystalLog;
    G4LogicalVolume* fCasingLog;
    G4LogicalVolume* fLiFCollimatorLog;
    G4LogicalVolume* fPbCollimatorLog;
    G4LogicalVolume* fPECollimatorLog;
    G4LogicalVolume* fPEPlugLog;

    G4double fCrystalRadius;
    G4double fCrystalLength;
    G4double fCasingThickness;
    
    G4double fLiFCollimatorInnerRadius;
    G4double fLiFCollimatorOuterRadius;
    G4double fLiFCollimatorLength;

    G4double fPbCollimatorInnerRadius;
    G4double fPbCollimatorOuterRadius;
    G4double fPbCollimatorLength;
    
    G4double fPECollimatorInnerRadius;
    G4double fPECollimatorOuterRadius;
    G4double fPECollimatorLength;

    G4double fPEPlugInnerRadius;
    G4double fPEPlugLipRadius;
    G4double fPEPlugInnerLength;
    G4double fPEPlugLipLength;

    G4String fCrystalMatName;
    G4String fCasingMatName;
    G4String fLiFColMatName;
    G4String fPbColMatName;
    G4String fPEColMatName;
    G4String fPEPlugMatName;

    G4Colour fCrystalColour;
    G4Colour fCasingColour;
    G4Colour fLiFColColour;
    G4Colour fPbColColour;
    G4Colour fPEColColour;
    G4Colour fPEPlugColour;
};

#endif
