#ifndef SurfaceFluxSamplerMessenger_h
#define SurfaceFluxSamplerMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWithADouble;
class G4UIcmdWithADoubleAndUnit;

class SurfaceFluxSamplerMessenger : public G4UImessenger
{
  public:
    SurfaceFluxSamplerMessenger();
    ~SurfaceFluxSamplerMessenger() override;

    void SetNewValue(G4UIcommand*, G4String) override;

  private:
    // Held only because UI commands arrive separately.
    G4int    fCaskNum   = 0;
    G4double fDecayRate = 0.0;   // [1/s]

    G4UIdirectory*             fDir = nullptr;
    G4UIcmdWithAnInteger* fMaxEntriesLoadedFromTreeCmd = nullptr;

    // sampler configuration
    G4UIcmdWithAString*        fFileCmd          = nullptr;
    G4UIcmdWithAnInteger*      fPidCmd           = nullptr;
    G4UIcmdWithAnInteger*      fMaxEntriesCmd    = nullptr;
    G4UIcmdWithADouble*        fSmearPhiCmd      = nullptr;
    G4UIcmdWithADoubleAndUnit* fSmearZCmd        = nullptr;
    G4UIcmdWithADouble*        fSmearAngleCmd    = nullptr;
    G4UIcmdWithADouble*        fSmearEfracCmd    = nullptr;

    // run driver
    G4UIcmdWithAnInteger*      fCaskNumCmd       = nullptr;
    G4UIcmdWithAnInteger*      fNumPrimariesCmd  = nullptr;
    G4UIcmdWithADouble*        fDecayRateCmd     = nullptr;
    G4UIcmdWithADoubleAndUnit* fStartRunCmd      = nullptr;
};

#endif

