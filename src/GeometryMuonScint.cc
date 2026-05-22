#include "GeometryMuonScint.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4SystemOfUnits.hh"
#include "G4PhysicalConstants.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

#include "G4Region.hh"
#include "G4RegionStore.hh"

#include <cmath>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

GeometryMuonScint::GeometryMuonScint() {}
GeometryMuonScint::~GeometryMuonScint() {}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Optical properties of the organic plastic scintillator (UPS-92S / EJ-200).
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::BuildOpticalProperties(G4Material* scintMat)
{
    const G4int N = 11;
    G4double photonE[N] = {
        2.067*eV, 2.156*eV, 2.250*eV, 2.339*eV, 2.450*eV,
        2.594*eV, 2.755*eV, 2.917*eV, 3.100*eV, 3.300*eV, 3.543*eV
    };
    G4double rindex[N]  = { 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58 };
    G4double absL[N]    = {
        2.0*m, 2.0*m, 2.0*m, 2.0*m, 2.0*m,
        2.0*m, 2.0*m, 1.5*m, 1.0*m, 0.5*m, 0.1*m
    };
    G4double scint[N]   = {
        0.00, 0.01, 0.05, 0.15, 0.35,
        0.60, 0.85, 1.00, 0.70, 0.20, 0.02
    };

    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX",                  photonE, rindex, N);
    mpt->AddProperty("ABSLENGTH",               photonE, absL,   N);
    mpt->AddProperty("SCINTILLATIONCOMPONENT1", photonE, scint,  N);

    //mpt->AddConstProperty("SCINTILLATIONYIELD",         100./MeV); // for vis debugging
    mpt->AddConstProperty("SCINTILLATIONYIELD",         10000./MeV);
    mpt->AddConstProperty("RESOLUTIONSCALE",            1.0);
    mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 2.4*ns);
    mpt->AddConstProperty("SCINTILLATIONYIELD1",        1.0);

    scintMat->SetMaterialPropertiesTable(mpt);
    scintMat->GetIonisation()->SetBirksConstant(0.126*mm/MeV);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Reflector optical surfaces.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::BuildOpticalSurfaces()
{
    fReflSurface = new G4OpticalSurface("MuonScintReflector");
    fReflSurface->SetModel(unified);

    auto* surfMPT = new G4MaterialPropertiesTable();
    const G4int N = 2;
    G4double e[N] = { 1.5*eV, 6.0*eV };

    switch (fReflectorType) {
      case kNoReflector: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polished);
        // Pure Fresnel from RINDEX -- no reflectivity table needed.
        break;
      }
      case kPaintTiO2: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetSigmaAlpha(0.20);
        G4double R[N] = { 0.97, 0.97 };
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        break;
      }
      case kAluminumFoil: {
        fReflSurface->SetType(dielectric_metal);
        fReflSurface->SetFinish(polished);
        G4double R[N] = { 0.90, 0.90 };
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        break;
      }
      case kGlossyPaper: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetSigmaAlpha(0.10);
        G4double R[N]    = { 0.95, 0.95 };
        G4double lobe[N] = { 0.70, 0.70 };   // mostly specular
        surfMPT->AddProperty("REFLECTIVITY",         e, R,    N);
        surfMPT->AddProperty("SPECULARLOBECONSTANT", e, lobe, N);
        break;
      }
      case kTeflon: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetSigmaAlpha(0.25);
        G4double R[N] = { 0.99, 0.99 };
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        break;
      }
    }

    fReflSurface->SetMaterialPropertiesTable(surfMPT);



    //// Build the SiPM coupling surface: a perfect absorber/detector that
    //// OVERRIDES the painted skin at the slab<->SiPM boundary.
    //fSiPMSurface = new G4OpticalSurface("MuonScintSiPMCoupling");
    //fSiPMSurface->SetModel(unified);
    //fSiPMSurface->SetType(dielectric_metal);    // metallic -> no transmission ambiguity
    //fSiPMSurface->SetFinish(polished);          // specular only
    //
    //auto* sipmMPT = new G4MaterialPropertiesTable();
    //const G4int N2 = 2;
    //G4double e2[N2]   = { 1.5*eV, 6.0*eV };
    //G4double R2[N2]   = { 0.0, 0.0 };             // never reflect off the SiPM face
    //G4double eff2[N] = { 1.0, 1.0 };             // 100% absorption at the surface
    //sipmMPT->AddProperty("REFLECTIVITY", e2, R2,   N2);
    //sipmMPT->AddProperty("EFFICIENCY",   e2, eff2, N2);
    //fSiPMSurface->SetMaterialPropertiesTable(sipmMPT);

}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryMuonScint::Build()
{
    G4cout << " -> Constructing GeometryMuonScint (plastic scintillator slab)..." << G4endl;

    auto* nist = G4NistManager::Instance();

    // ---- Scintillator material ----
    // Clone the NIST plastic scintillator with a friendly name so optical
    // properties only attach to OUR material instance.
    G4Material* baseScint = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
    G4Material* scintMat  = G4Material::GetMaterial(fScintMatName, false);
    if (!scintMat) {
        scintMat = new G4Material(fScintMatName,
                                  baseScint->GetDensity(),
                                  baseScint, baseScint->GetState(),
                                  baseScint->GetTemperature(),
                                  baseScint->GetPressure());
    }
    BuildOpticalProperties(scintMat);

    G4Material* air = nist->FindOrBuildMaterial("G4_AIR");
    // (RINDEX on air is now attached in DetectorConstruction::ConstructVolumes,
    //  before the world LV is created. We don't touch air here.)

    // ---- Slab solid + LV ----
    auto* slabSolid = new G4Box("MuonScintSolid",
                                fHalfSize.x(), fHalfSize.y(), fHalfSize.z());
    fScintLV = new G4LogicalVolume(slabSolid, scintMat, "MuonScintLV");
    fScintLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.8, 1.0, 0.4)));

    // ---- Region for fine production cuts ----
    G4Region* scintRegion = G4RegionStore::GetInstance()->GetRegion("MuonScintRegion", false);
    if (!scintRegion) {
        scintRegion = new G4Region("MuonScintRegion");
    }
    scintRegion->AddRootLogicalVolume(fScintLV);

    // ---- SiPM LV (one shared LV; placed multiple times as daughters) ----
    auto* sipmSolid = new G4Box("SiPMSolid", fSiPMHalfX, fSiPMHalfY, fSiPMHalfZ);
    fSiPMLV = new G4LogicalVolume(sipmSolid, air, "SiPMLV");
    fSiPMLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.0, 0.0, 1.0)));

    // ---- Optical surface (skin surface attached at PlaceDetector time) ----
    BuildOpticalSurfaces();

    G4cout << " -> Finished constructing GeometryMuonScint." << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::PlaceDetector(G4LogicalVolume* worldLV,
                                      const G4ThreeVector& pos,
                                      G4RotationMatrix*    rot,
                                      G4int                copyNo)
{
    G4bool surfCheck = true;

    // Place the slab directly in the world (no assembly volume).
    fScintPV = new G4PVPlacement(rot,
                                 pos,
                                 fScintLV,
                                 "MuonScintPhys",
                                 worldLV,
                                 false,
                                 copyNo,
                                 surfCheck);

    // Attach the reflector skin surface to the slab LV.
    if (fWrapWithReflector && fReflectorType != kNoReflector) {
        auto* skin = new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
        G4cout << " [MuonScint] Created skin surface: '" << skin->GetName()
               << "' on LV: '" << fScintLV->GetName()
               << "' (type=" << fReflSurface->GetType()
               << " finish=" << fReflSurface->GetFinish()
               << " model="  << fReflSurface->GetModel() << ")" << G4endl;
        G4cout << " [MuonScint] Total registered skin surfaces: "
               << G4LogicalSkinSurface::GetNumberOfSkinSurfaces() << G4endl;
    }

    // Place SiPMs as daughters of the slab LV. Optical photons that strike
    // the SiPM volume cross the slab/SiPM boundary (not the slab/world
    // boundary, so the skin surface does NOT apply there) and the SD kills
    // and counts them.
    PlaceSiPMs(fScintLV);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::PlaceSiPMs(G4LogicalVolume* parentLV)
{
    const G4double Lx = fHalfSize.x();
    const G4double Ly = fHalfSize.y();
    const G4double Lz = fHalfSize.z();

    for (size_t i = 0; i < fSiPMs.size(); ++i) {
        const auto& s = fSiPMs[i];

        G4ThreeVector p;
        G4RotationMatrix* r = new G4RotationMatrix();
        
        switch (s.edge) {
          case 0: // +X face
            p = G4ThreeVector(+Lx - fSiPMHalfZ,
                              s.uAlong * (Ly),
                              s.vAlong * (Lz));
            r->rotateY(90.*deg);
            break;
          case 1: // -X face
            p = G4ThreeVector(-Lx + fSiPMHalfZ,
                              s.uAlong * (Ly),
                              s.vAlong * (Lz));
            r->rotateY(90.*deg);
            break;
          case 2: // +Y face
            p = G4ThreeVector(s.uAlong * (Lx),
                              +Ly - fSiPMHalfZ,
                              s.vAlong * (Lz));
            r->rotateX(90.*deg);
            break;
          case 3: // -Y face
            p = G4ThreeVector(s.uAlong * (Lx),
                              -Ly + fSiPMHalfZ,
                              s.vAlong * (Lz));
            r->rotateX(90.*deg);
            break;
        }

        auto* sipmPV = new G4PVPlacement(r, p, fSiPMLV, "SiPMPhys",
                          parentLV, false, (G4int)i, true);
        
        //if (fScintPV && fSiPMSurface) {
        //    new G4LogicalBorderSurface(
        //        "MuonScint_SiPM_Border_" + std::to_string(i),
        //        fScintPV,        // pre   (scintillator)
        //        sipmPV,          // post  (SiPM)
        //        fSiPMSurface);
        //}

    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::PresetConfig_8SiPM_AllSides() {
    ClearSiPMs();
    //for (G4int e = 0; e < 4; ++e) {
    //    AddSiPM({e, -0.5, 0.0});
    //    AddSiPM({e, +0.5, 0.0});
    //}
    for (G4int e = 0; e < 4; ++e) {
        AddSiPM({e, -0.8, 0.0});
        AddSiPM({e, +0.8, 0.0});
    }
}

void GeometryMuonScint::PresetConfig_4SiPM_OnePerSide() {
    ClearSiPMs();
    //for (G4int e = 0; e < 4; ++e) AddSiPM({e, 0.0, 0.0});
    for (G4int e = 0; e < 4; ++e) AddSiPM({e, 0.8, 0.0});
}

void GeometryMuonScint::PresetConfig_2SiPM_OneSide() {
    ClearSiPMs();
    //AddSiPM({0, -0.5, 0.0});
    //AddSiPM({0, +0.5, 0.0});
    AddSiPM({0, -0.8, 0.0});
    AddSiPM({0, +0.8, 0.0});
}

void GeometryMuonScint::PresetConfig_4SiPM_Corners() {
    ClearSiPMs();
    AddSiPM({0, -1.0, 0.0});
    AddSiPM({0, +1.0, 0.0});
    AddSiPM({1, -1.0, 0.0});
    AddSiPM({1, +1.0, 0.0});
}







//     #include "GeometryMuonScint.hh"
//     
//     #include "G4Box.hh"
//     #include "G4LogicalVolume.hh"
//     #include "G4PVPlacement.hh"
//     #include "G4AssemblyVolume.hh"
//     #include "G4Material.hh"
//     #include "G4NistManager.hh"
//     #include "G4MaterialPropertiesTable.hh"
//     #include "G4OpticalSurface.hh"
//     #include "G4LogicalSkinSurface.hh"
//     #include "G4LogicalBorderSurface.hh"
//     #include "G4SystemOfUnits.hh"
//     #include "G4PhysicalConstants.hh"
//     #include "G4VisAttributes.hh"
//     #include "G4Colour.hh"
//     
//     #include "G4Region.hh"
//     #include "G4RegionStore.hh"
//     
//     #include <cmath>
//     
//     GeometryMuonScint::GeometryMuonScint()
//       : fHalfSize( G4ThreeVector(100.*mm, 100.*mm, 5.*mm) )   // 200 x 200 x 10 mm^3 default
//     {}
//     
//     GeometryMuonScint::~GeometryMuonScint() {
//         delete fAssembly;
//     }
//     
//     // -----------------------------------------------------------------------------
//     // Optical properties of the organic plastic scintillator.
//     // Values chosen to reproduce the UPS-92S / EJ-200 family used in Rao 2025
//     // (light yield ~10 000 ph/MeV, peak 425 nm, decay 2.4 ns, n=1.58, atten 2 m).
//     // -----------------------------------------------------------------------------
//     void GeometryMuonScint::BuildOpticalProperties(G4Material* scintMat)
//     {
//         // Wavelengths spanning the emission/absorption bands.
//         // Energies must be ASCENDING for G4MaterialPropertiesTable.
//         // 600 nm -> 350 nm
//         const G4int N = 11;
//         G4double photonE[N] = {
//             2.067*eV, 2.156*eV, 2.250*eV, 2.339*eV, 2.450*eV,
//             2.594*eV, 2.755*eV, 2.917*eV, 3.100*eV, 3.300*eV, 3.543*eV
//         };
//         // Refractive index ~ 1.58 (UPS-92S; Rao 2025 [[11]])
//         G4double rindex[N]  = { 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58 };
//         // Attenuation length 2 m (Rao 2025 [[11]]); use a smooth roll-off below 400 nm
//         G4double absL[N]    = {
//             2.0*m, 2.0*m, 2.0*m, 2.0*m, 2.0*m,
//             2.0*m, 2.0*m, 1.5*m, 1.0*m, 0.5*m, 0.1*m
//         };
//         // Emission spectrum (peaked at ~425 nm = 2.92 eV)
//         G4double scint[N]   = {
//             0.00, 0.01, 0.05, 0.15, 0.35,
//             0.60, 0.85, 1.00, 0.70, 0.20, 0.02
//         };
//     
//         auto* mpt = new G4MaterialPropertiesTable();
//         mpt->AddProperty("RINDEX",        photonE, rindex, N);
//         mpt->AddProperty("ABSLENGTH",     photonE, absL,   N);
//         mpt->AddProperty("SCINTILLATIONCOMPONENT1", photonE, scint, N);
//     
//         mpt->AddConstProperty("SCINTILLATIONYIELD",     10000./MeV);   // [[11]]
//         mpt->AddConstProperty("RESOLUTIONSCALE",        1.0);
//         mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 2.4*ns);   // [[11]]
//         mpt->AddConstProperty("SCINTILLATIONYIELD1",    1.0);
//     
//         scintMat->SetMaterialPropertiesTable(mpt);
//         scintMat->GetIonisation()->SetBirksConstant(0.126*mm/MeV);     // standard plastic Birks
//     }
//     
//     void GeometryMuonScint::BuildOpticalSurfaces()
//     {
//         fReflSurface = new G4OpticalSurface("MuonScintReflector");
//     
//         // Set the model + finish according to user choice.
//         switch (fReflectorType) {
//           case kPaintTiO2:
//             fReflSurface->SetModel(unified);
//             fReflSurface->SetType(dielectric_dielectric);
//             fReflSurface->SetFinish(groundfrontpainted);  // Lambertian paint
//             break;
//           case kAluminumFoil:
//             fReflSurface->SetModel(unified);
//             fReflSurface->SetType(dielectric_metal);
//             fReflSurface->SetFinish(polishedfrontpainted); // mostly specular
//             break;
//           case kGlossyPaper:
//             fReflSurface->SetModel(unified);
//             fReflSurface->SetType(dielectric_dielectric);
//             fReflSurface->SetFinish(groundfrontpainted);
//             break;
//           case kTeflon:
//             fReflSurface->SetModel(unified);
//             fReflSurface->SetType(dielectric_dielectric);
//             fReflSurface->SetFinish(groundfrontpainted);
//             break;
//           case kNoReflector:
//           default:
//             fReflSurface->SetModel(unified);
//             fReflSurface->SetType(dielectric_dielectric);
//             fReflSurface->SetFinish(polished);
//             break;
//         }
//     
//         // Reflectivity vs energy (broadband, paper-derived [[11]])
//         const G4int N = 2;
//         G4double e[N] = { 1.5*eV, 6.0*eV };
//         G4double R[N] = { 0.95, 0.95 };
//         G4double sigA[N] = { 0.1, 0.1 };  // micro-facet sigma_alpha (rad)
//     
//         switch (fReflectorType) {
//           case kPaintTiO2:    R[0] = R[1] = 0.97; sigA[0] = sigA[1] = 0.20; break;
//           case kAluminumFoil: R[0] = R[1] = 0.90; sigA[0] = sigA[1] = 0.05; break;
//           case kGlossyPaper:  R[0] = R[1] = 0.95; sigA[0] = sigA[1] = 0.10; break;
//           case kTeflon:       R[0] = R[1] = 0.99; sigA[0] = sigA[1] = 0.25; break;
//           case kNoReflector:  R[0] = R[1] = 0.04; sigA[0] = sigA[1] = 0.0;  break;
//         }
//     
//         auto* surfMPT = new G4MaterialPropertiesTable();
//         surfMPT->AddProperty("REFLECTIVITY", e, R, N);
//         if (fReflectorType != kNoReflector && fReflectorType != kAluminumFoil) {
//             fReflSurface->SetSigmaAlpha(sigA[0]);
//         }
//         fReflSurface->SetMaterialPropertiesTable(surfMPT);
//     }
//     
//     G4int GeometryMuonScint::Build()
//     {
//         G4cout << " -> Constructing GeometryMuonScint (plastic scintillator slab)..." << G4endl;
//         fAssembly = new G4AssemblyVolume();
//     
//         // ---- Materials ----
//         auto* nist = G4NistManager::Instance();
//     
//         // Use NIST plastic scintillator as the base (PVT-like polymer); rename for clarity.
//         G4Material* baseScint = nist->FindOrBuildMaterial("G4_PLASTIC_SC_VINYLTOLUENE");
//         G4Material* scintMat  = nullptr;
//         if (G4Material::GetMaterial(fScintMatName, false)) {
//             scintMat = G4Material::GetMaterial(fScintMatName, false);
//         } else {
//             // Clone with a friendly name so optical-properties only attach to OUR material
//             scintMat = new G4Material(fScintMatName,
//                                       baseScint->GetDensity(),
//                                       baseScint, baseScint->GetState(),
//                                       baseScint->GetTemperature(),
//                                       baseScint->GetPressure());
//         }
//         BuildOpticalProperties(scintMat);
//     
//         // Air around the slab (the world is already air; just ensure RINDEX is set so
//         // photons can refract into/out of it correctly).
//         G4Material* air = nist->FindOrBuildMaterial("G4_AIR");
//         if (!air->GetMaterialPropertiesTable()) {
//             const G4int Na = 2;
//             G4double ea[Na] = { 1.5*eV, 6.0*eV };
//             G4double na[Na] = { 1.0003, 1.0003 };
//             auto* mptAir = new G4MaterialPropertiesTable();
//             mptAir->AddProperty("RINDEX", ea, na, Na);
//             air->SetMaterialPropertiesTable(mptAir);
//         }
//     
//         // ---- Slab solid + LV ----
//         auto* slabSolid = new G4Box("MuonScintSolid",
//                                     fHalfSize.x(), fHalfSize.y(), fHalfSize.z());
//         fScintLV = new G4LogicalVolume(slabSolid, scintMat, "MuonScintLV");
//         fScintLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.8, 1.0, 0.4)));
//         
//         G4Region* scintRegion = G4RegionStore::GetInstance()->GetRegion("MuonScintRegion", false);
//         if (!scintRegion) {
//             scintRegion = new G4Region("MuonScintRegion");
//         }
//         scintRegion->AddRootLogicalVolume(fScintLV);
//     
//         G4ThreeVector move(0., 0., 0.);
//         G4RotationMatrix* rotate = new G4RotationMatrix();
//         fAssembly->AddPlacedVolume(fScintLV, move, rotate);
//     
//         // ---- SiPM "active surface" pseudo-detectors ----
//         // Modelled as a thin air box sitting flush against the slab edge, acting as
//         // an absorber-style photon counter (sensitive detector kills the photon and
//         // records it). This avoids modelling silicon optical properties unless you
//         // want PDE detail; we apply a 43% PDE inside the SD instead [[11]].
//         auto* sipmSolid = new G4Box("SiPMSolid", fSiPMHalfX, fSiPMHalfY, fSiPMHalfZ);
//         fSiPMLV = new G4LogicalVolume(sipmSolid, air, "SiPMLV");
//         fSiPMLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.0, 0.0, 1.0)));
//     
//         BuildOpticalSurfaces();
//     
//         G4cout << " -> Finished constructing GeometryMuonScint." << G4endl;
//         return 1;
//     }
//     
//     void GeometryMuonScint::PlaceDetector(G4LogicalVolume* worldLV,
//                                           const G4ThreeVector& pos,
//                                           G4RotationMatrix*    rot,
//                                           G4int                copyNo)
//     {
//         G4bool surfCheck = true;
//         fAssembly->MakeImprint(worldLV, const_cast<G4ThreeVector&>(pos), rot, copyNo, surfCheck);
//     
//         // After imprint, find the slab PV that was placed and (a) attach a skin
//         // surface for the reflector, (b) place SiPMs as world-coordinate daughters
//         // of the slab LV. The cleanest approach is to attach a *skin surface* to
//         // the LV (applies everywhere the LV's surface meets another material),
//         // then the SiPM placements override that locally because skin surfaces
//         // are bypassed where a logical border surface or daughter volume contact
//         // exists.
//         //if (fWrapWithReflector && fReflectorType != kNoReflector) {
//         //    new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
//         //}
//         if (fWrapWithReflector && fReflectorType != kNoReflector) {
//             auto* skinSurf = new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
//             G4cout << " [MuonScint] Created skin surface: " << skinSurf->GetName()
//                    << " on LV: " << fScintLV->GetName()
//                    << " with optical surface type=" << fReflSurface->GetType()
//                    << " finish=" << fReflSurface->GetFinish()
//                    << " model=" << fReflSurface->GetModel() << G4endl;
//             G4cout << " [MuonScint] Total registered skin surfaces: "
//                    << G4LogicalSkinSurface::GetNumberOfSkinSurfaces() << G4endl;
//         }
//     
//     
//         
//         // Place the SiPMs as DAUGHTERS of the slab logical volume so their faces
//         // touch the inside of the slab edges. Optical photons that strike the
//         // SiPM volume cross the boundary into air (no skin surface there since
//         // the daughter overrides it) and the sensitive detector kills + counts them.
//         PlaceSiPMs(fScintLV);
//     }
//     
//     void GeometryMuonScint::PlaceSiPMs(G4LogicalVolume* parentLV)
//     {
//         const G4double Lx = fHalfSize.x();
//         const G4double Ly = fHalfSize.y();
//         const G4double Lz = fHalfSize.z();
//     
//         for (size_t i = 0; i < fSiPMs.size(); ++i) {
//             const auto& s = fSiPMs[i];
//     
//             // Place the SiPM volume just *inside* the slab face so its outer face
//             // is flush with the slab's outer edge.
//             G4ThreeVector p;
//             G4RotationMatrix* r = new G4RotationMatrix();
//             switch (s.edge) {
//               case 0: // +X face: SiPM thickness along X
//                 p = G4ThreeVector(+Lx - fSiPMHalfZ, s.uAlong * (Ly - fSiPMHalfX),
//                                                      s.vAlong * (Lz - fSiPMHalfY));
//                 r->rotateY(90.*deg);
//                 break;
//               case 1: // -X face
//                 p = G4ThreeVector(-Lx + fSiPMHalfZ, s.uAlong * (Ly - fSiPMHalfX),
//                                                      s.vAlong * (Lz - fSiPMHalfY));
//                 r->rotateY(90.*deg);
//                 break;
//               case 2: // +Y face
//                 p = G4ThreeVector(s.uAlong * (Lx - fSiPMHalfX), +Ly - fSiPMHalfZ,
//                                   s.vAlong * (Lz - fSiPMHalfY));
//                 r->rotateX(90.*deg);
//                 break;
//               case 3: // -Y face
//                 p = G4ThreeVector(s.uAlong * (Lx - fSiPMHalfX), -Ly + fSiPMHalfZ,
//                                   s.vAlong * (Lz - fSiPMHalfY));
//                 r->rotateX(90.*deg);
//                 break;
//             }
//     
//             new G4PVPlacement(r, p, fSiPMLV, "SiPMPhys", parentLV, false, (G4int)i, true);
//         }
//     }
//     
//     void GeometryMuonScint::PresetConfig_8SiPM_AllSides() {
//         ClearSiPMs();
//         for (G4int e = 0; e < 4; ++e) {
//             AddSiPM({e, -0.5, 0.0});
//             AddSiPM({e, +0.5, 0.0});
//         }
//     }
//     
//     void GeometryMuonScint::PresetConfig_4SiPM_OnePerSide() {
//         ClearSiPMs();
//         for (G4int e = 0; e < 4; ++e) {
//             AddSiPM({e, 0.0, 0.0});
//         }
//     }
//     
//     void GeometryMuonScint::PresetConfig_2SiPM_OneSide() {
//         ClearSiPMs();
//         AddSiPM({0, -0.5, 0.0});
//         AddSiPM({0, +0.5, 0.0});
//     }
//     
//     void GeometryMuonScint::PresetConfig_4SiPM_Corners() {
//         ClearSiPMs();
//         // 4 corners of the +X / -X edges (i.e. corners of the slab in the X-Y plane)
//         AddSiPM({0, -1.0, 0.0});  // +X edge, "near" corner
//         AddSiPM({0, +1.0, 0.0});  // +X edge, "far"  corner
//         AddSiPM({1, -1.0, 0.0});  // -X edge, "near" corner
//         AddSiPM({1, +1.0, 0.0});  // -X edge, "far"  corner
//     }
//     
