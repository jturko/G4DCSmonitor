# AGENTS.md

Geant4 + ROOT 6 simulation of a CASTOR 440/84 spent-fuel storage cask monitored
by CLYC / plastic detectors. See `README.md` for physics, geometry, and macros.

## READ-ONLY Geant4 reference tree (`.refs/`) — hard rule
- **Never modify anything under `.refs/` or the Geant4 install/source trees it points
  at.** It is a read-only reference: read and search only. Use it to look up G4 APIs,
  base classes, and official example implementations instead of guessing.
- `.refs/examples` → `~/programs/geant4/geant4-v11.3.2/examples`,
  `.refs/source` → `~/programs/geant4/geant4-v11.3.2/source`,
  `.refs/include` → `~/programs/geant4/install/include/Geant4`. These paths are
  machine-specific, so `.refs/` is gitignored — do not commit it. Symlinks cannot
  enforce read-only (Linux has no read-only symlink); this rule is the guarantee.

## Build & run
- Build with CMake from `build/` (the checked-in `build/` is the working dir):
  ```bash
  cmake --build build -j          # or: cmake -S . -B build && cmake --build build -j
  ./build/G4DCSmonitor run.mac    # binary lands in build/
  ```
  Requires Geant4 (`ui_all vis_all`) + ROOT 6 (`RIO Net ROOTDataFrame Imt`); links the
  bare `Eve` library. Geant4 under `~/programs/geant4/install`, ROOT under
  `~/programs/root/build`.
- **Ignore `GNUmakefile`** — legacy Geant4 make system with a bogus
  `G4INSTALL = ../../../..`. CMake is the real build.
- `compile_commands.json` exists at repo root and in `build/` (untracked) for clangd.
- CMake copies `macros/{vis,vis_ogl,vis_vrml,run}.mac` into `build/` on configure;
  re-run cmake after editing them if running the copied copies.
- No test suite, lint, or CI. Verification = build cleanly and run a macro.

## CLI / biasing gotcha
- `G4DCSmonitor [--biasing gamma|neutron|none] [macro] [fileNum]` (args order-insensitive).
- Importance biasing must be enabled in **both** places or startup throws a fatal
  `BiasingConfigError`: CLI `--biasing <particle>` **and** macro
  `/dcs-monitor/det/useBiasing true`. A parallel `BiasingWorld` can only target one
  particle type, so `--biasing` also selects which parallel world is built.
- Surface-flux replay (`CASTOR440_surface_from_TTree`) is analog — do **not** pass `--biasing`.

## Command ordering (Geant4 UI)
- Pre-init geometry commands must precede `/run/initialize`: `setPosition`,
  `setRotation`, `.../add`, `useBiasing`, `setBiasing*`.
- Gun/surface engine, output toggles, and `/run/beamOn` come after `/run/initialize`.
- Source-mode candidates are defined in `src/PrimaryGeneratorMessenger.cc` (the enum
  there is the source of truth): `GPS`, `CASTOR440_surface`, `CASTOR440_surface_from_TTree`,
  `CASTOR440_fuel`, `CASTOR440_fuel_biased`.

## MuonScint optical model
Goal: reproduce Rao et al., J. Appl. Phys. 138, 114504 (2025) — polished 250×250×10 mm³
UPS-92S plate in a black 3D-printed holder, SiPM ASD-NUV4S-P-40 on the side faces.
Target ordering **none < TiO₂ < Al ≈ glossy**; yield centre > edge > corner.
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
`plot_whole_slab.C`). In this frame the cfg1/2 SiPMs sit on the four edges, cfg3 on the top
`+y` edge, and cfg4's clamp on the top `−x` edge with its SiPM at the top-left corner.

**Reflector model per finish** (`BuildOpticalSurfaces`), one shared 10-pt 360–650 nm
grid (ascending eV). Reflector and bare-absorber skins are mutually exclusive.
- `kNoReflector` (bare): **no** wrapping skin. The plate is in effective optical contact
  with the black holder (ABS n≈1.55 ≈ scint 1.58), so TIR is **frustrated** and trapped
  light leaks into the wall. Modelled by a `MuonScintHolderAbsorber` skin
  (REFLECTIVITY=TRANSMITTANCE=0); `fReflSurface` is built but **unused**. This is the key
  fix: a free-standing polished plate is a near-lossless TIR guide and would be the *best*
  collector, contradicting the measured bare minimum.
- `kPaintTiO2`: `groundfrontpainted` (Lambertian, **no TIR**), R≈0.955–0.983.
- `kAluminumFoil`: `polishedbackpainted` (TIR + specular mirror, SPECULARSPIKE=1),
  R≈0.840–0.910.
- `kGlossyPaper`: `polishedbackpainted` (lossier mirror, R≈0.900–0.955) — paper reports
  Al ≈ glossy and both highest, so glossy is not a diffuser.
- `kTeflon`: kept for completeness, not in the paper.

Result (200 evt, cfg1, centre, npe/event): bare=3.4 < TiO₂=139 < Al=513 < glossy=608,
centre>edge>corner for all four — matches the paper.

**SiPM detection via the boundary process**: grease↔SiPM is `dielectric_metal` with
`EFFICIENCY` = ASD-NUV4S-P-40 PDE(λ) (peak 0.43 @ 420 nm) and `REFLECTIVITY`=0.
`G4OpBoundaryProcess` does the PDE draw, sets status `Detection`, and invokes the SD —
needs **`SetBoundaryInvokeSD(true)`** (`PhysicsList.cc`). `MuonScintSiPMSD::ProcessHits`
therefore fires only for real photoelectrons and kills the photon (no manual draw).

**Black holder** (`BuildHolder`, bare config only): `G4SubtractionSolid` shell
(outer − cavity), `MuonScintHolderBlack` C:H 1.05 g/cm³ n=1.55 ABSLENGTH=1 µm, cavity =
slab + `fHolderGap` 1.2 mm all faces. `UsesBlackHolder()` = bare **and** not WLS, so the
Config 4 clamp never overlaps the holder. The bare frustrated-TIR absorber skin stays on
the other five faces via `UsesAbsorberSkin()` (bare, independent of the holder shell).

**SiPM presets** (see `PresetConfig_*` in `GeometryMuonScint.cc`; src is truth):
- cfg1 = 8 SiPMs: 2 per side, all four sides, at u = ±0.5.
- cfg2 = 4 SiPMs: **one per side at the SAME u as cfg1** (cfg1 minus one SiPM per side;
  the removed slot rotated 90° side-to-side, C4-symmetric). Keep +X u=−0.5, −X u=+0.5,
  +Y u=+0.5, −Y u=−0.5. **NOT centred.**
- cfg3 = 2 SiPMs: both on the **−X** side at u = −0.5, +0.5 (single instrumented side,
  opposite the beam edge/corner regions). **Not** +X.
- cfg4 (preset 5) = WLS fibre clamp on the **−X** narrow face (see "Config 4 / WLS").

**Beam regions vs. detector side**: beam patch centres stay `(0,0)` (centre), `(100,0)`
(+X edge), `(100,100)` (+X/+Y corner). The single-sided configs (3, 4) sit on −X so the
edge/corner beams fall on the opposite, un-instrumented side, as in the paper. Measured
ordering is **centre > edge > corner for all configs 1–4**, matching the paper's corner-lowest.

**Diagnostics/output**: `include/OpticalDiagnostics.hh` (thread-local per-event optical
accounting + per-slab produced/detected + primary-muon first-entry x-y); `opticalStats`
ntuple (nScint/nCerenkov/nKilled/nDetected/nEscaped/nAlive). Toggle
`/dcs-monitor/run/writeOpticalFluxMap` (default **off**) books per-slab
`h2_optflux_xy_slab<i>` **track-length only** (1 mm cells); booking is in
`BeginOfRunAction` (needs geometry).

**Collection efficiency lives in `sipmHits`** (not H2 maps). `MuonScintSiPMSD::EndOfEvent`
writes one row per (event, SiPM) — including zero-detection rows — with fixed columns
evtNb, det, sipm, nDetected, tFirst, meanWavelength_nm, rmsWavelength_nm, weight, muonX,
muonY, nProduced. `nProduced` = all optical photons created in the slab that event
(scint + Čerenkov). Rows are emitted only for slabs the muon entered
(`gOpticalDiag.muonEntrySet[det]`); legacy fallback (hit-only, muonX/Y = −9999) when
`fDetector == nullptr`. SD `EndOfEvent` runs before `EventAction::EndOfEventAction`
(`.refs/source/event/src/G4EventManager.cc:314` vs `:323`), so it can read `gOpticalDiag`.
For pe/evt, sum `nDetected` over all SiPMs and divide by the number of events (`evtNb`),
**not** by row count — dividing by rows gives per-SiPM pe/evt (the two differ by the SiPM count).

**Tooling**: `macros/{muon_scint_template.mac,generate_muon_matrix.sh,run_muon_matrix.sh}`,
`macros/plot_muon.C`, `macros/analyze_muon.C`, `macros/plot_collection_efficiency.C`
(tree-based collection-efficiency maps; `plot_light_efficiency.C` is a thin forwarder
kept for old command lines). `slurm/` bundle (one config×reflector per task, 32 threads,
plus whole-slab array). `slurm/run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS]`
(default 200 / 10000 / 32) generates both macro sets and `sbatch --parsable`s both arrays.
Matrix/whole generators emit cfg 1–4 (cfg 4 → `@PRESET@` = 5);
`slurm/run_local.sh <matrix|whole|all> [NEVENTS] [THREADS]` is a no-SLURM local runner
(default cfg 2 3 4). **There is no SLURM on this login node**, so production runs use
`build/G4DCSmonitor` locally (8 threads). Reflective configs run ~2.5 evt/s vs ~100 evt/s
for bare (long multi-bounce tracks). TTree writer leaves a <2 kB partial file for the
position *currently* being written — **not** an abort. Regenerate the report with
`macros/build_report_figures.sh [MATRIX_DIR] [WHOLE_DIR]` (runs `sipm_layouts.C`,
`report_figures.C`, `plot_collection_efficiency.C`, `plot_whole_slab.C` for cfg 1–4,
copies figures into `report/figures/`, then `pdflatex` twice).

**G4OpBoundaryProcess ground truth** (`.refs/source/processes/optical`): a
`G4LogicalBorderSurface` overrides a `G4LogicalSkinSurface` at the same boundary;
`RINDEX` on both materials is mandatory (else `NoRINDEX` kill); `polished`/`ground`
dielectric_dielectric → Fresnel/TIR; `groundfrontpainted` → rand-vs-R (Lambertian, no
TIR); `polishedbackpainted`/`groundbackpainted` → dielectric + backing REFLECTIVITY; no
surface object → lossless TIR default. `sigma_alpha` on `groundbackpainted` has no effect
on TIR.

## Config 4 / WLS (single SiPM + fibre clamp, preset 5)

**Goal / paper basis:** `~/Documents/muon_paper.pdf` (extract with `pdftotext -layout`)
specifies **four Ø1.0 mm fibres in a 250 mm bar with ~1.0 mm channels**, far side "covered
with a reflective material", one SiPM viewing all four ends. Trust the paper body over
`wls_image_description.txt` (surrogate vision: its Ø/pitch/white-holder are unverified).
Chosen: paper text + **black clamp with far-side reflector**.

**Geometry / coupling:** single row of fibres running the full 250 mm in **y** on the **−X**
narrow face (`fWLSOnNegX`, default true, signs all x placements). A **thin 0.10 mm grease
film** (`fClampGap`) sits between the slab face and the clamp front; each fibre pokes
`fFibreProtrusion = 0.05 mm` into it (coincident clad↔grease boundary; the grease stays
continuous against the slab). The clamp is a `G4Box` with one Ø1.1 mm
(`fChannelRadius = 0.55`) bent channel per fibre. Cladding n=1.49 > grease n=1.46 keeps the
core/clad TIR guide. Fibres are **world siblings** of the slab (not clamp daughters), so
they can protrude past the clamp mouth without tripping the mother-containment check, and
clamp/grease booleans reuse the same footprints so the volumes stay disjoint. Built in
`BuildWLSFibres()` / `PlaceWLSFibres()` (`fUseWLSFibres`); `OpWLS` auto-enabled in
`PhysicsList::ConstructProcess()` when `GeometryMuonScint::UsesWLSWorld()` (flag set in
Build; geometry precedes physics).

**Fibre solid = `G4TessellatedSolid` swept mitered cylinder**, built directly in slab-local
coords by `MakeSweptTube(name, r, path, ringSeg)` from the 4-point centreline
`BentPath(x, zSpread, zBundle, Ly, conv, resL, ov)`: a straight spread run at
`fFibrePitch = 1.5 mm`, a ramp converging over `fFibreConvergeLen = 15 mm` to a touching
`fBundlePitch = 1.0 mm` bundle (four +y ends bunch to a 4 mm bundle facing the single SiPM),
then a short stub before +y. Core and clad are separate tessellated solids (core is a
daughter of the clad). Paths are built slab-local in a canonical **+y-bundle / −y-mirror**
frame; `PlaceWLSFibres` then applies one `Rx180` (`flipRot`) to the film/clamp/fibre
placements so the read-out lands at slab-local **−y** (report-plan **top-left**) with the
mirror caps at **+y**. Why tessellated: an earlier `G4Tubs`+`G4Orb` union defeated
HepPolyhedron (`too many edges` → 10000-point cloud in VRML) — do **not** reintroduce
tube/sphere unions.

**Tessellated booleans are fragile — avoid tangency.** The grease film is carved with a
groove **concentric to each fibre at `rFibre`**, NOT the larger Ø1.1 channel radius: that
cutter's cylinder is tangent to the film's slab-side face, giving a degenerate boolean that
silently breaks the topmost channel and hangs the `TSG_OFFSCREEN` renderer. **Cutter
overshoot:** every cutter is `fHalfSize.y() + 10 mm` long so each opening is bounded by the
object's own faces, never by a face coincident with the tool (coincident faces leave
degenerate polygons). **Channels must not overlap:** `fFibrePitch = 1.5 mm` (> 2·channel
radius) keeps a solid clamp web between neighbours (Rao Fig. 4a) and the booleans valid.

**The Ø1.1 channel / Ø1.0 fibre clearance (0.05 mm air annulus) is deliberate — do NOT make
it snug.** The real Y-11(200) is multi-clad (outer fluoropolymer n≈1.42) but our single clad
is n=1.49, so a snug fibre touching the n=1.55 black clamp would kill the low-index gap and
lose the guided light (smoke: Ø1.1 + air = 15.0 pe/ev; snug Ø1.0 = 8.8 pe/ev). The paper
specifies grease only in the plate↔clamp gap, not around the fibres.

**Read-out bug to remember:** **all four** `cladPVs[k]` need the `fCouplingSurface` border
to the shared read-out pad; giving it to `cladPVs[0]` only traps fibres 1–3 in lossless TIR.
Far (**+y**, after the placement flip) ends are full-disc `G4_Al` mirror caps; the read-out
(**−y**) ends meet the shared grease pad + single SiPM. The clamp LV carries a
`dielectric_metal` specular mirror skin on its outboard face.

**Known caveat (documented, not chased):** cfg4 collects ~17 pe/evt (centre) vs cfg3 ~164,
i.e. cfg4 < cfg3, whereas paper Fig. 12 shows cfg4 > cfg3. The paper itself warns the configs
"cannot be directly compared" (different SiPM count / gain / input capacitance), and its cfg4
value is a pulse height over a 2500 mm² fibre-augmented effective area, not raw pe. The
geometry is structurally faithful. Measured ordering is **centre > edge > corner** (the
paper's corner-lowest) for all configs 1–4.

**Figures / vis:** collection comes from the `sipmHits` tree
(`plot_collection_efficiency.C` / `plot_whole_slab.C`). `report_figures.C::fig_wavelength_cfg`
is a 2×2 per-reflector overlay of cfg1–4: c1–3 ≈ 459–460 nm (blue scintillation), c4
green-shifted (confirms `G4OpWLS`). Vis `macros/muon_vis_wls.mac` / `muon_vis_wls_closeup.mac`
→ `report/figures/muon_event_display_wls.{png,wrl}`; colours = scint light blue, grease
amber, fibre core/clad pale/dark green, clamp dark grey, SiPM red.

**Rendering gotchas:** use `TSG_OFFSCREEN` + `transparency true` orbit views;
`addCutawayPlane` is unreliable headless (blank frames) — avoid it. `zoomTo ~2.5` ≈ whole
plate (world 10 m).

## Code layout / where things actually live
- `DetectorConstruction.{hh,cc}` is the hub; `src/DetectorMessenger.cc` registers all
  `/dcs-monitor/det/...` commands, including a table-driven block for plastic / hemi /
  hall / CAD. Valid command names come from there (`setPbColMaterial`, `setPEColMaterial`, …).
- The code supports more geometry than the README's "Geometries" section (plastic detector,
  hemi shields/panels, experimental hall, CAD/GDML import). Trust the code.
- **Output trees** (`RunAction.cc`): `primary`, `hits`, `surfaceFlux`, plus a `meta`
  detector-metadata TTree written by the master *after* the MT merge; written only if
  absent, so deleting/re-running is safe. Toggles: `/dcs-monitor/run/writePrimary`,
  `/dcs-monitor/run/writeSurfaceFlux`.
- `SurfaceFluxSampler` is a master-side singleton; step-1 `surfaceFlux` coordinates are
  **cask-local**, loaded once then sampled O(1) across threads. Geometry params (outer
  radius/height) are passed at load time.

## Examples
- `examples/p1-surface_flux_generation/` then `examples/p2-detector_response/` is a
  two-part workflow (generate surface flux → replay into detector).
- `generate_macros.sh` hardcodes `BASE_DIR="/bigdata/rimanus/turko46/data/SurfaceFlux"`
  which **does not exist** on this machine — edit before running. It writes macros to
  `macros/auto/` (relative), so run it from the `p2-detector_response/` dir.
- `_orbit_map.sh` holds hardcoded C6-symmetry lookup tables (`lookup_global_fuel`,
  `lookup_base_rot`); keep them mutually consistent (every fuel 0–83 appears once).
- Analysis ROOT macros in `p2-detector_response/analysis/` expect helper headers
  (`geometry_constraints.h`, `style.h`) on the ROOT include path.

## Shell commands
- Prefer a single command per tool call. Chained commands cannot match the permission
  allowlist, so each one requires manual approval.
- Where possible, avoid using `&&`, `;`, `||`, instead running each shell command on its own.
