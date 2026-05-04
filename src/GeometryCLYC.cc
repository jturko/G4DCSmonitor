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
    fCLYCCrystalLog(NULL),
    fAlumCasingLog(NULL),
    fLiFCollimatorLog(NULL),
    fPbCollimatorLog(NULL),
    fPEHDCollimatorLog(NULL),
    fPEPlugLog(NULL)
{
    fCLYCCrystalRadius = 25./2 * mm;
    fCLYCCrystalLength = 25. * mm;
    fAlumCasingThickness = 0.5 * mm;
    
    fLiFCollimatorInnerRadius = -1. * mm;
    fLiFCollimatorOuterRadius = -1. * mm;
    fLiFCollimatorLength = -1. * mm;
    
    fPbCollimatorInnerRadius = 29.4/2. * mm;
    fPbCollimatorOuterRadius = 40./2. * mm;
    fPbCollimatorLength = 50. * mm;
    
    fPEHDCollimatorInnerRadius = 40./2. * mm;
    fPEHDCollimatorOuterRadius = 50./2. * mm;
    fPEHDCollimatorLength = 50. * mm;

    fPEPlugInnerRadius = 29./2. * mm;
    fPEPlugLipRadius = 50./2. * mm;
    fPEPlugInnerLength = 35. * mm;
    fPEPlugLipLength = 10. * mm; 

    fCLYCCrystalMatName = "CLYC";
    fAlumMatName = "G4_Al";
    fPbMatName = "G4_Pb";
    fLiFMatName = "LiF";
    fPEHDMatName = "G4_POLYETHYLENE";

    G4double alpha = 1.0; 
    fCLYCCrystalColour = G4Colour(0.0, 1.0, 0.0, 0.5); // Green
    fAlumColour        = G4Colour(0.5, 0.5, 0.5, alpha); // Grey
    fLiFColour         = G4Colour(0.5, 1.0, 0.0, alpha); // Green-yellow-ish
    fPbColour          = G4Colour(0.6, 0.4, 0.2, alpha); // Brown
    fPEHDColour        = G4Colour(0.0, 1.0, 1.0, alpha); // Cyan
    fPEPlugColour      = G4Colour(1.0, 1.0, 0.0, alpha); // Yellow

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
    G4bool onlyBuildCLYC = false;

    G4NistManager* manager = G4NistManager::Instance();
    G4Material* CLYC_material = manager->FindOrBuildMaterial(fCLYCCrystalMatName);
    G4Material* Alum_material = manager->FindOrBuildMaterial(fAlumMatName);
    G4Material* LiF_material = manager->FindOrBuildMaterial(fLiFMatName);
    G4Material* Pb_material = manager->FindOrBuildMaterial(fPbMatName);
    G4Material* PEHD_material = manager->FindOrBuildMaterial(fPEHDMatName);

    // 1. PE Plug
    // Front face sits exactly at Z=0, projecting backwards.
    if(fPEPlugLipLength>0. || fPEPlugInnerLength>0.) {
        std::vector<G4double> rPlug = { 0.*mm, fPEPlugLipRadius,  fPEPlugLipRadius,  fPEPlugInnerRadius,  fPEPlugInnerRadius,                   0.*mm };
        std::vector<G4double> zPlug = { 0.*mm, 0.*mm,            -fPEPlugLipLength, -fPEPlugLipLength,   -fPEPlugLipLength-fPEPlugInnerLength, -fPEPlugLipLength-fPEPlugInnerLength };
        G4GenericPolycone* PE_plug_solid = new G4GenericPolycone("PE_plug_solid", startPhi, endPhi, rPlug.size(), rPlug.data(), zPlug.data());
        fPEPlugLog = new G4LogicalVolume(PE_plug_solid, PEHD_material, "PEPlugLog", 0, 0, 0);
        fPEPlugLog->SetVisAttributes(new G4VisAttributes(true, fPEPlugColour));
        move = G4ThreeVector(0., 0., 0.);
        if(!onlyBuildCLYC)
            fCLYCAssembly->AddPlacedVolume(fPEPlugLog, move, rotate);
    }

    // 2. Collimators (PEHD and Pb)
    // Front faces aligned with the back of the plug lip (Z = -fPEPlugLipLength).
    // Tubs are defined from their center, so we shift by half their length.
    // PE-HD collimator
    if(fPEHDCollimatorLength>0.) {
        G4double pehdCenterZ = -fPEPlugLipLength - (fPEHDCollimatorLength / 2.0);
        G4Tubs* PEHD_collimator_solid = new G4Tubs("PEHD_collimator_solid", fPEHDCollimatorInnerRadius, fPEHDCollimatorOuterRadius, fPEHDCollimatorLength/2., startPhi, endPhi);
        fPEHDCollimatorLog = new G4LogicalVolume(PEHD_collimator_solid, PEHD_material, "PEHDCollimatorLog", 0, 0, 0);
        fPEHDCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPEHDColour));
        move = G4ThreeVector(0., 0., pehdCenterZ);
        if(!onlyBuildCLYC)
            fCLYCAssembly->AddPlacedVolume(fPEHDCollimatorLog, move, rotate);
    }
    // Pb collimator
    if(fPbCollimatorLength>0.) {
        G4double pbCenterZ = -fPEPlugLipLength - (fPbCollimatorLength / 2.0);
        G4Tubs* Pb_collimator_solid = new G4Tubs("Pb_collimator_solid", fPbCollimatorInnerRadius, fPbCollimatorOuterRadius, fPbCollimatorLength/2., startPhi, endPhi);
        fPbCollimatorLog = new G4LogicalVolume(Pb_collimator_solid, Pb_material, "PbCollimatorLog", 0, 0, 0);
        fPbCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fPbColour));
        move = G4ThreeVector(0., 0., pbCenterZ);
        if(!onlyBuildCLYC)
            fCLYCAssembly->AddPlacedVolume(fPbCollimatorLog, move, rotate);
    }
    // LiF collimator
    if(fLiFCollimatorLength>0.) {
        G4double lifCenterZ = -fPEPlugLipLength - (fLiFCollimatorLength / 2.0);
        G4Tubs* LiF_collimator_solid = new G4Tubs("LiF_collimator_solid", fLiFCollimatorInnerRadius, fLiFCollimatorOuterRadius, fLiFCollimatorLength/2., startPhi, endPhi);
        fLiFCollimatorLog = new G4LogicalVolume(LiF_collimator_solid, LiF_material, "LiFCollimatorLog", 0, 0, 0);
        fLiFCollimatorLog->SetVisAttributes(new G4VisAttributes(true, fLiFColour));

        move = G4ThreeVector(0., 0., lifCenterZ);
        if(!onlyBuildCLYC)
            fCLYCAssembly->AddPlacedVolume(fLiFCollimatorLog, move, rotate);
    }

    // 3. CLYC Crystal and Aluminum Casing
    // Front face of the Aluminum Casing is flush with the rear face of the entire plug.
    G4double L = fCLYCCrystalLength;
    G4double R = fCLYCCrystalRadius;
    G4double t = fAlumCasingThickness;
    // The plug ends at -fPEPlugLength (-fPEPlugLipLength - fPEPlugInnerLength). We place the front of the casing exactly there.
    G4double crystalCenterZ = -fPEPlugLipLength - fPEPlugInnerLength - t - (L / 2.0);
    G4Tubs* CLYC_solid = new G4Tubs("CLYC_solid", 0.*mm, R, L/2.0, startPhi, endPhi);
    fCLYCCrystalLog = new G4LogicalVolume(CLYC_solid, CLYC_material, "CLYCCrystalLog", 0, 0, 0);
    fCLYCCrystalLog->SetVisAttributes(new G4VisAttributes(true, fCLYCCrystalColour));
    // --- set region in CLYC for finer physics list production cuts
    G4Region* clycRegion = G4RegionStore::GetInstance()->GetRegion("CLYCRegion", false);
    if (!clycRegion) {
        clycRegion = new G4Region("CLYCRegion");
    }
    clycRegion->AddRootLogicalVolume(fCLYCCrystalLog);
    // ---
    move = G4ThreeVector(0., 0., crystalCenterZ);
    fCLYCAssembly->AddPlacedVolume(fCLYCCrystalLog, move, rotate);

    // Aluminum casing overlaps the sides and FRONT face (+Z) of the crystal
    if(t>0.) {
        std::vector<G4double> rCasing = { 0.*mm, R + t, R + t, R, R, 0.*mm };
        std::vector<G4double> zCasing = { +(L/2. + t), +(L/2. + t), -L/2., -L/2., +L/2., +L/2. };

        G4GenericPolycone* Alum_solid = new G4GenericPolycone("Alum_solid", startPhi, endPhi, rCasing.size(), rCasing.data(), zCasing.data());
        fAlumCasingLog = new G4LogicalVolume(Alum_solid, Alum_material, "AlumCasingLog", 0, 0, 0);
        fAlumCasingLog->SetVisAttributes(new G4VisAttributes(true, fAlumColour));

        move = G4ThreeVector(0., 0., crystalCenterZ);
        if(!onlyBuildCLYC)
            fCLYCAssembly->AddPlacedVolume(fAlumCasingLog, move, rotate);
    }

    return 1;
}

void GeometryCLYC::PlaceDetector(G4LogicalVolume* logic_world, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo) 
{
    G4bool surfCheck = true;
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
    // enriched lithium
    G4Isotope* iso_Li6 = new G4Isotope("Li6", 3, 6, 6.015 * g/mole);
    G4Isotope* iso_Li7 = new G4Isotope("Li7", 3, 7, 7.016 * g/mole);
    G4Element* el_Li_enr = new G4Element("Enriched Lithium", "Li", 2);
    el_Li_enr->AddIsotope(iso_Li6, 95.0 * perCent);
    el_Li_enr->AddIsotope(iso_Li7, 5.0 * perCent);

    // materials
    // NIST 
    G4Material* mat_Pb = nist->FindOrBuildMaterial(fPbMatName);
    G4Material* mat_PE_HD = nist->FindOrBuildMaterial(fPEHDMatName);
    G4Material* mat_Al = nist->FindOrBuildMaterial(fAlumMatName);
    // CLYC
    G4Material* mat_CLYC = new G4Material("CLYC", 3.31 * g/cm3, 4);
    mat_CLYC->AddElement(el_Cs,     2);
    mat_CLYC->AddElement(el_Li_enr, 1);
    mat_CLYC->AddElement(el_Y,      1);
    mat_CLYC->AddElement(el_Cl,     6);   
    // enriched LiF
    G4Material* LiF = new G4Material("LiF", 2.635*g/cm3, 2);
    LiF->AddElement(el_Li_enr, 1);
    LiF->AddElement(el_F, 1);
}

G4ThreeVector GeometryCLYC::GetCrystalCenterLocal() const
{
    // Replicate the offset calculation used in Build()
    G4double crystalCenterZ = -fPEPlugLipLength - fPEPlugInnerLength - fAlumCasingThickness - (fCLYCCrystalLength / 2.0);
    return G4ThreeVector(0., 0., crystalCenterZ);
}

//void GeometryCLYC::PlaceDetectorByCrystalCenter(G4LogicalVolume* logic_world, G4ThreeVector move, G4RotationMatrix* rotate, G4int copyNo)
//{
//    G4ThreeVector localOffset = GetCrystalCenterLocal();
//    G4ThreeVector globalOffset = localOffset;
//
//    // Rotate the offset vector if a rotation matrix is provided
//    if (rotate) {
//        globalOffset.transform(*rotate);
//    }
//
//    // Shift the assembly origin so the crystal center aligns with the requested 'move' position
//    G4ThreeVector assemblyPos = move - globalOffset;
//
//    G4bool surfCheck = true;
//    fCLYCAssembly->MakeImprint(logic_world, assemblyPos, rotate, copyNo, surfCheck);
//}
