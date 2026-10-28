#include "SectorScoringSD.hh"

#include "DetectorConstruction.hh"
#include "GeometrySectorScoring.hh"
#include "SectorTrackInfo.hh"
#include "SurfaceFluxSampler.hh"

#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4EventManager.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4SystemOfUnits.hh"
#include "G4Track.hh"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr G4int kSectorFluxNtupleId = 3;  // primary, hits, surfaceFlux, sectorFlux
}

SectorScoringSD::SectorScoringSD(const G4String& name,
                                 DetectorConstruction* det)
    : G4VSensitiveDetector(name), fDetector(det)
{}

G4bool SectorScoringSD::ProcessHits(G4Step* step, G4TouchableHistory*)
{
    G4Track* track = step->GetTrack();
    const G4int pid = track->GetParticleDefinition()->GetPDGEncoding();

    // Match the p1 surface-flux source: only gammas and neutrons are useful
    // for the downstream background replay.
    if (pid != 22 && pid != 2112) return false;

    G4StepPoint* pre = step->GetPreStepPoint();
    if (!pre || !pre->GetPhysicalVolume()) return false;
    if (pre->GetStepStatus() != fGeomBoundary) return false;

    const G4int cellId = pre->GetPhysicalVolume()->GetCopyNo();
    const G4int nZLayers = fDetector->GetSectorZLayers();
    const G4int nCells = nZLayers * GeometrySectorScoring::kNumSectors;
    if (cellId < 0 || cellId >= nCells) return false;

    const G4int sector = cellId % GeometrySectorScoring::kNumSectors;
    const G4int zLayer = cellId / GeometrySectorScoring::kNumSectors;

    const G4int trackID = track->GetTrackID();
    if (!SectorTrackInfo::MarkCell(trackID, cellId)) return false;

    G4ThreeVector direction = pre->GetMomentum().unit();
    if (direction.mag2() <= 0.) return false;

    const G4ThreeVector globalPos = pre->GetPosition();
    const G4int evtNb = G4EventManager::GetEventManager()
                          ->GetConstCurrentEvent()->GetEventID();

    auto& source = SurfaceFluxSampler::Instance();
    G4int hexant = (G4int)std::lround(source.GetSourceRotZ() / (60. * deg));
    hexant = ((hexant % 6) + 6) % 6;

    auto* analysis = G4AnalysisManager::Instance();
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  0, pid);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  1, pre->GetKineticEnergy());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  2, pre->GetGlobalTime() / s);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  3, globalPos.x() / mm);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  4, globalPos.y() / mm);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  5, globalPos.z() / mm);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  6, direction.x());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  7, direction.y());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  8, direction.z());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId,  9, 1.0);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 10, sector);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 11, ClassifyFace(globalPos, sector, zLayer));
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 12, source.GetPlacementCask());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 13, source.GetSourceType());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 14, hexant);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 15, source.GetGlobalFuel());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 16, source.GetBatchId());
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 17, evtNb);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 18, trackID);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 19, zLayer);
    analysis->FillNtupleDColumn(kSectorFluxNtupleId, 20, cellId);
    analysis->AddNtupleRow(kSectorFluxNtupleId);

    return true;
}

G4int SectorScoringSD::ClassifyFace(const G4ThreeVector& globalPos,
                                    G4int sector, G4int zLayer) const
{
    const G4ThreeVector caskPos = fDetector->GetCASTOR440Position(0);
    G4RotationMatrix inv;
    if (auto* rot = fDetector->GetCASTOR440Rotation(0)) inv = rot->inverse();

    const G4ThreeVector local = inv * (globalPos - caskPos);
    const G4double r = std::hypot(local.x(), local.y());
    const G4double z = local.z();

    const G4int    nZLayers = fDetector->GetSectorZLayers();
    const G4double dz       = fDetector->GetSectorDz();
    const G4double zLow     = fDetector->GetSectorZMin() + zLayer * dz;
    const G4double zHigh    = zLow + dz;

    const G4double dInner = std::fabs(r - GeometrySectorScoring::kInnerRadiusMm * mm);
    const G4double dOuter = std::fabs(r - GeometrySectorScoring::kOuterRadiusMm * mm);
    const G4double dLow   = std::fabs(z - zLow);
    const G4double dHigh  = std::fabs(z - zHigh);
    const G4double dCap   = std::min(dLow, dHigh);

    const G4double twopi = 2. * CLHEP::pi;
    const G4double dPhi = twopi / GeometrySectorScoring::kNumSectors;
    G4double phi = std::atan2(local.y(), local.x());
    if (phi < 0.) phi += twopi;

    auto angularDistance = [twopi](G4double a, G4double b) {
        G4double d = std::fmod(std::fabs(a - b), twopi);
        return (d > CLHEP::pi) ? twopi - d : d;
    };

    const G4double phiLow  = sector * dPhi;
    const G4double phiHigh = (sector + 1) * dPhi;
    const G4double dSideLow  = r * std::sin(angularDistance(phi, phiLow));
    const G4double dSideHigh = r * std::sin(angularDistance(phi, phiHigh));

    G4int face = 0;
    G4double dMin = dInner;
    if (dOuter < dMin) { dMin = dOuter; face = 1; }
    if (dCap   < dMin) {
        dMin = dCap;
        if (zLayer == 0)               face = 3;  // external bottom
        else if (zLayer == nZLayers-1) face = 2;  // external top
        else face = (z >= 0.5 * (zLow + zHigh)) ? 6 : 7;  // internal caps
    }
    if (dSideLow  < dMin) { dMin = dSideLow;  face = 4; }
    if (dSideHigh < dMin) { dMin = dSideHigh; face = 5; }

    return face;
}
