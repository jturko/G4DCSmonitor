#ifndef SectorFluxSamplerMessenger_h
#define SectorFluxSamplerMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

class G4UIdirectory;
class G4UIcmdWithAString;
class G4UIcmdWithAnInteger;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithABool;

class SectorFluxSamplerMessenger : public G4UImessenger
{
  public:
    SectorFluxSamplerMessenger();
    ~SectorFluxSamplerMessenger() override;

    void SetNewValue(G4UIcommand*, G4String) override;

  private:
    G4UIdirectory* fDir = nullptr;

    G4UIcmdWithAString*        fFileCmd = nullptr;
    G4UIcmdWithAnInteger*      fSectorCmd = nullptr;
    G4UIcmdWithAString*        fCellIdsCmd = nullptr;
    G4UIcmdWithAString*        fZLayersCmd = nullptr;
    G4UIcmdWithABool*          fAutoCellsCmd = nullptr;
    G4UIcmdWithAnInteger*      fMaxEntriesCmd = nullptr;
    G4UIcmdWithAnInteger*      fNumPrimariesCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fTgenCmd = nullptr;
    G4UIcmdWithADoubleAndUnit* fStartRunCmd = nullptr;
};

#endif
