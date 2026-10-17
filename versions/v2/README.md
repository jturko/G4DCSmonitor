# Report v2 — frozen workflow bundle

This directory preserves the **workflow and source** that produced the *second*
version of the light-collection report (`matrix_report.pdf`, 37 pages): the four
SiPM layouts (configs 1–4, including the Config-4 WLS fibre clamp), the four
reflector treatments, and the three 50 × 50 mm² beam positions (centre / edge /
corner), for the MuonScint optical model.

Unlike v1, **v2 is fully reproducible** — every figure and every table number can
be regenerated from two data sets on the `bigdata` volume.

## Data provenance (read this first)

The report was **not** built from the repo-local `slurm/data_v2.2`. That set has
the broken bare-config optics (κ₂D(bare) ≈ 9.5, the old "bare ≪ TiO₂" ordering);
it is the *v2.2* pre-fix data. The report used two other collections, both under
`$DCS_DATA_ROOT` (default `${HOME}/bigdata/DCS-Monitor-II/turko46/MuonScinti`):

| report content | data set |
|---|---|
| every figure/table **except** the two arrival-time figures | `data_v2.3/{matrix,whole}` |
| `fig_persistence_grid`, `fig_wavelength_time_grid` | `data_v3/whole` |

**Why the split:** the `sipmHits` tree of `data_v3` adds the `tDetected` /
`wlDetected_nm` variable-length vector branches (per-photoelectron arrival time
and wavelength) needed by `wavelength_time_grid.C` and `persistence_map_grid.C`.
`data_v2.2` and `data_v2.3` predate those branches, so the timing figures cannot
be regenerated from `data_v2.3`.

Verification (regenerated outputs are **byte-identical** to the archived ones):

| archived output | macro | data set |
|---|---|---|
| `position_efficiency.csv` | `matrix_positions.C` | `data_v2.3/matrix` |
| `whole_uniformity.csv` | `whole_slab_uniformity.C` | `data_v2.3/whole` |
| `fig_{yield,detected,wavelength,wavelength_cfg}.png` | `report_figures.C` | `data_v2.3/matrix` |
| `fig_pe_grid.png` | `pe_spectra_grid.C` | `data_v2.3/matrix` |
| `fig_{eff_bars,sipm_sharing,pos_refl,pos_layouts}.png` | `matrix_positions.C` | `data_v2.3/matrix` |
| `fig_whole_{contrast,uniformity_map_cfg1..4,polar}.png` | `whole_slab_uniformity.C` | `data_v2.3/whole` |
| `efficiency_grid_cfg{1..4}_all.{pdf,png}` | `plot_collection_efficiency.C+` | `data_v2.3/matrix/plots/` |
| `whole_slab_cfg{1..4}.{pdf,png}` | `plot_whole_slab.C+` | `data_v2.3/whole/plots/` |
| `fig_persistence_grid.png`, `fig_wavelength_time_grid.png` | `persistence_map_grid.C`, `wavelength_time_grid.C` | `data_v3/whole` |

### Data layout
- `data_v2.3/matrix/muon_out_c<C>_r<R>_{centre,edge,corner}.root` — 4 configs ×
  4 reflectors × 3 positions, **1000 events each**.
- `data_v2.3/whole/muon_whole_out_c<C>_r<R>.root` — 4 × 4, **25000 events each**.
- `data_v3/{matrix,whole}` — same structure, plus the `tDetected` / `wlDetected_nm`
  branches (the physics is unchanged; only the timing vectors were added).

## Contents

```
versions/v2/
├── README.md                        this file
├── matrix_report.tex                the v2 report source (37 pp)
├── matrix_report.pdf                the compiled v2 report
├── driver/
│   └── build_report_figures.sh      regenerates every figure + the PDF
├── macros/
│   ├── common/style.h               ROOT style header (all plot macros need it)
│   ├── sim/                         simulation driver (slurm/ copy)
│   │   ├── muon_scint_template{,_fast}.mac   matrix template
│   │   ├── whole_slab_template.mac           whole-slab template
│   │   ├── generate_muon_matrix.sh / generate_whole_slab.sh
│   │   ├── run_local.sh / run_all.sh / run_*.sh / *.sbatch
│   │   └── SLURM_README.md
│   ├── plot/                        ROOT analysis / figure macros
│   │   ├── sipm_layouts.C, irradiation_sites.C, optical_spectra.C
│   │   ├── report_figures.C, pe_spectra_grid.C
│   │   ├── matrix_positions.C, whole_slab_uniformity.C
│   │   ├── plot_collection_efficiency.C, plot_whole_slab.C
│   │   └── persistence_map_grid.C, wavelength_time_grid.C
│   └── vis/                         event-display macros
│       ├── muon_vis.mac, muon_vis_hits_wls.mac
│       └── muon_vis_wls*.mac
└── figures/                         the 27 report figures (as generated),
                                     plus unreferenced extras (see below)
```

The report references 27 figures: `sipm_layouts`, `irradiation_sites`,
`optical_spectra`, `fig_{yield,detected,wavelength,wavelength_cfg,eff_bars,
sipm_sharing,pos_refl,pos_layouts,pe_grid,whole_contrast,persistence_grid,
wavelength_time_grid}`, `fig_whole_uniformity_map_cfg{1..4}`, `whole_slab_cfg{1..4}`,
and the static PNGs `muon_event_display.png`, `muonScint_3Dview_dimensions.png`,
`muonScint_WLS_crossSection.png`, `muonScint_WLS_holderWithFibres_top.png`.
`figures/` additionally keeps the unreferenced extras as generated
(`fig_radius`, `fig_uniformity`, `fig_whole_polar`, `fig_whole_radial`,
`whole_uniformity` maps, `efficiency_grid_cfg*_all`, `light_eff_*`, `wls_*`).

## How to reproduce

```bash
# 1. build (repo root)
cmake --build build -j

# 2. figures + PDF (defaults point at the bigdata data sets)
versions/v2/driver/build_report_figures.sh
#   or with explicit dirs / a different data root:
#   DCS_DATA_ROOT=/path/to/MuonScinti versions/v2/driver/build_report_figures.sh \
#       <MATRIX_DIR> <WHOLE_DIR> <TIMING_DIR>
```

The driver regenerates every figure into `figures/` and runs `pdflatex` twice to
rebuild `matrix_report.pdf`. It needs `ROOT` and `pdflatex` on `PATH`.

## Notes / caveats

- **The driver never modifies the data.** `plot_collection_efficiency.C` and
  `plot_whole_slab.C` write their output into `<dir>/plots/`; the driver instead
  runs them against a temporary staging directory of symlinks (`mktemp -d`) and
  copies the results out, so the production `bigdata` directories are left intact.
- **One macro drifted.** `plot_collection_efficiency.C` was last changed by commit
  `74e7e6c` (2026-10-15 15:35), *after* the archived `efficiency_grid_cfg*_all`
  figures were generated (10:41). Regenerating those four now yields slightly
  different images. They are **not referenced by the report**; every referenced
  figure regenerates byte-identically (PNG) from the data sets above. The
  `figures/` copies here are the ones the published report used.
- `versions/v2/matrix_report.tex` and `versions/v2/figures/` are byte-identical to
  `report/matrix_report.tex` / `report/figures/` at the time of archiving (the
  compiled `matrix_report.pdf` is the same 37 pages, differing from
  `report/matrix_report.pdf` only by the embedded PDF timestamp/ID).
- The report text contains Config-4 numbers/WLS descriptions and a stall caveat
  that later physics work (see `HANDOFF_muon_scint_optical.md`) superseded; this
  bundle freezes the report **as published**, not the corrected physics.
- `fig_persistence_grid.pdf` / `fig_wavelength_time_grid.pdf` differ from a fresh
  regeneration only by the PDF embedded timestamp; the `.png` versions are
  byte-identical.
