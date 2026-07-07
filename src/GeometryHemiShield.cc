#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Isotope.hh"
#include "G4Element.hh"

#include "G4Tubs.hh"
#include "G4Sphere.hh"
#include "G4SubtractionSolid.hh"
#include "G4GenericPolycone.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

#include "GeometryHemiShield.hh"
#include "G4Region.hh"
#include "G4RegionStore.hh"

#include <vector>
#include <string>

GeometryHemiShield::GeometryHemiShield() 
{

}

GeometryHemiShield::~GeometryHemiShield() {}

G4int GeometryHemiShield::Build()
{
    fHemiShieldAssembly = new G4AssemblyVolume();
    G4ThreeVector     move;
    G4RotationMatrix* rot = nullptr;
    G4RotationMatrix* mrot;
        
    BuildMaterials();
    G4NistManager* man = G4NistManager::Instance();

    G4Material* mPb = man->FindOrBuildMaterial("G4_Pb");
    auto pPb_precut = new G4Sphere("pPb_precut", 6.*cm, 8.*cm, 0.*deg, 180.*deg, 0.*deg, 180.*deg);
    auto pPb_cutter = new G4Tubs("pPb_cutter", 0.*cm, 3.*cm, 50.*cm, 0.*deg, 360.*deg);
    auto pPb = new G4SubtractionSolid("pPb", pPb_precut, pPb_cutter, nullptr, G4ThreeVector(0., 3.*cm, -50.*cm));
    auto lPb = new G4LogicalVolume(pPb, mPb, "lPb");
    lPb->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0,0.0,0.0,   1.0)));
    fHemiShieldAssembly->AddPlacedVolume(lPb, move, rot);

    G4Material* mLiF = man->FindOrBuildMaterial("LiF");
    auto pLiF_precut = new G4Sphere("pLiF_precut", 8.*cm, 8.5*cm, 0.*deg, 180.*deg, 0.*deg, 180.*deg);
    auto pLiF = new G4SubtractionSolid("pLiF", pLiF_precut, pPb_cutter, nullptr, G4ThreeVector(0., 3.*cm, -50.*cm));
    auto lLiF = new G4LogicalVolume(pLiF, mLiF, "lLiF");
    lLiF->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0,1.0,0.0,   1.0)));
    fHemiShieldAssembly->AddPlacedVolume(lLiF, move, rot);

    G4Material* mPE = man->FindOrBuildMaterial("PEHD_borated");
    auto pPE_precut = new G4Sphere("pPE_precut", 8.5*cm, 25.*cm, 0.*deg, 180.*deg, 0.*deg, 180.*deg);
    auto pPE = new G4SubtractionSolid("pPE", pPE_precut, pPb_cutter, nullptr, G4ThreeVector(0., 3.*cm, -50.*cm));
    auto lPE = new G4LogicalVolume(pPE, mPE, "lPE");
    lPE->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.95,1.0,0.95,   1.0)));
    fHemiShieldAssembly->AddPlacedVolume(lPE, move, rot);
    
    auto pLiF2_precut = new G4Sphere("pLiF2_precut", 25.*cm, 25.5*cm, 0.*deg, 180.*deg, 0.*deg, 180.*deg);
    auto pLiF2 = new G4SubtractionSolid("pLiF2", pLiF2_precut, pPb_cutter, nullptr, G4ThreeVector(0., 3.*cm, -50.*cm));
    auto lLiF2 = new G4LogicalVolume(pLiF2, mLiF, "lLiF2");
    lLiF2->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0,1.0,0.0,   1.0)));
    fHemiShieldAssembly->AddPlacedVolume(lLiF2, move, rot);

    auto pLiF3 = new G4Tubs("pLiF3", 8.*cm, 25.5*cm, 0.5/2.*cm, 0.*deg, 360.*deg); 
    auto lLiF3 = new G4LogicalVolume(pLiF3, mLiF, "lLiF3");
    lLiF3->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0,1.0,0.0,   1.0)));
    move = G4ThreeVector(0., -0.5/2.*cm, 0.);
    mrot = new G4RotationMatrix;
    mrot->rotateX(90.*deg);
    fHemiShieldAssembly->AddPlacedVolume(lLiF3, move, mrot);

    mPE = man->FindOrBuildMaterial("PEHD");
    auto pPEplug = new G4Tubs("pPEplug", 0.*cm, 25.5*cm, 4.5/2.*cm, 0.*deg, 360.*deg);
    auto lPEplug = new G4LogicalVolume(pPEplug, mPE, "lPEplug");
    lPEplug->SetVisAttributes(new G4VisAttributes(true, G4Colour(0,0,0.8,   1.0)));
    move = G4ThreeVector(0., -4.5/2.*cm - 0.5*cm, 0.);
    mrot = new G4RotationMatrix;
    mrot->rotateX(90.*deg);
    fHemiShieldAssembly->AddPlacedVolume(lPEplug, move, mrot);
        
    return 1;
}

void GeometryHemiShield::PlaceDetector(G4LogicalVolume* logic_world, G4ThreeVector move,
                                 G4RotationMatrix* rotate, G4int copyNo)
{
    fHemiShieldAssembly->MakeImprint(logic_world, move, rotate, copyNo, /*surfCheck*/true);
}

void GeometryHemiShield::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();

    // NIST elements
    G4Element* el_F  = nist->FindOrBuildElement("F");
    G4Element* el_C  = nist->FindOrBuildElement("C");
    G4Element* el_H  = nist->FindOrBuildElement("H");
    G4Element* el_B  = nist->FindOrBuildElement("B");

    // custom elements
    // enr. Lithium
    G4Isotope* Li6 = new G4Isotope("Li6", 3, 6, 6.015*g/mole);
    G4Isotope* Li7 = new G4Isotope("Li7", 3, 7, 7.016*g/mole);
    G4Element* el_Li_enr = new G4Element("Enriched Lithium", "Li", 2);
    el_Li_enr->AddIsotope(Li6, 95.*perCent);
    el_Li_enr->AddIsotope(Li7,  5.*perCent);
    // H (from polyethelene) with S(alpha,beta) for thermal stopping
    G4Element* el_TS_H = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE", 1.0, 1.0079*g/mole);

    // NIST materials
    nist->FindOrBuildMaterial("G4_Pb");
    nist->FindOrBuildMaterial("G4_POLYETHYLENE");
    nist->FindOrBuildMaterial("G4_AIR");

    // custom materials
    // LiF
    if (nist->FindOrBuildMaterial("LiF", false) == nullptr) {
        G4Material* lif = new G4Material("LiF", 2.635*g/cm3, 2);
        lif->AddElement(el_Li_enr, 1); lif->AddElement(el_F, 1);
    }
    // PEHD
    if (nist->FindOrBuildMaterial("PEHD", false) == nullptr) {
        G4Material* pehd = new G4Material("PEHD", 0.96*g/cm3, 2);
        pehd->AddElement(el_TS_H, 2); pehd->AddElement(el_C, 1);
    }
    // borated PEHD
    if (nist->FindOrBuildMaterial("PEHD_borated", false) == nullptr) {
        G4Material* pehdb = new G4Material("PEHD_borated", 1.01*g/cm3, 2);
        G4Material* pehd = nist->FindOrBuildMaterial("PEHD");
        pehdb->AddMaterial(pehd, 95.*perCent); pehdb->AddElement(el_B, 5.*perCent);
    }
}
