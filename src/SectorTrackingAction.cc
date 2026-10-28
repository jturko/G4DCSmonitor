#include "SectorTrackingAction.hh"
#include "SectorTrackInfo.hh"

#include "G4Track.hh"

#include <bitset>

void SectorTrackingAction::PreUserTrackingAction(const G4Track* track)
{
    const G4int trackID = track->GetTrackID();

    // Track IDs restart at 1 for each event; clear the thread-local registry
    // when the new primary appears.
    if (trackID == 1) SectorTrackInfo::Reset();

    std::bitset<SectorTrackInfo::kMaxCells> mask;
    if (track->GetParentID() > 0) {
        mask = SectorTrackInfo::GetMask(track->GetParentID());
    }

    SectorTrackInfo::SetMask(trackID, mask);
}
