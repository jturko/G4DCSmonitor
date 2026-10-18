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
/// \file RunMessenger.cc
/// \brief Implementation of the RunMessenger class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "RunMessenger.hh"
#include "RunAction.hh"

#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIdirectory.hh"

#include "G4VisManager.hh"
#include "G4VViewer.hh"
#include "G4ViewParameters.hh"
#include "G4Scene.hh"
#include "G4Point3D.hh"
#include "G4Vector3D.hh"
#include "G4SystemOfUnits.hh"
#include "G4Threading.hh"

#include <algorithm>
#include <cmath>

namespace {
// Movie camera schedule: hold the wide muon view, then ease into the
// fibre/SiPM close-up while the optical photons bounce and WLS-shift.
constexpr G4double kPanStart = 1.5;   // ns
constexpr G4double kPanEnd   = 12.0;  // ns
constexpr G4double kWideTheta  = 55.; // deg
constexpr G4double kCloseTheta = 80.; // deg
constexpr G4double kViewPhi    = 250.;// deg
constexpr G4double kWideZoom  = 1.0;
constexpr G4double kCloseZoom = 3.5;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunMessenger::RunMessenger(RunAction* run) : fRun(run)
{
    fRunDir = new G4UIdirectory("/dcs-monitor/run/");
    fRunDir->SetGuidance("run commands for the DCS monitor simulation");
    
    fWritePrimaryCmd = new G4UIcmdWithABool("/dcs-monitor/run/writePrimary", this);
    fWritePrimaryCmd->SetGuidance("Toggle filling of the primary vertex ntuple");
    fWritePrimaryCmd->SetParameterName("write", true);
    fWritePrimaryCmd->SetDefaultValue(true);
    fWritePrimaryCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    fWriteCASTOR440SurfaceFluxCmd = new G4UIcmdWithABool("/dcs-monitor/run/writeSurfaceFlux", this);
    fWriteCASTOR440SurfaceFluxCmd->SetGuidance("Toggle filling of the CASTOR 440 surface flux ntuple");
    fWriteCASTOR440SurfaceFluxCmd->SetParameterName("write", true);
    fWriteCASTOR440SurfaceFluxCmd->SetDefaultValue(true);
    fWriteCASTOR440SurfaceFluxCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fWriteOpticalFluxMapCmd = new G4UIcmdWithABool("/dcs-monitor/run/writeOpticalFluxMap", this);
    fWriteOpticalFluxMapCmd->SetGuidance("Toggle filling of the per-slab slab-local optical-photon x-y flux maps (only booked when a scintillator slab is built; default OFF)");
    fWriteOpticalFluxMapCmd->SetParameterName("write", true);
    fWriteOpticalFluxMapCmd->SetDefaultValue(false);
    fWriteOpticalFluxMapCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fShowDetectedPhotonsOnlyCmd = new G4UIcmdWithABool("/dcs-monitor/run/showDetectedPhotonsOnly", this);
    fShowDetectedPhotonsOnlyCmd->SetGuidance("Visualization-only: after each event keep the detected optical photons plus their full ancestor chain (creator photons and muon) (requires /tracking/storeTrajectory 1; default OFF)");
    fShowDetectedPhotonsOnlyCmd->SetParameterName("show", true);
    fShowDetectedPhotonsOnlyCmd->SetDefaultValue(false);
    fShowDetectedPhotonsOnlyCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMovieCameraCmd = new G4UIcmdWithADouble("/dcs-monitor/run/movieCamera", this);
    fMovieCameraCmd->SetGuidance("Set the movie camera for a given head time (ns): wide muon view -> fibre/SiPM close-up");
    fMovieCameraCmd->SetParameterName("time", false);
    fMovieCameraCmd->AvailableForStates(G4State_Idle);

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunMessenger::~RunMessenger()
{
    delete fRunDir;
    delete fWritePrimaryCmd;
    delete fWriteCASTOR440SurfaceFluxCmd;
    delete fWriteOpticalFluxMapCmd;
    delete fShowDetectedPhotonsOnlyCmd;
    delete fMovieCameraCmd;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if (command == fWritePrimaryCmd) {
        RunAction::WritePrimaryTree = fWritePrimaryCmd->GetNewBoolValue(newValue);
    }
    if (command == fWriteCASTOR440SurfaceFluxCmd) {
        RunAction::WriteCASTOR440SurfaceFluxTree = fWriteCASTOR440SurfaceFluxCmd->GetNewBoolValue(newValue);
    }
    if (command == fWriteOpticalFluxMapCmd) {
        RunAction::WriteOpticalFluxMap = fWriteOpticalFluxMapCmd->GetNewBoolValue(newValue);
    }
    if (command == fShowDetectedPhotonsOnlyCmd) {
        RunAction::ShowDetectedPhotonsOnly = fShowDetectedPhotonsOnlyCmd->GetNewBoolValue(newValue);
    }
    if (command == fMovieCameraCmd) {
        SetMovieCamera(fMovieCameraCmd->GetNewDoubleValue(newValue));
    }
}

void RunMessenger::SetMovieCamera(G4double time)
{
    // /control/loop is broadcast to the worker command stack as well as the
    // master; the visualization manager exists only on the master.
    if (!G4Threading::IsMasterThread()) return;

    G4VisManager* vis = G4VisManager::GetInstance();
    if (!vis) return;
    G4VViewer* viewer = vis->GetCurrentViewer();
    G4Scene* scene = vis->GetCurrentScene();
    if (!viewer || !scene) return;

    G4double u = (time - kPanStart) / (kPanEnd - kPanStart);
    u = std::clamp(u, 0., 1.);
    u = u*u*(3. - 2.*u);   // smoothstep

    const G4double theta = (kWideTheta + u*(kCloseTheta - kWideTheta)) * deg;
    const G4double phi   = kViewPhi * deg;
    const G4double zoom  = kWideZoom + u*(kCloseZoom - kWideZoom);
    const G4ThreeVector target = G4ThreeVector(0., 0., 0.)
        + u*(G4ThreeVector(-110.*mm, 0., 0.) - G4ThreeVector(0., 0., 0.));

    const G4Vector3D dir(std::sin(theta)*std::cos(phi),
                         std::sin(theta)*std::sin(phi),
                         std::cos(theta));

    G4ViewParameters vp = viewer->GetViewParameters();
    vp.SetViewpointDirection(dir);
    vp.SetUpVector(G4Vector3D(0., 0., 1.));
    vp.SetZoomFactor(zoom);
    vp.SetCurrentTargetPoint(G4Point3D(target.x(), target.y(), target.z())
                             - scene->GetStandardTargetPoint());
    viewer->SetViewParameters(vp);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
