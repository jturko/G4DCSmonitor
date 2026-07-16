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
#include "G4Transform3D.hh"

#include <sstream>
#include <iomanip>

GeometryHemiShield::GeometryHemiShield() {}
GeometryHemiShield::~GeometryHemiShield() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VSolid* GeometryHemiShield::MakeCutHemisphere(const G4String& name,
                                                G4double rMin, G4double rMax,
                                                G4VSolid* bore,
                                                const G4ThreeVector& boreShift) const
{
    auto* shell = new G4Sphere(name + "_precut", rMin, rMax,
                               0.*deg, 180.*deg,    // phi  -> material at y >= 0
                               0.*deg, 180.*deg);   // theta
    return new G4SubtractionSolid(name, shell, bore, nullptr, boreShift);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4Material* GeometryHemiShield::BuildBoratedPE()
{
    G4NistManager* nist = G4NistManager::Instance();

    // Base PEHD with a thermal-scattering hydrogen hook (shared, guarded).
    if (nist->FindOrBuildMaterial("PEHD", false) == nullptr) {
        auto* elTSH = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE",
                                    1.0, 1.0079*g/mole);
        auto* pehd = new G4Material("PEHD", 0.96*g/cm3, 2);
        pehd->AddElement(elTSH, 2);
        pehd->AddElement(nist->FindOrBuildElement("C"), 1);
    }
    G4Material* pehd = nist->FindOrBuildMaterial("PEHD");


    const G4double f = std::max(0., std::min(0.999, fBoronFrac));
    if (f <= 0.) return pehd;   // pure PE, no boron

    std::ostringstream os;
    os << "PEHD_B_" << std::fixed << std::setprecision(1) << 100.*f << "pct";
    const G4String matName = os.str();
    if (auto* m = nist->FindOrBuildMaterial(matName, false)) return m;

    // Empirical density of commercial borated PE (B4C-filled), fit to vendor
    // data: 0.96 (0%), 1.04 (5%), 1.06 (10%), 1.28 (30%). ~1% over 0-30 wt%.
    const G4double rho = 0.96 + 1.04*f;              // g/cm3
    auto* pehdb = new G4Material(matName, rho*g/cm3, 2);
    pehdb->AddMaterial(pehd, (1.-f));
    pehdb->AddElement(nist->FindOrBuildElement("B"), f);
    return pehdb;

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiShield::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();

    nist->FindOrBuildMaterial("G4_Pb");
    nist->FindOrBuildMaterial("G4_W");        // tungsten (for the gamma-layer toggle)
    nist->FindOrBuildMaterial("G4_Cd");       // cadmium  (alternative liner)
    nist->FindOrBuildMaterial("G4_AIR");

    // Enriched-Li LiF (gamma-free thermal absorber), guarded.
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
    // PEHD (+ borated variants) are created on demand in BuildBoratedPE().
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryHemiShield::Build()
{
    BuildMaterials();
    G4NistManager* man = G4NistManager::Instance();

    fHemiShieldAssembly = new G4AssemblyVolume();
    G4ThreeVector     move;                    // must be a named lvalue for AddPlacedVolume()
    G4RotationMatrix* noRot = nullptr;

    const G4double rCav      = fCavityRadius;
    const G4double rGammaO   = rCav     + fGammaThickness;
    const G4double rInLinerO = rGammaO  + fLinerThickness;   // = PE inner
    const G4double rPEO      = rInLinerO+ fPEThickness;
    const G4double rOutLinerO= rPEO     + fLinerThickness;
    const G4double rTotal    = rOutLinerO;
    const bool     haveLiner = (fLinerThickness > 0.);

    const G4double boreHalfLen = rTotal + 1.*cm;
    auto* bore = new G4Tubs("HemiBore", 0., fBoreRadius, boreHalfLen, 0.*deg, 360.*deg);
    const G4ThreeVector boreShift(0., fBoreOffsetY, -boreHalfLen);

    G4Material* mGamma  = man->FindOrBuildMaterial(fGammaMatName);
    G4Material* mLiner  = man->FindOrBuildMaterial(fLinerMatName);
    G4Material* mPE     = BuildBoratedPE();
    G4Material* mFacePE = man->FindOrBuildMaterial(fFacePEMatName);

    // 1) gamma shield (Pb or W)
    fGammaLog = new G4LogicalVolume(
        MakeCutHemisphere("pGamma", rCav, rGammaO, bore, boreShift), mGamma, "lGamma");
    fGammaLog->SetVisAttributes(new G4VisAttributes(true, fGammaColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fGammaLog, move, noRot);

    // 2) inner liner
    if (haveLiner) {
        fInnerLinerLog = new G4LogicalVolume(
            MakeCutHemisphere("pLiner1", rGammaO, rInLinerO, bore, boreShift), mLiner, "lLiner1");
        fInnerLinerLog->SetVisAttributes(new G4VisAttributes(true, fLinerColour));
        move = G4ThreeVector();
        fHemiShieldAssembly->AddPlacedVolume(fInnerLinerLog, move, noRot);
    }

    // 3) borated-PE moderator
    fPELog = new G4LogicalVolume(
        MakeCutHemisphere("pPE", rInLinerO, rPEO, bore, boreShift), mPE, "lPE");
    fPELog->SetVisAttributes(new G4VisAttributes(true, fPEColour));
    move = G4ThreeVector();
    fHemiShieldAssembly->AddPlacedVolume(fPELog, move, noRot);

    // 4) outer liner
    if (haveLiner) {
        fOuterLinerLog = new G4LogicalVolume(
            MakeCutHemisphere("pLiner2", rPEO, rOutLinerO, bore, boreShift), mLiner, "lLiner2");
        fOuterLinerLog->SetVisAttributes(new G4VisAttributes(true, fLinerColour));
        move = G4ThreeVector();
        fHemiShieldAssembly->AddPlacedVolume(fOuterLinerLog, move, noRot);
    }

    // 5) flat-face liner disk (third face of the encapsulation)
    if (haveLiner) {
        auto* s = new G4Tubs("pFaceLiner", rGammaO, rTotal, fLinerThickness/2., 0.*deg, 360.*deg);
        fFaceLinerLog = new G4LogicalVolume(s, mLiner, "lFaceLiner");
        fFaceLinerLog->SetVisAttributes(new G4VisAttributes(true, fLinerColour));
        auto* rot = new G4RotationMatrix(); rot->rotateX(90.*deg);
        move = G4ThreeVector(0., -fLinerThickness/2., 0.);
        fHemiShieldAssembly->AddPlacedVolume(fFaceLinerLog, move, rot);
    }

    // 6) opening-face PE moderator slab
    {
        auto* s = new G4Tubs("pFacePE", 0., rTotal, fFacePEThick/2., 0.*deg, 360.*deg);
        fFacePELog = new G4LogicalVolume(s, mFacePE, "lFacePE");
        fFacePELog->SetVisAttributes(new G4VisAttributes(true, fFaceColour));
        auto* rot = new G4RotationMatrix(); rot->rotateX(90.*deg); 
        move = G4ThreeVector(0., -(fFacePEThick/2. + fLinerThickness), 0.);
        fHemiShieldAssembly->AddPlacedVolume(fFacePELog, move, rot);
    }

    // 7) gamma-shield collimator cap: closes the flat opening (cavity mouth) and
    //    is drilled with a central hole to collimate on-axis (-y) gammas. It sits
    //    flush against the forward PE slab and nests inside the gamma shell; the
    //    dome-side rim is rounded by subtracting the gamma-layer inner sphere.
    //    The SAME detector bore that pierces the shells is also drilled through
    //    the cap, so the CLYC channel stays clear and every bore stays coaxial.
    if (fGammaCollThickness > 0.) {
        G4Material* mColl = man->FindOrBuildMaterial(fGammaCollMatName);

        // annular plate: outer = cavity radius, bore = collimator opening
        auto* disk = new G4Tubs("pGammaColl_disk", fGammaCollDiameter/2., rCav,
                                fGammaCollThickness/2., 0.*deg, 360.*deg);

        // gamma-layer inner sphere (material at y >= 0) used to round the rim
        auto* domeCut = new G4Sphere("pGammaColl_cut", rCav, rTotal + 1.*cm,
                                     0.*deg, 180.*deg,    // phi  -> y >= 0
                                     0.*deg, 180.*deg);   // theta

        auto* rot = new G4RotationMatrix(); rot->rotateX(90.*deg);
        move = G4ThreeVector(0., -fLinerThickness + fGammaCollThickness/2., 0.);

        // capTf maps local -> assembly; capTf.inverse() maps assembly -> local.
        G4Transform3D capTf(rot->inverse(), move);

        // (a) round the dome-side rim (sphere is centred at the assembly origin)
        auto* nested = new G4SubtractionSolid("pGammaColl_nested", disk, domeCut,
                                              capTf.inverse());

        // (b) drill the SAME detector bore the shells use. In the assembly frame
        //     the bore sits at 'boreShift'; re-express that placement in the cap
        //     frame so the CLYC channel is cleared and stays coaxial.
        G4Transform3D boreTf = capTf.inverse()
                             * G4Transform3D(G4RotationMatrix(), boreShift);
        auto* collSolid = new G4SubtractionSolid("pGammaColl", nested, bore, boreTf);

        rot = new G4RotationMatrix(); rot->rotateX(-90.*deg);
        fGammaCollLog = new G4LogicalVolume(collSolid, mColl, "lGammaColl");
        fGammaCollLog->SetVisAttributes(new G4VisAttributes(true, fGammaCollColour));
        fHemiShieldAssembly->AddPlacedVolume(fGammaCollLog, move, rot);
    }

    G4cout << " -> GeometryHemiShield: cavity r=" << rCav/cm << " cm, outer r=" << rTotal/cm
           << " cm; gamma=" << fGammaMatName << " (" << fGammaThickness/cm << " cm); "
           << "liner=" << (haveLiner ? fLinerMatName : G4String("none"))
           << "; PE boron=" << 100.*fBoronFrac << " wt%." << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryHemiShield::PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                                       G4RotationMatrix* rotate, G4int copyNo)
{
    fHemiShieldAssembly->MakeImprint(worldLog, move, rotate, copyNo, /*surfCheck*/true);
}

