#ifndef SectorTrackInfo_h
#define SectorTrackInfo_h 1

#include "G4Types.hh"

#include <bitset>

// Thread-local per-track sector-cell bitmask. The tracking action seeds each
// new track from its parent's mask, so secondaries inherit the cells already
// visited by the parent. The scoring SD sets the bit for the first entry.
namespace SectorTrackInfo
{
    constexpr G4int kMaxCells = 256;

    void Reset();
    std::bitset<kMaxCells> GetMask(G4int trackID);
    void SetMask(G4int trackID, const std::bitset<kMaxCells>& mask);
    bool MarkCell(G4int trackID, G4int cellId);
}

#endif
