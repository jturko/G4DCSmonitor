# Part 2: Simulate detector response from cask surface flux

In a two-part workflow for simulating a detector response from CASTOR 440/84
dry storage casks, this simulation is part 2: replay the surface flux generated
in part 1, transport it out through the cask and into a CLYC (or plastic-
scintillator) detector, and record the detector response. The same four sources
from part 1 are considered:

* Cs-137 (662 keV gamma)
* Co-60 (1173 and 1332 keV gammas)
* Eu-154 (full decay chain)
* a neutron Watt spectrum (approximating all neutron emitters)

Exploiting the C6 symmetry of the basket, the 84 fuel slots are covered by only
14 simulated base assemblies, each rotated by a multiple of 60 deg about z. A
single part-1 ROOT file is therefore loaded once and reused for all 6 hexant
images (and all 4 casks in the array). The orbit map (`_orbit_map.sh`) handles
the base<->global bookkeeping.

The macros are produced via:

`./generate_macros.sh [output_dir]`

which writes 70 run macros (14 base assemblies x 5 sources) to `output_dir`
(default `macros/auto/`). Each macro loads its surface-flux file once and
internally produces the detector response for every hexant.

The simulations can then be started via:

`G4DCSmonitor run_baseFuelBB_<source>.mac`

Two driver scripts are provided (both must live next to the generated macros):

* `./start_one_base_fuel.sh <base_fuel> <macro_dir>` -- runs the 5 source
  macros for a single base assembly (each = all 6 hexant outputs).
* `./start_all.sh <macro_dir>` -- runs all 14 base assemblies (i.e. the full
  cask).

Post-processing ROOT macros live in `analysis/`:

* `make_assembly_heatmaps.C`     -- build per-assembly (z, theta) heatmaps
* `make_assembly_spectra.C`      -- build per-assembly n/g energy spectra
* `plot_nominal_heatmap.C`       -- plot the nominal CLYC heatmap
* `plot_plastic_heatmap_diff.C`  -- plot the plastic (open - shadow) difference

Note: the source file paths, measurement time, decay rates, and detector
geometry (e.g. the optional tungsten shadow plug) are set near the top of
`generate_macros.sh` and should be edited there before generating the macros.
