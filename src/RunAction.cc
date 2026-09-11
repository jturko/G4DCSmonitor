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

#include "TTree.h"
#include <cstring>

#include <filesystem>   // C++17
namespace fs = std::filesystem;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

std::atomic<G4bool> RunAction::WritePrimaryTree{false};
std::atomic<G4bool> RunAction::WritePrimaryTreeOnlyOnHit{false};
std::atomic<G4bool> RunAction::WriteCASTOR440SurfaceFluxTree{false};
std::atomic<G4bool> RunAction::WriteFluxMap{true};

std::once_flag RunAction::fSurfaceSamplerOnce;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction(DetectorConstruction* det, PrimaryGeneratorAction* prim)
    : fDetector(det), fPrimary(prim), fProgBar(NULL)
{
    fHistoManager = new HistoManager(fDetector->GetWorldSize());
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
    //if (IsMaster() && (run->GetRunID() == 0)) {
        //MakeMassTable();
        //G4Random::showEngineStatus();
        //G4cout << *(G4Material::GetMaterialTable()) << G4endl;
    //}

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

    // ------------------------------------------------------------------
    // Self-describing detector metadata tree. Written ONCE per output file,
    // on the MASTER only, AFTER the merged file has been closed. Reopening
    // "UPDATE" and writing a plain TTree is deterministic and avoids all MT
    // ntuple-merge ambiguity, giving exactly one meta row per det ID.
    // ------------------------------------------------------------------
    if (isMaster) {
        G4String path = analysisManager->GetFileName();
        if (path.size() < 5 || path.substr(path.size() - 5) != ".root")
            path += ".root";

        TFile fmeta(path.c_str(), "UPDATE");
        if (!fmeta.IsZombie() && fmeta.Get("meta") == nullptr) {
            const auto rows = fDetector->BuildDetectorMeta();

            Int_t    det_id, det_index;
            char     det_type[16], placement_mode[16];
            Double_t anchor_x, anchor_y, anchor_z;
            Double_t rot_x_deg, rot_y_deg, rot_z_deg;
            Double_t crystal_x, crystal_y, crystal_z;
            Double_t scan_phi_deg, scan_z_mm;

            TTree meta("meta", "self-describing sensitive-detector metadata");
            meta.Branch("det_id",         &det_id,         "det_id/I");
            meta.Branch("det_type",        det_type,       "det_type/C");
            meta.Branch("det_index",      &det_index,      "det_index/I");
            meta.Branch("placement_mode",  placement_mode, "placement_mode/C");
            meta.Branch("anchor_x_mm",    &anchor_x,       "anchor_x_mm/D");
            meta.Branch("anchor_y_mm",    &anchor_y,       "anchor_y_mm/D");
            meta.Branch("anchor_z_mm",    &anchor_z,       "anchor_z_mm/D");
            meta.Branch("rot_x_deg",      &rot_x_deg,      "rot_x_deg/D");
            meta.Branch("rot_y_deg",      &rot_y_deg,      "rot_y_deg/D");
            meta.Branch("rot_z_deg",      &rot_z_deg,      "rot_z_deg/D");
            meta.Branch("crystal_x_mm",   &crystal_x,      "crystal_x_mm/D");
            meta.Branch("crystal_y_mm",   &crystal_y,      "crystal_y_mm/D");
            meta.Branch("crystal_z_mm",   &crystal_z,      "crystal_z_mm/D");
            meta.Branch("scan_phi_deg",   &scan_phi_deg,   "scan_phi_deg/D");
            meta.Branch("scan_z_mm",      &scan_z_mm,      "scan_z_mm/D");

            for (const auto& r : rows) {
                det_id    = r.id;
                det_index = r.typeIndex;
                std::strncpy(det_type,       r.type.c_str(),          sizeof(det_type) - 1);
                det_type[sizeof(det_type) - 1] = '\0';
                std::strncpy(placement_mode, r.placementMode.c_str(), sizeof(placement_mode) - 1);
                placement_mode[sizeof(placement_mode) - 1] = '\0';

                anchor_x = r.anchorPos.x() / mm;
                anchor_y = r.anchorPos.y() / mm;
                anchor_z = r.anchorPos.z() / mm;
                rot_x_deg = r.rotDeg.x();     // raw macro degrees
                rot_y_deg = r.rotDeg.y();
                rot_z_deg = r.rotDeg.z();
                crystal_x = r.crystalCenter.x() / mm;
                crystal_y = r.crystalCenter.y() / mm;
                crystal_z = r.crystalCenter.z() / mm;
                scan_phi_deg = r.scanPhiDeg;          // -9999 if unset
                scan_z_mm    = r.scanZmm;             // already mm; -9999 if unset

                meta.Fill();
            }

            fmeta.cd();
            meta.Write("", TObject::kOverwrite);
        }
        fmeta.Close();
    }


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
