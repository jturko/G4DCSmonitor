#include "SectorFluxSamplerMessenger.hh"
#include "SectorFluxSampler.hh"

#include "G4Exception.hh"
#include "G4UIcmdWithAString.hh"
#include "G4UIcmdWithABool.hh"
#include "G4UIcmdWithAnInteger.hh"
#include "G4UIcmdWithADoubleAndUnit.hh"
#include "G4UIdirectory.hh"
#include "G4SystemOfUnits.hh"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Accept comma-separated or whitespace-separated integers.  An empty string
// yields an empty list, which clears the requested selection.
std::vector<G4int> ParseIntList(const G4String& input)
{
    std::vector<G4int> values;

    std::string normalised = input;
    std::replace(normalised.begin(), normalised.end(), ',', ' ');

    std::istringstream stream(normalised);
    std::string token;
    while (stream >> token) {
        long value = 0;
        bool ok = false;
        try {
            std::size_t consumed = 0;
            value = std::stol(token, &consumed);
            ok = (consumed == token.size());
        } catch (const std::exception&) {
            ok = false;
        }

        if (!ok ||
            value < std::numeric_limits<G4int>::min() ||
            value > std::numeric_limits<G4int>::max()) {
            G4Exception(
                "SectorFluxSamplerMessenger::ParseIntList", "BadIntList",
                JustWarning,
                ("Ignoring invalid integer token '" + token +
                 "' in list '" + input + "'.").c_str());
            continue;
        }
        values.push_back(static_cast<G4int>(value));
    }

    return values;
}

} // namespace

SectorFluxSamplerMessenger::SectorFluxSamplerMessenger()
{
    fDir = new G4UIdirectory("/dcs-monitor/bkg/");
    fDir->SetGuidance("Unit-weight background-sector replay driver.");

    fFileCmd = new G4UIcmdWithAString("/dcs-monitor/bkg/sourceFile", this);
    fFileCmd->SetGuidance(
        "ROOT file containing the 'sectorFlux' tree (merged per sector).");
    fFileCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fSectorCmd = new G4UIcmdWithAnInteger("/dcs-monitor/bkg/sectorId", this);
    fSectorCmd->SetGuidance(
        "Sector to replay (0-7); -1 loads every sector in the file.");
    fSectorCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fCellIdsCmd = new G4UIcmdWithAString("/dcs-monitor/bkg/cellIds", this);
    fCellIdsCmd->SetGuidance(
        "Comma-separated cell ids to replay, e.g. 24,25,32,33; empty clears.");
    fCellIdsCmd->SetParameterName("cellIds", true);
    fCellIdsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fZLayersCmd = new G4UIcmdWithAString("/dcs-monitor/bkg/zLayers", this);
    fZLayersCmd->SetGuidance(
        "Comma-separated z-layer indices to replay; empty clears.");
    fZLayersCmd->SetParameterName("zLayers", true);
    fZLayersCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fAutoCellsCmd = new G4UIcmdWithABool("/dcs-monitor/bkg/autoCells", this);
    fAutoCellsCmd->SetGuidance(
        "Select every scoring cell that overlaps the CLYC detector envelope.");
    fAutoCellsCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fMaxEntriesCmd = new G4UIcmdWithAnInteger("/dcs-monitor/bkg/maxEntries", this);
    fMaxEntriesCmd->SetGuidance(
        "Cap on loaded entries (<=0 loads all); useful for pilot tests.");
    fMaxEntriesCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fTgenCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/bkg/tgen", this);
    fTgenCmd->SetGuidance(
        "Override T_gen from sectorMeta (e.g. 180 s).");
    fTgenCmd->SetDefaultUnit("s");
    fTgenCmd->SetUnitCategory("Time");
    fTgenCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fNumPrimariesCmd = new G4UIcmdWithAnInteger("/dcs-monitor/bkg/numPrimaries", this);
    fNumPrimariesCmd->SetGuidance(
        "Optional explicit replay count; <=0 uses M*T_replay/T_gen.");
    fNumPrimariesCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

    fStartRunCmd = new G4UIcmdWithADoubleAndUnit("/dcs-monitor/bkg/startRun", this);
    fStartRunCmd->SetGuidance(
        "Start a unit-weight background replay run for the requested measurement time.");
    fStartRunCmd->SetDefaultUnit("s");
    fStartRunCmd->SetUnitCategory("Time");
    fStartRunCmd->AvailableForStates(G4State_Idle);
}

SectorFluxSamplerMessenger::~SectorFluxSamplerMessenger()
{
    delete fStartRunCmd;
    delete fNumPrimariesCmd;
    delete fTgenCmd;
    delete fMaxEntriesCmd;
    delete fAutoCellsCmd;
    delete fZLayersCmd;
    delete fCellIdsCmd;
    delete fSectorCmd;
    delete fFileCmd;
    delete fDir;
}

void SectorFluxSamplerMessenger::SetNewValue(G4UIcommand* cmd, G4String val)
{
    auto& sampler = SectorFluxSampler::Instance();

    if (cmd == fFileCmd)       { sampler.SetSourceFile(val); return; }
    if (cmd == fSectorCmd)     { sampler.SetSectorId(fSectorCmd->GetNewIntValue(val)); return; }
    if (cmd == fCellIdsCmd)    { sampler.SetCellIds(ParseIntList(val)); return; }
    if (cmd == fZLayersCmd)    { sampler.SetZLayers(ParseIntList(val)); return; }
    if (cmd == fAutoCellsCmd)  { sampler.SetAutoCells(fAutoCellsCmd->GetNewBoolValue(val)); return; }
    if (cmd == fMaxEntriesCmd) { sampler.SetMaxEntries(fMaxEntriesCmd->GetNewIntValue(val)); return; }
    if (cmd == fTgenCmd)       { sampler.SetTgenSeconds(fTgenCmd->GetNewDoubleValue(val) / s); return; }
    if (cmd == fNumPrimariesCmd) {
        sampler.SetNumPrimariesOverride(fNumPrimariesCmd->GetNewIntValue(val));
        return;
    }
    if (cmd == fStartRunCmd) {
        sampler.StartRun(fStartRunCmd->GetNewDoubleValue(val));
    }
}
