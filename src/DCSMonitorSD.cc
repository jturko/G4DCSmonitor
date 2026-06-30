#include "DCSMonitorSD.hh"

#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4Track.hh"
#include "G4VPhysicalVolume.hh"
#include "G4LogicalVolume.hh"
#include "G4ParticleDefinition.hh"
#include "G4ThreeVector.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "G4ios.hh"

#include "G4AnalysisManager.hh"

#include <algorithm>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

DCSMonitorSD::DCSMonitorSD(const G4String& name, const G4String& hitsCollectionName)
    : G4VSensitiveDetector(name)
{
    collectionName.insert(hitsCollectionName);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DCSMonitorSD::Initialize(G4HCofThisEvent* hce)
{
    fHitsCollection =
        new DCSMonitorHitsCollection(SensitiveDetectorName, collectionName[0]);

    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
    hce->AddHitsCollection(hcID, fHitsCollection);

    fTrackShowerMap.clear();
    fShowers.clear();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool DCSMonitorSD::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    const G4int trackID  = step->GetTrack()->GetTrackID();
    const G4int parentID = step->GetTrack()->GetParentID();

    const G4bool isEntering =
        (step->GetPreStepPoint()->GetStepStatus() == fGeomBoundary);

    // ---- 1. Resolve which shower this step belongs to --------------------
    G4int showerIdx = -1;

    auto itTrk = fTrackShowerMap.find(trackID);
    if (itTrk != fTrackShowerMap.end()) {
        // Track is already attached to a shower (e.g. it entered earlier, or
        // it exited and re-entered the same crystal).
        showerIdx = itTrk->second;
    }
    else if (!isEntering) {
        // Track was BORN inside the crystal -> physical secondary of its
        // parent's shower. Same statistical realization, same weight.
        auto itPar = fTrackShowerMap.find(parentID);
        if (itPar != fTrackShowerMap.end()) {
            showerIdx = itPar->second;
            fTrackShowerMap[trackID] = showerIdx;
        }
    }

    // ---- 2. No shower yet -> this is an INDEPENDENT entrant. -------------
    // Every track crossing into the crystal from outside -- including each
    // separate geometric-biasing clone of the same primary -- opens its own
    // shower with its OWN weight. These showers are never merged together.
    if (showerIdx == -1) {
        Shower s;
        s.weight = step->GetPreStepPoint()->GetWeight();

        const G4LogicalVolume* lv =
            step->GetPreStepPoint()->GetPhysicalVolume()->GetLogicalVolume();
        auto itDet = fDetMap.find(lv);
        s.det = (itDet != fDetMap.end()) ? itDet->second : -1;

        showerIdx = static_cast<G4int>(fShowers.size());
        fShowers.push_back(std::move(s));
        fTrackShowerMap[trackID] = showerIdx;
    }

    // ---- 3. Record the deposit (if any) into this shower -----------------
    const G4double edep = step->GetTotalEnergyDeposit();
    if (edep > 0.) {
        Deposit d;
        d.t    = step->GetPreStepPoint()->GetGlobalTime();
        d.edep = edep;
        d.pos  = step->GetPostStepPoint()->GetPosition();
        d.pid  = step->GetTrack()->GetParticleDefinition()->GetPDGEncoding();
        fShowers[showerIdx].deps.push_back(d);
    }

    return true;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void DCSMonitorSD::EndOfEvent(G4HCofThisEvent*)
{
    auto* analysis = G4AnalysisManager::Instance();
    const G4int idx = 1;   // 'hits' ntuple

    const G4int evtNb = G4EventManager::GetEventManager()
                          ->GetConstCurrentEvent()->GetEventID();

    // Each shower is one independent, single-weight realization. We form
    // pulses by time-clustering WITHIN a shower only. Because distinct showers
    // are processed in separate iterations, deposits from different biasing
    // clones are structurally prevented from ever being summed.
    for (auto& sh : fShowers) {
        if (sh.deps.empty()) continue;

        std::sort(sh.deps.begin(), sh.deps.end(),
                  [](const Deposit& a, const Deposit& b){ return a.t < b.t; });

        std::size_t i = 0;
        while (i < sh.deps.size()) {
            const G4double tLead = sh.deps[i].t;     // pulse leading-edge time
            G4double      sumE = 0.0, eMax = -1.0;
            G4int         pidPulse = sh.deps[i].pid;  // pid of largest deposit
            G4ThreeVector cen(0., 0., 0.);

            G4double    tPrev = sh.deps[i].t;
            std::size_t j     = i;

            // Gap-based clustering: a new pulse begins when the gap to the
            // PREVIOUS deposit exceeds the resolving time. (For a fixed
            // integration gate instead, test (sh.deps[j].t - tLead).)
            while (j < sh.deps.size() &&
                   (sh.deps[j].t - tPrev) <= fResolvingTime) {
                const Deposit& d = sh.deps[j];
                sumE += d.edep;
                cen  += d.edep * d.pos;
                if (d.edep > eMax) { eMax = d.edep; pidPulse = d.pid; }
                tPrev = d.t;
                ++j;
            }

            if (sumE > 0.) cen /= sumE;

            analysis->FillNtupleDColumn(idx, 0, (G4double)pidPulse);
            analysis->FillNtupleDColumn(idx, 1, sumE);
            analysis->FillNtupleDColumn(idx, 2, tLead);
            analysis->FillNtupleDColumn(idx, 3, cen.x());
            analysis->FillNtupleDColumn(idx, 4, cen.y());
            analysis->FillNtupleDColumn(idx, 5, cen.z());
            analysis->FillNtupleIColumn(idx, 6, sh.det);
            analysis->FillNtupleDColumn(idx, 7, sh.weight);   
            analysis->FillNtupleDColumn(idx, 8, (G4double)evtNb);
            analysis->AddNtupleRow(idx);

            // Mirror each pulse into the hits collection so that
            // EventAction::EndOfEventAction's writePrimaryOnlyOnHit check
            // (which scans for any GetEdep() > 0) keeps working unchanged.
            // NOTE: the SD's EndOfEvent runs BEFORE the user EventAction's,
            // so this is visible to it.
            auto* hit = new DCSMonitorHit();
            hit->SetEdep(sumE);
            hit->SetTime(tLead);
            hit->SetPos(cen);
            hit->SetPID(pidPulse);
            hit->SetDetNum(sh.det);
            hit->SetWeight(sh.weight);
            fHitsCollection->insert(hit);

            i = j;
        }
    }
}

