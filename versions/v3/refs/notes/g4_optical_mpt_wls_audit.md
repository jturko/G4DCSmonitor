# Geant4 optical MPT conventions & WLS-fibre model — consistency audit

Read-only audit. Repo: `/home/turko46/geant4-sims/G4DCSmonitor`.
Date: 2026-10-15. No file under `.refs/` or any source file was modified; the only
file written is this note.

**Reference tree:** `.refs/source` → Geant4 **11.3.2**
(`.refs/include/G4Version.hh:46` `#define G4VERSION_NUMBER 1132`,
`:71` `$Name: geant4-11-03-patch-02`). Every Geant4 claim below is quoted from this
tree, not from memory.

**Method:** read the optical-process sources (`G4OpWLS.cc`, `G4OpWLS2.cc`,
`G4OpBoundaryProcess.cc`, the two `G4WLSTimeGeneratorProfile*`), the MPT plumbing
(`G4MaterialPropertiesTable.cc`, `G4MaterialPropertiesIndex.hh`,
`G4OpticalParameters.cc`), the official example
`.refs/examples/extended/optical/wls/src/WLSMaterials.cc`, and cross-checked against
our `src/GeometryMuonScint.cc`, `src/PhysicsList.cc`, `src/DetectorConstruction.cc`
and `report/matrix_report.tex`. Grep of the full 11.3.2 source **and** install trees
for `WSL01` / `WLS01` returns nothing (see §5).

---

## 1. MPT conventions (grid, keys, mandatory RINDEX)

| # | Convention | Our usage (file:line) | Geant4 requirement / example (quote) | Verdict | Notes |
|---|---|---|---|---|---|
| C1 | Energy grid **ascending** in eV | All six tables are ascending: scint 14-pt `GeometryMuonScint.cc:73-88`; reflector 10-pt `:191-202`; SiPM 11-pt `:381-382`; WLS core 13-pt `:799-801`; clad/grease/air/SiPM/holder/clamp 2-pt `:559,:579,:847,:1015,:139(DC)` | `G4MaterialPropertiesTable.cc:318-326` — "G4PhysicsVector assumes energies are in increasing order" … `mat215` **FatalException** if `E[i+1] < E[i]`; same for the vector overload `:378-388` (`mat216`). Example uses a strictly ascending 50-pt grid `WLSMaterials.cc:232-239`. | **CONSISTENT** | No table would throw `mat215/mat216`. |
| C2 | Equal #energies/#values | Every `AddProperty(k, e, v, N)` uses one `N` for both. | `G4MaterialPropertiesTable.cc:303-308` `mat204` Fatal on size mismatch. | **CONSISTENT** | — |
| C3 | `RINDEX` mandatory on **both** sides of a dielectric boundary | Slab n=1.58 (`:142`), air n=1.0003 (`DetectorConstruction.cc:137-145`), grease n=1.46 (`:563`), SiPM epoxy n=1.55 (`:582`), WLS core 1.59 (`:830`), clad 1.49 (`:851`), holder 1.55 (`:648`), clamp black 1.55 (`:1019`). `G4_Al` mirror cap carries **no** RINDEX (`:1051`). | `G4OpBoundaryProcess.cc:288-306` reads `MPT->GetProperty(kRINDEX)` on material1 and `:436-453` on material2; if absent → `fStatus = NoRINDEX; … fStopAndKill` ("hash" kill, silent unless verbose>1). | **CONSISTENT where it matters** | Every *dielectric_dielectric* pair has both RINDEX. The two no-RINDEX volumes (`G4_Al` cap, and the SiPM `dielectric_metal`) are handled by **border/skin optical surfaces** (`:1216-1224`, `:1242-1245`), for which RINDEX is not consulted. Correct. |
| C4 | RINDEX>0 (GROUPVEL) | All RINDEX grids start at 1.5–1.9 eV (>0). | `G4MaterialPropertiesTable.cc:583-586` `mat211` Fatal if `RINDEX` E0≤0; `:595-597` `mat212` if E1≤0. | **CONSISTENT** | — |
| C5 | Property key spellings | `RINDEX, ABSLENGTH, SCINTILLATIONCOMPONENT1, SCINTILLATIONYIELD, SCINTILLATIONTIMECONSTANT1, SCINTILLATIONRISETIME1, SCINTILLATIONYIELD1, RESOLUTIONSCALE, EFFICIENCY, REFLECTIVITY, TRANSMITTANCE, WLSABSLENGTH, WLSCOMPONENT, WLSTIMECONSTANT` | Exact strings in the key map `G4MaterialPropertiesTable.cc:67-172` (`RINDEX:67`, `REFLECTIVITY:68`, `EFFICIENCY:71`, `TRANSMITTANCE:72`, `WLSCOMPONENT:79`, `WLSABSLENGTH:80`, `SCINTILLATIONCOMPONENT1:90`; const: `WLSMEANNUMBERPHOTONS:101`, `WLSTIMECONSTANT:102`, `RESOLUTIONSCALE:109`, `SCINTILLATIONTIMECONSTANT1:128`, `SCINTILLATIONRISETIME1:131`, `SCINTILLATIONYIELD1:134`). | **CONSISTENT** | Every key we set is a registered name; no typo would silently no-op. |
| C6 | `EFFICIENCY`/`REFLECTIVITY` on a `dielectric_metal` photocathode | `fSiPMSurface` PDE + `REFLECTIVITY=0` (`:375-390`) | `G4OpBoundaryProcess.cc:724-760` (`DielectricMetal`) / `DoAbsorption` applies the Bernoulli `fEfficiency` draw. | **CONSISTENT** | Matches the official OpNovice2 photodetector idiom. |
| C7 | `REFLECTIVITY`/`TRANSMITTANCE` on a **plain polished `dielectric_dielectric`** boundary | `fHolderSurface` sets both to 0 (`:397-409`) intending absorption, and the slab skin uses the same for bare. | These constants are only consumed when the finish is `polishedbackpainted`/`groundbackpainted`: `G4OpBoundaryProcess.cc:1310-1319`. For `polished` `dielectric_dielectric` the branch is pure Fresnel/TIR (`:1076-1308`) and **ignores** them. | **RISKY (documentation/physics)** | The holder still absorbs because its *bulk* `ABSLENGTH=1 µm` (`:646`) kills the refracted photon, so the outcome is right — but the report's mechanism ("a skin with R=T=0 on the plate … models the frustrated-TIR loss", `matrix_report.tex:447-449`) is not how the boundary process works. No crash, no corruption. |

---

## 2. Scintillator property table (`BuildOpticalProperties`, `GeometryMuonScint.cc:51-178`)

- **(a)** usage: RINDEX `:91-94/:142`, ABSLENGTH `:100-117/:143`,
  SCINTILLATIONCOMPONENT1 `:123-138/:144`, `SCINTILLATIONYIELD=10000/MeV :149`,
  `RESOLUTIONSCALE=1 :152`, `SCINTILLATIONTIMECONSTANT1=2.1 ns :158`,
  `SCINTILLATIONRISETIME1=0.9 ns :162`, `SCINTILLATIONYIELD1=1 :166`, Birks
  `:172`.
- **(b)** Geant4: `G4Scintillation.cc:224` reads `SCINTILLATIONCOMPONENT1`;
  `:375-385` picks the component and reads `RESOLUTIONSCALE`; `:404-415`
  `SCINTILLATIONYIELD` / `SCINTILLATIONYIELD1`; `:478-484`
  `SCINTILLATIONTIMECONSTANT1` / `SCINTILLATIONRISETIME1`. All keys valid per C5.
- **(c) CONSISTENT.** Single-fast-component model; when
  `SetScintByParticleType(false)` (`PhysicsList.cc:146`) the component-1 + yield-1
  path is exactly the used one.
- **(d)** `SCINTILLATIONYIELD1=1.0` is redundant with the default but harmless.

---

## 3. Reflector / coupling / holder surfaces (`BuildOpticalSurfaces`, `:184-411`)

| Finish | Surface type | MPT constants | Verdict | Note |
|---|---|---|---|---|
| bare `kNoReflector` `:225-237` | none (no skin) / optional `ground` study | `SPECULARSPIKECONSTANT` only | **CONSISTENT** | No surface → `DielectricDielectric()` Fresnel/TIR default; RINDEX present on both media. |
| `kPaintTiO2` `:246-263` | `dielectric_dielectric`, `groundfrontpainted` | `REFLECTIVITY` (0.955–0.983) `:257` | **CONSISTENT** | `groundfrontpainted` uses rand-vs-R (`G4OpBoundaryProcess.cc` unified `Select/DoAbsorption` path). |
| `kAluminumFoil` `:280-300` | `polishedbackpainted` | `REFLECTIVITY` + backing `RINDEX=1.46` + spike/lobe/backscatter `:293-297` | **CONSISTENT** | `polishedbackpainted` reads RINDEX2 as backing index (`:456`+). |
| `kGlossyPaper` `:308-328` | `polishedbackpainted` | R + `RINDEX=1.0003` + spike `:321-325` | **CONSISTENT** | — |
| `kTeflon` `:334-354` | `groundbackpainted` | R + `RINDEX=1.0003` | **CONSISTENT** | — |
| coupling `fCouplingSurface` `:362-366` | `dielectric_dielectric`, `polished`, **no MPT** | — | **CONSISTENT** | Fresnel from material RINDEX; comment `:366` correct. |
| SiPM `fSiPMSurface` `:375-390` | `dielectric_metal` | `EFFICIENCY`+`REFLECTIVITY` | **CONSISTENT** | — |
| holder `fHolderSurface` `:397-409` | `dielectric_dielectric`, `polished` | `REFLECTIVITY=TRANSMITTANCE=0` | **RISKY** | See C7 — constants ignored for a plain polished boundary. |

The report's universal claim "Every material in the model therefore carries an
explicit `RINDEX` table" (`matrix_report.tex:410-411`) is **overstated**: the
`G4_Al` mirror cap (`:1051`) has no RINDEX (and needs none because it is viewed
through a `dielectric_metal` surface). **Documentation**, not physics.

---

## 4. WLS fibre material table (`BuildWLSFibres`, `:784-1076`)

- **(a)** WLS core = polystyrene C8H8, `:793-795`; grid `e[13]` = 1.907 … 3.263 eV
  ascending `:799-801`; `RINDEX=1.59 :802-803`; bulk `ABSLENGTH=5 m :806-808`;
  `WLSABSLENGTH :817-821`; `WLSCOMPONENT :825-828`; `WLSTIMECONSTANT=7 ns :834`
  (all added `:830-834`). Clad PMMA `:841-853` (`RINDEX=1.49`, `ABSLENGTH=20 m`).
  Clamp black `:1010-1021`; clamp mirror `dielectric_metal R=0.95 :1035-1040`;
  far-end cap mirror `dielectric_metal R=0.90 :1061-1066`.
- **(b)** Geant4 reads exactly these: `G4OpWLS.cc:253` `GetProperty(kWLSCOMPONENT)`;
  `:304` `GetProperty(kWLSABSLENGTH)`; `:132` `GetConstProperty(kWLSTIMECONSTANT)`.
  Example sets `WLSABSLENGTH`, `WLSCOMPONENT`, `WLSTIMECONSTANT` on the fibre core
  (`WLSMaterials.cc:277-281`).
- **(c) CONSISTENT** that we use the documented keys, a single-component WLS, and a
  common 13-pt grid for all four vectors (not required, but tidy and matches the
  example's shared grid).
- **(d)** Note only: the report labels `WLSTIMECONSTANT=7 ns` a "decay time"
  (`matrix_report.tex:320`); with the **delta** profile it is a fixed delay (see §6).

### 4.1 WLS time profile vs. Geant4 default

- **(a)** `PhysicsList.cc:154` `optParams->SetWLSTimeProfile("delta")`.
- **(b)** Default is already `"delta"` (`G4OpticalParameters.cc:117`
  `wlsTimeProfileName = "delta"`); `G4OpWLS::UseTimeProfile` `G4OpWLS.cc:314-336`
  constructs `G4WLSTimeGeneratorProfileDelta`, whose `GenerateTime` just returns the
  constant (`G4WLSTimeGeneratorProfileDelta.cc:63-67`). The LXe example does the
  same (`examples/extended/optical/LXe/LXe.cc:67`). The `wls` example leaves it at
  the default (also delta) with `WLSTIMECONSTANT=0.5 ns` (`WLSMaterials.cc:279`).
- **(c) CONSISTENT** with Geant4 default/example.
- **(d)** Physics caveat: "delta" gives a **fixed** 7 ns delay, not an exponential
  decay with τ=7 ns. If timing is studied, switch to `"exponential"`. The report's
  "decay time" wording (`matrix_report.tex:320`) is misleading. **RISKY
  (documentation).**

---

## 5. WLSABSLENGTH / WLSCOMPONENT overlap — would WSL01 trigger? (the key question)

### 5.1 The Geant4 rule, from source

`G4OpWLS::PostStepDoIt` (`G4OpWLS.cc:126-174`):

```
128:  G4double primaryEnergy = aTrack.GetDynamicParticle()->GetKineticEnergy();
...
143:    // Make sure the energy of the secondary is less than that of the primary
144:    for(G4int j = 1; j <= 100; ++j)
145:    {
146:      G4double CIIvalue = G4UniformRand() * CIImax;
147:      sampledEnergy     = WLSIntegral->GetEnergy(CIIvalue);
148:      ...
149:      if(sampledEnergy <= primaryEnergy)
150:        break;
151:    }
152:    // If no such energy can be sampled, return one less secondary, or none
153:    if(sampledEnergy > primaryEnergy)
154:    { ... NumberOfPhotons--; ... }
```

The WLS integral is built **from `WLSCOMPONENT`** in `BuildPhysicsTable`
(`:229-291`), while the *absorption* mean free path comes **from `WLSABSLENGTH`**
in `GetMeanFreePath` (`:294-311`). The failure condition is therefore:

> A photon is absorbed (WLSABSLENGTH finite) at an energy **below the whole support
> of WLSCOMPONENT** → no emitted energy ≤ Eprimary can be drawn → after 100 tries
> the secondary is dropped.

**WSL01 note:** a grep for `WSL01`/`WLS01` over the entire 11.3.2 source *and*
install trees returns **nothing**. In **this** reference build there is no fatal;
`:153-173` merely drops the photon (`verboseLevel>1` prints "One less WLS photon …").
`G4OpWLS2.cc:147-186` is identical. So the `AGENTS.md` statement that "newer Geant4
raises the fatal `WSL01` exception" cannot be verified here and does not apply to the
build the code is actually compiled against. The analysis below is nonetheless framed
as if such a fatal existed, because the safe-design rule is the same.

### 5.2 Our tables

| Energy (eV) | λ (nm) | `WLSABSLENGTH` | `WLSCOMPONENT` |
|---|---|---|---|
| 1.907 | 650 | 1×10⁶ m | 0.00 |
| 2.066 | 600 | 1×10⁶ m | 0.03 |
| 2.138 | 580 | 1×10⁶ m | 0.08 |
| 2.214 | 560 | 1×10⁶ m | 0.18 |
| 2.296 | 540 | 1×10⁶ m | 0.40 |
| 2.385 | 520 | 1×10⁶ m | 0.70 |
| 2.480 | 500 | 1×10⁶ m | 0.95 |
| **2.583** | **480** | **0.60 m** | **1.00** |
| 2.695 | 460 | 0.20 m | 0.55 |
| 2.818 | 440 | 0.05 m | 0.00 |
| 2.952 | 420 | 0.005 m | 0.00 |
| 3.100 | 400 | 0.0005 m | 0.00 |
| 3.263 | 380 | 0.00005 m | 0.00 |

(`GeometryMuonScint.cc:817-828`.)

**Absorption turns finite only at 2.583 eV.** Below it `WLSABSLENGTH` is 1×10⁶ m
(and between 2.480 and 2.583 it interpolates linearly from 10⁶ m down to 0.60 m, i.e.
still ≈10⁵–10⁶ m across that 0.1 eV band — no meaningful absorption). The **emission
support starts at 2.066 eV**, i.e. ~0.52 eV *below* the absorption onset. Therefore
every photon that can actually be WLS-absorbed (E ≳ 2.583 eV) has a large, non-zero
`WLSCOMPONENT` weight at lower energy → a valid `sampledEnergy ≤ Eprimary` always
exists.

**Verdict: the energy-overlap rule is SATISFIED; `WSL01` would NOT trigger.**

Numbers: trapezoidal integrals of `WLSCOMPONENT` give
∫(2.066→2.818) ≈ 0.388 and ∫(2.066→2.583) ≈ 0.268, so **≈69 % of the emission lies
below the absorption onset**. At the worst case (primary = 2.583 eV, exactly the
onset/peak) ≈31 % of single draws exceed the primary, so the chance that all 100
retry draws fail is ≈0.31¹⁰⁰ ≈ 10⁻⁵¹. Even on a hypothetical build that fatals after
100 failures, this cannot fire.

**Two caveats worth recording (RISKY, style only):**
1. Unlike the official example — where the emission high-energy cutoff sits *exactly*
   at the strong-absorption onset (`WLSCOMPONENT`=0 at 3.05 eV, `WLSABSLENGTH`→1 mm
   at 3.05 eV, `WLSMaterials.cc:264-272`) — our `WLSCOMPONENT` extends ~0.23 eV
   (2.583→2.818) **above** the absorption onset. This does not break the rule (emission
   still reaches far below the onset), but it does cause occasional reject-and-retry
   draws and means the re-emitted light (peak 2.583 eV) sits where `WLSABSLENGTH` is
   only 0.60 m → real **self-absorption** on the re-emission path.
2. `AGENTS.md` states the stricter rule "zero `WLSCOMPONENT` at/above that onset";
   our code does **not** meet that letter. The *actual* Geant4 requirement (emission
   weight below every absorbed energy) is met. A future editor must not "fix" this by
   making `WLSABSLENGTH` finite below the emission edge, nor by extending emission
   below 2.066 eV while keeping absorption transparent there.

The report's physics statement "the shifted emission no longer overlaps the fibre
absorption band" (`matrix_report.tex:462-463`) is **contradicted** by our own tables:
the emission peak (2.583 eV / 480 nm) coincides with the absorption onset, where
`WLSABSLENGTH` = 0.60 m. **Documentation error** (physics still runs).

---

## 6. WLS process wiring (`PhysicsList.cc`)

| Item | Our usage | Geant4 | Verdict | Note |
|---|---|---|---|---|
| OpWLS off by default | `SetProcessActivation("OpWLS", false) :167` | `G4OpticalPhysics::ConstructProcess` reads the flag at build time | **CONSISTENT** | Non-WLS runs pay nothing. |
| OpWLS auto-enable | `:255-258` `if (UsesWLSWorld()) SetProcessActivation("OpWLS", true)` **before** `G4VModularPhysicsList::ConstructProcess()` (`:260`) | Flag must be set before the optical physics constructor runs | **CONSISTENT** | Ordered correctly (geometry built before physics). |
| Time profile | `:154` delta | default delta | **CONSISTENT** | See §4.1 caveat. |
| `SetBoundaryInvokeSD(true)` | `:158` | `G4OpBoundaryProcess.cc:1514`; needed for `Detection` status to hit the SD | **CONSISTENT** | Used by the photocathode PDE path. |

---

## 7. Grids / figures / report cross-check

- All six energy grids are valid ascending eV (§1 C1). The 14-pt scint grid spans
  1.771–4.000 eV (700–310 nm) and the 10-pt reflector grid 1.907–3.444 eV
  (650–360 nm); both bracket the 425 nm scintillation band and the Y-11 bands. Fine.
- Report table `matrix_report.tex:317` says the WLS absorption onset is
  "430–460 nm", but the code onset is **480 nm** (`:820`); the *text* says 480 nm
  (`:366`). **Internal inconsistency.**
- Report emission peak is quoted as 476 nm (`:318`), 480 nm (`:343`), 490 nm
  (`:459`) and "mean ~485 nm" (`:465`); code tabulates 480 nm. **Internal
  inconsistency** (already noted in `sipm_wls_audit.md`).
- The NIST world air is the only material in the sliding chain lacking RINDEX by
  default; `DetectorConstruction.cc:137-145` attaches `RINDEX=1.0003` on a 2-pt
  {1.5, 6.0} eV grid — **CONSISTENT**, guards against a silent `NoRINDEX` at the
  fibre/clamp air annulus and the open faces.

---

## 8. Top discrepancies (prioritized)

1. **WLS time profile semantics (RISKY, documentation).** `WLSTIMECONSTANT=7 ns`
   (`GeometryMuonScint.cc:834`) is applied by the **delta** generator as a *fixed*
   delay (`G4WLSTimeGeneratorProfileDelta.cc:63-67`), not an exponential decay. The
   report calls it a "decay time" (`matrix_report.tex:320`). Geant4-correct; physics
   wording wrong. Fix: say "fixed 7 ns WLS delay (delta profile)" or use
   `"exponential"`.
2. **`WLSCOMPONENT` overlaps the absorption band (RISKY, style).** Emission extends
   ~0.23 eV above the `WLSABSLENGTH` onset (`:817-828`), causing retry draws and
   genuine self-absorption; the report explicitly claims there is *no* overlap
   (`matrix_report.tex:462-463`). Does **not** violate the WSL01 rule (see §5).
   Fix: either align the emission cutoff with the onset (matching `WLSMaterials.cc`)
   or correct the report text.
3. **Holder absorber mechanism mis-described (RISKY, documentation).** For a
   `polished` `dielectric_dielectric` boundary, `REFLECTIVITY`/`TRANSMITTANCE` are
   ignored (`G4OpBoundaryProcess.cc:1310-1319`); absorption actually comes from the
   holder's bulk `ABSLENGTH=1 µm` (`:646`). Report claim `matrix_report.tex:447-449`
   overstates the surface mechanism.
4. **"Every material carries explicit RINDEX" is false (minor documentation).** The
   `G4_Al` mirror cap (`GeometryMuonScint.cc:1051`) has no RINDEX; it is safe only
   because it is viewed through a `dielectric_metal` surface (`:1216-1224`). Report
   `matrix_report.tex:410-411`.
5. **WLS absorption/emission λ labels inconsistent in the report (documentation).**
   Onset "430–460 nm" (`:317`) vs code 480 nm; emission 476/480/490/485 nm
   (`:318/:343/:459/:465`) vs code 480 nm.

**No INCONSISTENT or crash/corruption-class findings** were found in the MPT
conventions or the WLS model. The grid, key-name, mandatory-RINDEX, and
energy-overlap requirements are all met; the WSL01 risk is **effectively zero** for
these tables in Geant4 11.3.2 (and, by the same argument, on any build that fatals
only after 100 failed samples).

### Verdict count
- **CONSISTENT:** C1–C6, scintillator table, all reflector/coupling/SiPM surfaces,
  WLS keys/grid, OpWLS wiring, world-air RINDEX (≈18 items).
- **RISKY:** C7 (holder surface mechanism), delta-profile "decay" wording,
  `WLSCOMPONENT`/`WLSABSLENGTH` band overlap + report text (4).
- **INCONSISTENT (physics/crash):** 0.
- **WSL01 verdict:** will **not** trigger.
