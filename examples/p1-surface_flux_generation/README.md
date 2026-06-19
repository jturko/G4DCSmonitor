# Part 1: Simulate surface flux of cask from different sources

In a two-part workflow for simulating a detector response from CASTOR 440/84 dry
storage casks, this is **part 1**: generate the surface flux corresponding to different
particle-emission types produced in different fuel assemblies stored in the cask. The
output is the `surfaceFlux` ROOT tree (in cask-local coordinates), which part 2 replays.

Four sources are considered:
* Cs-137 (662 keV gamma)
* Co-60 (1173 and 1332 keV gammas)
* Eu-154 (full radioactive-decay chain)
* a neutron Watt spectrum (approximating all neutron emitters)

## Running
The simulation uses importance biasing, so the biased particle **must** be specified on
the command line *and* enabled in the macro:

```bash
G4DCSmonitor --biasing gamma   run.mac     # for the gamma sources
G4DCSmonitor --biasing neutron run.mac     # for the Watt-spectrum source
```

## Macro structure
* `run.mac`        — entry point: executes `globals.mac`, places the cask, initializes,
  then hands off to `0_particle.mac`.
* `globals.mac`    — global settings: verbosity, `/run/numberOfThreads 128`, biasing
  shells (`nShell = 22`), output toggles (`writeSurfaceFlux true`, `writePrimary false`),
  and `nPrimary = 1e9`.
* `0_particle.mac` — selects the source mode (`CASTOR440_fuel`), particle/energy (or
  radioactive ion, or Watt spectrum), and loops over fuel assemblies. By default it uses
  `/control/foreach` over the 14 symmetry-unique assemblies
  (`0 1 2 3 5 6 7 8 13 14 15 22 23 32`).
* `1_fuel_scan.mac` — sets `caskNum`/`fuelNum` and calls `2_beamOn.mac`.
* `2_beamOn.mac`   — sets the output file name and runs `/run/beamOn {nPrimary}`.

To switch sources, edit the particle/energy block in `0_particle.mac` (the gamma,
radioactive-ion `ion`, and neutron Watt-spectrum stanzas are provided, with the unused
ones commented out). For the Watt source, set `/gun/particle neutron` together with
`/dcs-monitor/gun/useWattSpectrum true` and the `wattA`/`wattB` parameters.

Output files follow the naming pattern
`surface_{partTag}_fuel{fuelTag}_nShells{nShell}_nEvts{nPrimary}.root`, which part 2
expects.

