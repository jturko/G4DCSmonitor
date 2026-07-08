#include "GeometryHemiShield.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Isotope.hh"
#include "G4Element.hh"

#include "G4Tubs.hh"
#include "G4Sphere.hh"
#include "G4SubtractionSolid.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

GeometryHemiShield::GeometryHemiShield() {}
GeometryHemiShield::~GeometryHemiShield() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VSolid* GeometryHemiShield::MakeCutHemisphere(const G4String& name,
                                                G4double rMin, G4double rMax,
                                                G4VSolid* bore,
                                                const G4ThreeVector& boreShift) const
{
    // Hemisphere: full polar sweep, azimuth 0->180 deg => material at y >= 0.
    auto* shell = new G4Sphere(name + "_precut", rMin, rMax,
                               0.*deg, 180.*deg,   // phi
                               0.*deg, 180.*deg);  // theta
    return new G4SubtractionSolid(name, shell, bore, nullptr, boreShift);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryHemiShield::Build()
{
    BuildMaterials();
    G4NistManager* man = G4NistManager::Instance();

    fHemiShieldAssembly = new G4AssemblyVolume();

    // AddPlacedVolume() takes the translation by non-const reference, so it must
    // be a named lvalue (never a temporary). Reuse 'move' for every call.
    G4ThreeVector     move;
    G4RotationMatrix* noRot = nullptr;

    // ------------------------------------------------------------------
    // Radial build-up. The LiF liner (thickness fLiFThickness) wraps the PE on
    // both its inner and outer spherical faces => it fully encapsulates the PE.
    // ------------------------------------------------------------------
    const G4double rPbI     = fCavityRadius;                 // 6.0
    const G4double rPbO     = rPbI    + fPbThickness;        // 8.0
    const G4double rInLiFO  = rPbO    + fLiFThickness;       // 8.5  (= PE inner)
    const G4double rPEO     = rInLiFO + fPEThickness;        // 25.0 (= PE outer)
    const G4double rOutLiFO = rPEO    + fLiFThickness;       // 25.5 (overall outer)
    const G4double rTotal   = rOutLiFO;

    // ------------------------------------------------------------------
    // Detector bore: cylinder along +z, offset +y by fBoreOffsetY, drilling
    // ONLY the front (z < 0) hemisphere. Half-length is derived to just clear
    // the shell, positioned so its front face lands on the z = 0 plane.
    // ------------------------------------------------------------------
    const G4double boreHalfLen = rTotal + 1.*cm;
    auto* bore = new G4Tubs("HemiBore", 0., fBoreRadius, boreHalfLen, 0.*deg, 360.*deg);
    const G4ThreeVector boreShift(0., fBoreOffsetY, -boreHalfLen);

    G4Material* mPb      = man->FindOrBuildMaterial(fPbMatName);
    G4Material* mLiF     = man->FindOrBuildMaterial(fLiFMatName);
    G4Material* mShellPE = man->FindOrBuildMaterial(fShellPEMatName);
    G4Material* mFacePE  = man->FindOrBuildMaterial(fFacePEMatName);

    // ---------------- concentric hemispherical shells ----------------
    // 1) Pb gamma shield
    fPbLog = new G4LogicalVolume(
        MakeCutHemisphere("pPb", rPbI, rPbO, bore, boreShift), mPb, "lPb");
    fPbLog->SetVisAttributes(new G4VisAttributes(true, fPbColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fPbLog, move, noRot);

    // 2) inner LiF liner (thermal-neutron absorber, caps PE inner face)
    fInnerLiFLog = new G4LogicalVolume(
        MakeCutHemisphere("pLiF", rPbO, rInLiFO, bore, boreShift), mLiF, "lLiF");
    fInnerLiFLog->SetVisAttributes(new G4VisAttributes(true, fLiFColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fInnerLiFLog, move, noRot);

    // 3) borated-PE moderator (bulk of the shield)
    fPELog = new G4LogicalVolume(
        MakeCutHemisphere("pPE", rInLiFO, rPEO, bore, boreShift), mShellPE, "lPE");
    fPELog->SetVisAttributes(new G4VisAttributes(true, fPEColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fPELog, move, noRot);

    // 4) outer LiF liner (caps PE outer face)
    fOuterLiFLog = new G4LogicalVolume(
        MakeCutHemisphere("pLiF2", rPEO, rOutLiFO, bore, boreShift), mLiF, "lLiF2");
    fOuterLiFLog->SetVisAttributes(new G4VisAttributes(true, fLiFColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fOuterLiFLog, move, noRot);

    // ---------------- opening-face stack (below the y = 0 plane) ----------------
    // Both disks are rotated so their thickness is measured along y.
    //
    // (a) Flat-face LiF disk: the third part of the uniform LiF encapsulation.
    //     Radially it spans the PE annulus plus its liner on both edges
    //     (rPbO -> rTotal), and it is one liner-thickness thick, sitting flush
    //     under the opening face: y in [-fLiFThickness, 0].
    {
        auto* s = new G4Tubs("pFaceLiF", rPbO, rTotal, fLiFThickness/2.,
                             0.*deg, 360.*deg);
        fFaceLiFLog = new G4LogicalVolume(s, mLiF, "lFaceLiF");
        fFaceLiFLog->SetVisAttributes(new G4VisAttributes(true, fLiFColour));

        auto* rot = new G4RotationMatrix();
        rot->rotateX(90.*deg);
        move = G4ThreeVector(0., -fLiFThickness/2., 0.);
        fHemiShieldAssembly->AddPlacedVolume(fFaceLiFLog, move, rot);
    }

    // (b) Full PE moderator slab: solid disk (r 0 -> rTotal) stacked directly
    //     below the LiF disk, for moderating on-axis (-y) neutrons.
    //     y in [-(fFacePEThick + fLiFThickness), -fLiFThickness].
    {
        auto* s = new G4Tubs("pFacePE", 0., rTotal, fFacePEThick/2.,
                             0.*deg, 360.*deg);
        fFacePELog = new G4LogicalVolume(s, mFacePE, "lFacePE");
        fFacePELog->SetVisAttributes(new G4VisAttributes(true, fFacePEColour));

        auto* rot = new G4RotationMatrix();
        rot->rotateX(90.*deg);
        move = G4ThreeVector(0., -(fFacePEThick/2. + fLiFThickness), 0.);
        fHemiShieldAssembly->AddPlacedVolume(fFacePELog, move, rot);
    }

    G4cout << " -> GeometryHemiShield built: cavity r=" << rPbI/cm
           << " cm, outer r=" << rTotal/cm << " cm; "
           << "bore Ø" << 2*fBoreRadius/cm << " cm @ y=+" << fBoreOffsetY/cm << " cm; "
           << "face PE slab " << fFacePEThick/cm << " cm thick." << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiShield::PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                                       G4RotationMatrix* rotate, G4int copyNo)
{
    fHemiShieldAssembly->MakeImprint(worldLog, move, rotate, copyNo, /*surfCheck*/true);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiShield::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();

    nist->FindOrBuildMaterial("G4_Pb");
    nist->FindOrBuildMaterial("G4_POLYETHYLENE");
    nist->FindOrBuildMaterial("G4_AIR");

    // Enriched-Li LiF (95% Li-6) thermal-neutron absorber. Guarded so it is a
    // no-op if GeometryCLYC (or a previous shield) already defined it.
    if (nist->FindOrBuildMaterial("LiF", false) == nullptr) {
        auto* Li6 = new G4Isotope("Li6", 3, 6, 6.015*g/mole);
        auto* Li7 = new G4Isotope("Li7", 3, 7, 7.016*g/mole);
        auto* elLi = new G4Element("Enriched Lithium", "Li", 2);
        elLi->AddIsotope(Li6, 95.*perCent);
        elLi->AddIsotope(Li7,  5.*perCent);

        auto* lif = new G4Material("LiF", 2.635*g/cm3, 2);
        lif->AddElement(elLi, 1);
        lif->AddElement(nist->FindOrBuildElement("F"), 1);
    }

    // Polyethylene with a thermal-scattering (S(alpha,beta)) hydrogen hook.
    if (nist->FindOrBuildMaterial("PEHD", false) == nullptr) {
        auto* elTSH = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE",
                                    1.0, 1.0079*g/mole);
        auto* pehd = new G4Material("PEHD", 0.96*g/cm3, 2);
        pehd->AddElement(elTSH, 2);
        pehd->AddElement(nist->FindOrBuildElement("C"), 1);
    }

    // Borated polyethylene (shield bulk).
    if (nist->FindOrBuildMaterial("PEHD_borated", false) == nullptr) {
        auto* pehdb = new G4Material("PEHD_borated", 1.01*g/cm3, 2);
        pehdb->AddMaterial(nist->FindOrBuildMaterial("PEHD"), 95.*perCent);
        pehdb->AddElement(nist->FindOrBuildElement("B"), 5.*perCent);
    }
}

