#include "GeometryMuonScint.hh"

#include "G4Box.hh"
#include "G4Tubs.hh"
#include "G4CutTubs.hh"
#include "G4Transform3D.hh"
#include "G4TessellatedSolid.hh"
#include "G4TriangularFacet.hh"
#include "G4ExtrudedSolid.hh"
#include "G4TwoVector.hh"
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
#include <cstdlib>
#include <string>

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......
// Loss-study knobs (investigation: "can realistic losses make the bare plate
// the lowest collector?"). Set kStudyBulkAbsScale < 1 to shorten the
// scintillator bulk attenuation length; set kStudyBareRoughness > 0 to add a
// ground (sigma_alpha) surface on the bare plate faces. Both default to the
// physical baseline (unscaled attenuation, no face scattering).
namespace {
const G4double kStudyBulkAbsScale  = 1.0;   // multiplies scintillator ABSLENGTH
const G4double kStudyBareRoughness = 0.0;   // bare-face sigma_alpha (unified)
}

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
    // Loss study: optionally shorten the bulk attenuation length.
    for (G4int i = 0; i < N; ++i) absL[i] *= kStudyBulkAbsScale;

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
      // 0) BARE: no reflector layer. The polished plate sits in the black
      //    ABS enclosure with a real air gap (fHolderGap), so its faces are
      //    a plain polished scintillator (n=1.58) <-> air (n=1.0) dielectric
      //    boundary. With no surface object G4OpBoundaryProcess applies
      //    Fresnel + TIR from the material RINDEX (glisur/polished
      //    dielectric_dielectric default, G4OpBoundaryProcess.cc:312-314):
      //    a near-lossless guide, with the light that refracts past the
      //    critical angle crossing the gap and being absorbed by the black
      //    wall. kStudyBareRoughness > 0 attaches a ground surface with that
      //    sigma_alpha to model scattering at the plate faces (loss study).
      //    The SiPM coupling border surfaces override at their footprints.
      // ============================================================
      case kNoReflector: {
        if (UsesBlackHolder() && fBareContactFraction > 0.) {
          // Mean-field plate/holder contact: with probability f a surface hit
          // meets the black holder (index-matched, absorbing); with
          // probability 1-f it crosses the real air gap and follows the normal
          // polished scintillator <-> air Fresnel/TIR path.  G4OpBoundaryProcess
          // treats REFLECTIVITY + TRANSMITTANCE as a probability budget, so
          // R = 1-f, T = 0 gives exactly that split.  No surface RINDEX: the
          // non-absorbed branch must read the G4_AIR RINDEX.
          fReflSurface->SetType(dielectric_dielectric);
          fReflSurface->SetModel(glisur);
          fReflSurface->SetFinish(polished);
          fReflSurface->SetSigmaAlpha(0.0);

          auto* mpt = new G4MaterialPropertiesTable();
          G4double R[N], T[N];
          for (G4int i = 0; i < N; ++i) {
            R[i] = 1.0 - fBareContactFraction;
            T[i] = 0.0;
          }
          mpt->AddProperty("REFLECTIVITY",  e, R, N);
          mpt->AddProperty("TRANSMITTANCE", e, T, N);
          fReflSurface->SetMaterialPropertiesTable(mpt);
        }
        else if (kStudyBareRoughness > 0.) {
          fReflSurface->SetType(dielectric_dielectric);
          fReflSurface->SetModel(unified);   // sigma_alpha is only honoured by the unified model
          fReflSurface->SetFinish(ground);
          fReflSurface->SetSigmaAlpha(kStudyBareRoughness);
          auto* mpt = new G4MaterialPropertiesTable();
          G4double sl[N] = { 1.,1.,1.,1.,1., 1.,1.,1.,1.,1. };  // lobe -> sigma_alpha used
          G4double sp[N] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };  // no spike (spike resets facet normal)
          mpt->AddProperty("SPECULARLOBECONSTANT",  e, sl, N);
          mpt->AddProperty("SPECULARSPIKECONSTANT", e, sp, N);
          fReflSurface->SetMaterialPropertiesTable(mpt);
        }
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
      // 2) ALUMINIUM FOIL, shiny side, wrapped around the plate.
      //    Rao et al. (J. Appl. Phys. 138, 114504, 2025, Sec. II B) wrap the
      //    plate in commercial foil and apply optical grease selectively so
      //    the foil contacts the scintillator; it is NOT an air gap.
      //    Modelled as "polishedbackpainted" with the backing index set to
      //    the coupling grease (n = 1.46, same as fCouplingSurface): TIR is
      //    then confined to the grazing 67.5 deg cone (asin(1.46/1.58)) and
      //    most light refracts into the grease and reflects SPECULARLY off
      //    the smooth Al (SPECULARSPIKE = 1).
      //    REFLECTIVITY = normal-incidence reflectance of evaporated Al
      //    computed from the McPeak et al. (2015) n,k data
      //    (refractiveindex.info, CC0); this is an upper bound for a real,
      //    slightly rough/oxidised household foil.
      // ============================================================
      case kAluminumFoil: {
        fReflSurface->SetType(dielectric_dielectric);
        fReflSurface->SetFinish(polishedbackpainted);
        fReflSurface->SetSigmaAlpha(0.0);

        auto* mpt = new G4MaterialPropertiesTable();
        G4double R[N]       = { 0.904, 0.911, 0.916, 0.919, 0.921,
                                0.923, 0.924, 0.924, 0.925, 0.925 };
        G4double nBacking[N];
        const G4double nAlBacking =
            (fAlBackingMode == kAlBackingGrease) ? 1.46 : 1.0003;
        for (G4int i = 0; i < N; ++i) nBacking[i] = nAlBacking;
        G4double specSp[N]  = { 1.,1.,1.,1.,1., 1.,1.,1.,1.,1. };  // pure mirror
        G4double specLo[N]  = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        G4double backSc[N]  = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0. };
        mpt->AddProperty("REFLECTIVITY",          e, R,        N);
        mpt->AddProperty("RINDEX",                e, nBacking, N);
        mpt->AddProperty("SPECULARSPIKECONSTANT", e, specSp,  N);
        mpt->AddProperty("SPECULARLOBECONSTANT",  e, specLo,  N);
        mpt->AddProperty("BACKSCATTERCONSTANT",   e, backSc,  N);
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

    const G4int Np = 25;
    // ASD-NUV4S-P-40 PDE(lambda), digitized from the AdvanSiD datasheet
    // (report/refs/AdvanSiD_NUV.pdf, Fig. 8; 6 V overvoltage, 20 C), ascending
    // photon energy. Peak ~43% near 410-420 nm; ~30% at 490 nm (Rao's
    // "~30% for converted photons"); falls to 8.6% at 750 nm. Since this drives
    // the detection of WLS-shifted (green) light it lowers Config-4 yields.
    G4double ep[Np]  = { 1.653*eV, 1.771*eV, 1.907*eV, 2.066*eV, 2.156*eV,
                         2.254*eV, 2.362*eV, 2.431*eV, 2.480*eV, 2.530*eV,
                         2.583*eV, 2.638*eV, 2.695*eV, 2.755*eV, 2.818*eV,
                         2.883*eV, 2.952*eV, 3.024*eV, 3.100*eV, 3.139*eV,
                         3.179*eV, 3.220*eV, 3.263*eV, 3.306*eV, 3.443*eV };
    G4double pde[Np] = { 0.086, 0.110, 0.146, 0.178, 0.197,
                         0.224, 0.247, 0.266, 0.284, 0.300,
                         0.317, 0.330, 0.343, 0.360, 0.385,
                         0.408, 0.426, 0.431, 0.418, 0.380,
                         0.345, 0.280, 0.210, 0.150, 0.080 };
    G4double rZero[Np] = { 0.,0.,0.,0.,0., 0.,0.,0.,0.,0.,
                           0.,0.,0.,0.,0., 0.,0.,0.,0.,0.,
                           0.,0.,0.,0.,0. };
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

// Cross-section generators for MakeSweptPrism. A section is a list of 2-D
// points (x -> along u=(1,0,0), y -> along the transverse axis v), listed in
// the same (counter-clockwise) order so the side-wall winding is outward.
static std::vector<G4ThreeVector> CircleSection(G4double r, G4int nSeg)
{
    std::vector<G4ThreeVector> s;
    s.reserve(nSeg);
    for (G4int k = 0; k < nSeg; ++k) {
        const G4double th = (2.*3.14159265358979323846*k)/nSeg;
        s.emplace_back(r*std::cos(th), r*std::sin(th), 0.);
    }
    return s;
}

// Axis-aligned square of half-width hw. Adjacent side facets meet at a 90 deg
// dihedral; the 24-gon circle's shallow ~165 deg edges are far more prone to
// G4Navigator "stuck track" ambiguity, and a square also makes the
// clamp/film booleans purely planar (box minus box).
static std::vector<G4ThreeVector> SquareSection(G4double hw)
{
    return { G4ThreeVector(+hw, +hw, 0.), G4ThreeVector(-hw, +hw, 0.),
             G4ThreeVector(-hw, -hw, 0.), G4ThreeVector(+hw, -hw, 0.) };
}

// Build a swept (generalized) prism along a piecewise-linear centreline, with
// an arbitrary convex cross-section, as a G4TessellatedSolid.
//
// WHY TESSELLATED: Geant4 has no analytic bent-tube solid, and a union of
// tilted G4Tubs/G4Boxes leaves a wedge-shaped AIR GAP on the outside of every
// kink (a plain union is not watertight there), while adding joint spheres
// makes the boolean mesher give up (10000-point cloud -> 18 MB VRML). A
// tessellated generalized prism is a single watertight surface with no
// boolean, no gap, and a tiny mesh.
//
// The rings at each path node are oriented on the MITERED tangent (the
// bisector of the incoming/outgoing segments), so consecutive segments share
// one tilted ring: the surface closes exactly with no gap and no overlap (this
// is the standard swept-polyline tube). The centreline stays in a plane of
// constant x, so a single transverse axis u=(1,0,0) is perpendicular to every
// ring normal and the rings never twist.
static G4VSolid* MakeSweptPrism(const G4String& name,
                                const std::vector<G4ThreeVector>& section,
                                const std::vector<G4ThreeVector>& path)
{
    const std::size_t n = path.size();
    const std::size_t m = section.size();
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
                                                 std::vector<G4ThreeVector>(m));
    for (std::size_t i = 0; i < n; ++i) {
        const G4ThreeVector v = nrm[i].cross(u).unit();
        for (std::size_t k = 0; k < m; ++k) {
            ring[i][k] = path[i] + section[k].x()*u + section[k].y()*v;
        }
    }

    // Side wall: two triangles per (segment, azimuth) with outward orientation.
    for (std::size_t i = 0; i + 1 < n; ++i) {
        for (std::size_t k = 0; k < m; ++k) {
            const std::size_t k2 = (k + 1) % m;
            solid->AddFacet(new G4TriangularFacet(ring[i][k], ring[i][k2],
                                                  ring[i+1][k2], ABSOLUTE));
            solid->AddFacet(new G4TriangularFacet(ring[i][k], ring[i+1][k2],
                                                  ring[i+1][k], ABSOLUTE));
        }
    }
    // Flat end caps (triangle fans); the -y cap winds the opposite way so its
    // normal also points outward.
    for (std::size_t k = 0; k < m; ++k) {
        const std::size_t k2 = (k + 1) % m;
        solid->AddFacet(new G4TriangularFacet(path[0], ring[0][k2], ring[0][k],
                                              ABSOLUTE));
        solid->AddFacet(new G4TriangularFacet(path[n-1], ring[n-1][k],
                                              ring[n-1][k2], ABSOLUTE));
    }
    solid->SetSolidClosed(true);
    return solid;
}

// Circular swept tube of radius r (kept for the round-fibre Config 4).
static G4VSolid* MakeSweptTube(const G4String& name, G4double r,
                               const std::vector<G4ThreeVector>& path,
                               G4int nSeg)
{
    return MakeSweptPrism(name, CircleSection(r, nSeg), path);
}

// Square swept prism of half-width hw (Config 4 "box" cross-section).
static G4VSolid* MakeSweptSquare(const G4String& name, G4double hw,
                                 const std::vector<G4ThreeVector>& path)
{
    return MakeSweptPrism(name, SquareSection(hw), path);
}

// 2-D outline (in a local x-y plane holding the path's (Y,Z) coordinates) of the
// square cross-section swept along a planar polyline with mitered joins. The
// centreline lies in a plane of constant slab-x, so the swept solid is exactly
// the Cartesian product of an x-interval with this 2-D region: the polygon is
// the +hw offset chain followed by the -hw offset chain, using the same
// mitered bisector normal as MakeSweptPrism (v = nrm x u).
static std::vector<G4TwoVector> SweptSquareRegion2D(
        const std::vector<G4ThreeVector>& path, G4double hw)
{
    const std::size_t n = path.size();
    std::vector<G4TwoVector> P(n), N(n);
    for (std::size_t i = 0; i < n; ++i) P[i] = G4TwoVector(path[i].y(), path[i].z());
    for (std::size_t i = 0; i < n; ++i) {
        G4TwoVector tIn, tOut;
        if (i > 0)     tIn  = (P[i] - P[i-1]).unit();
        if (i + 1 < n) tOut = (P[i+1] - P[i]).unit();
        const G4TwoVector t =
            (i == 0) ? tOut : (i + 1 == n) ? tIn : (tIn + tOut).unit();
        N[i] = G4TwoVector(t.y(), -t.x());   // matches MakeSweptPrism's v
    }
    std::vector<G4TwoVector> poly;
    poly.reserve(2*n);
    for (std::size_t i = 0; i < n; ++i) poly.push_back(P[i] + hw*N[i]);
    for (std::size_t i = n; i-- > 0; )  poly.push_back(P[i] - hw*N[i]);
    return poly;
}

// Analytic equivalent of MakeSweptSquare: an extrusion of the swept (Y,Z)
// region along the fibre axis. G4ExtrudedSolid supplies exact surface normals
// and a prism-aware navigation (exact out-distance for a convex prism), so it
// avoids the near-tangent G4TessellatedSolid "stuck track" pathology that the
// 24-gon (round) and generic-tessellated (box) fibres both exhibit.
static G4VSolid* MakeExtrudedFibre(const G4String& name,
        const std::vector<G4ThreeVector>& path, G4double hw)
{
    return new G4ExtrudedSolid(name, SweptSquareRegion2D(path, hw), hw);
}

// ANALYTIC alternative to MakeSweptTube: one G4CutTubs per straight leg of a
// planar polyline (constant slab-x, as produced by BentPath), joined by exact
// miter planes. At an interior node both neighbouring legs are cut by the SAME
// bisector plane, so their elliptical end faces coincide exactly (the defining
// property of a miter joint) and the union is watertight with no facet edges.
// End nodes are cut perpendicular to the leg. Returns, per leg, the solid in
// its own frame (local z = leg direction, local x = slab x) and its object
// transform (rotation local->slab and centre) in the slab-local frame.
struct CutTubeLeg {
    G4VSolid*        solid = nullptr;
    G4RotationMatrix rot;
    G4ThreeVector    pos;
};
static std::vector<CutTubeLeg> MakeCutTubeLegs(const G4String& name,
                                               G4double rmax,
                                               const std::vector<G4ThreeVector>& path)
{
    const std::size_t n = path.size();
    std::vector<G4ThreeVector> dir(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) dir[i] = (path[i+1] - path[i]).unit();

    std::vector<CutTubeLeg> legs;
    legs.reserve(n - 1);
    const G4ThreeVector ex(1., 0., 0.);   // every leg is perpendicular to slab x
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const G4ThreeVector ez = dir[i];
        const G4ThreeVector ey = ez.cross(ex);
        G4RotationMatrix R;
        R.set(ex, ey, ez);                // columns = images of local x,y,z
        const G4RotationMatrix Rinv = R.inverse();

        // Outward cut normals in the slab frame: perpendicular at the polyline
        // ends, the (negated, at the low end) miter bisector at interior nodes.
        const G4ThreeVector mLo = (i == 0)     ? ez : (dir[i-1] + dir[i]).unit();
        const G4ThreeVector mHi = (i + 2 == n) ? ez : (dir[i] + dir[i+1]).unit();
        const G4ThreeVector lowN  = Rinv * (-mLo);
        const G4ThreeVector highN = Rinv * ( mHi);

        const G4double halfL = 0.5 * (path[i+1] - path[i]).mag();
        CutTubeLeg leg;
        leg.solid = new G4CutTubs(name + "_leg" + std::to_string(i),
                                  0., rmax, halfL, 0., CLHEP::twopi,
                                  lowN, highN);
        leg.rot = R;
        leg.pos = 0.5 * (path[i] + path[i+1]);
        legs.push_back(leg);
    }
    return legs;
}

// Union of the analytic legs of one cutter, expressed in the frame of the
// object being carved (path already shifted into that frame).
static G4VSolid* MakeCutTubeCutter(const G4String& name, G4double rmax,
                                   const std::vector<G4ThreeVector>& path)
{
    auto legs = MakeCutTubeLegs(name, rmax, path);
    G4VSolid* u = new G4DisplacedSolid(name + "_d0", legs[0].solid,
                                       G4Transform3D(legs[0].rot, legs[0].pos));
    for (std::size_t i = 1; i < legs.size(); ++i) {
        u = new G4UnionSolid(name + "_u" + std::to_string(i), u, legs[i].solid,
                             G4Transform3D(legs[i].rot, legs[i].pos));
    }
    return u;
}

// Rotation mapping the extruded solid's local frame (poly-x=Y, poly-y=Z,
// extrusion=local z) onto slab-local axes slab = (local z, local x, local y).
// Set explicitly by columns to avoid any active/passive-rotation ambiguity.
static G4RotationMatrix ExtrudedFibreRot()
{
    G4RotationMatrix m;
    m.set(G4ThreeVector(0., 1., 0.),    // image of local x -> slab y
          G4ThreeVector(0., 0., 1.),    // image of local y -> slab z
          G4ThreeVector(1., 0., 0.));   // image of local z (extrusion) -> slab x
    return m;
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
        // Non-WLS bulk loss (Rayleigh/impurities): flat and long so it does not
        // compete with the WLS absorption of the blue scintillation light.
        G4double absBulk[N] = {
            5.0*m, 5.0*m, 5.0*m, 5.0*m, 5.0*m, 5.0*m, 5.0*m,
            5.0*m, 5.0*m, 5.0*m, 5.0*m, 5.0*m, 5.0*m };
        auto* mptW = new G4MaterialPropertiesTable();
        mptW->AddProperty("RINDEX",       e, rindexW, N);
        mptW->AddProperty("ABSLENGTH",    e, absBulk, N);   // non-WLS loss

        // WLS (Y-11) absorption AND emission spectra, digitized from
        // report/refs/KurarayPSF.pdf (catalogue p. 9, "Absorption and Emission
        // Spectra", thickest curve = Y-11), each normalised to 1 at its peak
        // (absorption 430 nm, emission 476 nm). The catalogue gives only the
        // SHAPE; fWLSAbsLength430 is the absolute absorption length at the 430 nm
        // peak (default 1.0 mm; 0.3-3 mm is the systematic band). Tables are in
        // wavelength-descending order (= energy ascending).
        //
        // G4OpWLS samples the secondary from WLSCOMPONENT and requires its energy
        // to be <= the absorbed photon's; the emission is zero at and above the
        // 488 nm absorption cutoff, which keeps that energy check satisfiable.
        const G4double hc = 1239.84193;   // eV*nm
        const G4double L0 = fWLSAbsLength430;
        const G4int Na = 37;
        const G4double lamA[Na] = { 650, 500, 488, 486, 483, 478, 474, 472, 470,
                                    467, 465, 462, 459, 457, 453, 450, 448, 444,
                                    442, 440, 437, 435, 430, 427, 423, 420, 415,
                                    410, 405, 400, 396, 390, 386, 380, 370, 360,
                                    350 };
        const G4double relA[Na] = { 0.0, 0.0, 0.0, 0.005, 0.015, 0.054, 0.11, 0.20,
                                    0.31, 0.45, 0.59, 0.71, 0.80, 0.85, 0.88, 0.85,
                                    0.81, 0.77, 0.79, 0.84, 0.90, 0.96, 1.00, 0.93,
                                    0.84, 0.75, 0.64, 0.59, 0.51, 0.44, 0.37, 0.29,
                                    0.24, 0.15, 0.075, 0.045, 0.03 };
        G4double ea[Na], la[Na];
        for (G4int i = 0; i < Na; ++i) {
            ea[i] = hc/lamA[i] * eV;
            la[i] = (relA[i] > 0.) ? L0/relA[i] : 1e6*m;
        }
        mptW->AddProperty("WLSABSLENGTH", ea, la, Na);

        const G4int Ne = 36;
        const G4double lamE[Ne] = { 650, 640, 620, 600, 590, 580, 570, 560, 550,
                                    540, 531, 526, 520, 516, 512, 507, 503, 500,
                                    495, 490, 487, 485, 482, 479, 476, 473, 471,
                                    469, 466, 463, 461, 458, 456, 452, 449, 444 };
        const G4double relE[Ne] = { 0.0, 0.0, 0.005, 0.02, 0.03, 0.05, 0.085, 0.13,
                                    0.19, 0.25, 0.31, 0.43, 0.55, 0.63, 0.71, 0.77,
                                    0.76, 0.73, 0.69, 0.73, 0.80, 0.85, 0.92, 0.98,
                                    1.00, 0.92, 0.82, 0.67, 0.50, 0.32, 0.21, 0.14,
                                    0.09, 0.04, 0.01, 0.0 };
        G4double ee[Ne], we[Ne];
        for (G4int i = 0; i < Ne; ++i) { ee[i] = hc/lamE[i] * eV; we[i] = relE[i]; }
        mptW->AddProperty("WLSCOMPONENT", ee, we, Ne);
        G4cout << " [MuonScint] Y-11 Kuraray absorption+emission spectra, "
               << "WLSABSLENGTH = " << L0/mm << " mm at 430 nm" << G4endl;
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

    // ---------- Optional multi-clad: outer fluoropolymer sheath --------------
    // The real Y-11(200) is multi-clad; the outer low-index (n=1.42)
    // fluoropolymer layer is what sustains TIR when the fibre sits in a channel
    // cut in a higher-index clamp. Only built when fFibreMultiClad is set.
    G4Material* outerCladMat = cladMat;
    if (fFibreMultiClad) {
        outerCladMat = G4Material::GetMaterial("WLSFiberCladOuter", false);
        if (!outerCladMat) {
            outerCladMat = new G4Material("WLSFiberCladOuter", 1.43*g/cm3, 3);
            outerCladMat->AddElement(nist->FindOrBuildElement("C"), 2);
            outerCladMat->AddElement(nist->FindOrBuildElement("H"), 2);
            outerCladMat->AddElement(nist->FindOrBuildElement("F"), 2);

            const G4int No = 2;
            G4double eo[No]   = { 1.5*eV, 6.0*eV };
            G4double no[No]   = { 1.42, 1.42 };
            G4double abso[No] = { 20.*m, 20.*m };
            auto* mptO = new G4MaterialPropertiesTable();
            mptO->AddProperty("RINDEX",    eo, no,   No);
            mptO->AddProperty("ABSLENGTH", eo, abso, No);
            outerCladMat->SetMaterialPropertiesTable(mptO);
        }
    }

    // ---------- Bent fibre core + cladding solids (one pair per fibre) ------
    // Each fibre runs straight at the straight-section pitch (fFibrePitch) for
    // most of the plate, then converges over the ramp (fFibreConvergeLen minus
    // the 2 mm bundle stub) to a TOUCHING bundle (fBundlePitch) at the +y
    // read-out end, where all four ends face the single SiPM. The straight-run
    // pitch gives equal plastic margins across the 10 mm bar: x = 0.75 mm from
    // the outer fibre surfaces to the top/bottom faces, 2x = 1.5 mm between
    // neighbouring fibres (pitch 2.5 mm). The guide is a swept cylinder (see
    // MakeSweptTube / the analytic G4CutTubs legs), watertight through the
    // kinks; each fibre needs its own clad+core solids because the transverse
    // bend offset differs per fibre.
    const G4double rFibre = fFibreRadius;
    // Multi-clad: two 2%-of-D layers (0.02 mm each) -> inner clad r=0.48,
    // core r=0.46. Single clad: the legacy 3%-of-radius PMMA clad -> core
    // r=0.485.
    const G4double cladThick = 0.02 * (2.0 * rFibre);          // 2 % of diameter
    const G4double rInner = rFibre - cladThick;                // PMMA inner clad O/D
    const G4double rCore  = fFibreMultiClad ? (rFibre - 2.0*cladThick)
                                            : (rFibre * (1.0 - fFibreCladFrac));
    // At the touching bundle the centre pitch equals 2*rFibre, so square
    // cross-sections would share fully coincident flat faces and the mitered
    // ramp rings interpenetrate by ~0.1 um (GeomVol1002). Round fibres dodge
    // this because tangent circles meet only along a line. Shrink the square by
    // 2% (0.02 mm bundle gap); spacing/kink/bend are unchanged, only the
    // cross-section width. For round fibres the factor is exactly 1.
    const G4double sqShrink = fFibreSquareCross ? 0.98 : 1.0;
    const G4double cladSize = rFibre * sqShrink;
    const G4double coreSize = rCore  * sqShrink;
    const G4double halfL  = fHalfSize.y();   // run the full plate length (y)
    const G4double resL   = 2.0*mm;          // straight bundled stub before +end
    const G4double conv   = fFibreConvergeLen;
    // Cutter overshoot: the groove tool is made longer than the film/clamp so
    // the opening is bounded by the object's own faces, never by a face
    // coincident with the tool (coincident faces -> degenerate polygons).
    const G4double cutterOv = 10.0*mm;
    // Radial clearance of the grease groove over the fibre cladding.  Carving
    // the groove at exactly rFibre makes the boolean-meshed groove surface
    // coincide with the tessellated cladding surface to within the re-meshing
    // error (~18 nm, well above the 0.1-1 nm navigation tolerance); the
    // resulting ambiguity makes G4Navigator emit GeomNav1002 "Stuck Track".
    // A small clearance (<< fClampGap/2, so a grease layer still wets the
    // scintillator face) removes the coincidence.  Set grooveClear = 0 to
    // restore the original coincident-surface behaviour.
    // A/B result (round preset 5, centre, 400 ev): grooveClear = 0 -> 28.35 pe/evt,
    // grooveClear = 4 um -> 40.99 pe/evt (+45 %), because the sub-mm vacuum annulus
    // left around the fibre acts as a low-index cladding and boosts TIR guiding.
    // The clearance is therefore NOT optics-preserving and is rejected as a fix;
    // 0 (coincident, physically correct) is kept.
    const G4double grooveClear = 0.0*um;   // coincident clad<->grease (see above)
    const G4int    ringSeg  = 24;            // azimuthal facets around a fibre
    const G4bool   squareCross = fFibreSquareCross;
    // Cross-section dispatch: round swept tube (ringSeg-gon) or square swept
    // prism of the same width. r is the circle radius, hw the square half-width
    // (numerically equal in every use here, so a single "size" value suffices).
    auto swept = [&](const G4String& nm, G4double size,
                     const std::vector<G4ThreeVector>& p) -> G4VSolid* {
        return squareCross ? MakeSweptSquare(nm, size, p)
                           : MakeSweptTube(nm, size, p, ringSeg);
    };

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
    // Analytic round fibre: each straight leg of the centreline is a clad
    // G4CutTubs with a coaxial core G4CutTubs daughter carrying the SAME cut
    // planes (so core and clad end faces are coplanar, as in the tessellated
    // model). Legs of one fibre are world siblings joined at exact miter planes.
    const G4bool analytic = fFibreAnalytic && !squareCross;
    // Exactly touching bundled cylinders (pitch = 2 r) are tangent, and near
    // the end of the ramp the tilted legs (ellipse half-width r/cos(alpha),
    // alpha <= 3.3 deg) interpenetrate by up to ~0.3 um (GeomVol1002). The
    // analytic fibre therefore bundles at 2 r + 2 um: optically still touching.
    const G4double bundlePitch = analytic ? fBundlePitch + 2.0*um : fBundlePitch;

    std::vector<G4double> zSpread(fNFibres), zBundle(fNFibres);
    for (G4int k = 0; k < fNFibres; ++k) {
        zSpread[k] = (k - 0.5*(fNFibres - 1)) * fFibrePitch;
        zBundle[k] = (k - 0.5*(fNFibres - 1)) * bundlePitch;
    }

    fFibreCoreLVs.clear();
    fFibreCladLVs.clear();
    fFibreOuterLVs.clear();
    fFibreSegs.clear();
    if (analytic) fFibreSegs.resize(fNFibres);
    for (G4int k = 0; analytic && k < fNFibres; ++k) {
        const auto tag = std::to_string(k);
        const auto path = BentPath(fFibreAxisX, zSpread[k], zBundle[k],
                                   halfL, conv, resL, 0.);
        auto cladLegs  = MakeCutTubeLegs("WLSFibreCladSolid_" + tag, rFibre, path);
        auto innerLegs = fFibreMultiClad
            ? MakeCutTubeLegs("WLSFibreInnerSolid_" + tag, rInner, path)
            : std::vector<CutTubeLeg>();
        auto coreLegs  = MakeCutTubeLegs("WLSFibreCoreSolid_" + tag, rCore,  path);
        for (std::size_t s = 0; s < cladLegs.size(); ++s) {
            const auto stag = tag + "_" + std::to_string(s);
            auto* cladLV = new G4LogicalVolume(cladLegs[s].solid, outerCladMat,
                                               "WLSFibreCladLV_" + stag);
            cladLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.9, 0.5, 0.6)));
            // Innermost layer carrying the core: the extra PMMA clad when
            // multi-clad, otherwise the outer clad itself.
            G4LogicalVolume* coreMother = cladLV;
            if (fFibreMultiClad) {
                auto* innerLV = new G4LogicalVolume(innerLegs[s].solid, cladMat,
                                                    "WLSFibreInnerLV_" + stag);
                innerLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.9, 0.5, 0.6)));
                new G4PVPlacement(nullptr, G4ThreeVector(), innerLV,
                                  "WLSFibreInnerPhys_" + stag, cladLV, false, k, false);
                fFibreCladLVs.push_back(innerLV);   // for IsFibreLV only
                coreMother = innerLV;
            }
            auto* coreLV = new G4LogicalVolume(coreLegs[s].solid, coreMat,
                                               "WLSFibreCoreLV_" + stag);
            coreLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.7, 1.0, 0.9, 0.95)));
            coreLV->SetUserLimits(new G4UserLimits(DBL_MAX, DBL_MAX, 10.*ms));
            new G4PVPlacement(nullptr, G4ThreeVector(), coreLV,
                              "WLSFibreCorePhys_" + stag, coreMother, false, k, false);
            fFibreSegs[k].push_back({cladLV, cladLegs[s].rot, cladLegs[s].pos});
            fFibreCladLVs.push_back(cladLV);   // for IsFibreLV only
            fFibreCoreLVs.push_back(coreLV);
        }
    }
    for (G4int k = 0; !analytic && k < fNFibres; ++k) {
        const auto tag = std::to_string(k);
        const auto path = BentPath(fFibreAxisX, zSpread[k], zBundle[k],
                                   halfL, conv, resL, 0.);

        auto* cladSolid = (squareCross && fFibreUseExtruded)
            ? MakeExtrudedFibre("WLSFibreCladSolid_" + tag, path, cladSize)
            : swept("WLSFibreCladSolid_" + tag, cladSize, path);
        auto* cladLV = new G4LogicalVolume(cladSolid, outerCladMat,
                                           "WLSFibreCladLV_" + tag);
        cladLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.9, 0.5, 0.6)));

        // Optional PMMA inner clad (multi-clad only).
        G4LogicalVolume* coreMother = cladLV;
        if (fFibreMultiClad) {
            const G4double innerSize = rInner * sqShrink;
            auto* innerSolid = (squareCross && fFibreUseExtruded)
                ? MakeExtrudedFibre("WLSFibreInnerSolid_" + tag, path, innerSize)
                : swept("WLSFibreInnerSolid_" + tag, innerSize, path);
            auto* innerLV = new G4LogicalVolume(innerSolid, cladMat,
                                                "WLSFibreInnerLV_" + tag);
            innerLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.2, 0.9, 0.5, 0.6)));
            new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 0.), innerLV,
                              "WLSFibreInnerPhys_" + tag, cladLV, false, k, false);
            fFibreCladLVs.push_back(innerLV);
            coreMother = innerLV;
        }

        auto* coreSolid = (squareCross && fFibreUseExtruded)
            ? MakeExtrudedFibre("WLSFibreCoreSolid_" + tag, path, coreSize)
            : swept("WLSFibreCoreSolid_" + tag, coreSize, path);
        auto* coreLV = new G4LogicalVolume(coreSolid, coreMat,
                                           "WLSFibreCoreLV_" + tag);
        coreLV->SetVisAttributes(new G4VisAttributes(true, G4Colour(0.7, 1.0, 0.9, 0.95)));
        coreLV->SetUserLimits(new G4UserLimits(DBL_MAX, DBL_MAX, 10.*ms));
        // Core shares the clad's slab-local frame, so it is placed at the clad
        // origin; every clad placement then carries its own core copy.
        new G4PVPlacement(nullptr, G4ThreeVector(0., 0., 0.), coreLV,
                          "WLSFibreCorePhys_" + tag, coreMother, false, k, false);

        fFibreCladLVs.push_back(cladLV);
        fFibreCoreLVs.push_back(coreLV);
        fFibreOuterLVs.push_back(cladLV);
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
    // grease layer remains against the scintillator. The grease (n=1.46) now
    // wets the outer fluoropolymer sheath (n=1.42, multi-clad) rather than the
    // PMMA clad (n=1.49, single-clad); the core/inner-clad guide is unaffected
    // because the sheath is only 0.02 mm thick.
    auto* filmBox = new G4Box("WLSCoupleFilmBox",
                              filmHalfX, fHalfSize.y(), fHalfSize.z());
    G4Material* greaseMat = G4Material::GetMaterial("OpticalGrease", false);
    {
        // Carve the film with a groove CONCENTRIC to each fibre, of radius
        // rFibre + grooveClear, so the grease closely wets the fibre surface
        // without sharing a boolean-meshed boundary with the tessellated
        // cladding (which caused GeomNav1002 stuck tracks). The clearance must
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
            const auto tag = std::to_string(k);
            auto* groove = analytic
                ? MakeCutTubeCutter("WLSCoupleGroove_" + tag,
                                    rFibre + grooveClear, tool)
                : swept("WLSCoupleGroove_" + tag,
                        rFibre + grooveClear, tool);
            solid = new G4SubtractionSolid("WLSCoupleCut_" + tag,
                                           solid, groove, nullptr,
                                           G4ThreeVector(0., 0., 0.));
        }
        fGrease4LV = new G4LogicalVolume(solid, greaseMat, "WLSCoupleFilmLV");
    }
    fGrease4LV->SetVisAttributes(new G4VisAttributes(true, G4Colour(1.0, 0.85, 0.2, 0.6)));

    // ---------- Clamp body with milled fibre channels ----------
    // The clamp body is white 3D-printed plastic by default (see
    // fClampSurfaceMode); the material below is the n=1.55 bulk whose 1 um
    // ABSLENGTH makes it fully absorbing in kClampBlack mode. The clamp carries
    // one full-length channel per fibre, each of radius
    // fChannelRadius, cut with the SAME bent-tube tool as the fibre so the
    // channel follows the fibre through the straight run, the ramp and the
    // bundle. Channels are separated by fFibrePitch > 2*fChannelRadius in the
    // straight run so a solid web of clamp material remains between neighbours
    // (as in Rao et al. Fig. 4a). Over the last stretch the fibres converge to a
    // touching bundle, so the channels there merge into one slot -- which is
    // exactly the real holder: a narrow groove for the bundle feeding the SiPM.
    //
    // The channel is a hair wider than the fibre (Ø1.1 vs Ø1.0), leaving a thin
    // air annulus. The Y-11(200) is multi-clad (outer fluoropolymer n~1.42,
    // fFibreMultiClad default on); without that outer sheath the single PMMA
    // clad is n=1.49, so if it touched the n=1.55 clamp bulk the low-index gap
    // sustaining TIR would vanish. The clearance is kept regardless. The
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
            const auto tag = std::to_string(k);
            auto* channel = analytic
                ? MakeCutTubeCutter("WLSChannel_" + tag, fChannelRadius, tool)
                : swept("WLSChannel_" + tag, fChannelRadius, tool);
            clampSolid = new G4SubtractionSolid("WLSClampSolid_" + tag,
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

    // White 3D-printed-plastic clamp body (default): Lambertian, R = 0.85
    // (absorbs 15 %). groundfrontpainted => no TIR. A skin applies to EVERY
    // clamp face, so this models the whole printed bar, not just its outboard
    // side. See ClampSurfaceMode / setClampSurface.
    fClampWhite = new G4OpticalSurface("MuonScintClampWhite");
    fClampWhite->SetModel(unified);
    fClampWhite->SetType(dielectric_dielectric);
    fClampWhite->SetFinish(groundfrontpainted);   // Lambertian, no TIR
    {
        const G4int Nw = 2;
        G4double ew[Nw] = { 1.5*eV, 6.0*eV };
        G4double rw[Nw] = { 0.85, 0.85 };
        auto* mptWh = new G4MaterialPropertiesTable();
        mptWh->AddProperty("REFLECTIVITY", ew, rw, Nw);
        fClampWhite->SetMaterialPropertiesTable(mptWh);
    }

    // ---------- Far-end mirror cap (full disc at the fibre's far end) --------
    // A complete Ø2*rFibre Al disc closes the fibre end. (The old half-disc only
    // covered half the face; full discs sit at the SPREAD pitch fFibrePitch >
    // 2*rFibre so neighbouring caps do not overlap.)
    const G4double capHalf = 0.05 * mm;
    // Round fibre: a Ø2*rFibre disc (tube axis later rotated z->y). Square
    // fibre: an axis-aligned box closing the flat end face (fibre axis already
    // +y, so no placement rotation is needed for it).
    G4VSolid* capSolid = squareCross
        ? static_cast<G4VSolid*>(new G4Box("WLSFibreMirrorCap",
                                           rFibre, capHalf, rFibre))
        : static_cast<G4VSolid*>(new G4Tubs("WLSFibreMirrorCap", 0., rFibre,
                                            capHalf, 0., 360.*deg));
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

    // ---- Clamp body (outboard of the fibres) ----
    new G4PVPlacement(rotCopy(flipRot),
                      mapPos(G4ThreeVector(clampCx, 0., 0.)),
                      fClampLV, "WLSClampPhys", worldLV, false, 0, true);
    // NOTE: a skin surface applies to EVERY face of the LV (channel walls and
    // the grease-facing front included), not just the outboard face.
    switch (fClampSurfaceMode) {
        case kClampBlack:
            G4cout << " [MuonScint] clamp skin = none (black absorbing bulk)" << G4endl;
            break;
        case kClampMirror:
            new G4LogicalSkinSurface("WLSClampMirrorSkin", fClampLV, fClampMirror);
            break;
        case kClampWhite:
        default:
            new G4LogicalSkinSurface("WLSClampWhiteSkin", fClampLV, fClampWhite);
            G4cout << " [MuonScint] clamp skin = white Lambertian R=0.85" << G4endl;
            break;
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
    // Square fibres are built as G4ExtrudedSolid in their own local frame (see
    // MakeExtrudedFibre), which needs an extra local->slab rotation and an x
    // offset; the round tessellated fibres are already in slab-local coordinates.
    G4RotationMatrix baseFibreRot = flipRot;
    G4ThreeVector    cladPos(0., 0., 0.);
    if (fFibreSquareCross && fFibreUseExtruded) {
        // Desired object (local->slab) rotation is flipRot*M; G4 stores the
        // inverse of the matrix passed to G4PVPlacement, so pass its inverse.
        G4RotationMatrix objRot = flipRot * ExtrudedFibreRot();
        baseFibreRot = objRot.inverse();
        cladPos      = flipRot * G4ThreeVector(fFibreAxisX, 0., 0.);
    }
    // The clad PV touching the far-end mirror cap and the one touching the
    // read-out pad (the same PV for a single-solid fibre; the first / last leg
    // for the analytic multi-leg fibre).
    std::vector<G4VPhysicalVolume*> capSidePVs(fNFibres, nullptr);
    std::vector<G4VPhysicalVolume*> padSidePVs(fNFibres, nullptr);
    if (!fFibreSegs.empty()) {
        // Analytic legs: object transform = slab o flip o leg (all active).
        for (G4int k = 0; k < fNFibres; ++k) {
            const auto& legs = fFibreSegs[k];
            for (std::size_t s = 0; s < legs.size(); ++s) {
                G4RotationMatrix objRot = flipRot * legs[s].rot;
                G4ThreeVector    objPos = flipRot * legs[s].pos;
                if (slabRot) { objRot = (*slabRot) * objRot; objPos = (*slabRot) * objPos; }
                objPos += slabPos;
                auto* pv = new G4PVPlacement(G4Transform3D(objRot, objPos),
                                             legs[s].cladLV,
                                             "WLSFibreCladPhys_" + std::to_string(k)
                                                 + "_" + std::to_string(s),
                                             worldLV, false, k, true);
                if (s == 0)               capSidePVs[k] = pv;
                if (s + 1 == legs.size()) padSidePVs[k] = pv;
            }
        }
    }
    for (G4int k = 0; fFibreSegs.empty() && k < fNFibres; ++k) {
        cladPVs[k] = new G4PVPlacement(rotCopy(baseFibreRot),
                                       mapPos(cladPos),
                                       fFibreOuterLVs[k],
                                       "WLSFibreCladPhys_" + std::to_string(k),
                                       worldLV, false, k, true);
        capSidePVs[k] = cladPVs[k];
        padSidePVs[k] = cladPVs[k];
    }

    // ---- Far-end mirror caps (world siblings just past the +y fibre ends
    // after the flip). The cap disc's axis is put along y (rotateX(+90) maps
    // tube local z -> y); a full disc is rotationally symmetric so Phi is
    // immaterial. zk is mirrored so cap k pairs with the flipped fibre k.
    const G4double capPhi = fWLSOnNegX ? 180.*deg : 0.;
    G4RotationMatrix capRot;
    if (!fFibreSquareCross) capRot.rotateX(90.*deg);  // tube axis z -> y (box cap already aligned)
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
    // ---- Slab <-> read-out grease pad contact ----
    // The 4x4 mm read-out grease pad (x in [-127.55,-123.55], z in [-2,2])
    // overlaps the slab's -Y face over x in [-125.0,-123.55], z in [-2,2].
    // In the experimental setup (Rao et al. 2025) this portion of the SiPM
    // active face is in direct optical contact with the slab via optical grease
    // (fWLSPadSlabContact = true, default): register fCouplingSurface in both
    // directions so it overrides the reflector skin and couples light through
    // a polished Fresnel interface.
    // When fWLSPadSlabContact is false (legacy fibre-only study mode), wrapped
    // slabs fall back to MuonScintSkin and the bare slab uses an explicit
    // air-gap absorber border (P4a) so no direct slab light enters the pad.
    if (fWLSPadSlabContact) {
        if (fScintPV && padPV && fCouplingSurface) {
            new G4LogicalBorderSurface("MuonScint_SlabPad", fScintPV, padPV,
                                       fCouplingSurface);
            new G4LogicalBorderSurface("MuonScint_PadSlab", padPV, fScintPV,
                                       fCouplingSurface);
        }
    } else if (fReflectorType == kNoReflector && fScintPV && padPV) {
        auto* gap = new G4OpticalSurface("MuonScintPadAirGap");
        gap->SetModel(unified);
        gap->SetType(dielectric_dielectric);
        gap->SetFinish(polishedbackpainted);   // Fresnel/TIR vs the air backing
        auto* m = new G4MaterialPropertiesTable();
        const G4int ng = 2;
        G4double eg[ng]   = { 1.5*eV, 6.0*eV };
        G4double nAir[ng] = { 1.0003, 1.0003 };
        G4double r0[ng]   = { 0., 0. };
        m->AddProperty("RINDEX", eg, nAir, ng);         // surface backing index
        m->AddProperty("REFLECTIVITY", eg, r0, ng);     // transmitted light absorbed
        gap->SetMaterialPropertiesTable(m);
        new G4LogicalBorderSurface("MuonScint_SlabPadGap", fScintPV, padPV, gap);
        new G4LogicalBorderSurface("MuonScint_PadSlabGap", padPV, fScintPV, gap);
    }
    // far-end mirror cap: specular Al reflector.
    // A skin on the cap LV covers *every* face the borders below do not, in
    // particular the CORE<->cap crossing.  Without it that crossing has no
    // surface at all, and since G4_Al carries no RINDEX the boundary process
    // kills the photon with NoRINDEX (measured: 17/40 events, exactly the
    // core->cap crossings) instead of reflecting it.  Borders take precedence
    // over skins, so the clad<->cap borders below are unaffected.
    if (fFibreEndMirror) {
        new G4LogicalSkinSurface("WLSFibreCapMirrorSkin", fMirrorCapLV,
                                 fFibreEndMirror);
        for (G4int k = 0; k < fNFibres; ++k) {
            new G4LogicalBorderSurface("MuonScint_FibreCap_" + std::to_string(k),
                                       capSidePVs[k], capPVs[k], fFibreEndMirror);
            new G4LogicalBorderSurface("MuonScint_CapFibre_" + std::to_string(k),
                                       capPVs[k], capSidePVs[k], fFibreEndMirror);
        }
    }
    // fibre read-out end (-y after the placement flip) -> read-out pad:
    // transparent (couples guided light out).
    // The single SiPM views ALL FOUR fibre end faces simultaneously (Rao et al.
    // Fig. 4a), so every fibre's read-out end carries the coupling border into the
    // shared read-out pad. Without it the un-listed fibres fall back to the default
    // polished Fresnel boundary, which for core->pad is optically similar to
    // fCouplingSurface; the borders are therefore belt-and-braces, not load-bearing.
    if (fCouplingSurface && padPV) {
        for (std::size_t k = 0; k < padSidePVs.size(); ++k) {
            new G4LogicalBorderSurface("MuonScint_FibrePad_" + std::to_string(k),
                                       padSidePVs[k], padPV, fCouplingSurface);
            new G4LogicalBorderSurface("MuonScint_PadFibre_" + std::to_string(k),
                                       padPV, padSidePVs[k], fCouplingSurface);
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
    // The bare configuration (kNoReflector) gets a wrap only when the
    // face-scattering study is active (kStudyBareRoughness > 0); otherwise it
    // keeps the plain polished scintillator <-> air Fresnel/TIR boundary
    // across the real air gap.
    const G4bool bareContactSkin =
        (fReflectorType == kNoReflector) && UsesBlackHolder() &&
        fBareContactFraction > 0.;
    if (fWrapWithReflector &&
        (fReflectorType != kNoReflector || kStudyBareRoughness > 0. ||
         bareContactSkin)) {
        new G4LogicalSkinSurface("MuonScintSkin", fScintLV, fReflSurface);
    }

    // Black holder: bare configuration only. The shell is the structural
    // 3D-printed enclosure whose cavity is drawn fHolderGap larger than the
    // slab. The gap is optically real: light that refracts out of the plate
    // crosses it and is absorbed by the holder's skin below.
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
    SetFibreSquareCross(false);
    SetFibreAnalytic(false);
    fWLSOnNegX = true;
    AddSiPM({0, 0.0, 0.0});   // read-out SiPM; placement is overridden in PlaceWLSFibres
}

void GeometryMuonScint::PresetConfig_WLSFibreClampAnalytic() {
    // Config 4 with the round fibres, film grooves and clamp channels built
    // from analytic mitered G4CutTubs legs instead of tessellated swept tubes.
    // Identical centreline, radii, materials and surfaces as preset 5.
    PresetConfig_WLSFibreClamp();
    SetFibreAnalytic(true);
}

void GeometryMuonScint::PresetConfig_WLSFibreClampBox() {
    // Config 4 variant: identical to PresetConfig_WLSFibreClamp (same four
    // fibres, same pitch/kink/bend, single read-out SiPM on the -X narrow face)
    // but with a flat-faced SQUARE fibre cross-section instead of the Ø1.0 mm
    // round one. This removes the 24-gon's shallow facet edges (the suspected
    // G4Navigator stuck-track trigger) and makes the clamp/grease booleans
    // purely planar. Used for the round-vs-box stuck-track A/B comparison.
    ClearSiPMs();
    SetUseWLSFibres(true);
    SetFibreSquareCross(true);
    fWLSOnNegX = true;
    AddSiPM({0, 0.0, 0.0});   // read-out SiPM; placement is overridden in PlaceWLSFibres
}







