
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "EventAction.hh"
#include "RunAction.hh"

#include "G4SDManager.hh"
#include "G4HCofThisEvent.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4UnitsTable.hh"

#include "G4RunManager.hh"
#include "G4Run.hh"

#include "DCSMonitorHit.hh"
#include "G4PrimaryVertex.hh"
#include "G4PrimaryParticle.hh"

//#include "G4RootAnalysisManager.hh"
//#include "g4root.hh"

#include "G4AnalysisManager.hh"

#include "ProgressBar.hh"
#include "G4Threading.hh"

//
#include <csignal>
extern volatile std::sig_atomic_t g_sigint_received;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

EventAction::EventAction(RunAction* runAct)
    :fRunAction(runAct)
{
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

EventAction::~EventAction()
{
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::BeginOfEventAction(const G4Event* evt)
{
    //fEvtNb = evt->GetEventID();
    //fRunAction->GetProgBar()->Print(fEvtNb);
    
    uint64_t thisEventNumber = ProgressBar::gEvtNb.fetch_add(1, std::memory_order_relaxed) + 1;
    if(G4Threading::G4GetThreadId() == 0) {
        fRunAction->GetProgBar()->Print(thisEventNumber);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event * evt)
{
    const G4bool writeAll   = RunAction::WritePrimaryTree;      
    const G4bool writeOnHit = RunAction::WritePrimaryTreeOnlyOnHit; 

    // Neither mode requested -> nothing to write.
    if (!writeAll && !writeOnHit) return;

    G4bool doWrite = false;

    if (writeAll) {
        doWrite = true;
    } else {
        G4int hitsCollID = G4SDManager::GetSDMpointer()
                          ->GetCollectionID("DCSHitsCollection");
        if (hitsCollID < 0) {
        }

        G4HCofThisEvent* hce = evt->GetHCofThisEvent();
        if (hce && hitsCollID >= 0) {
            auto* hits = static_cast<DCSMonitorHitsCollection*>(
                             hce->GetHC(hitsCollID));
            if (hits) {
                for (std::size_t i = 0; i < hits->entries(); ++i) {
                    if ((*hits)[i]->GetEdep() > 0.) { doWrite = true; break; }
                }
            }
        }
    }

    if (!doWrite) return;

    // --- Fill the 'primary' ntuple (id 0) ---
    G4PrimaryVertex* vertex =
        evt->GetPrimaryVertex(evt->GetNumberOfPrimaryVertex() - 1);
    if (!vertex) return;
    G4PrimaryParticle* primary = vertex->GetPrimary(0);
    if (!primary) return;

    G4int         pid  = primary->GetPDGcode();
    G4double      ekin = primary->GetKineticEnergy();
    G4double      time = vertex->GetT0();
    G4ThreeVector pos  = vertex->GetPosition();
    G4ThreeVector mom  = primary->GetMomentumDirection();

    G4AnalysisManager* analysis = G4AnalysisManager::Instance();
    const G4int idx = 0;
    analysis->FillNtupleDColumn(idx, 0, pid);
    analysis->FillNtupleDColumn(idx, 1, ekin);
    analysis->FillNtupleDColumn(idx, 2, time);
    analysis->FillNtupleDColumn(idx, 3, pos.x());
    analysis->FillNtupleDColumn(idx, 4, pos.y());
    analysis->FillNtupleDColumn(idx, 5, pos.z());
    analysis->FillNtupleDColumn(idx, 6, mom.x());
    analysis->FillNtupleDColumn(idx, 7, mom.y());
    analysis->FillNtupleDColumn(idx, 8, mom.z());
    analysis->AddNtupleRow(idx);


}

//....oooOO0OOooo........oooOO0OOooo........oooOO0Oearooo........oooOO0OOooo......
