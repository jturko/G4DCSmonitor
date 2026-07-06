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
#include "GeometryCASTOR440.hh"
#include "SurfaceFluxSampler.hh"
#include "ProgressBar.hh"

#include "G4Run.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "G4Material.hh"
#include "Randomize.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4LogicalVolume.hh"
#include "G4SystemOfUnits.hh"
#include "globals.hh"
#include "G4Threading.hh"

#include "G4GDMLParser.hh"
#include "G4TransportationManager.hh"

#include <iomanip>

#include "TFile.h"
#include "TROOT.h"

#include <filesystem>   // C++17
namespace fs = std::filesystem;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

std::atomic<G4bool> RunAction::WritePrimaryTree{false};
std::atomic<G4bool> RunAction::WritePrimaryTreeOnlyOnHit{false};
std::atomic<G4bool> RunAction::WriteCASTOR440SurfaceFluxTree{false};

std::once_flag RunAction::fSurfaceSamplerOnce;

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
    // print mass table
    //
    //if (IsMaster())
    //    MakeMassTable();

    // show Rndm status
    //
    //if (isMaster) {
    //    G4Random::showEngineStatus();
    //    G4cout << *(G4Material::GetMaterialTable()) << G4endl;
    //}

    // histograms
    //
    G4AnalysisManager* analysisManager = G4AnalysisManager::Instance();
    //analysisManager->OpenFile();
    // --- ensure the output directory exists ---
    fs::path outPath = analysisManager->GetFileName();   // whatever was set as default
    if (outPath.has_parent_path()) {
        fs::create_directories(outPath.parent_path());    // makes nested dirs if missing
    }

    if (!analysisManager->OpenFile()) {
        G4Exception("RunAction::BeginOfRunAction", "OpenFail",
                    FatalException, "Could not open analysis output file.");
    }

    // prog bar
    //
    ProgressBar::gEvtNb.store(0, std::memory_order_relaxed);
    if(fProgBar)
        delete fProgBar;
    fProgBar = new ProgressBar(run->GetNumberOfEventToBeProcessed(), 1.0, 25);


    // GDML output
    //
    //if (run->GetRunID() == 0)
    //{
    //    G4VPhysicalVolume* world =
    //        G4TransportationManager::GetTransportationManager()
    //            ->GetNavigatorForTracking()->GetWorldVolume();

    //    G4GDMLParser parser;
    //    parser.Write("myGeometry.gdml", world->GetLogicalVolume());
    //}

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* run)
{
    //if(G4Threading::G4GetThreadId() == 0) {
    if(isMaster) {
        fProgBar->Print(run->GetNumberOfEventToBeProcessed()-1);
    }

    //if (isMaster) {
    //    // volumes
    //    G4cout << " -------------- Volumes in this run -------------- " << G4endl;
    //    G4PhysicalVolumeStore* PVStore = G4PhysicalVolumeStore::GetInstance();
    //    for (auto it = PVStore->begin(); it != PVStore->end(); ++it) {
    //        G4VPhysicalVolume* currentVolume = *it;
    //        G4String volumeName = currentVolume->GetName();
    //        G4cout << " - " << volumeName << G4endl;
    //    }
    //    G4cout << " ------------------------------------------------- " << G4endl;
    //    
    //    // run info
    //    fRun->EndOfRun(fPrint);
    //    
    //    // show Rndm status
    //    G4Random::showEngineStatus();
    //}

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

void RunAction::MakeMassTable() const
{
    G4LogicalVolumeStore* lvStore = G4LogicalVolumeStore::GetInstance();
    if (!lvStore || lvStore->empty())
    {
        G4cout << "[MassTable] Logical volume store is empty." << G4endl;
        return;
    }

    // --- Header ---------------------------------------------------------
    G4cout << "\n"
           << "=================================================================="
              "==================\n"
           << "  GEOMETRY MASS TABLE  (local masses, daughters listed separately)\n"
           << "=================================================================="
              "==================\n";

    G4cout << std::left
           << std::setw(22) << "Logical Volume"
           << std::setw(25) << "Material"
           << std::right
           << std::setw(14) << "Density[g/cm3]"
           << std::setw(16) << "Volume[cm3]"
           << std::setw(16) << "Mass[g]"
           << "\n"
           << "------------------------------------------------------------------"
              "------------------\n";

    G4double totalMass = 0.;

    for (auto* lv : *lvStore)
    {
        if (!lv) continue;

        G4Material* mat = lv->GetMaterial();
        if (!mat) continue;   // e.g. parallel/world helper volumes with no material

        // Local mass only (do NOT propagate into daughters here)
        const G4double mass    = lv->GetMass(false, false);          // internal units
        const G4double volume  = lv->GetSolid()->GetCubicVolume();   // internal units
        const G4double density = mat->GetDensity();                  // internal units

        totalMass += mass;

        G4cout << std::left
               << std::setw(22) << lv->GetName()
               << std::setw(25) << mat->GetName()
               << std::right << std::fixed << std::setprecision(4)
               << std::setw(14) << density / (g / cm3)
               << std::setw(16) << volume  / cm3
               << std::setw(16) << mass    / g
               << "\n";
    }

    G4cout << "------------------------------------------------------------------"
              "------------------\n"
           << std::left << std::setw(54) << "  TOTAL (sum of all local masses)"
           << std::right << std::fixed << std::setprecision(4)
           << std::setw(16) << totalMass / g << " g"
           << "\n"
           << "=================================================================="
              "==================\n"
           << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
