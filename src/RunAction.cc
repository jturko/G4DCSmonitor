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
/// \file RunAction.cc
/// \brief Implementation of the RunAction class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "HistoManager.hh"
#include "PrimaryGeneratorAction.hh"
#include "Run.hh"
#include "RunMessenger.hh"

#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "Randomize.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4Material.hh"

#include "G4ProcessTable.hh"

#include "ProgressBar.hh"

#include <iomanip>

#include "TFile.h"
#include "TROOT.h"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

//G4bool RunAction::WritePrimaryTree = false;
std::atomic<G4bool> RunAction::WritePrimaryTree{false};
std::atomic<G4bool> RunAction::WriteCASTOR440SurfaceFluxTree{false};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction(DetectorConstruction* det, PrimaryGeneratorAction* prim)
    : fDetector(det), fPrimary(prim), fProgBar(NULL)
{
    fHistoManager = new HistoManager();
    fRunMessenger = new RunMessenger(this);

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::~RunAction()
{
    delete fHistoManager;
    delete fRunMessenger;

    if(fProgBar)
        delete fProgBar;

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4Run* RunAction::GenerateRun()
{
    fRun = new Run(fDetector);
    return fRun;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run* run)
{
    // show Rndm status
    if (isMaster) {
        G4Random::showEngineStatus();
        G4cout << *(G4Material::GetMaterialTable()) << G4endl;
    }

    // histograms
    G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
    analysisManager->OpenFile();

    //auto* opBdry = G4ProcessTable::GetProcessTable()->FindProcess("OpBoundary", "opticalphoton");
    //if (opBdry) {
    //    opBdry->SetVerboseLevel(2);
    //    G4cout << " [Diag] OpBoundary verbosity raised to 2" << G4endl;
    //}

    ProgressBar::gEvtNb.store(0, std::memory_order_relaxed);
    if(fProgBar)
        delete fProgBar;
    fProgBar = new ProgressBar(run->GetNumberOfEventToBeProcessed(), 1.0, 25);

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* run)
{
    //if(G4Threading::G4GetThreadId() == 0) {
    if(isMaster) {
        fProgBar->Print(run->GetNumberOfEventToBeProcessed()-1);
    }


    // save histograms
    G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
    analysisManager->Write();
    analysisManager->CloseFile();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::SetPrintFlag(G4bool flag)
{
    fPrint = flag;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
