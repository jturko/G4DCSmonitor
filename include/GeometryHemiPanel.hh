#ifndef GEOMETRYHEMIPANEL_HH
#define GEOMETRYHEMIPANEL_HH 1

#include "G4RotationMatrix.hh"
#include "G4ThreeVector.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"
#include <vector>

class G4AssemblyVolume;
class G4LogicalVolume;
class G4VSolid;
class G4Material;

// Manufacturable stand-in for GeometryHemiShield.
//
// PE dome  : fNPanels stacked REGULAR OCTAGONAL PRISMS (each fPanelThickness
//            thick, adjustable fPanelGap between them for gluing). Each panel's
//            octagon circumscribes the approximated sphere (r=fSphereRadius) at
//            its base, so every panel is larger than the sphere it models.
// Gamma    : TWO nested RECTANGULAR (4-sided) metal box-shells that insert into
//            a rectangular pocket subtracted from the PE panels.
// Frame    : identical to GeometryHemiShield -> material at y>=0, opening face
//            at y=0, CLYC bore along local z (front z<0) at y=fBoreOffsetY,
//            GetCrystalAnchorLocal() = (0, fBoreOffsetY, 0).
class GeometryHemiPanel
{
  public:
    GeometryHemiPanel();
    ~GeometryHemiPanel();

    G4int Build();
    void  PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                        G4RotationMatrix* rotate, G4int copyNo = 0);

    // ---- PE octagonal-panel dome ----
    void SetSphereRadius(G4double r)      { fSphereRadius   = r; } // approximated sphere
    void SetNumPanels(G4int n)            { fNPanels        = n; }
    void SetPanelThickness(G4double t)    { fPanelThickness = t; }
    void SetPanelGap(G4double g)          { fPanelGap       = g; } // glue gap (adjustable)
    void SetPanelMinWall(G4double w)      { fPanelMinWall   = w; } // min PE around metal

    // ---- metal gamma shield (two rectangular layers) ----
    void SetCavityRadius(G4double r)      { fCavityRadius    = r; } // half-size of inner void box
    void SetGamma1Thickness(G4double t)   { fGamma1Thickness = t; } // inner metal layer
    void SetGamma2Thickness(G4double t)   { fGamma2Thickness = t; } // outer metal layer
    void SetMetalClearance(G4double c)    { fMetalClearance  = c; } // PE<->metal glue gap
    void SetGamma1MaterialName(G4String n){ fGamma1MatName   = n; }
    void SetGamma2MaterialName(G4String n){ fGamma2MatName   = n; }

    // ---- detector bore ----
    void SetBoreRadius(G4double r)        { fBoreRadius  = r; }
    void SetBoreOffsetY(G4double y)       { fBoreOffsetY = y; }

    // ---- PE composition ----
    void SetBoronMassFraction(G4double f) { fBoronFrac  = f; } // 0..1 (0 => pure PE)
    void SetPEMaterialName(G4String n)    { fPEMatName  = n; } // used when boron==0

    // ---- accessors (mirror GeometryHemiShield) ----
    G4double GetOuterRadius() const { return fSphereRadius; }
    G4ThreeVector GetCrystalAnchorLocal() const
        { return G4ThreeVector(0., fBoreOffsetY, 0.); }

  private:
    void        BuildMaterials();
    G4Material* BuildBoratedPE();
    G4VSolid*   MakeOctPrism(const G4String& name, G4double apothem,
                             G4double halfThick) const;

    G4AssemblyVolume* fHemiPanelAssembly = nullptr;

    std::vector<G4LogicalVolume*> fPanelLogs;
    G4LogicalVolume* fMetal1Log = nullptr;
    G4LogicalVolume* fMetal2Log = nullptr;

    // defaults chosen to sit around the bare CLYC like the hemishield does
    G4double fSphereRadius    = 270. * CLHEP::mm;
    G4int    fNPanels         = 6;
    G4double fPanelThickness  = 50.  * CLHEP::mm;
    G4double fPanelGap        = 0.   * CLHEP::mm;
    G4double fPanelMinWall    = 10.  * CLHEP::mm;

    G4double fCavityRadius    = 85.  * CLHEP::mm;
    G4double fGamma1Thickness = 20.  * CLHEP::mm;
    G4double fGamma2Thickness = 20.  * CLHEP::mm;
    G4double fMetalClearance  = 0.   * CLHEP::mm;

    G4double fBoreRadius      = 30.  * CLHEP::mm;
    G4double fBoreOffsetY     = 50.  * CLHEP::mm;

    G4double fBoronFrac       = 0.05;
    G4String fPEMatName       = "PEHD";
    G4String fGamma1MatName   = "G4_Pb";
    G4String fGamma2MatName   = "G4_Pb";

    G4Colour fPEColour     = G4Colour(0.0, 1.0, 1.0, 0.35);
    G4Colour fMetal1Colour = G4Colour(0.6, 0.4, 0.2, 1.0);
    G4Colour fMetal2Colour = G4Colour(0.4, 0.4, 0.5, 1.0);
};

#endif

