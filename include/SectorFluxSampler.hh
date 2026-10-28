#ifndef SectorFluxSampler_h
#define SectorFluxSampler_h 1

#include "G4Types.hh"
#include "G4ThreeVector.hh"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Unit-weight replay sampler for a merged 'sectorFlux' ROOT file.
// The p1.5 producer has already folded all p1 weights into the entry list, so
// every entry is sampled uniformly and replayed with weight 1.
class SectorFluxSampler
{
  public:
    struct Entry {
        G4float ekin;
        G4float t;
        G4float x, y, z;
        G4float px, py, pz;
        G4int   pid;
        G4int   evtNb = 0;
        std::uint64_t sourceKey = 0;
        G4int   cellId = -1;
    };

    // How the loaded entries were selected.  This records the precedence that
    // was actually applied after the file branches and metadata were inspected.
    enum SelectionMode {
        kAllEntries = 0,
        kExplicitCells,
        kZLayerSector,
        kAutoCells
    };

    static SectorFluxSampler& Instance();

    void SetSourceFile(const std::string& f) { fSourceFile = f; }
    const std::string& GetSourceFile() const { return fSourceFile; }

    void SetSectorId(G4int v) { fSectorId = v; }
    G4int GetSectorId() const { return fSectorId; }

    void SetCellIds(const std::vector<G4int>& v) { fCellIds = v; }
    const std::vector<G4int>& GetCellIds() const { return fCellIds; }

    void SetZLayers(const std::vector<G4int>& v) { fZLayers = v; }
    const std::vector<G4int>& GetZLayers() const { return fZLayers; }

    void SetAutoCells(G4bool v) { fAutoCells = v; }
    G4bool GetAutoCells() const { return fAutoCells; }

    void SetMaxEntries(long int n) { fMaxEntries = n; }
    long int GetMaxEntries() const { return fMaxEntries; }

    void SetTgenSeconds(G4double v) { fTgenOverride = v; }
    G4double GetTgenSeconds() const { return fTgen; }

    void SetNumPrimariesOverride(long long n) { fNumPrimariesOverride = n; }

    G4bool ForceLoad();
    G4bool IsLoaded() const { return fLoaded; }
    long long NumEntries() const { return fM; }
    long long NumLoaded() const { return fMLoaded; }

    // Metadata read from sectorMeta (nSectors defaults to 8).
    G4int GetNSectors() const { return fNSectors; }
    G4int GetNZLayers() const { return fNZLayers; }
    G4double GetZMin() const { return fZMin; }
    G4double GetZMax() const { return fZMax; }
    G4double GetDz() const { return fDz; }

    SelectionMode GetSelectionMode() const { return fSelectionMode; }
    const std::vector<G4int>& GetSelectedCells() const { return fSelectedCells; }

    bool Sample(G4ThreeVector& pos,
                G4ThreeVector& dir,
                G4double& ekin,
                G4double& t,
                G4int& pid) const;

    void StartRun(G4double measurement_time);

  private:
    SectorFluxSampler() = default;
    SectorFluxSampler(const SectorFluxSampler&) = delete;
    SectorFluxSampler& operator=(const SectorFluxSampler&) = delete;

    void EnsureLoaded() const;
    void DoLoad();

    mutable std::once_flag fLoadOnce;
    std::string fSourceFile;
    std::string fTreeName = "sectorFlux";
    G4int       fSectorId = -1;
    long int    fMaxEntries = 0;
    G4double    fTgenOverride = 0.;
    long long   fNumPrimariesOverride = 0;

    // Selection requested through the messenger.
    std::vector<G4int> fCellIds;
    std::vector<G4int> fZLayers;
    G4bool             fAutoCells = false;

    // Metadata read from sectorMeta.
    G4int    fNSectors = 8;
    G4int    fNZLayers = 0;
    G4double fZMin = 0.;
    G4double fZMax = 0.;
    G4double fDz = 0.;

    // Selection actually applied by DoLoad.
    SelectionMode       fSelectionMode = kAllEntries;
    std::vector<G4int>  fSelectedCells;

    mutable G4bool fLoaded = false;
    mutable long long fMLoaded = 0;
    mutable long long fM = 0;
    mutable G4double fTgen = 0.;
    mutable std::vector<Entry> fEntries;
};

#endif
