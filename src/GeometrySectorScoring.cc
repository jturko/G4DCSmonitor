#include "GeometrySectorScoring.hh"
#include "DetectorConstruction.hh"
#include "SectorScoringSD.hh"

#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Exception.hh"
#include "G4SDManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4VPhysicalVolume.hh"

#include <string>

GeometrySectorScoring::GeometrySectorScoring(G4String worldName,
                                             DetectorConstruction* det)
    : G4VUserParallelWorld(worldName), fDetector(det)
{}

void GeometrySectorScoring::Construct()
{
    G4VPhysicalVolume* ghostWorld = GetWorld();
    G4LogicalVolume* ghostLogical = ghostWorld->GetLogicalVolume();

    if (fDetector->GetNumCASTOR440s() <= 0) {
        G4Exception("GeometrySectorScoring::Construct", "NoTargetCask",
                    FatalException,
                    "Sector scoring requires at least one CASTOR440 cask.");
        return;
    }

    const G4ThreeVector caskPos = fDetector->GetCASTOR440Position(0);
    G4RotationMatrix* caskRot = fDetector->GetCASTOR440Rotation(0);

    const G4int    nZLayers = fDetector->GetSectorZLayers();
    const G4double zMin     = fDetector->GetSectorZMin();
    const G4double zMax     = fDetector->GetSectorZMax();
    const G4double dz       = fDetector->GetSectorDz();

    if (nZLayers < 1 || nZLayers > kMaxZLayers || dz <= 0.) {
        G4ExceptionDescription ed;
        ed << "Invalid sector scoring bounds: nZLayers = " << nZLayers
           << ", zMin = " << zMin / mm << " mm, zMax = " << zMax / mm
           << " mm, dz = " << dz / mm << " mm.";
        G4Exception("GeometrySectorScoring::Construct", "InvalidSectorBounds",
                    FatalException, ed);
        return;
    }

    const G4double rIn  = kInnerRadiusMm * mm;
    const G4double rOut = kOuterRadiusMm * mm;
    const G4double dPhi = 360. / static_cast<G4double>(kNumSectors) * deg;

    const G4int nCells = nZLayers * kNumSectors;
    fSectorLogicals.clear();
    fSectorLogicals.reserve(nCells);

    for (G4int iz = 0; iz < nZLayers; ++iz) {
        const G4double zLow = zMin + iz * dz;
        const G4double zc   = zLow + 0.5 * dz;

        // Local z offset of this layer, then rotated and translated with cask 0.
        G4ThreeVector offset(0., 0., zc);
        if (caskRot) offset.transform(*caskRot);
        const G4ThreeVector position = caskPos + offset;

        for (G4int s = 0; s < kNumSectors; ++s) {
            const std::string suffix = "_z" + std::to_string(iz)
                                     + "_s" + std::to_string(s);
            auto* solid = new G4Tubs("SectorScoringSolid" + suffix,
                                     rIn, rOut, 0.5 * dz, s * dPhi, dPhi);
            auto* logical = new G4LogicalVolume(solid, nullptr,
                                                "SectorScoringLogical" + suffix);
            const G4int copyNo = iz * kNumSectors + s;
            new G4PVPlacement(caskRot, position, logical,
                              "SectorScoringPhysical" + suffix,
                              ghostLogical, false, copyNo);
            fSectorLogicals.push_back(logical);
        }
    }

    G4cout << "[SectorScoring] built " << nCells << " cells ("
           << nZLayers << " z layers x " << kNumSectors
           << " sectors) around target cask 0 at " << caskPos / mm
           << " mm; R=" << kInnerRadiusMm << "-" << kOuterRadiusMm
           << " mm, z=" << zMin / mm << " .. " << zMax / mm
           << " mm (dz=" << dz / mm << " mm)" << G4endl;
}

void GeometrySectorScoring::ConstructSD()
{
    auto* sd = new SectorScoringSD("SectorScoringSD", fDetector);
    G4SDManager::GetSDMpointer()->AddNewDetector(sd);

    for (auto* logical : fSectorLogicals) {
        if (logical) SetSensitiveDetector(logical, sd);
    }
}
