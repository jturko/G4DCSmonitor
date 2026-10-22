# Scintillator materials/optical-properties faithfulness audit

Read-only audit of the muon-scintillator optical model against the three supplied
sources. Date: 2026-10-15. No source files were modified.

## Scope and sources

| Key | File | Notes |
|-----|------|-------|
| EljenEJ200 | `report/refs/EljenEJ200.pdf` | Eljen EJ-200/204/208/212 datasheet (Aug 2023). PVT base. |
| AmcrysPS | `report/refs/AmcrysPS.pdf` | Amcrys "Polystyrene-based Plastic Scintillators" (UPS-923A grade), Sep 2009. Polystyrene base. |
| Birks1951 | `report/refs/Birks1951.pdf` | Birks, *Proc. Phys. Soc. A* **64**, 874 (1951). **Anthracene crystals**, not PVT. |

Code inspected: `src/GeometryMuonScint.cc` (`BuildOpticalProperties`, `Build`),
`include/GeometryMuonScint.hh`, `src/PhysicsList.cc`, `report/matrix_report.tex`,
plus `.refs/geant4/source/...` Geant4 reference for the Birks plumbing only (read-only).

Geant4 base material: `G4_PLASTIC_SC_VINYLTOLUENE`, whose NIST density is
`1.032 g/cm3` (`.refs/geant4/source/materials/src/G4NistMaterialBuilder.cc:1481`); the
simulation reuses it verbatim (`src/GeometryMuonScint.cc:539-547`).

## Property-by-property

### 1. Light yield (photons/MeV)
- (a) EljenEJ200: "Scintillation Efficiency (photons/1 MeV e-) = 10,000"; "Light
  Output = 64 % Anthracene" (properties table, p. 2). AmcrysPS: "Light output,
  % of anthracene 56"; no absolute ph/MeV quoted.
- (b) `SCINTILLATIONYIELD = fLightYieldScale * 10000./MeV` (`src/GeometryMuonScint.cc:149`);
  `fLightYieldScale = 1.0` default (`include/GeometryMuonScint.hh:144`).
- (c) **MATCH** (to EljenEJ200 10,000 ph/MeV).
- (d) The comment "UPS-923A: ~9,500 ph/MeV" (`src/GeometryMuonScint.cc:146`) is
  **UNSUPPORTED by AmcrysPS** — the datasheet states 56 % of anthracene, not an
  absolute value. 9,500 ph/MeV is consistent with Artikov2005 (paywalled, absent).

### 2. Emission peak wavelength
- (a) EljenEJ200: 425 nm (EJ-200). AmcrysPS: "Wavelength of maximum emission = 418 nm".
- (b) 425 nm = 2.917 eV, used as the spectrum peak (`src/GeometryMuonScint.cc:82,132`);
  report table lists 425 nm (`report/matrix_report.tex:306`).
- (c) **MATCH** (EljenEJ200).
- (d) Amcrys 418 nm is **not used** (mixed provenance, see notes below).

### 3. Emission spectrum shape / range / FWHM
- (a) Neither datasheet gives a spectrum shape or FWHM; both list only the peak.
  EljenEJ200's "Pulse Width FWHM" (2.5 ns) is temporal, not spectral.
- (b) 14-point `SCINTILLATIONCOMPONENT1` (`src/GeometryMuonScint.cc:123-138`);
  code comment "Emission FWHM: ~80 nm; effectively zero below 380 nm and above
  550 nm" (`:61`); report repeats "essentially zero below 380 nm and above 550 nm"
  (`report/matrix_report.tex:124`).
- (c) **UNSUPPORTED** (modelled, not from either datasheet).
- (d) Internal inconsistency: the code spectrum is **non-zero above 550 nm**
  (0.10 at 551 nm, 0.18 at 530 nm is below, but 0.05 at 575 nm, 0.02 at 600 nm,
  0.00 at 700 nm). So the report/comment "zero above 550 nm" overstates the model.

### 4. Bulk attenuation length
- (a) EljenEJ200: "Light Attenuation Length (cm) = 380" (i.e. 3.8 m). AmcrysPS:
  "Absorption coefficient, cm^-1 = 0.002–0.005" ⇒ 2–5 m (no single value).
- (b) `absL` table (`src/GeometryMuonScint.cc:100-115`): plateau 3.8 m at
  478–551 nm, **2.5 m at the 425 nm emission peak**, falling in the UV.
  Report: "3.8 m (peak, falling in the UV)" (`report/matrix_report.tex:307`).
- (c) **MATCH (value present), with a modelling caveat**.
- (d) The datasheet quotes a single 3.8 m; the code assigns 3.8 m off-peak and
  reduces to 2.5 m exactly at the emission wavelength. No datasheet support for
  the 2.5 m point or the UV fall-off (the latter is a deliberate choice, stated
  in `report/matrix_report.tex:121-124`). Calling 3.8 m the "peak" is loose but
  numerically faithful.

### 5. Refractive index
- (a) EljenEJ200: 1.58 (flat, all grades). AmcrysPS: 1.6.
- (b) flat 1.58 (`src/GeometryMuonScint.cc:91-94`); report table 1.58 (`:303`).
- (c) **MATCH** (EljenEJ200). Amcrys 1.6 is close and not used.

### 6. Density
- (a) EljenEJ200: **1.023 g/cm3**. AmcrysPS: **1.06 g/cm3**.
- (b) inherited from `G4_PLASTIC_SC_VINYLTOLUENE`, `baseScint->GetDensity()`
  (`src/GeometryMuonScint.cc:539-547`), = **1.032 g/cm3**
  (`.refs/geant4/source/materials/src/G4NistMaterialBuilder.cc:1481`); report table also
  shows 1.032 (`report/matrix_report.tex:304`).
- (c) **MISMATCH** vs both datasheets (1.032 vs 1.023 Eljen; 1.032 vs 1.06 Amcrys).
- (d) The 1.032 value is a Geant4/NIST library number, **not sourced from either
  datasheet**. The report's "Ref." column (1.05) is from Rao2025, not these sources.

### 7. Decay time
- (a) EljenEJ200: 2.1 ns (EJ-200). AmcrysPS: "Decay time, ns = 2–3".
- (b) `SCINTILLATIONTIMECONSTANT1 = 2.1 ns` (`src/GeometryMuonScint.cc:158`);
  report table 2.1 ns (`:308`). Code comments repeatedly cite "UPS-923A -> 3.3 ns"
  (`src/GeometryMuonScint.cc:62,156`).
- (c) **MATCH** for the value used (EljenEJ200 2.1 ns).
- (d) The "UPS-923A = 3.3 ns" annotation is **UNSUPPORTED by AmcrysPS** (datasheet
  says 2–3 ns). 3.3 ns is the EJ-208 figure (EljenEJ200), so it is likely
  misattributed. Since 2.1 ns is actually used, this is a comment-level defect only.

### 8. Rise time
- (a) EljenEJ200: 0.9 ns (EJ-200). AmcrysPS: 0.9 ns.
- (b) `SCINTILLATIONRISETIME1 = 0.9 ns` (`src/GeometryMuonScint.cc:162`);
  report table 0.9 ns (`:309`).
- (c) **MATCH**.

### 9. Birks constant
- (a) **Neither provided source states a numeric mm/MeV Birks constant.**
  Birks1951 gives, for **anthracene**, `A = 82.5` and `kB = 7.15` (in MeV/cm air
  equivalent; §3, p. 876) and only establishes the functional form
  `dS/dr = A (dE/dr) / (1 + kB dE/dr)` (Eq. 1). Craun1970 (`Analysis of response
  data for several organic scintillators`, NIM 80) — the actual origin of the
  cited value — is **not among the provided files** (paywalled; `report/refs/manifest.csv`).
- (b) `scintMat->GetIonisation()->SetBirksConstant(0.126*mm/MeV)`
  (`src/GeometryMuonScint.cc:172`); report table 0.126 mm/MeV
  (`report/matrix_report.tex:310`); citation combines Birks1951 + Craun1970
  (`report/matrix_report.tex:119`).
- (c) **UNSUPPORTED** by the supplied sources (the number is not in Birks1951 and
  Craun1970 is absent). The citation of Birks1951 for the *value* is misleading —
  Birks1951 supports the *law*, applied to anthracene, not plastic at 0.126.
- (d) **Is it applied?** Yes. `G4OpticalPhysics` constructs `G4Scintillation`
  and calls `AddSaturation(G4LossTableManager::Instance()->EmSaturation())`
  (`.refs/geant4/source/physics_lists/constructors/electromagnetic/src/G4OpticalPhysics.cc:126-128`);
  `G4EmParameters::GetEmSaturation()` lazily creates the `G4EmSaturation` object
  and sets `birks = true` (`.refs/geant4/source/processes/electromagnetic/utils/src/G4EmParameters.cc:591-604`);
  `G4Scintillation` then scales the photon count by
  `VisibleEnergyDepositionAtAStep` (`.refs/geant4/source/processes/electromagnetic/xrays/src/G4Scintillation.cc:418-420`).
  `SetScintByParticleType(false)` is set (`src/PhysicsList.cc:146`), so Birks is
  not overridden. **The Birks constant is functionally active**, but its value is
  unsupported by the supplied references.

### 10. Polymer base / composition
- (a) EljenEJ200: "Polymer Base — Polyvinyltoluene" (H 5.17e22, C 4.69e22 cm^-3).
  AmcrysPS: polystyrene (H 4.91e22, C 4.91e22, H:C = 1.0).
- (b) `G4_PLASTIC_SC_VINYLTOLUENE` (PVT) (`src/GeometryMuonScint.cc:539`);
  report labels the material "PVT" (`report/matrix_report.tex:301`).
- (c) **MATCH** (to EljenEJ200 PVT); the polystyrene (Amcrys) composition is not used.

## Datasheet values present but NOT used by the model
- AmcrysPS emission peak 418 nm (code uses Eljen 425 nm).
- AmcrysPS refractive index 1.6 (code uses 1.58).
- AmcrysPS density 1.06 g/cm3 (code uses NIST PVT 1.032).
- AmcrysPS absorption coefficient 0.002–0.005 cm^-1 (code uses 3.8 m plateau).
- AmcrysPS H/C = 1.0, electrons 3.44e23 cm^-3 (code uses PVT NIST composition).
- EljenEJ200 pulse width FWHM 2.5 ns, softening point, temperature range (not modelled).
- EljenEJ200 "64 % anthracene" is implicitly carried by the 10,000 ph/MeV choice.

## Code/report values with NO support in the supplied sources
- Density **1.032 g/cm3** (`src/GeometryMuonScint.cc:539-547`; from Geant4 NIST).
- Birks constant **0.126 mm/MeV** (`src/GeometryMuonScint.cc:172`; Craun1970 absent).
- Emission **FWHM ~80 nm** and the 380/550 nm cut-offs
  (`src/GeometryMuonScint.cc:61`, `report/matrix_report.tex:124`).
- **2.5 m** attenuation at the 425 nm emission peak and the entire UV fall-off
  (`src/GeometryMuonScint.cc:109-115`).
- "UPS-923A" numbers **9,500 ph/MeV** (`:146`) and **3.3 ns** (`:62,156`) — not in
  AmcrysPS (likely Artikov2005, paywalled/absent).

## Top discrepancies (prioritized)
1. **Density MISMATCH**: code/report use 1.032 g/cm3
   (`src/GeometryMuonScint.cc:539-547`, `report/matrix_report.tex:304`), while
   EljenEJ200 says 1.023 and AmcrysPS says 1.06. The 1.032 value is unsourced
   (Geant4 NIST).
2. **Birks constant UNSUPPORTED**: 0.126 mm/MeV (`src/GeometryMuonScint.cc:172`,
   report `:310`) is not in EljenEJ200/AmcrysPS; Birks1951 is anthracene
   (kB = 7.15 cm·air/MeV) and establishes only the law form. Craun1970 is absent.
   The value is nevertheless active in the simulation, so this changes physics.
3. **Attenuation length applied off-peak**: datasheet quotes a single 3.8 m, but
   the model sets 2.5 m at the 425 nm emission peak with an unsupported UV
   fall-off (`src/GeometryMuonScint.cc:109-115`); report calls 3.8 m the "peak".
4. **Emission spectrum shape/FWHM/cut-offs UNSUPPORTED** and internally
   inconsistent: report says zero above 550 nm but the code has 0.05/0.02 at
   575/600 nm (`src/GeometryMuonScint.cc:123-138` vs `report/matrix_report.tex:124`).
5. **Mis-provenance of "UPS-923A" values**: 9,500 ph/MeV and 3.3 ns
   (`src/GeometryMuonScint.cc:146,62,156`) contradict AmcrysPS (56 % anthracene;
   2–3 ns); they trace to the paywalled Artikov2005, not the supplied datasheet.
6. **Mixed-source claim**: the model is described as "built from the EJ-200 and
   UPS-923A data sheets" (`report/matrix_report.tex:114-116`), but in practice
   every adopted optical constant comes from EljenEJ200; no AmcrysPS value is
   used. The provenance should be stated as EJ-200-only.
