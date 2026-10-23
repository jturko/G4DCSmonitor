# Analysis macros and plotting

Read on demand from `AGENTS.md`.

**Position reconstruction from the SiPM pattern** (report §6, `macros/position_reconstruction.C`):
can the *relative* charge/timing pattern of the eight config-1 SiPMs localise the muon
inside the plate? Uses the whole-slab files (`data_v6/whole/muon_whole_out_c1_r{0..3}.root`,
25 000 events each) with the truth `muonX/muonY` as the label. Per event and SiPM it
extracts `N`, `tfirst` (earliest photoelectron), `tbar`, `NP3` (prompt fraction, t < 3 ns)
and `wbar`; features are the 8 pulse-integral shares, the 8 relative arrival times, the 8 prompt
fractions, or all 24. `N` = `nDetected` = the **total** photoelectrons detected in the channel
over the event, i.e. the *integral* of the light pulse (× gain) — **not** the pulse-peak
amplitude; no peak-amplitude observable is used anywhere. Measured prompt (<3 ns) fractions
of the light: bare 53 %, TiO₂ 34 %, Al/glossy 15–16 %, so a peak-amplitude read-out would
work with a minority of the photons and be noisier. Two estimators: (a) the analytic
"opposite-pair" observables
`A_x = (N_right−N_left)/(N_right+N_left)` and `T_x = <t>_right−<t>_left` whose linear
response gives noise/slope directly; (b) a k-NN regression (k = 40, standardised features,
20 000 train / 5 000 held out) — the simulation analogue of a calibration-scan response
function. **Result:** TiO₂ localises to σ_x ≈ 12.5 mm from the pulse integrals alone (the light
sharing is first order: `A_x` slope 7.4e-3/mm); glossy/Al have *no* first-order light
sharing (slope 5e-5/mm) but the arrival-time pattern still gives σ_x ≈ 19 mm (the
information is strongly non-linear, hence the learned estimator); bare ≈ 22 mm with all
channels. Figures `fig_recon_{waveforms,observables,features,response,map}.{pdf,png}` +
CSVs `position_{reconstruction,observables,class_observables}.csv` in `report/figures/`.
Run it through ACLiC (`.C+`, compute-bound) with `ROOT_INCLUDE_PATH=$HOME/root-work` set; add it to
`macros/build_report_figures.sh` (already done). The shared style file (`style.h`) lives in `/home/turko46/root-work/`. Note the ROOT drawing pitfalls hit here:
`TMarker`/`TLine` `Draw("SAME")` corrupt the pad range (use `TBox`/`TGraph`), and a
`TH1` drawn with `"BAR"` fills from the frame bottom, so bars must be `TBox`es.

**Optical constant-fraction (CFD) timing + the α scan** (same macro, 2026-10-21). The
timing observable is the *wavefront time* `t̄_{≤α}` = mean arrival time of the leading α
fraction of a channel's photoelectrons (α = 12.5 % nominal) — the optical analogue of a
constant-fraction discriminator; it removes the `N_s^{-1/2}` amplitude walk of the
first-photoelectron time. `macros/position_reconstruction.C` computes the three timing
variants (`tcfd` point quantile, `tcfdm` wavefront mean, `tcfdp` tapered Beta(1,3)) for
**every** α of `kAlpha[]` = {0.03, 0.05, 0.075, 0.10, 0.125, 0.15, 0.20, 0.30, 0.50}
(`kAlphaNom` = 4) and stores them in `Event::tcfdA/tcfdmA/tcfdpA`; the scan block then
re-runs the learned `kTimeCFD` and `kAll` estimators and the linear opposite-pair fit per
α → `position_alpha_scan.csv` + `fig_recon_alpha_scan.{pdf,png}`. The scan needs ~50 min
(72 extra k-NN runs); re-draw the figure alone with the 5th argument:
`root -l -b -q 'macros/position_reconstruction.C+("","report/figures",0,0,"report/figures/position_alpha_scan.csv")'`.

**The 3rd (width) panel is computed without the k-NN sweep** (2026-10-22): the 6th argument
`doAlphaScan = 2` is a *cheap* mode that builds only the linear rows, keeps `knnSig`/`knnSigAll`
from the existing CSV (matched on wrapper + α) and rewrites it, so it returns **before** both
the `res` (20 fits) and the α `k-NN` (72 fits) loops → ~30 s instead of ~50 min:
`root -l -b -q 'macros/position_reconstruction.C+("<whole dir>","report/figures",25000,2,"report/figures/position_alpha_scan.csv",1)'`
(the `1` only suppresses the Monte-Carlo mode; `2` is what makes it cheap). The CSV gained
`linear_width_ns` / `linear_width_pointquantile_ns` (cols 9–10) = `PooledBinWidth`, the rms of
`tcfdm` **at fixed muon x** (pooled within 25 mm bins, ≥50 entries): the position-conditioned
width, i.e. what a CFD is actually tuned on. It is *not* the global residual `linear_rms_noise`
(col 4, which also contains the curvature of the response and is what `linear_sigma_x_mm`
divides by the slope). Measured, α = 3 → 50 %: bare 1.377 → 1.342 ns and TiO₂ 0.888 → 0.854 ns
(flat), Al(air) 0.393 → 0.665 and glossy 0.391 → 0.675 (×1.7). The slope meanwhile grows from
0.0018 to 0.0029 ns/mm up to α ≈ 20 %, so the width and the slope move in opposite directions
and only their ratio has the interior minimum at α ≈ 15 % — neither single quantity is a good
tuning criterion.
**Figure layout:** `DrawAlphaScan` is a 2 × 2 canvas (1180 × 900 px, printed at `\textwidth`
→ 8.1 cm per panel, ~6.6 pt capitals as measured on the PNG): (a) width, (b) linear, (c)
learned, (d) key cell (wrapper key + estimator key + the dashed-line note). Do **not** go back
to a 1 × 3 layout: at 5.4 cm per panel every text falls below 6.5 pt. Two ROOT traps hit here:
a `TH1` axis y-title is **clipped at the pad edge** at this pad height whatever the
`TitleOffset` (draw it yourself with `TText` + `SetTextAngle(90)` and an empty axis title), and
long titles ("rms of T_{CFD,x} [ns]", "CFD fraction") overflow a 425-px axis — use "rms width
[ns]" / "rms error [mm]" / "alpha"
**Result:** the learned estimator is insensitive to α (< 6 % over α = 3–20 %, nominal within
1 % of the best); the *linear* opposite-pair timing read-out of the specular wrappers has a
clear minimum at α ≈ 10–15 % and grows ×1.5 at α = 50 % (glossy 182 → 279 mm) — the
reverberation-tail argument of the report, now measured. Two consequences now stated in the
report: (i) the minimum is **not resolved** better than α = 12.5 ± 2.5 % (the neighbouring
differences are ≤ 1 %, the statistical precision of the error with 5000 test events), and
(ii) the advantage of the wavefront time over the first-hit time quoted in Table 6 is an
optimum-value statement — 32 % in glossy paper at α = 12.5 %, 17 % at α = 5 %, and *negative*
at α = 50 % (279 mm vs 268 mm for the first hit). Caveats in the macro: ROOT has a
global `::kTop` (TGLFontManager.h) that clashes with a local `kTop` → the SiPM group is
called `kTopEdge`; `TText` needs `#include <TText.h>` (cling would auto-include it and
trigger the clash). The **`data_v7/whole`** tree is the campaign quoted in the report tables
(its files are byte-identical to `data_v6p5/whole` except `muon_whole_out_c4_r0.root`, which was
re-run on 2026-10-21 — the α scan uses cfg 1 only, so the numbers are unchanged). Not
`data_v6/whole`.


**Movies**: `macros/build_movie.sh` (single pane, Config 1/glossy, blue optical photons) and
`macros/build_movie_quad.sh` (2×2 Config 1 × reflectors 0–3) are the older modes; both need
the X11 OpenGL stored viewer (`DISPLAY=:0`, Xwayland auth on this login node). Frames are
raster EPS → PNG via GraphicsMagick and assembled by `convert`/`ffmpeg`.

`macros/persistence_map_grid.C [WHOLE_DIR] [OUTDIR] [REFL]` and
`macros/wavelength_time_grid.C [WHOLE_DIR] [OUTDIR] [REFL]` (default REFL 3 = glossy) consume
the per-photoelectron `tDetected`/`wlDetected_nm` vector branches of `sipmHits`: a 2×2 grid
over cfg 1–4 (whole-plate data), shared log-z, from the pooled per-SiPM times of each layout.
`persistence_map_grid` bins each event's summed arrival times into 0.1 ns bins and fills a map
(x = arrival time, y = p.e./bin, z = number of events); `wavelength_time_grid` fills
(x = arrival time, y = wavelength, z = p.e.). `macros/avg_waveform.C [WHOLE_DIR] [OUTDIR] [REFL]`
reuses the same per-event traces and averages them over events, overlaying cfg 1–4 as
mean p.e./0.1 ns vs arrival time (left linear, right log) → `fig_avg_waveform.{pdf,png}`
(report `fig:avgwaveform`).
**Per-panel titles:** the shared `style.h` calls `SetOptTitle(0)`, so a macro that wants the
histogram/panel title drawn must re-enable `gStyle->SetOptTitle(1)` *after* `style(...)`.
`whole_slab_uniformity.C` always did; `plot_whole_slab.C` and `plot_collection_efficiency.C`
(the 2×2 reflector grids) do too now — without it the four panel titles (reflector names)
vanish. **Report packing (as of the 2026-10 restructure):** the report was rewritten around
questions rather than data types; the four normalised whole-slab uniformity maps
(`fig:wholeunifmap`–`fig:wholeunifmap4`, old Figs. 25–28) were **cut as redundant** with the
absolute maps and replaced by `fig_nearest_sensor` (see *Nearest-sensor rule* below). The four
whole-slab maps (`fig:wholeslab`–`fig:wholeslab4`) are `0.74\textwidth` and now use `[H]`
with the discussion interleaved, one per page.
`macros/pe_spectra_grid.C [MATRIX_DIR] [OUTDIR]` draws the Fig. 10 of Rao et al. layout:
a 3 (region: centre/edge/corner) × 4 (config 1–4) grid in which every cell overlays the four
reflector treatments, x = photoelectrons/event, y = normalised entries (both log). Zero-p.e.
events sit below the log abscissa. → `report/figures/fig_pe_grid.{pdf,png}`.
All 12 cells share one x and one y range (computed globally after normalisation); the
centre/edge/corner row labels are **rotated 90°** (`SetTextAngle(90)`, `SetTextAlign(22)`),
drawn on the leftmost pad at NDC `(0.05, 0.5)` (left margin 0.22) so they sit clear of the
frame and axis title and are centred on their row; the single shared legend is drawn in the
top-right cell (config 4) at `(0.60, 0.593, 0.995, 0.803)` — its top-right corner.
`macros/optical_spectra.C [OUTDIR]` plots the optical property tables actually fed to
Geant4 (`src/GeometryMuonScint.cc`): scintillator `SCINTILLATIONCOMPONENT1` emission +
bulk `ABSLENGTH`, Y-11 `WLSABSLENGTH` + `WLSCOMPONENT`, and the SiPM `EFFICIENCY` (PDE)
vs wavelength → `report/figures/optical_spectra.{pdf,png}` (no data files needed). It is a
horizontal 3×1 grid, each panel reserving a top strip (`SetTopMargin(0.24)`) for a
borderless fill-0 legend, so no legend overlaps a curve; each panel's vertical dashed line
marks that panel's characteristic wavelength (scint emission 425 nm, WLS absorption onset
480 nm, SiPM PDE peak 420 nm) — defined in the report caption.
`macros/whole_slab_uniformity.C [WHOLE_DIR] [OUTDIR]` is the whole-slab uniformity add-on
(full-plate analogue of `matrix_positions.C`). **No radial metric**: the SiPM layouts have
no continuous rotational symmetry and the plate is square, so a radial average is a
geometric artefact (for r>125 mm only the four corner wedges populate). Instead it computes
one per-(layout,reflector) contrast, 2-D `kappa_2d` over 5×5 50 mm cells, plus a polar
`epsilon(r,phi)` map (12 r-bins × 12 φ-bins over all configs, shared log z, SiPM azimuths /
cfg4 WLS clamp band marked) and a normalized `epsilon/<epsilon>` map for each cfg 1–4
(**5 mm** cells matching `plot_whole_slab.C`, shared **log** z; slab outline + SiPM/WLS
markers overlaid via the same `drawOverlays`; map histograms carry ±18 mm padding bins so
outboard markers fit). Writes `fig_whole_polar`, `fig_whole_uniformity_map_cfg<C>`,
`fig_whole_contrast` and `whole_uniformity.csv` into OUTDIR. `kappa_2d` is statistically
robust (25000 ev) and, for the **v6** data, ranks **TiO2 > bare > Al ≳ glossy** in every
layout (cfg1 bare ≈1.55, cfg3 bare ≈3.82, TiO2 up to ≈8.31). *The ≈9.5 figure quoted in older
revisions of this file was the v2/v3 full-absorber bare model and no longer applies.*
`fig_whole_contrast` is the **full-slab analogue of the old 3-patch `fig_uniformity`**: a 2×2
grid (one panel per layout), each a 4-bin bar chart of `kappa_2d` per reflector, **linear y,
each panel scaled to its own maximum** (dashed unity line), as in the original
`matrix_positions.C` three-patch figure. The report's "Whole-slab uniformity" subsection (§5.3)
uses `fig_whole_contrast` (Fig. 24) as the visual for the `kappa_2d` numbers, plus
`tab:wholerefl` (Table 9: `kappa_2d`±err per layout × reflector, the linear bars' near-unity
entries resolved numerically), plus the four
normalised `fig_whole_uniformity_map_cfg<C>` maps (Figs. 25–28, all at `0.68\textwidth` so
two fit per page). The polar `(r,φ)` `fig_whole_polar` map and its radial defence are
**deliberately omitted** (retained in the repo in `whole_slab_uniformity.C` but not referenced
by the report). `whole_uniformity.csv` is retained. The 3-patch κ figure/table and the matrix
2D efficiency grids were **removed from the report** (the macros still exist); the position
contrast is now carried by the full-slab `kappa_2d`.
`macros/whole_yield_table.C [WHOLE_DIR] [OUTDIR]` writes `whole_yield.csv` — the absolute
mean p.e./event (and /detected-event, plus detection probability) for every config × wrapper
from the whole-plate run; almost every event detects ≥1 p.e. (399981/400000; 19 without a
detected photoelectron, so the two means differ negligibly). It backs report Table 8
(`tab:wholeyield`); the p.e./event columns are just the `tab:eff` plate-averaged ε scaled by
the ~19700 photons/event produced.
