#include "GeometryParallelBiasing.hh"
#include "DetectorConstruction.hh"

#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4IStore.hh" // Required to set the importance values

#include <cmath>
#include <string>

GeometryParallelBiasing::GeometryParallelBiasing(G4String worldName, 
                                                 DetectorConstruction* det)
  : G4VUserParallelWorld(worldName), fDetector(det) {}

void GeometryParallelBiasing::Construct() {
    G4VPhysicalVolume* ghostWorld   = GetWorld();
    G4LogicalVolume*   ghostLogical = ghostWorld->GetLogicalVolume();
    G4IStore*          iStore       = G4IStore::GetInstance(GetName());

    if (!fDetector->GetUseBiasing()) {
        G4cout << " [Biasing] OFF - analog tracking only." << G4endl;
        iStore->AddImportanceGeometryCell(1, *ghostWorld);
        return;
    }

    const G4int    nShells = fDetector->GetNShells();
    const G4double rMin    = fDetector->GetBiasingInnerRadius();
    const G4double rMax    = fDetector->GetBiasingOuterRadius();
    const G4double hMin    = fDetector->GetBiasingInnerHeight();
    const G4double t       = (rMax - rMin) / static_cast<G4double>(nShells);

    G4cout << " [Biasing] nShells = " << nShells
           << ",  uniform shell thickness = " << t/mm << " mm (radial = axial)"
           << ",  max importance = 2^" << nShells
           << " (" << std::pow(2.0, nShells) << ")" << G4endl;

    // Outside the outermost shell -> highest importance (no Russian roulette near the boundary).
    iStore->AddImportanceGeometryCell(std::pow(2.0, nShells), *ghostWorld);

    const G4int numCasks = fDetector->GetNumCASTOR440s();
    for (G4int c = 0; c < numCasks; ++c) {
        const G4ThreeVector caskPos = fDetector->GetCASTOR440Position(c);
        G4RotationMatrix*   caskRot = fDetector->GetCASTOR440Rotation(c);

        // ---------------------------------------------------------------
        // Build NESTED tubs from OUTSIDE IN.
        //   T_i : r = rMin + i*t,    half-height = hMin/2 + i*t
        //   T_i is a daughter of T_{i+1}. The core T_0 lives inside T_1.
        // The physical "shell region" i (1..N) is the part of T_i that
        // is NOT occupied by its daughter T_{i-1}. Its importance is 2^i.
        // Radial AND axial thickness per shell are both exactly t.
        // ---------------------------------------------------------------

        G4LogicalVolume* parentLogical = ghostLogical;

        for (G4int i = nShells; i >= 1; --i) {
            const G4double rOuter = rMin + i * t;
            const G4double hHalf  = 0.5 * hMin + i * t;
            const G4double imp    = std::pow(2.0, i);

            const std::string sName = "BiasShellS_c" + std::to_string(c) + "_i" + std::to_string(i);
            const std::string lName = "BiasShellL_c" + std::to_string(c) + "_i" + std::to_string(i);
            const std::string pName = "BiasShellP_c" + std::to_string(c) + "_i" + std::to_string(i);

            auto* solid   = new G4Tubs(sName, 0., rOuter, hHalf, 0.*deg, 360.*deg);
            auto* logical = new G4LogicalVolume(solid, nullptr, lName); // no material: parallel world

            const bool        isOutermost = (i == nShells);
            G4RotationMatrix* placeRot    = isOutermost ? caskRot : nullptr;
            G4ThreeVector     placePos    = isOutermost ? caskPos : G4ThreeVector();

            auto* phys = new G4PVPlacement(placeRot, placePos, logical,
                                           pName, parentLogical, false,
                                           c * 1000 + i);

            iStore->AddImportanceGeometryCell(imp, *phys, c * 1000 + i);
            parentLogical = logical;
        }

        // Innermost core (importance 1), daughter of T_1.
        const std::string coreSName = "BiasCoreS_c" + std::to_string(c);
        const std::string coreLName = "BiasCoreL_c" + std::to_string(c);
        const std::string corePName = "BiasCoreP_c" + std::to_string(c);

        auto* coreSolid   = new G4Tubs(coreSName, 0., rMin, 0.5 * hMin, 0.*deg, 360.*deg);
        auto* coreLogical = new G4LogicalVolume(coreSolid, nullptr, coreLName);
        auto* corePhys    = new G4PVPlacement(nullptr, G4ThreeVector(),
                                              coreLogical, corePName,
                                              parentLogical, false,
                                              c * 1000 + 0);

        iStore->AddImportanceGeometryCell(1, *corePhys, c * 1000 + 0);
    }
}


