# AGENTS.md

Geant4 + ROOT 6 simulation of a CASTOR 440/84 spent-fuel storage cask monitored
by CLYC / plastic detectors. See `README.md` for physics, geometry, and macros.

## Two-worktree layout (this machine)
Two `git worktree`s of the same repo live side by side; both are buildable and
runnable, and each has its **own** `build/` and its own `report/` output tree.

| path | branch | build | content |
|---|---|---|---|
| `/home/turko46/geant4-sims/G4DCSmonitor` | `main` | `build/G4DCSmonitor` | CASTOR 440/84 + biasing code (upstream `origin/main`) |
| `/home/turko46/geant4-sims/G4DCSmonitor_muon` | `1-muon-trigger-detector` | `build/G4DCSmonitor` | MuonScint work (this branch) |

- `git worktree list` shows both; `git worktree remove <path>` detaches one.
- **A build dir cannot be moved between trees**: `build/CMakeCache.txt` pins
  `CMAKE_HOME_DIRECTORY` to the tree it was configured in. Configure each tree on its
  own: `cmake -S . -B build -DGeant4_DIR=$HOME/programs/geant4/install/lib/cmake/Geant4
  -DROOT_DIR=$HOME/programs/root/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  (`cmake --fresh` re-configures from scratch without deleting the directory).
- `.refs/` is gitignored and machine-specific — re-create the Geant4 symlinks
  (`geant4/{examples,include,source}`) and the ROOT ones (`root/{examples,include,source}`)
  in any new worktree.
- **The binary is never taken from `$PATH`** and there is no global install of it.
  Every driver resolves it from its own location (`slurm/run_*.sh`/`*.sbatch` →
  `${HERE}/../build/G4DCSmonitor`; `slurm/run_local.sh`, `macros/build_movie*.sh` →
  `${ROOTDIR}/build/G4DCSmonitor`) and always execs the absolute path;
  `command -v G4DCSmonitor` is only used to print a warning. To pin a build, export
  `BIN=/abs/path/G4DCSmonitor`. The SLURM drivers first probe the cluster-only path
  `/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor` — it does not exist on
  the login node (so they fall back to the worktree build), but on the HPC always
  export `BIN`.
- Report inputs are **not** in the repo: `slurm/data_*`, `report/refs/*.pdf` and
  `vrml/` are gitignored. The production trees live in
  `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti` (HPC native path:
  `/bigdata/DCS-Monitor-II/turko46/MuonScinti`); point the report at them with
  `DATA_ROOT=...` or the positional dirs of `macros/build_report_figures.sh`, or the
  `MATRIX_DIR`/`WHOLE_DIR`/… overrides of `make -f macros/Makefile.report`.
- Generated macros bake in absolute `/analysis/setFileName` paths for the tree they
  were generated in (`macros/muon/scan_c*.mac`, `slurm/muon*/`). After moving or
  re-creating a tree, regenerate them: `macros/generate_muon_matrix.sh` (writes
  `macros/muon/` and `data_<VERSION>/matrix/`).
- The `main` tree is the smaller CASTOR codebase: no `report/`, no `macros/muon/`, no
  `slurm/` drivers, and its `macros/run.mac` requires **both** `--biasing gamma` on
  the command line and `/dcs-monitor/det/useBiasing true` in the macro (see
  *CLI / biasing*).

## READ-ONLY reference trees (`.refs/`) — hard rule
- **Never modify anything under `.refs/` or the Geant4/ROOT install and source trees
  they point at.** It is a read-only reference: read and search only. Use it to look up
  APIs, base classes, and official example implementations instead of guessing.
- Geant4 (v11.3.2): `.refs/geant4/examples` → `~/programs/geant4/geant4-v11.3.2/examples`,
  `.refs/geant4/source` → `~/programs/geant4/geant4-v11.3.2/source`,
  `.refs/geant4/include` → `~/programs/geant4/install/include/Geant4`.
- ROOT (v6.36.02): `.refs/root/include` → `~/programs/root/build/include` (the
  configured install headers, e.g. `TFile.h`, generated `RConfigure.h`),
  `.refs/root/source` → `~/programs/root/root-6.36.02` (full source tree, per-module
  `inc/` + `src/` implementations), `.refs/root/examples` →
  `~/programs/root/root-6.36.02/tutorials`. The source tree contains the tutorials too,
  so the `examples` link is for convenience/parity with `.refs/geant4/examples`.
- These paths are machine-specific, so `.refs/` is gitignored — do not commit it.
  Symlinks cannot enforce read-only (Linux has no read-only symlink); this rule is the
  guarantee.

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
- `CMakeLists.txt` sets `CMAKE_EXPORT_COMPILE_COMMANDS ON`, so **every** configure
  (including a bare `cmake -S . -B build`) writes `build/compile_commands.json`; the
  repo-root `compile_commands.json` symlink points at it (both gitignored) for clangd.
- **clangd diagnostics are advisory and eventually consistent.** A header edit
  invalidates clangd's preamble asynchronously, so a mid-edit header/source pair (e.g.
  a new declaration before its definition) surfaces transient errors that disappear
  once clangd re-parses. Do not "fix" a diagnostic that contradicts a consistent
  header/source pair or vanishes after the next save; one that survives a second edit
  is real. Ground truth is `cmake --build build -j` (or that file's compile-DB command
  with `-fsyntax-only` for a quick single-file check).
- CMake copies `macros/{vis,vis_ogl,vis_vrml,run}.mac` into `build/` on configure;
  re-run cmake after editing them if running the copied copies.
- No test suite, lint, or CI. Verification = build cleanly and run a macro; treat clangd
  diagnostics only as hints.

## Data storage: HPC native path vs login-node sshfs
- **HPC (SLURM jobs):** the shared data root is
  `/bigdata/DCS-Monitor-II/turko46/MuonScinti`. Every `DATAROOT`, generated
  macro `/analysis/setFileName`, and `sbatch` command must use this native path.
- **This login node:** the same tree is mounted via sshfs at
  `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti`
  (`~/bigdata/...`). Use that for local inspection and ROOT analysis only; do
  **not** pass it as `DATAROOT` to a job that runs on the HPC.
- **~/bigdata info** Do not do recursive searches via Glob, or other tools, which 
  include `~/bigdata`. It is mounted via sshfs and it will freeze if you do so.
- `slurm/run_fast.sh` defaults `DATAROOT=$PWD` (so `cd` to the bigdata root
  first or set `DATAROOT=/bigdata/...` explicitly).
  `slurm/extend_contact_v6.sh` defaults to the native `/bigdata/...` path.
- The two paths are not interchangeable: an sshfs path in a batch script can
  silently send output through the login-node mount instead of the HPC
  filesystem, or fail on the compute node.

## CLI / biasing
- `G4DCSmonitor [macro] [fileNum]` — **positional only** (`G4DCSmonitor.cc:64-97,143`):
  `argv[1]` is the macro, `argv[2]` an integer that suffixes the output filename via
  `RootManager::SetFileNum`/`GetFileNum` (`HistoManager.cc:69-72`). There is **no** flag
  parsing (no `--biasing` or any option string); with no args (`argc==1`) it starts the
  interactive UI.
- **Importance biasing is currently disabled, not configurable.** The
  `G4ImportanceBiasing` / `G4GeometrySampler` block is commented out in
  `PhysicsList.cc:123-133`, and `RegisterParallelWorld(new GeometryParallelBiasing(...))`
  is commented out in `DetectorConstruction.cc:81` — `GeometryParallelBiasing.{hh,cc}`
  exists but is unused. `G4DCSmonitor.cc:113` still passes `useImportanceBiasing=true` to
  the `PhysicsList` ctor, but it has no effect. There is no `BiasingConfigError`.
- Macro command `/dcs-monitor/det/useBiasing` (pre-init; `DetectorMessenger.cc:58`) only
  sets `DetectorConstruction::fUseBiasing` (field default `false`, messenger default
  `true`); with the parallel world unregistered it changes nothing.
- The `CASTOR440_surface` source mode is **analog** and reads no phase-space file: it
  draws uniform-area points on the cask surface (top/bottom discs + side) with
  cosine-weighted emission (`PrimaryGeneratorAction.cc:113-169`).

## Command ordering (Geant4 UI)
- Pre-init geometry commands must precede `/run/initialize`: `setPosition`,
  `setRotation`, `.../add`, `useBiasing`, `setBiasing*`.
- Gun/surface engine, output toggles, and `/run/beamOn` come after `/run/initialize`.
- Source-mode candidates are defined in `src/PrimaryGeneratorMessenger.cc:27` (the enum
  in `PrimaryGeneratorAction.hh` is the source of truth): `GPS`, `CASTOR440_surface`,
  `CASTOR440_fuel`, `CASTOR440_fuel_biased`.

## MuonScint optical model
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

**Diagnostics/output**: `include/OpticalDiagnostics.hh` (thread-local per-event optical
accounting + per-slab produced/detected + primary-muon first-entry x-y); `opticalStats`
ntuple (nScint/nCerenkov/nKilled/nDetected/nEscaped/nAlive). Toggle
`/dcs-monitor/run/writeOpticalFluxMap` (default **off**) books per-slab
`h2_optflux_xy_slab<i>` **track-length only** (1 mm cells); booking is in
`BeginOfRunAction` (needs geometry).

**Collection efficiency lives in `sipmHits`** (not H2 maps). `MuonScintSiPMSD::EndOfEvent`
writes one row per (event, SiPM) — including zero-detection rows — with fixed columns
evtNb, det, sipm, nDetected, tFirst, meanWavelength_nm, rmsWavelength_nm, weight, muonX,
muonY, nProduced, **tDetected, wlDetected_nm, creatorDetected** (the first two are
variable-length vector branches: per-photoelectron global arrival times [ns] and wavelengths
[nm], one entry per detected photoelectron, empty for zero-detection rows; `creatorDetected`
is a parallel vector<int> giving the creating process of each recorded photoelectron —
1 = OpWLS, 2 = Scintillation, 3 = Čerenkov, 0 = unknown — via the thread-local
`gDetectedTimes`/`gDetectedWavelengths`/`gDetectedCreator` buffers in
`include/OpticalDiagnostics.hh`, refilled per row by `MuonScintSiPMSD::EndOfEvent`, enabling
offline pulse-shape and process-mix studies).
`nProduced` = all optical photons created in the slab that event
(scint + Čerenkov). Rows are emitted only for slabs the muon entered
(`gOpticalDiag.muonEntrySet[det]`); legacy fallback (hit-only, muonX/Y = −9999) when
`fDetector == nullptr`. SD `EndOfEvent` runs before `EventAction::EndOfEventAction`
(`.refs/geant4/source/event/src/G4EventManager.cc:315` (`sdManager->TerminateCurrentEvent`) vs
`:323` (`userEventAction->EndOfEventAction`)), so it can read `gOpticalDiag`.
For pe/evt, sum `nDetected` over all SiPMs and divide by the number of events (`evtNb`),
**not** by row count — dividing by rows gives per-SiPM pe/evt (the two differ by the SiPM count).

**Detected-photon visualization**: `/dcs-monitor/run/showDetectedPhotonsOnly true` (default
**off**, `RunMessenger`) makes `EventAction::EndOfEventAction` prune the event's
`G4TrajectoryContainer` to the track IDs recorded by `MuonScintSiPMSD::ProcessHits`
(`gOpticalDiag.detectedTrackIDs`, cleared per event) **plus their full ancestor chain**
(walk `G4VTrajectory::GetParentID`). The ancestor walk matters for WLS: `G4OpWLS` kills the
creator optical photon, so its track ID is not in `detectedTrackIDs`; including parents keeps
that pre-WLS photon (and the primary muon) in the event. It is vis-only and requires
`/tracking/storeTrajectory 1`; G4VisManager draws the event *after* `EndOfEventAction`, so
the stored viewer sees the detected WLS photons, their creator optical photons and the muon;
unrelated undetected photons are removed. `macros/muon_movie_wls.mac` +
`macros/build_movie_wls.sh` use it for Config 4 (preset **7 analytic** fibres by default;
`WLS_PRESET=5` selects the legacy tessellated fibres, but their nested multi-clad cores 2/3
can be culled by the OpenGL viewer — the clads still draw, so prefer preset 7;
the black `WLSClampLV` is hidden, together with the coupling grease film
(`WLSCoupleFilmLV`) and read-out grease pads (`GreaseLV`): those `G4SubtractionSolid`
booleans render as a stipple-dot cloud over the fibres in the OGL viewer.
The camera is animated by `/dcs-monitor/run/movieCamera <time>` (master-only; worker copies
are skipped): it holds a wide view (target `0 0 0`, theta 55, zoom 1.0) through the muon
crossing, then eases over 1.5–12 ns to the fibre/SiPM close-up (target `-110 0 0 mm`, theta
80, zoom 3.5). The loop body is `macros/muon_movie_wls.loop` (separate from the shared quad
loop), glossy reflector, seed 12345/67890, display-only `LY_SCALE=0.4` for preset 5 / `0.1`
for preset 7, 0–80 ns, step 0.25 ns, tail 10 ns) →
`report/movie_wls/muon_movie_wls.{gif,mp4}` (270 frames after trimming). Detected photons
and their pre-WLS creators are drawn green, the muon red, delta electrons orange; all
detections are WLS secondaries except rare direct scintillation hits. WLS re-emission is
exponential (τ≈7 ns), so detections occur out to ~60–70 ns.
`macros/movie_trim.py` now takes `[pad] [color]`; use `green` for these frames (the
fibre geometry is pale/dark green and does not satisfy the pure-green mask; a moving camera
also invalidates the first-frame `green-delta` baseline). `green-delta` remains for
static-camera movies.

**Position reconstruction from the SiPM pattern** (report §6, `macros/position_reconstruction.C`):
can the *relative* charge/timing pattern of the eight config-1 SiPMs localise the muon
inside the plate? Uses the whole-slab files (`data_v6/whole/muon_whole_out_c1_r{0..3}.root`,
25 000 events each) with the truth `muonX/muonY` as the label. Per event and SiPM it
extracts `N`, `tfirst` (earliest photoelectron), `tbar`, `NP3` (prompt fraction, t < 3 ns)
and `wbar`; features are the 8 pulse-integral shares, the 8 relative arrival times, the 8 prompt
fractions, or all 24. `N` = `nDetected` = the **total** photoelectrons detected in the channel
over the event, i.e. the *integral* of the light pulse (× gain) — **not** the pulse-peak
amplitude; no peak-amplitude observable is used anywhere. Measured prompt (<3 ns) fractions
of the light: bare 53 %, TiO₂ 34 %, Al/glossy 15–16 %, so a peak-amplitude read-out would
work with a minority of the photons and be noisier. Two estimators: (a) the analytic
"opposite-pair" observables
`A_x = (N_right−N_left)/(N_right+N_left)` and `T_x = <t>_right−<t>_left` whose linear
response gives noise/slope directly; (b) a k-NN regression (k = 40, standardised features,
20 000 train / 5 000 held out) — the simulation analogue of a calibration-scan response
function. **Result:** TiO₂ localises to σ_x ≈ 12.5 mm from the pulse integrals alone (the light
sharing is first order: `A_x` slope 7.4e-3/mm); glossy/Al have *no* first-order light
sharing (slope 5e-5/mm) but the arrival-time pattern still gives σ_x ≈ 19 mm (the
information is strongly non-linear, hence the learned estimator); bare ≈ 22 mm with all
channels. Figures `fig_recon_{waveforms,observables,features,response,map}.{pdf,png}` +
CSVs `position_{reconstruction,observables,class_observables}.csv` in `report/figures/`.
Run it through ACLiC (`.C+`, compute-bound) with `ROOT_INCLUDE_PATH=$HOME/root-work` set; add it to
`macros/build_report_figures.sh` (already done). The shared style file (`style.h`) lives in `/home/turko46/root-work/`. Note the ROOT drawing pitfalls hit here:
`TMarker`/`TLine` `Draw("SAME")` corrupt the pad range (use `TBox`/`TGraph`), and a
`TH1` drawn with `"BAR"` fills from the frame bottom, so bars must be `TBox`es.

**Optical constant-fraction (CFD) timing + the α scan** (same macro, 2026-10-21). The
timing observable is the *wavefront time* `t̄_{≤α}` = mean arrival time of the leading α
fraction of a channel's photoelectrons (α = 12.5 % nominal) — the optical analogue of a
constant-fraction discriminator; it removes the `N_s^{-1/2}` amplitude walk of the
first-photoelectron time. `macros/position_reconstruction.C` computes the three timing
variants (`tcfd` point quantile, `tcfdm` wavefront mean, `tcfdp` tapered Beta(1,3)) for
**every** α of `kAlpha[]` = {0.03, 0.05, 0.075, 0.10, 0.125, 0.15, 0.20, 0.30, 0.50}
(`kAlphaNom` = 4) and stores them in `Event::tcfdA/tcfdmA/tcfdpA`; the scan block then
re-runs the learned `kTimeCFD` and `kAll` estimators and the linear opposite-pair fit per
α → `position_alpha_scan.csv` + `fig_recon_alpha_scan.{pdf,png}`. The scan needs ~50 min
(72 extra k-NN runs); re-draw the figure alone with the 5th argument:
`root -l -b -q 'macros/position_reconstruction.C+("","report/figures",0,0,"report/figures/position_alpha_scan.csv")'`.

**The 3rd (width) panel is computed without the k-NN sweep** (2026-10-22): the 6th argument
`doAlphaScan = 2` is a *cheap* mode that builds only the linear rows, keeps `knnSig`/`knnSigAll`
from the existing CSV (matched on wrapper + α) and rewrites it, so it returns **before** both
the `res` (20 fits) and the α `k-NN` (72 fits) loops → ~30 s instead of ~50 min:
`root -l -b -q 'macros/position_reconstruction.C+("<whole dir>","report/figures",25000,2,"report/figures/position_alpha_scan.csv",1)'`
(the `1` only suppresses the Monte-Carlo mode; `2` is what makes it cheap). The CSV gained
`linear_width_ns` / `linear_width_pointquantile_ns` (cols 9–10) = `PooledBinWidth`, the rms of
`tcfdm` **at fixed muon x** (pooled within 25 mm bins, ≥50 entries): the position-conditioned
width, i.e. what a CFD is actually tuned on. It is *not* the global residual `linear_rms_noise`
(col 4, which also contains the curvature of the response and is what `linear_sigma_x_mm`
divides by the slope). Measured, α = 3 → 50 %: bare 1.377 → 1.342 ns and TiO₂ 0.888 → 0.854 ns
(flat), Al(air) 0.393 → 0.665 and glossy 0.391 → 0.675 (×1.7). The slope meanwhile grows from
0.0018 to 0.0029 ns/mm up to α ≈ 20 %, so the width and the slope move in opposite directions
and only their ratio has the interior minimum at α ≈ 15 % — neither single quantity is a good
tuning criterion.
**Figure layout:** `DrawAlphaScan` is a 2 × 2 canvas (1180 × 900 px, printed at `\textwidth`
→ 8.1 cm per panel, ~6.6 pt capitals as measured on the PNG): (a) width, (b) linear, (c)
learned, (d) key cell (wrapper key + estimator key + the dashed-line note). Do **not** go back
to a 1 × 3 layout: at 5.4 cm per panel every text falls below 6.5 pt. Two ROOT traps hit here:
a `TH1` axis y-title is **clipped at the pad edge** at this pad height whatever the
`TitleOffset` (draw it yourself with `TText` + `SetTextAngle(90)` and an empty axis title), and
long titles ("rms of T_{CFD,x} [ns]", "CFD fraction") overflow a 425-px axis — use "rms width
[ns]" / "rms error [mm]" / "alpha"
**Result:** the learned estimator is insensitive to α (< 6 % over α = 3–20 %, nominal within
1 % of the best); the *linear* opposite-pair timing read-out of the specular wrappers has a
clear minimum at α ≈ 10–15 % and grows ×1.5 at α = 50 % (glossy 182 → 279 mm) — the
reverberation-tail argument of the report, now measured. Two consequences now stated in the
report: (i) the minimum is **not resolved** better than α = 12.5 ± 2.5 % (the neighbouring
differences are ≤ 1 %, the statistical precision of the error with 5000 test events), and
(ii) the advantage of the wavefront time over the first-hit time quoted in Table 6 is an
optimum-value statement — 32 % in glossy paper at α = 12.5 %, 17 % at α = 5 %, and *negative*
at α = 50 % (279 mm vs 268 mm for the first hit). Caveats in the macro: ROOT has a
global `::kTop` (TGLFontManager.h) that clashes with a local `kTop` → the SiPM group is
called `kTopEdge`; `TText` needs `#include <TText.h>` (cling would auto-include it and
trigger the clash). The **`data_v7/whole`** tree is the campaign quoted in the report tables
(its files are byte-identical to `data_v6p5/whole` except `muon_whole_out_c4_r0.root`, which was
re-run on 2026-10-21 — the α scan uses cfg 1 only, so the numbers are unchanged). Not
`data_v6/whole`.

**Ten-design campaign (`v7_opt`, report Sec. 10, 2026-10-21).** Ten whole-plate runs,
25 000 muons each, all files in `data_v7_opt/whole/<tag>.root` (production tree
`/home/turko46/bigdata/.../MuonScinti`); tags `c1_split_black`, `c8_split_black`,
`c10_wls4_tio2`, `c11_wls8_tio2`, `c1_edge_diffuse`, `c1_6mm_diffuse`, `c8_6mm_diffuse`,
`c10_wls4_glossy`, `c11_wls8_glossy`, `c1_same_1p55`. An eleventh design,
`c2_cornercut` (preset 12, glossy, `setCouplingIndex 1.55`), is reserved in the same table;
`slurm/run_cornercut.sh` submits **only** its matrix, whole-slab and v7_opt array task
(25 000 events), leaving the ten completed files untouched. The table (order, colour, human label,
data stem) lives in **`macros/opt_designs.h`** — the single source of names **and colours**:
`opt::label(i)`/`d.label` is what every figure prints (never the raw tag), `opt::color(i)`
repeats a design's colour in every figure of the section. The control is `c1_same_1p55`
(`setCouplingIndex 1.55`, 568.2 pe/evt = +9 % over the v7 default 1.46, 520.7), because a
denominator must be measured in the same campaign. Figures: `opt_yield.C` (yield bars +
per-event spectra), `opt_maps.C` (absolute + normalised efficiency maps, and the kappa bar
chart not used in the report), `opt_de.C` (detection efficiency vs threshold + the 90 %
threshold), `opt_waveform.C` (mean arrival-time profile); all write `opt_*.csv` beside them.
Position resolution needs the CPU-bound sweep: `macros/run_opt_recon.sh` (reuses
`position_reconstruction.C` through per-design symlinks) then `opt_recon_fig.C+("<raw.csv>")`.
`macros/build_report_figures.sh` runs the four fast macros when `data_v7_opt` is present
(`OPT_DIR` override) and redraws the recon figure only if `OPT_RECON_RAW` exists; it also
regenerates the two geometry figures of Sec. 10.1 (`opt_layouts.C` needs no data,
`render_designs.sh` runs only when `RENDERS=1`, the default).
**Geometry figures (Sec. 10.1).** `macros/opt_layouts.C [OUTDIR]` draws the three sensor
patterns (preset 1 at u=+-0.5, preset 8 at u=+-0.75, the four-sided clamp) as square 250 mm
plan views in the report frame with true-scale positions, plus the design names and the
6 mm footprint; sensor markers are drawn 2.5x oversize (a true 4 mm sensor is <2 pt at the
print width) and the caption says so. `macros/render_designs.sh` drives four geometry-only
macros (`vis_design_{c1_4mm,c1_6mm,wls10,wls11}.mac`: no beam, no trajectories,
`TSG_OFFSCREEN`, `transparency true`, forced opacity 1) into
`report/figures/design_ogl_{c1_4mm,c1_6mm,wls10,wls11,wls10_clamp}.png`; the 4/6 mm pair and
the 10/11 pair share one camera each. The 4/6 mm macros are single-sensor close-ups of the
+X narrow face (theta 65, phi 0, target 125.4 62.5 0 mm, zoomTo 13.5 = ~26 mm field) with
`lightsVector 0.5 0 1`, because the default light `(1,1,1)` shades the top and +X faces
identically; no in-image scale bar (`/vis/scene/add/scale` draws a 1-px line with a 13-px
label, a hairline at print size). The WLS macros must set `setFibreAnalytic false` **after**
`preset` (analytic `G4CutTubs` boolean results have no polyhedron and render as a
10 000-marker point cloud) and hide `WLSClampLV`, `WLSCoupleFilmLV`, `GreaseLV`; the
faceted variant is the one measured 20 % lower in yield, so the renders are geometry only.
**VRML** (visual, not in the report): `vrml/gen_v7opt_designs.sh` writes one `.wrl` per
design into the gitignored `vrml/v7_opt/`; the tessellated presets draw as solids, the
analytic ones as point clouds. View them with any VRML97 viewer or
`/vis/open VRML2FILE` from the generated macro, or convert with
`view3dscene`/`blender --python` for a still. Nothing under `vrml/` is committed.
Results (combined estimator, 5000 held-out events, mm): 6 mm 11.48/11.78 < TiO2-fibre
13.89/13.75 < control 15.94 < C8 black 17.41 < C1 black 18.75 < diffuse 19.85 << glossy-mirror fibre 55.23/60.71. The direct-coupled family follows
sqrt(N) (6 mm predicts 1.45, measured 1.39; diffuse edge predicts 20.6, measured 19.9); the white-clamp fibre design
carries ~6.6x the position information per photoelectron; the mirror clamp destroys the
information (55-61 mm vs the 72 mm uniform prior) because it transports a plate-wide line
integral to the bundle. Two plotting conventions matter: the map grids are 4x3 (10 panels +
2 blanks) with **one shared palette below the grid** on a taller canvas, and every figure is
sized so that printed capitals are >= 6.5 pt at 0.95-0.98 textwidth (ROOT scales
`SetTextSize` against the **pad**, not the canvas — measure the PNG, do not compute from the
canvas height).

**Movies**: `macros/build_movie.sh` (single pane, Config 1/glossy, blue optical photons) and
`macros/build_movie_quad.sh` (2×2 Config 1 × reflectors 0–3) are the older modes; both need
the X11 OpenGL stored viewer (`DISPLAY=:0`, Xwayland auth on this login node). Frames are
raster EPS → PNG via GraphicsMagick and assembled by `convert`/`ffmpeg`.

**Tooling**: `macros/{muon_scint_template.mac,generate_muon_matrix.sh,run_muon_matrix.sh}`,
`macros/plot_muon.C`, `macros/analyze_muon.C`, `macros/plot_collection_efficiency.C`
(tree-based collection-efficiency maps; `plot_light_efficiency.C` is a thin forwarder
kept for old command lines). `slurm/` bundle now submits **up to four arrays**: matrix
(config×reflector, 16 tasks), whole-slab (16), bare-contact scan (default cfg 1–3 × eleven
`f=0..0.05` = 99; v6 nominal `f=0.045`), and Al backing (cfg 1–3 × `grease|air` = 6). Each task runs
centre/edge/corner back-to-back; contact/Al use
`slurm/muon_characterization_template.mac` (flux map off, `--mem=4G`, `low`, 1 h, 32
threads, 1000 ev/position) and write case subdirs
`data_<VERSION>/contact/c<C>_f<val>/` and `data_<VERSION>/al/c<C>_<backing>/` with the
standard `muon_out_c<C>_r<R>_<pos>.root` names. `slurm/run_fast.sh [MATRIX_EVENTS]
[WHOLE_EVENTS] [MATRIX_THREADS] [WHOLE_THREADS]` is the **recommended SLURM driver**
(matrix flux map off, `--mem=4G`, `low`; defaults 200/10000/32/32); contact/Al knobs are
`CONTACT_EVENTS`/`CONTACT_THREADS`/`CONTACT_FRACTIONS`/`CONTACT_CFGS` and
`AL_EVENTS`/`AL_THREADS`/`AL_BACKINGS`/`AL_CFGS` (defaults 1000, matrix threads, the grids
above). `SKIP_CONTACT=1`/`SKIP_AL=1` skip the respective calibration scan;
`SKIP_CALIBRATION=1` skips both and submits only matrix + whole-slab (the main-production
mode once the nominal surface parameters are locked). `run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS] [NEVENTS_CONTACT]
[NEVENTS_AL]` is the older heavier matrix/whole driver (flux map on, 64G, `cpu-genoa`) but
its contact/Al arrays still use the fast characterization template (4G, flux off). Key env
knobs: `BIN=/abs/path/G4DCSmonitor` (the **only** thing selecting a specific build — it must
be built on the cluster's module stack, e.g. via `slurm/build.sbatch`; the login-node
`build/` links Geant4 11.3.2), `VERSION` (data tag → `data_<VERSION>/`, default `v2.2` —
**override or it clobbers existing data**), `DATAROOT` (default `$PWD`; `cd` to the bigdata
dir first), per-array `MATRIX_*`/`WHOLE_*`/`CONTACT_*`/`AL_*`
partition/account/time/mem, `ARRAY_LIMIT` (applies to all submitted arrays), and
`CONTACT_ARRAY`/`AL_ARRAY` (override the auto manifest-derived bounds). When the
calibration scans are not skipped, both drivers regenerate the shared
`slurm/muon_contact/` + `slurm/muon_al/` macros, so use one driver at a time. `slurm/collect.sh` counts all four trees and prints rsync lines. Matrix/whole
generators emit cfg 1–4 (cfg 4 → `@PRESET@` = **7**, the analytic variant; preset 5 =
legacy tessellated vis-only); `slurm/run_local.sh <matrix|whole|all> [NEVENTS] [THREADS]` is
a no-SLURM local runner (defaults NEVENTS 1000, THREADS 8, `CFGS="1 2 3 4"`, `REFLS="0 1
2 3"`, `SUFFIX=_v2`, `PARALLEL=2`; override via env vars; it still covers matrix/whole
only). **There is no SLURM on this login node**, so local runs use `build/G4DCSmonitor`
(8 threads) while production runs on the HPC via `run_fast.sh`.
Reflective configs run ~2.5 evt/s vs ~100 evt/s
for bare (long multi-bounce tracks). TTree writer leaves a <2 kB partial file for the
position *currently* being written — **not** an abort. Regenerate the report with
`macros/build_report_figures.sh [MATRIX_DIR] [WHOLE_DIR] [CONTACT_DIR] [AL_DIR]`
(defaults `slurm/data_v5/{matrix,whole,contact,al}`; pass absolute dirs when the data
live outside the repo) or, equivalently, with the make target
`make -f macros/Makefile.report` (same 21 macros, one target per figure group —
`make -f macros/Makefile.report help` lists them; `figures` skips the LaTeX pass,
`pdf` runs it alone). The v6 report uses `data_v6/{matrix,whole,contact}` plus
`data_v5/al`, with `AL_MATRIX_DIR=<...>/data_v5/matrix` so the Al curves stay paired with
their v5 matrix references (the Al model is unchanged; v6 Al is incomplete). It runs `sipm_layouts.C`, `irradiation_sites.C`,
`optical_spectra.C`, `report_figures.C`, `pe_spectra_grid.C`, `matrix_positions.C`,
`contact_scan.C`, `al_backing.C`, `plot_collection_efficiency.C`, `plot_whole_slab.C`
for cfg 1–4, `whole_slab_uniformity.C`, `nearest_sensor_collapse.C`, `de_threshold.C`,
`whole_yield_table.C`, `persistence_map_grid.C`,
`wavelength_time_grid.C`, `avg_waveform.C`, `position_reconstruction.C+`, copies figures into `report/figures/`, then
runs `pdflatex` twice. `DATA_ROOT` (default `${ROOTDIR}/slurm`) moves the data root; the
production tree lives at `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti`.
**Report structure (2026-10 rewrite).** `report/matrix_report.tex` is organised around
questions, not data types: 1 Introduction · 2 The measurement being reproduced (what Rao et
al. actually measured) · 3 The detector and its optical model · 4 Simulation campaigns and
surface calibration · 5 Light collection: yield and the wrapper ordering · 6 The uniformity of
the response over the plate · 7 The spectral and temporal signature of the detected light ·
8 Position reconstruction from the SiPM pattern · 9 Comparison with the measurement ·
10 Caveats and open items · 11 Summary · Appendix A (Geant4 optical-physics notes) ·
Appendix B (the position-reconstruction machinery in detail). The
normalised whole-slab maps (old Figs. 25–28) were cut; `fig_nearest_sensor` and
`fig_de_threshold` were added. Every bibliography entry was verified against its DOI
record; the McPeak 2015 author list, the Geant4 PRM version/URL, the Eljen datasheet year
and the Amcrys datasheet title/URL were corrected.
**Section 8 was de-mathematised (2026-10-21).** It carried 13 of the report's 20 displayed
equations (the Weibull extreme-value derivation, three CFD definitions, the `D_4` group
presentation, the permutation matrices, the quotient distance, the standardisation
sums). The main text now has **4** displayed equations (`eq:tcfd-mean` the wavefront time,
`eq:risetime`, `eq:pair` the opposite-pair observables, `eq:knn-simple` the weighted
neighbour average) and states the estimator in plain words (relative pattern → rescale →
eight symmetries of the square → down-weight uninformative blocks); the formalism moved
verbatim to **Appendix B** (`app:recon`: B.1 first-arrival order statistics, B.2 the two
CFD variants not used for the headline results, B.3 the estimator in detail). The
`D_4`/`CFD` jargon was replaced in the main text by "symmetry-augmented" and
"wavefront time"; the numbers are unchanged.

`macros/persistence_map_grid.C [WHOLE_DIR] [OUTDIR] [REFL]` and
`macros/wavelength_time_grid.C [WHOLE_DIR] [OUTDIR] [REFL]` (default REFL 3 = glossy) consume
the per-photoelectron `tDetected`/`wlDetected_nm` vector branches of `sipmHits`: a 2×2 grid
over cfg 1–4 (whole-plate data), shared log-z, from the pooled per-SiPM times of each layout.
`persistence_map_grid` bins each event's summed arrival times into 0.1 ns bins and fills a map
(x = arrival time, y = p.e./bin, z = number of events); `wavelength_time_grid` fills
(x = arrival time, y = wavelength, z = p.e.). `macros/avg_waveform.C [WHOLE_DIR] [OUTDIR] [REFL]`
reuses the same per-event traces and averages them over events, overlaying cfg 1–4 as
mean p.e./0.1 ns vs arrival time (left linear, right log) → `fig_avg_waveform.{pdf,png}`
(report `fig:avgwaveform`).
**Per-panel titles:** the shared `style.h` calls `SetOptTitle(0)`, so a macro that wants the
histogram/panel title drawn must re-enable `gStyle->SetOptTitle(1)` *after* `style(...)`.
`whole_slab_uniformity.C` always did; `plot_whole_slab.C` and `plot_collection_efficiency.C`
(the 2×2 reflector grids) do too now — without it the four panel titles (reflector names)
vanish. **Report packing (as of the 2026-10 restructure):** the report was rewritten around
questions rather than data types; the four normalised whole-slab uniformity maps
(`fig:wholeunifmap`–`fig:wholeunifmap4`, old Figs. 25–28) were **cut as redundant** with the
absolute maps and replaced by `fig_nearest_sensor` (see *Nearest-sensor rule* below). The four
whole-slab maps (`fig:wholeslab`–`fig:wholeslab4`) are `0.74\textwidth` and now use `[H]`
with the discussion interleaved, one per page.
`macros/pe_spectra_grid.C [MATRIX_DIR] [OUTDIR]` draws the Fig. 10 of Rao et al. layout:
a 3 (region: centre/edge/corner) × 4 (config 1–4) grid in which every cell overlays the four
reflector treatments, x = photoelectrons/event, y = normalised entries (both log). Zero-p.e.
events sit below the log abscissa. → `report/figures/fig_pe_grid.{pdf,png}`.
All 12 cells share one x and one y range (computed globally after normalisation); the
centre/edge/corner row labels are **rotated 90°** (`SetTextAngle(90)`, `SetTextAlign(22)`),
drawn on the leftmost pad at NDC `(0.05, 0.5)` (left margin 0.22) so they sit clear of the
frame and axis title and are centred on their row; the single shared legend is drawn in the
top-right cell (config 4) at `(0.60, 0.593, 0.995, 0.803)` — its top-right corner.
`macros/optical_spectra.C [OUTDIR]` plots the optical property tables actually fed to
Geant4 (`src/GeometryMuonScint.cc`): scintillator `SCINTILLATIONCOMPONENT1` emission +
bulk `ABSLENGTH`, Y-11 `WLSABSLENGTH` + `WLSCOMPONENT`, and the SiPM `EFFICIENCY` (PDE)
vs wavelength → `report/figures/optical_spectra.{pdf,png}` (no data files needed). It is a
horizontal 3×1 grid, each panel reserving a top strip (`SetTopMargin(0.24)`) for a
borderless fill-0 legend, so no legend overlaps a curve; each panel's vertical dashed line
marks that panel's characteristic wavelength (scint emission 425 nm, WLS absorption onset
480 nm, SiPM PDE peak 420 nm) — defined in the report caption.
`macros/whole_slab_uniformity.C [WHOLE_DIR] [OUTDIR]` is the whole-slab uniformity add-on
(full-plate analogue of `matrix_positions.C`). **No radial metric**: the SiPM layouts have
no continuous rotational symmetry and the plate is square, so a radial average is a
geometric artefact (for r>125 mm only the four corner wedges populate). Instead it computes
one per-(layout,reflector) contrast, 2-D `kappa_2d` over 5×5 50 mm cells, plus a polar
`epsilon(r,phi)` map (12 r-bins × 12 φ-bins over all configs, shared log z, SiPM azimuths /
cfg4 WLS clamp band marked) and a normalized `epsilon/<epsilon>` map for each cfg 1–4
(**5 mm** cells matching `plot_whole_slab.C`, shared **log** z; slab outline + SiPM/WLS
markers overlaid via the same `drawOverlays`; map histograms carry ±18 mm padding bins so
outboard markers fit). Writes `fig_whole_polar`, `fig_whole_uniformity_map_cfg<C>`,
`fig_whole_contrast` and `whole_uniformity.csv` into OUTDIR. `kappa_2d` is statistically
robust (25000 ev) and, for the **v6** data, ranks **TiO2 > bare > Al ≳ glossy** in every
layout (cfg1 bare ≈1.55, cfg3 bare ≈3.82, TiO2 up to ≈8.31). *The ≈9.5 figure quoted in older
revisions of this file was the v2/v3 full-absorber bare model and no longer applies.*
`fig_whole_contrast` is the **full-slab analogue of the old 3-patch `fig_uniformity`**: a 2×2
grid (one panel per layout), each a 4-bin bar chart of `kappa_2d` per reflector, **linear y,
each panel scaled to its own maximum** (dashed unity line), as in the original
`matrix_positions.C` three-patch figure. The report's "Whole-slab uniformity" subsection (§5.3)
uses `fig_whole_contrast` (Fig. 24) as the visual for the `kappa_2d` numbers, plus
`tab:wholerefl` (Table 9: `kappa_2d`±err per layout × reflector, the linear bars' near-unity
entries resolved numerically), plus the four
normalised `fig_whole_uniformity_map_cfg<C>` maps (Figs. 25–28, all at `0.68\textwidth` so
two fit per page). The polar `(r,φ)` `fig_whole_polar` map and its radial defence are
**deliberately omitted** (retained in the repo in `whole_slab_uniformity.C` but not referenced
by the report). `whole_uniformity.csv` is retained. The 3-patch κ figure/table and the matrix
2D efficiency grids were **removed from the report** (the macros still exist); the position
contrast is now carried by the full-slab `kappa_2d`.
`macros/whole_yield_table.C [WHOLE_DIR] [OUTDIR]` writes `whole_yield.csv` — the absolute
mean p.e./event (and /detected-event, plus detection probability) for every config × wrapper
from the whole-plate run; almost every event detects ≥1 p.e. (399981/400000; 19 without a
detected photoelectron, so the two means differ negligibly). It backs report Table 8
(`tab:wholeyield`); the p.e./event columns are just the `tab:eff` plate-averaged ε scaled by
the ~19700 photons/event produced.

**G4OpBoundaryProcess ground truth** (`.refs/geant4/source/processes/optical`): a
`G4LogicalBorderSurface` overrides a `G4LogicalSkinSurface` at the same boundary;
`RINDEX` is required on **material-1 always** (`G4OpBoundaryProcess.cc:288-306`) and on
material-2 only for a `polished`/`ground` `dielectric_dielectric` boundary (`:427-454`,
else `NoRINDEX` kill); `*backpainted` finishes instead need the **surface** backing `RINDEX`
(`:356-374`), and `*frontpainted` needs neither. `polished`/`ground`
dielectric_dielectric → Fresnel/TIR; `groundfrontpainted` → rand-vs-R (Lambertian, no
TIR); `polishedbackpainted`/`groundbackpainted` → dielectric + backing REFLECTIVITY; no
surface object → lossless TIR default. `sigma_alpha` on `groundbackpainted` **does** reach
the TIR test — the smeared facet normal sets the incidence cosine (`:1124-1137`) — so it is
inert only when all three unified constants are zero (pure Lambertian, as in our Teflon).
**A `NoRINDEX` kill is silent** (prints only at verbose>1, `G4OpBoundaryProcess.cc:448`),
so its absence in a normal run log proves nothing — count it with
`/process/optical/boundary/verbose 2` or via the `opticalStats` budget.

## Config 4 / WLS (single SiPM + fibre clamp, preset 7 analytic; preset 5 legacy vis)

**Goal / paper basis:** `~/Documents/muon_paper.pdf` (extract with `pdftotext -layout`)
specifies **four Ø1.0 mm fibres in a 250 mm bar with ~1.0 mm channels**, far side "covered
with a reflective material", one SiPM viewing all four ends. Trust the paper body over
`wls_image_description.txt` (surrogate vision: its Ø/pitch/white-holder are unverified).
Chosen: paper text + **white clamp (default; `setClampSurface white|black|mirror`) with a
far-side reflector**. Production runs use the analytic preset 7 geometry with multi-clad fibres
and the digitized Kuraray Y-11 tables.

**Geometry / coupling:** single row of fibres running the full 250 mm in **y** on the **−X**
narrow face (`fWLSOnNegX`, default true, signs all x placements). A **thin 0.10 mm grease
film** (`fClampGap`) sits between the slab face and the clamp front; each fibre pokes
`fFibreProtrusion = 0.05 mm` into it (coincident clad↔grease boundary; the grease stays
continuous against the slab). The clamp is a `G4Box` with one Ø1.1 mm
(`fChannelRadius = 0.55`) bent channel per fibre. The outer sheath (n=1.42) sits over the
core/inner-clad (n=1.59/1.49) guide across the grease (n=1.46). Fibres are **world siblings**
of the slab (not clamp daughters), so
they can protrude past the clamp mouth without tripping the mother-containment check, and
clamp/grease booleans reuse the same footprints so the volumes stay disjoint. Built in
`BuildWLSFibres()` / `PlaceWLSFibres()` (`fUseWLSFibres`); `OpWLS` auto-enabled in
`PhysicsList::ConstructProcess()` when `GeometryMuonScint::UsesWLSWorld()` (flag set in
Build; geometry precedes physics).

**Fibre solid = analytic mitered `G4CutTubs` legs** (production, preset 7), one per straight
segment of the 4-point centreline `BentPath(x, zSpread, zBundle, Ly, conv, resL, ov)`: a
straight spread run at `fFibrePitch = 2.5 mm` (axes at z = ±1.25, ±3.75 mm: equal
0.75 mm plastic margins to the top/bottom faces, 1.5 mm between neighbouring fibres), a ramp
converging over `fFibreConvergeLen = 41 mm` (39 mm ramp + 2 mm stub, preserving the old ~3.3°
outer kink) to a touching `fBundlePitch = 1.0 mm` bundle (four +y ends bunch to a 4 mm bundle
facing the single SiPM), then a short stub before +y. `MakeCutTubeLegs` builds
each leg; `PlaceWLSFibres` places the legs as world siblings joined at exact miter planes
("analytic", `fFibreAnalytic`) — no facet edges, hence no stuck tracks. Preset 5 keeps the
legacy `G4TessellatedSolid` swept cylinder (`MakeSweptTube`) **for visualisation only**.
Multi-clad nesting: core (r=0.46) ⊂ inner PMMA clad (r=0.48) ⊂ outer fluoropolymer sheath
(r=0.50) ⊂ the leg solid. **Placement subtlety:** `fFibreCladLVs` holds *every* clad (outer
and inner) for `IsFibreLV`; the per-fibre world placement must index `fFibreOuterLVs[k]`.
Indexing `fFibreCladLVs[k]` with multi-clad on places only the inner+outer of fibres 0/1, so
the VRML/OpenGL view shows just two fibres. Paths are built slab-local in a canonical **+y-bundle / −y-mirror**
frame; `PlaceWLSFibres` then applies one `Rx180` (`flipRot`) to the film/clamp/fibre placements
so the read-out lands at slab-local **−y** (report-plan **top-left**) with the mirror caps at
**+y**. Do **not** reintroduce `G4Tubs`+`G4Orb` unions (HepPolyhedron `too many edges` →
10000-point VRML cloud).

**Booleans — avoid tangency.** The grease film and clamp channels are carved with analytic
mitered `G4CutTubs` cutters (`MakeCutTubeCutter`); the film groove is **concentric to each
fibre at `rFibre`**, NOT the larger Ø1.1 channel radius: that cutter's cylinder would be
tangent to the film's slab-side face, giving a degenerate boolean that silently breaks the
topmost channel and hangs the `TSG_OFFSCREEN` renderer. **Cutter overshoot:** every cutter is
`fHalfSize.y() + 10 mm` long so each opening is bounded by the object's own faces, never by a
face coincident with the tool (coincident faces leave degenerate polygons). **Channels must not
overlap in the straight run:** `fFibrePitch = 2.5 mm` (> 2·channel radius) keeps a solid clamp
web between neighbours (Rao Fig. 4a). In the converging bundle they *do* meet —
`fBundlePitch = 1.0 mm` < 2·`fChannelRadius` (1.1 mm) — so the four channels merge into a
single slot near the bundle (the last few mm of the 39 mm ramp), by design.

**The Ø1.1 channel / Ø1.0 fibre clearance (0.05 mm air annulus) is deliberate — do NOT make
it snug.** The real Y-11(200) is multi-clad, so the model builds an **outer fluoropolymer
sheath (n=1.42)** over the **PMMA inner clad (n=1.49)** and core (n=1.59) — toggle
`/dcs-monitor/det/muonScint/setFibreMultiClad` (default on). A snug fibre touching the n=1.55
clamp would kill the low-index gap and lose the guided light. The paper specifies grease only
in the plate↔clamp gap, not around the fibres.

**Tessellated vs analytic (vis-only) and the 2026-10-21 placement fix.**
`/dcs-monitor/det/muonScint/setFibreAnalytic <bool>` (pre-init, issue it **after** `preset`)
selects analytic mitered `G4CutTubs` fibre legs and cutters (`true`, production; presets
7/10/11) or the `G4TessellatedSolid` swept-prism path (`false`; presets 5/6). The analytic
clamp/film subtraction results have **no polyhedron**, so VRML/OpenGL fall back to a point
cloud (10 000 markers per failed solid); the tessellated path draws as a solid. The switch is
ignored for square-section fibres (always tessellated) and does not change the optical model
of the analytic path. VRML exports: `vrml/gen_all_presets.sh` and
`vrml/gen_v7opt_designs.sh` (gitignored output); the plain `wls_preset10/11*.wrl` files are
tessellated, the `*_analytic.wrl` ones production geometry but clouded.
The tessellated fibre placement was **wrong until 2026-10-21**: it passed
`rotCopyLocal(baseFibreRot)` with `baseFibreRot = localRot`, and `localRot` is an involution,
so the matrix was `I` — every side's fibres stayed in the construction frame (bundle at +y
under the mirror cap, pad on the spread end) instead of being flipped and distributed. Fixed
by placing with `G4Transform3D(fibreRot, fibrePos)` (round: `localRot`; square:
`localRot*ExtrudedFibreRot()`), the analytic-branch convention. **All earlier preset 5/6
results (and the groove-clearance / round-vs-box A/B) were taken on misoriented fibres and are
invalid.** Post-fix A/B, 100 ev centre glossy 4 GeV: preset 5 = 66.1 vs preset 7 = 81.9
pe/evt — the residual gap is the tessellated square-section grooves / 24-gon, not placement.
The film/clamp placements still use `rotCopyLocal` and keep a latent non-identity-`slabRot`
bug (harmless while `setRotation` is unused): do not reorient the WLS clamp with
`setRotation` until that is fixed.

**Read-out / clamp skin:** all four fibre read-out ends carry the `fCouplingSurface` border to
the shared read-out pad (belt-and-braces: with no surface the default is Fresnel, optically
similar). Far (**+y**, after the placement flip) ends are full-disc `G4_Al` mirror caps; the
read-out (**−y**) ends meet the shared grease pad + single SiPM. The clamp LV skin covers
**every** clamp face (channel walls and grease-facing front included), not just the outboard
one: default **white Lambertian R=0.85** (`setClampSurface white|black|mirror`; `mirror` = the
legacy all-faces 95 % specular `dielectric_metal` skin, `black` = no skin).

**Far-end mirror needs a cap skin, not just clad borders.** The borders are registered only
for the `(cladPV[k], capPV[k])` pair, but guided light sits in the **core**, which is a
daughter of the clad; the `core→cap` crossing therefore had *no* surface. Since `G4_Al` has no
`RINDEX`, the default-Fresnel path killed every core photon reaching the mirror with a silent
`NoRINDEX` (measured: 17/40 events, exactly the core→cap crossings; A/B on cfg4/refl0 showed
centre 0.186→0.260 pe/evt, +40%; edge +24%, corner +19%). Fix: `new G4LogicalSkinSurface(
"WLSFibreCapMirrorSkin", fMirrorCapLV, fFibreEndMirror)` — a skin on the cap LV covers every
face not already border-ed (core→cap, cap→world); borders take precedence, so the existing
`(clad,cap)` borders are unaffected. Verified: `NoRINDEX`=0, core→cap now `SpikeReflection`.
`DielectricMetal()` uses the surface REFLECTIVITY and never needs the metal's `RINDEX`.

**WLS core tables = digitized Kuraray Y-11 (P1).** The core `WLSABSLENGTH` is the digitized
Kuraray Y-11 absorption band (finite only in the blue, ~430 nm peak) and `WLSCOMPONENT` the
digitized Y-11 emission, both fed unconditionally (no env switch). The absolute scale is set by
`fWLSAbsLength430` (L(430 nm), default **1.0 mm**; systematic band 0.3–3 mm) via
`/dcs-monitor/det/muonScint/setWLSAbsLength <value> <unit>` (pre-init). `G4OpWLS` samples the
secondary from `WLSCOMPONENT` and requires E_sec ≤ E_absorbed, so keep `WLSABSLENGTH`
effectively transparent at and below the emission onset and zero `WLSCOMPONENT` at/above that
onset (see `.refs/geant4/examples/extended/optical/wls/src/WLSMaterials.cc`). **In the v11.3.2 tree
there is no `WSL01` fatal at all** — a grep of the whole source/install trees finds no such
message, and `G4OpWLS` merely drops the photon after 100 failed samples — so this is a
data/correctness rule, not a crash expected here. Our tables satisfy it (≈69 % of the emission
weight lies below the absorption onset; failure chance ≈10⁻⁵¹), though our `WLSCOMPONENT`
extends ~0.23 eV **above** the `WLSABSLENGTH` onset (mild self-absorption), unlike
`WLSMaterials.cc`, and the emission peak (2.583 eV / 480 nm) sits **on** the absorption onset —
so do not claim "no overlap".

**WLS re-emission is exponential** (`SetWLSTimeProfile("exponential")`, τ ≈ 7 ns), not the old
`"delta"` fixed-delay profile. This only affects the timing histograms
(`persistence_map_grid.C` / `wavelength_time_grid.C`), not yields.

**Known caveat (documented, not chased):** cfg4 collects ~67 pe/evt (whole-plate mean, glossy;
the post-P1–P6 nominal: analytic + Kuraray L0=1 mm + white clamp + multi-clad + digitized
PDE) vs cfg3 ~140 pe/evt, i.e. cfg4 < cfg3, whereas paper Fig. 12 shows cfg4 > cfg3. The paper
quotes ~30 % efficiency for the WLS-converted photons, so a large loss in that path is expected
and is **not** reproduced; a quantitative light budget has never been made. This is the
largest open item of the study (report §10). (The old production
`data_v3` value was 29.0.) The paper itself warns the configs "cannot be directly compared"
(different SiPM count / gain / input capacitance), and its cfg4 value is a pulse height over a
2500 mm² fibre-augmented effective area, not raw pe. The geometry is structurally faithful.

**Figures / vis:** collection comes from the `sipmHits` tree
(`plot_collection_efficiency.C` / `plot_whole_slab.C`). `report_figures.C::fig_wavelength_cfg`
is a 2×2 per-reflector overlay of cfg1–4: c1–3 ≈ 457.0–457.4 nm for the v6 data (blue
scintillation), c4 ≈ 509.5 nm green-shifted (confirms `G4OpWLS`). Vis `macros/muon_vis_wls.mac` / `muon_vis_wls_closeup.mac`
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
- There is no `SurfaceFluxSampler`. The sampling helper is `RootManager` (singleton,
  `RootManager::GetInstance()`), which clones a `THnSparseD` into thread-local copies and
  samples via `GetRandom` (`src/RootManager.cc`). It is **not** wired into the generator:
  the `rootManager.Initialize(...)` calls are commented out in `G4DCSmonitor.cc:81-87`, so
  today `RootManager` only carries the output `fileNum`. The `surfaceFlux` tree feeds the
  two-part `examples/` workflow; the in-binary `CASTOR440_surface` source is a synthetic
  uniform-area + cosine emitter, not a replay.

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
