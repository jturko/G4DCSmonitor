#include "GeometryMuonScint.hh"

#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4TessellatedSolid.hh"
#include "G4TriangularFacet.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4MaterialPropertiesTable.hh"
#include "G4OpticalSurface.hh"
#include "G4LogicalSkinSurface.hh"
#include "G4LogicalBorderSurface.hh"
#include "G4SubtractionSolid.hh"
#include "G4UnionSolid.hh"
#include "G4DisplacedSolid.hh"
#include "G4UserLimits.hh"
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

G4bool GeometryMuonScint::sHasWLSFibres = false;

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
    mpt->AddConstProperty("SCINTILLATIONYIELD", fLightYieldScale * 10000./MeV);

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
    // -----------------------------------------------------------------
    // Common photon-energy grid for all reflector spectra.
    // ~650 nm (red) ... ~360 nm (UV), ascending in eV as required.
    // -----------------------------------------------------------------
    const G4int N = 10;
    G4double e[N] = {
        1.907*eV,  // 650 nm
        2.066*eV,  // 600 nm
        2.254*eV,  // 550 nm
        2.480*eV,  // 500 nm
        2.638*eV,  // 470 nm
        2.818*eV,  // 440 nm
        2.952*eV,  // 420 nm  <-- SiPM/scint peak
        3.100*eV,  // 400 nm
        3.263*eV,  // 380 nm
        3.444*eV   // 360 nm
    };

    // -----------------------------------------------------------------
    // 1) Wrapping reflector (skin surface on the scintillator LV).
    // -----------------------------------------------------------------
    fReflSurface = new G4OpticalSurface("MuonScintReflector");
    fReflSurface->SetModel(unified);

    switch (fReflectorType) {

      // ============================================================
      // 0) BARE: no reflector layer. This G4OpticalSurface is NOT applied
      //    as a wrapping skin. Instead the plate is treated as in effective
      //    optical contact with the black holder, so non-instrumented faces
      //    absorb via the absorber skin (see PlaceDetector / BuildHolder).
      //    Free-standing polished TIR is deliberately not used: measured
      //    bare yield is the lowest of all configurations, which a lossless
      //    TIR guide cannot reproduce.
      // ============================================================
      case kNoReflector: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polished);
        // Deliberately attach NO MPT here -- this surface is unused.
        break;
      }

      // ============================================================
      // 1) TiO2 PAINT (EJ-510 or similar epoxy paint applied directly).
      //    groundfrontpainted: the paint is the outermost medium, so the
      //    REFLECTIVITY/TRANSMITTANCE path is used directly. Lambertian
      //    emission is physically correct for TiO2 sub-surface scattering.
      //    EJ-510 effective reflectance is ~0.98 over the emission band.
      // ============================================================
      case kPaintTiO2: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundfrontpainted);
        fReflSurface->SetSigmaAlpha(0.1);  // ~5.7 deg, paint texture

        auto* mpt = new G4MaterialPropertiesTable();
        G4double R[N]      = { 0.975, 0.980, 0.982, 0.983, 0.983,
                               0.982, 0.980, 0.977, 0.970, 0.955 };
        G4double specSp[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double specLo[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double backSc[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        mpt->AddProperty("REFLECTIVITY",          e, R,      N);
        mpt->AddProperty("SPECULARSPIKECONSTANT", e, specSp, N);
        mpt->AddProperty("SPECULARLOBECONSTANT",  e, specLo, N);
        mpt->AddProperty("BACKSCATTERCONSTANT",   e, backSc, N);
        fReflSurface->SetMaterialPropertiesTable(mpt);
        break;
      }

      // ============================================================
      // 2) ALUMINUM FOIL with air gap (loose-wrap like Mylar/Al).
      //    "polishedbackpainted" preserves TIR at the slab/air
      //    interface; sub-TIR photons enter the gap, hit the Al
      //    "paint", and reflect SPECULARLY (spike = 1.0) because Al
      //    is a smooth metal mirror.
      // ============================================================
      case kAluminumFoil: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polishedbackpainted);
        fReflSurface->SetSigmaAlpha(0.0);

        auto* mpt = new G4MaterialPropertiesTable();
        G4double R[N]      = { 0.880, 0.900, 0.910, 0.910, 0.910,
                               0.900, 0.890, 0.880, 0.860, 0.840 };
        G4double nGap[N]   = { 1.0003,1.0003,1.0003,1.0003,1.0003,
                               1.0003,1.0003,1.0003,1.0003,1.0003 };
        G4double specSp[N] = { 1.,1.,1.,1.,1., 1.,1.,1.,1.,1. };  // pure mirror
        G4double specLo[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double backSc[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        mpt->AddProperty("REFLECTIVITY",          e, R,      N);
        mpt->AddProperty("RINDEX",                e, nGap,   N);
        mpt->AddProperty("SPECULARSPIKECONSTANT", e, specSp, N);
        mpt->AddProperty("SPECULARLOBECONSTANT",  e, specLo, N);
        mpt->AddProperty("BACKSCATTERCONSTANT",   e, backSc, N);
        fReflSurface->SetMaterialPropertiesTable(mpt);
        break;
      }

      // ============================================================
      // 3) HIGH-GLOSS PAPER (specular mirror finish, air gap).
      //    The paper reports Al foil and glossy paper give nearly
      //    identical, highest signal-to-noise; glossy is therefore
      //    modelled like a slightly lossier mirror (specular spike).
      // ============================================================
      case kGlossyPaper: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polishedbackpainted);
        fReflSurface->SetSigmaAlpha(0.0);

        auto* mpt = new G4MaterialPropertiesTable();
        G4double R[N]      = { 0.940, 0.950, 0.955, 0.955, 0.955,
                               0.950, 0.945, 0.940, 0.925, 0.900 };
        G4double nGap[N]   = { 1.0003,1.0003,1.0003,1.0003,1.0003,
                               1.0003,1.0003,1.0003,1.0003,1.0003 };
        G4double specSp[N] = { 1.,1.,1.,1.,1., 1.,1.,1.,1.,1. };
        G4double specLo[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double backSc[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        mpt->AddProperty("REFLECTIVITY",          e, R,      N);
        mpt->AddProperty("RINDEX",                e, nGap,   N);
        mpt->AddProperty("SPECULARSPIKECONSTANT", e, specSp, N);
        mpt->AddProperty("SPECULARLOBECONSTANT",  e, specLo, N);
        mpt->AddProperty("BACKSCATTERCONSTANT",   e, backSc, N);
        fReflSurface->SetMaterialPropertiesTable(mpt);
        break;
      }

      // ============================================================
      // 4) PTFE / TEFLON tape with air gap (NOT part of Rao 2025;
      //    kept for completeness). Near-perfect Lambertian scatterer.
      // ============================================================
      case kTeflon: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(groundbackpainted);
        fReflSurface->SetSigmaAlpha(0.1);

        auto* mpt = new G4MaterialPropertiesTable();
        G4double R[N]      = { 0.990, 0.990, 0.990, 0.990, 0.990,
                               0.985, 0.980, 0.970, 0.950, 0.920 };
        G4double nGap[N]   = { 1.0003,1.0003,1.0003,1.0003,1.0003,
                               1.0003,1.0003,1.0003,1.0003,1.0003 };
        G4double specSp[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double specLo[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double backSc[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        mpt->AddProperty("REFLECTIVITY",          e, R,      N);
        mpt->AddProperty("RINDEX",                e, nGap,   N);
        mpt->AddProperty("SPECULARSPIKECONSTANT", e, specSp, N);
        mpt->AddProperty("SPECULARLOBECONSTANT",  e, specLo, N);
        mpt->AddProperty("BACKSCATTERCONSTANT",   e, backSc, N);
        fReflSurface->SetMaterialPropertiesTable(mpt);
        break;
      }
    }

    // -----------------------------------------------------------------
    // 2) Slab <-> Grease coupling patch.
    //    Pure Fresnel on a polished dielectric/dielectric boundary.
    //    n_scint = 1.58, n_grease = 1.46  ->  T(0 deg) ~ 99.84%
    // -----------------------------------------------------------------
    fCouplingSurface = new G4OpticalSurface("MuonScintCoupling");
    fCouplingSurface->SetModel(unified);
    fCouplingSurface->SetType(dielectric_dielectric);
    fCouplingSurface->SetFinish(polished);
    // No MPT -- Fresnel from material RINDEX is automatic.

    // -----------------------------------------------------------------
    // 3) Grease <-> SiPM photocathode boundary.
    //    dielectric_metal + EFFICIENCY = PDE(lambda), REFLECTIVITY = 0.
    //    G4OpBoundaryProcess performs the PDE Bernoulli draw and, on
    //    success, marks the status Detection and invokes the SD. Photons
    //    are killed either way (the SiPM is opaque).
    // -----------------------------------------------------------------
    fSiPMSurface = new G4OpticalSurface("MuonScintSiPMPhotocathode");
    fSiPMSurface->SetModel(unified);
    fSiPMSurface->SetType(dielectric_metal);
    fSiPMSurface->SetFinish(polished);

    const G4int Np = 11;
    G4double ep[Np]    = { 2.067*eV, 2.156*eV, 2.250*eV, 2.339*eV, 2.450*eV,
                           2.594*eV, 2.755*eV, 2.917*eV, 3.100*eV, 3.300*eV, 3.543*eV };
    // ASD-NUV4S-P-40: PDE peaks ~43% near 420 nm (blue/UV-enhanced).
    G4double pde[Np]   = { 0.20, 0.23, 0.27, 0.31, 0.35,
                           0.40, 0.43, 0.43, 0.40, 0.30, 0.18 };
    G4double rZero[Np] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0.,0. };
    auto* sipmMPT = new G4MaterialPropertiesTable();
    sipmMPT->AddProperty("EFFICIENCY",   ep, pde,   Np);
    sipmMPT->AddProperty("REFLECTIVITY", ep, rZero, Np);
    fSiPMSurface->SetMaterialPropertiesTable(sipmMPT);

    // -----------------------------------------------------------------
    // 4) Black holder inner surface (absorber).
    //    dielectric_dielectric with REFLECTIVITY = TRANSMITTANCE = 0 and
    //    EFFICIENCY = 0 -> every photon is absorbed (killed).
    // -----------------------------------------------------------------
    fHolderSurface = new G4OpticalSurface("MuonScintHolderAbsorber");
    fHolderSurface->SetModel(unified);
    fHolderSurface->SetType(dielectric_dielectric);
    fHolderSurface->SetFinish(polished);
    {
        const G4int Nh = 2;
        G4double eh[Nh] = { 1.5*eV, 6.0*eV };
        G4double r0[Nh] = { 0.0, 0.0 };
        G4double t0[Nh] = { 0.0, 0.0 };
        auto* holdMPT = new G4MaterialPropertiesTable();
        holdMPT->AddProperty("REFLECTIVITY",   eh, r0, Nh);
        holdMPT->AddProperty("TRANSMITTANCE",  eh, t0, Nh);
        fHolderSurface->SetMaterialPropertiesTable(holdMPT);
    }
}





//void GeometryMuonScint::BuildOpticalSurfaces()
//{
//    // the main reflector which covers the entire scintillator slab. 
//    // the coupling patches (to grease, then to sipm) should be over-ridden using 
//    // the optical surfaces defined after fReflSurface
//    fReflSurface = new G4OpticalSurface("OpSurface_scinti_reflector");
//    fReflSurface->SetModel(unified);
//
//    auto* surfMPT = new G4MaterialPropertiesTable();
//    const G4int N = 2;
//    G4double e[N] = { 1.5*eV, 6.0*eV };
//
//    switch (fReflectorType) {
//      case kNoReflector: {
//        fReflSurface->SetType(dielectric_dielectric);
//        fReflSurface->SetFinish(polished);
//        break;
//        //fReflSurface->SetType(dielectric_dielectric);
//        //fReflSurface->SetFinish(ground);
//        //fReflSurface->SetSigmaAlpha(0.0); 
//        //G4double one[N]  = {1.0, 1.0};
//        //G4double zero[N] = {0.0, 0.0};
//        //surfMPT->AddProperty("SPECULARLOBECONSTANT",  e, one,  N);
//        //surfMPT->AddProperty("SPECULARSPIKECONSTANT", e, zero, N);
//        //surfMPT->AddProperty("BACKSCATTERCONSTANT",   e, zero, N);
//        //break;
//      }
//      case kPaintTiO2: {
//        fReflSurface->SetType(dielectric_dielectric);
//        fReflSurface->SetFinish(groundfrontpainted);
//        fReflSurface->SetSigmaAlpha(0.20);
//        G4double R[N] = { 0.97, 0.97 };
//        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
//        break;
//      }
//      case kAluminumFoil: {
//        fReflSurface->SetType(dielectric_dielectric);
//        fReflSurface->SetFinish(polishedbackpainted);
//        fReflSurface->SetSigmaAlpha(0.0);
//        G4double R[N] = {0.90, 0.90};
//        G4double nGap[N] = {1.0003, 1.0003};
//        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
//        surfMPT->AddProperty("RINDEX",    e, nGap, N);
//        break;
//      }
//      case kGlossyPaper: {
//        fReflSurface->SetType(dielectric_dielectric);
//        fReflSurface->SetFinish(groundbackpainted);
//        fReflSurface->SetSigmaAlpha(0.10);
//        G4double R[N]    = {0.95, 0.95};
//        G4double lobe[N] = {0.70, 0.70};
//        G4double nGap[N] = {1.0003, 1.0003};
//        surfMPT->AddProperty("REFLECTIVITY",         e, R,    N);
//        surfMPT->AddProperty("SPECULARLOBECONSTANT", e, lobe, N);
//        surfMPT->AddProperty("RINDEX",               e, nGap, N);
//        break;
//      }
//      case kTeflon: {
//        fReflSurface->SetType(dielectric_dielectric);
//        fReflSurface->SetFinish(groundbackpainted);
//        fReflSurface->SetSigmaAlpha(0.25);
//        G4double R[N] = { 0.99, 0.99 };
//        G4double nGap[N] = {1.0003, 1.0003};
//        surfMPT->AddProperty("REFLECTIVITY", e, R, N);
//        surfMPT->AddProperty("RINDEX",    e, nGap, N);
//        break;
//      }
//    }
//    fReflSurface->SetMaterialPropertiesTable(surfMPT);
//
//    // coupling to grease, should be polished and use pure Fresnel
//    // this should override the skin surface fReflSurface when placed
//    fCouplingSurface = new G4OpticalSurface("OpSurface_scinti_to_grease");
//    fCouplingSurface->SetModel(unified);
//    fCouplingSurface->SetType(dielectric_dielectric);
//    fCouplingSurface->SetFinish(polished);
//    auto* couplingMPT = new G4MaterialPropertiesTable();
//    G4double R_coup[N] = { 0.99, 0.99 };
//    G4double nGap_coup[N] = {1.0003, 1.0003};
//    couplingMPT->AddProperty("RINDEX",    e, nGap_coup, N);
//
//    // ===================================================================
//    // 3) Grease <-> SiPM photocathode boundary.
//    //    EFFICIENCY = PDE(lambda); REFLECTIVITY = 0; type = dielectric_metal.
//    //    Geant4's OpBoundaryProcess will:
//    //      - apply Fresnel at the n=1.46 -> n=1.55 interface (small loss),
//    //      - on transmission, kill the photon and (with prob = EFFICIENCY)
//    //        register a "Detection" hit in the SD via the boundary process.
//    //    Using dielectric_metal here is the standard CosmicWatch-style trick:
//    //    the photon never propagates inside the SiPM bulk, exactly matching
//    //    a real silicon photodetector (silicon is opaque at 425 nm anyway).
//    // ===================================================================
//    fSiPMSurface = new G4OpticalSurface("OpSurface_grease_to_SiPM");
//    fSiPMSurface->SetModel(unified);
//    fSiPMSurface->SetType(dielectric_metal);
//    //fSiPMSurface->SetType(dielectric_dielectric);
//    fSiPMSurface->SetFinish(polished);
//
//    auto* sipmMPT = new G4MaterialPropertiesTable();
//    // For broadband: replace with measured PDE(lambda) curve from your SiPM datasheet.
//    // Hamamatsu S13360-3050 / SensL MicroFC: PDE peaks at ~40-50% near 420 nm.
//    const G4int Np = 11;
//    G4double ep[Np]    = { 2.067*eV, 2.156*eV, 2.250*eV, 2.339*eV, 2.450*eV,
//                           2.594*eV, 2.755*eV, 2.917*eV, 3.100*eV, 3.300*eV, 3.543*eV };
//    G4double pde[Np]   = { 0.20, 0.23, 0.27, 0.31, 0.35,
//                           0.40, 0.43, 0.43, 0.40, 0.30, 0.18 };
//    G4double rZero[Np] = { 0,0,0,0,0, 0,0,0,0,0,0 };
//    sipmMPT->AddProperty("EFFICIENCY",   ep, pde,   Np);
//    sipmMPT->AddProperty("REFLECTIVITY", ep, rZero, Np);
//    fSiPMSurface->SetMaterialPropertiesTable(sipmMPT);
//}


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
    fScintLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.45, 0.75, 1.0, 0.55)));

    // ---------- Region for fine production cuts ----------
    G4Region* scintRegion = G4RegionStore::GetInstance()->GetRegion("MuonScintRegion", false);
    if (!scintRegion) scintRegion = new G4Region("MuonScintRegion");
    scintRegion->AddRootLogicalVolume(fScintLV);

    // ---------- Grease pad LV (shared, placed N times in world) ----------
    // Solid is built so the third half-extent is the COUPLING half-thickness;
    // the rotation we apply at placement time aligns it with each edge normal.
    auto* greaseSolid = new G4Box("GreaseSolid", fSiPMHalfX, fSiPMHalfY, fCouplingHalfZ);
    fGreaseLV = new G4LogicalVolume(greaseSolid, greaseMat, "GreaseLV");
    fGreaseLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.85, 0.2, 0.6)));

    // ---------- SiPM LV (shared, placed N times in world) ----------
    auto* sipmSolid = new G4Box("SiPMSolid", fSiPMHalfX, fSiPMHalfY, fSiPMHalfZ);
    fSiPMLV = new G4LogicalVolume(sipmSolid, sipmMat, "SiPMLV");
    fSiPMLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.0, 0.0, 1.0)));

    // ---------- Optical surfaces ----------
    BuildOpticalSurfaces();

    // ---------- Config 4: WLS fibre clamp ----------
    if (fUseWLSFibres) BuildWLSFibres();

    // ---------- Black holder for the bare configuration ----------
    if (UsesBlackHolder()) BuildHolder();

    G4cout << " -> Finished constructing GeometryMuonScint." << G4endl;
    return 1;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Black 3D-printed holder that encloses the bare plate, reproducing the real
// "bare plate in a black ABS holder" setup (Rao et al. 2025, Sec. III A).
// The holder is a shell (outer box minus an inner cavity box). The cavity is
// drawn slightly larger than the plate for visualization; the optical loss
// from the tight plate/holder contact (frustrated TIR) is modelled by the
// absorber skin on the slab in PlaceDetector, while this shell's own
// absorbing skin catches any photon that does cross the drawn gap.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::BuildHolder()
{
    G4NistManager* nist = G4NistManager::Instance();

    // Matte-black absorber (near-zero reflectivity at optical wavelengths).
    G4Material* holderMat = G4Material::GetMaterial("MuonScintHolderBlack", false);
    if (!holderMat) {
        holderMat = new G4Material("MuonScintHolderBlack", 1.05*g/cm3, 2);
        holderMat->AddElement(nist->FindOrBuildElement("C"), 1);
        holderMat->AddElement(nist->FindOrBuildElement("H"), 1);

        const G4int Nb = 2;
        G4double eb[Nb]   = { 1.5*eV, 6.0*eV };
        G4double nb[Nb]   = { 1.55, 1.55 };
        G4double absb[Nb] = { 1.0*um, 1.0*um };   // strongly absorbing
        auto* mptB = new G4MaterialPropertiesTable();
        mptB->AddProperty("RINDEX",    eb, nb,   Nb);
        mptB->AddProperty("ABSLENGTH", eb, absb, Nb);
        holderMat->SetMaterialPropertiesTable(mptB);
    }

    // Outer dimensions of the cavity = slab + air gap on all six faces.
    const G4double cavX = fHalfSize.x() + fHolderGap;
    const G4double cavY = fHalfSize.y() + fHolderGap;
    const G4double cavZ = fHalfSize.z() + fHolderGap;

    auto* outerSolid = new G4Box("HolderOuter",
                                 cavX + fHolderWall,
                                 cavY + fHolderWall,
                                 cavZ + fHolderWall);
    auto* cavitySolid = new G4Box("HolderCavity", cavX, cavY, cavZ);
    auto* shellSolid = new G4SubtractionSolid("HolderShell",
                                              outerSolid, cavitySolid);

    fHolderLV = new G4LogicalVolume(shellSolid, holderMat, "MuonScintHolderLV");
    fHolderLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.15, 0.15, 0.15, 0.5)));
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Config 4 (Rao et al. 2025): WLS fibre clamp.
//
// A black, optically-absorbing bar clamp sits on the -X narrow face of the
// plate (greased to it). It holds fNFibres Y-11(200) WLS fibres that run the
// full length of the plate (y); their far ends are mirrored, the other ends are
// bundled onto a single SiPM. Built with the bundle at slab-local +y / mirror at
// -y, the whole sub-assembly is then flipped 180 deg about the slab-local x
// axis at placement so the read-out ends up at -y (report frame: top-left).
// The clamp face pointing away from the slab (the fibres' radial "open"
// direction, -X) carries a specular reflector so light escaping a fibre
// radially is bounced back into it.
//
// The fibres are the WLS medium: core = polystyrene + Y-11 fluor
// (n=1.59, absorption peak 430 nm, emission peak 476 nm, tau=~7 ns), single
// PMMA cladding (n=1.49). G4OpWLS is driven by the MPT properties
// WLSABSLENGTH / WLSCOMPONENT / WLSTIMECONSTANT on the core material.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

// Centreline of one bent fibre, expressed DIRECTLY in slab-local coordinates
// (axis runs along +y; the transverse offset is along z). This sidesteps every
// rotation-convention pitfall of the old union approach: the SAME point list
// feeds both the fibre and the clamp/grease cutter, with no placement or
// operand rotations at all.
//   x         fibre axis x (slab-local, same for all four fibres' own frame)
//   zSpread   centreline z over the straight run
//   zBundle   centreline z of the touching bundle at the read-out end
//   Ly        plate half-length; the fibre spans y in [-Ly, +Ly]
//   conv      straight run stops this far short of +Ly (ramp start)
//   resL      straight bundled stub before +Ly (ramp end)
//   ov        axial overshoot of the two straight ends (0 for the fibre, >0 for
//             the cutter, so the groove is a through-bore bounded by the box)
static std::vector<G4ThreeVector> BentPath(G4double x, G4double zSpread,
                                           G4double zBundle, G4double Ly,
                                           G4double conv, G4double resL,
                                           G4double ov)
{
    const G4double yA = Ly - conv;   // spread -> ramp
    const G4double yB = Ly - resL;   // ramp    -> bundle
    return { G4ThreeVector(x, -Ly - ov, zSpread),
             G4ThreeVector(x,  yA,     zSpread),
             G4ThreeVector(x,  yB,     zBundle),
             G4ThreeVector(x,  Ly + ov, zBundle) };
}

// Build a swept (generalized) cylinder along a piecewise-linear centreline with
// a circular cross-section of radius r, as a G4TessellatedSolid.
//
// WHY TESSELLATED: Geant4 has no analytic bent-tube solid, and a union of
// tilted G4Tubs leaves a wedge-shaped AIR GAP on the outside of every kink (a
// plain union is not watertight there), while adding joint spheres makes the
// boolean mesher give up (10000-point cloud -> 18 MB VRML). A tessellated
// generalized cylinder is a single watertight surface with no boolean, no gap,
// and a tiny mesh.
//
// The rings at each path node are oriented on the MITERED tangent (the
// bisector of the incoming/outgoing segments), so consecutive segments share
// one tilted ring: the surface closes exactly with no gap and no overlap (this
// is the standard swept-polyline tube). The centreline stays in a plane of
// constant x, so a single transverse axis u=(1,0,0) is perpendicular to every
// ring normal and the rings never twist.
static G4VSolid* MakeSweptTube(const G4String& name, G4double r,
                               const std::vector<G4ThreeVector>& path,
                               G4int nSeg)
{
    const std::size_t n = path.size();
    auto* solid = new G4TessellatedSolid(name);

    // Ring normal at each node: end nodes use the single adjacent tangent, the
    // interior nodes use the normalized bisector (the miter direction).
    std::vector<G4ThreeVector> nrm(n);
    for (std::size_t i = 0; i < n; ++i) {
        G4ThreeVector tIn, tOut;
        if (i > 0)     tIn  = (path[i] - path[i-1]).unit();
        if (i + 1 < n) tOut = (path[i+1] - path[i]).unit();
        nrm[i] = (i == 0)          ? tOut
               : (i + 1 == n)      ? tIn
               : (tIn + tOut).unit();
    }

    const G4ThreeVector u(1., 0., 0.);   // perpendicular to every ring normal
    std::vector<std::vector<G4ThreeVector>> ring(n,
                                                 std::vector<G4ThreeVector>(nSeg));
    for (std::size_t i = 0; i < n; ++i) {
        const G4ThreeVector v = nrm[i].cross(u).unit();
        for (G4int k = 0; k < nSeg; ++k) {
            const G4double th = (2.*3.14159265358979323846*k)/nSeg;
            ring[i][k] = path[i] + r*(std::cos(th)*u + std::sin(th)*v);
        }
    }

    // Side wall: two triangles per (segment, azimuth) with outward orientation.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        for (G4int k = 0; k < nSeg; ++k) {
            const G4int k2 = (k + 1) % nSeg;
            solid->AddFacet(new G4TriangularFacet(ring[i][k], ring[i][k2],
                                                  ring[i+1][k2], ABSOLUTE));
            solid->AddFacet(new G4TriangularFacet(ring[i][k], ring[i+1][k2],
                                                  ring[i+1][k], ABSOLUTE));
        }
    }
    // Flat end caps (triangle fans); the -y cap winds the opposite way so its
    // normal also points outward.
    for (G4int k = 0; k < nSeg; ++k) {
        const G4int k2 = (k + 1) % nSeg;
        solid->AddFacet(new G4TriangularFacet(path[0], ring[0][k2], ring[0][k],
                                              ABSOLUTE));
        solid->AddFacet(new G4TriangularFacet(path[n-1], ring[n-1][k],
                                              ring[n-1][k2], ABSOLUTE));
    }
    solid->SetSolidClosed(true);
    return solid;
}

void GeometryMuonScint::BuildWLSFibres()
{
    G4NistManager* nist = G4NistManager::Instance();

    // ---------- Fibre core: polystyrene + Y-11(200) fluor ----------
    // (Kuraray Y-11: PS core. The fluor is modelled only through its optical
    //  signature, not its ppm concentration, which is the standard approach.)
    G4Material* coreMat = G4Material::GetMaterial("WLSFiberCore", false);
    if (!coreMat) {
        coreMat = new G4Material("WLSFiberCore", 1.05*g/cm3, 2);
        coreMat->AddElement(nist->FindOrBuildElement("C"), 8);
        coreMat->AddElement(nist->FindOrBuildElement("H"), 8);

        // Photon-energy grid: 1.9 eV (650 nm) ... 3.4 eV (365 nm), ascending.
        const G4int N = 13;
        G4double e[N] = { 1.907*eV, 2.066*eV, 2.138*eV, 2.214*eV, 2.296*eV,
                          2.385*eV, 2.480*eV, 2.583*eV, 2.695*eV, 2.818*eV,
                          2.952*eV, 3.100*eV, 3.263*eV };
        G4double rindexW[N] = { 1.59,1.59,1.59,1.59,1.59,1.59,1.59,
                                1.59,1.59,1.59,1.59,1.59,1.59 };
        // Bulk absorption: essentially transparent (long polystyrene
        // attenuation) in the green (>500 nm); drops steeply below the Y-11
        // absorption peak at 430 nm (2.883 eV) so blue light is captured.
        G4double absW[N] = {
            3.5*m, 3.5*m, 3.5*m, 3.5*m, 3.5*m,      // 650-541 nm: transparent
            3.5*m, 3.5*m, 1.5*m, 0.30*m, 0.06*m,    // 500-428 nm: absorption onset
            0.006*m, 0.0006*m, 0.00006*m };          // <415 nm: strong Y-11 absorption
        // Y-11 emission (peak ~476 nm / 2.605 eV), broad, zero above ~3.0 eV.
        G4double wlsComp[N] = {
            0.00, 0.02, 0.05, 0.12, 0.30,
            0.65, 1.00, 0.72, 0.35, 0.12,
            0.03, 0.00, 0.00 };
        auto* mptW = new G4MaterialPropertiesTable();
        mptW->AddProperty("RINDEX",       e, rindexW, N);
        mptW->AddProperty("ABSLENGTH",    e, absW,    N);   // non-WLS loss
        mptW->AddProperty("WLSABSLENGTH", e, absW,    N);   // WLS absorption length
        mptW->AddProperty("WLSCOMPONENT", e, wlsComp, N);
        mptW->AddConstProperty("WLSTIMECONSTANT", 7.0*ns);  // Y-11 decay
        coreMat->SetMaterialPropertiesTable(mptW);
    }

    // ---------- Fibre cladding: PMMA (n=1.49) ----------
    G4Material* cladMat = G4Material::GetMaterial("WLSFiberClad", false);
    if (!cladMat) {
        cladMat = new G4Material("WLSFiberClad", 1.19*g/cm3, 3);
        cladMat->AddElement(nist->FindOrBuildElement("C"), 5);
        cladMat->AddElement(nist->FindOrBuildElement("H"), 8);
        cladMat->AddElement(nist->FindOrBuildElement("O"), 2);

        const G4int Nc = 2;
        G4double ec[Nc]   = { 1.5*eV, 6.0*eV };
        G4double nc[Nc]   = { 1.49, 1.49 };
        G4double absc[Nc] = { 20.*m, 20.*m };
        auto* mptC = new G4MaterialPropertiesTable();
        mptC->AddProperty("RINDEX",    ec, nc,   Nc);
        mptC->AddProperty("ABSLENGTH", ec, absc, Nc);
        cladMat->SetMaterialPropertiesTable(mptC);
    }

    // ---------- Bent fibre core + cladding solids (one pair per fibre) ------
    // Each fibre runs straight at the straight-section pitch (fFibrePitch) for
    // most of the plate, then converges over fFibreConvergeLen to a TOUCHING
    // bundle (fBundlePitch) at the +y read-out end, where all four ends face the
    // single SiPM. The guide is a tessellated swept cylinder (see MakeSweptTube),
    // watertight through the kinks; each fibre needs its own clad+core solids
    // because the transverse bend offset differs per fibre.
    const G4double rFibre = fFibreRadius;
    const G4double rCore  = rFibre * (1.0 - fFibreCladFrac);
    const G4double halfL  = fHalfSize.y();   // run the full plate length (y)
    const G4double resL   = 2.0*mm;          // straight bundled stub before +end
    const G4double conv   = fFibreConvergeLen;
    // Cutter overshoot: the groove tool is made longer than the film/clamp so
    // the opening is bounded by the object's own faces, never by a face
    // coincident with the tool (coincident faces -> degenerate polygons).
    const G4double cutterOv = 10.0*mm;
    const G4int    ringSeg  = 24;            // azimuthal facets around a fibre

    const G4double sx = fWLSOnNegX ? -1.0 : +1.0;      // instrumented face normal
    const G4double zSpacing = fFibrePitch;              // straight-section pitch

    const G4double filmHalfX  = 0.5 * fClampGap;
    const G4double filmCx     = sx * (fHalfSize.x() + filmHalfX);

    // Fibre read-out axis positions (slab-local). The fibre centre sits in the
    // clamp channel, offset toward the holder so the fibre is proud of the
    // clamp front face (x = sx*(fHalfSize.x()+fClampGap)) by fFibreProtrusion,
    // i.e. it dips into the grease film but stops short of the scintillator.
    const G4double clampFrontX    = fHalfSize.x() + fClampGap;
    const G4double fibreAxisXAbs  = clampFrontX + rFibre - fFibreProtrusion;
    fFibreAxisX    = sx * fibreAxisXAbs;
    fFibreZSpacing = zSpacing;

    // Per-fibre centreline z on the straight run and at the bundle. The paths
    // below are in slab-local coordinates, so no rotation is ever involved.
    std::vector<G4double> zSpread(fNFibres), zBundle(fNFibres);
    for (G4int k = 0; k < fNFibres; ++k) {
        zSpread[k] = (k - 0.5*(fNFibres - 1)) * fFibrePitch;
        zBundle[k] = (k - 0.5*(fNFibres - 1)) * fBundlePitch;
    }

    fFibreCoreLVs.clear();
    fFibreCladLVs.clear();
    for (G4int k = 0; k < fNFibres; ++k) {
        const auto tag = std::to_string(k);
        const auto path = BentPath(fFibreAxisX, zSpread[k], zBundle[k],
                                   halfL, conv, resL, 0.);

        auto* cladSolid = MakeSweptTube("WLSFibreCladSolid_" + tag, rFibre,
                                        path, ringSeg);
        auto* cladLV = new G4LogicalVolume(cladSolid, cladMat,
                                           "WLSFibreCladLV_" + tag);
        cladLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.9, 0.5, 0.6)));

        auto* coreSolid = MakeSweptTube("WLSFibreCoreSolid_" + tag, rCore,
                                        path, ringSeg);
        auto* coreLV = new G4LogicalVolume(coreSolid, coreMat,
                                           "WLSFibreCoreLV_" + tag);
        coreLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.7, 1.0, 0.9, 0.95)));
        coreLV->SetUserLimits(new G4UserLimits(DBL_MAX, DBL_MAX, 10.*ms));
        // Core shares the clad's slab-local frame, so it is placed at the clad
        // origin; every clad placement then carries its own core copy.
        new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 0.), coreLV,
                          "WLSFibreCorePhys_" + tag, cladLV, false, k, false);

        fFibreCladLVs.push_back(cladLV);
        fFibreCoreLVs.push_back(coreLV);
    }

    // ---------- Thin grease coupling film (slab face -> clamp front) ----------
    // The narrow gap between the instrumented narrow face (default -X; the side
    // opposite the edge/corner MCD regions) and the clamp body is filled with
    // optical grease, matching Rao et al. 2025, Sec. III A: "the gap between
    // the WLS fiber clamp and the side face of the scintillator plate is
    // liberally filled with optical grease." The four fibres are seated in
    // channels milled into the clamp and poke fFibreProtrusion past the channel
    // mouths into this film; their cylindrical footprints are subtracted from
    // the film so the volumes stay disjoint. The grooves FOLLOW the converging
    // fibres (same dOff), so the grease<->cladding interface stays continuous
    // along the whole length (no air gap, no overlap). The groove is a through
    // bore; the fibre only dips fFibreProtrusion into the film, so a continuous
    // grease layer remains against the scintillator. The clad index (1.49) still
    // exceeds the grease index (1.46), so the fibre core/clad TIR guide is
    // preserved.
    auto* filmBox = new G4Box("WLSCoupleFilmBox",
                              filmHalfX, fHalfSize.y(), fHalfSize.z());
    G4Material* greaseMat = G4Material::GetMaterial("OpticalGrease", false);
    {
        // Carve the film with a groove CONCENTRIC to each fibre and of radius
        // rFibre, so the grease directly wets the fibre surface -- a coincident
        // clad<->grease optical boundary, as in the real greased clamp. It must
        // NOT reuse fChannelRadius (rFibre + 0.05 mm): that larger cutter reaches
        // exactly the film's slab-side face (tangency -> degenerate boolean) and
        // leaves a 50 um air annulus, so the fibre never couples to the grease.
        //
        // The path is in slab-local coordinates; the film box is built centred
        // on its own origin and then placed at (filmCx, 0, 0), so the tool x is
        // shifted by -filmCx and NO rotation is needed (the path already runs
        // along +y with the transverse offset in z).
        G4VSolid* solid = filmBox;
        for (G4int k = 0; k < fNFibres; ++k) {
            const auto tool = BentPath(fFibreAxisX - filmCx, zSpread[k],
                                       zBundle[k], halfL, conv, resL, cutterOv);
            auto* groove = MakeSweptTube("WLSCoupleGroove_" + std::to_string(k),
                                         rFibre, tool, ringSeg);
            solid = new G4SubtractionSolid("WLSCoupleCut_" + std::to_string(k),
                                           solid, groove, nullptr,
                                           G4ThreeVector(0., 0., 0.));
        }
        fGrease4LV = new G4LogicalVolume(solid, greaseMat, "WLSCoupleFilmLV");
    }
    fGrease4LV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.85, 0.2, 0.6)));

    // ---------- Black clamp body with milled fibre channels ----------
    // The clamp carries one full-length channel per fibre, each of radius
    // fChannelRadius, cut with the SAME bent-tube tool as the fibre so the
    // channel follows the fibre through the straight run, the ramp and the
    // bundle. Channels are separated by fFibrePitch > 2*fChannelRadius in the
    // straight run so a solid web of clamp material remains between neighbours
    // (as in Rao et al. Fig. 4a). Over the last stretch the fibres converge to a
    // touching bundle, so the channels there merge into one slot -- which is
    // exactly the real holder: a narrow groove for the bundle feeding the SiPM.
    //
    // The channel is a hair wider than the fibre (Ø1.1 vs Ø1.0), leaving a thin
    // air annulus. The real Y-11(200) is multi-clad (outer fluoropolymer
    // n~1.42); our simplified single clad is n=1.49, so if it touched the
    // n=1.55 black absorber the low-index gap sustaining TIR would vanish. The
    // paper only specifies grease in the plate<->clamp gap, not around the
    // fibres, so the clearance is consistent with it.
    const G4double clampHalfX = 0.5 * fClampWallZ;
    const G4double clampCxAbs = clampFrontX + clampHalfX;   // slab-local |x|
    const G4double channelCxAbs = fibreAxisXAbs;            // coaxial with fibre
    // Channel centre relative to the clamp centre (local frame, no rotation).
    const G4double relChX = sx * (channelCxAbs - clampCxAbs);
    auto* clampBox = new G4Box("WLSClampBox",
                               clampHalfX, fHalfSize.y(), fHalfSize.z());
    G4VSolid* clampSolid = clampBox;
    {
        // Same swept tools as the fibres, in slab-local coordinates, shifted by
        // -clampCx (the box is placed at (clampCx,0,0)), so NO rotation. The
        // straight-run channel web is preserved by fFibrePitch > 2*fChannelRadius;
        // the channels merge into the bundle slot over the converging stretch.
        for (G4int k = 0; k < fNFibres; ++k) {
            const auto tool = BentPath(relChX, zSpread[k],
                                       zBundle[k], halfL, conv, resL, cutterOv);
            auto* channel = MakeSweptTube("WLSChannel_" + std::to_string(k),
                                          fChannelRadius, tool, ringSeg);
            clampSolid = new G4SubtractionSolid("WLSClampSolid_" + std::to_string(k),
                                                clampSolid, channel, nullptr,
                                                G4ThreeVector(0., 0., 0.));
        }
    }
    G4Material* clampMat = G4Material::GetMaterial("WLSClampBlack", false);
    if (!clampMat) {
        clampMat = new G4Material("WLSClampBlack", 1.05*g/cm3, 2);
        clampMat->AddElement(nist->FindOrBuildElement("C"), 1);
        clampMat->AddElement(nist->FindOrBuildElement("H"), 1);

        const G4int Nb = 2;
        G4double eb[Nb]   = { 1.5*eV, 6.0*eV };
        G4double nb[Nb]   = { 1.55, 1.55 };
        G4double absb[Nb] = { 1.0*um, 1.0*um };
        auto* mptB = new G4MaterialPropertiesTable();
        mptB->AddProperty("RINDEX",    eb, nb,   Nb);
        mptB->AddProperty("ABSLENGTH", eb, absb, Nb);
        clampMat->SetMaterialPropertiesTable(mptB);
    }
    fClampMat = clampMat;
    fClampLV = new G4LogicalVolume(clampSolid, clampMat, "WLSClampLV");
    fClampLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.15, 0.15, 0.15, 0.6)));

    // Clamp outboard surface = specular reflector (the reflective layer on the
    // clamp side facing away from the SiPM).
    fClampMirror = new G4OpticalSurface("MuonScintClampMirror");
    fClampMirror->SetModel(unified);
    fClampMirror->SetType(dielectric_metal);
    fClampMirror->SetFinish(polished);
    {
        const G4int Nm = 2;
        G4double em[Nm]    = { 1.5*eV, 6.0*eV };
        G4double reflm[Nm] = { 0.95, 0.95 };
        G4double effm[Nm]  = { 0.0, 0.0 };
        auto* mptM = new G4MaterialPropertiesTable();
        mptM->AddProperty("REFLECTIVITY", em, reflm, Nm);
        mptM->AddProperty("EFFICIENCY",   em, effm,  Nm);
        fClampMirror->SetMaterialPropertiesTable(mptM);
    }

    // ---------- Far-end mirror cap (full disc at the fibre's far end) --------
    // A complete Ø2*rFibre Al disc closes the fibre end. (The old half-disc only
    // covered half the face; full discs sit at the SPREAD pitch fFibrePitch >
    // 2*rFibre so neighbouring caps do not overlap.)
    const G4double capHalf = 0.05 * mm;
    auto* capSolid = new G4Tubs("WLSFibreMirrorCap", 0., rFibre, capHalf,
                                0., 360.*deg);
    G4Material* capMat = nist->FindOrBuildMaterial("G4_Al");
    fMirrorCapLV = new G4LogicalVolume(capSolid, capMat, "WLSFibreMirrorCapLV");
    fMirrorCapLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.9, 0.9, 0.9, 0.9)));

    fFibreEndMirror = new G4OpticalSurface("MuonScintFibreEndMirror");
    fFibreEndMirror->SetModel(unified);
    fFibreEndMirror->SetType(dielectric_metal);
    fFibreEndMirror->SetFinish(polished);
    {
        const G4int Ne = 2;
        G4double ee[Ne]   = { 1.5*eV, 6.0*eV };
        G4double refl[Ne] = { 0.90, 0.90 };   // Al end mirror
        G4double eff[Ne]  = { 0.0, 0.0 };
        auto* mptE = new G4MaterialPropertiesTable();
        mptE->AddProperty("REFLECTIVITY", ee, refl, Ne);
        mptE->AddProperty("EFFICIENCY",   ee, eff,  Ne);
        fFibreEndMirror->SetMaterialPropertiesTable(mptE);
    }

    sHasWLSFibres = true;

    G4cout << " [MuonScint] Built WLS fibre clamp (Config 4): "
           << fNFibres << " x Ø" << 2.0*rFibre/mm << " mm Y-11 fibres, "
           << "core r=" << rCore/mm << " mm, clad r=" << rFibre/mm << " mm, "
           << "z-spacing=" << zSpacing/mm << " mm" << G4endl;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Config 4 fibre-clamp placement. All pieces are siblings of the slab in the
// world. A thin grease film carries the slab<->grease Fresnel boundary; the
// fibres run inside channels milled into the clamp and poke slightly out of the
// channel mouths into the film, so their core/clad TIR (and long-axis
// attenuation) is preserved. The clamp body is a light absorber, and each
// fibre's far (+y after the placement flip) end is capped by a specular Al
// mirror; the read-out (converged bundle + SiPM) is at -y.
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void GeometryMuonScint::PlaceWLSFibres(G4LogicalVolume* worldLV,
                                       const G4ThreeVector& slabPos,
                                       G4RotationMatrix* slabRot,
                                       G4int /*copyNo*/)
{
    const G4double rFibre = fFibreRadius;
    const G4double sx      = fWLSOnNegX ? -1.0 : +1.0;
    const G4double clampFrontX = fHalfSize.x() + fClampGap;
    const G4double filmHalfX = 0.5 * fClampGap;
    const G4double filmCx  = sx * (fHalfSize.x() + filmHalfX);
    const G4double clampCx = sx * (clampFrontX + 0.5*fClampWallZ);
    const G4double Ly = fHalfSize.y();

    auto mapPos = [&](const G4ThreeVector& local) {
        G4ThreeVector w = local;
        if (slabRot) w = (*slabRot) * w;
        return w + slabPos;
    };
    auto rotCopy = [&](const G4RotationMatrix& base) {
        auto* r = new G4RotationMatrix(base);
        if (slabRot) *r = (*slabRot) * (*r);
        return r;
    };
    // The WLS sub-assembly is built with the fibre bundle at slab-local +y and
    // the mirror at -y, then the whole assembly (film, clamp, fibres, caps,
    // read-out pad) is rotated 180 deg about the slab-local x axis here.  This
    // puts the clamp along the report-plan TOP edge (the -x narrow face) with
    // the read-out SiPM at its LEFT (-x) end -- the top-left corner of the
    // top-down view, matching Fig. 1 / Fig. 3(b) of Rao et al.  Rx180 is a
    // proper rotation, so the tessellated fibres and the milled clamp/grease
    // channels keep their outward facet orientation and flip together; the
    // clamp and grease boxes are symmetric about that axis.  The fibre / clamp
    // / channel CONSTRUCTION (BuildWLSFibres) is untouched.
    G4RotationMatrix flipRot;
    flipRot.rotateX(180.*deg);

    // ---- Thin grease film (grooves flip with the assembly) ----
    auto* filmPV = new G4PVPlacement(rotCopy(flipRot),
                                     mapPos(G4ThreeVector(filmCx, 0., 0.)),
                                     fGrease4LV, "WLSCoupleFilmPhys",
                                     worldLV, false, 0, true);

    // ---- Black clamp body (outboard of the fibres) ----
    new G4PVPlacement(rotCopy(flipRot),
                      mapPos(G4ThreeVector(clampCx, 0., 0.)),
                      fClampLV, "WLSClampPhys", worldLV, false, 0, true);
    if (fClampMirror) {
        new G4LogicalSkinSurface("WLSClampMirrorSkin", fClampLV, fClampMirror);
    }

    // ---- Fibres (clad + core), seated in the clamp channels --------------
    // The fibres are world siblings of the slab (like the mirror caps and the
    // read-out), running the full plate length in y and spread across z. Each
    // sits inside a milled channel and pokes fFibreProtrusion past the channel
    // mouth (the clamp front face) into the grease film; the clamp and grease
    // solids were carved with the same swept-tube tools so the three never
    // overlap. Placing the fibres in the world (rather than as daughters of the
    // clamp) is what lets them protrude out of the clamp without tripping
    // Geant4's mother-containment overlap check.
    //
    // Each fibre solid (MakeSweptTube) is built DIRECTLY in slab-local
    // coordinates (axis +y, transverse offset in z, centred on y=0); it is
    // placed at the slab-local origin and carries the SAME flipRot as the
    // clamp/film so its bundle moves to -y along with the milled channels. The
    // core is a daughter of each clad LV (built above), so it is not placed
    // here.
    std::vector<G4VPhysicalVolume*> cladPVs(fNFibres, nullptr);
    for (G4int k = 0; k < fNFibres; ++k) {
        cladPVs[k] = new G4PVPlacement(rotCopy(flipRot),
                                       mapPos(G4ThreeVector(0., 0., 0.)),
                                       fFibreCladLVs[k],
                                       "WLSFibreCladPhys_" + std::to_string(k),
                                       worldLV, false, k, true);
    }

    // ---- Far-end mirror caps (world siblings just past the +y fibre ends
    // after the flip). The cap disc's axis is put along y (rotateX(+90) maps
    // tube local z -> y); a full disc is rotationally symmetric so Phi is
    // immaterial. zk is mirrored so cap k pairs with the flipped fibre k.
    const G4double capPhi = fWLSOnNegX ? 180.*deg : 0.;
    G4RotationMatrix capRot;
    capRot.rotateX(90.*deg);
    std::vector<G4VPhysicalVolume*> capPVs(fNFibres, nullptr);
    for (G4int k = 0; k < fNFibres; ++k) {
        const G4double zk = -(k - 0.5*(fNFibres - 1)) * fFibreZSpacing;
        const G4ThreeVector capCentre(fFibreAxisX, Ly + 0.05*mm, zk);
        capPVs[k] = new G4PVPlacement(rotCopy(capRot), mapPos(capCentre),
                                      fMirrorCapLV,
                                      "WLSFibreMirrorPhys_" + std::to_string(k),
                                      worldLV, false, k, true);
    }

    // ---- Read-out grease pad + single SiPM at the -y fibre ends (after the
    // flip). The pad/SiPM boxes are large in their local (x,y); after the
    // axisRot (tube axis -> y) we additionally rotate about the axis by Phi so
    // their wide face aligns with the slab face (x-z), not the slab edge. The
    // assembly flip is composed on top so the wide face still faces the slab.
    const G4double padCentreY  = -(Ly + fCouplingHalfZ);
    const G4double sipmCentreY = -(Ly + 2.*fCouplingHalfZ + fSiPMHalfZ);
    G4RotationMatrix padRot;
    padRot.rotateX(90.*deg);
    padRot.rotateY(capPhi);
    const G4RotationMatrix padRotFlip = flipRot * padRot;
    auto* padPV  = new G4PVPlacement(rotCopy(padRotFlip),
                                     mapPos(G4ThreeVector(fFibreAxisX, padCentreY, 0.)),
                                     fGreaseLV, "WLSReadoutGrease",
                                     worldLV, false, 0, true);
    auto* sipmPV = new G4PVPlacement(rotCopy(padRotFlip),
                                     mapPos(G4ThreeVector(fFibreAxisX, sipmCentreY, 0.)),
                                     fSiPMLV, "WLSReadoutSiPMPhys",
                                     worldLV, false, 0, true);

    // ---- Optical borders ----
    // slab instrumented face <-> grease film: transparent Fresnel (overrides
    // the wrap).
    if (fScintPV && fCouplingSurface) {
        new G4LogicalBorderSurface("MuonScint_SlabFilm", fScintPV, filmPV,
                                   fCouplingSurface);
        new G4LogicalBorderSurface("MuonScint_FilmSlab", filmPV, fScintPV,
                                   fCouplingSurface);
    }
    // fibre <-> far-end mirror cap: specular Al reflector.
    if (fFibreEndMirror) {
        for (G4int k = 0; k < fNFibres; ++k) {
            new G4LogicalBorderSurface("MuonScint_FibreCap_" + std::to_string(k),
                                       cladPVs[k], capPVs[k], fFibreEndMirror);
            new G4LogicalBorderSurface("MuonScint_CapFibre_" + std::to_string(k),
                                       capPVs[k], cladPVs[k], fFibreEndMirror);
        }
    }
    // fibre read-out end (-y after the placement flip) -> read-out pad:
    // transparent (couples guided light out).
    // The single SiPM views ALL FOUR fibre end faces simultaneously (Rao et al.
    // Fig. 4a), so every fibre's read-out end must carry the coupling border into the
    // shared read-out pad. Without this the three un-listed fibres have no
    // surface at their end face, so the default lossless-TIR rule traps their
    // light instead of delivering it to the SiPM.
    if (fCouplingSurface && padPV) {
        for (std::size_t k = 0; k < cladPVs.size(); ++k) {
            new G4LogicalBorderSurface("MuonScint_FibrePad_" + std::to_string(k),
                                       cladPVs[k], padPV, fCouplingSurface);
            new G4LogicalBorderSurface("MuonScint_PadFibre_" + std::to_string(k),
                                       padPV, cladPVs[k], fCouplingSurface);
        }
    }
    // pad -> SiPM: photocathode (PDE) detection.
    if (fSiPMSurface) {
        new G4LogicalBorderSurface("MuonScint_ReadoutGreaseSiPM", padPV, sipmPV,
                                   fSiPMSurface);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

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
    if (fWrapWithReflector && fReflectorType != kNoReflector) {
        new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
    }

    // Bare configuration: the polished plate is clamped into the black ABS
    // holder. The holder index (~1.55) is close to the scintillator (1.58)
    // and the fit is tight, so the plate faces are in effective optical
    // contact: total internal reflection is frustrated and trapped light
    // leaks into the black wall, where it is absorbed. This is why the bare
    // plate is the weakest light collector even though it is "all-sides
    // polished" (a free-standing polished plate would instead be a near
    // lossless TIR guide). The absorbing skin on the slab models that
    // contact / frustrated-TIR loss. SiPM coupling border surfaces override
    // it at their footprints.
    if (UsesAbsorberSkin() && fHolderSurface) {
        new G4LogicalSkinSurface("MuonScintBareAbsorber", fScintLV, fHolderSurface);
    }

    // Black holder: bare configuration only. The shell is the structural
    // 3D-printed enclosure; its cavity is drawn slightly larger than the slab
    // for visualization. The optical loss at the plate faces is already
    // handled by the absorber skin above (effective contact), so the drawn
    // gap is not optically traversed by this model. The holder's own
    // absorbing skin is kept as a safety net for any photon that does cross
    // the gap.
    if (fHolderLV) {
        new G4PVPlacement(rot, pos, fHolderLV,
                          "MuonScintHolderPhys",
                          worldLV, false, copyNo, true);
        if (fHolderSurface) {
            new G4LogicalSkinSurface("MuonScintHolderSkin",
                                     fHolderLV, fHolderSurface);
        }
    }

    // Place grease pads + SiPMs as siblings of the slab in the world, or the
    // Config 4 WLS fibre clamp.
    if (fUseWLSFibres) {
        PlaceWLSFibres(worldLV, pos, rot, copyNo);
    } else {
        PlaceSiPMs(worldLV, pos, rot, copyNo);
    }
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
    // Config 1: two SiPMs per side, equidistantly mounted along the 250 mm
    // side (i.e. 62.5 mm and 187.5 mm -> u = -0.5, +0.5).
    for (G4int e = 0; e < 4; ++e) {
        AddSiPM({e, -0.5, 0.0});
        AddSiPM({e, +0.5, 0.0});
    }
}

void GeometryMuonScint::PresetConfig_4SiPM_OnePerSide() {
    ClearSiPMs();
    // Config 2: four SiPMs, one per side, at the SAME along-side positions as
    // Config 1 (i.e. Config 1 with one SiPM removed per side). The removed one
    // is the same relative slot on every side (top-left in a top view, rotated
    // by 90 deg to the other three sides) -> keep u=-0.5 on +X/-Y, u=+0.5 on
    // -X/+Y for a C4-symmetric layout.
    AddSiPM({0, -0.5, 0.0});   // +X
    AddSiPM({1, +0.5, 0.0});   // -X
    AddSiPM({2, +0.5, 0.0});   // +Y
    AddSiPM({3, -0.5, 0.0});   // -Y
}

void GeometryMuonScint::PresetConfig_2SiPM_OneSide() {
    ClearSiPMs();
    // Config 3: two SiPMs, both on the -X side (u = -0.5, +0.5). The single
    // instrumented side is opposite the +X edge / +X+Y corner MCD regions.
    AddSiPM({1, -0.5, 0.0});
    AddSiPM({1, +0.5, 0.0});
}

void GeometryMuonScint::PresetConfig_4SiPM_Corners() {
    ClearSiPMs();
    AddSiPM({0, -1.0, 0.0});
    AddSiPM({0, +1.0, 0.0});
    AddSiPM({1, -1.0, 0.0});
    AddSiPM({1, +1.0, 0.0});
}

void GeometryMuonScint::PresetConfig_WLSFibreClamp() {
    // Config 4 (Rao et al. 2025): a single 4x4 mm SiPM at the +Y end of the
    // WLS fibre bundle on the -X narrow face (opposite the +X edge / +X+Y
    // corner MCD regions; set fWLSOnNegX to move it). The read-out channel
    // occupies slot 0 so the SD enumerates exactly one SiPM. The fibres/clamp
    // are built in Build() once fUseWLSFibres is set.
    ClearSiPMs();
    SetUseWLSFibres(true);
    fWLSOnNegX = true;
    AddSiPM({0, 0.0, 0.0});   // read-out SiPM; placement is overridden in PlaceWLSFibres
}







