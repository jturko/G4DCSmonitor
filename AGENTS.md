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
