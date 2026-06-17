#include "SurfaceFluxSamplerMessenger.hh"
#include "SurfaceFluxSampler.hh"
#include "DetectorConstruction.hh"
#include "GeometryCASTOR440.hh"

#include "G4UIdirectory.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithADouble.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Exception.hh"

SurfaceFluxSamplerMessenger::SurfaceFluxSamplerMessenger()
{
    fDir = new G4UIdirectory("/dcs-monitor/surf/");
    fDir->SetGuidance("Master-side driver and configuration for SurfaceFluxSampler.");

    // -------- sampler configuration --------
    fFileCmd = new G4UIcmdWithAString("/dcs-monitor/surf/sourceFile", this);
    fFileCmd->SetGuidance("Path to step-1 ROOT file containing the 'surfaceFlux' tree.");
    fFileCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMaxEntriesLoadedFromTreeCmd = new G4UIcmdWithAnInteger("/dcs-monitor/surf/maxEntriesLoadedFromTree", this);
    fMaxEntriesLoadedFromTreeCmd->SetGuidance(
        "Cap on the number of entries read from the input ROOT tree at load time.\n"
        "  <= 0 (default) : load every entry\n"
        "  >  0           : load at most this many entries.\n"
        "Useful for quick-turn debugging when the step-1 file is huge.");
    fMaxEntriesLoadedFromTreeCmd->SetParameterName("nEntries", false);
    fMaxEntriesLoadedFromTreeCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fPidCmd = new G4UIcmdWithAnInteger("/dcs-monitor/surf/sourcePid", this);
    fPidCmd->SetGuidance("PDG-code filter: 0=any, 2112=neutron, 22=gamma.");
    fPidCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearPhiCmd = new G4UIcmdWithADouble("/dcs-monitor/surf/smearPhi", this);
    fSmearPhiCmd->SetGuidance("Gaussian sigma for azimuthal phi smearing [rad].");
    fSmearPhiCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearZCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/surf/smearZ", this);
    fSmearZCmd->SetGuidance("Gaussian sigma for axial z smearing.");
    fSmearZCmd->SetDefaultUnit("mm");
    fSmearZCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearAngleCmd = new G4UIcmdWithADouble("/dcs-monitor/surf/smearAngle", this);
    fSmearAngleCmd->SetGuidance("Gaussian sigma for small-angle direction smearing [rad].");
    fSmearAngleCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSmearEfracCmd = new G4UIcmdWithADouble("/dcs-monitor/surf/smearEfrac", this);
    fSmearEfracCmd->SetGuidance("Fractional sigma for kinetic-energy smearing.");
    fSmearEfracCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    // -------- run driver --------
    fCaskNumCmd = new G4UIcmdWithAnInteger("/dcs-monitor/surf/caskNum", this);
    fCaskNumCmd->SetGuidance("Cask index whose geometry to use for the surface filter.");
    fCaskNumCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSourceRotZCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/surf/sourceRotZ", this);
    fSourceRotZCmd->SetGuidance("Rotate the loaded surface source about +z (C6 hexant reuse).");
    fSourceRotZCmd->SetParameterName("phi", false);
    fSourceRotZCmd->SetUnitCategory("Angle");
    fSourceRotZCmd->SetDefaultUnit("deg");
    fSourceRotZCmd->AvailableForStates(G4State_Idle);

    fNumPrimariesCmd = new G4UIcmdWithAnInteger("/dcs-monitor/surf/numPrimaries", this);
    fNumPrimariesCmd->SetGuidance("Number of primaries that produced the input ROOT tree.");
    fNumPrimariesCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fDecayRateCmd = new G4UIcmdWithADouble("/dcs-monitor/surf/decayRate", this);
    fDecayRateCmd->SetGuidance("Decay rate in the fuel assembly [1/s].");
    fDecayRateCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fStartRunCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/surf/startRun", this);
    fStartRunCmd->SetGuidance("Start a surface-source run; N primaries auto-computed.");
    fStartRunCmd->SetDefaultUnit("s");
    fStartRunCmd->SetUnitCategory("Time");
    fStartRunCmd->AvailableForStates(G4State_Idle);
}

SurfaceFluxSamplerMessenger::~SurfaceFluxSamplerMessenger()
{
    delete fStartRunCmd;
    delete fDecayRateCmd;
    delete fNumPrimariesCmd;
    delete fCaskNumCmd;
    delete fSourceRotZCmd;
    delete fSmearEfracCmd;
    delete fSmearAngleCmd;
    delete fSmearZCmd;
    delete fSmearPhiCmd;
    delete fPidCmd;
    delete fFileCmd;
    delete fDir;
    delete fMaxEntriesLoadedFromTreeCmd;
}

void SurfaceFluxSamplerMessenger::SetNewValue(G4UIcommand* cmd, G4String val)
{
    auto& s = SurfaceFluxSampler::Instance();
    
    if (cmd == fMaxEntriesLoadedFromTreeCmd) {
        s.SetSurfaceSourceMaxEntriesLoadedFromTree(
            fMaxEntriesLoadedFromTreeCmd->GetNewIntValue(val));
        return;
    }

    // ---- sampler configuration (single source of truth: master) ----
    if (cmd == fFileCmd)        { s.SetSourceFile(val); return; }
    if (cmd == fPidCmd)         { /* PID filter is consumed by Sample(); store on sampler */ 
                                  // We piggy-back on fCaskNum-style state: keep it on the
                                  // sampler so workers read it too.
                                  // Easiest: extend SurfaceFluxSampler with SetRequestedPid,
                                  // but for now we cache here and use it from StartRun.
                                  // -> see step 2 below.
                                  // For now: parse and store via a helper:
                                  s.SetRequestedPid(fPidCmd->GetNewIntValue(val));
                                  return; }
    if (cmd == fSmearPhiCmd)    { s.SetSmearPhi  (fSmearPhiCmd->GetNewDoubleValue(val)); return; }
    if (cmd == fSmearZCmd)      { s.SetSmearZ    (fSmearZCmd  ->GetNewDoubleValue(val)); return; }
    if (cmd == fSmearAngleCmd)  { s.SetSmearAngle(fSmearAngleCmd->GetNewDoubleValue(val)); return; }
    if (cmd == fSmearEfracCmd)  { s.SetSmearEfrac(fSmearEfracCmd->GetNewDoubleValue(val)); return; }

    // ---- driver state ----
    if (cmd == fCaskNumCmd)       { 
        fCaskNum = fCaskNumCmd->GetNewIntValue(val); 
        s.SetPlacementCask(fCaskNum);                  
        return; 
    }
    if (cmd == fSourceRotZCmd) {
        s.SetSourceRotZ(fSourceRotZCmd->GetNewDoubleValue(val));  
        return;
    }

    if (cmd == fNumPrimariesCmd)  { s.SetNumPrimaries(fNumPrimariesCmd->GetNewIntValue(val)); return; }
    if (cmd == fDecayRateCmd)     { s.SetDecayRate(fDecayRateCmd->GetNewDoubleValue(val)); return; }

    if (cmd == fStartRunCmd) {
        const G4double measurement_time = fStartRunCmd->GetNewDoubleValue(val);

        // Push current cask cylinder dimensions into the sampler.
        auto* rm  = G4RunManager::GetRunManager();
        auto* dc  = const_cast<G4VUserDetectorConstruction*>(rm->GetUserDetectorConstruction());
        auto* det = dynamic_cast<DetectorConstruction*>(dc);
        if (!det) {
            G4Exception("SurfaceFluxSamplerMessenger::SetNewValue",
                        "NoDetector", FatalException,
                        "DetectorConstruction not available.");
            return;
        }
        auto* cask = det->GetCASTOR440(fCaskNum);
        if (!cask) {
            G4Exception("SurfaceFluxSamplerMessenger::SetNewValue",
                        "NoCask", FatalException,
                        "Cask not constructed. Did /run/initialize run first?");
            return;
        }
        s.SetGeometryParameters(cask->GetCaskOuterRadius() / CLHEP::mm,
                                cask->GetCaskHeight()      / CLHEP::mm,
                                /*tol mm*/ 2.0);

        s.StartRun(measurement_time);
    }
}

