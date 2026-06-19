# Part 2: Simulate detector response from cask surface flux

In a two-part workflow for simulating a detector response from CASTOR 440/84 dry storage
casks, this is **part 2**: replay the surface flux generated in part 1 (source mode
`CASTOR440_surface_from_TTree`), transport it out through the cask and into a CLYC (or
plastic-scintillator) detector, and record the detector response. The same four sources
from part 1 are used:

* Cs-137 (662 keV gamma)
* Co-60 (1173 and 1332 keV gammas)
* Eu-154 (full decay chain)
* a neutron Watt spectrum (approximating all neutron emitters)

## C6 symmetry reuse
Exploiting the C6 symmetry of the basket, the 84 fuel slots are covered by only 14
simulated base assemblies, each rotated by a multiple of 60° about z (via
`/dcs-monitor/surf/sourceRotZ`). A single part-1 ROOT file is loaded **once** by the
`SurfaceFluxSampler` and reused for all 6 hexant images (and all 4 casks in the array).
The orbit map (`_orbit_map.sh`) handles the base↔global bookkeeping.

## Generating the macros
```bash
./generate_macros.sh [output_dir]   # default: macros/auto/
```
This writes 70 run macros (14 base assemblies × 5 sources). Each macro loads its
surface-flux file once and internally produces the detector response for every hexant
(4 casks × 6 hexants = 24 runs per macro). The source file paths, measurement time
(`MEAS_TIME`), per-source decay rates, primary counts, and detector geometry (including
the borated-PE collimator and optional shadow configurations) are set near the top of
`generate_macros.sh` — **edit these before generating the macros.**

> The generated macros use the current CLYC command names (`setPbColMaterial`,
> `setPEColMaterial`, `setPEPlugMaterial`, etc.), consistent with `DetectorMessenger.cc`.

## Running
```bash
G4DCSmonitor run_baseFuelBB_<source>.mac     # a single macro (no --biasing needed:
                                             # replay mode uses analog tracking)
```
Two driver scripts are provided (both must live next to the generated macros):
* `./start_one_base_fuel.sh <base_fuel> <macro_dir>` — runs the 5 source macros for one
  base assembly (each = all 6 hexant outputs).
* `./start_all.sh <macro_dir>` — runs all 14 base assemblies (i.e. the full cask).

## Post-processing
ROOT macros in `analysis/`:
* `make_assembly_heatmaps.C`    — build per-assembly hex-bin (x, y) heatmaps from the
  `hits` tree, distinguishing missing files from genuine zeros.
* `make_assembly_spectra.C`     — build/overlay per-assembly n/γ energy spectra.
* `plot_nominal_heatmap.C`      — plot the nominal CLYC heatmaps across the 4 casks.
* `plot_plastic_heatmap_diff.C` — plot the plastic (with − without shadow bar)
  difference, using a diverging palette.

These macros depend on helper headers (`geometry_constraints.h`, `style.h`) that define
the basket constants and styling; ensure they are present on the ROOT include path.

