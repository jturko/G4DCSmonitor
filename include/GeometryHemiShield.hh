#ifndef GEOMETRYHEMISHIELD_HH
#define GEOMETRYHEMISHIELD_HH 1

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

class G4AssemblyVolume;
class G4LogicalVolume;
class G4VSolid;
class G4Material;

// Concentric hemispherical shells (material at y >= 0; flat y=0 = opening face):
//     [cavity] | gamma layer (Pb or W) | liner | borated-PE | liner
// A uniform-thickness liner fully encapsulates the PE (inner + outer spherical
// faces AND the flat annular face). A PE slab sits below the opening face to
// moderate on-axis (-y) neutrons. A cylindrical bore (offset +y) is drilled
// through the FRONT (z<0) hemisphere for the bare CLYC (lying on its side).
class GeometryHemiShield
{
  public:
    GeometryHemiShield();
    ~GeometryHemiShield();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo = 0);

    // radial build-up
    void SetCavityRadius(G4double r)    { fCavityRadius   = r; } // inner void / gamma-layer inner radius
    void SetGammaThickness(G4double t)  { fGammaThickness = t; } // Pb or W shell
    void SetLinerThickness(G4double t)  { fLinerThickness = t; } // LiF liner (0 => no liner)
    void SetPEThickness(G4double t)     { fPEThickness    = t; } // borated-PE moderator
    void SetFacePEThickness(G4double t) { fFacePEThick    = t; } // opening-face slab

    // detector bore
    void SetBoreRadius(G4double r)      { fBoreRadius  = r; }
    void SetBoreOffsetY(G4double y)     { fBoreOffsetY = y; }

    // materials / composition
    void SetBoronMassFraction(G4double f) { fBoronFrac = f; }    // 0..1, mass fraction of shell PE
    void SetGammaMaterialName(G4String n) { fGammaMatName = n; } // e.g. "G4_Pb", "G4_W"
    void SetLinerMaterialName(G4String n) { fLinerMatName = n; } // e.g. "LiF", "G4_Cd"
    void SetFacePEMaterialName(G4String n){ fFacePEMatName = n; }

    // accessors
    G4LogicalVolume* GetGammaLog() const { return fGammaLog; }
    G4double GetOuterRadius() const
        { return fCavityRadius + fGammaThickness + 2.*fLinerThickness + fPEThickness; }
    // Local point that should coincide with the CLYC crystal centre (for
    // crystal-centre nesting, mirroring GeometryCLYC::GetCrystalCenterLocal).
    G4ThreeVector GetCrystalAnchorLocal() const
        { return G4ThreeVector(0., fBoreOffsetY, 0.); }

  private:
    void        BuildMaterials();
    G4Material* BuildBoratedPE();                // unique material per boron fraction
    G4VSolid*   MakeCutHemisphere(const G4String& name, G4double rMin, G4double rMax,
                                  G4VSolid* bore, const G4ThreeVector& boreShift) const;

    G4AssemblyVolume* fHemiShieldAssembly = nullptr;

    G4LogicalVolume* fGammaLog     = nullptr;
    G4LogicalVolume* fInnerLinerLog= nullptr;
    G4LogicalVolume* fPELog        = nullptr;
    G4LogicalVolume* fOuterLinerLog= nullptr;
    G4LogicalVolume* fFaceLinerLog = nullptr;
    G4LogicalVolume* fFacePELog    = nullptr;

    // defaults reproduce the original hard-coded design
    G4double fCavityRadius   =  6.0  * CLHEP::cm;
    G4double fGammaThickness =  2.0  * CLHEP::cm;   // 6.0 -> 8.0
    G4double fLinerThickness =  0.5  * CLHEP::cm;
    G4double fPEThickness    = 16.5  * CLHEP::cm;   // 8.5 -> 25.0
    G4double fFacePEThick    =  4.5  * CLHEP::cm;
    G4double fBoreRadius     =  3.0  * CLHEP::cm;
    G4double fBoreOffsetY    =  3.0  * CLHEP::cm;

    G4double fBoronFrac      =  0.05;               // 5% by mass (original)
    G4String fGammaMatName   = "G4_Pb";
    G4String fLinerMatName   = "LiF";
    G4String fFacePEMatName  = "PEHD";

    G4Colour fGammaColour = G4Colour(1.00, 0.00, 0.00, 1.0);
    G4Colour fLinerColour = G4Colour(1.00, 1.00, 0.00, 1.0);
    G4Colour fPEColour    = G4Colour(0.95, 1.00, 0.95, 1.0);
    G4Colour fFaceColour  = G4Colour(0.00, 0.00, 0.80, 1.0);
};

#endif

