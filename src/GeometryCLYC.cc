// 1. Geant4 Headers First (Ensures G4RotationMatrix/G4ThreeVector are declared)
#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"

#include "G4Tubs.hh"
#include "G4Box.hh"
#include "G4Cons.hh"
#include "G4GenericPolycone.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"

#include "G4SubtractionSolid.hh"
#include "G4GeometryManager.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4SolidStore.hh"
#include "G4AssemblyVolume.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

#include "GeometryCLYC.hh"
#include <string>

// for setting finer production cuts in CLYC
#include "G4Region.hh"
#include "G4RegionStore.hh"


GeometryCLYC::GeometryCLYC() :
    fCLYCAssembly(NULL),
    fCrystalLog(NULL),
    fCasingLog(NULL),
    fLiFCollimatorLog(NULL),
    fPbCollimatorLog(NULL),
    fPECollimatorLog(NULL),
    fPEPlugLog(NULL)
{
    fCrystalRadius = 25./2 * mm;
    fCrystalLength = 25. * mm;
    fCasingThickness = 0.5 * mm;
    
    fPbCollimatorInnerRadius = 29.4/2. * mm;
    fPbCollimatorOuterRadius = 40./2. * mm;
    fPbCollimatorLength = 50. * mm;
    
    fLiFCollimatorInnerRadius = -1. * mm;
    fLiFCollimatorOuterRadius = -1. * mm;
    fLiFCollimatorLength = -1. * mm;
    
    fPECollimatorInnerRadius = 40./2. * mm;
    fPECollimatorOuterRadius = 50./2. * mm;
    fPECollimatorLength = 50. * mm;

    fPEPlugInnerRadius = 29./2. * mm;
    fPEPlugLipRadius = 50./2. * mm;
    fPEPlugInnerLength = 35. * mm;
    fPEPlugLipLength = 10. * mm; 

    fCrystalMatName = "CLYC";
    fCasingMatName = "G4_Al";
    fPbColMatName = "G4_Pb";
    fLiFColMatName = "LiF";
    fPEColMatName = "G4_POLYETHYLENE";
    fPEPlugMatName = "G4_POLYETHYLENE";

    G4double alpha = 1.0; 
    fCrystalColour = G4Colour(0.0, 1.0, 0.0, 0.5); // Green
    fCasingColour  = G4Colour(0.5, 0.5, 0.5, alpha); // Grey
    fPbColColour   = G4Colour(0.6, 0.4, 0.2, alpha); // Brown
    fLiFColColour  = G4Colour(0.6, 0.0, 0.6, alpha); // Magenta-ish
    fPEColColour   = G4Colour(0.0, 1.0, 1.0, alpha); // Cyan
    fPEPlugColour  = G4Colour(1.0, 1.0, 0.0, alpha); // Yellow

}

GeometryCLYC::~GeometryCLYC() {}

// CLYC crystal centered with HDPE collimator rear face
G4int GeometryCLYC::Build()
{
    BuildMaterials();
    fCLYCAssembly = new G4AssemblyVolume();
    
    G4double startPhi = 0.0*deg, endPhi = 360.0*deg;
    G4ThreeVector move;
    G4RotationMatrix* rotate = NULL;
    G4bool onlyBuildCrystal = false;

    G4NistManager* manager = G4NistManager::Instance();
    G4Material* Crystal_material    = manager->FindOrBuildMaterial(fCrystalMatName);
    G4Material* Casing_material     = manager->FindOrBuildMaterial(fCasingMatName);
    G4Material* LiFCol_material     = manager->FindOrBuildMaterial(fLiFColMatName);
    G4Material* PbCol_material      = manager->FindOrBuildMaterial(fPbColMatName);
    G4Material* PECol_material      = manager->FindOrBuildMaterial(fPEColMatName);
    G4Material* PEPlug_material     = manager->FindOrBuildMaterial(fPEPlugMatName);

    // 1. PE Plug
    // Front face sits exactly at Z=0, projecting backwards.
    if(fPEPlugLipLength>0. || fPEPlugInnerLength>0.) {
        std::vector<G4double> rPlug = { 0.*mm, fPEPlugLipRadius,  fPEPlugLipRadius,  fPEPlugInnerRadius,  fPEPlugInnerRadius,                   0.*mm };
        std::vector<G4double> zPlug = { 0.*mm, 0.*mm,            -fPEPlugLipLength, -fPEPlugLipLength,   -fPEPlugLipLength-fPEPlugInnerLength, -fPEPlugLipLength-fPEPlugInnerLength };
        G4GenericPolycone* PE_plug_solid = new G4GenericPolycone("PE_plug_solid", startPhi, endPhi, rPlug.size(), rPlug.data(), zPlug.data());
        fPEPlugLog = new G4LogicalVolume(PE_plug_solid, PEPlug_material, "PEPlugLog", 0, 0, 0);
        fPEPlugLog->SetVisAttributes(new G4VisAttributes(true, fPEPlugColour));
        move = G4ThreeVector(0., 0., 0.);
        if(!onlyBuildCrystal)
            fCLYCAssembly->AddPlacedVolume(fPEPlugLog, move, rotate);
    }

    // 2. Collimators (PEHD and Pb)
    // Front faces aligned with the back of the plug lip (Z = -fPEPlugLipLength).
    // Tubs are defined from their center, so we shift by half their length.
    // PE-HD collimator
    if(fPECollimatorLength>0.) {
        G4double peColCenterZ = -fPEPlugLipLength - (fPECollimatorLength / 2.0);
        G4Tubs* PE_collimator_solid = new G4Tubs("PE_collimator_solid", fPECollimatorInnerRadius, fPECollimatorOuterRadius, fPECollimatorLength/2., startPhi, endPhi);
        fPECollimatorLog = new G4LogicalVolume(PE_collimator_solid, PECol_material, "PECollimatorLog", 0, 0, 0);
        fPECollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPEColColour));
        move = G4ThreeVector(0., 0., peColCenterZ);
        if(!onlyBuildCrystal)
            fCLYCAssembly->AddPlacedVolume(fPECollimatorLog, move, rotate);
    }
    // Pb collimator
    if(fPbCollimatorLength>0.) {
        G4double pbCenterZ = -fPEPlugLipLength - (fPbCollimatorLength / 2.0);
        G4Tubs* Pb_collimator_solid = new G4Tubs("Pb_collimator_solid", fPbCollimatorInnerRadius, fPbCollimatorOuterRadius, fPbCollimatorLength/2., startPhi, endPhi);
        fPbCollimatorLog = new G4LogicalVolume(Pb_collimator_solid, PbCol_material, "PbCollimatorLog", 0, 0, 0);
        fPbCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPbColColour));
        move = G4ThreeVector(0., 0., pbCenterZ);
        if(!onlyBuildCrystal)
            fCLYCAssembly->AddPlacedVolume(fPbCollimatorLog, move, rotate);
    }
    // LiF collimator
    if(fLiFCollimatorLength>0.) {
        G4double lifCenterZ = -fPEPlugLipLength - (fLiFCollimatorLength / 2.0);
        G4Tubs* LiF_collimator_solid = new G4Tubs("LiF_collimator_solid", fLiFCollimatorInnerRadius, fLiFCollimatorOuterRadius, fLiFCollimatorLength/2., startPhi, endPhi);
        fLiFCollimatorLog = new G4LogicalVolume(LiF_collimator_solid, LiFCol_material, "LiFCollimatorLog", 0, 0, 0);
        fLiFCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fLiFColColour));

        move = G4ThreeVector(0., 0., lifCenterZ);
        if(!onlyBuildCrystal)
            fCLYCAssembly->AddPlacedVolume(fLiFCollimatorLog, move, rotate);
    }

    // 3. CLYC Crystal and Aluminum Casing
    // Front face of the Aluminum Casing is flush with the rear face of the entire plug.
    G4double L = fCrystalLength;
    G4double R = fCrystalRadius;
    G4double t = fCasingThickness;
    // The plug ends at -fPEPlugLength (-fPEPlugLipLength - fPEPlugInnerLength). We place the front of the casing exactly there.
    G4double crystalCenterZ = -fPEPlugLipLength - fPEPlugInnerLength - t - (L / 2.0);
    G4Tubs* Crystal_solid = new G4Tubs("Crystal_solid", 0.*mm, R, L/2.0, startPhi, endPhi);
    fCrystalLog = new G4LogicalVolume(Crystal_solid, Crystal_material, "CrystalLog", 0, 0, 0);
    fCrystalLog->SetVisAttributes(new G4VisAttributes(true, fCrystalColour));
    // --- set region in CLYC for finer physics list production cuts
    G4Region* clycRegion = G4RegionStore::GetInstance()->GetRegion("CLYCRegion", false);
    if (!clycRegion) {
        clycRegion = new G4Region("CLYCRegion");
    }
    clycRegion->AddRootLogicalVolume(fCrystalLog);
    // ---
    move = G4ThreeVector(0., 0., crystalCenterZ);
    fCLYCAssembly->AddPlacedVolume(fCrystalLog, move, rotate);

    // Aluminum casing overlaps the sides and FRONT face (+Z) of the crystal
    if(t>0.) {
        std::vector<G4double> rCasing = { 0.*mm, R + t, R + t, R, R, 0.*mm };
        std::vector<G4double> zCasing = { +(L/2. + t), +(L/2. + t), -L/2., -L/2., +L/2., +L/2. };

        G4GenericPolycone* Casing_solid = new G4GenericPolycone("Casing_solid", startPhi, endPhi, rCasing.size(), rCasing.data(), zCasing.data());
        fCasingLog = new G4LogicalVolume(Casing_solid, Casing_material, "CasingLog", 0, 0, 0);
        fCasingLog->SetVisAttributes(new G4VisAttributes(true, fCasingColour));

        move = G4ThreeVector(0., 0., crystalCenterZ);
        if(!onlyBuildCrystal)
            fCLYCAssembly->AddPlacedVolume(fCasingLog, move, rotate);
    }

    return 1;
}

void GeometryCLYC::PlaceDetector(G4LogicalVolume* logic_world, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo) 
{
    G4bool surfCheck = false;
    fCLYCAssembly->MakeImprint(logic_world, move, rotate, copyNo, surfCheck);
}

void GeometryCLYC::BuildMaterials() 
{
    G4NistManager* nist = G4NistManager::Instance();
    
    // return early if custom materials already built
    if (nist->FindOrBuildMaterial("CLYC") != nullptr) {
        return;
    }

    // elements
    // NIST 
    G4Element* el_Cs = nist->FindOrBuildElement("Cs");
    G4Element* el_Y  = nist->FindOrBuildElement("Y");
    G4Element* el_Cl = nist->FindOrBuildElement("Cl");
    G4Element* el_Li = nist->FindOrBuildElement("Li");
    G4Element* el_F  = nist->FindOrBuildElement("F");
    G4Element* el_C  = nist->FindOrBuildElement("C");
    G4Element* el_B  = nist->FindOrBuildElement("B");
    // enriched lithium
    G4Isotope* iso_Li6 = new G4Isotope("Li6", 3, 6, 6.015 * g/mole);
    G4Isotope* iso_Li7 = new G4Isotope("Li7", 3, 7, 7.016 * g/mole);
    G4Element* el_Li_enr = new G4Element("Enriched Lithium", "Li", 2);
    el_Li_enr->AddIsotope(iso_Li6, 95.0 * perCent);
    el_Li_enr->AddIsotope(iso_Li7, 5.0 * perCent);
    // hydrogen from polyethylene, for S(alpha,beta) thermal neutron scattering cross-sections below 4 eV
    G4Element* el_TS_H = new G4Element("TS_H_of_Polyethylene", "H_POLYETHYLENE", 1.0, 1.0079*g/mole); 

    // materials
    // NIST 
    G4Material* mat_Al      = nist->FindOrBuildMaterial("G4_Al");
    G4Material* mat_Pb      = nist->FindOrBuildMaterial("G4_Pb");
    G4Material* mat_PE      = nist->FindOrBuildMaterial("G4_POLYETHYLENE");
    G4Material* mat_PSC     = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
    G4Material* mat_Air     = nist->FindOrBuildMaterial("G4_AIR");
    G4Material* mat_W       = nist->FindOrBuildMaterial("G4_W");
    // CLYC
    G4Material* mat_CLYC = new G4Material("CLYC", 3.31 * g/cm3, 4);
    mat_CLYC->AddElement(el_Cs,     2);
    mat_CLYC->AddElement(el_Li_enr, 1);
    mat_CLYC->AddElement(el_Y,      1);
    mat_CLYC->AddElement(el_Cl,     6);   
    // enriched LiF
    G4Material* mat_LiF = new G4Material("LiF", 2.635*g/cm3, 2);
    mat_LiF->AddElement(el_Li_enr, 1);
    mat_LiF->AddElement(el_F, 1);
    // custom-defined LD/HD/boratedHD PE, using hydrogen element which uses the thermal scattering cross-sectiond data 
    // high-density polyethylene (PEHD) 
    G4Material* mat_PEHD = new G4Material("PEHD", 0.96*g/cm3, 2);
    mat_PEHD->AddElement(el_TS_H, 2);
    mat_PEHD->AddElement(el_C, 1);
    // borated PEHD
    G4Material* mat_PEHD_borated = new G4Material("PEHD_borated", 1.01*g/cm3, 2);
    mat_PEHD_borated->AddMaterial(mat_PEHD, 95.*perCent);
    mat_PEHD_borated->AddElement(el_B, 5.*perCent);
}

G4ThreeVector GeometryCLYC::GetCrystalCenterLocal() const
{
    // Replicate the offset calculation used in Build()
    G4double crystalCenterZ = -fPEPlugLipLength - fPEPlugInnerLength - fCasingThickness - (fCrystalLength / 2.0);
    return G4ThreeVector(0., 0., crystalCenterZ);
}

