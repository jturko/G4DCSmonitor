#ifndef GEOMETRYPLASTIC_HH
#define GEOMETRYPLASTIC_HH 1

#include "G4VUserDetectorConstruction.hh"
#include "G4Colour.hh"
#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"

#include <vector>

class G4AssemblyVolume;
class G4LogicalVolume;

// Plastic-scintillator detector with a DECOUPLED, standoff "shadow bar",
// a gap "snout" that catches bar back-scatter, and rear/side Pb shielding.
//
// Local frame (same convention as GeometryCLYC):
//   * +z points toward the source/cask.
//   * the collimator FRONT face is at z = 0.
//   * the detector body (collimators, crystal, casing, rear/side shields)
//     lives at z < 0.
//   * the shadow bar + snout live at z > 0 (between detector and cask),
//     separated from the front face by an air gap (fShadowStandoff).
class GeometryPlastic
{
  public:
    GeometryPlastic();
    ~GeometryPlastic();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* world, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo);
    G4ThreeVector GetCrystalCenterLocal() const;
    G4double      GetFrontFaceLocalZ()  const;     // <-- add: front-most surface (+z)

    G4LogicalVolume* GetCrystalLog() { return fCrystalLog; }

    // ---- detector body ----
    void SetCrystalRadius   (G4double v) { fCrystalRadius   = v; }
    void SetCrystalLength   (G4double v) { fCrystalLength   = v; }
    void SetCasingThickness (G4double v) { fCasingThickness = v; }

    void SetPbColInnerRadius(G4double v) { fPbColInnerRadius = v; }
    void SetPbColOuterRadius(G4double v) { fPbColOuterRadius = v; }
    void SetPbColLength     (G4double v) { fPbColLength      = v; }

    void SetPEColInnerRadius(G4double v) { fPEColInnerRadius = v; }
    void SetPEColOuterRadius(G4double v) { fPEColOuterRadius = v; }
    void SetPEColLength     (G4double v) { fPEColLength      = v; }

    void SetLiFColInnerRadius(G4double v){ fLiFColInnerRadius = v; }
    void SetLiFColOuterRadius(G4double v){ fLiFColOuterRadius = v; }
    void SetLiFColLength     (G4double v){ fLiFColLength      = v; }

    // ---- shadow bar ----
    void SetShadowStandoff   (G4double v){ fShadowStandoff    = v; }
    void SetShadowRadiusDet  (G4double v){ fShadowRadiusDet   = v; }
    void SetShadowRadiusSrc  (G4double v){ fShadowRadiusSrc   = v; }
    void SetShadowBackLength (G4double v){ fShadowBackLength  = v; }
    void SetShadowBodyLength (G4double v){ fShadowBodyLength  = v; }
    void SetShadowFrontLength(G4double v){ fShadowFrontLength = v; }

    // ---- snout (gap collimator) ----
    void SetSnoutInnerRadius (G4double v){ fSnoutInnerRadius = v; }
    void SetSnoutOuterRadius (G4double v){ fSnoutOuterRadius = v; }
    void SetSnoutLength      (G4double v){ fSnoutLength      = v; }

    // ---- rear / side gamma shield ----
    void SetBackShieldRadius     (G4double v){ fBackShieldRadius      = v; }
    void SetBackShieldLength     (G4double v){ fBackShieldLength      = v; }
    void SetSideShieldInnerRadius(G4double v){ fSideShieldInnerRadius = v; }
    void SetSideShieldOuterRadius(G4double v){ fSideShieldOuterRadius = v; }
    void SetSideShieldLength     (G4double v){ fSideShieldLength      = v; }

    // ---- materials ----
    void SetCrystalMaterialName    (G4String n){ fCrystalMatName     = n; }
    void SetCasingMaterialName     (G4String n){ fCasingMatName      = n; }
    void SetPbColMaterialName      (G4String n){ fPbColMatName       = n; }
    void SetPEColMaterialName      (G4String n){ fPEColMatName       = n; }
    void SetLiFColMaterialName     (G4String n){ fLiFColMatName      = n; }
    void SetShadowBackMaterialName (G4String n){ fShadowBackMatName  = n; }
    void SetShadowBodyMaterialName (G4String n){ fShadowBodyMatName  = n; }
    void SetShadowFrontMaterialName(G4String n){ fShadowFrontMatName = n; }
    void SetSnoutMaterialName      (G4String n){ fSnoutMatName       = n; }
    void SetBackShieldMaterialName (G4String n){ fBackShieldMatName  = n; }
    void SetSideShieldMaterialName (G4String n){ fSideShieldMatName  = n; }

  private:
    void BuildMaterials();
    void AddShadowSegment(G4double zDetFace, G4double len,
                          G4double rDetEnd, G4double rSrcEnd,
                          const G4String& matName, const G4String& name,
                          const G4Colour& col);

    G4AssemblyVolume* fAssembly = nullptr;
    G4LogicalVolume*  fCrystalLog = nullptr;

    // detector body
    G4double fCrystalRadius, fCrystalLength, fCasingThickness;
    G4double fPbColInnerRadius, fPbColOuterRadius, fPbColLength;
    G4double fPEColInnerRadius, fPEColOuterRadius, fPEColLength;
    G4double fLiFColInnerRadius, fLiFColOuterRadius, fLiFColLength;

    // shadow bar
    G4double fShadowStandoff;
    G4double fShadowRadiusDet, fShadowRadiusSrc;
    G4double fShadowBackLength, fShadowBodyLength, fShadowFrontLength;

    // snout
    G4double fSnoutInnerRadius, fSnoutOuterRadius, fSnoutLength;

    // rear / side shield
    G4double fBackShieldRadius, fBackShieldLength;
    G4double fSideShieldInnerRadius, fSideShieldOuterRadius, fSideShieldLength;

    // materials
    G4String fCrystalMatName, fCasingMatName;
    G4String fPbColMatName, fPEColMatName, fLiFColMatName;
    G4String fShadowBackMatName, fShadowBodyMatName, fShadowFrontMatName;
    G4String fSnoutMatName, fBackShieldMatName, fSideShieldMatName;
};

#endif

