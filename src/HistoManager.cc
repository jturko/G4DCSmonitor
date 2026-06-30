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

#include "G4UnitsTable.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

HistoManager::HistoManager()
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

    G4int idx;
    
    // force one histogram so empty files are still created
    idx = analysisManager->CreateH1("_dummy", "_dummy", 1, 0, 1);

    // ntuple for primary particles
    idx = analysisManager->CreateNtuple("primary", "tree of primary particles");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");    // 0
    analysisManager->CreateNtupleDColumn("ekin");   // 1  
    analysisManager->CreateNtupleDColumn("t");      // 2
    analysisManager->CreateNtupleDColumn("x");      // 3
    analysisManager->CreateNtupleDColumn("y");      // 4
    analysisManager->CreateNtupleDColumn("z");      // 5
    analysisManager->CreateNtupleDColumn("px");     // 6
    analysisManager->CreateNtupleDColumn("py");     // 7
    analysisManager->CreateNtupleDColumn("pz");     // 8
    analysisManager->CreateNtupleDColumn("evtNb");  // 9 
    analysisManager->FinishNtuple();
    
    // ntuple for detector hits
    idx = analysisManager->CreateNtuple("hits", "tree of sensitive detector hits");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");    // 0
    analysisManager->CreateNtupleDColumn("edep");   // 1  
    analysisManager->CreateNtupleDColumn("t");      // 2
    analysisManager->CreateNtupleDColumn("x");      // 3
    analysisManager->CreateNtupleDColumn("y");      // 4
    analysisManager->CreateNtupleDColumn("z");      // 5
    analysisManager->CreateNtupleIColumn("det");    // 6
    analysisManager->CreateNtupleDColumn("weight"); // 7
    analysisManager->CreateNtupleDColumn("evtNb");  // 8
    
    // ntuple for CASTOR 440 surface tracker
    idx = analysisManager->CreateNtuple("surfaceFlux", "tree of CASTOR 440 surface flux (particles leaving the cask)");
    analysisManager->SetNtupleActivation(idx, true);
    analysisManager->CreateNtupleDColumn("pid");    // 0
    analysisManager->CreateNtupleDColumn("ekin");   // 1
    analysisManager->CreateNtupleDColumn("t");      // 2
    analysisManager->CreateNtupleDColumn("x");      // 3
    analysisManager->CreateNtupleDColumn("y");      // 4
    analysisManager->CreateNtupleDColumn("z");      // 5
    analysisManager->CreateNtupleDColumn("px");     // 6
    analysisManager->CreateNtupleDColumn("py");     // 7
    analysisManager->CreateNtupleDColumn("pz");     // 8
    analysisManager->CreateNtupleDColumn("weight"); // 9
    analysisManager->CreateNtupleDColumn("evtNb");  // 10
    analysisManager->FinishNtuple();
    
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
