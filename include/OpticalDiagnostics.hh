#ifndef OpticalDiagnostics_h
#define OpticalDiagnostics_h 1

#include "globals.hh"

#include <algorithm>
#include <cstddef>
#include <vector>

// Per-event accounting of optical photons in the MuonScint slab.
// Thread-local so it composes with Geant4's event-level multithreading.
struct OpticalDiag {
    G4int nScint    = 0;  // scintillation photons generated
    G4int nCerenkov = 0;  // Cherenkov photons generated
    G4int nKilled   = 0;  // all optical photons reaching fStopAndKill
    G4int nDetected = 0;  // detected photoelectrons (passed the SiPM PDE)
    G4int nEscaped  = 0;  // killed while outside the detector (lost to world)

    // Per-slab primary-muon first-entry position (slab-local, mm), recorded on
    // the first step of the primary track inside each slab's scintillator LV.
    // Used to bin the light-collection efficiency maps by interaction position.
    std::vector<G4bool>   muonEntrySet;
    std::vector<G4double> muonEntryX;
    std::vector<G4double> muonEntryY;

    // Per-slab counters for the light-collection efficiency maps:
    //   nProducedPerSlab[i] = optical photons created in slab i's scintillator
    //   nDetectedPerSlab[i] = photoelectrons detected in slab i's SiPMs.
    std::vector<G4int> nProducedPerSlab;
    std::vector<G4int> nDetectedPerSlab;

    void EnsureSlabs(std::size_t n)
    {
        if (muonEntrySet.size() < n) {
            muonEntrySet.resize(n, false);
            muonEntryX.resize(n, 0.);
            muonEntryY.resize(n, 0.);
        }
        if (nProducedPerSlab.size() < n) nProducedPerSlab.resize(n, 0);
        if (nDetectedPerSlab.size() < n) nDetectedPerSlab.resize(n, 0);
    }

    void Reset()
    {
        nScint = nCerenkov = nKilled = nDetected = nEscaped = 0;
        for (std::size_t i = 0; i < muonEntrySet.size(); ++i) {
            muonEntrySet[i] = false;
            muonEntryX[i]   = 0.;
            muonEntryY[i]   = 0.;
        }
        std::fill(nProducedPerSlab.begin(),  nProducedPerSlab.end(),  0);
        std::fill(nDetectedPerSlab.begin(),  nDetectedPerSlab.end(),  0);
    }

    G4int NGenerated() const { return nScint + nCerenkov; }
};

inline G4ThreadLocal OpticalDiag gOpticalDiag;

#endif
