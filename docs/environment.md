# Environment, build and code layout

Read on demand from `AGENTS.md`.

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
