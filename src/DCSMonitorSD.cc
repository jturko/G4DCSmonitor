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
//
/// \file B2/B2a/src/TrackerSD.cc
/// \brief Implementation of the B2::TrackerSD class

#include "DCSMonitorSD.hh"

#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4Track.hh"
#include "G4ParticleDefinition.hh"
#include "G4ThreeVector.hh"
#include "G4ios.hh"

#include "G4AnalysisManager.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DCSMonitorSD::DCSMonitorSD(const G4String& name, const G4String& hitsCollectionName)
    : G4VSensitiveDetector(name)
{
    collectionName.insert(hitsCollectionName);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DCSMonitorSD::Initialize(G4HCofThisEvent* hce)
{
    // Create hits collection
    fHitsCollection = new DCSMonitorHitsCollection(SensitiveDetectorName, collectionName[0]);

    // Add this collection in hce
    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
    hce->AddHitsCollection(hcID, fHitsCollection);

    fTrackHitIndexMap.clear();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool DCSMonitorSD::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    G4int trackID = step->GetTrack()->GetTrackID();
    G4int parentID = step->GetTrack()->GetParentID();
    G4int hitIndex = -1;

    // Detect if this track is crossing into the detector from the outside
    G4bool isEntering = (step->GetPreStepPoint()->GetStepStatus() == fGeomBoundary);

    // 1. Resolve Ancestry
    if (fTrackHitIndexMap.find(trackID) != fTrackHitIndexMap.end()) {
        // Track is already registered
        hitIndex = fTrackHitIndexMap[trackID];
    } else if (!isEntering && fTrackHitIndexMap.find(parentID) != fTrackHitIndexMap.end()) {
        // Track was created INSIDE the detector (physical secondary). Map to parent's hit.
        hitIndex = fTrackHitIndexMap[parentID];
        fTrackHitIndexMap[trackID] = hitIndex;
    }

    G4double w = step->GetPreStepPoint()->GetWeight(); 

    // 2. Create the "Shower Container" if no valid hit index exists
    if (hitIndex == -1) {
        auto newHit = new DCSMonitorHit();
        newHit->SetTrackID(trackID);
        newHit->SetEdep(0.); // Initialise to zero
        newHit->SetWeight(w);
        
        hitIndex = fHitsCollection->insert(newHit) - 1;
        fTrackHitIndexMap[trackID] = hitIndex;
    } 

    G4double edep = step->GetTotalEnergyDeposit();

    // 3. Record physical data ONLY when energy is deposited
    if (edep > 0.) {
        auto oldHit = (DCSMonitorHit*)fHitsCollection->GetHit(hitIndex);
        G4double t = step->GetPreStepPoint()->GetGlobalTime();
        
        if (oldHit->GetEdep() == 0.) {
            oldHit->SetTime(t);
            oldHit->SetPos(step->GetPostStepPoint()->GetPosition());
            oldHit->SetPID(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
        } 
        else if (t < oldHit->GetTime()) {
            oldHit->SetTime(t);
            oldHit->SetPos(step->GetPostStepPoint()->GetPosition());
            oldHit->SetPID(step->GetTrack()->GetParticleDefinition()->GetPDGEncoding());
        }

        oldHit->AddEdep(edep);
    }

    return true;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DCSMonitorSD::EndOfEvent(G4HCofThisEvent*)
{
    std::size_t nofHits = fHitsCollection->entries();
    
    if (verboseLevel > 1) {
        G4cout << G4endl << "-------->Hits Collection: in this event they are " << nofHits
            << " hits in the tracker chambers: " << G4endl;
        for (std::size_t i = 0; i < nofHits; i++)
            (*fHitsCollection)[i]->Print();
    }

    G4AnalysisManager* analysis = G4AnalysisManager::Instance();
    for (std::size_t i = 0; i < nofHits; i++) {
        G4double edep       = (*fHitsCollection)[i]->GetEdep();
        if(edep == 0.) continue; // skip hits which exist only due to the shower tracker,
                                 // which will generate a hit even for edep=0
        G4double pid        = (*fHitsCollection)[i]->GetPID();
        G4double t          = (*fHitsCollection)[i]->GetTime();
        G4ThreeVector pos   = (*fHitsCollection)[i]->GetPos();
        G4int det           = (*fHitsCollection)[i]->GetDetNum();
        G4double weight     = (*fHitsCollection)[i]->GetWeight();
        
        // 2nd ntuple is for detector hits
        G4int idx = 1;
        analysis->FillNtupleDColumn(idx, 0, pid);
        analysis->FillNtupleDColumn(idx, 1, edep);
        analysis->FillNtupleDColumn(idx, 2, t);
        analysis->FillNtupleDColumn(idx, 3, pos.x());
        analysis->FillNtupleDColumn(idx, 4, pos.y());
        analysis->FillNtupleDColumn(idx, 5, pos.z());
        analysis->FillNtupleIColumn(idx, 6, det);
        analysis->FillNtupleDColumn(idx, 7, weight);
        analysis->AddNtupleRow(idx);
    }

    fTrig = false;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

