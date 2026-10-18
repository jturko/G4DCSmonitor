
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "EventAction.hh"
#include "RunAction.hh"
#include "OpticalDiagnostics.hh"

#include "G4SDManager.hh"
#include "G4HCofThisEvent.hh"

#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4TrajectoryContainer.hh"
#include "G4VTrajectory.hh"
#include "G4UnitsTable.hh"

#include "G4RunManager.hh"
#include "G4Run.hh"
#include "G4AnalysisManager.hh"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
    // Never carry detected track IDs across events (e.g. after an aborted event).
    gOpticalDiag.detectedTrackIDs.clear();

    //fEvtNb = evt->GetEventID();
    //fRunAction->GetProgBar()->Print(fEvtNb);
    
    uint64_t thisEventNumber = ProgressBar::gEvtNb.fetch_add(1, std::memory_order_relaxed) + 1;
    if(G4Threading::G4GetThreadId() == 0) {
        fRunAction->GetProgBar()->Print(thisEventNumber);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void EventAction::EndOfEventAction(const G4Event* evt)
{
    // Optical-photon accounting row (ntuple id 4: primary/hits/surfaceFlux/sipmHits/opticalStats)
    if (evt) {
        G4AnalysisManager* analysis = G4AnalysisManager::Instance();
        const G4int idx = 4;
        const G4int nAlive = std::max(0, gOpticalDiag.NGenerated() - gOpticalDiag.nKilled);
        analysis->FillNtupleIColumn(idx, 0, evt->GetEventID());
        analysis->FillNtupleIColumn(idx, 1, gOpticalDiag.nScint);
        analysis->FillNtupleIColumn(idx, 2, gOpticalDiag.nCerenkov);
        analysis->FillNtupleIColumn(idx, 3, gOpticalDiag.nKilled);
        analysis->FillNtupleIColumn(idx, 4, gOpticalDiag.nDetected);
        analysis->FillNtupleIColumn(idx, 5, gOpticalDiag.nEscaped);
        analysis->FillNtupleIColumn(idx, 6, nAlive);
        analysis->AddNtupleRow(idx);
    }

    // Visualization-only mode: keep exactly the optical-photon trajectories
    // that produced a SiPM hit this event. The pruning must happen here (not
    // in the SD): trajectories are only inserted into the container when each
    // track ends, and G4VisManager draws the event after EndOfEventAction
    // (on the state change to GeomClosed).
    if (evt && RunAction::ShowDetectedPhotonsOnly) {
        G4TrajectoryContainer* tc = evt->GetTrajectoryContainer();
        if (tc) {
            TrajectoryVector* v = tc->GetVector();
            std::unordered_map<G4int, G4VTrajectory*> byId;
            byId.reserve(v->size());
            for (G4VTrajectory* t : *v) {
                if (t) byId[t->GetTrackID()] = t;
            }

            // Keep each detected photon and its full ancestry.  The optical
            // photon that is absorbed by G4OpWLS is killed, so its track ID is
            // not in detectedTrackIDs; walking GetParentID() puts that creator
            // photon (and, for scintillation, the primary muon) back into the
            // event.  This also keeps multi-step WLS chains.
            std::unordered_set<G4int> keep;
            keep.reserve(gOpticalDiag.detectedTrackIDs.size() * 4);
            for (G4int id : gOpticalDiag.detectedTrackIDs) {
                G4int cur = id;
                while (cur > 0 && keep.insert(cur).second) {
                    auto it = byId.find(cur);
                    if (it == byId.end()) break;
                    const G4int parent = it->second->GetParentID();
                    if (parent <= 0) break;
                    cur = parent;
                }
            }

            TrajectoryVector kept;
            kept.reserve(keep.size());
            for (G4VTrajectory* t : *v) {
                if (t && keep.find(t->GetTrackID()) != keep.end()) {
                    kept.push_back(t);
                } else {
                    delete t;
                }
            }
            *v = kept;
        }
    }

    gOpticalDiag.Reset();

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
