# Handoff: v6 surface-calibration refactor

**Date:** 2026-10-18

**Repository:** `/home/turko46/geant4-sims/G4DCSmonitor`

**Branch:** `1-muon-trigger-detector`

**Last commit:** `b83b4e6` (on origin)

> **Note on data provenance:** the production data in `/home/turko46/bigdata/...` were produced on the HPC cluster; only the ROOT files are visible from this login node. HPC scheduler logs (`.out`/`.err`) are not stored here. The local `/tmp/opencode/contact_ext_logs/` logs are from the interrupted login-node extension run only.

## What we are doing

The simulation workflow is being refactored so that the two uncertain surface/interface parameters are treated as **calibration inputs**, frozen before the production matrix and whole-slab runs:

1. **Bare-plate / black-holder contact fraction `f`** (`/dcs-monitor/det/muonScint/setBareContactFraction`).
2. **Aluminium-foil backing index** (`/dcs-monitor/det/muonScint/setAlBacking air|grease`).

The current `data_v5` production used `f = 0` (clean air gap) and `grease` Al backing. These choices leave the bare plate as the second-brightest wrapper, contrary to the reference experiment (Rao *et al.*, J. Appl. Phys. 138, 114504). A new `data_v6` production will use calibrated nominal values.

## Decisions already taken

| Parameter | Decision |
|---|---|
| **Bare contact `f`** | Choose the smallest global `f` that makes the bare plate the weakest wrapper in **all** nine config/position cells. This requires extending the contact scan to `f = 0.050`. Preliminary data point to `f ≈ 0.037–0.040`. |
| **Al backing** | Use **air** (`n = 1.0003`) as the nominal backing. It reproduces the measured Al ≈ glossy relation (~92% of glossy yield). Grease remains the lower-bound systematic. |
| **Rerun scope** | Full `data_v6` matrix + whole-slab production via the SLURM driver scripts. |
| **"Sub radiation"** | Whole-slab irradiation only (not the surface-flux/CASTOR workflow). |

## Current state

### v5 data (complete, air gap / grease default)
- `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5`
  - `matrix/` — 48 ROOT files (4 configs × 4 reflectors × 3 positions)
  - `whole/` — 16 ROOT files
  - `contact/` — 63 ROOT files (configs 1–3 × 7 fractions × 3 positions)
  - `al/` — 18 ROOT files (configs 1–3 × 2 backings × 3 positions)

### v6 partial work
- `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v6/contact` created as a copy of `data_v5/contact`.
- Extension macros for `f = 0.035, 0.040, 0.045, 0.050` were generated in `slurm/muon_contact/`.
- A first local run of the extension was started but **aborted** because `slurm/muon_contact/` still contained stale macros from earlier defaults (`f = 0.000–0.030`) whose output paths point to `data_v2.2/contact`. Those stale macros segfault (exit 139) in `G4RootMainNtupleManager::CreateNtupleFromBooking` because the output directory does not exist.
- Only one new ROOT file was produced: `data_v6/contact/c1_f0p035/muon_out_c1_r0_centre.root`.
- Extension logs: `/tmp/opencode/contact_ext_logs/`

## Immediate next steps

1. **Clean up `slurm/muon_contact/`** — remove stale `f = 0.000–0.030` macros, or regenerate the full v6 contact scan manifest in a fresh directory.
2. **Rerun the contact extension** for `f = 0.035, 0.040, 0.045, 0.050` (configs 1–3, three positions, 1000 events) into `data_v6/contact`.
3. **Re-run `contact_scan.C`** on the completed `data_v6/contact` to confirm the crossing and lock `f` (expected `0.040`).
4. **Wire the nominal values into the production templates:**
   - Add `@BARECONTACT@` and `@ALBACKING@` placeholders to `slurm/muon_scint_template.mac` and `slurm/whole_slab_template.mac`.
   - Add a single-source env file (e.g. `slurm/nominal_surface.env`) with `NOMINAL_BARE_CONTACT` and `NOMINAL_AL_BACKING`.
   - Update `generate_muon_matrix.sh`, `generate_whole_slab.sh`, and `run_fast.sh` to read it.
5. **Pilot two cells** (e.g. config-1 bare at `f = 0.040` and config-1 Al air) to verify commands are applied and yields match the calibration scan.
6. **Submit full v6 matrix + whole-slab SLURM production.**
7. **Restructure the report:** move contact/Al material into a new “Surface calibration and nominal model” section *before* the matrix results, then regenerate all figures/tables on `data_v6`.

## Known issues / gotchas

- **Stale macros in `slurm/muon_contact/`** cause segfaults; do not run them again until cleaned up.
- **No `meta` TTree exists on this branch.** Provenance must be carried by macro headers, manifests, and the nominal env file, not ROOT metadata.
- **The TiO₂ corner/position reversal is independent of `f` and Al backing.** Even after calibration it will likely remain a disagreement with the paper; keep it as a caveat.
- **The contact model applies `f` uniformly to every bare face.** A more physical model would restrict contact to the bottom/support faces, but that is out of scope for this refactor.

## Useful paths and commands

```bash
# Extension generation (after cleanup):
CONTACT_FRACTIONS="0.035 0.040 0.045 0.050" \
  CONTACT_CFGS="1 2 3" \
  FLUXMAP=off \
  slurm/generate_contact_scan.sh 1000 8 \
  /home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v6/contact

# Re-analyse contact scan:
ROOT_INCLUDE_PATH=/home/turko46/root-work \
  /home/turko46/programs/root/build/bin/root -l -b -q \
  'macros/contact_scan.C+("/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v6/contact",\
  "/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v5/matrix",\
  "report/figures")'

# Full v6 production (planned):
VERSION=v6 \
  ./slurm/run_fast.sh 2500 25000 32 32
```

## Contact

This handoff was written before the nominal values were locked or any v6 production data were produced. The next person to pick this up should start with step 1 above.
