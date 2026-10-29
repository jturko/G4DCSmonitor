# `slurm/` — p1.5 background-sector production on the HPC

SLURM package for the p1.5 background-sector production campaign: all
background sources, casks, hexants and fuel assemblies, replayed from the
p1 surface-flux files into the 64-cell sector scoring shell, producing a
per-config library of `sectorFlux` + `sectorMeta` trees for later
background subsampling with the `/dcs-monitor/bkg/` replay (`p2_bkgd`).

Validated model and the two-stage workflow are described in
`report/sector_implementation/main.tex`. The drivers follow the
`ZLN_setup_v4` templates (`generate_macros.sh` / `submit_macro.sh` /
`rerun_failed.sh`) and the MuonScint `slurm/` conventions; the geometry
template is ZLN's `macros/no_detector/` (12 CASTOR 440/84 casks + concrete
hall, **no detector placed**).

## Where this runs

| script | runs where |
|---|---|
| `run_local.sh`, `merge_config.sh`, `verify_p15.sh`, `generate_p15_macros.sh` | any machine with a `build/G4DCSmonitor` + local `root` — the login node/laptop for smoke tests (no SLURM there) |
| `build.sbatch`, `run_p15.sh`, `p15_job.sbatch`, `rerun_failed.sh` | **the HPC**, from a copy/clone of this repo's `slurm/` dir (scripts resolve their own location, so they can be invoked from any cwd) |

Nothing needs to be created by hand on the HPC except a copy of this repo
(`build.sbatch` builds its `build/G4DCSmonitor`; do **not** copy a local
`build/` dir there — `CMakeCache.txt` pins paths, `--fresh` handles it) and
the p1 files it expects at `P1_BASE_DIR`
(default `/home/turko46/data/SurfaceFlux_v3`, confirm with `ls`).
Everything else is created by the drivers:

- `run_p15.sh` runs `mkdir -p` for `macros_<VERSION>/p15` and
  `data_<VERSION>/logs` — the whole `DATAROOT` tree springs into
  existence on first call (also in `DRY_RUN=1`, only the macros get
  written).
- per-config output dirs (`data_<VERSION>/p15/<source>/cask<C>/gFuel<G>/`)
  are created by `RunAction` when it opens the analysis file.

## What one task produces

One array task = one p1.5 config = (baseFuel, source, cask, hexant):

- geometry built once per task: target cask 0 (passive) + background casks
  1–11 + hall; the 64-cell sector shell (8 sectors × 8 z-layers) is built
  in a parallel world centered on target cask 0
- p1 file loaded once per task (`CASTOR440_surface_from_TTree`), then
  `T_gen = 180 s` written as **10 chunks of 18 s** (one output file per
  chunk — 18 s ≈ 1.4 GB for gamma 662 keV; single ~2.7 GB files proved
  unreadable in the validation)
- in-task merge: `hadd` the 10 chunks, then `consolidate_meta.C` rewrites
  `sectorMeta` to a **single row with `T_gen = 180 s` and `M` = total
  entry count**. This is required because `SectorFluxSampler::DoLoad`
  reads only `sectorMeta` entry 0 — a hadded file with per-chunk rows
  would replay one chunk's `T_gen`.
- normalization is the validated p1.5 rule: the sampler computes
  `N = λ · T_gen · r` per chunk from the p1 file's decay rate `λ` and
  kept-weight ratio `r`; the replay normalizes with
  `N_replay = M_unique · T_replay / T_gen` using the consolidated row.

Replay entries carry unit weight, so subsampling is a straight fraction of
the library entries.

## Matrix and defaults

| knob | value |
|---|---|
| sources | 5 (`ZLN_setup_v4` rates): neutron_Watt 4.16e6 (sourceType 10), gamma_662keV 1.4e14 (28), gamma_1173keV 1.0e11 (22), gamma_1332keV 1.0e11 (20), isotope_Eu154 7.0e10 (25) |
| base fuels | 0 1 2 3 5 6 7 8 13 14 15 22 23 32 (`_orbit_map.sh` maps (base, hexant) → `globalFuel`) |
| source casks | 1–11 (target cask 0 is passive and hosts the shell) |
| hexants | 0/60/120/180/240/300 deg |
| tasks | 5 × 14 × 11 × 6 = **4620**, neutron tasks first (longest first) |
| resources | 32 CPUs / 64 GB / 6 h per task (ZLN-proven; the 23 GB p1 load fits) |
| partition / account | cluster default (`PARTITION`/`ACCOUNT` env to override) |

Manifest task index layout (for `SUBSET=`): one source block of 924
consecutive indices, ordered neutron_Watt 0–923, gamma_662keV 924–1847,
gamma_1173keV 1848–2771, gamma_1332keV 2772–3695, isotope_Eu154 3696–4619.
Within a block: baseFuel-major, then cask, then hexant — so consecutive
six-packs are one cask's six hexants (e.g. tasks 924–929 = gamma_662keV
baseFuel 0, cask 1, all six hexants; adding cask 2 shifts by +6, ...,
cask 11 by +60, and +66 starts baseFuel 1).

Measured cluster rates (ZLN v4 logs, 32 threads): gamma 662 keV
≈ 2.9e5 evt/s → ~19 min per config; neutron Watt ≈ 5.6e3 evt/s, ~3.75e7
events → ~1.9 h per config. At a `%128` throttle the campaign is roughly
2 days, dominated by neutrons. Storage: gamma-662 configs are ~14 GB each
merged (~13 TB for 924 configs); keep `KEEP_CHUNKS=0` only after the
consolidation is trusted, otherwise the chunks double that footprint.

## Files

| file | role |
|---|---|
| `generate_p15_macros.sh` | writes one macro per config + `manifest.txt` (line i = array task i−1: `macro\|cfgdir\|merged-stem\|nchunks`) |
| `run_p15.sh` | driver: knobs, generate, `sbatch` the array (`DRY_RUN=1` prints only) |
| `p15_job.sbatch` | array task: binary `--mode background-sector`, then in-task merge |
| `merge_config.sh` | hadd + consolidate + verify + optional chunk deletion (shared with `run_local.sh`) |
| `consolidate_meta.C` | sectorMeta consolidation (single row, `T_gen` = sum) |
| `verify_p15.sh` / `verify_meta.C` | campaign verification (meta sanity, `M` vs `sectorFlux` entries; `--deep` also checks cellId coverage) |
| `rerun_failed.sh <job-id>` | resubmit failed array tasks (sacct/squeue logic from ZLN) |
| `build.sbatch` | build `../build/G4DCSmonitor` on the cluster (module stack) |
| `run_local.sh` | local smoke test of the whole path without SLURM |
| `p2_replay_example.mac` | p2_bkgd template for replaying one library file into a placed detector (new files: edit `sourceFile`/`setFileName`) |

## Usage

```bash
# 0. one-off build on the cluster (module stack inside)
sbatch build.sbatch

# 1. smoke-test locally on the login node or laptop (no SLURM):
./run_local.sh                 # tiny exposure, baseFuel 0 / cask 1 / rot 0

# 2. dry-run the submission (generates macros + manifest, prints sbatch cmd)
DRY_RUN=1 ./run_p15.sh

# small first batch: gamma_662keV, baseFuel 0, casks 1-2 (12 tasks)
SUBSET=924-935 DRY_RUN=1 ./run_p15.sh          # check the printed sbatch line
SUBSET=924-935 ./run_p15.sh                    # real submission
# full campaign later, all sources:
./run_p15.sh

# 3. resubmit failures, then verify the library
./rerun_failed.sh <array-job-id>
./verify_p15.sh                # add --deep on a subset for cell coverage
```

## Layout on the HPC

```
DATAROOT (default /bigdata/DCS-Monitor-II/turko46/SectorBkgd)   # native /bigdata path ONLY
├── macros_v1/p15/
│   ├── run_baseFuel<NN>_<source>_cask<C>_gFuel<G>.mac   (4620 files)
│   └── manifest.txt
└── data_v1/
    ├── p15/<source>/cask<C>/gFuel<G>/p15_<source>_cask<C>_gFuel<G>.root
    │     └── ..._chunk01..chunk10.root                  (kept unless KEEP_CHUNKS=0)
    └── logs/                                            # job logs, next to the data
        p15_<jobid>_<task>.out / .err, verify_p15.log
```

The macro files are self-contained and bake absolute paths (p1 source,
output dirs), so the campaign location is pinned into the macros — after
moving `DATAROOT`, rerun `run_p15.sh` to regenerate them. Deleting the
`sectorMeta` write target is safe: `RunAction.cc` writes it only if
absent, so re-running a task overwrites cleanly.

## Using the library (p2_bkgd replay)

Reference example: `p2_replay_example.mac` in this directory — geometry as
in production p1.5 (12 casks + hall, 8 z layers), then the CLYC at the
desired position and the `/dcs-monitor/bkg/` replay. Launch it with the
same CLI as production (`--mode background-sector`, no `--biasing`): the
replay itself only reads the library file, but `autoCells` resolves cell
ids through the sector-shell geometry of the parallel world, and the CLYC
hit trees come from the main-world SDs.

For a given detector position:

- place the detector with the `setPosition`/`setRotation`/`clyc/add`
  commands **before** `/run/initialize`;
- cell-selection precedence: explicit `cellIds` > `zLayers`/`sectorId` >
  `autoCells`. `autoCells true` maps the detector envelope onto the shell
  cells it overlaps — for the nominal CLYC position those are the two
  sectors it straddles at `z` layer 4 (cellIds 32 and 39);
- `N_replay = M_unique · T_replay / T_gen`, `T_gen` taken from the
  library's consolidated `sectorMeta` row (180 s). `T_replay = 180 s`
  replays every entry of the selected cells exactly once;
  `T_replay > T_gen` oversamples each selected entry; `T_replay < T_gen`
  subsamples.

One library file covers ONE config (source × baseFuel × cask × hexant).
To cover several configs, use one block per config:

```
/dcs-monitor/bkg/sourceFile <config-A library>
/dcs-monitor/bkg/sectorId -1
/dcs-monitor/bkg/autoCells true
/analysis/setFileName <outA>
/dcs-monitor/bkg/startRun 180 s

/dcs-monitor/bkg/sourceFile <config-B library>
/analysis/setFileName <outB>
/dcs-monitor/bkg/startRun 180 s
```

(each `/bkg/startRun` re-loads the current `sourceFile` with the filter
flags set before it). The equal-time config contributions are then summed
over their output files in the analysis stage, one per config — same
pattern as the p2 validation macros. Statistical scale: a production
library file carries ~3.3e8 entries over 64 cells, a smoke-scale one
~17k — the smoke file's 29 entries in the detector's two cells are too
few to guarantee crystal intercepts (GPS probe on the same placement:
71 hits from 100 events, so the detector chain itself is fine).

## Gotchas

- The binary is **never taken from `$PATH`**; `BIN=...` pins a build,
  otherwise `${REPO}/build/G4DCSmonitor` (absolute, resolved from the
  script's own location — scripts run from anywhere).
- `--mode background-sector` on the CLI is mandatory (it registers the
  parallel world before the macro executes); do **not** pass `--biasing`
  with it (`--biasing` is rejected in this mode).
- `/dcs-monitor/det/setSectorZLayers 8` must precede `/run/initialize`
  (the generator places it with the other pre-init commands).
- Data root on the HPC is the native `/bigdata/...` path. The
  `~/bigdata/...` view is an sshfs mount for inspection only — never
  point `DATAROOT`, generated macros, or sbatch at it. Also never
  recursively search the sshfs mirror from a login node (it freezes it).
- One macro runs 10 chunk blocks, each with its own `/analysis/setFileName`;
  each chunk's `sectorMeta` has its own `T_gen`/`M` row — only the merged
  file (after `merge_config.sh`) carries the consolidated single row.
- Random seed: `main()` seeds from `std::random_device` per process; the
  seed is printed to the job log (`showEngineStatus`) — recover it from
  the log for reproducibility.
- `sourcePid` is a no-op filter for these pid-homogeneous p1 files (0 for the
  gamma/Eu154 files, 2112 for the Watt file; ZLN v4 sets 0 for all);
  `sourceType` tags are free labels written to `sectorFlux`/`sectorMeta` for
  later filtering.
- A missing p1 file skips that config with a warning (check the generator
  summary); `verify_p15.sh` reports any config whose library file never
  appeared.
