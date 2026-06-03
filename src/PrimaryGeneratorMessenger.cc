//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "PrimaryGeneratorMessenger.hh"
#include "PrimaryGeneratorAction.hh"
#include "SurfaceFluxSampler.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIcmdWith3Vector.hh"
#include "G4UIcmdWithoutParameter.hh"
#include "G4UIcmdWithABool.hh"

#include "G4Threading.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorMessenger::PrimaryGeneratorMessenger(PrimaryGeneratorAction* gun)
:fPrimaryGeneratorAction(gun)
{
    // directory
    fDir = new G4UIdirectory("/dcs-monitor/gun/");
    fDir->SetGuidance("primary generator commands");

    // set SourceMode enum
    fSourceModeCmd = new G4UIcmdWithAString("/dcs-monitor/gun/sourceMode", this);
    fSourceModeCmd->SetGuidance("Select the source distribution mode.");
    fSourceModeCmd->SetParameterName("mode", false);
    fSourceModeCmd->SetCandidates("GPS CASTOR440_surface CASTOR440_surface_from_TTree CASTOR440_fuel CASTOR440_fuel_biased");
    fSourceModeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // For FuelFlux generators -- set the CASTOR440 cask/fuel assembly 
    fCaskNumCmd = new G4UIcmdWithAnInteger("/dcs-monitor/gun/caskNum", this);
    fCaskNumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    fFuelNumCmd = new G4UIcmdWithAnInteger("/dcs-monitor/gun/fuelNum", this);
    fFuelNumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    // For geom. biasing -- CLYC bounding radius
    fCLYCBoundingRadiusCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/gun/clycBoundingRadius", this);
    fCLYCBoundingRadiusCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // Watt spectrum generator
    fUseWattCmd = new G4UIcmdWithABool("/dcs-monitor/gun/useWattSpectrum", this);
    fUseWattCmd->SetGuidance("Enable Watt fission spectrum for kCASTOR440_fuel* modes.");
    fUseWattCmd->SetParameterName("flag", false);
    fUseWattCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fWattACmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/gun/wattA", this);
    fWattACmd->SetGuidance("Watt spectrum parameter 'a' (energy units, e.g. MeV).");
    fWattACmd->SetParameterName("a", false);
    fWattACmd->SetUnitCategory("Energy");
    fWattACmd->SetDefaultUnit("MeV");
    fWattACmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fWattBCmd = new G4UIcmdWithADouble("/dcs-monitor/gun/wattB", this);
    fWattBCmd->SetGuidance("Watt spectrum parameter 'b' in units of 1/MeV.");
    fWattBCmd->SetParameterName("b", false);
    fWattBCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // surface flux sampler from root TTree
    fSurfaceSourceFileCmd = new G4UIcmdWithAString("/dcs-monitor/gun/surfaceSourceFile", this);
    fSurfaceSourceFileCmd->SetGuidance("Path to step-1 ROOT file containing the 'surfaceFlux' tree.");
    fSurfaceSourceFileCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    fSurfaceSourcePidCmd = new G4UIcmdWithAnInteger("/dcs-monitor/gun/surfaceSourcePid", this);
    fSurfaceSourcePidCmd->SetGuidance("PDG code filter: 0=any, 2112=neutron, 22=gamma.");
    fSurfaceSourcePidCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSurfaceSourceMaxEntriesCmd = new G4UIcmdWithAnInteger("/dcs-monitor/gun/surfaceSourceMaxEntries", this);
    fSurfaceSourceMaxEntriesCmd->SetGuidance("Set number of primaries to load from the surface source file");
    fSurfaceSourceMaxEntriesCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // smearing commands for surface flux sampler
    fSmearPhiCmd = new G4UIcmdWithADouble("/dcs-monitor/gun/surfaceSmearPhi", this);
    fSmearPhiCmd->SetGuidance("Gaussian sigma for azimuthal phi smearing on the cask "
                              "side surface, in radians. 0 = no smearing (default).");
    fSmearPhiCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearZCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/gun/surfaceSmearZ", this);
    fSmearZCmd->SetGuidance("Gaussian sigma for axial z smearing on the cask side "
                            "surface. 0 = no smearing (default).");
    fSmearZCmd->SetDefaultUnit("mm");
    fSmearZCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearAngleCmd = new G4UIcmdWithADouble("/dcs-monitor/gun/surfaceSmearAngle", this);
    fSmearAngleCmd->SetGuidance("Gaussian sigma (rad) for direction smearing in a small "
                                "cone about the stored direction. 0 = no smearing.");
    fSmearAngleCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearEfracCmd = new G4UIcmdWithADouble("/dcs-monitor/gun/surfaceSmearEfrac", this);
    fSmearEfracCmd->SetGuidance("Fractional Gaussian sigma for kinetic-energy smearing "
                                "(E -> E*(1 + N(0,sigma))). 0 = no smearing.");
    fSmearEfracCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // for calc. of surface flux to primary ratio
    fSurfaceSourceNumPrimariesCmd = new G4UIcmdWithAnInteger("/dcs-monitor/gun/surfaceSourceNumPrimaries", this);
    fSurfaceSourceNumPrimariesCmd->SetGuidance("set number of primaries in the file, used to calculate "
                                               "the surface flux to primaries ratio");
    fSurfaceSourceNumPrimariesCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
    
    fSurfaceSourceDecayRateCmd = new G4UIcmdWithADouble("/dcs-monitor/gun/surfaceSourceDecayRate", this);
    fSurfaceSourceDecayRateCmd->SetGuidance("set the decay rate of the priamries in the fuel assesmbly, used to calculate"
                                            "the number of events to generate when running /dcs-monitor/gun/startSurfaceSourceRun");
    fSurfaceSourceDecayRateCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorMessenger::~PrimaryGeneratorMessenger()
{
    delete fDir;
    
    delete fSourceModeCmd;

    delete fCaskNumCmd;
    delete fFuelNumCmd;

    delete fCLYCBoundingRadiusCmd;

    delete fUseWattCmd;
    delete fWattACmd;
    delete fWattBCmd;

    delete fSurfaceSourceFileCmd;
    delete fSurfaceSourcePidCmd;
    delete fSurfaceSourceMaxEntriesCmd;
    delete fSmearPhiCmd;
    delete fSmearZCmd;
    delete fSmearAngleCmd;
    delete fSmearEfracCmd;

    delete fSurfaceSourceNumPrimariesCmd;
    delete fSurfaceSourceDecayRateCmd;

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
    if(command == fSourceModeCmd) {
        if (newValue == "GPS") {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Setting source mode to the G4GeneralParticleSource (kGPS)" << G4endl;
            fPrimaryGeneratorAction->SetSourceMode(kGPS);
        }
        else if (newValue == "CASTOR440_surface") {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Setting source mode to the CASTOR 440/84 surface flux (kCASTOR440_surface)" << G4endl;
            fPrimaryGeneratorAction->SetSourceMode(kCASTOR440_surface);
        }
        else if (newValue == "CASTOR440_surface_from_TTree") {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Setting source mode to the CASTOR 440/84 surface flux, sampled from a ROOT tree (kCASTOR440_surface_from_TTree)" << G4endl;
            fPrimaryGeneratorAction->SetSourceMode(kCASTOR440_surface_from_TTree);
        }
        else if (newValue == "CASTOR440_fuel") {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Setting source mode to CASTOR 440/84 fuel element volume flux (kCASTOR440_fuel)" << G4endl;
            fPrimaryGeneratorAction->SetSourceMode(kCASTOR440_fuel);
        }
        else if (newValue == "CASTOR440_fuel_biased") {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Setting source mode to CASTOR 440/84 fuel element volume flux with directional bias (kCASTOR440_fuel_biased)" << G4endl;
            fPrimaryGeneratorAction->SetSourceMode(kCASTOR440_fuel_biased);
        }
        else {
            if(G4Threading::G4GetThreadId() == 0) 
                G4cout << " --> Unknown source mode, returning..." << G4endl;
            return;
        }
    }

    if (command == fCaskNumCmd) fPrimaryGeneratorAction->SetCaskNum(fCaskNumCmd->GetNewIntValue(newValue));
    if (command == fFuelNumCmd) fPrimaryGeneratorAction->SetFuelNum(fFuelNumCmd->GetNewIntValue(newValue));
    
    if (command == fCLYCBoundingRadiusCmd) fPrimaryGeneratorAction->SetCLYCBoundingRadius(fCLYCBoundingRadiusCmd->GetNewDoubleValue(newValue));

    if (command == fUseWattCmd) {
        fPrimaryGeneratorAction->SetUseWattSpectrum(
            fUseWattCmd->GetNewBoolValue(newValue));
    }
    if (command == fWattACmd) {
        fPrimaryGeneratorAction->SetWattA(
            fWattACmd->GetNewDoubleValue(newValue));
    }
    if (command == fWattBCmd) {
        // 'b' is treated as plain numeric value in 1/MeV; convert to internal units.
        const G4double bInternal = fWattBCmd->GetNewDoubleValue(newValue) / MeV;
        fPrimaryGeneratorAction->SetWattB(bInternal);
    }

    // surface flux sampler
    if (command == fSurfaceSourceFileCmd) {
        if(G4Threading::G4GetThreadId() == 0) 
            G4cout << "[PrimaryGeneratorMessenger::SetNewValue] Setting the surface flux file to: " << newValue << G4endl;
        SurfaceFluxSampler::Instance().SetSourceFile(newValue);
    }
    if (command == fSurfaceSourcePidCmd) {
        fPrimaryGeneratorAction->SetSurfaceSourcePid(fSurfaceSourcePidCmd->GetNewIntValue(newValue));
    }
    if (command == fSurfaceSourceMaxEntriesCmd) {
        if(G4Threading::G4GetThreadId() == 0) 
            G4cout << "[PrimaryGeneratorMessenger::SetNewValue] Loading " << newValue << " entries from the input surface source file" << G4endl;
        SurfaceFluxSampler::Instance().SetMaxEntries(fSurfaceSourceMaxEntriesCmd->GetNewIntValue(newValue));
    }
    // smearing
    if (command == fSmearPhiCmd)
        SurfaceFluxSampler::Instance().SetSmearPhi(
            fSmearPhiCmd->GetNewDoubleValue(newValue));
    if (command == fSmearZCmd)
        SurfaceFluxSampler::Instance().SetSmearZ(
            fSmearZCmd->GetNewDoubleValue(newValue));
    if (command == fSmearAngleCmd)
        SurfaceFluxSampler::Instance().SetSmearAngle(
            fSmearAngleCmd->GetNewDoubleValue(newValue));
    if (command == fSmearEfracCmd)
        SurfaceFluxSampler::Instance().SetSmearEfrac(
            fSmearEfracCmd->GetNewDoubleValue(newValue));
    
    if(command == fSurfaceSourceNumPrimariesCmd) {
        SurfaceFluxSampler::Instance().SetNumPrimaries(fSurfaceSourceNumPrimariesCmd->GetNewIntValue(newValue));
    }
    if(command == fSurfaceSourceDecayRateCmd) {
        fPrimaryGeneratorAction->SetSurfaceSourceDecayRate(fSurfaceSourceDecayRateCmd->GetNewDoubleValue(newValue));
    }

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......


