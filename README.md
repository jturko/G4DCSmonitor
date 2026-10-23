# G4DCSmonitor: MuonScint

Geant4 + ROOT 6 simulation of a plastic-scintillator muon trigger detector read
out by side-coupled silicon photomultipliers (SiPMs). The model reproduces the
optical measurements of Rao *et al.*, [J. Appl. Phys. **138**, 114504
(2025)](https://doi.org/10.1063/5.0288270), and extends them to alternative
read-outs, wrappers, wavelength-shifting (WLS) fibres and position
reconstruction.

> **Branch note.** This is the `1-muon-trigger-detector` branch. The `main`
> branch/worktree is the CASTOR 440/84 dry-cask + CLYC application. Both
> geometries are still compiled into this executable, but this README documents
> the MuonScint work developed here. For cask/CLYC workflows, use `main`.

**Status:** research code, no automated test suite. A change is validated by a
clean build plus a short macro run. Results are collected in
[`report/matrix_report.pdf`](report/matrix_report.pdf); the narrative status
update with the physics motivation and result tables is
[`workitem_muon_status.md`](workitem_muon_status.md).

## Gallery

Layout schematics, 3-D renderings of the VRML exports
([`vis_from_vrml/`](vis_from_vrml)) and the WLS event animation:

| Design | Renderings |
|---|---|
| Read-out layouts | [Rao layouts + config 2 mod](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/sipm_layouts.png) · [campaign sensor patterns](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/fig_design_layouts.png) |
| Config 2 mod (cut corners) | [overview](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config2mod_overview.png) · [chamfer close-up](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config2mod_corner-zoom.png) |
| Config 4 (single WLS bundle) | [overview](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config4_WLS-overview.png) · [fibres exposed](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config4_WLS-holder-removed.png) · [fibre-to-SiPM read-out](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config4_WLS-fibre-SiPM-coupling.png) |
| Config 11 (four-sided, dual-ended) | [corner view](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config11_WLS-dual-ended-zoom.png) · [holder removed](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config11_WLS-dual-ended-holder-removed.png) |
| 6 mm sensor design / 4-sided WLS | [C1, 6 mm](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/design_ogl_c1_6mm.png) · [four-sided WLS](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/design_ogl_wls10.png) |
| WLS event animation | [GIF](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/movie_wls/muon_movie_wls.gif) · [MP4](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/movie_wls/muon_movie_wls.mp4) |

![Config 4: four WLS fibres and one SiPM on the narrow face](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/vis_from_vrml/config4_WLS-overview.png)

## The detector model

The baseline detector is a polished **250 × 250 × 10 mm³ PVT** plate
(`G4_PLASTIC_SC_VINYLTOLUENE`, n = 1.58), viewed by **4 × 4 mm** SiPMs
(ASD-NUV4S-P-40) coupled to the narrow faces. The bare layout sits in a black
holder; the reflective layouts use a wrapper skin instead. The plate size is a
macro setting; the `GeometryMuonScint` default is 200 × 200 × 10 mm³. Production
macros set 250 × 250 × 10 mm³ explicitly.

The four Rao layouts and the main extensions are selected by a preset:

| Layout | Preset | Read-out |
|---|---:|---|
| Config 1 | `1` | 8 SiPMs, 2 per side, all four sides |
| Config 2 | `2` | 4 SiPMs, 1 per side |
| Config 2 mod | `12` | 4 SiPMs on 45° truncated corners |
| Config 3 | `3` | 2 SiPMs, both on the `-X` face |
| Config 4 | `7` | 4 WLS fibres → 1 SiPM on the `-X` face (production) |
| Config 4 variants | `5`, `6` | legacy tessellated round fibres (`5`) and square-section fibres (`6`); geometry/A-B and visualisation |
| 8-SiPM wide | `8` | 2 per side at u = ±0.75 |
| Four-sided WLS | `10` | one clamp per side, 4 SiPMs total |
| Four-sided WLS, dual-ended | `11` | 4 clamps read at both ends, 8 SiPMs |
| — | `9` | deferred, not implemented |

`preset 12` sets a 12 mm corner leg, leaving four 17.0 mm chamfer faces and one
4 × 4 mm SiPM per face. The cut removes 0.46 % of the plate area and 2.88 % of
the 50 × 50 mm corner beam patch, so count **distinct `evtNb`** values when
normalising corner runs, not `max(evtNb)+1`.

### Reflectors

`setReflector` selects the wrapper applied by the optical skin:

| Value | Model |
|---:|---|
| `0` | Bare polished plate in the black holder, real 1.2 mm air gap; optional contact-loss fraction |
| `1` | TiO₂ paint: Lambertian, R ≈ 0.955–0.983, no TIR |
| `2` | Aluminium foil: specular, R ≈ 0.904–0.925; backing is grease (n = 1.46) by default, or air |
| `3` | High-gloss paper: specular, R ≈ 0.900–0.955 over air |
| `4` | PTFE/Teflon: Lambertian, R ≈ 0.92–0.99; not part of the Rao study |

The bare model uses a per-hit contact fraction `f`
(`setBareContactFraction`, default `0`): `R = 1−f`, `T = 0` on every bare face,
so a fraction `f` of surface hits is absorbed by the black holder and the rest
follows the polished scintillator–air Fresnel/TIR path. The production value
used to recover the paper's `bare < TiO₂` ordering is `f = 0.045`; `f = 0` is the
clean-air reference. The Al backing is changed with
`setAlBacking grease|air`; it changes only the backing refractive index, not the
Al reflectance.

### Config 4 / WLS

Preset `7` is the production WLS geometry. Four Ø1.0 mm Kuraray Y-11 fibres run
along the 250 mm `-X` face in a 3-D-printed clamp, converge from a 2.5 mm pitch
to a 1.0 mm bundle, and are read by one 4 × 4 mm SiPM. The far ends have
aluminium mirror caps. A 0.10 mm grease film couples the clamp to the plate.
The clamp skin is white Lambertian by default (`setClampSurface white`), with
`black` and `mirror` as alternatives. Fibres are multi-clad by default
(`setFibreMultiClad true`), and the Y-11 absorption scale is set by
`setWLSAbsLength` (default 1.0 mm at 430 nm).

The analytic `G4CutTubs` fibre path is the production geometry. The tessellated
variant (`setFibreAnalytic false`, issued **after** `preset`) exists so VRML and
OpenGL can draw the fibres as solids; its optics are not validated for physics.
The WLS clamp cannot be combined with the corner cut: building the fibres forces
`fCornerCut = 0`.

### Geometry commands

All `/dcs-monitor/det/...` commands are **pre-init**: issue them before
`/run/initialize`. `setPosition` and `setRotation` apply to the next volume;
`muonScint/add` creates a new slab at that state. Setters act on the
most-recently-added slab.

| Command | Meaning |
|---|---|
| `/dcs-monitor/det/setPosition x y z [unit]` | Position for the next volume |
| `/dcs-monitor/det/setRotation x y z` | Rotation in degrees |
| `/dcs-monitor/det/muonScint/add` | Add a slab |
| `/dcs-monitor/det/muonScint/setSize x y z [unit]` | Full slab lengths; default unit mm |
| `/dcs-monitor/det/muonScint/preset N` | Apply SiPM/WLS layout `N` |
| `/dcs-monitor/det/muonScint/setReflector N` | Wrapper `0`–`4` |
| `/dcs-monitor/det/muonScint/setWrap true\|false` | Enable/disable the wrapper skin |
| `/dcs-monitor/det/muonScint/setBareContactFraction f` | Bare-holder contact fraction, `0`–`1` |
| `/dcs-monitor/det/muonScint/setAlBacking air\|grease` | Al-foil backing index |
| `/dcs-monitor/det/muonScint/setCouplingIndex n` | Plate–SiPM grease index, default 1.46 |
| `/dcs-monitor/det/muonScint/setSiPMSize sx sy [ignored]` | Active area in mm; thickness is fixed |
| `/dcs-monitor/det/muonScint/addSiPM edge u v` | Add one SiPM: edge `0=+X, 1=−X, 2=+Y, 3=−Y`; `u,v ∈ [−1,1]` |
| `/dcs-monitor/det/muonScint/clearSiPMs` | Remove all SiPMs from the current slab |
| `/dcs-monitor/det/muonScint/setCornerCut length` | 45° corner truncation; issue after `preset` to override |
| `/dcs-monitor/det/muonScint/setEdgeReflector same\|black\|diffuse\|mirror` | Finish of uninstrumented narrow edges, presets 1–8 |
| `/dcs-monitor/det/muonScint/setClampSurface white\|black\|mirror` | WLS clamp skin |
| `/dcs-monitor/det/muonScint/setFibreAnalytic true\|false` | Analytic production fibres (`true`) or tessellated renderable fibres (`false`) |
| `/dcs-monitor/det/muonScint/setFibreMultiClad true\|false` | Add the outer n = 1.42 fluoropolymer sheath |
| `/dcs-monitor/det/muonScint/setWLSAbsLength length [unit]` | Y-11 absorption scale at 430 nm |
| `/dcs-monitor/det/muonScint/setWLSPadSlabContact true\|false` | Direct slab-to-read-out-pad coupling |
| `/dcs-monitor/det/muonScint/setLightYieldScale s` | **Visualisation only**; do not use in physics runs |

### Optical model

| Item | Model |
|---|---|
| Plate | PVT, polished, n = 1.58; production 250 × 250 × 10 mm³ |
| Light yield | 1.0 × 10⁴ ph/MeV, Birks 0.126 mm/MeV |
| Emission | 425 nm peak, essentially 380–550 nm |
| Timing | rise 0.9 ns, decay 2.1 ns |
| Bulk attenuation | 3.8 m at the peak, falling in the UV |
| Transport | scintillation + Čerenkov over 310–700 nm; absorption and Rayleigh on, Mie off |
| Coupling | 0.1 mm grease n = 1.46 → SiPM epoxy n = 1.55 |
| SiPM | ASD-NUV4S-P-40, PDE peak 0.431 at 410 nm, ≈ 0.30 at 490 nm |
| WLS fibre | Y-11: core 1.59, clad 1.49, outer sheath 1.42; absorbs near 430 nm, emits 476 nm, τ = 7 ns |

The SiPM PDE is **not** applied in the analysis. It is the `EFFICIENCY(λ)` table
of the grease/SiPM `dielectric_metal` boundary with `REFLECTIVITY = 0`.
`G4OpBoundaryProcess` performs the PDE Bernoulli draw and invokes the sensitive
detector only on a detected photon, so one hit is one photoelectron and
`nDetected` already includes the PDE. There is no gain, dark count, crosstalk,
saturation or timing jitter in the model.

## Physics list

`PhysicsList` registers the standard EM and hadronic stack
(`G4EmStandardPhysics_option4`, HP elastic/inelastic, thermal neutrons, ions,
gamma-nuclear and radioactive decay), plus `G4OpticalPhysics` with
scintillation, Čerenkov, absorption, Rayleigh and boundary processes. Mie
scattering is off. `OpWLS` is enabled automatically when a WLS geometry is
built. `SetBoundaryInvokeSD(true)` is what makes the SiPM boundary the detector.
Importance biasing is currently commented out, so `/dcs-monitor/det/useBiasing`
has no effect.

## Primary generation

`/dcs-monitor/gun/sourceMode` selects the generator:

| Mode | Use |
|---|---|
| `GPS` | Standard `/gps/` commands; the MuonScint default |
| `CASTOR440_surface` | Legacy cask surface flux |
| `CASTOR440_fuel` | Legacy fuel-assembly volume source |
| `CASTOR440_fuel_biased` | Legacy directional bias toward CLYC |

The Rao reproduction uses a vertical 4 GeV `mu-` beam in a 50 × 50 mm² patch:

```macro
/dcs-monitor/gun/sourceMode GPS
/gps/particle mu-
/gps/ene/type Mono
/gps/ene/mono 4 GeV
/gps/pos/type Plane
/gps/pos/shape Square
/gps/pos/halfx 25 mm
/gps/pos/halfy 25 mm
/gps/pos/centre 0 0 100 mm
/gps/ang/type planar
```

`/gps/ang/type planar` fires the default momentum direction `(0,0,−1)`, giving a
vertical downward beam. The three equal-area patches are centred at `(0,0)`,
`(100,0)` mm (the `+X` edge) and `(100,100)` mm (the `+X/+Y` corner), so each
50 × 50 mm² patch lies fully on the plate. Configs 3 and 4 are deliberately on
`-X`, opposite the edge/corner beams. `muonEntryX/Y` are slab-local with the slab
rotation undone; a global `/dcs-monitor/det/setRotation` is therefore a no-op for
all output coordinates.

## Building and running

Requirements: Geant4 11.x with UI/Vis, ROOT 6 (with `RIO` and `Net`), CMake, and
a C++17 compiler. The login-node build is known to work with Geant4 11.3.2.

```bash
cmake -S . -B build \
  -DGeant4_DIR=$HOME/programs/geant4/install/lib/cmake/Geant4 \
  -DROOT_DIR=$HOME/programs/root/build \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
```

Run a macro in batch mode; with no arguments the program opens the interactive
Geant4 UI:

```bash
./build/G4DCSmonitor my_run.mac
./build/G4DCSmonitor my_run.mac 7    # optional fileNum, default output suffix _000007
```

A minimal physics run:

```macro
/run/numberOfThreads 8

/dcs-monitor/det/setPosition 0 0 0 mm
/dcs-monitor/det/setRotation 0 0 0
/dcs-monitor/det/muonScint/add
/dcs-monitor/det/muonScint/setSize 250 250 10 mm
/dcs-monitor/det/muonScint/preset 1
/dcs-monitor/det/muonScint/setReflector 3
/dcs-monitor/det/muonScint/setWrap true

/run/initialize

/dcs-monitor/run/writePrimary false
/dcs-monitor/run/writeSurfaceFlux false
/dcs-monitor/run/writeOpticalFluxMap false

/dcs-monitor/gun/sourceMode GPS
/gps/particle mu-
/gps/ene/type Mono
/gps/ene/mono 4 GeV
/gps/pos/type Plane
/gps/pos/shape Square
/gps/pos/halfx 25 mm
/gps/pos/halfy 25 mm
/gps/pos/centre 0 0 100 mm
/gps/ang/type planar

/analysis/setFileName muon_test.root
/run/beamOn 1000
```

`macros/muon_scint_template.mac` is a **template** with `@...@` placeholders;
`macros/generate_muon_matrix.sh` fills it. One generated macro runs centre, edge
and corner back-to-back after a single `/run/initialize`, which avoids rebuilding
the geometry per position. `macros/muon_vis.mac` is a single-event, head-less
event display (it lowers the light yield for legibility; never use that setting
for physics). `macros/run.mac` is the CASTOR/CLYC demo inherited from `main`; do
not use it for a MuonScint run.

## Output

The default file is `G4DCSmonitor.root` in the working directory; the optional
second command-line argument appends `_000007`-style suffixes. A macro-level
`/analysis/setFileName` overrides the name and takes precedence.

| Object | Contents |
|---|---|
| `primary` | Optional primary-particle ntuple; off by default |
| `hits` | General sensitive-detector hits; MuonScint response is in `sipmHits` |
| `surfaceFlux` | Optional CASTOR 440 surface flux; off by default |
| `sipmHits` | **Main MuonScint output**: one row per `(event, SiPM)` |
| `opticalStats` | Per-event optical-photon accounting |
| `h2_optflux_xy_slab<i>` | Optional per-slab optical-photon track-length map, 1 mm cells |

`sipmHits` columns: `evtNb`, `det`, `sipm`, `nDetected`, `tFirst`,
`meanWavelength_nm`, `rmsWavelength_nm`, `weight`, `muonX`, `muonY`,
`nProduced`, `tDetected`, `wlDetected_nm`, `creatorDetected`. Zero-detection
SiPMs are written too, so the row count is `nSiPM × nEvents` for events that
enter the slab. `creatorDetected` is `1 = OpWLS`, `2 = Scintillation`,
`3 = Čerenkov`, `0 = other`.

`opticalStats` columns: `evtNb`, `nScint`, `nCerenkov`, `nKilled`, `nDetected`,
`nEscaped`, `nAlive`.

To compute the mean photoelectron yield, sum `nDetected` over all SiPM rows of an
event and divide by the number of events (**distinct `evtNb`**, not the number of
rows). This matters for the corner-cut geometry, where a muon can miss the
plate entirely and produce no row.

Toggles:

| Command | Default | Effect |
|---|---|---|
| `/dcs-monitor/run/writePrimary` | off | Fill `primary` |
| `/dcs-monitor/run/writeSurfaceFlux` | off | Fill `surfaceFlux` |
| `/dcs-monitor/run/writeOpticalFluxMap` | off | Book/fill per-slab `h2_optflux_xy_slab<i>` |
| `/dcs-monitor/run/showDetectedPhotonsOnly` | off | Visualisation: keep detected photons and ancestors |

## Analysis and campaigns

| Tool | Purpose |
|---|---|
| `macros/plot_muon.C` | Matrix yield, spectra, wavelength and position trends |
| `macros/analyze_muon.C` | Per-directory yield/statistics summary |
| `macros/plot_collection_efficiency.C` | Whole-slab efficiency maps from `sipmHits` |
| `macros/position_reconstruction.C+` | Symmetry-augmented k-NN position reconstruction |
| `macros/opt_*.C` | Design-campaign figures (yield, maps, DE, waveforms, recon) |
| `macros/build_report_figures.sh` | Regenerate the report figures and PDF |
| `slurm/run_fast.sh` | Recommended HPC driver (matrix, whole-slab, contact, Al scans) |
| `slurm/run_local.sh` | Local no-SLURM runner for matrix/whole-slab |

Typical local analysis:

```bash
root -l -b -q 'macros/plot_muon.C+("data_v2.2/matrix")'
root -l -b -q 'macros/analyze_muon.C+("data_v2.2/matrix")'
root -l -b -q 'macros/plot_collection_efficiency.C+("data_v2.2/whole")'
root -l -b -q 'macros/position_reconstruction.C+("data_v2.2/whole")'
```

Production runs on the HPC use the native data root
`/bigdata/DCS-Monitor-II/turko46/MuonScinti` and an absolute `BIN` path; the
login-node `~/bigdata/...` mirror is for inspection only. There is no SLURM on
the login node. See [`slurm/README.md`](slurm/README.md) and
[`docs/campaigns.md`](docs/campaigns.md) for the current driver, versioning and
array layout.

## Results at a glance

The report and status update contain the full tables. The headline results are:

- Rao configs 1–4 plus the cut-corner config 2 mod, four wrappers and three beam
  positions are reproduced; the wrapper ordering is
  `bare < TiO₂ < Al ≈ glossy`.
- The detection threshold is reproduced for configs 1–3; config 4's single
  WLS channel is too dim to open the same threshold window.
- Whole-plate position reconstruction reaches σx ≈ **11.9 mm** for config 1 with
  TiO₂, improving to ≈ **11.5 mm** with 6 mm sensors and diffuse edge faces.
- Four-sided WLS read-out trades light and speed for position information: the
  diffuse-clamp design reaches σx ≈ 13.9 mm at ≈ 17 % of the direct-coupled
  yield; the mirror-clamp design destroys the position information
  (σx ≈ 55–61 mm).

Result figures: [photoelectron spectra](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/fig_pe_grid.png),
[position resolution](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/fig_opt_recon.png),
[optical spectra](https://codebase.helmholtz.cloud/fwdf/measurementscience/projects/myon/g4dcsmonitor/-/raw/1-muon-trigger-detector/report/figures/optical_spectra.png).

## Known limitations

- The chain stops at the optical limit: no SiPM gain, dark count, crosstalk,
  saturation, electronics noise or pulse shaping.
- The bare contact fraction is an effective area fraction, not a measured holder
  gap. The `f = 0.045` production value is calibrated to recover the observed
  wrapper ordering.
- Configs 3 and 4 are both placed on the `-X` face; the paper places them on
  adjacent faces. This is a deliberate simplification.
- The TiO₂ position trend is corner-high in the four-sided layouts, opposite to
  the paper's corner-lowest rule. The report documents this as the main genuine
  disagreement with the reference measurement.
- The tessellated WLS fibre path is for rendering and A/B geometry checks, not
  for production physics.
- The MuonScint model is a plate; there is no tray, module or full trigger
  system geometry.

## Repository map

| Path | Contents |
|---|---|
| `src/GeometryMuonScint.cc`, `include/GeometryMuonScint.hh` | Active MuonScint detector, optical surfaces, presets, WLS |
| `src/DetectorConstruction.cc`, `src/DetectorMessenger.cc` | Geometry hub and all `/dcs-monitor/det/...` commands |
| `src/PhysicsList.cc` | Modular physics list and optical-process configuration |
| `src/MuonScintSiPMSD.cc` | SiPM sensitive detector and `sipmHits` filling |
| `src/HistoManager.cc`, `src/RunAction.cc` | Ntuple/histogram booking and output |
| `macros/` | Run templates, analysis macros, report and movie scripts |
| `slurm/` | HPC drivers and generated campaign macros |
| `docs/` | Detailed reference notes (optics, WLS, outputs, campaigns, report) |
| `report/` | LaTeX report, figures and movies |
| `vis_from_vrml/` | Rendered views of the VRML exports |

## Further documentation

| Topic | File |
|---|---|
| Optical model, reflectors, beam regions, presets | [`docs/muon-optics.md`](docs/muon-optics.md) |
| Config 4 / WLS fibres, clamps, Kuraray tables | [`docs/config4-wls.md`](docs/config4-wls.md) |
| Reading `sipmHits`, yield/PE, diagnostics, event display | [`docs/output-and-diagnostics.md`](docs/output-and-diagnostics.md) |
| Analysis macros, plots, movies, position reconstruction | [`docs/analysis-macros.md`](docs/analysis-macros.md) |
| SLURM campaigns, data trees, versioning | [`docs/campaigns.md`](docs/campaigns.md) |
| Report structure and regeneration | [`docs/report.md`](docs/report.md) |
| Build/worktree/environment notes | [`docs/environment.md`](docs/environment.md) |
| Geant4 optical-boundary semantics | [`docs/geant4-reference.md`](docs/geant4-reference.md) |

## Reference

G. Rao, A. Bieberle, U. Hampel, and M. Wagner, "Design studies on large muon
trigger detectors using silicon photomultiplier devices," J. Appl. Phys.
**138**, 114504 (2025). [doi:10.1063/5.0288270](https://doi.org/10.1063/5.0288270)
