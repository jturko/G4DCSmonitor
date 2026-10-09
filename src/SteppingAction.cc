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
#include "GeometryMuonScint.hh"
#include "OpticalDiagnostics.hh"

#include "G4HadronicProcess.hh"
#include "G4ParticleTypes.hh"
#include "G4RunManager.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "G4OpticalPhoton.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"

#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

#include <algorithm>
#include <cmath>
#include <string>


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

    G4ParticleDefinition* particle = aStep->GetTrack()->GetDefinition();
    G4String volName = aStep->GetPreStepPoint()->GetPhysicalVolume()->GetName();
    
    G4bool inShielding = (volName.find("CastorBody") != G4String::npos || 
                          volName.find("Cavity") != G4String::npos || 
                          volName.find("FuelPhys") != G4String::npos);
    if (inShielding) {
        // 1. Kill ALL Electrons and Positrons immediately (Perfectly fair to do)
        if (particle == G4Electron::Electron() || particle == G4Positron::Positron()) {
            aStep->GetTrack()->SetTrackStatus(fStopAndKill);
            return;
        }
        // kill gammas below a given threshold
        if (particle == G4Gamma::Gamma() && aStep->GetTrack()->GetKineticEnergy() < 50.0 * keV) {
            aStep->GetTrack()->SetTrackStatus(fStopAndKill);
            return;
        }
    }

    // Light-collection efficiency: record the primary track's first entry
    // position (slab-local) into each scintillator slab. Written to the
    // sipmHits tree (muonX/muonY) so the collection efficiency can be binned
    // offline. Done for the primary track only; relies on the first step
    // inside the slab's scintillator LV. Always recorded when a slab exists.
    if (fDetector && particle &&
        aStep->GetTrack()->GetTrackID() == 1) {
        G4VPhysicalVolume* prePV = aStep->GetPreStepPoint()->GetPhysicalVolume();
        if (prePV) {
            gOpticalDiag.EnsureSlabs(fDetector->GetNumMuonScints());
            const G4LogicalVolume* preLV = prePV->GetLogicalVolume();
            for (G4int i = 0; i < fDetector->GetNumMuonScints(); ++i) {
                GeometryMuonScint* ms = fDetector->GetMuonScint(i);
                if (!ms || preLV != ms->GetScintLV()) continue;

                if (!gOpticalDiag.muonEntrySet[i]) {
                    const G4VPhysicalVolume* slabPV = ms->GetScintPV();
                    if (slabPV) {
                        G4RotationMatrix rotInv;
                        const G4RotationMatrix* rr = slabPV->GetRotation();
                        if (rr) rotInv = rr->inverse();
                        const G4ThreeVector lp = rotInv *
                            (aStep->GetPreStepPoint()->GetPosition()
                             - slabPV->GetTranslation());
                        gOpticalDiag.muonEntryX[i]   = lp.x();
                        gOpticalDiag.muonEntryY[i]   = lp.y();
                        gOpticalDiag.muonEntrySet[i] = true;
                    }
                }
                break;
            }
        }
    }

    // Optical-photon accounting inside the MuonScint slab family
    // (slab + grease + SiPM + holder). Per-photon termination and generation
    // are accumulated here; the detected count is counted in the SiPM SD and
    // written -- together with the muon-entry x-y and produced count -- to the
    // sipmHits tree in MuonScintSiPMSD::EndOfEvent.
    if (particle == G4OpticalPhoton::OpticalPhoton()) {
        G4VPhysicalVolume* prePV  = aStep->GetPreStepPoint()->GetPhysicalVolume();
        G4VPhysicalVolume* postPV = aStep->GetPostStepPoint()->GetPhysicalVolume();

        // Generation: first step of a photon created inside the slab family.
        if (aStep->GetTrack()->GetCurrentStepNumber() == 1 &&
            IsInMuonScint(prePV)) {
            const G4VProcess* creator = aStep->GetTrack()->GetCreatorProcess();
            const G4String cname = creator ? creator->GetProcessName() : "";
            if (cname == "Scintillation")      ++gOpticalDiag.nScint;
            else if (cname == "Cerenkov")      ++gOpticalDiag.nCerenkov;

            // Per-slab "produced" counter: attribute the photon to the slab
            // whose scintillator LV created it. This is the denominator of the
            // collection efficiency and is always recorded (also emitted in
            // the sipmHits tree), independent of the optional flux map.
            if (fDetector && prePV) {
                gOpticalDiag.EnsureSlabs(fDetector->GetNumMuonScints());
                const G4LogicalVolume* preLV = prePV->GetLogicalVolume();
                for (G4int i = 0; i < fDetector->GetNumMuonScints(); ++i) {
                    GeometryMuonScint* ms = fDetector->GetMuonScint(i);
                    if (!ms || preLV != ms->GetScintLV()) continue;
                    ++gOpticalDiag.nProducedPerSlab[i];
                    break;
                }
            }
        }

        // Every optical photon ends with fStopAndKill (bulk/surface
        // absorption, photodetection, or escape). Count them all once; the
        // detection subset is counted authoritatively in MuonScintSiPMSD.
        if (aStep->GetTrack()->GetTrackStatus() == fStopAndKill) {
            ++gOpticalDiag.nKilled;
            if (!IsInMuonScint(prePV) && !IsInMuonScint(postPV)) {
                ++gOpticalDiag.nEscaped;
            }
        }

        // Optional per-slab optical-photon x-y flux/fluence map
        // (track-length estimator, slab-local frame). Only filled for steps
        // taken inside a scintillator slab's own logical volume. Histogram id
        // is resolved by name, so it is independent of booking order.
        if (RunAction::WriteOpticalFluxMap && fDetector && prePV) {
            const G4LogicalVolume* preLV = prePV->GetLogicalVolume();
            for (G4int i = 0; i < fDetector->GetNumMuonScints(); ++i) {
                GeometryMuonScint* ms = fDetector->GetMuonScint(i);
                if (!ms || preLV != ms->GetScintLV()) continue;

                const G4VPhysicalVolume* slabPV = ms->GetScintPV();
                const G4double stepLen = aStep->GetStepLength();
                if (!slabPV || stepLen <= 0.) break;

                // Global step endpoints -> slab-local coordinates.
                const G4ThreeVector trans = slabPV->GetTranslation();
                G4RotationMatrix rotInv;   // identity unless the slab is rotated
                const G4RotationMatrix* rr = slabPV->GetRotation();
                if (rr) rotInv = rr->inverse();

                const G4ThreeVector p0 = rotInv *
                    (aStep->GetPreStepPoint()->GetPosition()  - trans);
                const G4ThreeVector p1 = rotInv *
                    (aStep->GetPostStepPoint()->GetPosition() - trans);

                const G4double w = aStep->GetPreStepPoint()->GetWeight();

                const G4int h2id = analysis->GetH2Id(
                    "h2_optflux_xy_slab" + std::to_string(i));
                if (h2id < 0) break;

                // MUST match the cell size used in HistoManager.
                static const G4double kCell = 1. * CLHEP::mm;

                const G4double dx  = p1.x() - p0.x();
                const G4double dy  = p1.y() - p0.y();
                const G4double l2d = std::hypot(dx, dy);

                // one sub-fill per ~half cell of x-y travel (>=1)
                const G4int    nsub = std::max(1, (G4int)std::ceil(l2d / (0.5 * kCell)));
                const G4double dep  = stepLen * w / nsub;   // share 3-D length evenly
                for (G4int k = 0; k < nsub; ++k) {
                    const G4double f = (k + 0.5) / nsub;
                    analysis->FillH2(h2id, p0.x() + f * dx, p0.y() + f * dy, dep);
                }
                break;
            }
        }
    }

    // CASTOR 440 surface flux tracker
    if(RunAction::WriteCASTOR440SurfaceFluxTree) {
        for(G4int c=0; c<fDetector->GetNumCASTOR440s(); c++) {
            GeometryCASTOR440 * thisCask = fDetector->GetCASTOR440(c);
            G4LogicalVolume * preLV = aStep->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume();
            G4bool inBodyOrFin =  (preLV == thisCask->GetCASTORLog() ||
                                   preLV == thisCask->GetFinLog());

            if(inBodyOrFin && 
               aStep->GetPostStepPoint()->GetPhysicalVolume() == fDetector->GetWorld() &&
               (particle->GetPDGEncoding() == 22 || particle->GetPDGEncoding() == 2112)) // gammas and neutrons only
            {
                G4int idx = 2; // third  ntuple for castor_surf
                analysis->FillNtupleDColumn(idx, 0, particle->GetPDGEncoding());
                analysis->FillNtupleDColumn(idx, 1, aStep->GetPostStepPoint()->GetKineticEnergy());
                analysis->FillNtupleDColumn(idx, 2, aStep->GetPostStepPoint()->GetGlobalTime());
                analysis->FillNtupleDColumn(idx, 3, aStep->GetPostStepPoint()->GetPosition().x());
                analysis->FillNtupleDColumn(idx, 4, aStep->GetPostStepPoint()->GetPosition().y());
                analysis->FillNtupleDColumn(idx, 5, aStep->GetPostStepPoint()->GetPosition().z());
                analysis->FillNtupleDColumn(idx, 6, aStep->GetPostStepPoint()->GetMomentum().x());
                analysis->FillNtupleDColumn(idx, 7, aStep->GetPostStepPoint()->GetMomentum().y());
                analysis->FillNtupleDColumn(idx, 8, aStep->GetPostStepPoint()->GetMomentum().z());
                analysis->FillNtupleDColumn(idx, 9, aStep->GetPreStepPoint()->GetWeight());
                analysis->FillNtupleDColumn(idx,10, G4EventManager::GetEventManager()->GetConstCurrentEvent()->GetEventID());
                analysis->AddNtupleRow(idx);
            }
            break; //  can only exit one cask
        }
    }

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool SteppingAction::IsInMuonScint(const G4VPhysicalVolume* pv) const
{
    if (!pv || !fDetector) return false;
    const G4LogicalVolume* lv = pv->GetLogicalVolume();
    for (G4int i = 0; i < fDetector->GetNumMuonScints(); ++i) {
        GeometryMuonScint* ms = fDetector->GetMuonScint(i);
        if (!ms) continue;
        if (lv == ms->GetScintLV() ||
            lv == ms->GetGreaseLV() ||
            lv == ms->GetSiPMLV()  ||
            lv == ms->GetHolderLV()) {
            return true;
        }
    }
    return false;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


