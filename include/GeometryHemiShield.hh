#ifndef GEOMETRYHEMISHIELD_HH
#define GEOMETRYHEMISHIELD_HH 1

#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"

class G4AssemblyVolume;
class G4LogicalVolume;
class G4VSolid;

// ---------------------------------------------------------------------------
// GeometryHemiShield
//
// A CLYC shielding design built from concentric HEMISPHERICAL shells
// (material fills the y >= 0 half-space; the flat y = 0 plane is the OPENING
// FACE that looks toward the source). From the inside out the shells are:
//
//     [cavity] | Pb | LiF | borated-PE | LiF
//
// The LiF is a uniform-thickness liner that fully encapsulates the PE: it wraps
// the inner and outer spherical faces (the two hemispherical liners) AND the
// flat annular face at y = 0 (a thin LiF disk). A large PE slab is then stacked
// just below the opening face to moderate on-axis (-y) neutrons.
//
// A cylindrical bore is drilled through the FRONT (z < 0) side of every shell to
// admit the bare CLYC detector (no collimators/plug). The CLYC is inserted lying
// on its side (its cylinder axis along z), so the crystal's curved wall faces the
// opening; the bore is therefore offset +y by fBoreOffsetY to line the crystal up
// with the opening face.
//
// All defaults reproduce the original hard-coded design exactly.
// ---------------------------------------------------------------------------
class GeometryHemiShield
{
  public:
    GeometryHemiShield();
    ~GeometryHemiShield();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo = 0);

    // --- radial build-up of the concentric shells ---
    void SetCavityRadius(G4double r) { fCavityRadius = r; }  // inner void (Pb inner radius)
    void SetPbThickness(G4double t)  { fPbThickness  = t; }
    void SetLiFThickness(G4double t) { fLiFThickness = t; }  // uniform liner (radial + face)
    void SetPEThickness(G4double t)  { fPEThickness  = t; }  // borated-PE moderator shell

    // --- detector bore (drilled through the front, z < 0, hemisphere) ---
    void SetBoreRadius(G4double r)   { fBoreRadius  = r; }
    void SetBoreOffsetY(G4double y)  { fBoreOffsetY = y; }

    // --- opening-face moderator slab (below the y = 0 plane) ---
    void SetFacePEThickness(G4double t) { fFacePEThick = t; }

    // --- materials ---
    void SetPbMaterialName(G4String n)      { fPbMatName      = n; }
    void SetLiFMaterialName(G4String n)     { fLiFMatName     = n; }
    void SetShellPEMaterialName(G4String n) { fShellPEMatName = n; }  // borated shell
    void SetFacePEMaterialName(G4String n)  { fFacePEMatName  = n; }  // moderator slab

    // --- accessors ---
    G4LogicalVolume* GetPbLog() const { return fPbLog; }
    G4double GetOuterRadius() const
        { return fCavityRadius + fPbThickness + 2.*fLiFThickness + fPEThickness; }

    G4ThreeVector GetCrystalAnchorLocal() const
        { return G4ThreeVector(0., fBoreOffsetY, 0.); }

  private:
    void BuildMaterials();

    // One hemispherical shell [rMin,rMax] with the detector bore subtracted.
    G4VSolid* MakeCutHemisphere(const G4String& name,
                                G4double rMin, G4double rMax,
                                G4VSolid* bore,
                                const G4ThreeVector& boreShift) const;

    G4AssemblyVolume* fHemiShieldAssembly = nullptr;

    // logical volumes (kept for possible SD hookup / debugging)
    G4LogicalVolume* fPbLog       = nullptr;
    G4LogicalVolume* fInnerLiFLog = nullptr;
    G4LogicalVolume* fPELog       = nullptr;
    G4LogicalVolume* fOuterLiFLog = nullptr;
    G4LogicalVolume* fFaceLiFLog  = nullptr;
    G4LogicalVolume* fFacePELog   = nullptr;

    // geometry parameters (defaults == original hard-coded values)
    G4double fCavityRadius =  6.0  * CLHEP::cm;   // Pb inner radius
    G4double fPbThickness  =  2.0  * CLHEP::cm;   // 6.0  -> 8.0  cm
    G4double fLiFThickness =  0.5  * CLHEP::cm;   // uniform LiF liner
    G4double fPEThickness  = 16.5  * CLHEP::cm;   // 8.5  -> 25.0 cm

    G4double fBoreRadius   =  3.0  * CLHEP::cm;
    G4double fBoreOffsetY  =  3.0  * CLHEP::cm;

    G4double fFacePEThick  =  4.5  * CLHEP::cm;

    // materials
    G4String fPbMatName      = "G4_Pb";
    G4String fLiFMatName     = "LiF";
    G4String fShellPEMatName = "PEHD_borated";
    G4String fFacePEMatName  = "PEHD";

    // colours (unchanged from original)
    G4Colour fPbColour     = G4Colour(1.00, 0.00, 0.00, 1.0);
    G4Colour fLiFColour    = G4Colour(1.00, 1.00, 0.00, 1.0);
    G4Colour fPEColour     = G4Colour(0.95, 1.00, 0.95, 1.0);
    G4Colour fFacePEColour = G4Colour(0.00, 0.00, 0.80, 1.0);
};

#endif

