//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

#include "MuonScintSiPMSD.hh"

#include "G4HCofThisEvent.hh"
#include "G4SDManager.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4Track.hh"
#include "G4TouchableHandle.hh"
#include "G4OpticalPhoton.hh"
#include "G4ParticleDefinition.hh"
#include "G4ThreeVector.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "Randomize.hh"

#include "G4AnalysisManager.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

MuonScintSiPMSD::MuonScintSiPMSD(const G4String& name,
                                 const G4String& hitsCollectionName)
    : G4VSensitiveDetector(name)
{
    collectionName.insert(hitsCollectionName);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void MuonScintSiPMSD::Initialize(G4HCofThisEvent* hce)
{
    fHitsCollection = new MuonScintHitsCollection(SensitiveDetectorName,
                                                  collectionName[0]);
    G4int hcID = G4SDManager::GetSDMpointer()->GetCollectionID(collectionName[0]);
    hce->AddHitsCollection(hcID, fHitsCollection);

    fSiPMHitIndexMap.clear();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4bool MuonScintSiPMSD::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    G4Track* track = step->GetTrack();

    // 1) Optical photons only.
    if (track->GetDefinition() != G4OpticalPhoton::OpticalPhoton()) return false;

    G4StepPoint* preStep  = step->GetPreStepPoint();
    G4StepPoint* postStep = step->GetPostStepPoint();

    // 2) We get called for *every* step on the SiPM volume. We only care
    //    about steps that LAND on the grease<->SiPM boundary. Detection is
    //    signalled by OpBoundaryProcess setting the post-step process
    //    status to "Detection" (track is also flagged fStopAndKill).
    //
    //    The boundary status is exposed via G4OpBoundaryProcess; rather
    //    than chase that pointer, we use the simpler proxy: the photon
    //    just got killed AND the post-step is on a geometry boundary AND
    //    the post-step volume's LV is the SiPM LV.
    if (postStep->GetStepStatus() != fGeomBoundary) return false;
    if (track->GetTrackStatus() != fStopAndKill)   return false;

    // 3) Identify (det, sipm) from the POST-step touchable: the photon is
    //    crossing INTO the SiPM volume right now.
    const G4VTouchable* postTouch = postStep->GetTouchable();
    G4VPhysicalVolume*  postPV    = postStep->GetPhysicalVolume();
    if (!postPV || postPV->GetLogicalVolume()->GetName() != "SiPMLV") return false;

    const G4int sipmCopy = postTouch->GetCopyNumber(0);
    // For external-SiPM design, SiPMs are siblings of the slab in the world;
    // we don't have a "parent slab copy" available from the touchable. If
    // you support multiple slabs, you'll need a SiPM->slab mapping stored
    // in GeometryMuonScint. For a single-slab setup, det = 0 is fine.
    const G4int detCopy = 0;

    const G4int key = (detCopy << 16) | (sipmCopy & 0xFFFF);

    // 4) Find or create the aggregate hit.
    G4int hitIdx = -1;
    auto it = fSiPMHitIndexMap.find(key);
    if (it != fSiPMHitIndexMap.end()) {
        hitIdx = it->second;
    } else {
        auto* newHit = new MuonScintHit();
        newHit->SetDetNum (detCopy);
        newHit->SetSiPMNum(sipmCopy);
        newHit->SetWeight (preStep->GetWeight());
        hitIdx = fHitsCollection->insert(newHit) - 1;
        fSiPMHitIndexMap[key] = hitIdx;
    }
    auto* hit = (*fHitsCollection)[hitIdx];

    // Every photon that reached this point was absorbed by the photocathode
    // (i.e. detected, since EFFICIENCY = PDE and REFLECTIVITY = 0).
    hit->AddIncident();

    const G4double t   = preStep->GetGlobalTime();
    const G4double E   = track->GetKineticEnergy();
    const G4double lam = (E > 0.)
        ? (CLHEP::h_Planck * CLHEP::c_light / E) / CLHEP::nanometer
        : 0.;

    hit->AddDetected(t, lam, postStep->GetPosition());
    return true;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void MuonScintSiPMSD::EndOfEvent(G4HCofThisEvent*)
{
    const std::size_t nofHits = fHitsCollection->entries();

    if (verboseLevel > 1) {
        G4cout << "\n -- MuonScintSiPMSD '" << SensitiveDetectorName
               << "': " << nofHits << " SiPM(s) hit this event:" << G4endl;
        for (std::size_t i = 0; i < nofHits; ++i) (*fHitsCollection)[i]->Print();
    }

    if (nofHits == 0) return;

    G4AnalysisManager* analysis = G4AnalysisManager::Instance();
    const G4int idx = fNtupleId;
    const G4int eventNb = G4EventManager::GetEventManager()
                          ->GetConstCurrentEvent()->GetEventID();

    for (std::size_t i = 0; i < nofHits; ++i) {
        const auto* h = (*fHitsCollection)[i];

        // Skip SiPMs that had photons enter but none detected -- typically
        // not interesting and would otherwise inflate the ntuple.
        if (h->GetNDetected() == 0) continue;

        analysis->FillNtupleIColumn(idx, 0, eventNb);
        analysis->FillNtupleIColumn(idx, 1, h->GetDetNum());
        analysis->FillNtupleIColumn(idx, 2, h->GetSiPMNum());
        analysis->FillNtupleIColumn(idx, 3, h->GetNDetected());
        analysis->FillNtupleIColumn(idx, 4, h->GetNIncident());
        analysis->FillNtupleDColumn(idx, 5, h->GetTFirst());
        analysis->FillNtupleDColumn(idx, 6, h->GetMeanWavelength());
        analysis->FillNtupleDColumn(idx, 7, h->GetRMSWavelength());
        analysis->FillNtupleDColumn(idx, 8, h->GetWeight());
        analysis->AddNtupleRow(idx);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

