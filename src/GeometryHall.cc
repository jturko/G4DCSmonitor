#include "GeometryHall.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4AssemblyVolume.hh"
#include "G4RotationMatrix.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"

GeometryHall::GeometryHall()
{
    fHallLength       = 22000. * mm;   // spans the symmetric 3-cluster train (world = 22 m)
    fFloorTopZ        = -2040. * mm;   // = -caskHeight/2 (CASTOR 440 body = 4080 mm)
    fFloorThickness   =   500. * mm;
    fWallInnerY       =  5410. * mm;   // 2910 (cluster y half-extent) + 2500 (clearance)
    fWallThickness    =   500. * mm;
    fWallHeight       =  8000. * mm;
    fCeilingThickness =   400. * mm;

    fUseFloor   = true;
    fUseWalls   = true;
    fUseCeiling = false;               // room-return floor + 2 walls by default

    fFloorMatName   = "G4_CONCRETE";
    fWallMatName    = "G4_CONCRETE";
    fCeilingMatName = "G4_CONCRETE";

    fFloorColour    = G4Colour(0.50, 0.50, 0.50, 0.40);
    fWallColour     = G4Colour(0.60, 0.60, 0.55, 0.30);
    fCeilingColour  = G4Colour(0.60, 0.60, 0.65, 0.30);
}

GeometryHall::~GeometryHall() {}

void GeometryHall::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();
    nist->FindOrBuildMaterial("G4_CONCRETE");
    nist->FindOrBuildMaterial("G4_AIR");
}

G4int GeometryHall::Build()
{
    BuildMaterials();
    G4NistManager* man = G4NistManager::Instance();

    fAssembly = new G4AssemblyVolume();
    G4ThreeVector     move;             // must be a NAMED lvalue for AddPlacedVolume()
    G4RotationMatrix* noRot = nullptr;

    const G4double halfX     = fHallLength / 2.0;
    const G4double roomHalfY = fWallInnerY + fWallThickness;   // outer edge of the walls

    // ---- 1) floor slab (top at fFloorTopZ, extends under the walls) ----
    if (fUseFloor) {
        auto* s = new G4Box("HallFloor", halfX, roomHalfY, fFloorThickness / 2.0);
        auto* m = man->FindOrBuildMaterial(fFloorMatName);
        fFloorLog = new G4LogicalVolume(s, m, "HallFloorLog");
        fFloorLog->SetVisAttributes(new G4VisAttributes(true, fFloorColour));
        move = G4ThreeVector(0., 0., fFloorTopZ - fFloorThickness / 2.0);
        fAssembly->AddPlacedVolume(fFloorLog, move, noRot);
    }

    // ---- 2) two side walls, parallel to x, resting on the floor top ----
    if (fUseWalls) {
        auto* s = new G4Box("HallWall", halfX, fWallThickness / 2.0, fWallHeight / 2.0);
        auto* m = man->FindOrBuildMaterial(fWallMatName);
        fWallLog = new G4LogicalVolume(s, m, "HallWallLog");
        fWallLog->SetVisAttributes(new G4VisAttributes(true, fWallColour));

        const G4double wallCenY = fWallInnerY + fWallThickness / 2.0;
        const G4double wallCenZ = fFloorTopZ + fWallHeight / 2.0;

        move = G4ThreeVector(0., +wallCenY, wallCenZ);   // +y wall
        fAssembly->AddPlacedVolume(fWallLog, move, noRot);
        move = G4ThreeVector(0., -wallCenY, wallCenZ);   // -y wall
        fAssembly->AddPlacedVolume(fWallLog, move, noRot);
    }

    // ---- 3) optional ceiling (off by default) ----
    if (fUseCeiling) {
        auto* s = new G4Box("HallCeiling", halfX, roomHalfY, fCeilingThickness / 2.0);
        auto* m = man->FindOrBuildMaterial(fCeilingMatName);
        fCeilingLog = new G4LogicalVolume(s, m, "HallCeilingLog");
        fCeilingLog->SetVisAttributes(new G4VisAttributes(true, fCeilingColour));
        move = G4ThreeVector(0., 0., fFloorTopZ + fWallHeight + fCeilingThickness / 2.0);
        fAssembly->AddPlacedVolume(fCeilingLog, move, noRot);
    }

    G4cout << " -> GeometryHall: length " << fHallLength / m << " m, floor top z="
           << fFloorTopZ / mm << " mm (thk " << fFloorThickness / mm << "), walls at y=+/-"
           << fWallInnerY / mm << " mm (thk " << fWallThickness / mm << ", height "
           << fWallHeight / mm << "), ceiling " << (fUseCeiling ? "ON" : "OFF")
           << ", material=" << fWallMatName << "." << G4endl;
    return 1;
}

void GeometryHall::PlaceDetector(G4LogicalVolume* worldLog, G4ThreeVector move,
                                 G4RotationMatrix* rotate, G4int copyNo)
{
    fAssembly->MakeImprint(worldLog, move, rotate, copyNo, /*surfCheck*/ false);
}

