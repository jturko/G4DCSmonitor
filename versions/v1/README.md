# Report v1 — frozen workflow bundle

This directory preserves the **workflow** that produced the *first* version of
the light-collection report (`matrix_report.pdf`, v1): the three SiPM layouts
(8 / 4 / 2 sensors) with the **original** placements, the four reflector
treatments, and the three beam positions. It is kept so that v1 can be
regenerated/understood independently of the ongoing v2 work in `report/`.

**The v1 data itself is not included** (it was overwritten in place during the
v2 re-runs). The finished v1 report is archived separately by the author.

## ⚠️ v1 is NOT reproducible from the current working tree

The working tree has moved on to v2. The v2 changes that make the *current* code
unable to reproduce v1 are:

- **Config 2 SiPM placement changed.** v1: one SiPM *centred* on each of the four
  side faces (`AddSiPM({e,0,0})`). v2: one per side at the Config-1 offsets
  (`u = ±0.5`), C4-symmetric.
- **Config 3 SiPM placement changed.** v1: both SiPMs on the **+X** face. v2:
  both on the **−X** face.
- **Config 4 (WLS fibre clamp) added** in v2 (preset 5); it does not exist in v1.
- **Beam-site history.** The `HEAD` template places the edge/corner patches at
  `(125,0)` / `(125,125)` (patches half off the plate). The v1 **report** used the
  corrected, fully-on-plate sites `(100,0)` / `(100,100)`. The v1-generated macros
  bundled here (`macros/sim/scan_c*_r*.mac`, `whole_c*_r*.mac`) already use the
  corrected `(100,0)`/`(100,100)` sites and Config-1-era presets.

To reproduce v1 exactly you must check out the pre-change source:

```
git checkout c97b424     # working-tree HEAD; v1-era geometry
cmake --build build -j
```

But note that commit `c97b424`'s *template* still has the old `(125,0)/(125,125)`
beam sites — use the bundled generated macros in `macros/sim/` (or edit the sites)
to match the v1 report. The Python/ROOT analysis and geometry sources at that
commit are the ground truth for v1 behaviour.

## Contents

```
versions/v1/
├── README.md                         this file
├── macros/
│   ├── sim/                          simulation driver (v1)
│   │   ├── muon_scint_template.mac   matrix template (git c97b424)
│   │   ├── generate_muon_matrix.sh   generates the per-(cfg,refl) macros
│   │   ├── run_muon_matrix.sh        runs them locally
│   │   ├── scan_c1..c3_r0..r3.mac    the ACTUAL generated v1 matrix macros
│   │   └── whole_c1..c3_r0..r3.mac   the ACTUAL generated v1 whole-slab macros
│   ├── plot/                         ROOT analysis / figure macros (v1)
│   │   ├── plot_collection_efficiency.C   per-config eff maps + grids
│   │   ├── plot_whole_slab.C              full-plate eff maps
│   │   ├── report_figures.C               bars/yield/detected/wavelength/radius
│   │   └── sipm_layouts.C                 3-panel SiPM-layout schematic
│   └── vis/                          event-display macros (v1)
│       ├── muon_vis.mac              head-less TSG event display (used by report)
│       ├── vis.mac                   interactive vis
│       └── vis_filterSD.mac          vis with SD filtering
├── driver/                           the ad-hoc local run scripts used for v1
│   ├── local_matrix_1000.sh          matrix @1000 ev/pos
│   ├── local_matrix_newpos.sh        matrix with corrected (100,0)/(100,100) sites
│   ├── local_whole_10000.sh          whole-slab @10000 ev
│   └── flatten_figs.sh               copy figure PDFs into report/figures
├── figures/                          the v1 report figures (as generated)
└── matrix_report.tex                 (add archival copy — see below)
```

The bundled `figures/` are the ones the v1 report referenced: `sipm_layouts.pdf`
(3-panel, original placements), `efficiency_grid_cfg{1,2,3}_all.pdf`, the
`fig_{eff_bars,detected,yield,wavelength,radius}.pdf` set, `whole_slab_cfg{1,2,3}.pdf`,
`light_eff_c1_{none,glossy}.pdf` and `muon_event_display.png`.

## How v1 was produced (step by step)

1. **Build** (at git `c97b424`): `cmake --build build -j`.
2. **Matrix scan** — for each of the three configs, four reflectors and three
   beam positions, 1000 events each, flux map OFF:
   ```
   cd slurm
   bash ../versions/v1/macros/sim/generate_muon_matrix.sh   # writes macros/
   bash ../versions/v1/macros/sim/run_muon_matrix.sh        # runs them
   ```
   Output: `data_output/muon_out_c<C>_r<R>_{centre,edge,corner}.root`.
   The ready-made v1 macros are already bundled in `macros/sim/`.
3. **Whole-slab scan** — full 250×250 mm² GPS plane, 10000 events per
   (config, reflector): run `macros/sim/whole_c*_r*.mac`; output
   `data_output_whole/muon_whole_out_c<C>_r<R>.root`.
4. **Figures** — from the repo root:
   ```
   root -l -b -q 'versions/v1/macros/plot/report_figures.C("data_output","report/figures")'
   root -l -b -q 'versions/v1/macros/plot/plot_collection_efficiency.C+("data_output",5.0,"",125.0)'
   root -l -b -q 'versions/v1/macros/plot/sipm_layouts.C("report/figures")'
   for C in 1 2 3; do
     root -l -b -q "versions/v1/macros/plot/plot_whole_slab.C+(\"data_output_whole\",$C,5.0,125.0,true)"
   done
   ```
   Copy `data_output*/plots/efficiency_grid_cfg*_all.pdf` and
   `whole_slab_cfg*.pdf` into `report/figures/`.
5. **Geometry / event-display figure** —
   ```
   ./build/G4DCSmonitor versions/v1/macros/vis/muon_vis.mac
   ```
   produces `report/figures/muon_event_display.png`.
6. **Report** — `pdflatex matrix_report.tex` (twice) from `report/`.

## Add the v1 report source

The v1 `matrix_report.tex` (and its `report/figures/*.pdf`) are **not** in this
repo: drop the archival copy from your other machine into `versions/v1/` as
`versions/v1/matrix_report.tex` (and `versions/v1/figures/`). The macros above
regenerate every figure it references.

## v1 result summary (for reference)

Historical (v1) collection efficiencies are recorded in the v1 `matrix_report.pdf`
(archived by the author); they were produced from data that has since been
overwritten, so the exact numbers are not reproduced here. The physical ordering
was `bare ≪ TiO₂ < Al ≈ glossy` (matching Rao et al. 2025), same as v2.
