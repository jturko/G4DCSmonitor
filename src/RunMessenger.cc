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
/// \file RunMessenger.cc
/// \brief Implementation of the RunMessenger class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "RunMessenger.hh"
#include "RunAction.hh"

#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIdirectory.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunMessenger::RunMessenger(RunAction* run) : fRun(run)
{
    fRunDir = new G4UIdirectory("/dcs-monitor/run/");
    fRunDir->SetGuidance("run commands for the DCS monitor simulation");
    
    fWritePrimaryCmd = new G4UIcmdWithABool("/dcs-monitor/run/writePrimary", this);
    fWritePrimaryCmd->SetGuidance("Toggle filling of the primary vertex ntuple");
    fWritePrimaryCmd->SetParameterName("write", true);
    fWritePrimaryCmd->SetDefaultValue(true);
    fWritePrimaryCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    fWritePrimaryOnlyOnHitCmd = new G4UIcmdWithABool("/dcs-monitor/run/writePrimaryOnlyOnHit", this);
    fWritePrimaryOnlyOnHitCmd->SetGuidance("Toggle filling of the primary vertex ntuple, which is only done if a sensitive detector is triggered (edep>0)");
    fWritePrimaryOnlyOnHitCmd->SetParameterName("write", true);
    fWritePrimaryOnlyOnHitCmd->SetDefaultValue(true);
    fWritePrimaryOnlyOnHitCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    fWriteCASTOR440SurfaceFluxCmd = new G4UIcmdWithABool("/dcs-monitor/run/writeSurfaceFlux", this);
    fWriteCASTOR440SurfaceFluxCmd->SetGuidance("Toggle filling of the CASTOR 440 surface flux ntuple");
    fWriteCASTOR440SurfaceFluxCmd->SetParameterName("write", true);
    fWriteCASTOR440SurfaceFluxCmd->SetDefaultValue(true);
    fWriteCASTOR440SurfaceFluxCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunMessenger::~RunMessenger()
{
    delete fRunDir;
    delete fWritePrimaryCmd;
    delete fWriteCASTOR440SurfaceFluxCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if (command == fWritePrimaryCmd) {
        RunAction::WritePrimaryTree = fWritePrimaryCmd->GetNewBoolValue(newValue);
    }

    if (command == fWritePrimaryOnlyOnHitCmd) {
        RunAction::WritePrimaryTreeOnlyOnHit = fWritePrimaryOnlyOnHitCmd->GetNewBoolValue(newValue);
    }

    if (command == fWriteCASTOR440SurfaceFluxCmd) {
        RunAction::WriteCASTOR440SurfaceFluxTree = fWriteCASTOR440SurfaceFluxCmd->GetNewBoolValue(newValue);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
