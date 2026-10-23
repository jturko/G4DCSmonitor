# AGENTS.md

Geant4 + ROOT 6 simulation of a CASTOR 440/84 spent-fuel storage cask monitored
by CLYC / plastic detectors, plus the MuonScint optical-plate study. Physics,
geometry and user macros are in `README.md`; current status is in
`workitem_muon_status.md`. This file holds only durable operating rules — detailed
findings live in `docs/` and are read on demand (see the routing table below).

## Hard rules
- **Never modify anything under `.refs/`** or the Geant4/ROOT install and source
  trees they point at. Read and search only; symlinks cannot enforce this.
- **Never recursively search `~/bigdata`** — it is sshfs and freezes the login node.
- **HPC data root is `/bigdata/DCS-Monitor-II/turko46/MuonScinti`** (native path,
  for every `DATAROOT`, generated `/analysis/setFileName`, and `sbatch`). The
  login-node mirror `~/bigdata/DCS-Monitor-II/turko46/MuonScinti` is for inspection
  only — never pass it as a job's `DATAROOT`. The two paths are not interchangeable.
- **There is no SLURM on this login node.** Local runs use `build/G4DCSmonitor`;
  production goes to the HPC. The binary is never taken from `$PATH`; every driver
  resolves its own absolute path. Pin a build with `BIN=/abs/path/G4DCSmonitor`
  (mandatory on the HPC).
- There is no test suite, lint, or CI. Verification = build cleanly + run a macro;
  treat clangd diagnostics only as hints (ground truth is `cmake --build build -j`).
- **Keep this file ≤ 150 lines.** New findings go into the relevant `docs/*.md`;
  add at most one routing line here.

## Worktrees (this machine)
- `../G4DCSmonitor` = branch `main` (CASTOR + biasing); this tree =
  `1-muon-trigger-detector` (MuonScint). Each has its own `build/` and `report/`.
- A `build/` cannot be moved between trees (`CMakeCache.txt` pins the source dir).

## Build & run
```bash
cmake --build build -j
./build/G4DCSmonitor run.mac        # $1 = macro, $2 = optional fileNum
```
- Configure: `cmake -S . -B build -DGeant4_DIR=$HOME/programs/geant4/install/lib/cmake/Geant4 -DROOT_DIR=$HOME/programs/root/build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`
- Ignore `GNUmakefile` (legacy Geant4 make system with a bogus `G4INSTALL`); CMake is real.

## CLI and command ordering
- `G4DCSmonitor [macro] [fileNum]` — positional only, no flag parsing. `fileNum`
  suffixes the output name; `argc==1` starts the interactive UI.
- Importance biasing is disabled, not configurable: the `G4ImportanceBiasing` block
  and the parallel-world registration are commented out, so `useBiasing` changes
  nothing (`PhysicsList.cc`, `DetectorConstruction.cc`, `GeometryParallelBiasing.*`).
- Pre-init geometry commands (`setPosition`, `setRotation`, `.../add`, biasing,
  preset/surface knobs) must precede `/run/initialize`; gun, output toggles and
  `/run/beamOn` follow it.
- Source modes: `GPS`, `CASTOR440_surface`, `CASTOR440_fuel`, `CASTOR440_fuel_biased`
  (truth: enum in `PrimaryGeneratorAction.hh`; `CASTOR440_surface` is analog, not a replay).

## Traps worth inlining
- A `NoRINDEX` kill is silent (prints only at verbose>1): its absence from a log
  proves nothing. Count it with `/process/optical/boundary/verbose 2`.
- For pe/event, sum `nDetected` over all SiPMs and divide by the number of events
  (`evtNb`), **not** by row count.
- `muonEntryX/Y` are slab-local with the slab rotation undone, so a global
  `/dcs-monitor/det/setRotation` is a no-op for every output. Report macros map
  simulation→report with `(x,y) → (y,−x)`.

## Code layout (hub)
- `DetectorConstruction.{hh,cc}` is the hub; `src/DetectorMessenger.cc` registers
  all `/dcs-monitor/det/...` commands (table-driven for plastic/hemi/hall/CAD).
  Valid command names come from there; trust the code over the README.
- Output trees (`RunAction.cc`): `primary`, `hits`, `surfaceFlux`, plus a master-written
  `meta` tree. Toggles: `/dcs-monitor/run/writePrimary|writeSurfaceFlux`.
- `RootManager` is a sampling helper, not wired into the generator; today it only
  carries the output `fileNum`. There is no `SurfaceFluxSampler`.
- The code supports more geometry than the README (plastic detector, hemi shields,
  experimental hall, CAD/GDML import).

## Read on demand
Read the relevant file with the Read tool before changing that area.
| When doing | Read |
|---|---|
| build/worktree/env problems, `.refs/`, code layout, examples | `docs/environment.md` |
| optics, reflectors, holder, contact fraction, presets, beam regions | `docs/muon-optics.md` |
| Config 4 / WLS fibres, clamps, Kuraray tables | `docs/config4-wls.md` |
| reading `sipmHits`, computing yield/PE, photon visualisation | `docs/output-and-diagnostics.md` |
| running macros, plots, movies, position recon / CFD | `docs/analysis-macros.md` |
| submitting SLURM, choosing a data tree, starting a campaign | `docs/campaigns.md` |
| regenerating the report or editing its structure | `docs/report.md` |
| Geant4 optical boundary-process semantics | `docs/geant4-reference.md` |

## Shell commands
- Prefer a single command per tool call. Chained commands cannot match the permission
  allowlist, so each one requires manual approval.
- Where possible, avoid using `&&`, `;`, `||`; run each shell command on its own.
