#include "GeometryParallelBiasing.hh"
#include "DetectorConstruction.hh"

#include "G4LogicalVolume.hh"
#include "G4VPhysicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4SystemOfUnits.hh"
#include "G4Tubs.hh"
#include "G4ThreeVector.hh"
#include "G4RotationMatrix.hh"
#include "G4IStore.hh" // Required to set the importance values
#include "G4Exception.hh"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
// Minimum distance between two 3D line segments [p1,q1] and [p2,q2]
// (Ericson, Real-Time Collision Detection). Used to conservatively detect
// cask shell sets whose outermost cylinders may intersect.
G4double SegmentSegmentDistance(const G4ThreeVector& p1, const G4ThreeVector& q1,
                                const G4ThreeVector& p2, const G4ThreeVector& q2)
{
    const G4ThreeVector d1 = q1 - p1;
    const G4ThreeVector d2 = q2 - p2;
    const G4ThreeVector r  = p1 - p2;
    const G4double a = d1.dot(d1);
    const G4double e = d2.dot(d2);
    const G4double f = d2.dot(r);
    const G4double eps = 1e-12;

    G4double s, t;
    if (a <= eps && e <= eps) return r.mag();
    if (a <= eps) {
        s = 0.;
        t = std::min(std::max(f / e, 0.), 1.);
    } else {
        const G4double c = d1.dot(r);
        if (e <= eps) {
            t = 0.;
            s = std::min(std::max(-c / a, 0.), 1.);
        } else {
            const G4double b     = d1.dot(d2);
            const G4double denom = a * e - b * b;
            s = (denom > eps) ? (b * f - c * e) / denom : 0.;
            s = std::min(std::max(s, 0.), 1.);
            t = (b * s + f) / e;
            if (t < 0.) {
                t = 0.;
                s = std::min(std::max(-c / a, 0.), 1.);
            } else if (t > 1.) {
                t = 1.;
                s = std::min(std::max((b - c) / a, 0.), 1.);
            }
        }
    }
    const G4ThreeVector c1 = p1 + d1 * s;
    const G4ThreeVector c2 = p2 + d2 * t;
    return (c1 - c2).mag();
}
}  // namespace

GeometryParallelBiasing::GeometryParallelBiasing(G4String worldName, 
                                                 DetectorConstruction* det)
  : G4VUserParallelWorld(worldName), fDetector(det) {}

void GeometryParallelBiasing::Construct() {
    G4VPhysicalVolume* ghostWorld   = GetWorld();
    G4LogicalVolume*   ghostLogical = ghostWorld->GetLogicalVolume();
    G4IStore*          iStore       = G4IStore::GetInstance(GetName());

    // Defensive only: the parallel world is registered only when the CLI
    // selects --biasing gamma|neutron, and a CLI/macro mismatch is already a
    // fatal error in DetectorConstruction::ConstructVolumes(). Kept as a
    // safety net if the registration policy ever changes.
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

    // -------------------------------------------------------------------
    // Overlap pre-check. Every cask's nested shells are placed directly in
    // the ghost world, so two casks whose outermost cylinders intersect would
    // produce overlapping parallel volumes (undefined importance lookup).
    // The outermost shell of cask c is a cylinder of radius rMax and total
    // height 2*(hMin/2 + nShells*t) centred on the cask, axis along the cask
    // local z. Inflating that cylinder by a ball of radius rMax yields a
    // capsule that contains it, and two equal-radius capsules intersect iff
    // the distance between their axis segments is below 2*rMax. This is a
    // conservative test (it may flag flat-end corner cases, never misses one).
    // -------------------------------------------------------------------
    const G4double halfLen = 0.5 * hMin + nShells * t;
    const G4ThreeVector zLocal(0., 0., 1.);
    std::vector<G4ThreeVector> axisA(numCasks), axisB(numCasks);
    for (G4int c = 0; c < numCasks; ++c) {
        const G4ThreeVector pos = fDetector->GetCASTOR440Position(c);
        G4RotationMatrix*   rot = fDetector->GetCASTOR440Rotation(c);
        G4ThreeVector axis = zLocal;
        if (rot) axis.transform(*rot);
        axisA[c] = pos - axis * halfLen;
        axisB[c] = pos + axis * halfLen;
    }
    for (G4int a = 0; a < numCasks; ++a) {
        for (G4int b = a + 1; b < numCasks; ++b) {
            const G4double d = SegmentSegmentDistance(axisA[a], axisB[a],
                                                      axisA[b], axisB[b]);
            if (d < 2. * rMax) {
                G4Exception("GeometryParallelBiasing::Construct", "BiasingCaskOverlap",
                            FatalException,
                            "Biasing shells of neighbouring CASTOR440 casks overlap. "
                            "The importance parallel world would contain intersecting "
                            "volumes with conflicting importances. Reduce "
                            "/dcs-monitor/det/setBiasingOuterRadius or space the casks "
                            "further apart.");
            }
        }
    }

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


