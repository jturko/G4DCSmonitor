#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"

#include "G4Tubs.hh"
#include "G4Polyhedra.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4AssemblyVolume.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include "GeometryCASTOR440.hh"
#include <string>
#include <cmath>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

GeometryCASTOR440::GeometryCASTOR440() :
    fCASTORAssembly(NULL),
    fCASTORBodyPhys(NULL),
    fCASTORBodyLog(NULL),
    fCavityLog(NULL),
    fLidLog(NULL),
    fFuelAssemblyLog(NULL),
    fModeratorRodLog(NULL),
    fFinLog(NULL)
{
    // Real-world dimensions for CASTOR 440/84
    fCaskHeight        = 4080. * mm;
    fCaskInnerRadius   =  900. * mm;
    fCaskOuterRadius   = 1270. * mm;
    fFinTipRadius      = 1330. * mm;
    fCavityHeight      = 3260. * mm;
    fLidThickness      =  290. * mm;
    fBottomThickness   =  390. * mm;
    fActiveFuelLength  = 2420. * mm;
    fTotalFuelLength   = 3217. * mm;

    // Hexagonal fuel-assembly parameters.
    // NOTE: fAssyApothem is the value passed to G4Polyhedra as rOuter; for a
    // 6-sided polyhedra rOuter is the *circumradius* (vertex distance from axis).
    fAssyApothem  =  72.  * mm;
    fAssyPhiStart =  30.  * deg;
    fAssyPitch    = 147.  * mm;

    fCastIronMatName = "CastIron";
    fHeliumMatName   = "G4_He";
    fPEMatName       = "G4_POLYETHYLENE";
    fFuelMatName     = "UO2";
    fSteelMatName    = "G4_STAINLESS-STEEL";

    GenerateFuelPositions();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

GeometryCASTOR440::~GeometryCASTOR440() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryCASTOR440::Build()
{
    G4cout << " -> Constructing GeometryCASTOR440..." << G4endl;

    G4bool surfCheck = false;

    BuildMaterials();
    fCASTORAssembly = new G4AssemblyVolume();

    G4NistManager* manager = G4NistManager::Instance();
    G4Material* mat_CastIron = manager->FindOrBuildMaterial(fCastIronMatName);
    G4Material* mat_He       = manager->FindOrBuildMaterial(fHeliumMatName);
    G4Material* mat_PE       = manager->FindOrBuildMaterial(fPEMatName);
    G4Material* mat_Fuel     = manager->FindOrBuildMaterial(fFuelMatName);
    G4Material* mat_Steel    = manager->FindOrBuildMaterial(fSteelMatName);

    G4ThreeVector move;
    G4RotationMatrix* rotate = nullptr;
    G4double alpha = 1.0;

    // --- IRON CASK BODY ---
    G4Tubs* body_solid = new G4Tubs("CastorBody", 0.*mm, fCaskOuterRadius,
                                    fCaskHeight/2.0, 0.*deg, 360.*deg);
    fCASTORBodyLog = new G4LogicalVolume(body_solid, mat_CastIron, "CastorBodyLog");
    fCASTORBodyLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.3, 0.3, 0.3, alpha)));

    // --- HELIUM CAVITY ---
    G4Tubs* cavity_solid = new G4Tubs("Cavity", 0.*mm, fCaskInnerRadius,
                                      fCavityHeight/2.0, 0.*deg, 360.*deg);
    fCavityLog = new G4LogicalVolume(cavity_solid, mat_He, "CavityLog");
    fCavityLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.9, 0.9, 0.9, alpha)));

    const G4double cavityZ = GetCavityZOffsetInBody();
    new G4PVPlacement(0, G4ThreeVector(0., 0., cavityZ), fCavityLog,
                      "CavityPhys", fCASTORBodyLog, false, 0, surfCheck);

    // --- STAINLESS STEEL LID ---
    G4Tubs* lid_solid = new G4Tubs("Lid", 0.*mm, fCaskInnerRadius,
                                   fLidThickness/2.0, 0.*deg, 360.*deg);
    fLidLog = new G4LogicalVolume(lid_solid, mat_Steel, "LidLog");
    fLidLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.6, 0.6, 0.7, alpha)));
    const G4double lidZ = cavityZ + fCavityHeight/2.0 + fLidThickness/2.0;
    new G4PVPlacement(0, G4ThreeVector(0., 0., lidZ), fLidLog,
                      "LidPhys", fCASTORBodyLog, false, 0, surfCheck);

    // --- FUEL ASSEMBLIES (VVER-440 hex) ---
    G4double zPlanes[2] = { -fActiveFuelLength/2.0, +fActiveFuelLength/2.0 };
    G4double rInner [2] = { 0., 0. };
    G4double rOuter [2] = { fAssyApothem, fAssyApothem };
    G4Polyhedra* fuel_solid = new G4Polyhedra(
        "FuelAssy",
        fAssyPhiStart, 360.*deg,
        6, 2, zPlanes, rInner, rOuter);

    fFuelAssemblyLog = new G4LogicalVolume(fuel_solid, mat_Fuel, "FuelAssyLog");
    fFuelAssemblyLog->SetVisAttributes(new G4VisAttributes(true,
                                       G4Colour(1.0, 0.0, 0.0, alpha)));

    G4int nPlaced = 0;
    for (size_t i = 0; i < fFuelPositions.size(); ++i) {
        //G4cout << " -> Placing fuel assembly " << i
        //       << " at (x,y) = (" << fFuelPositions[i].x()/mm
        //       << ", " << fFuelPositions[i].y()/mm << ") mm" << G4endl;
        new G4PVPlacement(nullptr, fFuelPositions[i],
                          fFuelAssemblyLog, "FuelPhys",
                          fCavityLog, false, nPlaced, surfCheck);
        ++nPlaced;
    }

    // --- MODERATOR RODS ---
    G4double rodRadius     =   30. * mm;
    G4double rodRingRadius = 1050. * mm;
    G4Tubs* rod_solid = new G4Tubs("ModRod", 0.*mm, rodRadius,
                                   fCavityHeight/2.0, 0.*deg, 360.*deg);
    fModeratorRodLog = new G4LogicalVolume(rod_solid, mat_PE, "ModRodLog");
    fModeratorRodLog->SetVisAttributes(new G4VisAttributes(true,
                                       G4Colour(0.0, 1.0, 1.0, alpha)));

    for (int i = 0; i < 60; ++i) {
        G4double angle = i * (360. * deg / 60.);
        G4double x = rodRingRadius * std::cos(angle);
        G4double y = rodRingRadius * std::sin(angle);
        new G4PVPlacement(0, G4ThreeVector(x, y, cavityZ), fModeratorRodLog,
                          "ModRodPhys", fCASTORBodyLog, false, i, surfCheck);
    }

    // --- HEAT-DISPERSION FINS ---
    const G4double finThickness = 10. * mm;
    const G4double finSpacing   = 50. * mm;
    G4Tubs* fin_solid = new G4Tubs("Fin",
                                   fCaskOuterRadius, fFinTipRadius,
                                   finThickness/2.0,
                                   0.*deg, 360.*deg);
    fFinLog = new G4LogicalVolume(fin_solid, mat_CastIron, "FinLog");
    fFinLog->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.2, 0.2, alpha)));

    const G4double finRegionTop    = cavityZ + fCavityHeight/2.0;
    const G4double finRegionBottom = cavityZ - fCavityHeight/2.0;
    const G4double finRegionLength = finRegionTop - finRegionBottom;

    const G4int    nFins  = static_cast<G4int>(std::floor(finRegionLength / finSpacing));
    const G4double startZ = finRegionBottom + 0.5 * finSpacing;

    for (G4int i = 0; i < nFins; ++i) {
        G4ThreeVector finPos(0., 0., startZ + i * finSpacing);
        fCASTORAssembly->AddPlacedVolume(fFinLog, finPos, rotate);
    }

    move = G4ThreeVector(0., 0., 0.);
    fCASTORAssembly->AddPlacedVolume(fCASTORBodyLog, move, rotate);

    G4cout << " -> Finished constructing GeometryCASTOR440!" << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryCASTOR440::PlaceDetector(G4LogicalVolume* logic_world,
                                      G4ThreeVector move,
                                      G4RotationMatrix* rotate, G4int copyNo)
{
    G4bool surfCheck = true;
    fCASTORAssembly->MakeImprint(logic_world, move, rotate, copyNo, surfCheck);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryCASTOR440::BuildMaterials()
{
    G4NistManager* nist = G4NistManager::Instance();
    if (nist->FindOrBuildMaterial("CastIron", false) != nullptr) return;

    G4Element* el_Fe = nist->FindOrBuildElement("Fe");
    G4Element* el_C  = nist->FindOrBuildElement("C");
    G4Material* mat_CastIron = new G4Material("CastIron", 7.3 * g/cm3, 2);
    mat_CastIron->AddElement(el_Fe, 96.0 * perCent);
    mat_CastIron->AddElement(el_C,   4.0 * perCent);

    G4Element* el_U = nist->FindOrBuildElement("U");
    G4Element* el_O = nist->FindOrBuildElement("O");
    G4Material* mat_UO2 = new G4Material("UO2", 10.5 * g/cm3, 2);
    mat_UO2->AddElement(el_U, 1);
    mat_UO2->AddElement(el_O, 2);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryCASTOR440::GenerateFuelPositions()
{
    const G4double pitch  = fAssyPitch;
    const G4double zOffset = 0. * mm;

    const G4double dx = pitch;
    const G4double dy = pitch * std::sqrt(3.0) / 2.0;
    const G4int    R  = 5;

    fFuelPositions.clear();
    fFuelPositions.reserve(84);

    // Build a full 5-ring hex lattice (91 candidate positions).
    for (G4int q = -R; q <= R; ++q) {
        const G4int r1 = std::max(-R, -q - R);
        const G4int r2 = std::min( R, -q + R);
        for (G4int r = r1; r <= r2; ++r) {
            const G4double x = dx * (q + r / 2.0);
            const G4double y = dy * r;

            // (a) Centre position empty (CASTOR 440/84 basket has no central rod).
            if (std::hypot(x, y) < 1e-6) continue;

            // (b) Six corner positions of the outermost ring.
            const G4double rmax = R * pitch;
            if (std::abs(std::hypot(x, y) - rmax) < 1e-3) {
                G4double ang = std::atan2(y, x) * 180.0 / CLHEP::pi;
                if (ang < 0) ang += 360.0;
                bool isCorner = false;
                for (G4double a = 0.0; a < 360.0; a += 60.0) {
                    if (std::abs(ang - a) < 1.0) { isCorner = true; break; }
                }
                if (isCorner) continue;
            }

            fFuelPositions.emplace_back(x, y, zOffset);
        }
    }
    G4cout << " -> Generated " << (int)fFuelPositions.size()
           << " fuel positions" << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreeVector GeometryCASTOR440::GetFuelPosition(G4int index) const
{
    if (index >= 0 && index < (G4int)fFuelPositions.size()) {
        return fFuelPositions[index];
    }
    return G4ThreeVector(0., 0., 0.);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4ThreeVector GeometryCASTOR440::SampleUniformPointInFuel(G4int fuelIdx) const
{
    if (fuelIdx < 0 || fuelIdx >= (G4int)fFuelPositions.size()) {
        G4cerr << " [GeometryCASTOR440::SampleUniformPointInFuel] Invalid fuelIdx = "
               << fuelIdx << G4endl;
        return G4ThreeVector();
    }

    // ----------------------------------------------------------------------
    // Uniform sampling inside a regular hexagon of circumradius R (vertex
    // distance from axis = R). The hexagon is decomposed into 6 equilateral
    // triangles. We sample uniformly inside the foundational triangle with
    // vertices (0,0), (R,0), (R/2, R*sqrt(3)/2) using barycentric coordinates,
    // then rotate the point into one of the 6 sectors and finally apply the
    // global hexagon orientation (fAssyPhiStart) so the sampled distribution
    // lines up exactly with the placed G4Polyhedra.
    // ----------------------------------------------------------------------
    const G4double R = fAssyApothem;     // = G4Polyhedra rOuter (circumradius)

    G4double r1 = G4UniformRand();
    G4double r2 = G4UniformRand();
    if (r1 + r2 > 1.0) { r1 = 1.0 - r1; r2 = 1.0 - r2; }

    const G4double tx = R * r1 + 0.5 * R * r2;
    const G4double ty = (std::sqrt(3.0) / 2.0) * R * r2;

    const G4int    sector = (G4int)(G4UniformRand() * 6);
    const G4double angle  = fAssyPhiStart + sector * 60. * deg;
    const G4double cs     = std::cos(angle);
    const G4double sn     = std::sin(angle);

    const G4double localX = tx * cs - ty * sn;
    const G4double localY = tx * sn + ty * cs;

    // Uniform Z within the active fuel length (matches G4Polyhedra zPlanes).
    const G4double localZ = (G4UniformRand() - 0.5) * fActiveFuelLength;

    // (localX, localY, localZ) is in the *fuel polyhedra* frame; offset by the
    // fuel's position within the cavity, then by the cavity's offset within
    // the cask body to obtain a point in the cask body local frame.
    const G4ThreeVector posInCavity =
        fFuelPositions[fuelIdx] + G4ThreeVector(localX, localY, localZ);
    const G4ThreeVector posInCaskBody =
        posInCavity + G4ThreeVector(0., 0., GetCavityZOffsetInBody());

    return posInCaskBody;
}

