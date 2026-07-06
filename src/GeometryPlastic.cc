#include "GeometryPlastic.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Tubs.hh"
#include "G4Cons.hh"
#include "G4GenericPolycone.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"
#include "G4RotationMatrix.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "G4Region.hh"
#include "G4RegionStore.hh"

#include <algorithm>
#include <vector>

GeometryPlastic::GeometryPlastic()
{
    // 2" x 2" plastic (matches your CLYC-hack macro)
    fCrystalRadius   = 25.4 * mm;
    fCrystalLength   = 50.8 * mm;
    fCasingThickness = 0.5  * mm;

    // gamma collimator on by default; neutron moderators off (counterproductive
    // for a fast-n plastic detector -- can be switched on from macro)
    fPbColInnerRadius = 30.0  * mm;
    fPbColOuterRadius = 50.0  * mm;
    fPbColLength      = 150.0 * mm;

    fPEColInnerRadius = 50.0 * mm;
    fPEColOuterRadius = 60.0 * mm;
    fPEColLength      = 0.0  * mm;

    fLiFColInnerRadius = 30.0 * mm;
    fLiFColOuterRadius = 32.0 * mm;
    fLiFColLength      = 0.0  * mm;

    // shadow bar: default = solid-W cylinder, standoff 0 -> reproduces the
    // current flush test. Increase standoff + use a taper to optimize.
    fShadowStandoff    = 0.0   * mm;
    fShadowRadiusDet   = 50.0  * mm;
    fShadowRadiusSrc   = 50.0  * mm;
    fShadowBackLength  = 0.0   * mm;   // detector-side layer (default LiF), off
    fShadowBodyLength  = 100.0 * mm;   // bulk (default W)
    fShadowFrontLength = 0.0   * mm;   // source-side layer (default HDPE), off

    // gap collimator -> catches bar back-scatter (keep in BOTH runs)
    fSnoutInnerRadius = 30.0 * mm;
    fSnoutOuterRadius = 50.0 * mm;
    fSnoutLength      = 0.0  * mm;     // off by default

    // rear / side gamma shield (keep in BOTH runs)
    fBackShieldRadius      = 50.0 * mm;
    fBackShieldLength      = 0.0  * mm;   // off by default
    fSideShieldInnerRadius = 26.0 * mm;
    fSideShieldOuterRadius = 50.0 * mm;
    fSideShieldLength      = 0.0  * mm;   // off by default

    fCrystalMatName     = "G4_PLASTIC_SC_VINYLTOLUENE";
    fCasingMatName      = "G4_Al";
    fPbColMatName       = "G4_Pb";
    fPEColMatName       = "PEHD_borated";
    fLiFColMatName      = "LiF";
    fShadowBackMatName  = "LiF";    // 6Li(n,a)t : no capture gamma
    fShadowBodyMatName  = "G4_W";
    fShadowFrontMatName = "PEHD";   // hydrogenous moderator on the source side
    fSnoutMatName       = "PEHD_borated";
    fBackShieldMatName  = "G4_Pb";
    fSideShieldMatName  = "G4_Pb";
}

GeometryPlastic::~GeometryPlastic() {}

void GeometryPlastic::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();

    // If GeometryCLYC (or a previous GeometryPlastic) already built these,
    // do nothing -- avoids duplicate element/material definitions.
    if (nist->FindOrBuildMaterial("PEHD_borated", false) != nullptr) return;

    // enriched lithium (95% Li-6) -- unique element names to avoid clashes
    G4Isotope* iso_Li6 = new G4Isotope("Li6_pl", 3, 6, 6.015 * g/mole);
    G4Isotope* iso_Li7 = new G4Isotope("Li7_pl", 3, 7, 7.016 * g/mole);
    G4Element* el_Li_enr = new G4Element("EnrichedLithium_pl", "Li", 2);
    el_Li_enr->AddIsotope(iso_Li6, 95.0 * perCent);
    el_Li_enr->AddIsotope(iso_Li7,  5.0 * perCent);

    G4Element* el_F  = nist->FindOrBuildElement("F");
    G4Element* el_C  = nist->FindOrBuildElement("C");
    G4Element* el_B  = nist->FindOrBuildElement("B");
    G4Element* el_TS_H = new G4Element("TS_H_of_Polyethylene_pl",
                                       "H_POLYETHYLENE", 1.0, 1.0079*g/mole);

    if (nist->FindOrBuildMaterial("LiF", false) == nullptr) {
        G4Material* mat_LiF = new G4Material("LiF", 2.635*g/cm3, 2);
        mat_LiF->AddElement(el_Li_enr, 1);
        mat_LiF->AddElement(el_F, 1);
    }
    if (nist->FindOrBuildMaterial("PEHD", false) == nullptr) {
        G4Material* mat_PEHD = new G4Material("PEHD", 0.96*g/cm3, 2);
        mat_PEHD->AddElement(el_TS_H, 2);
        mat_PEHD->AddElement(el_C, 1);
    }
    G4Material* mat_PEHD = nist->FindOrBuildMaterial("PEHD");
    G4Material* mat_PEHD_b = new G4Material("PEHD_borated", 1.01*g/cm3, 2);
    mat_PEHD_b->AddMaterial(mat_PEHD, 95.*perCent);
    mat_PEHD_b->AddElement(el_B, 5.*perCent);
}

void GeometryPlastic::AddShadowSegment(G4double zDetFace, G4double len,
                                       G4double rDetEnd, G4double rSrcEnd,
                                       const G4String& matName,
                                       const G4String& name, const G4Colour& col)
{
    if (len <= 0.) return;
    G4Material* mat = G4NistManager::Instance()->FindOrBuildMaterial(matName);
    // G4Cons: radii index 1 at -dz (detector side), index 2 at +dz (source side)
    G4Cons* solid = new G4Cons(name + "_solid",
                               0., std::max(1e-6*mm, rDetEnd),
                               0., std::max(1e-6*mm, rSrcEnd),
                               len/2.0, 0.*deg, 360.*deg);
    G4LogicalVolume* lv = new G4LogicalVolume(solid, mat, name + "_log");
    lv->SetVisAttributes(new G4VisAttributes(true, col));
    G4ThreeVector move(0., 0., zDetFace + len/2.0);
    fAssembly->AddPlacedVolume(lv, move, (G4RotationMatrix*)nullptr);
}

G4int GeometryPlastic::Build()
{
    BuildMaterials();
    fAssembly = new G4AssemblyVolume();

    const G4double sPhi = 0.*deg, dPhi = 360.*deg;
    G4RotationMatrix* rot = nullptr;
    G4ThreeVector move;

    G4NistManager* nist = G4NistManager::Instance();
    G4Material* crystalMat = nist->FindOrBuildMaterial(fCrystalMatName);
    G4Material* casingMat  = nist->FindOrBuildMaterial(fCasingMatName);

    const G4Colour cCrystal(0.0,1.0,0.0,0.5), cCasing(0.5,0.5,0.5,1.),
                   cPb(0.6,0.4,0.2,1.), cPE(0.0,1.0,1.0,1.), cLiF(0.6,0.0,0.6,1.),
                   cW(0.8,0.2,0.2,0.6), cSnout(0.2,0.6,1.0,0.4);

    // ---------------- forward collimators: front face at z=0, toward -z ----
    if (fPbColLength > 0.) {
        G4Tubs* s = new G4Tubs("Pl_PbCol", fPbColInnerRadius, fPbColOuterRadius,
                               fPbColLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fPbColMatName), "Pl_PbCol_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cPb));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,-fPbColLength/2.), rot);
    }
    if (fPEColLength > 0.) {
        G4Tubs* s = new G4Tubs("Pl_PECol", fPEColInnerRadius, fPEColOuterRadius,
                               fPEColLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fPEColMatName), "Pl_PECol_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cPE));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,-fPEColLength/2.), rot);
    }
    if (fLiFColLength > 0.) {
        G4Tubs* s = new G4Tubs("Pl_LiFCol", fLiFColInnerRadius, fLiFColOuterRadius,
                               fLiFColLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fLiFColMatName), "Pl_LiFCol_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cLiF));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,-fLiFColLength/2.), rot);
    }

    // ---------------- crystal: just behind the longest collimator ----------
    const G4double colMax = std::max({fPbColLength, fPEColLength, fLiFColLength, 0.*mm});
    const G4double L = fCrystalLength, R = fCrystalRadius, t = fCasingThickness;
    const G4double crystalCenterZ = -colMax - t - L/2.0;

    G4Tubs* crystalSolid = new G4Tubs("Pl_Crystal", 0., R, L/2., sPhi, dPhi);
    fCrystalLog = new G4LogicalVolume(crystalSolid, crystalMat, "PlasticCrystalLog");
    fCrystalLog->SetVisAttributes(new G4VisAttributes(true, cCrystal));
    // reuse the fine-cut region defined for CLYC
    G4Region* reg = G4RegionStore::GetInstance()->GetRegion("CLYCRegion", false);
    if (!reg) reg = new G4Region("CLYCRegion");
    reg->AddRootLogicalVolume(fCrystalLog);
    fAssembly->AddPlacedVolume(fCrystalLog, move = G4ThreeVector(0,0,crystalCenterZ), rot);

    if (t > 0.) {
        std::vector<G4double> rC = { 0.*mm, R+t, R+t, R, R, 0.*mm };
        std::vector<G4double> zC = { +(L/2.+t), +(L/2.+t), -L/2., -L/2., +L/2., +L/2. };
        G4GenericPolycone* cs = new G4GenericPolycone("Pl_Casing", sPhi, dPhi,
                                                      rC.size(), rC.data(), zC.data());
        auto* lv = new G4LogicalVolume(cs, casingMat, "Pl_Casing_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cCasing));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,crystalCenterZ), rot);
    }

    // ---------------- rear gamma backstop (cask BEHIND detector) -----------
    if (fBackShieldLength > 0.) {
        const G4double zRear = crystalCenterZ - L/2.0;   // bare crystal rear face
        G4Tubs* s = new G4Tubs("Pl_BackShield", 0., fBackShieldRadius,
                               fBackShieldLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fBackShieldMatName),
                                       "Pl_BackShield_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cPb));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,zRear - fBackShieldLength/2.0), rot);
    }

    // ---------------- side gamma skirt (adjacent casks) --------------------
    if (fSideShieldLength > 0.) {
        // casing front face = crystalCenterZ + L/2 + t = -colMax (flush w/ collimator back)
        const G4double zFront = crystalCenterZ + L/2.0 + t;   // == -colMax
        G4Tubs* s = new G4Tubs("Pl_SideShield", fSideShieldInnerRadius,
                               fSideShieldOuterRadius, fSideShieldLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fSideShieldMatName),
                                       "Pl_SideShield_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cPb));
        // hang it BACKWARD from the casing front so it wraps only the crystal region
        move = G4ThreeVector(0., 0., zFront - fSideShieldLength/2.0);
        fAssembly->AddPlacedVolume(lv, move, rot);
    }

    // ---------------- shadow bar: 3 graded cone layers (det -> src) --------
    const G4double T = fShadowBackLength + fShadowBodyLength + fShadowFrontLength;
    if (T > 0.) {
        const G4double dR = fShadowRadiusSrc - fShadowRadiusDet;
        auto radAt = [&](G4double s){ return fShadowRadiusDet + dR*(s/T); };

        G4double z = fShadowStandoff;
        AddShadowSegment(z, fShadowBackLength,
                         radAt(0.), radAt(fShadowBackLength),
                         fShadowBackMatName, "Pl_ShadowBack", cLiF);
        z += fShadowBackLength;
        AddShadowSegment(z, fShadowBodyLength,
                         radAt(fShadowBackLength), radAt(fShadowBackLength+fShadowBodyLength),
                         fShadowBodyMatName, "Pl_ShadowBody", cW);
        z += fShadowBodyLength;
        AddShadowSegment(z, fShadowFrontLength,
                         radAt(fShadowBackLength+fShadowBodyLength), radAt(T),
                         fShadowFrontMatName, "Pl_ShadowFront", cPE);
    }

    // ---------------- snout: low-Z tube filling the gap --------------------
    if (fSnoutLength > 0.) {
        G4Tubs* s = new G4Tubs("Pl_Snout", fSnoutInnerRadius, fSnoutOuterRadius,
                               fSnoutLength/2., sPhi, dPhi);
        auto* lv = new G4LogicalVolume(s, nist->FindOrBuildMaterial(fSnoutMatName), "Pl_Snout_log");
        lv->SetVisAttributes(new G4VisAttributes(true, cSnout));
        fAssembly->AddPlacedVolume(lv, move = G4ThreeVector(0,0,+fSnoutLength/2.), rot);
    }

    return 1;
}

void GeometryPlastic::PlaceDetector(G4LogicalVolume* world, G4ThreeVector move,
                                    G4RotationMatrix* rotate, G4int copyNo)
{
    fAssembly->MakeImprint(world, move, rotate, copyNo, /*surfCheck*/false);
}

G4ThreeVector GeometryPlastic::GetCrystalCenterLocal() const
{
    const G4double colMax = std::max({fPbColLength, fPEColLength, fLiFColLength, 0.*mm});
    G4double z = -colMax - fCasingThickness - fCrystalLength/2.0;
    return G4ThreeVector(0.,0.,z);
}

G4double GeometryPlastic::GetFrontFaceLocalZ() const
{
    // Front-most physical surface in +z (toward the cask):
    //  - collimator front face is at z = 0
    //  - shadow-bar tip is at standoff + total bar length
    //  - snout tip is at snoutLength
    const G4double T = fShadowBackLength + fShadowBodyLength + fShadowFrontLength;
    G4double front = 0.;
    if (T > 0.)            front = std::max(front, fShadowStandoff + T);
    if (fSnoutLength > 0.) front = std::max(front, fSnoutLength);
    return front;
}
