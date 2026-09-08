#include "GeometryHemiPanel.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Element.hh"

#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4IntersectionSolid.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

GeometryHemiPanel::GeometryHemiPanel() {}
GeometryHemiPanel::~GeometryHemiPanel() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Regular octagonal right prism, axis along y:
//   octagon lives in the x-z plane (apothem = a), thickness = 2*halfThick in y.
//   Built as (axis-aligned box) INTERSECT (same box rotated 45 deg about y).
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
G4VSolid* GeometryHemiPanel::MakeOctPrism(const G4String& name, G4double a,
                                          G4double hz) const
{
    auto* b1  = new G4Box(name + "_b1", a, hz, a);
    auto* b2  = new G4Box(name + "_b2", a, hz, a);
    auto* rot = new G4RotationMatrix();
    rot->rotateY(45.*deg);
    return new G4IntersectionSolid(name, b1, b2, rot, G4ThreeVector());
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Borated PE (identical recipe/density fit to GeometryHemiShield::BuildBoratedPE)
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
G4Material* GeometryHemiPanel::BuildBoratedPE()
{
    G4NistManager* nist = G4NistManager::Instance();

    if (nist->FindOrBuildMaterial("PEHD", false) == nullptr) {
        auto* elTSH = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE",
                                    1.0, 1.0079*g/mole);
        auto* pehd = new G4Material("PEHD", 0.96*g/cm3, 2);
        pehd->AddElement(elTSH, 2);
        pehd->AddElement(nist->FindOrBuildElement("C"), 1);
    }
    G4Material* pehd = nist->FindOrBuildMaterial("PEHD");

    const G4double f = std::max(0., std::min(0.999, fBoronFrac));
    if (f <= 0.) return nist->FindOrBuildMaterial(fPEMatName);

    std::ostringstream os;
    os << "PEHD_B_" << std::fixed << std::setprecision(1) << 100.*f << "pct";
    const G4String matName = os.str();
    if (auto* m = nist->FindOrBuildMaterial(matName, false)) return m;

    const G4double rho = 0.96 + 1.04*f;              // g/cm3, vendor-data fit
    auto* pehdb = new G4Material(matName, rho*g/cm3, 2);
    pehdb->AddMaterial(pehd, (1.-f));
    pehdb->AddElement(nist->FindOrBuildElement("B"), f);
    return pehdb;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiPanel::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();
    nist->FindOrBuildMaterial("G4_Pb");
    nist->FindOrBuildMaterial("G4_W");
    nist->FindOrBuildMaterial("G4_AIR");
    // PEHD (+ borated variants) are created on demand in BuildBoratedPE().
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryHemiPanel::Build()
{
    BuildMaterials();
    G4NistManager* man = G4NistManager::Instance();

    fHemiPanelAssembly = new G4AssemblyVolume();
    G4ThreeVector     move;                  // must be a NAMED lvalue for AddPlacedVolume()
    G4RotationMatrix* noRot = nullptr;

    G4Material* mPE     = BuildBoratedPE();
    G4Material* mMetal1 = man->FindOrBuildMaterial(fGamma1MatName);
    G4Material* mMetal2 = man->FindOrBuildMaterial(fGamma2MatName);

    const G4double t = fPanelThickness;
    const G4double R = fSphereRadius;

    // concentric rectangular "radii" (half-extents in x,z) of the metal build-up
    const G4double r0 = fCavityRadius;                     // inner void
    const G4double r1 = r0 + fGamma1Thickness;             // after inner metal
    const G4double r2 = r1 + fGamma2Thickness;             // after outer metal

    // rectangular pocket carved out of the PE for the metal to slot into
    const G4double Renv = r2 + fMetalClearance;            // half-size in x,z
    const G4double Henv = r2 + fMetalClearance;            // top height in y

    // bore: cylinder along z at (0, boreOffsetY), covering z<=0 (FRONT only)
    const G4double boreHalfLen = R + fNPanels * t + 10.*mm;
    auto* bore   = new G4Tubs("HP_bore", 0., fBoreRadius, boreHalfLen, 0., 360.*deg);
    auto* envBox = new G4Box ("HP_env",  Renv, Henv/2., Renv);   // centered at y=Henv/2

    // ---------------- PE octagonal panels (staircase dome) ----------------
    fPanelLogs.clear();
    for (G4int i = 0; i < fNPanels; ++i) {
        const G4double yBase = i * (t + fPanelGap);
        const G4double Cy    = yBase + t/2.0;

        const G4double sphereA = (yBase < R) ? std::sqrt(R*R - yBase*yBase) : 0.;
        G4double a;
        if (yBase < Henv)                          // panel overlaps the metal
            a = std::max(sphereA, r2 + fMetalClearance + fPanelMinWall);
        else                                       // clean octagon dome cap
            a = std::max(sphereA, 2.*fPanelMinWall);

        std::ostringstream nm; nm << "HP_panel" << i;

        G4VSolid* oct = MakeOctPrism(nm.str(), a, t/2.0);

        // carve the rectangular metal pocket (only bites where metal exists)
        G4VSolid* s = new G4SubtractionSolid(nm.str() + "_env", oct, envBox, 0,
                          G4ThreeVector(0., Henv/2.0 - Cy, 0.));
        // drill the shared detector bore
        s = new G4SubtractionSolid(nm.str() + "_bore", s, bore, 0,
                          G4ThreeVector(0., fBoreOffsetY - Cy, -boreHalfLen));

        auto* lv = new G4LogicalVolume(s, mPE, nm.str() + "_log");
        lv->SetVisAttributes(new G4VisAttributes(true, fPEColour));
        fPanelLogs.push_back(lv);

        move = G4ThreeVector(0., Cy, 0.);
        fHemiPanelAssembly->AddPlacedVolume(lv, move, noRot);
    }

    // ---------------- two nested rectangular metal box-shells ----------------
    // Each box_i is centered at its own origin and represents y in [0, r_i]
    // once placed at (0, r_i/2, 0).
    auto* cav  = new G4Box("HP_cav",  r0, r0/2., r0);
    auto* box1 = new G4Box("HP_box1", r1, r1/2., r1);
    auto* box2 = new G4Box("HP_box2", r2, r2/2., r2);

    // inner metal layer = box1 - cavity - bore  (uniform shell of thickness g1)
    G4VSolid* m1 = new G4SubtractionSolid("HP_m1a", box1, cav, 0,
                        G4ThreeVector(0., r0/2. - r1/2., 0.));
    m1 = new G4SubtractionSolid("HP_m1", m1, bore, 0,
                        G4ThreeVector(0., fBoreOffsetY - r1/2., -boreHalfLen));
    fMetal1Log = new G4LogicalVolume(m1, mMetal1, "HP_metal1_log");
    fMetal1Log->SetVisAttributes(new G4VisAttributes(true, fMetal1Colour));
    move = G4ThreeVector(0., r1/2., 0.);
    fHemiPanelAssembly->AddPlacedVolume(fMetal1Log, move, noRot);

    // outer metal layer = box2 - box1 - bore  (uniform shell of thickness g2)
    G4VSolid* m2 = new G4SubtractionSolid("HP_m2a", box2, box1, 0,
                        G4ThreeVector(0., r1/2. - r2/2., 0.));
    m2 = new G4SubtractionSolid("HP_m2", m2, bore, 0,
                        G4ThreeVector(0., fBoreOffsetY - r2/2., -boreHalfLen));
    fMetal2Log = new G4LogicalVolume(m2, mMetal2, "HP_metal2_log");
    fMetal2Log->SetVisAttributes(new G4VisAttributes(true, fMetal2Colour));
    move = G4ThreeVector(0., r2/2., 0.);
    fHemiPanelAssembly->AddPlacedVolume(fMetal2Log, move, noRot);

    G4cout << " -> GeometryHemiPanel: " << fNPanels << " octagonal PE panels ("
           << t/mm << " mm each, gap " << fPanelGap/mm << " mm), sphere r="
           << R/mm << " mm; metal shells: " << fGamma1MatName << " ("
           << fGamma1Thickness/mm << " mm) + " << fGamma2MatName << " ("
           << fGamma2Thickness/mm << " mm); cavity half=" << r0/mm
           << " mm; PE boron=" << 100.*fBoronFrac << " wt%." << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiPanel::PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                                      G4RotationMatrix* rotate, G4int copyNo)
{
    fHemiPanelAssembly->MakeImprint(worldLog, move, rotate, copyNo, /*surfCheck*/true);
}

