#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

#include <time.h>

class RunAction;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

class EventAction : public G4UserEventAction
{
  public:
    EventAction(RunAction* runAct);
   ~EventAction();

  public:
    void BeginOfEventAction(const G4Event*);
    void   EndOfEventAction(const G4Event*);

    RunAction* GetRunAction() { return fRunAction; }
    G4int GetEventNumber() {    return fEvtNb;     }

  private:
    G4int                       fEvtNb;
    RunAction*                  fRunAction;

    // For time remaining calculation
    int fNEvents;
    clock_t fOverallBeginClock;
    clock_t fLastPrintClock;
    int fLastPrintEvents;
};

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#endif

    
