# AGENTS.md

Geant4 + ROOT 6 simulation of a CASTOR 440/84 spent-fuel storage cask monitored
by CLYC / plastic detectors. See `README.md` for physics, geometry, and macros.

## Two-worktree layout (this machine)
This tree is the `main` branch of a two-worktree checkout; the MuonScint work lives
in a second worktree next to it.

| path | branch | content |
|---|---|---|
| `/home/turko46/geant4-sims/G4DCSmonitor` | `main` | CASTOR 440/84 + biasing code (this tree) |
| `/home/turko46/geant4-sims/G4DCSmonitor_muon` | `1-muon-trigger-detector` | MuonScint detector, report and SLURM drivers |

- `git worktree list` shows both; each tree keeps its **own** `build/` (a build dir
  cannot be moved: `CMakeCache.txt` pins `CMAKE_HOME_DIRECTORY` to its tree) and its
  own `report/` output tree.
- Configure here with
  `cmake -S . -B build -DGeant4_DIR=$HOME/programs/geant4/install/lib/cmake/Geant4
  -DROOT_DIR=$HOME/programs/root/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
  (`cmake --fresh` re-configures from scratch without deleting the directory).
- There is **no global install** of the binary and nothing is taken from `$PATH`;
  the drivers resolve `build/G4DCSmonitor` relative to their own location and exec
  the absolute path. `BIN=/abs/path/G4DCSmonitor` overrides it.
- `main` has no `report/` and no `macros/muon/`; it has `slurm/` drivers for the
  p1.5 background-sector production campaign (see `slurm/README.md`); its
  `macros/run.mac` needs **both** `--biasing gamma` on the command line and
  `/dcs-monitor/det/useBiasing true` in the macro (see *CLI / biasing*).
- The login node reaches HPC data through the sshfs mount `/mnt/bigdata`;
  `~/bigdata`, `~/my-bigdata-dcs`, and `~/my-bigdata-rimanus` are symlinks into it.
  Never search it recursively (see the global rules).

## READ-ONLY reference trees (`.refs/`) — hard rule
- **Never modify anything reachable under `.refs/` or the Geant4/ROOT install and
  source trees it points at.** `.refs/` is a read-only reference for understanding
  Geant4 and ROOT; treat every path you resolve into it as read-only, for reading
  and searching only.
- `.refs/` mirrors the layout used by the MuonScint worktree, with one directory per
  project:
  - Geant4 (v11.3.2): `.refs/geant4/examples` →
    `~/programs/geant4/geant4-v11.3.2/examples`, `.refs/geant4/source` →
    `~/programs/geant4/geant4-v11.3.2/source`, `.refs/geant4/include` →
    `~/programs/geant4/install/include/Geant4`.
  - ROOT (v6.36.02): `.refs/root/include` → `~/programs/root/build/include` (the
    configured install headers, e.g. `TFile.h`, generated `RConfigure.h`),
    `.refs/root/source` → `~/programs/root/root-6.36.02` (full source tree,
    per-module `inc/` + `src/`), `.refs/root/examples` →
    `~/programs/root/root-6.36.02/tutorials` (the source tree already contains
    the tutorials; this link is for parity with `.refs/geant4/examples`).
- These paths are machine-specific, so `.refs/` is gitignored — do not commit it.
  Re-create the symlinks in any new worktree.
- The symlinks cannot enforce read-only (Linux has no read-only symlink); this rule
  is the guarantee. Use `.refs/` to look up G4/ROOT APIs, base classes, and official
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
