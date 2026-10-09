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
/// \file HistoManager.hh
/// \brief Definition of the HistoManager class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef HistoManager_h
#define HistoManager_h 1

#include "G4AnalysisManager.hh"
#include "globals.hh"

#include <vector>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class DetectorConstruction;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class HistoManager
{
  public:
    HistoManager(DetectorConstruction* detector = nullptr,
                 G4double cellSize = 1.);
    ~HistoManager() = default;

    // Book the per-slab optical-photon x-y flux maps. Geometry-dependent, so
    // this MUST be called after the geometry has been built (i.e. from
    // RunAction::BeginOfRunAction), not from the constructor: in MT the master
    // RunAction is created before the macro's geometry commands execute.
    // Idempotent; safe to call on master and worker threads alike.
    void BookOpticalFluxMaps();

    // Analysis-manager H2 id of the optical-photon slab-local x-y flux map,
    // or -1 if that slab has no map booked (no slab / detector not supplied).
    G4int GetOpticalFluxH2Id(G4int slabIndex) const
    {
        return (slabIndex >= 0 && slabIndex < (G4int)fOpticalFluxH2Ids.size())
                   ? fOpticalFluxH2Ids[slabIndex] : -1;
    }

  private:
    void Book();
    G4String fFileName = "G4DCSmonitor";
    DetectorConstruction* fDetector = nullptr;
    G4double fCellSize;

    G4bool fOpticalFluxBooked = false;
    std::vector<G4int> fOpticalFluxH2Ids;   // one per built scintillator slab
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
