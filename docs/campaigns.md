# Simulation campaigns and SLURM drivers

Read on demand from `AGENTS.md`.

**Ten-design campaign (`v7_opt`, report Sec. 10, 2026-10-21).** Ten whole-plate runs,
25 000 muons each, all files in `data_v7_opt/whole/<tag>.root` (production tree
`/home/turko46/bigdata/.../MuonScinti`); tags `c1_split_black`, `c8_split_black`,
`c10_wls4_tio2`, `c11_wls8_tio2`, `c1_edge_diffuse`, `c1_6mm_diffuse`, `c8_6mm_diffuse`,
`c10_wls4_glossy`, `c11_wls8_glossy`, `c1_same_1p55`. An eleventh design,
`c2_cornercut` (preset 12, glossy, `setCouplingIndex 1.55`), is reserved in the same table;
`slurm/run_cornercut.sh` submits **only** its matrix, whole-slab and v7_opt array task
(25 000 events), leaving the ten completed files untouched. The table (order, colour, human label,
data stem) lives in **`macros/opt_designs.h`** — the single source of names **and colours**:
`opt::label(i)`/`d.label` is what every figure prints (never the raw tag), `opt::color(i)`
repeats a design's colour in every figure of the section. The control is `c1_same_1p55`
(`setCouplingIndex 1.55`, 568.2 pe/evt = +9 % over the v7 default 1.46, 520.7), because a
denominator must be measured in the same campaign. Figures: `opt_yield.C` (yield bars +
per-event spectra), `opt_maps.C` (absolute + normalised efficiency maps, and the kappa bar
chart not used in the report), `opt_de.C` (detection efficiency vs threshold + the 90 %
threshold), `opt_waveform.C` (mean arrival-time profile); all write `opt_*.csv` beside them.
Position resolution needs the CPU-bound sweep: `macros/run_opt_recon.sh` (reuses
`position_reconstruction.C` through per-design symlinks) then `opt_recon_fig.C+("<raw.csv>")`.
`macros/build_report_figures.sh` runs the four fast macros when `data_v7_opt` is present
(`OPT_DIR` override) and redraws the recon figure only if `OPT_RECON_RAW` exists; it also
regenerates the two geometry figures of Sec. 10.1 (`opt_layouts.C` needs no data,
`render_designs.sh` runs only when `RENDERS=1`, the default).
**Geometry figures (Sec. 10.1).** `macros/opt_layouts.C [OUTDIR]` draws the three sensor
patterns (preset 1 at u=+-0.5, preset 8 at u=+-0.75, the four-sided clamp) as square 250 mm
plan views in the report frame with true-scale positions, plus the design names and the
6 mm footprint; sensor markers are drawn 2.5x oversize (a true 4 mm sensor is <2 pt at the
print width) and the caption says so. `macros/render_designs.sh` drives four geometry-only
macros (`vis_design_{c1_4mm,c1_6mm,wls10,wls11}.mac`: no beam, no trajectories,
`TSG_OFFSCREEN`, `transparency true`, forced opacity 1) into
`report/figures/design_ogl_{c1_4mm,c1_6mm,wls10,wls11,wls10_clamp}.png`; the 4/6 mm pair and
the 10/11 pair share one camera each. The 4/6 mm macros are single-sensor close-ups of the
+X narrow face (theta 65, phi 0, target 125.4 62.5 0 mm, zoomTo 13.5 = ~26 mm field) with
`lightsVector 0.5 0 1`, because the default light `(1,1,1)` shades the top and +X faces
identically; no in-image scale bar (`/vis/scene/add/scale` draws a 1-px line with a 13-px
label, a hairline at print size). The WLS macros must set `setFibreAnalytic false` **after**
`preset` (analytic `G4CutTubs` boolean results have no polyhedron and render as a
10 000-marker point cloud) and hide `WLSClampLV`, `WLSCoupleFilmLV`, `GreaseLV`; the
faceted variant is the one measured 20 % lower in yield, so the renders are geometry only.
**VRML** (visual, not in the report): `vrml/gen_v7opt_designs.sh` writes one `.wrl` per
design into the gitignored `vrml/v7_opt/`; the tessellated presets draw as solids, the
analytic ones as point clouds. View them with any VRML97 viewer or
`/vis/open VRML2FILE` from the generated macro, or convert with
`view3dscene`/`blender --python` for a still. Nothing under `vrml/` is committed.
Results (combined estimator, 5000 held-out events, mm): 6 mm 11.48/11.78 < TiO2-fibre
13.89/13.75 < control 15.94 < C8 black 17.41 < C1 black 18.75 < diffuse 19.85 << glossy-mirror fibre 55.23/60.71. The direct-coupled family follows
sqrt(N) (6 mm predicts 1.45, measured 1.39; diffuse edge predicts 20.6, measured 19.9); the white-clamp fibre design
carries ~6.6x the position information per photoelectron; the mirror clamp destroys the
information (55-61 mm vs the 72 mm uniform prior) because it transports a plate-wide line
integral to the bundle. Two plotting conventions matter: the map grids are 4x3 (10 panels +
2 blanks) with **one shared palette below the grid** on a taller canvas, and every figure is
sized so that printed capitals are >= 6.5 pt at 0.95-0.98 textwidth (ROOT scales
`SetTextSize` against the **pad**, not the canvas — measure the PNG, do not compute from the
canvas height).

**Movies**: `macros/build_movie.sh` (single pane, Config 1/glossy, blue optical photons) and
`macros/build_movie_quad.sh` (2×2 Config 1 × reflectors 0–3) are the older modes; both need
the X11 OpenGL stored viewer (`DISPLAY=:0`, Xwayland auth on this login node). Frames are
raster EPS → PNG via GraphicsMagick and assembled by `convert`/`ffmpeg`.

**Tooling**: `macros/{muon_scint_template.mac,generate_muon_matrix.sh,run_muon_matrix.sh}`,
`macros/plot_muon.C`, `macros/analyze_muon.C`, `macros/plot_collection_efficiency.C`
(tree-based collection-efficiency maps; `plot_light_efficiency.C` is a thin forwarder
kept for old command lines). `slurm/` bundle now submits **up to four arrays**: matrix
(config×reflector, 16 tasks), whole-slab (16), bare-contact scan (default cfg 1–3 × eleven
`f=0..0.05` = 99; v6 nominal `f=0.045`), and Al backing (cfg 1–3 × `grease|air` = 6). Each task runs
centre/edge/corner back-to-back; contact/Al use
`slurm/muon_characterization_template.mac` (flux map off, `--mem=4G`, `low`, 1 h, 32
threads, 1000 ev/position) and write case subdirs
`data_<VERSION>/contact/c<C>_f<val>/` and `data_<VERSION>/al/c<C>_<backing>/` with the
standard `muon_out_c<C>_r<R>_<pos>.root` names. `slurm/run_fast.sh [MATRIX_EVENTS]
[WHOLE_EVENTS] [MATRIX_THREADS] [WHOLE_THREADS]` is the **recommended SLURM driver**
(matrix flux map off, `--mem=4G`, `low`; defaults 200/10000/32/32); contact/Al knobs are
`CONTACT_EVENTS`/`CONTACT_THREADS`/`CONTACT_FRACTIONS`/`CONTACT_CFGS` and
`AL_EVENTS`/`AL_THREADS`/`AL_BACKINGS`/`AL_CFGS` (defaults 1000, matrix threads, the grids
above). `SKIP_CONTACT=1`/`SKIP_AL=1` skip the respective calibration scan;
`SKIP_CALIBRATION=1` skips both and submits only matrix + whole-slab (the main-production
mode once the nominal surface parameters are locked). `run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS] [NEVENTS_CONTACT]
[NEVENTS_AL]` is the older heavier matrix/whole driver (flux map on, 64G, `cpu-genoa`) but
its contact/Al arrays still use the fast characterization template (4G, flux off). Key env
knobs: `BIN=/abs/path/G4DCSmonitor` (the **only** thing selecting a specific build — it must
be built on the cluster's module stack, e.g. via `slurm/build.sbatch`; the login-node
`build/` links Geant4 11.3.2), `VERSION` (data tag → `data_<VERSION>/`, default `v2.2` —
**override or it clobbers existing data**), `DATAROOT` (default `$PWD`; `cd` to the bigdata
dir first), per-array `MATRIX_*`/`WHOLE_*`/`CONTACT_*`/`AL_*`
partition/account/time/mem, `ARRAY_LIMIT` (applies to all submitted arrays), and
`CONTACT_ARRAY`/`AL_ARRAY` (override the auto manifest-derived bounds). When the
calibration scans are not skipped, both drivers regenerate the shared
`slurm/muon_contact/` + `slurm/muon_al/` macros, so use one driver at a time. `slurm/collect.sh` counts all four trees and prints rsync lines. Matrix/whole
generators emit cfg 1–4 (cfg 4 → `@PRESET@` = **7**, the analytic variant; preset 5 =
legacy tessellated vis-only); `slurm/run_local.sh <matrix|whole|all> [NEVENTS] [THREADS]` is
a no-SLURM local runner (defaults NEVENTS 1000, THREADS 8, `CFGS="1 2 3 4"`, `REFLS="0 1
2 3"`, `SUFFIX=_v2`, `PARALLEL=2`; override via env vars; it still covers matrix/whole
only). **There is no SLURM on this login node**, so local runs use `build/G4DCSmonitor`
(8 threads) while production runs on the HPC via `run_fast.sh`.

Reflective configs run ~2.5 evt/s vs ~100 evt/s
for bare (long multi-bounce tracks). TTree writer leaves a <2 kB partial file for the
position *currently* being written — **not** an abort.
