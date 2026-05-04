
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

void EventAction::EndOfEventAction(const G4Event *)
{
    //if (g_sigint_received != 0 && G4Threading::G4GetThreadId() == 0) {
    //    G4cout << "\n>>> Ctrl-C detected. Soft-aborting run after current event..." << G4endl;
    //    
    //    // true = soft abort. Allows the current event to finish and triggers EndOfRunAction 
    //    G4RunManager::GetRunManager()->AbortRun(true); 
    //    
    //    // Reset the flag to avoid repeated console prints across threads
    //    g_sigint_received = 0; 
    //}
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
