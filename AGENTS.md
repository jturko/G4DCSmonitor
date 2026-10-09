# AGENTS.md

Geant4 + ROOT 6 simulation of a CASTOR 440/84 spent-fuel storage cask monitored
by CLYC / plastic detectors. See `README.md` for physics, geometry, and macros.

## READ-ONLY Geant4 reference tree (`.refs/`) — hard rule
- **Never modify anything reachable under `.refs/` or the Geant4 install/source
  trees it points at.** `.refs/` is a read-only reference for understanding Geant4;
  treat every path you resolve into it as read-only, for reading and searching only.
- `.refs/` contains symlinks into a machine-local Geant4 v11.3.2:
  `.refs/examples` → `~/programs/geant4/geant4-v11.3.2/examples`,
  `.refs/source` → `~/programs/geant4/geant4-v11.3.2/source`,
  `.refs/include` → `~/programs/geant4/install/include/Geant4`.
  These paths are machine-specific, so `.refs/` is gitignored — do not commit it.
- The symlinks cannot enforce read-only (Linux has no read-only symlink); this rule
  is the guarantee. Use `.refs/` to look up G4 APIs, base classes, and official
  example implementations instead of guessing.

## Build & run
- Build with CMake from `build/` (the checked-in `build/` is the working dir):
  ```bash
  cmake --build build -j          # or: cmake -S . -B build && cmake --build build -j
  ./build/G4DCSmonitor run.mac    # binary lands in build/
  ```
  Requires Geant4 (`ui_all vis_all`) + ROOT 6 (`RIO Net ROOTDataFrame Imt`);
  links the bare `Eve` library. On this machine Geant4 is under
  `~/programs/geant4/install`, ROOT under `~/programs/root/build`.
- **Ignore `GNUmakefile`** — it is the legacy Geant4 make system with a bogus
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

## MuonScint optical model (progress log)
Goal: reproduce Rao et al., J. Appl. Phys. 138, 114504 (2025) — polished 250×250×10 mm³
UPS-92S plate in a black 3D-printed holder, SiPM ASD-NUV4S-P-40 on the side faces.
Target ordering **none < TiO₂ < Al ≈ glossy**, and yield centre > edge > corner.
Work lives in `src/GeometryMuonScint.cc` / `include/GeometryMuonScint.hh`.

**Code changes and their effects**
- **Reflector model per finish** (`BuildOpticalSurfaces`), one shared 10-pt 360–650 nm
  grid (ascending eV). Reflector and bare-absorber skins are mutually exclusive.
  - `kNoReflector` (bare): **no** wrapping skin. The plate is in effective optical
    contact with the black holder (ABS n≈1.55 ≈ scint 1.58), so TIR is **frustrated**
    and trapped light leaks into the wall. Modelled by a `MuonScintHolderAbsorber` skin
    (REFLECTIVITY=TRANSMITTANCE=0). `fReflSurface` is built but **unused** here. This is
    the key fix: a free-standing polished plate is a near-lossless TIR guide and would be
    the *best* collector, contradicting the measured bare minimum.
  - `kPaintTiO2`: `groundfrontpainted` (Lambertian, **no TIR**), R≈0.955–0.983.
  - `kAluminumFoil`: `polishedbackpainted` (TIR + specular mirror, SPECULARSPIKE=1),
    R≈0.840–0.910.
  - `kGlossyPaper`: **changed** from `groundbackpainted` (Lambertian) to
    `polishedbackpainted` (lossier mirror, R≈0.900–0.955) — paper reports Al ≈ glossy and
    both highest, so glossy is not a diffuser.
  - `kTeflon`: kept for completeness, not in the paper.
- **SiPM detection via the boundary process**: grease↔SiPM is now `dielectric_metal`
  with `EFFICIENCY` = ASD-NUV4S-P-40 PDE(λ) (peak 0.43 @ 420 nm) and `REFLECTIVITY`=0.
  `G4OpBoundaryProcess` does the PDE draw, sets status `Detection`, invokes the SD —
  needs **`SetBoundaryInvokeSD(true)`** (`PhysicsList.cc`). `MuonScintSiPMSD::ProcessHits`
  therefore fires only for real photoelectrons and kills the photon (no manual draw).
- **Black holder** (`BuildHolder`, bare config only): `G4SubtractionSolid` shell
  (outer − cavity), `MuonScintHolderBlack` C:H 1.05 g/cm³ n=1.55 ABSLENGTH=1 µm, cavity =
  slab + `fHolderGap` 1.2 mm all faces. Placed/skinned only when `UsesBlackHolder()`.
- **SiPM presets fixed**: cfg1 = 8 (2/side, u=±0.5), cfg2 = 4 (1/side, centred),
  cfg3 = 2 (both on +X, u=±0.5).
- **Diagnostics/output**: `include/OpticalDiagnostics.hh` (thread-local per-event optical
  accounting + per-slab produced/detected + primary-muon first-entry x-y); new
  `opticalStats` ntuple (nScint/nCerenkov/nKilled/nDetected/nEscaped/nAlive). Toggle
  `/dcs-monitor/run/writeOpticalFluxMap` (default **off**) books the per-slab
  `h2_optflux_xy_slab<i>` **track-length only** (1 mm cells); booking is in
  `BeginOfRunAction` (needs geometry).
- **Collection efficiency lives in `sipmHits`** (not H2 maps). `MuonScintSiPMSD::EndOfEvent`
  writes one row per (event, SiPM) — including zero-detection rows — with fixed columns
  evtNb, det, sipm, nDetected, tFirst, meanWavelength_nm, rmsWavelength_nm, weight,
  muonX, muonY, nProduced. `nProduced` = all optical photons created in the slab that
  event (scint + Čerenkov). Rows are emitted only for slabs the muon entered
  (`gOpticalDiag.muonEntrySet[det]`); legacy fallback (hit-only, muonX/Y = −9999) when
  `fDetector == nullptr`. SD `EndOfEvent` runs before `EventAction::EndOfEventAction`
  (`.refs/source/event/src/G4EventManager.cc:314` vs `:323`), so it can read `gOpticalDiag`.
  The old `h2_optprod_xy_slab<i>` / `h2_optcollected_xy_slab<i>` maps were removed.
- **Tooling**: `macros/{muon_scint_template.mac,generate_muon_matrix.sh,run_muon_matrix.sh}`,
  `macros/plot_muon.C`, `macros/analyze_muon.C`, `macros/plot_collection_efficiency.C`
  (tree-based collection-efficiency maps; `plot_light_efficiency.C` is a thin forwarder
  kept for old command lines); SLURM bundle `slurm/` (12-task array, one config×reflector
  per task, 32 threads, plus whole-slab array). `slurm/run_all.sh [NEVENTS_MATRIX]
  [NEVENTS_WHOLE] [THREADS]` (default 200 / 10000 / 32) generates both macro sets and
  `sbatch --parsable`s both arrays.


**G4OpBoundaryProcess ground truth** (`.refs/source/processes/optical`): a
`G4LogicalBorderSurface` overrides a `G4LogicalSkinSurface` at the same boundary;
`RINDEX` on both materials is mandatory (else `NoRINDEX` kill); `polished`/`ground`
dielectric_dielectric → Fresnel/TIR; `groundfrontpainted` → rand-vs-R (Lambertian, no
TIR); `polishedbackpainted`/`groundbackpainted` → dielectric + backing REFLECTIVITY; no
surface object → lossless TIR default. `sigma_alpha` on `groundbackpainted` had no
effect on TIR (commit `dede7bc`).

**Test result** (200 evt, cfg1, centre, npe/event): HEAD `dede7bc` broke the ordering
(bare=363 > TiO₂=77); current model gives bare=3.4 < TiO₂=139 < Al=513 < glossy=608, with
centre>edge>corner for all four — matches the paper. Bare-absorber change is comment-only;
the bare `fReflSurface` is intentionally dead. **Next:** run `slurm/run_all.sh` on SLURM to
produce the 12-config matrix plus whole-slab (250×250) scans; collection efficiency is read
from the `sipmHits` tree by `macros/plot_collection_efficiency.C`.

## Code layout / where things actually live
- `DetectorConstruction.{hh,cc}` is the hub; `src/DetectorMessenger.cc` registers all
  `/dcs-monitor/det/...` commands, including a table-driven block for plastic / hemi /
  hall / CAD. Valid command names come from there (`setPbColMaterial`, `setPEColMaterial`, …).
- The code supports more geometry than the README's "Geometries" section (plastic
  detector, hemi shields/panels, experimental hall, CAD/GDML import). Trust the code.
- **Output trees** (`RunAction.cc`): `primary`, `hits`, `surfaceFlux`, plus a
  `meta` detector-metadata TTree written by the master *after* the MT merge; it is
  written only if absent, so deleting/re-running is safe. Toggles:
  `/dcs-monitor/run/writePrimary`, `/dcs-monitor/run/writeSurfaceFlux`.
- `SurfaceFluxSampler` is a master-side singleton; step-1 `surfaceFlux` coordinates are
  **cask-local**, loaded once then sampled O(1) across threads. Geometry params
  (outer radius/height) are passed at load time.

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
- Prefer a single command per tool call. Chained commands cannot match
  the permission allowlist, so each one requires manual approval.
- Where possible, avoid using `&&`, `;`, `||`, instead running each shell command on its own.
