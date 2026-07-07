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

    // crystal / casing (unchanged API)
    void SetCrystalRadius(G4double r) { fCrystalRadius = r; }
    void SetCrystalLength(G4double l) { fCrystalLength = l; }
    void SetCasingThickness(G4double t) { fCasingThickness = t; }

    // optional front collimators / plug (unchanged API)
    void SetLiFCollimatorInnerRadius(G4double r){ fLiFCollimatorInnerRadius = r; }
    void SetLiFCollimatorOuterRadius(G4double r){ fLiFCollimatorOuterRadius = r; }
    void SetLiFCollimatorLength(G4double l)     { fLiFCollimatorLength = l; }
    void SetPbCollimatorInnerRadius(G4double r) { fPbCollimatorInnerRadius = r; }
    void SetPbCollimatorOuterRadius(G4double r) { fPbCollimatorOuterRadius = r; }
    void SetPbCollimatorLength(G4double l)      { fPbCollimatorLength = l; }
    void SetPECollimatorInnerRadius(G4double r) { fPECollimatorInnerRadius = r; }
    void SetPECollimatorOuterRadius(G4double r) { fPECollimatorOuterRadius = r; }
    void SetPECollimatorLength(G4double l)      { fPECollimatorLength = l; }
    void SetPEPlugInnerRadius(G4double r)       { fPEPlugInnerRadius = r; }
    void SetPEPlugLipRadius(G4double r)         { fPEPlugLipRadius   = r; }
    void SetPEPlugInnerLength(G4double l)       { fPEPlugInnerLength = l; }
    void SetPEPlugLipLength(G4double l)         { fPEPlugLipLength   = l; }

    // materials (unchanged API)
    void SetCrystalMaterialName(G4String n)      { fCrystalMatName = n; }
    void SetCasingMaterialName(G4String n)       { fCasingMatName = n; }
    void SetLiFColMaterialName(G4String n)       { fLiFColMatName = n; }
    void SetPbColMaterialName(G4String n)        { fPbColMatName = n; }
    void SetPEColMaterialName(G4String n)        { fPEColMatName = n; }
    void SetPEPlugMaterialName(G4String n)       { fPEPlugMatName = n; }

    // NEW fixed real-sensor package (drawing VS-0889-305)
    void SetSnoutOuterRadius(G4double r)         { fSnoutOuterRadius = r; }      // Ø29 -> 14.5
    void SetSnoutLength(G4double l)              { fSnoutLength = l; }           // "15"
    void SetReflectorThickness(G4double v)       { fReflectorThickness = v; }
    void SetOpticalInterfaceThickness(G4double v){ fOpticalInterfaceThickness = v; }
    void SetMagShieldThickness(G4double v)       { fMagShieldThickness = v; }    // 0.64
    void SetMagShieldOuterRadius(G4double v)     { fMagShieldOuterRadius = v; }  // Ø58.8 -> 29.4
    void SetTotalLength(G4double v)              { fTotalLength = v; }           // 125
    void SetPMTRadius(G4double v)                { fPMTRadius = v; }             // Ø51 -> 25.5
    void SetPMTGlassThickness(G4double v)        { fPMTGlassThickness = v; }
    void SetBaseLength(G4double v)               { fBaseLength = v; }

  private:
    void BuildMaterials();

    G4AssemblyVolume* fCLYCAssembly;

    G4LogicalVolume* fCrystalLog;
    G4LogicalVolume* fCasingLog;            // stepped Al body (snout+shoulder+wide)
    G4LogicalVolume* fReflectorLog;
    G4LogicalVolume* fOpticalInterfaceLog;
    G4LogicalVolume* fMagShieldLog;
    G4LogicalVolume* fPMTWindowLog;
    G4LogicalVolume* fPMTGlassLog;
    G4LogicalVolume* fPMTVacuumLog;
    G4LogicalVolume* fPMTBackLog;
    G4LogicalVolume* fBaseLog;
    G4LogicalVolume* fLiFCollimatorLog;
    G4LogicalVolume* fPbCollimatorLog;
    G4LogicalVolume* fPECollimatorLog;
    G4LogicalVolume* fPEPlugLog;

    G4double fCrystalRadius, fCrystalLength, fCasingThickness;

    G4double fLiFCollimatorInnerRadius, fLiFCollimatorOuterRadius, fLiFCollimatorLength;
    G4double fPbCollimatorInnerRadius,  fPbCollimatorOuterRadius,  fPbCollimatorLength;
    G4double fPECollimatorInnerRadius,  fPECollimatorOuterRadius,  fPECollimatorLength;
    G4double fPEPlugInnerRadius, fPEPlugLipRadius, fPEPlugInnerLength, fPEPlugLipLength;

    G4double fSnoutOuterRadius, fSnoutLength;
    G4double fReflectorThickness, fOpticalInterfaceThickness;
    G4double fMagShieldThickness, fMagShieldOuterRadius, fTotalLength;
    G4double fPMTRadius, fPMTGlassThickness, fBaseLength;

    G4String fCrystalMatName, fCasingMatName, fLiFColMatName, fPbColMatName,
             fPEColMatName, fPEPlugMatName;
    G4String fReflectorMatName, fOpticalInterfaceMatName, fMagShieldMatName,
             fPMTGlassMatName, fPMTVacuumMatName, fBaseMatName;

    G4Colour fCrystalColour, fCasingColour, fLiFColColour, fPbColColour,
             fPEColColour, fPEPlugColour, fReflectorColour, fOpticalColour,
             fMagShieldColour, fPMTColour, fVacuumColour, fBaseColour;
};

#endif

