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
/// \file DetectorMessenger.hh
/// \brief Definition of the DetectorMessenger class
//
//
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#ifndef DetectorMessenger_h
#define DetectorMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

#include <map>
#include <functional>

class DetectorConstruction;
class G4UIdirectory;
class G4UIcommand;
class G4UIcmdWithAString;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithoutParameter;
class G4UIcmdWith3Vector;
class G4UIcmdWith3VectorAndUnit;
class G4UIcmdWithABool;
class G4UIcmdWithAnInteger;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class DetectorMessenger : public G4UImessenger
{
  public:
    DetectorMessenger(DetectorConstruction*);
    ~DetectorMessenger() override;

    void SetNewValue(G4UIcommand*, G4String) override;

  private:
    DetectorConstruction* fDetector = nullptr;

    // directory
    G4UIdirectory* fDir = nullptr;

    // biasing
    G4UIcmdWithABool* fUseBiasingCmd = nullptr;
    G4UIcmdWithAString*        fSetBiasingParticleCmd = nullptr;
    G4UIcmdWithAnInteger*      fSetNShellsCmd   = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetBiasRMinCmd  = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetBiasRMaxCmd  = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetBiasHMinCmd  = nullptr;

    // placement
    G4UIcmdWith3VectorAndUnit*  fSetPositionCmd = nullptr;
    G4UIcmdWith3Vector*         fSetRotationCmd = nullptr;

    // DCS monitor
    // CLYC detector
    G4UIcmdWithoutParameter* fAddCLYCCmd = nullptr;
    G4UIcmdWithoutParameter* fAddCLYCByCrystalCenterCmd = nullptr;
    // crystal dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCCrystalRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCCrystalLengthCmd = nullptr;
    // aluminum dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCCasingThicknessCmd = nullptr;
    // LiF collimator dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCLiFColInnerRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCLiFColOuterRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCLiFColLengthCmd = nullptr;
    // Pb collimator dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCPbColInnerRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPbColOuterRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPbColLengthCmd = nullptr;
    // PEHD collimator dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEColInnerRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEColOuterRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEColLengthCmd = nullptr;
    // PE plug dims
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEPlugLipRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEPlugInnerRadiusCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEPlugLipLengthCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fSetCLYCPEPlugInnerLengthCmd = nullptr;
    // materials
    G4UIcmdWithAString* fSetCLYCCrystalMaterialNameCmd = nullptr;
    G4UIcmdWithAString* fSetCLYCCasingMaterialNameCmd = nullptr;
    G4UIcmdWithAString* fSetCLYCLiFColMaterialNameCmd = nullptr;
    G4UIcmdWithAString* fSetCLYCPbColMaterialNameCmd = nullptr;
    G4UIcmdWithAString* fSetCLYCPEColMaterialNameCmd = nullptr;
    G4UIcmdWithAString* fSetCLYCPEPlugMaterialNameCmd = nullptr;

    // Plastic (shadowed) commands: command -> action(value-string)
    std::map<G4UIcommand*, std::function<void(const G4String&)>> fPlasticActions;
    void BuildPlasticCommands();

    // castor 440
    G4UIcmdWithoutParameter* fAddCASTOR440Cmd = nullptr;

    // hemi-shield
    std::map<G4UIcommand*, std::function<void(const G4String&)>> fHemiShieldActions;
    void BuildHemiShieldCommands();


};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif
