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
/// \file SteppingAction.cc
/// \brief Implementation of the SteppingAction class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "SteppingAction.hh"
#include "HistoManager.hh"
#include "RunAction.hh"
#include "Run.hh"
#include "DetectorConstruction.hh"
#include "GeometryCASTOR440.hh"

#include "G4HadronicProcess.hh"
#include "G4ParticleTypes.hh"
#include "G4RunManager.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"

#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// SteppingAction::SteppingAction()
//: G4UserSteppingAction()
//{ }

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

SteppingAction::SteppingAction(DetectorConstruction* det) : G4UserSteppingAction(), fDetector(det)
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void SteppingAction::UserSteppingAction(const G4Step* aStep)
{
    G4AnalysisManager* analysis = G4AnalysisManager::Instance();
    
    // check trackID and stepNumber
    G4int trackID = aStep->GetTrack()->GetTrackID();
    G4int stepNb = aStep->GetTrack()->GetCurrentStepNumber();

    //// --- TEMPORARY DEBUG: WATCH THE WEIGHT WINDOWS ---
    //const G4VProcess* process = aStep->GetPostStepPoint()->GetProcessDefinedStep();
    //G4String procName = process ? process->GetProcessName() : "None";
    //if (procName == "ImportanceProcess") {
    //    G4double weight = aStep->GetTrack()->GetWeight();
    //    G4String volName = aStep->GetPreStepPoint()->GetPhysicalVolume()->GetName();
    //    
    //    G4cout << " [Biasing Triggered] " 
    //           << " Track: " << trackID 
    //           << " | Boundary: " << volName 
    //           << " | New Weight: " << weight 
    //           << G4endl;
    //}
    //// -------------------------------------------------
    
    //// --- TEMPORARY MONITOR FOR NEUTRON CAPTURE ---
    //const G4VProcess* process = aStep->GetPostStepPoint()->GetProcessDefinedStep();
    //G4String procName = process ? process->GetProcessName() : "None";
    //if(procName.find("nCapture") != G4String::npos ||
    //   procName.find("neutronInelastic") != G4String::npos ||
    //   procName.find("neutronHPInelastic") != G4String::npos) {
    //    G4Track* track = aStep->GetTrack();
    //    G4StepPoint* pre  = aStep->GetPreStepPoint();
    //    G4StepPoint* post = aStep->GetPostStepPoint();
    //    if(post->GetMaterial()->GetName() == "LiF") {
    //    //if(post->GetMaterial()->GetName() != "G4_POLYETHYLENE") {
    //    //if(1) {
    //        G4cout << "=== Neutron Capture ===" << G4endl;
    //        G4cout << "  TrackID      : " << track->GetTrackID() << G4endl;
    //        G4cout << "  ParentID     : " << track->GetParentID() << G4endl;
    //        G4cout << "  Kinetic E    : " << pre->GetKineticEnergy()/eV << " eV" << G4endl;
    //        G4cout << "  Position     : " << post->GetPosition()/cm << " cm" << G4endl;
    //        G4cout << "  Time         : " << post->GetGlobalTime()/ns << " ns" << G4endl;
    //        G4cout << "  Volume       : "
    //               << post->GetTouchableHandle()->GetVolume()->GetName() << G4endl;
    //        G4cout << "  Material     : "
    //               << post->GetMaterial()->GetName() << G4endl;
    //        G4cout << "  Process      : " << procName << G4endl;
    //    }
    //}
    //// -------------------------------------------------

    G4ParticleDefinition* particle = aStep->GetTrack()->GetDefinition();
    G4String volName = aStep->GetPreStepPoint()->GetPhysicalVolume()->GetName();
    
    G4bool inShielding = (volName.find("CastorBody") != G4String::npos || 
                          volName.find("Cavity") != G4String::npos || 
                          volName.find("FuelPhys") != G4String::npos);
    //if (inShielding) {
    //    // 1. Kill ALL Electrons and Positrons immediately (Perfectly fair to do)
    //    if (particle == G4Electron::Electron() || particle == G4Positron::Positron()) {
    //        aStep->GetTrack()->SetTrackStatus(fStopAndKill);
    //        return;
    //    }
    //    // kill gammas below a given threshold
    //    if (particle == G4Gamma::Gamma() && aStep->GetTrack()->GetKineticEnergy() < 50.0 * keV) {
    //        aStep->GetTrack()->SetTrackStatus(fStopAndKill);
    //        return;
    //    }
    //}

    //if(particle == G4NeutrinoE::NeutrinoE() ||
    //   particle == G4AntiNeutrinoE::AntiNeutrinoE() ||
    //   particle == G4NeutrinoMu::NeutrinoMu() ||
    //   particle == G4AntiNeutrinoMu::AntiNeutrinoMu() ||
    //   particle == G4NeutrinoTau::NeutrinoTau() ||
    //   particle == G4AntiNeutrinoTau::AntiNeutrinoTau()) {
    //    aStep->GetTrack()->SetTrackStatus(fStopAndKill);
    //}

    // CASTOR 440 surface flux tracker
    if(RunAction::WriteCASTOR440SurfaceFluxTree) {
        for(G4int c=0; c<fDetector->GetNumCASTOR440s(); c++) {
            GeometryCASTOR440 * thisCask = fDetector->GetCASTOR440(c);
            G4LogicalVolume * preLV = aStep->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume();
            G4LogicalVolume * postLV = nullptr;
            if(aStep->GetPostStepPoint()->GetPhysicalVolume() != nullptr)
                postLV = aStep->GetPostStepPoint()->GetPhysicalVolume()->GetLogicalVolume();
            G4bool inBodyOrFin =  (preLV == thisCask->GetCASTORLog() ||
                                   preLV == thisCask->GetFinLog());
            G4bool inBody      =  (preLV == thisCask->GetCASTORLog());
            G4bool inFinAfter  =  (postLV == thisCask->GetFinLog());
            G4bool inWorldAfter=  (aStep->GetPostStepPoint()->GetPhysicalVolume() == fDetector->GetWorld());

            
            if(inBody && // if pre-volume is the cask body, and...
               (inWorldAfter || inFinAfter) && // if post-volume is 1) world, or 2) a fin, and...
               (particle->GetPDGEncoding() == 22 || particle->GetPDGEncoding() == 2112)) // is a gamma or neutron
            {
                //// global coordinates
                //G4int idx = 2; // third  ntuple for castor_surf
                //analysis->FillNtupleDColumn(idx, 0, particle->GetPDGEncoding());
                //analysis->FillNtupleDColumn(idx, 1, aStep->GetPostStepPoint()->GetKineticEnergy());
                //analysis->FillNtupleDColumn(idx, 2, aStep->GetPostStepPoint()->GetGlobalTime());
                //analysis->FillNtupleDColumn(idx, 3, aStep->GetPostStepPoint()->GetPosition().x());
                //analysis->FillNtupleDColumn(idx, 4, aStep->GetPostStepPoint()->GetPosition().y());
                //analysis->FillNtupleDColumn(idx, 5, aStep->GetPostStepPoint()->GetPosition().z());
                //analysis->FillNtupleDColumn(idx, 6, aStep->GetPostStepPoint()->GetMomentum().x());
                //analysis->FillNtupleDColumn(idx, 7, aStep->GetPostStepPoint()->GetMomentum().y());
                //analysis->FillNtupleDColumn(idx, 8, aStep->GetPostStepPoint()->GetMomentum().z());
                //analysis->FillNtupleDColumn(idx, 9, aStep->GetPreStepPoint()->GetWeight());
                //analysis->FillNtupleDColumn(idx,10, G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID());
                //analysis->AddNtupleRow(idx);

                // local coordinates
                const G4ThreeVector    caskPos = fDetector->GetCASTOR440Position(c);
                const G4RotationMatrix* caskRot = fDetector->GetCASTOR440Rotation(c);
                G4RotationMatrix caskRotInv;                       // identity by default
                if (caskRot) caskRotInv = caskRot->inverse();
                const G4ThreeVector globalPos = aStep->GetPostStepPoint()->GetPosition();
                const G4ThreeVector globalMom = aStep->GetPostStepPoint()->GetMomentum();
                const G4ThreeVector localPos = caskRotInv * (globalPos - caskPos);
                const G4ThreeVector localMom = caskRotInv *  globalMom;
                G4int idx = 2; // "surfaceFlux" ntuple
                analysis->FillNtupleDColumn(idx, 0, particle->GetPDGEncoding());
                analysis->FillNtupleDColumn(idx, 1, aStep->GetPostStepPoint()->GetKineticEnergy());
                analysis->FillNtupleDColumn(idx, 2, aStep->GetPostStepPoint()->GetGlobalTime());
                analysis->FillNtupleDColumn(idx, 3, localPos.x());
                analysis->FillNtupleDColumn(idx, 4, localPos.y());
                analysis->FillNtupleDColumn(idx, 5, localPos.z());
                analysis->FillNtupleDColumn(idx, 6, localMom.x());
                analysis->FillNtupleDColumn(idx, 7, localMom.y());
                analysis->FillNtupleDColumn(idx, 8, localMom.z());
                analysis->FillNtupleDColumn(idx, 9, aStep->GetPreStepPoint()->GetWeight());
                analysis->FillNtupleDColumn(idx,10, G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID());
                analysis->AddNtupleRow(idx);

                // kill the particle after exiting
                aStep->GetTrack()->SetTrackStatus(fStopAndKill);

            }
            break; //  can only exit one cask
        }
    }

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


