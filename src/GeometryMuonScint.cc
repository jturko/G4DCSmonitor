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
    // -----------------------------------------------------------------
    // Optical properties of UPS-92S / EJ-200 (PVT-based plastic scint).
    //
    // Key spec values (Eljen EJ-200 datasheet, Amcrys UPS-923A datasheet):
    //   - n = 1.58
    //   - Light yield = 10,000 photons/MeV (64% of anthracene)
    //   - Bulk attenuation length ~3.8 m at peak emission
    //   - Emission peak: 425 nm (2.917 eV)
    //   - Emission FWHM: ~80 nm; effectively zero below 380 nm and above 550 nm
    //   - Decay time: 2.1 ns (EJ-200) / 3.3 ns (UPS-923A)
    //   - Rise time: ~0.9 ns
    //   - Birks constant: 0.126 mm/MeV (Craun & Smith 1970, standard PVT value)
    //
    // Energy table is ASCENDING in eV (required by G4MaterialPropertiesTable).
    // We use 14 points spanning 1.77 eV (700 nm) to 4.00 eV (310 nm) so that
    // both the scintillation emission band AND any Cherenkov photons in the
    // slab are covered.
    // -----------------------------------------------------------------

    const G4int N = 14;
    G4double photonE[N] = {
        1.771*eV,  // 700 nm  - far red tail (Cherenkov coverage)
        2.067*eV,  // 600 nm
        2.156*eV,  // 575 nm
        2.250*eV,  // 551 nm
        2.339*eV,  // 530 nm
        2.450*eV,  // 506 nm
        2.594*eV,  // 478 nm
        2.755*eV,  // 450 nm
        2.917*eV,  // 425 nm  <-- emission peak
        3.024*eV,  // 410 nm
        3.100*eV,  // 400 nm
        3.300*eV,  // 376 nm
        3.543*eV,  // 350 nm
        4.000*eV   // 310 nm  - deep UV (Cherenkov coverage)
    };

    // ---- Refractive index (essentially flat across the visible) -------
    G4double rindex[N] = {
        1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58,
        1.58, 1.58, 1.58, 1.58, 1.58, 1.58, 1.58
    };

    // ---- Bulk absorption length ----------------------------------------
    // Peak: ~3.8 m (manufacturer spec). Drops rapidly above 3.1 eV due to
    // wavelength-shifter (POPOP/PPO) re-absorption, then plummets above
    // ~4 eV due to polystyrene/PVT aromatic ring absorption.
    G4double absL[N] = {
        3.0*m,       // 700 nm  - red tail, well-transmitted
        3.5*m,       // 600 nm
        3.7*m,       // 575 nm
        3.8*m,       // 551 nm  - approaching plateau
        3.8*m,       // 530 nm
        3.8*m,       // 506 nm  - peak transparency window
        3.8*m,       // 478 nm
        3.5*m,       // 450 nm
        2.5*m,       // 425 nm  - emission peak, mild self-absorption
        1.0*m,       // 410 nm  - WLS re-absorption onset
        0.30*m,      // 400 nm
        0.05*m,      // 376 nm  - POPOP absorption band
        0.005*m,     // 350 nm  - deep into fluor absorption
        0.0005*m     // 310 nm  - polymer aromatic absorption
    };

    // ---- Emission spectrum (SCINTILLATIONCOMPONENT1) -------------------
    // Sharper UV cutoff than before (real spectrum is essentially zero
    // above 3.1 eV / below 400 nm) and slightly stronger long tail to
    // match the actual EJ-200 spectrum.
    G4double scint[N] = {
        0.00,        // 700 nm  - outside emission band
        0.02,        // 600 nm  - long red tail
        0.05,        // 575 nm
        0.10,        // 551 nm
        0.18,        // 530 nm
        0.32,        // 506 nm
        0.55,        // 478 nm
        0.85,        // 450 nm
        1.00,        // 425 nm  <-- peak
        0.75,        // 410 nm
        0.30,        // 400 nm  - sharp drop
        0.02,        // 376 nm  - essentially zero
        0.00,        // 350 nm
        0.00         // 310 nm
    };

    // ---- Build and attach the properties table -------------------------
    auto* mpt = new G4MaterialPropertiesTable();
    mpt->AddProperty("RINDEX",                  photonE, rindex, N);
    mpt->AddProperty("ABSLENGTH",               photonE, absL,   N);
    mpt->AddProperty("SCINTILLATIONCOMPONENT1", photonE, scint,  N);

    // Light yield. UPS-923A: ~9,500 ph/MeV; EJ-200: 10,000 ph/MeV.
    // (Use the lower value if you want to be slightly conservative for
    //  the polystyrene-based UPS-923A; the difference is < 5%.)
    mpt->AddConstProperty("SCINTILLATIONYIELD", 10000./MeV);

    // Statistical scaling: 1.0 = pure Poisson photon-number fluctuations.
    mpt->AddConstProperty("RESOLUTIONSCALE", 1.0);

    // Decay time. Choose based on actual material:
    //   EJ-200    -> 2.1 ns
    //   UPS-923A  -> 3.3 ns
    // Default to 2.1 ns (EJ-200 datasheet).
    mpt->AddConstProperty("SCINTILLATIONTIMECONSTANT1", 2.1*ns);

    // Rise time (~0.9 ns for EJ-200). Important for TOF / timing studies;
    // negligible for muon counting. Set to zero to disable.
    mpt->AddConstProperty("SCINTILLATIONRISETIME1", 0.9*ns);

    // Single-component emission: yield fraction = 1.0 (all photons in the
    // primary fast component).
    mpt->AddConstProperty("SCINTILLATIONYIELD1", 1.0);

    scintMat->SetMaterialPropertiesTable(mpt);

    // Birks quenching constant for non-linear light yield at high dE/dx.
    // 0.126 mm/MeV is the canonical PVT value (Craun & Smith, 1970).
    scintMat->GetIonisation()->SetBirksConstant(0.126*mm/MeV);

    G4cout << " [MuonScint] Optical properties attached to '"
           << scintMat->GetName() << "' (n=1.58, yield=10k/MeV, "
           << "tau=2.1ns, rise=0.9ns, lambda_peak=425nm, "
           << "L_abs(peak)=2.5 m)" << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Reflector optical surfaces.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::BuildOpticalSurfaces()
{
    // ===================================================================
    // 1) Wrapping reflector (skin surface on the scintillator LV).
    //    Will be overridden at coupling patches by border surfaces below.
    // ===================================================================
    fReflSurface = new G4OpticalSurface("MuonScintReflector");
    fReflSurface->SetModel(unified);

    auto* surfMPT = new G4MaterialPropertiesTable();
    const G4int N = 2;
    G4double e[N] = { 1.5*eV, 6.0*eV };

    switch (fReflectorType) {
      case kNoReflector:
        //fReflSurface->SetType(dielectric_dielectric);
        //fReflSurface->SetFinish(polished);
        //break;
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(ground);
        fReflSurface->SetSigmaAlpha(0.1);  // tune: try 0.03, 0.05, 0.10, 0.20
        break;
      case kPaintTiO2: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetSigmaAlpha(0.20);
        G4double R[N] = { 0.97, 0.97 };
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        break;
      }
      case kAluminumFoil: {
        //fReflSurface->SetType(dielectric_metal);
        //fReflSurface->SetFinish(polished);
        //G4double R[N] = { 0.90, 0.90 };
        //surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        //break;
        
        // Loose foil approximation:
        // scintillator -> air gap -> polished aluminum reflector.
        // This preserves the scintillator/air Fresnel + TIR behavior.
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polishedbackpainted);
        fReflSurface->SetSigmaAlpha(0.0);
        G4double R[N] = {0.90, 0.90};
        G4double nGap[N] = {1.0003, 1.0003};
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        surfMPT->AddProperty("RINDEX",    e, nGap, N);
        break;
      }
      case kGlossyPaper: {
        //fReflSurface->SetType(dielectric_dielectric);
        //fReflSurface->SetFinish(groundfrontpainted);
        //fReflSurface->SetSigmaAlpha(0.10);
        //G4double R[N]    = { 0.95, 0.95 };
        //G4double lobe[N] = { 0.70, 0.70 };
        //surfMPT->AddProperty("REFLECTIVITY",         e, R,    N);
        //surfMPT->AddProperty("SPECULARLOBECONSTANT", e, lobe, N);
        //break;
        
        // Loose glossy paper approximation:
        // scintillator -> air gap -> mixed/specular-diffuse paper reflector.
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundbackpainted);
        fReflSurface->SetSigmaAlpha(0.10);
        G4double R[N]    = {0.95, 0.95};
        G4double lobe[N] = {0.70, 0.70};
        G4double nGap[N] = {1.0003, 1.0003};
        surfMPT->AddProperty("REFLECTIVITY",         e, R,    N);
        surfMPT->AddProperty("SPECULARLOBECONSTANT", e, lobe, N);
        surfMPT->AddProperty("RINDEX",               e, nGap, N);
        break;
      }
      case kTeflon: {
        fReflSurface->SetType(dielectric_dielectric);
        //fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetFinish(groundbackpainted);
        fReflSurface->SetSigmaAlpha(0.25);
        G4double R[N] = { 0.99, 0.99 };
        G4double nGap[N] = {1.0003, 1.0003};
        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
        surfMPT->AddProperty("RINDEX",    e, nGap, N);
        break;
      }
    }
    fReflSurface->SetMaterialPropertiesTable(surfMPT);

    // ===================================================================
    // 2) Slab <-> Grease coupling patch.
    //    Define an explicit "polished dielectric_dielectric" surface so
    //    Geant4 uses pure Fresnel transmission (and bypasses the wrapping
    //    skin surface at this patch). With n_scint=1.58, n_grease=1.46
    //    the Fresnel transmittance at normal incidence is ~99.8%.
    // ===================================================================
    fCouplingSurface = new G4OpticalSurface("MuonScintCoupling");
    fCouplingSurface->SetModel(unified);
    fCouplingSurface->SetType(dielectric_dielectric);
    fCouplingSurface->SetFinish(polished);
    // No properties table needed -- Fresnel from RINDEX is automatic.

    // ===================================================================
    // 3) Grease <-> SiPM photocathode boundary.
    //    EFFICIENCY = PDE(lambda); REFLECTIVITY = 0; type = dielectric_metal.
    //    Geant4's OpBoundaryProcess will:
    //      - apply Fresnel at the n=1.46 -> n=1.55 interface (small loss),
    //      - on transmission, kill the photon and (with prob = EFFICIENCY)
    //        register a "Detection" hit in the SD via the boundary process.
    //    Using dielectric_metal here is the standard CosmicWatch-style trick:
    //    the photon never propagates inside the SiPM bulk, exactly matching
    //    a real silicon photodetector (silicon is opaque at 425 nm anyway).
    // ===================================================================
    fSiPMSurface = new G4OpticalSurface("MuonScintSiPMPhotocathode");
    fSiPMSurface->SetModel(unified);
    //fSiPMSurface->SetType(dielectric_metal);
    fSiPMSurface->SetType(dielectric_dielectric);
    fSiPMSurface->SetFinish(polished);

    auto* sipmMPT = new G4MaterialPropertiesTable();
    // For broadband: replace with measured PDE(lambda) curve from your SiPM datasheet.
    // Hamamatsu S13360-3050 / SensL MicroFC: PDE peaks at ~40-50% near 420 nm.
    const G4int Np = 11;
    G4double ep[Np]    = { 2.067*eV, 2.156*eV, 2.250*eV, 2.339*eV, 2.450*eV,
                           2.594*eV, 2.755*eV, 2.917*eV, 3.100*eV, 3.300*eV, 3.543*eV };
    G4double pde[Np]   = { 0.20, 0.23, 0.27, 0.31, 0.35,
                           0.40, 0.43, 0.43, 0.40, 0.30, 0.18 };
    G4double rZero[Np] = { 0,0,0,0,0, 0,0,0,0,0,0 };
    //sipmMPT->AddProperty("EFFICIENCY",   ep, pde,   Np);
    //sipmMPT->AddProperty("REFLECTIVITY", ep, rZero, Np);
    fSiPMSurface->SetMaterialPropertiesTable(sipmMPT);
}


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

G4int GeometryMuonScint::Build()
{
    G4cout << " -> Constructing GeometryMuonScint (CosmicWatch-style external SiPM)..." << G4endl;

    auto* nist = G4NistManager::Instance();

    // ---------- Scintillator (PVT, n=1.58) ----------
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

    // ---------- Optical grease (silicone, n=1.46) ----------
    // EJ-560 / BC-630-style silicone optical coupling pad.
    G4Material* greaseMat = G4Material::GetMaterial("OpticalGrease", false);
    if (!greaseMat) {
        greaseMat = new G4Material("OpticalGrease", 1.06*g/cm3, 2);
        greaseMat->AddElement(nist->FindOrBuildElement("Si"), 1);
        greaseMat->AddElement(nist->FindOrBuildElement("O"),  2);

        const G4int Ng = 2;
        G4double eg[Ng]   = { 1.5*eV, 6.0*eV };
        G4double ng[Ng]   = { 1.46, 1.46 };
        G4double absg[Ng] = { 1.*m, 1.*m };   // grease is essentially transparent over 0.1 mm
        auto* mptG = new G4MaterialPropertiesTable();
        mptG->AddProperty("RINDEX",    eg, ng,   Ng);
        mptG->AddProperty("ABSLENGTH", eg, absg, Ng);
        greaseMat->SetMaterialPropertiesTable(mptG);
    }

    // ---------- SiPM bulk (epoxy/glass equivalent, n=1.55) ----------
    // The volume is a Bernoulli sink at its inner face (border surface), so
    // only the RINDEX of this material matters for the Fresnel calc at that boundary.
    G4Material* sipmMat = G4Material::GetMaterial("SiPMEpoxy", false);
    if (!sipmMat) {
        sipmMat = new G4Material("SiPMEpoxy", 1.18*g/cm3, 3);
        sipmMat->AddElement(nist->FindOrBuildElement("C"), 18);
        sipmMat->AddElement(nist->FindOrBuildElement("H"), 20);
        sipmMat->AddElement(nist->FindOrBuildElement("O"),  3);

        const G4int Ns = 2;
        G4double es[Ns]   = { 1.5*eV, 6.0*eV };
        G4double ns[Ns]   = { 1.55, 1.55 };
        auto* mptS = new G4MaterialPropertiesTable();
        mptS->AddProperty("RINDEX", es, ns, Ns);
        sipmMat->SetMaterialPropertiesTable(mptS);
    }

    // ---------- Slab solid + LV ----------
    auto* slabSolid = new G4Box("MuonScintSolid",
                                fHalfSize.x(), fHalfSize.y(), fHalfSize.z());
    fScintLV = new G4LogicalVolume(slabSolid, scintMat, "MuonScintLV");
    fScintLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.8, 1.0, 0.4)));

    // ---------- Region for fine production cuts ----------
    G4Region* scintRegion = G4RegionStore::GetInstance()->GetRegion("MuonScintRegion", false);
    if (!scintRegion) scintRegion = new G4Region("MuonScintRegion");
    scintRegion->AddRootLogicalVolume(fScintLV);

    // ---------- Grease pad LV (shared, placed N times in world) ----------
    // Solid is built so the third half-extent is the COUPLING half-thickness;
    // the rotation we apply at placement time aligns it with each edge normal.
    auto* greaseSolid = new G4Box("GreaseSolid", fSiPMHalfX, fSiPMHalfY, fCouplingHalfZ);
    fGreaseLV = new G4LogicalVolume(greaseSolid, greaseMat, "GreaseLV");
    fGreaseLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.6, 0.6, 1.0, 0.6)));

    // ---------- SiPM LV (shared, placed N times in world) ----------
    auto* sipmSolid = new G4Box("SiPMSolid", fSiPMHalfX, fSiPMHalfY, fSiPMHalfZ);
    fSiPMLV = new G4LogicalVolume(sipmSolid, sipmMat, "SiPMLV");
    fSiPMLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.0, 0.0, 1.0)));

    // ---------- Optical surfaces ----------
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

    // Place the slab in the world, alone (no daughters).
    fScintPV = new G4PVPlacement(rot,
                                 pos,
                                 fScintLV,
                                 "MuonScintPhys",
                                 worldLV,
                                 false,
                                 copyNo,
                                 surfCheck);

    // Wrapping skin -- applies on every face of the slab where no border
    // surface exists. The grease patches will override at their footprints.
    //if (fWrapWithReflector && fReflectorType != kNoReflector) {
        new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
    //}

    // Place grease pads + SiPMs as siblings of the slab in the world.
    PlaceSiPMs(worldLV, pos, rot, copyNo);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::PlaceSiPMs(G4LogicalVolume* worldLV,
                                   const G4ThreeVector& slabPos,
                                   G4RotationMatrix* slabRot,
                                   G4int /*copyNo*/)
{
    const G4double Lx = fHalfSize.x();
    const G4double Ly = fHalfSize.y();
    const G4double Lz = fHalfSize.z();

    // Half-thickness offsets along each edge's outward normal (slab-local frame).
    const G4double dGreaseHalf = fCouplingHalfZ;      // along normal
    const G4double dSiPMHalf   = fSiPMHalfZ;          // along normal

    // Centre of grease pad: just outside the slab face, half its thickness out.
    // Centre of SiPM box: outside the grease, by (grease full thickness + SiPM half).
    // i.e.
    //   grease centre offset = slabHalfExtentAlongNormal + dGreaseHalf
    //   SiPM   centre offset = slabHalfExtentAlongNormal + 2*dGreaseHalf + dSiPMHalf

    for (size_t i = 0; i < fSiPMs.size(); ++i) {
        const auto& s = fSiPMs[i];

        // -----------------------------------------------------------------
        // Clamp u/v so the (post-rotation) SiPM/grease footprint stays
        // entirely inside the slab face. After the per-edge rotations below:
        //   - For edges 0/1 (+/-X normal): the grease/SiPM box's in-plane
        //     half-extents in (u=Y, v=Z) are (fSiPMHalfX, fSiPMHalfY).
        //   - For edges 2/3 (+/-Y normal): the in-plane half-extents in
        //     (u=X, v=Z) are (fSiPMHalfX, fSiPMHalfY).
        // We clamp s.uAlong/s.vAlong (in [-1, +1]) so the footprint can't
        // overhang the slab face.
        // -----------------------------------------------------------------
        const G4double uMax = (s.edge < 2)
                            ? std::max(0.0, (Ly - fSiPMHalfX) / Ly)
                            : std::max(0.0, (Lx - fSiPMHalfX) / Lx);
        const G4double vMax = std::max(0.0, (Lz - fSiPMHalfY) / Lz);
        const G4double uC   = std::max(-uMax, std::min(+uMax, s.uAlong));
        const G4double vC   = std::max(-vMax, std::min(+vMax, s.vAlong));

        if (uC != s.uAlong || vC != s.vAlong) {
            G4cout << " [MuonScint] WARNING: SiPM #" << i
                   << " (edge " << s.edge
                   << ", u=" << s.uAlong << ", v=" << s.vAlong
                   << ") clamped to (u=" << uC << ", v=" << vC
                   << ") to fit inside slab face." << G4endl;
        }

        // -----------------------------------------------------------------
        // Slab-local centres (before applying the slab pose).
        // -----------------------------------------------------------------
        G4ThreeVector localGreaseCentre, localSiPMCentre;
        G4RotationMatrix localRot;   // rotation of grease/SiPM solid in slab-local frame

        switch (s.edge) {
          case 0: // +X face, outward normal = +x_hat
            localGreaseCentre = G4ThreeVector(+Lx + dGreaseHalf,
                                              uC * Ly,
                                              vC * Lz);
            localSiPMCentre   = G4ThreeVector(+Lx + 2.*fCouplingHalfZ + dSiPMHalf,
                                              uC * Ly,
                                              vC * Lz);
            localRot.rotateY(90.*deg);   // box's local-Z aligned with +X
            break;

          case 1: // -X face, outward normal = -x_hat
            localGreaseCentre = G4ThreeVector(-Lx - dGreaseHalf,
                                              uC * Ly,
                                              vC * Lz);
            localSiPMCentre   = G4ThreeVector(-Lx - 2.*fCouplingHalfZ - dSiPMHalf,
                                              uC * Ly,
                                              vC * Lz);
            localRot.rotateY(90.*deg);
            break;

          case 2: // +Y face, outward normal = +y_hat
            localGreaseCentre = G4ThreeVector(uC * Lx,
                                              +Ly + dGreaseHalf,
                                              vC * Lz);
            localSiPMCentre   = G4ThreeVector(uC * Lx,
                                              +Ly + 2.*fCouplingHalfZ + dSiPMHalf,
                                              vC * Lz);
            localRot.rotateX(90.*deg);
            break;

          case 3: // -Y face, outward normal = -y_hat
            localGreaseCentre = G4ThreeVector(uC * Lx,
                                              -Ly - dGreaseHalf,
                                              vC * Lz);
            localSiPMCentre   = G4ThreeVector(uC * Lx,
                                              -Ly - 2.*fCouplingHalfZ - dSiPMHalf,
                                              vC * Lz);
            localRot.rotateX(90.*deg);
            break;

          default:
            G4cerr << " [MuonScint] Invalid SiPM edge " << s.edge
                   << "; skipping." << G4endl;
            continue;
        }

        // -----------------------------------------------------------------
        // Convert slab-local -> world coordinates by composing with the
        // slab's global pose (slabRot, slabPos).
        // -----------------------------------------------------------------
        G4ThreeVector worldGreaseCentre = localGreaseCentre;
        G4ThreeVector worldSiPMCentre   = localSiPMCentre;
        if (slabRot) {
            worldGreaseCentre = (*slabRot) * worldGreaseCentre;
            worldSiPMCentre   = (*slabRot) * worldSiPMCentre;
        }
        worldGreaseCentre += slabPos;
        worldSiPMCentre   += slabPos;

        // The grease/SiPM solid orientation in world = slabRot * localRot.
        // G4PVPlacement takes ownership of the rotation matrix, so allocate.
        G4RotationMatrix* worldGreaseRot = new G4RotationMatrix(localRot);
        G4RotationMatrix* worldSiPMRot   = new G4RotationMatrix(localRot);
        if (slabRot) {
            *worldGreaseRot = (*slabRot) * (*worldGreaseRot);
            *worldSiPMRot   = (*slabRot) * (*worldSiPMRot);
        }

        // -----------------------------------------------------------------
        // Place grease pad (sibling of slab in world)
        // -----------------------------------------------------------------
        const G4String greaseName = "GreasePhys_"  + std::to_string(i);
        auto* greasePV = new G4PVPlacement(worldGreaseRot,
                                           worldGreaseCentre,
                                           fGreaseLV,
                                           greaseName,
                                           worldLV,
                                           false,
                                           (G4int)i,
                                           true);

        // -----------------------------------------------------------------
        // Place SiPM (sibling of slab and grease in world)
        // -----------------------------------------------------------------
        const G4String sipmName = "SiPMPhys_" + std::to_string(i);
        auto* sipmPV = new G4PVPlacement(worldSiPMRot,
                                         worldSiPMCentre,
                                         fSiPMLV,
                                         sipmName,
                                         worldLV,
                                         false,
                                         (G4int)i,
                                         true);

        // -----------------------------------------------------------------
        // Border surfaces:
        //   1) slab <-> grease : "polished dielectric_dielectric"
        //      (transparent Fresnel; OVERRIDES the wrapping skin surface
        //       at this patch, which is exactly what we want -- a hole in
        //       the wrapping where the SiPM couples in.)
        //   2) grease <-> SiPM : photocathode (EFFICIENCY = PDE(lambda))
        //      Geant4's OpBoundaryProcess applies Fresnel + EFFICIENCY,
        //      and on detection KILLS the photon. The SD just counts.
        // -----------------------------------------------------------------
        if (fScintPV && fCouplingSurface) {
            new G4LogicalBorderSurface(
                "MuonScint_SlabGrease_" + std::to_string(i),
                fScintPV, greasePV, fCouplingSurface);
            // also the reverse direction (a photon could be re-entering the
            // slab from the grease after a Fresnel reflection; explicit border
            // here keeps Geant4 from falling back to the wrapping skin).
            new G4LogicalBorderSurface(
                "MuonScint_GreaseSlab_" + std::to_string(i),
                greasePV, fScintPV, fCouplingSurface);
        }

        if (fSiPMSurface) {
            new G4LogicalBorderSurface(
                "MuonScint_GreaseSiPM_" + std::to_string(i),
                greasePV, sipmPV, fSiPMSurface);
        }
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







