# G4OpBoundaryProcess usage audit

Read-only consistency audit of every use of the optical **boundary process** in this
project against the Geant4 v11.3.2 source in `.refs/`.

- Ground truth: `.refs/source/processes/optical/{src/G4OpBoundaryProcess.cc,include/G4OpBoundaryProcess.hh}`,
  `.refs/source/processes/electromagnetic/utils/src/G4OpticalParameters{,Messenger}.cc`,
  `.refs/source/physics_lists/constructors/electromagnetic/src/G4OpticalPhysics.cc`,
  `.refs/source/materials/{src/G4OpticalSurface.cc,include/G4OpticalSurface.hh}`,
  `.refs/source/geometry/volumes/src/G4Logical{Border,Skin}Surface.cc`.
- No file outside `report/refs/notes/` was written or modified. `.refs/` untouched.
- Verdict vocabulary: **CONSISTENT**, **INCONSISTENT**, **UNSUPPORTED-BY-SOURCE**.
- Line numbers of our code refer to `src/`; the report text refers to `report/matrix_report.tex`.

---

## 1. How `G4OpBoundaryProcess::PostStepDoIt` actually decides (source walk)

This is the decision tree the rest of the audit checks our surfaces against.

1. **Material-1 index is mandatory.** If `fMaterial1->GetMaterialPropertiesTable()` has no
   `RINDEX`, the process sets `fStatus = NoRINDEX`, calls `ProposeLocalEnergyDeposit(fPhotonMomentum)`
   and `StopAndKill` — the photon dies. Silent unless `verboseLevel > 1`.
   (`G4OpBoundaryProcess.cc:288-306`.)
2. **Defaults reset each step:** `fReflectivity=1, fEfficiency=0, fTransmittance=0, fModel=glisur,
   fFinish=polished`, surface type `dielectric_dielectric`. (`:308-314`.)
3. **Surface lookup precedence:** a `G4LogicalBorderSurface` between the pre- and post-step
   volumes is consulted **first**; only if none exists is a `G4LogicalSkinSurface` used (of the
   post volume if its mother is the pre volume, else the pre volume, else the post volume).
   (`:319-347`.) → **border overrides skin.**
4. `fModel`/`fFinish` taken from the surface (`:351-352`).
5. For **backpainted** finishes (`polishedbackpainted`, `groundbackpainted`) the surface's *own*
   MPT must carry `RINDEX`, else `NoRINDEX` kill (`:356-374`). This surface `RINDEX` is the
   **backing** index (`fRindex2`), not a material index.
6. `fReflectivity`/`fEfficiency`/`fTransmittance` read from the surface MPT if present (`:383,392,396`).
7. `unified` model reads `SPECULARSPIKECONSTANT`, `SPECULARLOBECONSTANT`, `BACKSCATTERCONSTANT`
   from the surface MPT, each defaulting to **0** (`:403-414`).
8. For **polished/ground `dielectric_dielectric`**, the *material-2* `RINDEX` is required, else
   `NoRINDEX` kill (`:427-454`, `:448`).
9. `backpainted` → `DielectricDielectric()` (Fresnel/TIR at the coated interface) (`:456-459`).
10. Else (front-painted / bare): draws `rand`. `rand > R+T` → `DoAbsorption`; `rand > R` →
    `Transmission`; otherwise `polishedfrontpainted` → `SpikeReflection`, `groundfrontpainted` →
    `LambertianReflection`, anything else → `DielectricDielectric()` (`:460-489`).
11. `Detection` is produced **only** by `DoAbsorption` via `G4BooleanRand(fEfficiency)`
    (`include/G4OpBoundaryProcess.hh:303-322`). If `fStatus==Detection && fInvokeSD`, the SD is
    invoked with `step->AddTotalEnergyDeposit(fPhotonMomentum)` (`:559-560`, `InvokeSD` `:1498-1508`).
12. `sigma_alpha` is only ever consumed inside `GetFacetNormal` (`:659-694`), which is used only
    when the *interaction* is a `unified`/`LUT`/`DAVIS` rough facet. `polished` always takes the
    global normal (`:1124-1130`).

`SetInvokeSD` / `SetBoundaryVerboseLevel` write through to the global `G4OpticalParameters`
(`:1511-1523`); the process is only built if `GetProcessActivation("OpBoundary")` is true
(`G4OpticalPhysics.cc:114-116`).

---

## 2. Per-usage audit

### U1 — Photo-cathode (SiPM) detection — the whole point of the model
- **Ours:** `GeometryMuonScint.cc:375-390`: `fSiPMSurface` = `unified`, `dielectric_metal`,
  `polished`; MPT `REFLECTIVITY=0`, `EFFICIENCY=PDE(λ)` (peak 0.43 @ 420 nm). Wired via
  `G4LogicalBorderSurface(grease, SiPM, fSiPMSurface)` in `PlaceSiPMs`; SD `MuonScintSiPMSD`
  attached in `DetectorConstruction.cc:224-227`; `PhysicsList.cc:158` calls
  `optParams->SetBoundaryInvokeSD(true)`.
- **Source:** `dielectric_metal` → `DielectricMetal()` (`:724-817`) draws
  `G4BooleanRand(fEfficiency)`; on success `fStatus=Detection` (`:785-789`), and
  `:559-560` invokes the SD with `fPhotonMomentum` on the step. `REFLECTIVITY` used for the
  reflected branch. `InvokeSD` only fires for `Detection` (`:1498-1508`).
- **Verdict: CONSISTENT.** This is exactly the intended API path: the PDE draw is in Geant4, and
  our SD's `ProcessHits` therefore fires only for real photoelectrons (no manual draw).
  `SetBoundaryInvokeSD(true)` is required and present; without it the SD would never be called.
- **Notes:** `IsLocked()` is false in `PreInit`/`Init`/`Idle` (`G4OpticalParameters.cc:551-557`),
  so the PhysicsList-constructor call sticks. `MuonScintSiPMSD::ProcessHits` guards on
  `step->GetTotalEnergyDeposit()>0` and the SiPM LV, so the synthetic `AddTotalEnergyDeposit`
  is the only thing that can trigger it — correct. **No fix.**

### U2 — Reflective wraps (TiO₂ / Al / glossy / Teflon / bare)
- **Ours:** `GeometryMuonScint.cc:184-411`.
  - `kPaintTiO2` `:246-263`: `unified`, `dielectric_dielectric`, `groundfrontpainted`,
    `SetSigmaAlpha(0.1)`, R≈0.955–0.983, no `RINDEX`.
  - `kAluminumFoil` `:280-300`: `unified`, `polishedbackpainted`, surface-MPT `RINDEX=1.46`
    (backing), R≈0.904–0.925, `SPECULARSPIKECONSTANT=1`.
  - `kGlossyPaper` `:308-328`: `unified`, `polishedbackpainted`, backing `RINDEX=1.0003`,
    R≈0.900–0.955, `SPECULARSPIKECONSTANT=1`.
  - `kTeflon` `:334-354`: `unified`, `groundbackpainted`, backing `RINDEX=1.0003`, all three
    unified constants = 0.
  - `kNoReflector` `:225-237`: a slab skin is built **only** if `kStudyBareRoughness>0`, with
    `ground` finish + `SetSigmaAlpha(kStudyBareRoughness)` + `SPECULARSPIKECONSTANT=1`.
    Default `kStudyBareRoughness=0` (`include/GeometryMuonScint.hh`), so bare = **no surface**.
- **Source:** front-painted branch `:460-489`; `groundfrontpainted` → pure Lambertian
  (`:481`, `DoReflection`); backpainted → `DielectricDielectric()` + backing logic (`:456-459`,
  `:1310-1348`); `polished` forces the global facet normal (`:1124-1130`), so `sigma_alpha` is
  ignored; bare = no surface object → `glisur/polished` + `DielectricDielectric()` Fresnel/TIR
  with material `RINDEX`s (`:427-454`, `:1174-1294`).
- **Verdict:**
  - TiO₂ / Al / glossy / Teflon: **CONSISTENT** — options are valid and routed as intended.
  - TiO₂ `SetSigmaAlpha(0.1)`: **UNSUPPORTED-BY-SOURCE as an active parameter.** For
    `groundfrontpainted` the process never calls `GetFacetNormal` (`:460-489`), so `0.1` has
    **no effect**; the code comment ("~5.7° paint texture") is misleading. Harmless.
  - `kNoReflector` roughness study: **INCONSISTENT (latent).** `ground` + `unified` +
    `SPECULARSPIKECONSTANT=1` makes `ChooseReflection()` (`hh:281-301`) always pick
    `SpikeReflection`, which resets the facet normal to the global one (`:1154-1155`,
    `:1124-1130`). `sigma_alpha` is therefore never used → the "roughness" knob does not roughen.
    Dormant because `kStudyBareRoughness=0` by default, but the experimental branch is broken.
    Fix would be to set `SPECULARSPIKECONSTANT=0` (and let the Lambertian/lobe remainder use
    `sigma_alpha`) or drop `unified`-roughness intent.

### U3 — Black 3D-printed holder absorber
- **Ours:** `GeometryMuonScint.cc:397-410`: `fHolderSurface` = `unified`,
  `dielectric_dielectric`, `polished`, `REFLECTIVITY=TRANSMITTANCE=0`. Applied as a
  `G4LogicalSkinSurface` on `fHolderLV` (`:1283-1290`), and only when `UsesBlackHolder()`
  (bare & not WLS). `BuildHolder` `:654-667` uses `fHolderGap=1.2 mm`.
- **Source:** `polished` `dielectric_dielectric`, material2 (cavity air) has `RINDEX`
  (`DetectorConstruction.cc:137-145`), `rand > R+T = 0` → `DoAbsorption` → photon killed
  (`:460-489`, `hh:303-322`). Effective against light that refracts across the real air gap.
- **Verdict: CONSISTENT.** Absorber works as intended. (See D1 about how the *report* describes it.)

### U4 — Slab↔grease coupling & slab↔air (bare)
- **Ours:** `fCouplingSurface` `:362-366`: `unified`, `dielectric_dielectric`, `polished`,
  **no MPT**; borders registered in `PlaceSiPMs`/WLS placement. Bare slab keeps the auto
  skin↔air boundary (no surface).
- **Source:** polished dielectric-dielectric uses both materials' `RINDEX` (`:427-454`) ; no
  surface object uses defaults `glisur/polished` → `DielectricDielectric()` (`:308-314`,
  `:460-489`).
- **Verdict: CONSISTENT.** No MPT is required because both sides carry `RINDEX`
  (scint `1.58`, grease `1.46`, air `1.0003`). Energy-conserving Fresnel + TIR.

### U5 — WLS fibre end mirror & clamp mirror
- **Ours:** `fFibreEndMirror` `:1055-1067`: `unified`, `dielectric_metal`, `polished`,
  `REFLECTIVITY=0.90`, `EFFICIENCY=0`; applied as skin on the Al cap LV `:1053`. `fClampMirror`
  `:1029-1042`: `unified`, `dielectric_metal`, `polished`, `REFLECTIVITY=0.95`, `EFFICIENCY=0`,
  skin on `fClampLV` `:1134-1135`.
- **Source:** `dielectric_metal` (`:724-817`) with `REFLECTIVITY` for the reflected branch,
  `EFFICIENCY=0` → never `Detection`; a `G4LogicalSkinSurface` covers every face of the LV not
  already border-ed, and borders take precedence (`:319-347`).
- **Verdict: CONSISTENT.** `DielectricMetal()` needs only the surface `REFLECTIVITY`, never the
  metal `RINDEX`, so placing it on `G4_Al` caps is safe (this is the historical `NoRINDEX`
  core→cap bug's fix). Skin-on-cap + borders-on-(clad,cap) coexist by precedence.

### U6 — Global RINDEX provisioning
- **Ours:** MPT `RINDEX` on scintillator, grease, fibre core/clad, clamp, air; backing `RINDEX`
  on the surface MPTs of Al/glossy/Teflon.
- **Source:** material-1 `RINDEX` always required (`:288-306`); material-2 required for
  polished/ground dielectric-dielectric (`:427-454`); backpainted uses the **surface** backing
  `RINDEX` (`:356-374`); `groundfrontpainted` needs neither material index.
- **Verdict: CONSISTENT.** All boundaries are covered; no `NoRINDEX` path is reachable in a
  normal run (the fibre cap fix removed the only one).

---

## 3. Surface → process-behaviour mapping

| Our surface (file:line) | type / finish / model | key MPT | Geant4 path (source) | net behaviour |
|---|---|---|---|---|
| `fSiPMSurface` `:375-390` | dielectric_metal / polished / unified | EFFICIENCY=PDE, REFLECTIVITY=0 | `DielectricMetal` `:724-817`; SD `:559-560` | photo-electron draw, `Detection`, SD invoked |
| `kPaintTiO2` `:246-263` | dielectric_dielectric / groundfrontpainted / unified | R (no RINDEX) | front-painted `:460-489` (`:481`) | rand-vs-R; refracted remainder Lambertian, **no TIR**; `sigma_alpha` inert |
| `kAluminumFoil` `:280-300` | polishedbackpainted / unified | surface RINDEX=1.46, R, spike=1 | `:456-459`, backing `:1310-1348` | Fresnel/TIR + specular mirror backing R |
| `kGlossyPaper` `:308-328` | polishedbackpainted / unified | surface RINDEX=1.0003, R, spike=1 | same as Al | lossier mirror backing (paper: ≈Al) |
| `kTeflon` `:334-354` | groundbackpainted / unified | surface RINDEX=1.0003, R, all consts 0 | backing `:1310-1348` → Lambertian | diffuse backing |
| `fHolderSurface` `:397-410` | dielectric_dielectric / polished / unified | R=T=0 | `:460-489` → `DoAbsorption` | absorber (photon killed) |
| `fCouplingSurface` `:362-366` | dielectric_dielectric / polished / unified | (none; material RINDEXs) | `:427-454`, Fresnel `:1174-1294` | Fresnel/TIR |
| bare slab↔air | (no surface) | material RINDEXs | `:308-314`, `:460-489` | Fresnel/TIR |
| `fFibreEndMirror` `:1055-1067` | dielectric_metal / polished / unified | R=0.90, EFF=0 | `DielectricMetal` `:724-817` | specular mirror, no detection |
| `fClampMirror` `:1029-1042` | dielectric_metal / polished / unified | R=0.95, EFF=0 | `DielectricMetal` `:724-817` | specular mirror, no detection |

---

## 4. Report-text audit (`report/matrix_report.tex`, boundary subsection)

| Report claim | Location | Verdict | Detail |
|---|---|---|---|
| border surface consulted before skin; fallback to daughter/mother skin | `:396-406` | CONSISTENT | `:319-347`; "daughter/mother" phrasing is loose but functionally right |
| "no surface object → lossless TIR via `DielectricDielectric()`" | `:404-405` | CONSISTENT (wording) | `:460-489`; it is energy-conserving Fresnel **+** TIR, not TIR-only |
| "Every material ... carries an explicit RINDEX table" | `:408-411` | CONSISTENT | material-1 always; material-2 for polished/ground; backing via surface MPT |
| unified constants f_ss/f_sl/f_bs; "surviving probability `1-f_ss-f_sl-f_bs` is shared between diffuse (Lambertian) and bare reflection" | `:413-420` | **INCONSISTENT (minor)** | `ChooseReflection` (`hh:281-301`) gives the entire remainder to `LambertianReflection`; there is no separate "bare reflection" bucket. Reword to "…is assigned to Lambertian reflection". |
| polishedbackpainted/groundbackpainted use backing REFLECTIVITY | `:456-462` | CONSISTENT | `:456-459`, `:1310-1348` |
| groundfrontpainted rand-vs-R (no TIR), remainder Lambertian | `:462-488` | CONSISTENT | `:460-489`, `:481` |
| photodetection path / `SetBoundaryInvokeSD(true)` | `:470-504` | CONSISTENT | `:724-817`, `:559-560`, `:1498-1508`; `PhysicsList.cc:158` |
| line refs `DoAbsorption` 305–320, `NoRINDEX` 436–453 | `:...` | trivial | header `hh:303-322`; source `:427-454` (NoRINDEX `:448`) — off by a couple of lines |

**Bare-absorber bullet (`:447-450`)** — see D1: the report says the bare plate carries a skin
with `R=T=0` **and** the holder shell does; the code places the absorber skin **only on the
holder** (`GeometryMuonScint.cc:1275-1290`, guarded by `UsesBlackHolder()`). **INCONSISTENT.**

---

## 5. Prioritized "Top discrepancies"

1. **D1 (report, medium) — bare-absorber origin.** `matrix_report.tex:447-450` attributes a
   `R=T=0` skin to the *plate* in the bare configuration. The code never builds such a slab skin:
   `fReflSurface` is only placed when `fWrapWithReflector && (fReflectorType != kNoReflector ||
   kStudyBareRoughness>0)` (`:1275-1277`), and the absorber skin is on `fHolderLV`
   (`:1283-1290`). Light is killed at the holder wall after crossing the real 1.2 mm air gap.
   **Fix:** describe the holder-shell absorber only; the bare plate keeps the un-coated
   Fresnel/TIR boundary.
2. **D2 (code latent bug, medium) — the bare-roughness branch cannot roughen.**
   `kNoReflector`+`kStudyBareRoughness>0` builds `ground`+`unified`+`SPECULARSPIKECONSTANT=1`
   (`:225-237`). `ChooseReflection` then forces `SpikeReflection`, resetting the facet normal to
   global (`:1154-1155`, `:1124-1130`), so `SetSigmaAlpha(kStudyBareRoughness)` is never used.
   **Fix:** set the spike constant to 0 (or pick a Lambertian/lobe remainder) if roughness is
   ever to be studied. Currently dormant (`kStudyBareRoughness=0`).
3. **D3 (AGENTS claim, medium) — "`sigma_alpha` on `groundbackpainted` has no effect on TIR".**
   **REFUTED / UNSUPPORTED-BY-SOURCE** as a general rule. `groundbackpainted` reaches
   `DielectricDielectric()`; there `fFacetNormal = GetFacetNormal(...)` (sigma_alpha) is used to
   compute the incidence cosine and hence the Snell/TIR test (`:1124-1137`, `:1146-1172`). With
   `SPECULARLOBECONSTANT>0` the smeared normal also sets the lobe reflection direction. The
   statement is only *correct for our Teflon setting* (all unified constants 0 → Lambertian,
   which ignores the facet normal). **Fix:** qualify the AGENTS sentence to "…only matters when a
   specular lobe is selected (or for the TIR test via the smeared normal); with all unified
   constants zero the reflection is pure Lambertian and the facet normal is not used."
4. **D4 (AGENTS claim, low) — "`RINDEX` on both materials is mandatory".** **PARTIALLY CONFIRMED.**
   Material-1 `RINDEX` is always mandatory (`:288-306`), but material-2 is required only for
   polished/ground `dielectric_dielectric` (`:427-454`); backpainted finishes instead require the
   **surface** backing `RINDEX` (`:356-374`), and `groundfrontpainted`/`polishedfrontpainted`
   need neither material index. **Fix:** state the per-type condition.
5. **D5 (report/code comment, low) — TiO₂ `SetSigmaAlpha(0.1)` is inert.**
   `groundfrontpainted` never calls `GetFacetNormal` (`:460-489`); the "~5.7° paint texture"
   comment (`:249`) implies an effect that the process does not have. **Fix:** drop the constant
   or correct the comment.
6. **D6 (report wording, low) — D1-related & "shared between diffuse and bare reflection".**
   See §4 row and D1; both are wording/accuracy fixes, not modelling errors.
7. **D7 (report, cosmetic) — a few source line references are off by 1–10 lines**
   (`DoAbsorption` header `hh:303-322` vs. quoted 305–320; `NoRINDEX` `:448`/`:427-454` vs.
   quoted 436–453). No behavioural impact.

**No INCONSISTENT *behaviour* was found in the geometry itself** — every surface is a valid
Geant4 option combination and produces the intended optical response. The discrepancies are one
report misstatement (D1), one dormant experimental branch (D2), and two over/precise
AGENTS.md generalisations (D3, D4).

---

## 6. AGENTS.md ground-truth sentences — confirm / refute

Sentence by sentence from the *"G4OpBoundaryProcess ground truth"* paragraph:

| # | AGENTS statement | Verdict | Source / caveat |
|---|---|---|---|
| S1 | "a `G4LogicalBorderSurface` overrides a `G4LogicalSkinSurface` at the same boundary" | **CONFIRMED** | `:319-347` border lookup precedes skin |
| S2 | "`RINDEX` on both materials is mandatory (else `NoRINDEX` kill)" | **PARTIALLY CONFIRMED** | material-1 always (`:288-306`); material-2 only polished/ground dielectric-dielectric (`:427-454`); backpainted uses surface backing `RINDEX` (`:356-374`); front-painted needs neither → see D4 |
| S3 | "`polished`/`ground` dielectric_dielectric → Fresnel/TIR" | **CONFIRMED** | `:427-454`, Fresnel `:1174-1294`, TIR `:1146-1172` |
| S4 | "`groundfrontpainted` → rand-vs-R (Lambertian, no TIR)" | **CONFIRMED** | `:460-489`, Lambertian `:481` |
| S5 | "`polishedbackpainted`/`groundbackpainted` → dielectric + backing REFLECTIVITY" | **CONFIRMED** | `:456-459`, backing `:1310-1348` |
| S6 | "no surface object → lossless TIR default" | **CONFIRMED (nuance)** | `:308-314` `glisur/polished` → `DielectricDielectric()`; energy-conserving Fresnel **+** TIR, not TIR-only |
| S7 | "`sigma_alpha` on `groundbackpainted` has no effect on TIR" | **REFUTED** | `:1124-1137` feed the smeared normal into the TIR test; only true when all unified constants are 0 (our Teflon) → see D3 |
| S8 | "A `NoRINDEX` kill is silent (prints only at verbose>1, `G4OpBoundaryProcess.cc:448`)" | **CONFIRMED** | `:448` NoRINDEX with `if(verboseLevel>1)` guard `:449`; material-1 case `:300-301` likewise |
| S9 | "count it with `/process/optical/boundary/verbose 2`" | **CONFIRMED** | `G4OpticalParametersMessenger.cc:75,244-250` (range 0–2); note it prints *all* statuses, not only `NoRINDEX` |

Additional AGENTS/WLS-section claim checked and **CONFIRMED**: the fibre far-end mirror fix is
source-correct — `DielectricMetal()` uses the surface `REFLECTIVITY` and never needs the metal
`RINDEX` (`:724-817`), and a cap-LV skin covers the previously-uncovered core→cap crossing while
`(clad,cap)` borders keep precedence (`:319-347`). The earlier `NoRINDEX` core→cap kill is
explained by `:288-306` (G4_Al has no `RINDEX`).

---

## 7. Method / limitations

- All Geant4 statements read directly from the v11.3.2 tree under `.refs/source/`; quotes were
  taken from `G4OpBoundaryProcess.cc` (1831 lines) and the header, not from memory.
- No run was performed; conclusions are static-source only. A targeted runtime confirmation of
  D3 (e.g. two `groundbackpainted`+`sigma_alpha` runs with a non-zero lobe fraction) would
  settle it empirically, but the source is unambiguous about `GetFacetNormal` entering the TIR
  test.
- `IsLocked()` (`G4OpticalParameters.cc:551-557`) returns false in `PreInit/Init/Idle` on the
  master thread, so `PhysicsList.cc:158` `SetBoundaryInvokeSD(true)` is effective.
