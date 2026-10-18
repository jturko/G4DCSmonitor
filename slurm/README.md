# SLURM bundle: MuonScint matrix, whole-slab, contact, and Al-backing scans

Self-contained job scripts for the MuonScint detector study (Rao et al.,
J. Appl. Phys. 138, 114504). Four arrays are submitted:

1. **Matrix scan** (`run_matrix.sbatch`, 16 tasks) -- 4 SiPM configs x 4
   reflectors, each running centre/edge/corner back-to-back in one Geant4
   process. For the Npe yield / photon-budget / efficiency comparisons.
2. **Whole-slab scan** (`run_whole.sbatch`, 16 tasks) -- same 16 configs x
   reflectors, but with the muon beam covering the **full 250 x 250 mm^2**
   plate. The light-collection-efficiency map (vs muon interaction x-y) is
   reconstructed offline from the `sipmHits` tree.
3. **Contact scan** (`run_contact.sbatch`, 21 tasks by default) -- bare plate
   (reflector 0), configs 1-3, sweeping the effective black-holder contact
   fraction `f = 0.000, 0.005, 0.010, 0.015, 0.020, 0.025, 0.030`. Each task
   runs centre/edge/corner back-to-back. Config 4 has no black holder, so it is
   excluded by default.
4. **Al-backing scan** (`run_al.sbatch`, 6 tasks by default) -- Al foil
   (reflector 2), configs 1-3, comparing the backing index `grease` (n=1.46,
   default) vs `air` (n=1.0003). Each task runs centre/edge/corner back-to-back.

Config 4 is the single-SiPM + WLS-fibre clamp layout (geometry preset 7, the
analytic G4CutTubs variant; preset 5 is the legacy tessellated vis-only variant);
configs 1-3 stay presets 1-3.

All arrays request `--cpus-per-task=32` by default; this **must** match
`THREADS` passed to the generators (default 32). The contact/Al arrays use the
fast characterization template (flux map OFF); their yield also comes from the
`sipmHits` tree.

## Versioned data directories (`data_<VERSION>/`)

Each data set is a version root holding all four campaigns:

```
${DATAROOT}/data_<VERSION>/
├── matrix/   centre/edge/corner scan          (48 files)
├── whole/    full-plate efficiency map        (16 files)
├── contact/  bare f=0..0.03, cfg 1-3          (63 files)
│   ├── c1_f0p000/ ...
│   └── c3_f0p030/ ...
└── al/       Al grease vs air, cfg 1-3         (18 files)
    ├── c1_grease/ ...
    └── c3_air/ ...
```

The contact/Al case subdirectories keep the standard
`muon_out_c<C>_r<R>_<pos>.root` names, so `analyze_muon.C` works on each case
directory without modification.

Report v1 is frozen under `versions/v1/`; the v2 work has two data generations:

| Version | Data | Note |
|---|---|---|
| `v2.1` | `data_v2.1/{matrix,whole}` | previous set (pre WLS-orientation flip); `whole/` is incomplete |
| `v2.2` | `data_v2.2/{matrix,whole,contact,al}` | **current** set (script default) |

The tag is controlled by `VERSION` (default `v2.2`). `DATAROOT` defaults to the
directory you run the driver from (`$PWD`); set `DATAROOT=/bigdata/<user>/muon`
to write elsewhere.
`run_all.sh` / `run_fast.sh` export `VERSION` so all four arrays (and the
generated macros) agree. The report pipeline still defaults to
`slurm/data_v2.2/{matrix,whole}` (`macros/build_report_figures.sh`); the
contact/Al trees are additive and do not change the existing report run.

## Fast + priority-aware path (`run_fast.sh`) -- recommended

`run_all.sh` targets the default `cpu-genoa` queue. If that queue is congested
(SLURM reason `Priority`) use **`run_fast.sh`** instead -- it writes isolated
matrix/whole macro sets (`muon_fast/`, `muon_whole_fast/`) plus the shared
contact/Al sets (`muon_contact/`, `muon_al/`) and its own logs, and adds:

- **flux map OFF** for the matrix and the two characterization scans (measured
  3x faster: 0.23 vs 0.70 s/event). The efficiency comes from the `sipmHits`
  tree, not `h2_optflux`.
- **`--mem=4G`** (measured peak RSS ~0.4 GB). Short walltimes backfill much
  better than the old 2-day times.
- **routed to `low`** by default (`-p low -A low`): 145 idle nodes and a
  fair-share of 0.64 vs 0.005 on `default`.
- **contact and Al arrays** added to the same driver; their event counts and
  scan grids are independent knobs (see below). Defaults: 1000 events/position,
  f = 0.000..0.030 in 0.005 steps, configs 1-3, grease vs air.

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
# contact/Al events and threads are environment knobs (defaults 1000 / matrix threads):
CONTACT_EVENTS=2000 AL_EVENTS=2000 ./run_fast.sh
```

Routing is fully overridable, per array, via environment:

```bash
# whole-slab on the guaranteed default queue (no preemption), matrix on low:
WHOLE_PARTITION=cpu-genoa WHOLE_ACCOUNT=default WHOLE_TIME=02:00:00 ./run_fast.sh
# throttle all four arrays to 4 concurrent tasks (be kinder to fair-share):
ARRAY_LIMIT=4 ./run_fast.sh
# write to an explicit path (default is the current directory):
DATAROOT=/bigdata/<user>/muon ./run_fast.sh
# cap memory per task at 2 GB:
MATRIX_MEM=2G WHOLE_MEM=2G CONTACT_MEM=2G AL_MEM=2G ./run_fast.sh
# custom contact grid / configs:
CONTACT_FRACTIONS="0.000 0.010 0.020 0.030" CONTACT_CFGS="1 2 3" ./run_fast.sh
# Custom Al configs (include WLS config 4 if desired):
AL_CFGS="1 2 3 4" ./run_fast.sh
```

### Characterization scans (contact + Al backing)

Both scans use `slurm/muon_characterization_template.mac`, which adds the two
pre-init knobs after `muonScint/add`:

```
/dcs-monitor/det/muonScint/setBareContactFraction <f>   # 0..1, default 0
/dcs-monitor/det/muonScint/setAlBacking grease|air     # default grease
```

Contact fraction is an effective bare-plate/black-holder contact area fraction
(per-hit loss `R=1-f`, `T=0`), not a measured holder gap. It applies only when
the black holder exists (`kNoReflector` and not WLS), i.e. configs 1-3; config 4
is excluded. The Al backing changes the backing refractive index only
(`grease` n=1.46, `air` n=1.0003); the Al surface reflectance is unchanged.

Default grids (override via environment):

| Knob | Default | Meaning |
|---|---|---|
| `CONTACT_FRACTIONS` | `0.000 0.005 0.010 0.015 0.020 0.025 0.030` | f values |
| `CONTACT_CFGS` | `1 2 3` | contact scan configs |
| `CONTACT_EVENTS` | `1000` | events per beam position |
| `CONTACT_THREADS` | `MATRIX_THREADS` | Geant4 threads |
| `AL_BACKINGS` | `grease air` | backing cases |
| `AL_CFGS` | `1 2 3` | Al scan configs |
| `AL_EVENTS` | `1000` | events per beam position |
| `AL_THREADS` | `MATRIX_THREADS` | Geant4 threads |

Each generated macro runs centre/edge/corner back-to-back. The case
subdirectories are `contact/c<C>_f<value>` and `al/c<C>_<backing>`, e.g.
`contact/c1_f0p020/muon_out_c1_r0_centre.root`.

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
# contact/Al: 1000 ev/position by default,
# -> /bigdata/<user>/muon/data_v2.2/{matrix,whole,contact,al}/
```

`BIN` is the **only** thing you point at a specific build/binary version; it is
exported to all four job arrays. If unset, resolution is: pre-built HZDR binary
(if present) → `../build/G4DCSmonitor` next to `slurm/`.

Logs and generated macros always stay in `slurm/`, and the macros carry
**absolute** output paths, so the jobs write to `DATAROOT` regardless of the
working directory the array tasks start in.

### Knobs

| What | `run_fast.sh` | `run_all.sh` |
|---|---|---|
| matrix events/position | `$1` / `MATRIX_EVENTS` (200) | `$1` / `NEVENTS_MATRIX` (200) |
| whole-slab events | `$2` / `WHOLE_EVENTS` (10000) | `$2` / `NEVENTS_WHOLE` (10000) |
| contact events/position | `CONTACT_EVENTS` (1000) | `$4` / `NEVENTS_CONTACT` (1000) |
| Al events/position | `AL_EVENTS` (1000) | `$5` / `NEVENTS_AL` (1000) |
| threads == `--cpus-per-task` | `$3`/`$4` / `MATRIX_THREADS`/`WHOLE_THREADS` (32); `CONTACT_THREADS`/`AL_THREADS` default to `MATRIX_THREADS` | `$3` / `THREADS` (32); `CONTACT_THREADS`/`AL_THREADS` default to `THREADS` |
| memory per task (`--mem`) | `MATRIX_MEM`/`WHOLE_MEM`/**`CONTACT_MEM`/`AL_MEM`** (4G) | `MATRIX_MEM`/`WHOLE_MEM` (64G), `CONTACT_MEM`/`AL_MEM` (4G) |
| contact grid / configs | `CONTACT_FRACTIONS` / `CONTACT_CFGS` | same env |
| Al backings / configs | `AL_BACKINGS` / `AL_CFGS` | same env |
| output root | `DATAROOT` (default `$PWD`) | `DATAROOT` (default `$PWD`) |
| data-set version | `VERSION` (default `v2.2`) | `VERSION` (default `v2.2`) |
| binary | `BIN` | `BIN` |
| partition / account / time / array | `MATRIX_*` / `WHOLE_*` / `CONTACT_*` / `AL_*` env | edit `.sbatch` `#SBATCH` lines |
| throttle all arrays | `ARRAY_LIMIT` | `ARRAY_LIMIT` |

Both drivers pass `--cpus-per-task` **and** `--mem` to `sbatch`, overriding the
`#SBATCH` defaults, so threads stay in sync with the `/run/numberOfThreads`
baked into the macros. All four event counts are independent. The contact/Al
array bounds are auto-computed from the generated manifests unless
`CONTACT_ARRAY` / `AL_ARRAY` is set.

```bash
# 500 matrix / 20000 whole, 16 threads, 8 GB per task (positional form):
./run_fast.sh 500 20000 16 16        # ...with MATRIX_MEM=8G WHOLE_MEM=8G
# equivalent, all via the environment:
MATRIX_EVENTS=500 WHOLE_EVENTS=20000 MATRIX_THREADS=16 WHOLE_THREADS=16 \
  MATRIX_MEM=8G WHOLE_MEM=8G ./run_fast.sh
```

> `run_all.sh` uses the flux-map (heavier) matrix/whole templates and requests
> `64G` there by default; its contact/Al arrays still use the characterization
> template with `4G` and the flux map off. `run_fast.sh` turns the matrix flux
> map off and needs only `4G` (measured peak RSS ~0.4 GB). Lower `*_MEM` if the
> queue charges by memory.

Both drivers only **generate macros and submit**; they do not build or run the
analysis.

## What is here

| File | Purpose |
|---|---|
| `muon_scint_template.mac` | Matrix template (`@CONFIG@ @REFLECTOR@ @NEVENTS@ @THREADS@ @DATADIR@`) |
| `whole_slab_template.mac` | Whole-slab template (same tokens) |
| `muon_characterization_template.mac` | Contact/Al template adding `@BARECONTACT@` + `@ALBACKING@` |
| `generate_muon_matrix.sh` | Emits `muon/scan_c<C>_r<R>.mac` (16 macros) + `muon/manifest.txt` |
| `generate_whole_slab.sh` | Emits `muon_whole/whole_c<C>_r<R>.mac` (16 macros) + manifest |
| `generate_contact_scan.sh` | Emits `muon_contact/c<C>_f<val>.mac` (21 macros) + manifest |
| `generate_al_backing.sh` | Emits `muon_al/c<C>_<backing>.mac` (6 macros) + manifest |
| `run_all.sh` | Generates all four sets and submits all four arrays |
| `run_matrix.sbatch` | Job array `--array=0-15` for the matrix scan |
| `run_whole.sbatch` | Job array `--array=0-15` for the whole-slab scan |
| `run_contact.sbatch` | Job array for the contact scan (`--array=0-20` default) |
| `run_al.sbatch` | Job array for the Al-backing scan (`--array=0-5` default) |
| `build.sbatch` | One-off build of `../build/G4DCSmonitor` |
| `collect.sh` | Count / list / rsync-back helper (all four arrays) |
| `run_fast.sh` | **Fast/priority-aware driver** (flux off, 4G/1h, `low`) |
| `run_matrix_fast.sbatch` | Job array for the fast matrix scan |
| `run_whole_fast.sbatch` | Job array for the fast whole-slab scan |
| `muon_scint_template_fast.mac` | Matrix template with the flux map off |

The matrix/whole array index maps deterministically:

```
 0=c1_r0  1=c1_r1  2=c1_r2  3=c1_r3
 4=c2_r0  5=c2_r1  6=c2_r2  7=c2_r3
 8=c3_r0  9=c3_r1 10=c3_r2 11=c3_r3
12=c4_r0 13=c4_r1 14=c4_r2 15=c4_r3
```

Contact/Al indices come from their manifests: contact is
`cfg-major, fraction-minor` (default 3 x 7 = 21); Al is
`cfg-major, backing-minor` (default 3 x 2 = 6).

## Adapt to your cluster

Edit the **USER CONFIG** block at the top of `run_matrix.sbatch`,
`run_whole.sbatch`, `run_contact.sbatch`, `run_al.sbatch`, and `build.sbatch`:

- load your Geant4 (`ui_all vis_all`) + ROOT 6 (`RIO Net ROOTDataFrame Imt`)
  modules -- either `module load <your-single-modulefile>` or the explicit
  `module purge` + `module load gcc/... root/... cmake/... xerces/... qtbase/... geant4/...`
  list (currently set to the HZDR set: gcc/15.2.0, root/6.32.14, cmake/4.0.3,
  xerces/3.3.0, qtbase/6.10.1, geant4/11.4.0), and
- set `BIN` to the built executable. The default resolution order is:
  1. `BIN` if already exported,
  2. the pre-built HZDR binary `/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor` if present,
  3. `../build/G4DCSmonitor` next to `slurm/`.
  `run_all.sh` / `run_fast.sh` resolve `BIN` once and export it to all arrays.
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
- Contact: `${DATAROOT}/data_<VERSION>/contact/c<C>_f<val>/muon_out_c<C>_r0_<pos>.root` (63 files)
- Al backing: `${DATAROOT}/data_<VERSION>/al/c<C>_<backing>/muon_out_c<C>_r2_<pos>.root` (18 files)
- Logs (always in `slurm/`):
  - `logs/muonscan_<jobid>_<arrayid>.out|.err`
  - `logs/muonwhole_<jobid>_<arrayid>.out|.err`
  - `logs/muoncontact_<jobid>_<arrayid>.out|.err`
  - `logs/muonal_<jobid>_<arrayid>.out|.err`

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
root -l -b -q 'macros/analyze_muon.C+("data_v2.2/contact/c1_f0p020")'
root -l -b -q 'macros/analyze_muon.C+("data_v2.2/al/c1_air")'
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
  off (the tree already carries the product). The contact/Al macros also leave
  it off regardless of driver.
- `run_all.sh` and `run_fast.sh` both write `slurm/muon_contact/` and
  `slurm/muon_al/` (unlike the isolated `muon_fast`/`muon_whole_fast` sets), so
  use one driver at a time or regenerate deliberately. The `.sbatch` array
  bounds are taken from whichever manifest was generated last.
- `/run/numberOfThreads` is set by the generator; the matrix macro's three
  `/run/beamOn` blocks reuse one geometry build, so no per-position process is
  needed.
- Re-running an array task overwrites its own files; other tasks are
  independent.
