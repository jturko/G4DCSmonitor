# SLURM bundle: MuonScint scan matrix + whole-slab irradiation

Self-contained job scripts for the MuonScint detector study (Rao et al.,
J. Appl. Phys. 138, 114504). Two arrays are submitted:

1. **Matrix scan** (`run_matrix.sbatch`, 16 tasks) -- 4 SiPM configs x 4
   reflectors, each running centre/edge/corner back-to-back in one Geant4
   process. For the Npe yield / photon-budget / efficiency comparisons.
2. **Whole-slab scan** (`run_whole.sbatch`, 16 tasks) -- same 16 configs x
   reflectors, but with the muon beam covering the **full 250 x 250 mm^2**
   plate. The light-collection-efficiency map (vs muon interaction x-y) is
   reconstructed offline from the `sipmHits` tree.

Config 4 is the single-SiPM + WLS-fibre clamp layout (geometry preset 5);
configs 1-3 stay presets 1-3.

Both arrays request `--cpus-per-task=32`; this **must** match `THREADS` passed
to the generators (default 32).

## Versioned data directories (`data_<VERSION>/`)

Each data set is a version root holding both campaigns:

```
${DATAROOT}/data_<VERSION>/
├── matrix/   centre/edge/corner scan   (48 files)
└── whole/    full-plate efficiency map (16 files)
```

Report v1 is frozen under `versions/v1/`; the v2 work has two data generations:

| Version | Data | Note |
|---|---|---|
| `v2.1` | `data_v2.1/{matrix,whole}` | previous set (pre WLS-orientation flip); `whole/` is incomplete |
| `v2.2` | `data_v2.2/{matrix,whole}` | **current** set (script default) |

The tag is controlled by `VERSION` (default `v2.2`). `DATAROOT` defaults to the
directory you run the driver from (`$PWD`); set `DATAROOT=/bigdata/<user>/muon`
to write elsewhere.
`run_all.sh` / `run_fast.sh` export `VERSION` so both arrays (and the generated
macros) agree. The report pipeline defaults to `slurm/data_v2.2/{matrix,whole}`
(`macros/build_report_figures.sh`).

## Fast + priority-aware path (`run_fast.sh`) -- recommended

`run_all.sh` (above) targets the default `cpu-genoa` queue. If that queue is
congested (SLURM reason `Priority`) use the self-contained **`run_fast.sh`**
instead -- it does **not** touch any of the existing scripts, writes its own
macro sets (`muon_fast/`, `muon_whole_fast/`) and logs, and adds:

- **flux map OFF** for the matrix (measured 3x faster: 0.23 vs 0.70 s/event).
  The efficiency comes from the `sipmHits` tree, not `h2_optflux`.
- **`--mem=4G`** (measured peak RSS ~0.4 GB). Short walltimes backfill much
  better than the old 2-day times.
- **routed to `low`** by default (`-p low -A low`): 145 idle nodes and a
  fair-share of 0.64 vs 0.005 on `default`.

Measured walltimes (local 4-physical-core box, flux map off):

| Threads | whole-slab per-event | whole-slab 10k ev |
|---|---|---|
| 1 | 1.44 s | ~4 h |
| 2 | 0.77 s | ~2.1 h |
| 4 | 0.40 s | ~1.1 h |
| 8 | 0.37 s | ~61 min (box saturated at 4 cores) |

Walltime scales ~**1/N** with thread count, so on a 32-core node a whole-slab
task is **~9-11 min**; the default `--time=02:00:00` keeps margin if you drop
to 8-16 threads on a busy node (do **not** shrink it to 1h unless you keep
32 threads). Matrix at 32 cores is ~24 s/task; `--time=01:00:00` is ample.

```bash
slurm/run_fast.sh [MATRIX_EVENTS] [WHOLE_EVENTS] [MATRIX_THREADS] [WHOLE_THREADS]
# defaults: 200 10000 32 32
```

Routing is fully overridable, per array, via environment:

```bash
# whole-slab on the guaranteed default queue (no preemption), matrix on low:
WHOLE_PARTITION=cpu-genoa WHOLE_ACCOUNT=default WHOLE_TIME=02:00:00 ./run_fast.sh
# throttle both arrays to 4 concurrent tasks (be kinder to fair-share):
ARRAY_LIMIT=4 ./run_fast.sh
# write to an explicit path (default is the current directory):
DATAROOT=/bigdata/<user>/muon ./run_fast.sh
# cap memory per task at 2 GB:
MATRIX_MEM=2G WHOLE_MEM=2G ./run_fast.sh
```

### What `low` is

The name `low` exists in **three** SLURM namespaces; `run_fast.sh` sets the
partition and account:

| Namespace | Flag | In your site's output | Effect |
|---|---|---|---|
| Partition | `-p` | `sinfo` row `low` (177 nodes, 5-day limit, ~145 idle) | *which nodes* |
| Account | `-A` | `sshare` row `low` (`RawUsage=0`, `FairShare=0.64`) | *fair-share bucket* |
| QOS | `--qos` | `qos_low` (`Priority=0`, `MaxSubmitPU=1000`) | policy/limits |

`qos_low` Priority 0 means it is very likely **preemptible**. A preempted
Geant4 run restarts from scratch (no checkpointing), so route the long
whole-slab jobs to a guaranteed queue if that is unacceptable.

**Verify access before submitting** (permission is a separate gate from
"partition is up"):

```bash
scontrol show partition low | grep -Ei "AllowAccounts|AllowQos|Preempt|MaxTime"
sacctmgr show assoc user=$USER format=Account%15,Partition%15,QOS%15
sbatch --test-only -p low -A low -t 00:10:00 -n1 --wrap=true   # validates, queues nothing
```

## Running on the cluster (workflow + knobs)

Pick the directory that should hold the output, `cd` there, and run the driver
by its **absolute** path. `DATAROOT` defaults to the current directory, so the
ROOT files land right where you ran it:

```bash
cd /bigdata/<user>/muon                     # <-- output goes here
BIN=/abs/path/to/G4DCSmonitor \
    /abs/path/to/repo/slurm/run_fast.sh 500 20000 16 16
# matrix: 500 ev/position, whole-slab: 20000 ev, 16 threads each,
# -> /bigdata/<user>/muon/data_v2.2/{matrix,whole}/
```

`BIN` is the **only** thing you point at a specific build/binary version; it is
exported to both job arrays. If unset, resolution is: pre-built HZDR binary (if
present) → `../build/G4DCSmonitor` next to `slurm/`.

Logs and generated macros always stay in `slurm/`, and the macros carry
**absolute** output paths, so the jobs write to `DATAROOT` regardless of the
working directory the array tasks start in.

### Knobs

| What | `run_fast.sh` | `run_all.sh` |
|---|---|---|
| matrix events/position | `$1` / `MATRIX_EVENTS` (200) | `$1` / `NEVENTS_MATRIX` (200) |
| whole-slab events | `$2` / `WHOLE_EVENTS` (10000) | `$2` / `NEVENTS_WHOLE` (10000) |
| threads == `--cpus-per-task` | `$3`/`$4` / `MATRIX_THREADS`/`WHOLE_THREADS` (32) | `$3` / `THREADS` (32) |
| memory per task (`--mem`) | `MATRIX_MEM`/`WHOLE_MEM` (**4G**) | `MATRIX_MEM`/`WHOLE_MEM` (**64G**) |
| output root | `DATAROOT` (default `$PWD`) | `DATAROOT` (default `$PWD`) |
| data-set version | `VERSION` (default `v2.2`) | `VERSION` (default `v2.2`) |
| binary | `BIN` | `BIN` |
| partition / account / time / array | `MATRIX_*` / `WHOLE_*` env | edit `.sbatch` `#SBATCH` lines |

Both drivers pass `--cpus-per-task` **and** `--mem` to `sbatch`, overriding the
`#SBATCH` defaults, so threads stay in sync with the `/run/numberOfThreads`
baked into the macros. The two event counts are independent.

```bash
# 500 matrix / 20000 whole, 16 threads, 8 GB per task (positional form):
./run_fast.sh 500 20000 16 16        # ...with MATRIX_MEM=8G WHOLE_MEM=8G
# equivalent, all via the environment:
MATRIX_EVENTS=500 WHOLE_EVENTS=20000 MATRIX_THREADS=16 WHOLE_THREADS=16 \
  MATRIX_MEM=8G WHOLE_MEM=8G ./run_fast.sh
```

> `run_all.sh` uses the flux-map (heavier) templates and requests `64G` by
> default; `run_fast.sh` turns the matrix flux map off and needs only `4G`
> (measured peak RSS ~0.4 GB). Lower `*_MEM` if the queue charges by memory.

Both drivers only **generate macros and submit**; they do not build or run the
analysis.

## What is here

| File | Purpose |
|---|---|
| `muon_scint_template.mac` | Matrix template (`@CONFIG@ @REFLECTOR@ @NEVENTS@ @THREADS@ @DATADIR@`) |
| `whole_slab_template.mac` | Whole-slab template (same tokens) |
| `generate_muon_matrix.sh` | Emits `muon/scan_c<C>_r<R>.mac` (16 macros) + `muon/manifest.txt` |
| `generate_whole_slab.sh` | Emits `muon_whole/whole_c<C>_r<R>.mac` (16 macros) + manifest |
| `run_all.sh` | Generates both sets and submits both arrays |
| `run_matrix.sbatch` | Job array `--array=0-15` for the matrix scan |
| `run_whole.sbatch` | Job array `--array=0-15` for the whole-slab scan |
| `build.sbatch` | One-off build of `../build/G4DCSmonitor` |
| `collect.sh` | Count / list / rsync-back helper |
| `run_fast.sh` | **Fast/priority-aware driver** (flux off, 4G/1h, `low`) |
| `run_matrix_fast.sbatch` | Job array for the fast matrix scan |
| `run_whole_fast.sbatch` | Job array for the fast whole-slab scan |
| `muon_scint_template_fast.mac` | Matrix template with the flux map off |

The array index maps deterministically for both:

```
 0=c1_r0  1=c1_r1  2=c1_r2  3=c1_r3
 4=c2_r0  5=c2_r1  6=c2_r2  7=c2_r3
 8=c3_r0  9=c3_r1 10=c3_r2 11=c3_r3
12=c4_r0 13=c4_r1 14=c4_r2 15=c4_r3
```

## Adapt to your cluster

Edit the **USER CONFIG** block at the top of `run_matrix.sbatch`,
`run_whole.sbatch`, and `build.sbatch`:

- load your Geant4 (`ui_all vis_all`) + ROOT 6 (`RIO Net ROOTDataFrame Imt`)
  modules -- either `module load <your-single-modulefile>` or the explicit
  `module purge` + `module load gcc/... root/... cmake/... xerces/... qtbase/... geant4/...`
  list (currently set to the HZDR set: gcc/15.2.0, root/6.32.14, cmake/4.0.3,
  xerces/3.3.0, qtbase/6.10.1, geant4/11.4.0), and
- set `BIN` to the built executable. The default resolution order is:
  1. `BIN` if already exported,
  2. the pre-built HZDR binary `/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor` if present,
  3. `../build/G4DCSmonitor` next to `slurm/`.
  `run_all.sh` resolves `BIN` once and exports it to both arrays.
- adjust `--mem`, `--time`, `--partition`/`--account`, and `--mail-user` to taste.

If you already have a built `G4DCSmonitor` elsewhere, skip `build.sbatch` and run,
e.g., `BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ./run_all.sh`.

Each `.sbatch` runs from `$SLURM_SUBMIT_DIR` and exports `OMP_NUM_THREADS`.
The array task reads its macro from `muon*/manifest.txt` (falling back to a
built-in mapping if absent), so the 16-task order is always consistent with the
generator.

## Outputs

ROOT files land under `${DATAROOT}` (default: the directory the driver was run
from; override with `DATAROOT=/bigdata/<user>/muon`):

- Matrix: `${DATAROOT}/data_<VERSION>/matrix/muon_out_c<C>_r<R>_<pos>.root` (48 files)
- Whole-slab: `${DATAROOT}/data_<VERSION>/whole/muon_whole_out_c<C>_r<R>.root` (16 files)
- Logs (always in `slurm/`): `slurm/logs/muonscan_<jobid>_<arrayid>.out|.err`
        `slurm/logs/muonwhole_<jobid>_<arrayid>.out|.err`

List what was produced and get an rsync line:

```bash
DATAROOT=/bigdata/<user>/muon VERSION=v2.2 ./collect.sh
```

If `DATAROOT` is not on local disk, fetch back and analyse:

```bash
rsync -avP <cluster>:/bigdata/<user>/muon/data_v2.2/ ./data_v2.2/
# then locally:
root -l -b -q 'macros/plot_muon.C+("data_v2.2/matrix")'
root -l -b -q 'macros/plot_collection_efficiency.C+("data_v2.2/whole")'
macros/build_report_figures.sh slurm/data_v2.2/matrix slurm/data_v2.2/whole
```

## Notes

- The collection efficiency (nDetected / nProduced) is **not** stored as a
  histogram. `MuonScintSiPMSD` writes, per (event, SiPM) row of the `sipmHits`
  tree, the muon first-entry slab-local `muonX`/`muonY` and the event's
  `nProduced` photons in the slab; `nDetected` is the numerator. Zero-detection
  SiPMs are written too. Bin offline at any cell size and for any SiPM subset
  (see `macros/plot_collection_efficiency.C`).
- The matrix macros still enable the optional per-slab optical-photon
  track-length flux map (`h2_optflux_xy_slab0`); the whole-slab macros leave it
  off (the tree already carries the product).
- `/run/numberOfThreads` is set by the generator; the matrix macro's three
  `/run/beamOn` blocks reuse one geometry build, so no per-position process is
  needed.
- Re-running an array task overwrites its own files; other tasks are
  independent.
