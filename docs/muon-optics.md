# MuonScint optical model (reference)

Read on demand from `AGENTS.md`.

Goal: reproduce Rao et al., J. Appl. Phys. 138, 114504 (2025) — polished 250×250×10 mm³
UPS-92S plate in a black 3D-printed holder, SiPM ASD-NUV4S-P-40 on the side faces.
Target ordering **none < TiO₂ < Al ≈ glossy**; yield centre > edge > corner (paper target —
the v6 simulation reproduces corner-lowest for bare/Al/glossy but **reverses** it for TiO₂ in
the four-sided layouts; see *Beam regions* below and report §9.2).
Work lives in `src/GeometryMuonScint.cc` / `include/GeometryMuonScint.hh`.

**Beam positions (centre/edge/corner)**: the 50×50 mm GPS beam is placed fully *on* the
plate, matching the paper's 50×50 mm² MCD active area. With the slab spanning ±125 mm,
patch centres are `(0,0)`, `(100,0)` (edge), `(100,100)` (corner), i.e. illuminated
regions `[-25,25]²`, `[75,125]×[-25,25]`, `[75,125]²` — equal area. Do **not** revert to
the old `(125,0)`/`(125,125)` centres. Templates: `slurm/muon_scint_template{,_fast}.mac`
and `macros/muon_scint_template.mac`.

**Report frame vs. simulation frame**: the report plan view (`macros/irradiation_sites.C`,
Fig 5) has top=+y, bottom=−y, left=−x, right=+x. `muonEntryX/Y` is recorded in
**slab-local** coordinates with the slab rotation undone (`src/SteppingAction.cc:141-148`),
so a global `/dcs-monitor/det/setRotation` is a **no-op for every output** — orientation can
only be changed by reorienting a sub-assembly *within* the slab. All report macros therefore
map simulation→report with the same 90°-CW transform `(x,y) → (y,−x)`
(`toReportFrame(double&,double&)` in `plot_collection_efficiency.C`, `matrix_positions.C`,
`plot_whole_slab.C`). In this frame the cfg1/2 SiPMs sit on the four edges, and **both** cfg3
and cfg4 fall on the report **top** (`+y`) edge, because both live on the model's single −X
face: cfg3's two SiPMs at `x=±62.5`, cfg4's clamp as a bar along the top with its read-out
SiPM at the top-left corner. This is exactly how report Figs. 1 and 5 draw them
(`macros/sipm_layouts.C`, `macros/irradiation_sites.C`) — verified consistent with the sim.
(Rao's Table IV instead puts cfg3 on the top and cfg4 on the left face — adjacent, not
shared — but we deliberately keep our report's shared-face layout; see the beam-region note
below.)

**Reflector model per finish** (`BuildOpticalSurfaces`), one shared 10-pt 360–650 nm
grid (ascending eV). Reflector and bare-absorber skins are mutually exclusive.
- `kNoReflector` (bare): by default the slab gets **no** wrapping skin (the `MuonScintSkin`
  is placed only when `fWrapWithReflector && (fReflectorType != kNoReflector ||
  kStudyBareRoughness>0 || bareContactSkin)`, `GeometryMuonScint.cc:1275-1277`), so it
  keeps the plain polished scint↔air Fresnel/TIR boundary across a **real 1.2 mm air gap**. The black holder LV (see *Black holder* below)
  carries the `MuonScintHolderAbsorber` skin (`fHolderSurface`, REFLECTIVITY=TRANSMITTANCE=0,
  unified/polished), so light that refracts out of the plate crosses the gap and is killed at
  the wall. The `R=T=0` constants **do** kill it at the surface: `G4OpBoundaryProcess.cc:460-466`
  computes `rand > R + T` for a `polished`/`ground` `dielectric_dielectric` boundary and, with
  R=T=0, absorbs every photon that reaches the holder skin (the Fresnel/TIR branch is only
  reached when `rand <= R`). The holder **bulk** `ABSLENGTH=1 µm` would absorb it too, so the
  outcome is the same either way. `fReflSurface` is also used for the optional bare-contact
  model (`setBareContactFraction f>0`): a polished `dielectric_dielectric` skin with
  `REFLECTIVITY=1-f`, `TRANSMITTANCE=0`, so each surface hit is absorbed with probability
  `f` (holder contact) and otherwise follows the normal Fresnel/TIR path. `f=0` (default)
  creates no skin and reproduces the clean air gap exactly. The roughness branch
  (`kStudyBareRoughness>0`) is now functional: `ground`+`unified` with
  `SPECULARLOBECONSTANT=1`, `SPECULARSPIKECONSTANT=0`, so `sigma_alpha` is applied (dormant
  while `kStudyBareRoughness=0`).
- `kPaintTiO2`: `groundfrontpainted` (Lambertian, **no TIR**), R≈0.955–0.983.
- `kAluminumFoil`: `polishedbackpainted` (TIR + specular mirror, SPECULARSPIKE=1), backing
  RINDEX selectable as 1.46 grease (default) or 1.0003 air via `setAlBacking air|grease`;
  R≈0.904–0.925 (evaporated-Al normal-incidence, McPeak 2015).
- `kGlossyPaper`: `polishedbackpainted` (lossier mirror, R≈0.900–0.955) — paper reports
  Al ≈ glossy and both highest, so glossy is not a diffuser.
- `kTeflon`: kept for completeness, not in the paper.

Current default (f=0, Al grease; 300 evt cfg1 centre, npe/event): bare=305,
TiO₂=122, Al=193, glossy=510 — the report's `TiO₂ < Al < bare < glossy` ordering.
Pilot with `f=0.02` + `setAlBacking air` (300 bare / 100 reflector evt) gives
bare=114, TiO₂=122, Al=473, glossy=510, recovering the paper's
`bare < TiO₂ < Al ≈ glossy` ordering; see *Bare-holder contact / Al backing pilot*
below. Position ordering at config 1 is **not** uniformly centre-dominant: Al/glossy and
bare peak at centre, but TiO₂ peaks at **corner** (see `tab:positions`), reversing the
paper's corner-lowest rule.

**SiPM detection via the boundary process**: grease↔SiPM is `dielectric_metal` with
`EFFICIENCY` = ASD-NUV4S-P-40 PDE(λ), the 25-pt digitized AdvanSiD Fig. 8 curve (6 V OV,
360–750 nm; peak 0.431 @ 410 nm, ~0.30 @ 490 nm, 0.086 @ 750 nm) and `REFLECTIVITY`=0.
`G4OpBoundaryProcess` does the PDE draw, sets status `Detection`, and invokes the SD —
needs **`SetBoundaryInvokeSD(true)`** (`PhysicsList.cc`). `MuonScintSiPMSD::ProcessHits`
therefore fires only for real photoelectrons and kills the photon (no manual draw).

**Black holder** (`BuildHolder`, bare config only): `G4SubtractionSolid` shell
(outer − cavity), `MuonScintHolderBlack` C:H 1.05 g/cm³ n=1.55 ABSLENGTH=1 µm, cavity =
slab + `fHolderGap` 1.2 mm all faces. `UsesBlackHolder()` = bare (`kNoReflector`) **and**
not WLS, so the Config 4 clamp never overlaps the holder (the holder shell is omitted there;
the escaped sub-critical light dies at the world boundary instead, which is optically
equivalent). The absorber skin
(`MuonScintHolderSkin` → `fHolderSurface`) is applied to the **holder LV**, not the slab
(`GeometryMuonScint.cc:1287-1290`); there is no `UsesAbsorberSkin()`.

**Bare-holder contact / Al backing pilot (2026-10-18).** Two pre-init knobs bracket the
as-built surfaces: `/dcs-monitor/det/muonScint/setBareContactFraction <f>` (`0..1`, default
`0`) and `/dcs-monitor/det/muonScint/setAlBacking air|grease` (default `grease`). The
contact model is a mean-field per-hit loss on every bare face (`R=1-f`, `T=0`), gated by
`fWrapWithReflector && UsesBareContactModel()`; `UsesBareContactModel()` =
`kNoReflector && f>0`, i.e. it applies to **every bare layout including Config 4**
(`include/GeometryMuonScint.hh`), whose bare faces (`+X`, `+-Y` outside the read-out pad,
`+-Z`) carry the same skin — the `-X` face is coupled to the clamp through the grease-film
border surface, which overrides the skin there. Defaults
reproduce the previous production model exactly (verified event-by-event at `f=0`; `f=1`
reproduces the old full-absorber value ≈3 pe/evt). A config-1 pilot (GPS, 4 GeV, 300
bare / 100 reflector evt per point; centre unless noted) gives npe/event:

| case | centre | edge | corner |
|---|---:|---:|---:|
| bare f=0 | 305.2 | 288.2 | 278.9 |
| bare f=0.01 | 154.2 | 141.1 | 137.5 |
| bare f=0.02 | 113.7 | 99.2 | 101.2 |
| bare f=0.03 | 94.9 | 83.3 | 78.5 |
| TiO₂ | 121.9 | 154.4 | 228.2 |
| Al grease | 193.0 | 177.8 | 191.9 |
| Al air | 473.0 | 441.1 | 472.1 |
| glossy | 510.2 | 485.5 | 492.1 |

At `f≈0.017–0.02` the bare plate falls below TiO₂ at all three positions; Al-air lands
just below glossy. A finer centre scan gives `f=0.0125/0.015/0.0175` → 142/134/120 pe/evt
(300 evt each) versus TiO₂ 122. This recovers Rao's **reflector** ordering
`bare < TiO₂ < Al ≈ glossy` without changing the defaults. The contact fraction is an effective area fraction, not a
measured holder gap: 2 % of the total plate area is only a few contact spots on the bottom
face. Position ordering remains a separate discrepancy: TiO₂ is corner-highest, and bare
at `f=0.02` is centre-highest but edge≈corner. The v5 scan (1000 ev/point, cfg 1–3) gave a
centre crossing of f≈0.017 in every layout, but cfg1–2 edge/corner cross at 0.003–0.009
while cfg3 edge/corner need >0.039; because it stopped at 0.03, that scan could not match
every cell. The v6 extension to 0.050 (below) locks the global nominal. Report Sec. 3.2 uses
`f=0.045` as production and `f=0` as the clean-air reference; Sec. 3.3 uses Al-air as
production and grease as the lower bound (air/grease = 2.38–2.59, air-backed Al ≈92% of glossy).

**v6 lock (2026-10-18):** extending the scan to `f=0.035–0.050` (1000 ev/point,
cfg 1–3) gives crossing fractions cfg1/2 centre 0.0172/0.0170, edge
0.0085/0.0086, corner 0.0034/0.0035; cfg3 centre 0.0171, edge 0.0397, corner
0.0410. At `f=0.040` cfg3 corner is still +0.34 pe above the v5 TiO₂ reference
(+1.5σ), so the nominal is locked at **`f=0.045`**, where all nine cells are
below TiO₂ (worst: cfg3 edge −0.97 pe, −4.3σ; cfg3 corner −1.39 pe, −6.4σ).
`slurm/nominal_surface.env` carries this value.

**SiPM presets** (see `PresetConfig_*` in `GeometryMuonScint.cc`; src is truth):
- cfg1 = 8 SiPMs: 2 per side, all four sides, at u = ±0.5.
- cfg2 = 4 SiPMs: **one per side at the SAME u as cfg1** (cfg1 minus one SiPM per side;
  the removed slot rotated 90° side-to-side, C4-symmetric). Keep +X u=−0.5, −X u=+0.5,
  +Y u=+0.5, −Y u=−0.5. **NOT centred.**
- cfg3 = 2 SiPMs: both on the **−X** side at u = −0.5, +0.5 (single instrumented side,
  opposite the beam edge/corner regions). **Not** +X.
- cfg4 (preset 7 analytic; preset 5 = legacy tessellated, vis only) = WLS fibre clamp on the
  **−X** narrow face (see "Config 4 / WLS").
- preset 12 (`c2_cornercut`, the report's "4-corner cut, glossy") = the slab is a **convex
  octagon**: each square corner is truncated by a 45° cut with a 12 mm leg, leaving four
  17.0 mm faces at the face centres `(±119,±119)` in the report frame. One 4 mm SiPM sits on
  each cut face (4 sensors; `sipmHits` channel order 0..3 = report BR, TR, TL, BL). The solid
  is a convex `G4ExtrudedSolid` (analytic, exact face normals); `setCornerCut 0` restores the
  legacy `G4Box` bit-for-bit. `/dcs-monitor/det/muonScint/setCornerCut <length>` (pre-init)
  sets the leg on any preset. The cut is incompatible with the WLS clamp (`BuildWLSFibres`
  forces the cut to 0) and skips `BuildEdgeStrips`. The removed triangles are 2.88 % of a
  50×50 mm corner beam patch (0.461 % of the plate, all four corners), so those muons never
  enter and write no `sipmHits` row — count distinct `evtNb`, never `max(evtNb)+1`.

**Beam regions vs. detector side**: beam patch centres stay `(0,0)` (centre), `(100,0)`
(+X edge), `(100,100)` (+X/+Y corner). The single-sided configs (3, 4) sit on −X so the
edge/corner beams fall on the opposite, un-instrumented side. This is a **simplification**:
the paper places config 3 on the top `+y` face and config 4 on the left face (*adjacent*, not
shared), whereas here both are on −X. On ordering, do **not** assume `centre > edge > corner`
everywhere: the report's config-1 table (`tab:positions`, v6) reports **centre highest** for
bare ($3.868\times10^{-3}$ centre vs $3.254\times10^{-3}$ corner — corner-lowest, as in the
paper) but **corner highest** for TiO$_2$ ($6.380\times10^{-3}$ centre vs $1.121\times10^{-2}$
corner) — the reverse of the paper's corner-lowest rule, and the one genuine disagreement with
Ref. Rao 2025. The mechanism is the *nearest-sensor* rule (below): the diffuse paint is a poor
light mixer, so the response tracks the distance to the nearest sensor.

**Nearest-sensor rule (the central finding, v6).** The full-plate efficiency field is *not*
radial: for cfg1 the bare plate maps to a checkerboard whose bright cells are exactly the cells
containing a SiPM, for cfg2 the four sensor cells read 3.1–3.35e-3 against 1.63–2.08e-3
elsewhere, and for cfg3 the two sensor cells are bright while the cell *between* them (on the
instrumented face but not on a sensor) is the darkest of the plate. The organising coordinate
is the distance from the muon entry point to the nearest collection site
(`macros/nearest_sensor_collapse.C` → `fig_nearest_sensor.{pdf,png}`, `nearest_sensor.csv`,
`nearest_sensor_maps.txt`): the specular wrappers are flat (they mix), the diffuse paint and
the contact-limited bare plate fall away from the sensors (they do not mix). The same rule
explains the sign flip of the TiO₂ position trend between four-sided and single-sided layouts.
It is *not* a strict point-by-point law — see the config-3 cell between the sensors.

**Detection-efficiency observable (v6).** `macros/de_threshold.C` computes the paper's
DE(u_thresh) = Ndet/Nref as P(Σ N_s ≥ N_thr) from the whole-slab per-event totals
(`fig_de_threshold.{pdf,png}`, `de_threshold.csv`, `de_threshold_window.csv`). The paper's
statement is threshold-free ("all configs except the bare one reach DE ≥ 80 % at the working
threshold"), and it is reproduced: a non-empty threshold window exists in configs 1–3
(57–130, 28–63 and 14–18 pe/event) and is empty for cfg4 — there the bare plate is the
weakest wrapper (the f-fix put it at 19.5 pe/event, below TiO₂'s 36.9) but its pe
distribution, set by the single SiPM behind the WLS fibres, overlaps TiO₂'s too closely to
open a window (and the paper has no bare cfg4 measurement).
