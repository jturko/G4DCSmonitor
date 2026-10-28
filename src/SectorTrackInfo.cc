#include "SectorTrackInfo.hh"

#include <unordered_map>

namespace
{
    thread_local std::unordered_map<G4int, std::bitset<SectorTrackInfo::kMaxCells>>
        gSectorMasks;
}

void SectorTrackInfo::Reset()
{
    gSectorMasks.clear();
}

std::bitset<SectorTrackInfo::kMaxCells> SectorTrackInfo::GetMask(G4int trackID)
{
    auto it = gSectorMasks.find(trackID);
    if (it == gSectorMasks.end()) return {};
    return it->second;
}

void SectorTrackInfo::SetMask(G4int trackID,
                              const std::bitset<SectorTrackInfo::kMaxCells>& mask)
{
    gSectorMasks[trackID] = mask;
}

bool SectorTrackInfo::MarkCell(G4int trackID, G4int cellId)
{
    if (cellId < 0 || cellId >= kMaxCells) return false;

    auto& mask = gSectorMasks[trackID];
    if (mask.test((size_t)cellId)) return false;
    mask.set((size_t)cellId);
    return true;
}
