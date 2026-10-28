#ifndef SectorTrackingAction_h
#define SectorTrackingAction_h 1

#include "G4UserTrackingAction.hh"

class SectorTrackingAction : public G4UserTrackingAction
{
  public:
    SectorTrackingAction() = default;
    ~SectorTrackingAction() override = default;

    void PreUserTrackingAction(const G4Track*) override;
};

#endif
