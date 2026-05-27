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
    fSetCLYCCasingThicknessCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setCasingThickness", this);
    fSetCLYCCasingThicknessCmd->AvailableForStates(G4State_PreInit);
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
    fSetCLYCPEColInnerRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEColInnerRadius", this);
    fSetCLYCPEColInnerRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEColOuterRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEColOuterRadius", this);
    fSetCLYCPEColOuterRadiusCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEColLengthCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/det/clyc/setPEColLength", this);
    fSetCLYCPEColLengthCmd->AvailableForStates(G4State_PreInit);
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
    fSetCLYCCasingMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setCasingMaterial", this);
    fSetCLYCCasingMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCLiFColMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setLiFColMaterial", this);
    fSetCLYCLiFColMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPbColMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setPbColMaterial", this);
    fSetCLYCPbColMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEColMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setPEColMaterial", this);
    fSetCLYCPEColMaterialNameCmd->AvailableForStates(G4State_PreInit);
    fSetCLYCPEPlugMaterialNameCmd = new G4UIcmdWithAString("/dcs-monitor/det/clyc/setPEPlugMaterial", this);
    fSetCLYCPEPlugMaterialNameCmd->AvailableForStates(G4State_PreInit);
    
    // CASTOR 440
    fAddCASTOR440Cmd = new G4UIcmdWithoutParameter("/dcs-monitor/det/castor440/add", this);
    fAddCASTOR440Cmd->AvailableForStates(G4State_PreInit);

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
    delete fSetCLYCCasingThicknessCmd;
    // LiF collimator dims
    delete fSetCLYCLiFColInnerRadiusCmd;
    delete fSetCLYCLiFColOuterRadiusCmd;
    delete fSetCLYCLiFColLengthCmd;
    // Pb collimator dims
    delete fSetCLYCPbColInnerRadiusCmd;
    delete fSetCLYCPbColOuterRadiusCmd;
    delete fSetCLYCPbColLengthCmd;
    // PE collimator dims
    delete fSetCLYCPEColInnerRadiusCmd;
    delete fSetCLYCPEColOuterRadiusCmd;
    delete fSetCLYCPEColLengthCmd;
    // PE plug dims
    delete fSetCLYCPEPlugLipRadiusCmd;
    delete fSetCLYCPEPlugInnerRadiusCmd;
    delete fSetCLYCPEPlugLipLengthCmd;
    delete fSetCLYCPEPlugInnerLengthCmd;
    // materials
    delete fSetCLYCCrystalMaterialNameCmd;
    delete fSetCLYCCasingMaterialNameCmd;
    delete fSetCLYCLiFColMaterialNameCmd;
    delete fSetCLYCPbColMaterialNameCmd;
    delete fSetCLYCPEColMaterialNameCmd;
    delete fSetCLYCPEPlugMaterialNameCmd;
    
    // CASTOR 440
    delete fAddCASTOR440Cmd;
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
    
    if(command == fSetCLYCCasingThicknessCmd)       fDetector->SetCLYCCasingThickness(fSetCLYCCasingThicknessCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCLiFColInnerRadiusCmd)     fDetector->SetCLYCLiFCollimatorInnerRadius(fSetCLYCLiFColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCLiFColOuterRadiusCmd)     fDetector->SetCLYCLiFCollimatorOuterRadius(fSetCLYCLiFColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCLiFColLengthCmd)          fDetector->SetCLYCLiFCollimatorLength(fSetCLYCLiFColLengthCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCPbColInnerRadiusCmd)      fDetector->SetCLYCPbCollimatorInnerRadius(fSetCLYCPbColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPbColOuterRadiusCmd)      fDetector->SetCLYCPbCollimatorOuterRadius(fSetCLYCPbColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPbColLengthCmd)           fDetector->SetCLYCPbCollimatorLength(fSetCLYCPbColLengthCmd->GetNewDoubleValue(value));
    
    if(command == fSetCLYCPEColInnerRadiusCmd)      fDetector->SetCLYCPECollimatorInnerRadius(fSetCLYCPEColInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEColOuterRadiusCmd)      fDetector->SetCLYCPECollimatorOuterRadius(fSetCLYCPEColOuterRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEColLengthCmd)           fDetector->SetCLYCPECollimatorLength(fSetCLYCPEColLengthCmd->GetNewDoubleValue(value));

    if(command == fSetCLYCPEPlugLipRadiusCmd)       fDetector->SetCLYCPEPlugLipRadius(  fSetCLYCPEPlugLipRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugInnerRadiusCmd)     fDetector->SetCLYCPEPlugInnerRadius(fSetCLYCPEPlugInnerRadiusCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugLipLengthCmd)       fDetector->SetCLYCPEPlugLipLength(  fSetCLYCPEPlugLipLengthCmd->GetNewDoubleValue(value));
    if(command == fSetCLYCPEPlugInnerLengthCmd)     fDetector->SetCLYCPEPlugInnerLength(fSetCLYCPEPlugInnerLengthCmd->GetNewDoubleValue(value));

    if(command == fSetCLYCCrystalMaterialNameCmd)   fDetector->SetCLYCCrystalMaterialName(value);
    if(command == fSetCLYCCasingMaterialNameCmd)    fDetector->SetCLYCCasingMaterialName(value);
    if(command == fSetCLYCLiFColMaterialNameCmd)    fDetector->SetCLYCLiFColMaterialName(value);
    if(command == fSetCLYCPbColMaterialNameCmd)     fDetector->SetCLYCPbColMaterialName(value);
    if(command == fSetCLYCPEColMaterialNameCmd)     fDetector->SetCLYCPEColMaterialName(value);
    if(command == fSetCLYCPEPlugMaterialNameCmd)     fDetector->SetCLYCPEPlugMaterialName(value);
    
    // CASTOR 440
    if(command == fAddCASTOR440Cmd)                 fDetector->AddCASTOR440();

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


