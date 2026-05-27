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
/// \file G4DCSmonitor.cc
/// \brief Main program of the hadronic/Hadr03 example
//
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "ActionInitialization.hh"
#include "DetectorConstruction.hh"
#include "PhysicsList.hh"

#include "G4ParticleHPManager.hh"
#include "G4RunManagerFactory.hh"
#include "G4SteppingVerbose.hh"
#include "G4Types.hh"
#include "G4UIExecutive.hh"
#include "G4UImanager.hh"
#include "G4VisExecutive.hh"
#include "Randomize.hh"

#include "TROOT.h"
#include "TH1.h"
#include "RootManager.hh"

#include <csignal>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// for ctrl-c quitting, properly closing the ROOT file
volatile std::sig_atomic_t g_sigint_received = 0;

void sigint_handler(int signal) {
    g_sigint_received = signal;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

int main(int argc, char** argv)
{
    //std::signal(SIGINT, sigint_handler);

    // ROOT memory management
    gROOT->SetBatch(kTRUE);
    TH1::AddDirectory(kFALSE);

    // set seed
    G4Random::setTheSeed(time(NULL));
    G4Random::showEngineStatus();

    //// --- CASTOR-MOD: comment the entire rootManager setup out, as we do not have a catcher phase-space file 
    //// --- to sample from. Instead, we will start with just the a simple G4GeneralParticleSource
    //// initialize ROOT manager for phase space sampling
    TFile::SetCacheFileDir("/tmp/root_cache"); // Optional: set cache dir
    RootManager& rootManager = RootManager::GetInstance();
    ////rootManager.Initialize("root_files/G4Li_3mm_1e9_phase.root", "hsparse");
    ////rootManager.Initialize("root_files/catchers/protons_iso_Be_1e8_phase.root", "hsparse2");
    //rootManager.Initialize("root_files/catchers/phase/protons_cos_Be_1e9_phase.root", "hsparse2");
    //if (!rootManager.IsInitialized()) {
    //    G4cerr << "Failed to initialize RootManager! Exiting." << G4endl;
    //    return 1;
    //}


    // -----------------------------------------------------------------
    // CLI parsing.
    //
    // Supported invocations (positional arg meanings unchanged):
    //   G4DCSmonitor                                  -> interactive
    //   G4DCSmonitor run.mac                          -> batch, no biasing
    //   G4DCSmonitor run.mac 17                       -> batch, fileNum=17
    //   G4DCSmonitor --biasing neutron run.mac        -> batch, bias neutrons
    //   G4DCSmonitor --biasing gamma run.mac 17       -> batch, bias gammas, fileNum=17
    //   G4DCSmonitor run.mac --biasing neutron 17     -> order-insensitive
    //
    // --biasing accepts: gamma | neutron | none | off
    // -----------------------------------------------------------------
    G4String biasParticle = "";                  // "" -> no importance biasing
    std::vector<G4String> positional;
    positional.reserve(argc);
    for (int i = 1; i < argc; ++i) {
        const G4String arg = argv[i];
        if (arg == "--biasing" || arg == "-b") {
            if (i + 1 >= argc) {
                G4cerr << "[CLI] '" << arg
                       << "' requires an argument (gamma|neutron|none)." << G4endl;
                return 1;
            }
            G4String val = argv[++i];
            if (val == "none" || val == "off" || val == "false") val = "";
            if (!val.empty() && val != "gamma" && val != "neutron") {
                G4cerr << "[CLI] --biasing must be one of: gamma, neutron, none. "
                       << "Got '" << val << "'." << G4endl;
                return 1;
            }
            biasParticle = val;
        }
        else if (arg.length() > 10 && arg.substr(0, 10) == "--biasing=") {
            G4String val = arg.substr(10);
            if (val == "none" || val == "off" || val == "false") val = "";
            if (!val.empty() && val != "gamma" && val != "neutron") {
                G4cerr << "[CLI] --biasing must be one of: gamma, neutron, none. "
                       << "Got '" << val << "'." << G4endl;
                return 1;
            }
            biasParticle = val;
        }
        else if (arg == "--help" || arg == "-h") {
            G4cout << "Usage: " << argv[0]
                   << " [--biasing gamma|neutron|none] [macro] [fileNum]" << G4endl;
            return 0;
        }
        else {
            positional.push_back(arg);
        }
    }
    
    // 2nd positional arg = file number, if present
    if (positional.size() > 1) {
        rootManager.SetFileNum(std::atoi(positional[1].c_str()));
    }
    //// file number
    //if(argc > 2) {
    //    G4int fileNum = atoi(argv[2]);
    //    rootManager.SetFileNum(fileNum);
    //}

    G4UIExecutive* ui = nullptr;
    if (positional.empty()) ui = new G4UIExecutive(argc, argv);
    //// detect interactive mode (if no arguments) and define UI session
    //G4UIExecutive* ui = nullptr;
    //if (argc == 1) ui = new G4UIExecutive(argc, argv);

    // use G4SteppingVerboseWithUnits
    G4int precision = 4;
    G4SteppingVerbose::UseBestUnit(precision);

    // construct the run manager
    auto runManager = G4RunManagerFactory::CreateRunManager();

    // set mandatory initialization classes
    DetectorConstruction* det = new DetectorConstruction;
    runManager->SetUserInitialization(det);
    
    PhysicsList* phys = new PhysicsList(biasParticle);
    runManager->SetUserInitialization(phys);
    runManager->SetUserInitialization(new ActionInitialization(det));

    // Replaced HP environmental variables with C++ calls
    // TODO - can we put this into the physics list class?
    G4ParticleHPManager::GetInstance()->SetSkipMissingIsotopes(true); // testing
    G4ParticleHPManager::GetInstance()->SetDoNotAdjustFinalState(true);
    G4ParticleHPManager::GetInstance()->SetUseOnlyPhotoEvaporation(true);
    G4ParticleHPManager::GetInstance()->SetNeglectDoppler(false);
    G4ParticleHPManager::GetInstance()->SetProduceFissionFragments(false); // testing
    G4ParticleHPManager::GetInstance()->SetUseWendtFissionModel(false);
    G4ParticleHPManager::GetInstance()->SetUseNRESP71Model(false);

    // initialize visualization
    G4VisManager* visManager = nullptr;

    // get the pointer to the User Interface manager
    G4UImanager* UImanager = G4UImanager::GetUIpointer();

    visManager = new G4VisExecutive;
    visManager->Initialize();
    if (ui) {
        // interactive mode
        ui->SessionStart();
        delete ui;
    }
    else {
        // batch mode
        G4String command = "/control/execute ";
        //G4String fileName = argv[1];
        G4String fileName = positional[0];
        UImanager->ApplyCommand(command + fileName);
    }

    // job termination
    delete visManager;
    delete runManager;

    rootManager.Cleanup();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
