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
/// \file HistoManager.cc
/// \brief Implementation of the HistoManager class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "HistoManager.hh"

#include "RootManager.hh"
#include "DetectorConstruction.hh"
#include "GeometryMuonScint.hh"

#include "G4UnitsTable.hh"

#include <algorithm>
#include <cmath>
#include <sstream>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

HistoManager::HistoManager(DetectorConstruction* detector, G4double cellSize)
    : fDetector(detector),
      fCellSize(cellSize > 0. ? cellSize : 1.)
{
    Book();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void HistoManager::Book()
{
    // Create or get analysis manager
    // The choice of analysis technology is done via selection of a namespace
    // in HistoManager.hh
    G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
    analysisManager->SetDefaultFileType("root");
    analysisManager->SetFileName(fFileName);
    analysisManager->SetVerboseLevel(1);
    //analysisManager->SetActivation(true);  // enable inactivation of histograms
    analysisManager->SetNtupleMerging(true);

    // check if file number set
    RootManager& rootManager = RootManager::GetInstance();
    if(rootManager.GetFileNum() >= 0) {
        std::ostringstream oss;
        oss << fFileName << "_" << std::setw(6) << std::setfill('0') << rootManager.GetFileNum();
        analysisManager->SetFileName(oss.str());
    }

    G4int idx;

    // ntuple for primary particles
    idx = analysisManager->CreateNtuple("primary", "tree of primary particles");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");
    analysisManager->CreateNtupleDColumn("ekin");
    analysisManager->CreateNtupleDColumn("t");
    analysisManager->CreateNtupleDColumn("x");
    analysisManager->CreateNtupleDColumn("y");
    analysisManager->CreateNtupleDColumn("z");
    analysisManager->CreateNtupleDColumn("px");
    analysisManager->CreateNtupleDColumn("py");
    analysisManager->CreateNtupleDColumn("pz");
    analysisManager->FinishNtuple();
    //G4cout << " Created ntuple \"primary\" (id " << idx << ")" << G4endl;
    
    // ntuple for detector hits
    idx = analysisManager->CreateNtuple("hits", "tree of sensitive detector hits");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");
    analysisManager->CreateNtupleDColumn("edep");
    analysisManager->CreateNtupleDColumn("t");
    analysisManager->CreateNtupleDColumn("x");
    analysisManager->CreateNtupleDColumn("y");
    analysisManager->CreateNtupleDColumn("z");
    analysisManager->CreateNtupleIColumn("det");
    analysisManager->CreateNtupleDColumn("weight");
    analysisManager->FinishNtuple();
    //G4cout << " Created ntuple \"hits\" (id " << idx << ")" << G4endl;
    
    // ntuple for CASTOR 440 surface tracker
    idx = analysisManager->CreateNtuple("surfaceFlux", "tree of CASTOR 440 surface flux (particles leaving the cask)");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");
    analysisManager->CreateNtupleDColumn("ekin");
    analysisManager->CreateNtupleDColumn("t");
    analysisManager->CreateNtupleDColumn("x");
    analysisManager->CreateNtupleDColumn("y");
    analysisManager->CreateNtupleDColumn("z");
    analysisManager->CreateNtupleDColumn("px");
    analysisManager->CreateNtupleDColumn("py");
    analysisManager->CreateNtupleDColumn("pz");
    analysisManager->CreateNtupleDColumn("weight");
    analysisManager->CreateNtupleDColumn("evtNb");
    analysisManager->FinishNtuple();
    //G4cout << " Created ntuple \"castor_surf\" (id " << idx << ")" << G4endl;
 
    // ntuple for MuonScint SiPM hits (one row per (event, SiPM), including
    // zero-detection SiPMs, so collection efficiency can be reconstructed).
    // muonX/muonY are the primary muon's first-entry position in the slab
    // (slab-local, mm) and nProduced is the event's total optical photons
    // produced in that slab -- the denominator for the collection efficiency
    // (nDetected / nProduced), binned by interaction x-y offline.
    idx = analysisManager->CreateNtuple("sipmHits", "tree of detected optical photon counts in MuonScint SiPMs");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleIColumn("evtNb");
    analysisManager->CreateNtupleIColumn("det");
    analysisManager->CreateNtupleIColumn("sipm");
    analysisManager->CreateNtupleIColumn("nDetected");
    analysisManager->CreateNtupleDColumn("tFirst");
    analysisManager->CreateNtupleDColumn("meanWavelength_nm");
    analysisManager->CreateNtupleDColumn("rmsWavelength_nm");
    analysisManager->CreateNtupleDColumn("weight");
    analysisManager->CreateNtupleDColumn("muonX");
    analysisManager->CreateNtupleDColumn("muonY");
    analysisManager->CreateNtupleIColumn("nProduced");
    analysisManager->FinishNtuple();
    //G4cout << " Created ntuple \"sipmHits\" (id " << idx << ")" << G4endl;

    // ntuple for optical-photon accounting diagnostics (one row per event)
    idx = analysisManager->CreateNtuple("opticalStats", "per-event optical photon accounting in the MuonScint slab");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleIColumn("evtNb");
    analysisManager->CreateNtupleIColumn("nScint");     // scintillation photons generated in slab
    analysisManager->CreateNtupleIColumn("nCerenkov");  // Cherenkov photons generated in slab
    analysisManager->CreateNtupleIColumn("nKilled");    // optical photons that reached fStopAndKill
    analysisManager->CreateNtupleIColumn("nDetected");  // detected photoelectrons (all SiPMs)
    analysisManager->CreateNtupleIColumn("nEscaped");   // killed while outside the detector
    analysisManager->CreateNtupleIColumn("nAlive");     // generated - killed (should be ~0)
    analysisManager->FinishNtuple();

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void HistoManager::BookOpticalFluxMaps()
{
    // Per-slab optical-photon x-y flux/fluence map (track-length estimator),
    // slab-local frame. Booked ONLY when a scintillator slab is part of the
    // built geometry, so geometries without a slab create no such histogram.
    //
    // The collection-efficiency maps are no longer booked here: the produced /
    // detected information lives in the sipmHits tree (muonX/muonY/nProduced/
    // nDetected), so the efficiency can be binned offline at any cell size and
    // for any SiPM subset.
    //
    // Must be called after the geometry exists (RunAction::BeginOfRunAction).
    // Called on master and worker threads; CreateH2 is idempotent per name in
    // the analysis manager, so no cross-thread duplication occurs.
    if (fOpticalFluxBooked) return;
    fOpticalFluxH2Ids.clear();
    if (fDetector && fDetector->GetNumMuonScints() > 0) {
        auto* analysisManager = G4AnalysisManager::Instance();
        for (G4int i = 0; i < fDetector->GetNumMuonScints(); ++i) {
            GeometryMuonScint* slab = fDetector->GetMuonScint(i);
            if (!slab) {
                fOpticalFluxH2Ids.push_back(-1);
                continue;
            }

            const G4ThreeVector h = slab->GetHalfSize();
            const G4int nbX = std::max(1, (G4int)std::lround(2. * h.x() / fCellSize));
            const G4int nbY = std::max(1, (G4int)std::lround(2. * h.y() / fCellSize));

            std::ostringstream name, title;
            name  << "h2_optflux_xy_slab" << i;
            title << "slab " << i
                  << " optical-photon track-length flux map (slab-local);"
                  << "x [mm];y [mm]";
            const G4int id = analysisManager->CreateH2(
                name.str(), title.str(),
                nbX, -h.x(), h.x(), nbY, -h.y(), h.y());
            fOpticalFluxH2Ids.push_back(id);
        }
    }
    fOpticalFluxBooked = true;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
