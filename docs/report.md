# Report generation and structure

Read on demand from `AGENTS.md`.

Regenerate the report with `macros/build_report_figures.sh [MATRIX_DIR] [WHOLE_DIR] [CONTACT_DIR] [AL_DIR]`
(defaults `slurm/data_v5/{matrix,whole,contact,al}`; pass absolute dirs when the data
live outside the repo) or, equivalently, with the make target
`make -f macros/Makefile.report` (same 21 macros, one target per figure group —
`make -f macros/Makefile.report help` lists them; `figures` skips the LaTeX pass,
`all` builds figures then typesets, `pdf` typesets only and has **no figure
prerequisites** — it must never be able to start a data-driven regeneration, since
`position_reconstruction.C` (ACLiC k-NN over 25 000 whole-slab events read over
sshfs) takes many minutes; rebuild it explicitly with
`make -f macros/Makefile.report reconstruction`). **The LaTeX pass has its own
`report/Makefile`** (`make -C report`): the default target builds
`matrix_report.pdf` with latexmk (falling back to repeated `pdflatex` when
latexmk is absent), `make -C report check` verifies that every
`\includegraphics` file exists, `warnings` lists the LaTeX warnings, and
`clean`/`distclean` drop the aux files / the PDF. It depends on the `.tex` and
on every referenced figure, so a regenerated figure triggers a rebuild; it
never builds figures itself — `make -C report figures` delegates to
`macros/Makefile.report`. The v6 report uses `data_v6/{matrix,whole,contact}` plus
`data_v5/al`, with `AL_MATRIX_DIR=<...>/data_v5/matrix` so the Al curves stay paired with
their v5 matrix references (the Al model is unchanged; v6 Al is incomplete). It runs `sipm_layouts.C`, `irradiation_sites.C`,
`optical_spectra.C`, `report_figures.C`, `pe_spectra_grid.C`, `matrix_positions.C`,
`contact_scan.C`, `al_backing.C`, `plot_collection_efficiency.C`, `plot_whole_slab.C`
for cfg 1–4 (and cfg 12 when `WHOLE_DIR` contains `muon_whole_out_c12_r*.root` —
`Makefile.report` adds that figure only if the preset-12 run is present), `whole_slab_uniformity.C`, `nearest_sensor_collapse.C`, `de_threshold.C`,
`whole_yield_table.C`, `persistence_map_grid.C`,
`wavelength_time_grid.C`, `avg_waveform.C`, `position_reconstruction.C+`, copies figures into `report/figures/`, then
typesets the PDF (`make -C report`). `DATA_ROOT` (default `${ROOTDIR}/slurm`) moves the data root; the
production tree lives at `/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti`.
**Report structure (2026-10 rewrite).** `report/matrix_report.tex` is organised around
questions, not data types: 1 Introduction · 2 The measurement being reproduced (what Rao et
al. actually measured) · 3 The detector and its optical model · 4 Simulation campaigns and
surface calibration · 5 Light collection: yield and the wrapper ordering · 6 The uniformity of
the response over the plate · 7 The spectral and temporal signature of the detected light ·
8 Position reconstruction from the SiPM pattern · 9 Comparison with the measurement ·
10 Caveats and open items · 11 Summary · Appendix A (Geant4 optical-physics notes) ·
Appendix B (the position-reconstruction machinery in detail). The
normalised whole-slab maps (old Figs. 25–28) were cut; `fig_nearest_sensor` and
`fig_de_threshold` were added. Every bibliography entry was verified against its DOI
record; the McPeak 2015 author list, the Geant4 PRM version/URL, the Eljen datasheet year
and the Amcrys datasheet title/URL were corrected.
**Section 8 was de-mathematised (2026-10-21).** It carried 13 of the report's 20 displayed
equations (the Weibull extreme-value derivation, three CFD definitions, the `D_4` group
presentation, the permutation matrices, the quotient distance, the standardisation
sums). The main text now has **4** displayed equations (`eq:tcfd-mean` the wavefront time,
`eq:risetime`, `eq:pair` the opposite-pair observables, `eq:knn-simple` the weighted
neighbour average) and states the estimator in plain words (relative pattern → rescale →
eight symmetries of the square → down-weight uninformative blocks); the formalism moved
verbatim to **Appendix B** (`app:recon`: B.1 first-arrival order statistics, B.2 the two
CFD variants not used for the headline results, B.3 the estimator in detail). The
`D_4`/`CFD` jargon was replaced in the main text by "symmetry-augmented" and
"wavefront time"; the numbers are unchanged.
