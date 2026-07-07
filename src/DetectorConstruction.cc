//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file DetectorConstruction.cc
/// \brief Implementation of the DetectorConstruction class
//
//

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "DCSMonitorSD.hh"

#include "GeometryCLYC.hh"
#include "GeometryHemiShield.hh"
#include "GeometryPlastic.hh"
#include "GeometryCASTOR440.hh"

#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4SubtractionSolid.hh"
#include "G4GeometryManager.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4RunManager.hh"
#include "G4SolidStore.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SDManager.hh"

#include "GeometryParallelBiasing.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::DetectorConstruction(G4String biasParticle)
{
    fPosition = G4ThreeVector(0., 0., 0.);
    fRotation = G4ThreeVector(0., 0., 0.);

    fWorldXYZ = 20. * m; // could need tweaking, as CASTOR 440's are pretty large (d2660 x 4080 mm3)

    DefineMaterials();
    fDetectorMessenger = new DetectorMessenger(this);

    // biasing
    if(biasParticle == "gamma" || biasParticle == "neutron") {
        G4cout << "[DetectorConstruction] Registering parallel world w. class GeometryParallelBiasing." << G4endl;
        RegisterParallelWorld(new GeometryParallelBiasing("BiasingWorld", this));
        fUseBiasingFromCLI = true;
    }
    else if(biasParticle.empty()){
        G4cout << "[DetectorConstruction] No biasing particle set, no parallel world registration." << G4endl;
        fUseBiasingFromCLI = false;
    }
    else {
        G4ExceptionDescription ed;
        ed << "Unknown biasing particle \"" << biasParticle << "\"";
        G4Exception("DetectorConstruction::DetectorConstruction",
                    "InvalidBiasingParticle", FatalException, ed);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorConstruction::~DetectorConstruction()
{
    delete fDetectorMessenger;

    for (auto clyc : fCLYCDetectors) {
        delete clyc;
    }
    for (auto rot : fCLYCRotations) {
        delete rot;
    }

    for (auto p : fPlasticDetectors) delete p;
    for (auto r : fPlasticRotations) delete r;

    for (auto castor : fCASTOR440Detectors) {
        delete castor;
    }
    for (auto rot : fCASTOR440Rotations) {
        delete rot;
    }

    for(auto h : fHemiShieldDetectors) delete h;
    for(auto h : fHemiShieldRotations) delete h;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::Construct()
{
    return ConstructVolumes();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::DefineMaterials()
{
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4VPhysicalVolume* DetectorConstruction::ConstructVolumes()
{
    // if mix-match biasing settings
    if((GetUseBiasing() && !GetUseBiasingFromCLI()) ||
       (!GetUseBiasing() && GetUseBiasingFromCLI())) {
        G4ExceptionDescription ed;
        ed << "Biasing needs to be on/off both:\n"
           << " 1) in the run macro via \"/dcs-monitor/det/useBiasing <true/false>\", and\n"
           << " 2) via the command-line via \"--biasing <gamma/neutron>\"";
        G4Exception("DetectorConstruction::ConstructVolumes",
                    "BiasingConfigError", FatalException, ed);
    }
    
    
    // Cleanup old geometry
    G4GeometryManager::GetInstance()->OpenGeometry();
    G4PhysicalVolumeStore::GetInstance()->Clean();
    G4LogicalVolumeStore::GetInstance()->Clean();
    G4SolidStore::GetInstance()->Clean();

    // materials
    G4NistManager* man = G4NistManager::Instance();
    G4Material * material; 

    // world volume
    G4Box* sWorld = new G4Box("sWorld", 
            fWorldXYZ/2., fWorldXYZ/2., fWorldXYZ/2.);
    material = man->FindOrBuildMaterial("G4_AIR");
    fLWorld = new G4LogicalVolume(sWorld, 
            material,
            material->GetName());
    fLWorld->SetVisAttributes(G4VisAttributes::GetInvisible());
    fPWorld = new G4PVPlacement(0,  // no rotation
            G4ThreeVector(),        // at (0,0,0)
            fLWorld,                // its logical volume
            "pWorld",               // its name
            0,                      // its mother  volume
            false,                  // no boolean operation
            0);                     // copy number

    // CLYC detector(s)
    // old method - fCLYCPositions stores either the front PE plug pos OR the crystal centroid pos
    //for (size_t i = 0; i < fCLYCDetectors.size(); ++i) {
    //    fCLYCDetectors[i]->Build();

    //    if (fCLYCPlaceByCrystalCenter[i]) {
    //        fCLYCDetectors[i]->PlaceDetectorByCrystalCenter(fLWorld, fCLYCPositions[i], fCLYCRotations[i], i);
    //    } else {
    //        fCLYCDetectors[i]->PlaceDetector(fLWorld, fCLYCPositions[i], fCLYCRotations[i], i);
    //    }
    //}
    // new method - fCLYCPositions ALWAYS stores the PE plug front pos, and the local offset to the crystal center is applied if 
    // we the bool flag for center placement has been set
    for (size_t i = 0; i < fCLYCDetectors.size(); ++i) {
        fCLYCDetectors[i]->Build();
    
        if (fCLYCPlaceByCrystalCenter[i]) {
            G4ThreeVector localCenter  = fCLYCDetectors[i]->GetCrystalCenterLocal();
            G4ThreeVector globalOffset = localCenter;
            if (fCLYCRotations[i]) globalOffset.transform(*fCLYCRotations[i]);
    
            // fCLYCPositions[i] currently holds the desired crystal centre.
            // Convert it in-place to the corresponding front-face position.
            fCLYCPositions[i] -= globalOffset;
        }
    
        // fCLYCPositions[i] is now guaranteed to be the front-face position.
        fCLYCDetectors[i]->PlaceDetector(fLWorld, fCLYCPositions[i], fCLYCRotations[i], i);
    }

    // Plastic detector(s)
    for (size_t i = 0; i < fPlasticDetectors.size(); ++i) {
        fPlasticDetectors[i]->Build();
        G4ThreeVector localOffset =
            fPlasticPlaceByCrystalCenter[i]
                ? fPlasticDetectors[i]->GetCrystalCenterLocal()
                : G4ThreeVector(0., 0., fPlasticDetectors[i]->GetFrontFaceLocalZ());

        G4ThreeVector globalOffset = localOffset;
        if (fPlasticRotations[i]) globalOffset.transform(*fPlasticRotations[i]);

        fPlasticPositions[i] -= globalOffset;   // convert requested anchor -> assembly origin
        fPlasticDetectors[i]->PlaceDetector(fLWorld, fPlasticPositions[i],
                                            fPlasticRotations[i], i);
    }


    // CASTOR 440 cask(s)
    for (size_t i = 0; i < fCASTOR440Detectors.size(); ++i) {
        fCASTOR440Detectors[i]->Build();
        fCASTOR440Detectors[i]->PlaceDetector(fLWorld, fCASTOR440Positions[i], fCASTOR440Rotations[i], i);

        fLCASTOR440s.push_back(fCASTOR440Detectors[i]->GetCASTORLog());
    }
 
    G4cout << " -> Gonna build/place " << (int)fHemiShieldDetectors.size() << " hemi-shields..." << std::flush;   
    for (size_t i = 0; i < fHemiShieldDetectors.size(); ++i) {
        G4cout << ", building " << i << std::flush;
        fHemiShieldDetectors[i]->Build();
        G4cout << ", done!, placing " << i << std::flush;
        fHemiShieldDetectors[i]->PlaceDetector(fLWorld, fHemiShieldPositions[i], fHemiShieldRotations[i], i);
        G4cout << ", done!" << std::flush;
    }
    G4cout << ", all done the hemi-stuff! " << G4endl;

    //PrintParameters();
    //G4cout << *(G4Material::GetMaterialTable()) << G4endl;

    // Quiet overlap check: verbose=false -> prints ONLY on overlap.
    {
        auto* pvStore = G4PhysicalVolumeStore::GetInstance();
        for (auto* pv : *pvStore) {
            if (pv) pv->CheckOverlaps(/*nStat=*/1000,
                                      /*tol=*/0.,
                                      /*verbose=*/false);
        }
    }

    // always return the root volume
    return fPWorld;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::ConstructSDandField()
{
    G4int detCounter = 0;   // global, first CLYC, then plastics

    if (!fCLYCDetectors.empty()) {
        auto* clycSD = new DCSMonitorSD("ClycSD", "DCSHitsCollection");
        G4SDManager::GetSDMpointer()->AddNewDetector(clycSD);
        clycSD->SetResolvingTime(1.0 * CLHEP::us);     // CLYC-appropriate
        for (auto clyc : fCLYCDetectors) {
            G4LogicalVolume* lv = clyc->GetCLYCLog();
            if (lv) {
                SetSensitiveDetector(lv, clycSD);
                clycSD->SetDetectorID(lv, detCounter++);
            }
        }
    }

    if (!fPlasticDetectors.empty()) {
        auto* plasticSD = new DCSMonitorSD("PlasticSD", "DCSHitsCollection");
        G4SDManager::GetSDMpointer()->AddNewDetector(plasticSD);
        plasticSD->SetResolvingTime(10.0 * CLHEP::ns); // fast plastic
        for (auto p : fPlasticDetectors) {
            G4LogicalVolume* lv = p->GetCrystalLog();
            if (lv) {
                SetSensitiveDetector(lv, plasticSD);
                plasticSD->SetDetectorID(lv, detCounter++);
            }
        }
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreeVector DetectorConstruction::GetCLYCCrystalPosition(G4int index) const
{
    if (index < 0 || index >= (G4int)fCLYCDetectors.size())
        return G4ThreeVector();

    // Local-frame offset of the crystal centre w.r.t. the assembly origin
    // (which, after the normalisation done in ConstructVolumes(), is exactly
    // what fCLYCPositions[index] represents in world coordinates).
    G4ThreeVector globalOffset = fCLYCDetectors[index]->GetCrystalCenterLocal();
    if (fCLYCRotations[index]) globalOffset.transform(*fCLYCRotations[index]);

    return fCLYCPositions[index] + globalOffset;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::AddCLYC() 
{
    fCLYCDetectors.push_back(new GeometryCLYC());
    fCLYCPositions.push_back(fPosition);
    
    G4RotationMatrix* rot = new G4RotationMatrix();
    rot->rotateX(fRotation.x()*M_PI/180.);
    rot->rotateY(fRotation.y()*M_PI/180.);
    rot->rotateZ(fRotation.z()*M_PI/180.); 
    fCLYCRotations.push_back(rot);
    
    fCLYCPlaceByCrystalCenter.push_back(false); // Flag for default placement
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorConstruction::AddCLYCByCrystalCenter() 
{
    fCLYCDetectors.push_back(new GeometryCLYC());
    fCLYCPositions.push_back(fPosition);
    
    G4RotationMatrix* rot = new G4RotationMatrix();
    rot->rotateX(fRotation.x()*M_PI/180.);
    rot->rotateY(fRotation.y()*M_PI/180.);
    rot->rotateZ(fRotation.z()*M_PI/180.); 
    fCLYCRotations.push_back(rot);
    
    fCLYCPlaceByCrystalCenter.push_back(true); // Flag for COM placement
}

void DetectorConstruction::SetCLYCCrystalRadius(G4double val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetCrystalRadius(val); }
void DetectorConstruction::SetCLYCCrystalLength(G4double val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetCrystalLength(val); }
void DetectorConstruction::SetCLYCCasingThickness(G4double val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetCasingThickness(val); }

void DetectorConstruction::SetCLYCLiFCollimatorInnerRadius(G4double val)    { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetLiFCollimatorInnerRadius(val); }
void DetectorConstruction::SetCLYCLiFCollimatorOuterRadius(G4double val)    { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetLiFCollimatorOuterRadius(val); }
void DetectorConstruction::SetCLYCLiFCollimatorLength(G4double val)         { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetLiFCollimatorLength(val); }

void DetectorConstruction::SetCLYCPbCollimatorInnerRadius(G4double val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPbCollimatorInnerRadius(val); }
void DetectorConstruction::SetCLYCPbCollimatorOuterRadius(G4double val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPbCollimatorOuterRadius(val); }
void DetectorConstruction::SetCLYCPbCollimatorLength(G4double val)      { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPbCollimatorLength(val); }

void DetectorConstruction::SetCLYCPECollimatorInnerRadius(G4double val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPECollimatorInnerRadius(val); }
void DetectorConstruction::SetCLYCPECollimatorOuterRadius(G4double val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPECollimatorOuterRadius(val); }
void DetectorConstruction::SetCLYCPECollimatorLength(G4double val)        { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPECollimatorLength(val); }

void DetectorConstruction::SetCLYCPEPlugLipRadius(G4double val)     { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEPlugLipRadius(val); }
void DetectorConstruction::SetCLYCPEPlugInnerRadius(G4double val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEPlugInnerRadius(val); }
void DetectorConstruction::SetCLYCPEPlugLipLength(G4double val)     { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEPlugLipLength(val); }
void DetectorConstruction::SetCLYCPEPlugInnerLength(G4double val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEPlugInnerLength(val); }

void DetectorConstruction::SetCLYCCrystalMaterialName(G4String val) { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetCrystalMaterialName(val); }
void DetectorConstruction::SetCLYCCasingMaterialName(G4String val)  { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetCasingMaterialName(val); }
void DetectorConstruction::SetCLYCLiFColMaterialName(G4String val)  { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetLiFColMaterialName(val); }
void DetectorConstruction::SetCLYCPbColMaterialName(G4String val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPbColMaterialName(val); }
void DetectorConstruction::SetCLYCPEColMaterialName(G4String val)   { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEColMaterialName(val); }
void DetectorConstruction::SetCLYCPEPlugMaterialName(G4String val)  { if (!fCLYCDetectors.empty()) fCLYCDetectors.back()->SetPEPlugMaterialName(val); }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

//....oooOO0OOooo  Plastic (shadowed) detector  oooOO0OOooo....

void DetectorConstruction::AddPlastic()
{
    fPlasticDetectors.push_back(new GeometryPlastic());
    fPlasticPositions.push_back(fPosition);
    G4RotationMatrix* rot = new G4RotationMatrix();
    rot->rotateX(fRotation.x()*M_PI/180.);
    rot->rotateY(fRotation.y()*M_PI/180.);
    rot->rotateZ(fRotation.z()*M_PI/180.);
    fPlasticRotations.push_back(rot);
    fPlasticPlaceByCrystalCenter.push_back(false);
}

void DetectorConstruction::AddPlasticByCrystalCenter()
{
    AddPlastic();
    fPlasticPlaceByCrystalCenter.back() = true;
}

#define PL fPlasticDetectors.back()
#define IFPL if (!fPlasticDetectors.empty())
void DetectorConstruction::SetPlasticCrystalRadius(G4double v){ IFPL PL->SetCrystalRadius(v); }
void DetectorConstruction::SetPlasticCrystalLength(G4double v){ IFPL PL->SetCrystalLength(v); }
void DetectorConstruction::SetPlasticCasingThickness(G4double v){ IFPL PL->SetCasingThickness(v); }
void DetectorConstruction::SetPlasticPbColInnerRadius(G4double v){ IFPL PL->SetPbColInnerRadius(v); }
void DetectorConstruction::SetPlasticPbColOuterRadius(G4double v){ IFPL PL->SetPbColOuterRadius(v); }
void DetectorConstruction::SetPlasticPbColLength(G4double v){ IFPL PL->SetPbColLength(v); }
void DetectorConstruction::SetPlasticPEColInnerRadius(G4double v){ IFPL PL->SetPEColInnerRadius(v); }
void DetectorConstruction::SetPlasticPEColOuterRadius(G4double v){ IFPL PL->SetPEColOuterRadius(v); }
void DetectorConstruction::SetPlasticPEColLength(G4double v){ IFPL PL->SetPEColLength(v); }
void DetectorConstruction::SetPlasticLiFColInnerRadius(G4double v){ IFPL PL->SetLiFColInnerRadius(v); }
void DetectorConstruction::SetPlasticLiFColOuterRadius(G4double v){ IFPL PL->SetLiFColOuterRadius(v); }
void DetectorConstruction::SetPlasticLiFColLength(G4double v){ IFPL PL->SetLiFColLength(v); }
void DetectorConstruction::SetPlasticShadowStandoff(G4double v){ IFPL PL->SetShadowStandoff(v); }
void DetectorConstruction::SetPlasticShadowRadiusDet(G4double v){ IFPL PL->SetShadowRadiusDet(v); }
void DetectorConstruction::SetPlasticShadowRadiusSrc(G4double v){ IFPL PL->SetShadowRadiusSrc(v); }
void DetectorConstruction::SetPlasticShadowBackLength(G4double v){ IFPL PL->SetShadowBackLength(v); }
void DetectorConstruction::SetPlasticShadowBodyLength(G4double v){ IFPL PL->SetShadowBodyLength(v); }
void DetectorConstruction::SetPlasticShadowFrontLength(G4double v){ IFPL PL->SetShadowFrontLength(v); }
void DetectorConstruction::SetPlasticSnoutInnerRadius(G4double v){ IFPL PL->SetSnoutInnerRadius(v); }
void DetectorConstruction::SetPlasticSnoutOuterRadius(G4double v){ IFPL PL->SetSnoutOuterRadius(v); }
void DetectorConstruction::SetPlasticSnoutLength(G4double v){ IFPL PL->SetSnoutLength(v); }
void DetectorConstruction::SetPlasticBackShieldRadius(G4double v){ IFPL PL->SetBackShieldRadius(v); }
void DetectorConstruction::SetPlasticBackShieldLength(G4double v){ IFPL PL->SetBackShieldLength(v); }
void DetectorConstruction::SetPlasticSideShieldInnerRadius(G4double v){ IFPL PL->SetSideShieldInnerRadius(v); }
void DetectorConstruction::SetPlasticSideShieldOuterRadius(G4double v){ IFPL PL->SetSideShieldOuterRadius(v); }
void DetectorConstruction::SetPlasticSideShieldLength(G4double v){ IFPL PL->SetSideShieldLength(v); }
void DetectorConstruction::SetPlasticCrystalMaterialName(G4String v){ IFPL PL->SetCrystalMaterialName(v); }
void DetectorConstruction::SetPlasticCasingMaterialName(G4String v){ IFPL PL->SetCasingMaterialName(v); }
void DetectorConstruction::SetPlasticPbColMaterialName(G4String v){ IFPL PL->SetPbColMaterialName(v); }
void DetectorConstruction::SetPlasticPEColMaterialName(G4String v){ IFPL PL->SetPEColMaterialName(v); }
void DetectorConstruction::SetPlasticLiFColMaterialName(G4String v){ IFPL PL->SetLiFColMaterialName(v); }
void DetectorConstruction::SetPlasticShadowBackMaterialName(G4String v){ IFPL PL->SetShadowBackMaterialName(v); }
void DetectorConstruction::SetPlasticShadowBodyMaterialName(G4String v){ IFPL PL->SetShadowBodyMaterialName(v); }
void DetectorConstruction::SetPlasticShadowFrontMaterialName(G4String v){ IFPL PL->SetShadowFrontMaterialName(v); }
void DetectorConstruction::SetPlasticSnoutMaterialName(G4String v){ IFPL PL->SetSnoutMaterialName(v); }
void DetectorConstruction::SetPlasticBackShieldMaterialName(G4String v){ IFPL PL->SetBackShieldMaterialName(v); }
void DetectorConstruction::SetPlasticSideShieldMaterialName(G4String v){ IFPL PL->SetSideShieldMaterialName(v); }
#undef PL
#undef IFPL

//....oooOO0OOooo  Plastic (shadowed) detector  oooOO0OOooo....

void DetectorConstruction::AddCASTOR440() 
{
    fCASTOR440Detectors.push_back(new GeometryCASTOR440());
    fCASTOR440Positions.push_back(fPosition);
    
    G4RotationMatrix* rot = new G4RotationMatrix();
    rot->rotateX(fRotation.x()*M_PI/180.);
    rot->rotateY(fRotation.y()*M_PI/180.);
    rot->rotateZ(fRotation.z()*M_PI/180.); 
    fCASTOR440Rotations.push_back(rot);
}

void DetectorConstruction::AddHemiShield() 
{
    fHemiShieldDetectors.push_back(new GeometryHemiShield());
    fHemiShieldPositions.push_back(fPosition);
    
    G4RotationMatrix* rot = new G4RotationMatrix();
    rot->rotateX(fRotation.x()*M_PI/180.);
    rot->rotateY(fRotation.y()*M_PI/180.);
    rot->rotateZ(fRotation.z()*M_PI/180.); 
    fHemiShieldRotations.push_back(rot);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// legacy helper method, no longer used
//G4ThreeVector DetectorConstruction::GetCASTOR440FuelGlobalPosition(G4int caskIndex,
//                                                                   G4int fuelIndex,
//                                                                   G4ThreeVector pointInFuel) const
//{
//    if (caskIndex < 0 || caskIndex >= (G4int)fCASTOR440Detectors.size())
//        return G4ThreeVector();
//
//    GeometryCASTOR440* cask = fCASTOR440Detectors[caskIndex];
//    G4ThreeVector fuelCenter = cask->GetFuelPosition(fuelIndex);
//
//    // Cavity Z offset taken directly from the cask geometry (no hard-coded -150 mm).
//    G4ThreeVector caskLocalPos = pointInFuel + fuelCenter
//                               + G4ThreeVector(0., 0., cask->GetCavityZOffsetInBody());
//
//    G4ThreeVector globalPos = caskLocalPos;
//    G4RotationMatrix* rot = fCASTOR440Rotations[caskIndex];
//    if (rot) globalPos.transform(*rot);
//
//    globalPos += fCASTOR440Positions[caskIndex];
//    return globalPos;
//}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreeVector DetectorConstruction::SampleUniformGlobalPositionInFuel(G4int caskIndex,
                                                                      G4int fuelIndex) const
{
    if (caskIndex < 0 || caskIndex >= (G4int)fCASTOR440Detectors.size())
        return G4ThreeVector();

    GeometryCASTOR440* cask = fCASTOR440Detectors[caskIndex];

    // Local point sampled inside the requested fuel assembly, expressed in
    // the cask body frame.
    G4ThreeVector localPos = cask->SampleUniformPointInFuel(fuelIndex);

    // Apply the per-cask global placement transform (rotation + translation).
    G4ThreeVector globalPos = localPos;
    G4RotationMatrix* rot = fCASTOR440Rotations[caskIndex];
    if (rot) globalPos.transform(*rot);

    globalPos += fCASTOR440Positions[caskIndex];
    return globalPos;
}

