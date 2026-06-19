# Part 1: Simulate surface flux of cask from different sources

In a two-part workflow for simulating a detector response from CASTOR 440/84
dry storage casks, this simulation is part 1: generate the surface flux 
corresponding to different particle emission types produced in different fuel
assemblies stored in the cask. Currently, four sources are considered:

* Cs-137 (662 keV gamma)
* Co-60 (1173 and 1332 keV gammas)
* Eu-154 (full rdecay chain)
* a neutron Watt spectrum (approximating all neutron emitters) 

The scripts here can be modified and used to produce this data. The simulation
can be started via:

`G4DCSmonitor --biasing <gamma/neutron> run.mac`

The current setup uses importance biasing, for which the particle of choice
MUST be specified via the command line. Other settings, such as the number of
biasing shells and the number of primaries per assembly, are given in 
'globals.mac'. 

