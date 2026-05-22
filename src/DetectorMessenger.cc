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
/// \file DetectorMessenger.cc
/// \brief Implementation of the DetectorMessenger class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "DetectorMessenger.hh"

#include "DetectorConstruction.hh"

#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3Vector.hh"
#include "G4UIcmdWith3VectorAndUnit.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcommand.hh"
#include "G4UIdirectory.hh"
#include "G4UIparameter.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorMessenger::DetectorMessenger(DetectorConstruction* Det) : fDetector(Det)
{
    // directory
    fDir = new G4UIdirectory("/dcs-monitor/det/");
    fDir->SetGuidance("detector construction commands for DCS monitor project");
 
    // biasing
    fUseBiasingCmd = new G4UIcmdWithABool("/dcs-monitor/det/useBiasing", this);
    fUseBiasingCmd->SetGuidance("Toggle Geometry Importance Biasing (Weight Windows) on/off.");
    fUseBiasingCmd->SetParameterName("useBiasing", true);
    fUseBiasingCmd->SetDefaultValue(true);
    fUseBiasingCmd->AvailableForStates(G4State_PreInit);
    
    fSetNShellsCmd = new G4UIcmdWithAnInteger("/dcs-monitor/det/setBiasingShells", this);
    fSetNShellsCmd->SetGuidance("Number of importance-biasing shells (radial & axial thickness uniform).");
    fSetNShellsCmd->SetGuidance("Max importance = 2^N. Applies to both gamma and neutron biasing.");
    fSetNShellsCmd->SetParameterName("nShells", false);
    fSetNShellsCmd->SetRange("nShells >= 1 && nShells <= 30");
    fSetNShellsCmd->AvailableForStates(G4State_PreInit);

    fSetBiasRMinCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/setBiasingInnerRadius", this);
    fSetBiasRMinCmd->SetGuidance("Inner (core) radius of the biasing shells.");
    fSetBiasRMinCmd->SetDefaultUnit("mm");
    fSetBiasRMinCmd->AvailableForStates(G4State_PreInit);

    fSetBiasRMaxCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/setBiasingOuterRadius", this);
    fSetBiasRMaxCmd->SetGuidance("Outer radius of the outermost biasing shell.");
    fSetBiasRMaxCmd->SetDefaultUnit("mm");
    fSetBiasRMaxCmd->AvailableForStates(G4State_PreInit);

    fSetBiasHMinCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/setBiasingInnerHeight", this);
    fSetBiasHMinCmd->SetGuidance("Full axial height of the biasing core (should span the source region).");
    fSetBiasHMinCmd->SetDefaultUnit("mm");
    fSetBiasHMinCmd->AvailableForStates(G4State_PreInit);

    // placement 
    fSetPositionCmd = new G4UIcmdWith3VectorAndUnit("/dcs-monitor/det/setPosition",this);
    fSetPositionCmd->SetGuidance("set the position of the next volume");
    fSetPositionCmd->SetDefaultUnit("mm");
    //fSetPositionCmd->AvailableForStates(G4State_Idle);
    
    fSetRotationCmd = new G4UIcmdWith3Vector("/dcs-monitor/det/setRotation",this);
    fSetRotationCmd->SetGuidance("set the rotation of the next volume (in degrees)");
    //fSetRotationCmd->AvailableForStates(G4State_Idle);
    
    // DCS monitor
    // CLYC detector 
    fAddCLYCCmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/clyc/add", this);
    fAddCLYCCmd->AvailableForStates(G4State_PreInit);
    fAddCLYCByCrystalCenterCmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/clyc/addByCrystalCenter", this);
    fAddCLYCByCrystalCenterCmd->AvailableForStates(G4State_PreInit);
    // crystal
    fSetCLYCCrystalRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setCrystalRadius", this);
    fSetCLYCCrystalRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCCrystalLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setCrystalLength", this);
    fSetCLYCCrystalLengthCmd->AvailableForStates(G4State_PreInit);
    // aluminum casing
    fSetCLYCAlumThicknessCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setAlumThickness", this);
    fSetCLYCAlumThicknessCmd->AvailableForStates(G4State_PreInit);
    // LiF collimator liner
    fSetCLYCLiFColInnerRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setLiFColInnerRadius", this);
    fSetCLYCLiFColInnerRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCLiFColOuterRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setLiFColOuterRadius", this);
    fSetCLYCLiFColOuterRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCLiFColLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setLiFColLength", this);
    fSetCLYCLiFColLengthCmd->AvailableForStates(G4State_PreInit);
     // Pb collimator
    fSetCLYCPbColInnerRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPbColInnerRadius", this);
    fSetCLYCPbColInnerRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPbColOuterRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPbColOuterRadius", this);
    fSetCLYCPbColOuterRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPbColLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPbColLength", this);
    fSetCLYCPbColLengthCmd->AvailableForStates(G4State_PreInit);
    // PE collimator
    fSetCLYCPEHDColInnerRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEHDColInnerRadius", this);
    fSetCLYCPEHDColInnerRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEHDColOuterRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEHDColOuterRadius", this);
    fSetCLYCPEHDColOuterRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEHDColLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEHDColLength", this);
    fSetCLYCPEHDColLengthCmd->AvailableForStates(G4State_PreInit);
    // PE plug
    fSetCLYCPEPlugLipRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEPlugLipRadius", this);
    fSetCLYCPEPlugLipRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEPlugInnerRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEPlugInnerRadius", this);
    fSetCLYCPEPlugInnerRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEPlugLipLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEPlugLipLength", this);
    fSetCLYCPEPlugLipLengthCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEPlugInnerLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEPlugInnerLength", this);
    fSetCLYCPEPlugInnerLengthCmd->AvailableForStates(G4State_PreInit);
    // materials
    fSetCLYCCrystalMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setCrystalMaterial", this);
    fSetCLYCCrystalMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCAlumMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setAlumMaterial", this);
    fSetCLYCAlumMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCLiFMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setLiFMaterial", this);
    fSetCLYCLiFMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPbMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setPbMaterial", this);
    fSetCLYCPbMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEHDMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setPEHDMaterial", this);
    fSetCLYCPEHDMaterialNameCmd->AvailableForStates(G4State_PreInit);
    
    // CASTOR 440
    fAddCASTOR440Cmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/castor440/add", this);
    fAddCASTOR440Cmd->AvailableForStates(G4State_PreInit);





    // -----------------------------------------------------------------
    // muon scintillator
    // -----------------------------------------------------------------
    fAddMuonScintCmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/muonScint/add", this);
    fAddMuonScintCmd->SetGuidance("Instantiate a plastic scintillator slab at the current "
                                  "position/rotation. Each call creates a new slab.");
    fAddMuonScintCmd->AvailableForStates(G4State_PreInit);

    fSetMuonScintSizeCmd = new G4UIcmdWith3VectorAndUnit("/dcs-monitor/det/muonScint/setSize", this);
    fSetMuonScintSizeCmd->SetGuidance("Set the HALF-lengths (Lx/2, Ly/2, Lz/2) of the slab. "
                                     "Default 100 100 5 mm = 200x200x10 mm^3.");
    fSetMuonScintSizeCmd->SetParameterName("hx", "hy", "hz", false);
    fSetMuonScintSizeCmd->SetDefaultUnit("mm");
    fSetMuonScintSizeCmd->AvailableForStates(G4State_PreInit);

    fSetMuonScintReflectorCmd = new G4UIcmdWithAnInteger("/dcs-monitor/det/muonScint/setReflector", this);
    fSetMuonScintReflectorCmd->SetGuidance("Reflector type:");
    fSetMuonScintReflectorCmd->SetGuidance("  0 = none (bare polished)");
    fSetMuonScintReflectorCmd->SetGuidance("  1 = EJ-510 TiO2 paint  (Lambertian, ~97%)");
    fSetMuonScintReflectorCmd->SetGuidance("  2 = aluminium foil      (specular,   ~90%)");
    fSetMuonScintReflectorCmd->SetGuidance("  3 = high-gloss paper    (mixed,      ~95%)");
    fSetMuonScintReflectorCmd->SetGuidance("  4 = PTFE / Teflon       (Lambertian, ~99%)");
    fSetMuonScintReflectorCmd->SetParameterName("type", false);
    fSetMuonScintReflectorCmd->SetRange("type >= 0 && type <= 4");
    fSetMuonScintReflectorCmd->AvailableForStates(G4State_PreInit);

    fSetMuonScintWrapCmd = new G4UIcmdWithABool("/dcs-monitor/det/muonScint/setWrap", this);
    fSetMuonScintWrapCmd->SetGuidance("Wrap the slab in the selected reflector (true/false).");
    fSetMuonScintWrapCmd->SetParameterName("wrap", true);
    fSetMuonScintWrapCmd->SetDefaultValue(true);
    fSetMuonScintWrapCmd->AvailableForStates(G4State_PreInit);

    fSetMuonScintSiPMSizeCmd = new G4UIcmdWith3Vector("/dcs-monitor/det/muonScint/setSiPMSize", this);
    fSetMuonScintSiPMSizeCmd->SetGuidance("SiPM active-area size (sx, sy) in mm. The third component "
                                         "is ignored (thickness is fixed internally).");
    fSetMuonScintSiPMSizeCmd->SetParameterName("sx_mm", "sy_mm", "ignored", false);
    fSetMuonScintSiPMSizeCmd->AvailableForStates(G4State_PreInit);

    fAddMuonScintSiPMCmd = new G4UIcmdWith3Vector("/dcs-monitor/det/muonScint/addSiPM", this);
    fAddMuonScintSiPMCmd->SetGuidance("Add a SiPM at (edge, u, v):");
    fAddMuonScintSiPMCmd->SetGuidance("  edge: 0=+X, 1=-X, 2=+Y, 3=-Y    (the 4 narrow side faces)");
    fAddMuonScintSiPMCmd->SetGuidance("  u: position along the long axis of the edge, in [-1, +1]");
    fAddMuonScintSiPMCmd->SetGuidance("  v: position across the thickness, in [-1, +1] (usually 0)");
    fAddMuonScintSiPMCmd->SetParameterName("edge", "u", "v", false);
    fAddMuonScintSiPMCmd->AvailableForStates(G4State_PreInit);

    fClearMuonScintSiPMsCmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/muonScint/clearSiPMs", this);
    fClearMuonScintSiPMsCmd->SetGuidance("Remove all SiPMs from the most-recently-added slab.");
    fClearMuonScintSiPMsCmd->AvailableForStates(G4State_PreInit);

    fApplyMuonScintPresetCmd = new G4UIcmdWithAnInteger("/dcs-monitor/det/muonScint/preset", this);
    fApplyMuonScintPresetCmd->SetGuidance("Apply a preset SiPM layout (Configs from Rao 2025):");
    fApplyMuonScintPresetCmd->SetGuidance("  1 = Config 1: 8 SiPMs (2 per side, all 4 sides)");
    fApplyMuonScintPresetCmd->SetGuidance("  2 = Config 2: 4 SiPMs (1 per side, all 4 sides)");
    fApplyMuonScintPresetCmd->SetGuidance("  3 = Config 3: 2 SiPMs (both on the +X side)");
    fApplyMuonScintPresetCmd->SetGuidance("  4 = Corners:  4 SiPMs (one at each corner of +X/-X edges)");
    fApplyMuonScintPresetCmd->SetParameterName("preset", false);
    fApplyMuonScintPresetCmd->SetRange("preset >= 1 && preset <= 4");
    fApplyMuonScintPresetCmd->AvailableForStates(G4State_PreInit);


}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DetectorMessenger::~DetectorMessenger()
{
    // directory
    delete fDir;
 
    // biasing   
    delete fUseBiasingCmd;
    delete fSetNShellsCmd;
    delete fSetBiasRMinCmd;
    delete fSetBiasRMaxCmd;
    delete fSetBiasHMinCmd;

    // pos/rot
    delete fSetPositionCmd;
    delete fSetRotationCmd;
    
    // DCS monitor
    // adders
    delete fAddCLYCCmd;
    delete fAddCLYCByCrystalCenterCmd;
    // crystal dims
    delete fSetCLYCCrystalRadiusCmd;
    delete fSetCLYCCrystalLengthCmd;
    // alum dims
    delete fSetCLYCAlumThicknessCmd;
    // LiF collimator dims
    delete fSetCLYCLiFColInnerRadiusCmd;
    delete fSetCLYCLiFColOuterRadiusCmd;
    delete fSetCLYCLiFColLengthCmd;
    // Pb collimator dims
    delete fSetCLYCPbColInnerRadiusCmd;
    delete fSetCLYCPbColOuterRadiusCmd;
    delete fSetCLYCPbColLengthCmd;
    // PE collimator dims
    delete fSetCLYCPEHDColInnerRadiusCmd;
    delete fSetCLYCPEHDColOuterRadiusCmd;
    delete fSetCLYCPEHDColLengthCmd;
    // PE plug dims
    delete fSetCLYCPEPlugLipRadiusCmd;
    delete fSetCLYCPEPlugInnerRadiusCmd;
    delete fSetCLYCPEPlugLipLengthCmd;
    delete fSetCLYCPEPlugInnerLengthCmd;
    // materials
    delete fSetCLYCCrystalMaterialNameCmd;
    delete fSetCLYCAlumMaterialNameCmd;
    delete fSetCLYCLiFMaterialNameCmd;
    delete fSetCLYCPbMaterialNameCmd;
    delete fSetCLYCPEHDMaterialNameCmd;
    
    // CASTOR 440
    delete fAddCASTOR440Cmd; 


    // muon scintillator
    delete fAddMuonScintCmd;
    delete fSetMuonScintSizeCmd;
    delete fSetMuonScintReflectorCmd;
    delete fSetMuonScintWrapCmd;
    delete fSetMuonScintSiPMSizeCmd;
    delete fAddMuonScintSiPMCmd;
    delete fClearMuonScintSiPMsCmd;
    delete fApplyMuonScintPresetCmd;

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DetectorMessenger::SetNewValue(G4UIcommand* command, G4String value)
{
    // biasing
    if (command == fUseBiasingCmd)
        fDetector->SetUseBiasing(fUseBiasingCmd->GetNewBoolValue(value));
    if (command == fSetNShellsCmd)
        fDetector->SetNShells(fSetNShellsCmd->GetNewIntValue(value));
    if (command == fSetBiasRMinCmd)
        fDetector->SetBiasingInnerRadius(fSetBiasRMinCmd->GetNewDoubleValue(value));
    if (command == fSetBiasRMaxCmd)
        fDetector->SetBiasingOuterRadius(fSetBiasRMaxCmd->GetNewDoubleValue(value));
    if (command == fSetBiasHMinCmd)
        fDetector->SetBiasingInnerHeight(fSetBiasHMinCmd->GetNewDoubleValue(value));

    // placement
    if(command == fSetPositionCmd) 
        fDetector->SetPosition(fSetPositionCmd->GetNew3VectorValue(value));
    if(command == fSetRotationCmd) 
        fDetector->SetRotation(fSetRotationCmd->GetNew3VectorValue(value));
    
    // DCS monitor
    if(command == fAddCLYCCmd)                      fDetector->AddCLYC();
    if(command == fAddCLYCByCrystalCenterCmd)       fDetector->AddCLYCByCrystalCenter();

    if(command == fSetCLYCCrystalRadiusCmd)         fDetector->SetCLYCCrystalRadius(fSetCLYCCrystalRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCCrystalLengthCmd)         fDetector->SetCLYCCrystalLength(fSetCLYCCrystalLengthCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCAlumThicknessCmd)         fDetector->SetCLYCAlumThickness(fSetCLYCAlumThicknessCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCLiFColInnerRadiusCmd)     fDetector->SetCLYCLiFCollimatorInnerRadius(fSetCLYCLiFColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCLiFColOuterRadiusCmd)     fDetector->SetCLYCLiFCollimatorOuterRadius(fSetCLYCLiFColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCLiFColLengthCmd)          fDetector->SetCLYCLiFCollimatorLength(fSetCLYCLiFColLengthCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCPbColInnerRadiusCmd)      fDetector->SetCLYCPbCollimatorInnerRadius(fSetCLYCPbColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPbColOuterRadiusCmd)      fDetector->SetCLYCPbCollimatorOuterRadius(fSetCLYCPbColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPbColLengthCmd)           fDetector->SetCLYCPbCollimatorLength(fSetCLYCPbColLengthCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCPEHDColInnerRadiusCmd)    fDetector->SetCLYCPEHDCollimatorInnerRadius(fSetCLYCPEHDColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEHDColOuterRadiusCmd)    fDetector->SetCLYCPEHDCollimatorOuterRadius(fSetCLYCPEHDColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEHDColLengthCmd)         fDetector->SetCLYCPEHDCollimatorLength(fSetCLYCPEHDColLengthCmd->GetNewDoubleValue(value));

    if(command == fSetCLYCPEPlugLipRadiusCmd)       fDetector->SetCLYCPEPlugLipRadius(  fSetCLYCPEPlugLipRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugInnerRadiusCmd)     fDetector->SetCLYCPEPlugInnerRadius(fSetCLYCPEPlugInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugLipLengthCmd)       fDetector->SetCLYCPEPlugLipLength(  fSetCLYCPEPlugLipLengthCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugInnerLengthCmd)     fDetector->SetCLYCPEPlugInnerLength(fSetCLYCPEPlugInnerLengthCmd->GetNewDoubleValue(value));

    if(command == fSetCLYCCrystalMaterialNameCmd)   fDetector->SetCLYCCrystalMaterialName(value);
    if(command == fSetCLYCAlumMaterialNameCmd)      fDetector->SetCLYCAlumMaterialName(value);
    if(command == fSetCLYCLiFMaterialNameCmd)       fDetector->SetCLYCLiFMaterialName(value);
    if(command == fSetCLYCPbMaterialNameCmd)        fDetector->SetCLYCPbMaterialName(value);
    if(command == fSetCLYCPEHDMaterialNameCmd)      fDetector->SetCLYCPEHDMaterialName(value);
    
    // CASTOR 440
    if(command == fAddCASTOR440Cmd)                 fDetector->AddCASTOR440();



    // muon scintillator
    if (command == fAddMuonScintCmd)             fDetector->AddMuonScint();
    if (command == fSetMuonScintSizeCmd)         fDetector->SetMuonScintSize(fSetMuonScintSizeCmd->GetNew3VectorValue(value));
    if (command == fSetMuonScintReflectorCmd)    fDetector->SetMuonScintReflector(fSetMuonScintReflectorCmd->GetNewIntValue(value));
    if (command == fSetMuonScintWrapCmd)         fDetector->SetMuonScintWrap(fSetMuonScintWrapCmd->GetNewBoolValue(value));
    if (command == fSetMuonScintSiPMSizeCmd) {
        G4ThreeVector v = fSetMuonScintSiPMSizeCmd->GetNew3VectorValue(value);
        fDetector->SetMuonScintSiPMSize(v.x() * CLHEP::mm, v.y() * CLHEP::mm);
    }
    if (command == fAddMuonScintSiPMCmd) {
        G4ThreeVector v = fAddMuonScintSiPMCmd->GetNew3VectorValue(value);
        fDetector->AddMuonScintSiPM((G4int)v.x(), v.y(), v.z());
    }
    if (command == fClearMuonScintSiPMsCmd)      fDetector->ClearMuonScintSiPMs();
    if (command == fApplyMuonScintPresetCmd)     fDetector->ApplyMuonScintPreset(fApplyMuonScintPresetCmd->GetNewIntValue(value));


}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


