# Large-scale fidelity audit — aggregated summary

Date: 2026-10-15. Scope: the simulation code + `report/matrix_report.tex` audited
against (A) the downloaded papers/datasheets in `report/refs/`, and (B) the
read-only Geant4 v11.3.2 tree in `.refs/`. All audits were read-only; only files
under `report/refs/notes/` were written.

Per-source notes:

| Note | Covers |
|---|---|
| `rao2025_audit.md` | Primary paper vs code/report (66 claims) |
| `scintillator_materials_audit.md` | EljenEJ200, AmcrysPS, Birks1951 |
| `sipm_wls_audit.md` | AdvanSiD NUV SiPM, Kuraray Y-11 |
| `context_papers_audit.md` | Gundacker2020, Bonechi2020, Korpachev2017, Alekseev2022, Anfimov2021 |
| `geant4_citations_audit.md` | Agostinelli2003, Allison2016 |
| `g4_boundary_process_audit.md` | G4OpBoundaryProcess / surface semantics |
| `g4_optical_mpt_wls_audit.md` | MPT conventions, G4OpWLS, examples/optical/wls |
| `g4_geometry_solids_audit.md` | G4TessellatedSolid, booleans, containment, skins/borders |
| `g4_physics_biasing_mt_audit.md` | Physics list, optical activation, importance biasing, MT |

---

## 1. Headline findings (highest priority)

**H1. Config 4 result is inverted vs the paper.** Rao2025 (abstract p.1, Conclusion
p.11) makes the single-SiPM + WLS design the *most* efficient, position-independent
collector (DE up to 90 %); our report gives it the **lowest** efficiency and the
largest position contrast (`matrix_report.tex:641,669`; `κ₂D=138`;
`src/GeometryMuonScint.cc:1524-1534`). Known caveat; still the single largest
result-level disagreement.

**H2. The report's bare-plate mechanism contradicts the code.** The report says the
bare plate has a "perfect-absorber skin (R=T=0)" / "effective optical contact /
frustrated TIR" (`matrix_report.tex:207-216`, `:447-450`). The code applies the
absorber skin **only on the holder LV** (`GeometryMuonScint.cc:1283-1290`) and leaves
the plate as a polished scint↔air Fresnel/TIR boundary across a **real 1.2 mm air
gap** (`:1275`). Confirmed independently by the boundary-process and optical-MPT
audits. Related: for a plain `polished dielectric_dielectric` boundary the surface
`REFLECTIVITY/TRANSMITTANCE` are ignored — absorption actually comes from the holder
**bulk** `ABSLENGTH = 1 µm`. Stale code comment at `GeometryMuonScint.cc:626-629`.

**H3. Configs 3 & 4 are collocated.** Rao Table IV: config 3 = T1,T2 (top), config 4
= L1 (left) — **adjacent** faces. Code puts both on **−X**
(`GeometryMuonScint.cc:1508-1514`, `:1532`); the report labels both the top edge.

**H4. Two citations are overstated.** `Anfimov2021` is cited for "(which drive the
PDE)" but its headline result is *"no significant temperature dependence of the PDE"*;
`Korpachev2017` is cited for an "analytic-intractability" claim absent from the paper.

**H5. Several used parameter values are unsourced by the referenced files.**
Density 1.032 g/cm³ (Geant4 NIST, not Eljen 1.023 / Amcrys 1.06); Birks 0.126 mm/MeV
(Craun1970 absent; Birks1951 is anthracene law only); the emission-spectrum shape /
FWHM cut-offs; "UPS-923A" 9,500 ph/MeV & 3.3 ns. The report describes the material as
"built from EJ-200 **and** UPS-923A", but in practice every adopted value is EJ-200.

**H6. WLS model/report mismatches.** Code emission peak is 480 nm while the report
quotes 476/480/490/485 nm in different places and the datasheet/Rao value is 476 nm;
code absorption onset is 480 nm while the report table says "430–460 nm"; the report
claims the shifted emission "no longer overlaps the fibre absorption band" but our own
tables overlap (self-absorption); `WLSTIMECONSTANT = 7 ns` under the `delta` profile is
a **fixed delay**, not a decay time.

---

## 2. Track A — code/report vs papers & datasheets

**Rao2025** — 35 MATCH / 15 MISMATCH / 9 UNSUPPORTED / 7 ABSENT (66 claims). Faithful:
plate dims/material class, SiPM footprint + 43 % PDE, all 4 reflectors and the
`none ≪ TiO₂ < Al ≈ glossy` ordering, WLS-clamp concept, 50×50 mm² MCD regions,
configs 1–2 layouts. Beyond H1–H5: corner is **lowest** in the paper for all configs,
but the report's `tab:positions` has corner **highest** for bare and TiO₂. Electrical/DAQ
results (thresholds, dark counts, DE curves) are absent by design.

**Scintillator datasheets** — MATCH 6, MISMATCH 1 (density), UNSUPPORTED 3 (spectrum
shape, Birks value, "UPS-923A" annotations). The 2.5 m attenuation point at the 425 nm
emission peak + UV fall-off is a deliberate but unsourced model choice (datasheet gives a
single 3.8 m); the report calls 3.8 m the "peak". Birks **is** functionally active.

**SiPM/WLS datasheets** — Y-11 emission peak 480 vs 476 nm; absorption has no 430 nm
peak (monotonic to UV, onset ≈480 nm); SiPM PDE peak red-shifted to 425/450 nm vs
datasheet 420 nm, range truncated at 600 nm vs 350–900 nm; report gain 10⁶ vs spec
3.6×10⁶; epoxy n 1.55 vs 1.5115; cladding: Kuraray Y-11(200) is **single-clad** while
Rao says multi-clad (code follows the datasheet). Kuraray spectra are figures with no
text layer, so those numbers trace to Rao2025, not the datasheet.

**Context papers** — Gundacker2020 & Bonechi2020 SUPPORTED (metadata MATCH);
Alekseev2022 SUPPORTED (Y-11 1 mm reference); Korpachev2017 OVERSTATED;
Anfimov2021 OVERSTATED (see H4). No metadata MISMATCH; Anfimov/Alekseev cite final
journal numbers not printed on the arXiv copies supplied.

**Geant4 citations** — both metadata-correct; only one citation site (`:82`), generic.
Low-priority: `v11.3.2` not documented by either paper; multithreading (`:58`,`:1259`)
and optical physics (`:377`) are missed-citation opportunities for Allison2016 §2 / §4.1.9.
`Agostinelli2003.pdf` is the 86-page preprint with non-extractable body text.

---

## 3. Track B — code vs Geant4 v11.3.2 source/examples

**Boundary process (9/9 surface behaviours CONSISTENT).** The SiPM
`dielectric_metal` PDE→`Detection`→`SetBoundaryInvokeSD(true)` chain is source-correct.
AGENTS.md ground-truth sentences: S1 border>skin ✅, S3 ✅, S4 ✅, S5 ✅, S6 ✅ (nuance:
Fresnel+TIR, not TIR-only), S8 silent NoRINDEX ✅, S9 verbose ✅; **S2 partially** (RINDEX
mandatory per-type, not always both materials); **S7 REFUTED** ("`sigma_alpha` on
`groundbackpainted` has no effect on TIR" — the smeared normal enters the TIR test; only
true when all unified constants are 0 as in our Teflon). Also: the `kNoReflector`
bare-roughness study is a **no-op** (`SPECULARSPIKECONSTANT=1` forces `SpikeReflection`,
so `sigma_alpha` is never used); TiO₂ `SetSigmaAlpha(0.1)` is inert for
`groundfrontpainted`.

**Optical MPT / WLS (≈18 CONSISTENT, 4 RISKY, 0 crash/corrupt).** All grids ascending,
key spellings valid, mandatory RINDEX present. **WSL01 will NOT trigger**: the v11.3.2
source/install trees contain no `WSL01`/`WLS01` fatal at all (G4OpWLS just drops the
photon after 100 retries), and our tables satisfy the actual rule (~69 % of emission
weight lies below the absorption onset; failure probability ≈10⁻⁵¹). RISKY/documentation:
`delta`-profile "decay"; emission extending ~0.23 eV above the absorption onset (unlike
`WLSMaterials.cc`) with report text claiming no overlap; holder `R=T=0` ignored for a
polished dielectric boundary; report's "every material carries explicit RINDEX" is false
(`G4_Al` cap relies on its `dielectric_metal` skin).

**Geometry/solids.** `G4TessellatedSolid` swept tubes: CONSISTENT (watertight, outward,
non-degenerate; should pass `CheckStructure`). Boolean cutters: CONSISTENT (film groove
clears by 0.05 mm; `fChannelRadius` variant correctly avoided as exactly tangent; 10 mm
axial overshoot). World-sibling placement + border-over-skin precedence: CONSISTENT.
FRAGILE: coincident core↔clad and fibre↔clad↔cap end planes (not caught by the checks
since core placement uses `pSurfChk=false`). AGENTS.md "channels must not overlap" is
straight-run-only (they merge into a slot over the last ~15 mm by design). No runtime
`GeomSolids1001`/`GeomVol1002` evidence captured.

**Physics/biasing/MT.** Importance biasing is **dead code** (parallel world +
`G4ParallelWorldPhysics`/`G4ImportanceBiasing` all commented; samplers always null;
`/dcs-monitor/det/useBiasing` has no consumer). Optical activation sequence correct;
WLS auto-enable runs at `G4State_Init` (works via `IsLocked()`, but outside the documented
PreInit-only contract) → RISKY. `thread_local`/`G4ThreadLocal` usage safe; SDs per-worker;
`G4AnalysisManager` thread-local. `RootManager` sampling is dead (retained only for
`fileNum`). Misleading comment: post-step ordering 9900 is invoked **last**, not ahead of
`OpBoundary`. Revived biasing should follow the B02 registration order.

---

## 4. Reconciliation actions (not yet applied)

**Report (`matrix_report.tex`) — factual fixes:**
- Bare-plate mechanism (`:207-216`, `:447-450`): describe the holder-shell absorber over a
  real air gap; drop "effective optical contact / frustrated TIR / skin on the plate".
- Al reflectivity table (`:201`): `0.84–0.91` → `0.904–0.925`.
- WLS: reconcile emission-peak λ (one value, 476 or 480) and absorption onset
  ("430–460 nm" → 480 nm); drop "no overlap" claim; relabel 7 ns as a fixed delay.
- Configs 3 vs 4 face placement (`:147-148`, `:242`) if the geometry is not changed.
- `Anfimov2021` (`:1290-1291`): drop "which drive the PDE" (source says PDE has no
  significant T dependence); `Korpachev2017` (`:80`): reword to a simulation example.
- Provenance: material built from EJ-200 (not "EJ-200 and UPS-923A"); flag density/Birks
  as unsourced or cite Craun1970 explicitly.
- Add Allison2016 §2 cite for the multithreading discussion.

**AGENTS.md — corrections:**
- Refute/qualify the `sigma_alpha`/TIR sentence (S7).
- Qualify "RINDEX on both materials is mandatory" (per-type).
- Note the bare-roughness study is currently a no-op.
- Note holder `R=T=0` is ignored for a polished dielectric boundary (absorption is bulk).
- Note WLS emission overlaps the absorption onset (no WSL01 fatal in v11.3.2) vs the
  current "zero WLSCOMPONENT at/above onset" letter.
- "Channels must not overlap" → straight-run-only.

**Code (optional, separate decision):** fix/remove the bare-roughness branch; decide
revive-vs-delete the importance-biasing scaffolding; correct the `PhysicsList.cc`
ordering comment; consider hardening the coincident core/clad/cap end faces.
