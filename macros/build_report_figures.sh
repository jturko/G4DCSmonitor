#!/usr/bin/env bash
# =============================================================================
# build_report_figures.sh -- regenerate every figure used by
# report/matrix_report.tex from the simulation output and (re)build the PDF.
#
# Usage:
#   macros/build_report_figures.sh [MATRIX_DATA_DIR] [WHOLE_DATA_DIR] \
#                                  [CONTACT_DATA_DIR] [AL_DATA_DIR]
#
# Defaults: slurm/data_v6/{matrix,whole,contact} + slurm/data_v5/al
# (DATA_VERSION selects the tree; override dirs positionally if needed.
#  DATA_ROOT moves the whole data root, e.g.
#  DATA_ROOT=/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti)
# AL_MATRIX_DIR selects the matrix tree used for the Al reference curves; it
# defaults to the sibling matrix of AL_DATA_DIR so a v5 Al scan stays paired
# with its v5 matrix references.
#
# Reads:
#   <MATRIX_DATA_DIR>/muon_out_c<C>_r<R>_{centre,edge,corner}.root
#   <WHOLE_DATA_DIR>/muon_whole_out_c<C>_r<R>.root
#   <CONTACT_DATA_DIR>/c<C>_f<frac>/muon_out_c<C>_r0_<pos>.root
#   <AL_DATA_DIR>/c<C>_<backing>/muon_out_c<C>_r2_<pos>.root
# Writes:
#   report/figures/{sipm_layouts,irradiation_sites,optical_spectra,
#                   fig_yield,fig_detected,fig_wavelength}.{pdf,png}
#   report/figures/{fig_pos_layouts,fig_pos_refl,fig_uniformity,
#                   fig_sipm_sharing}.{pdf,png}
#   report/figures/{contact_scan,al_backing}.{pdf,png}
#   report/figures/{contact_scan,al_backing}.csv
#   report/figures/fig_pe_grid.{pdf,png}                  (configs 1,2,12,3,4)
#   report/figures/efficiency_grid_cfg<C>_all.{pdf,png}   (cfg 1..4)
#   report/figures/whole_slab_cfg<C>.{pdf,png}            (cfg 1,2,12,3,4)
#   report/figures/fig_whole_polar.{pdf,png}
#   report/figures/fig_eff_bars.{pdf,png}
#   report/figures/fig_whole_uniformity_map_cfg<C>.{pdf,png}   (cfg 1..4)
#   report/figures/{optical_timing,reflector_reflectivity,
#                   refractive_index}.{pdf,png}
#   report/figures/fig_persistence_grid.{pdf,png}
#   report/figures/fig_avg_waveform.{pdf,png}
#   report/figures/fig_wavelength_time_grid.{pdf,png}
#   report/figures/fig_recon_{waveforms,observables,features,response,map}.{pdf,png}
#   report/figures/fig_recon_layouts.{pdf,png}   (C1/C2/C2mod comparison)
#   report/figures/position_{reconstruction,observables,class_observables}.csv
#   report/figures/position_reconstruction_{c2,c12}.csv
#   report/figures/recon_layouts.csv
#   report/figures/whole_uniformity.csv
#   report/figures/whole_yield.csv
#   report/figures/fig_nearest_sensor.{pdf,png}
#   report/figures/nearest_sensor.csv
#   report/figures/nearest_sensor_maps.txt
#   report/figures/fig_de_threshold.{pdf,png}
#   report/figures/{fig_opt_yield,fig_opt_spectra,fig_opt_maps,
#                   fig_opt_maps_norm,fig_opt_contrast,fig_opt_de,
#                   fig_opt_waveform}.{pdf,png}       (design study, Sec. 10)
#   report/figures/{opt_yield,opt_maps,opt_de,opt_de_summary,opt_waveform}.csv
#   report/figures/fig_opt_recon.{pdf,png}   (only if OPT_RECON_RAW exists)
#   report/figures/fig_design_layouts.{pdf,png}       (Sec. 10.1 plan views)
#   report/figures/design_ogl_*.png                   (Sec. 10.1 renders, RENDERS=1)
#   report/figures/de_threshold.csv
#   report/figures/de_threshold_window.csv
#   report/matrix_report.pdf
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
ROOT_BIN="${ROOT_BIN:-${HOME}/programs/root/build/bin/root}"
DATA_VERSION="${DATA_VERSION:-v6}"
# Root of the data trees; the repo-local slurm/ tree by default, override to
# point at the shared production root, e.g.
#   DATA_ROOT=/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti
DATA_ROOT="${DATA_ROOT:-${ROOTDIR}/slurm}"
MATRIX_DIR="${1:-${DATA_ROOT}/data_${DATA_VERSION}/matrix}"
WHOLE_DIR="${2:-${DATA_ROOT}/data_${DATA_VERSION}/whole}"
CONTACT_DIR="${3:-${DATA_ROOT}/data_${DATA_VERSION}/contact}"
# The aluminium-backing scan is still taken from the v5 tree: the v6 al run is
# INCOMPLETE (the c*_air subdirectories hold only 2 of the 3 position files),
# so v6 cannot be used for this scan.  For the same reason the Al reference
# curves are taken from the v5 matrix tree, which is the matrix run the v5 al
# scan was produced with; mixing a v6 matrix with a v5 al scan would compare
# the two backings against a different set of reference curves.
AL_DIR="${4:-${DATA_ROOT}/data_v5/al}"
AL_MATRIX_DIR="${AL_MATRIX_DIR:-${DATA_ROOT}/data_v5/matrix}"
FIGDIR="${ROOTDIR}/report/figures"

# Figure macros #include "style.h" (shared fonts/ticks + PetroffPalette P6).
# It lives outside the repo, so expose its directory to the ROOT interpreter and
# ACLiC.  Override with ROOT_STYLE_DIR if it lives elsewhere.
export ROOT_INCLUDE_PATH="${ROOT_STYLE_DIR:-${HOME}/root-work}:${ROOTDIR}/macros${ROOT_INCLUDE_PATH:+:${ROOT_INCLUDE_PATH}}"

mkdir -p "${FIGDIR}"

echo "=== sipm layouts ==="
"${ROOT_BIN}" -l -b -q "macros/sipm_layouts.C(\"${FIGDIR}\")"

echo "=== irradiation sites ==="
"${ROOT_BIN}" -l -b -q "macros/irradiation_sites.C(\"${FIGDIR}\")"

echo "=== optical property tables (scint / WLS / SiPM) ==="
"${ROOT_BIN}" -l -b -q "macros/optical_spectra.C(\"${FIGDIR}\")"

echo "=== source timing (scintillation pulse + WLS re-emission delay) ==="
"${ROOT_BIN}" -l -b -q "macros/optical_timing.C(\"${FIGDIR}\")"

echo "=== reflector reflectivity tables ==="
"${ROOT_BIN}" -l -b -q "macros/reflector_reflectivity.C(\"${FIGDIR}\")"

echo "=== refractive-index structure ==="
"${ROOT_BIN}" -l -b -q "macros/refractive_index.C(\"${FIGDIR}\")"

echo "=== report figures (yield/wavelength) ==="
"${ROOT_BIN}" -l -b -q "macros/report_figures.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== photoelectron spectra grid (Rao Fig. 10 layout) ==="
"${ROOT_BIN}" -l -b -q "macros/pe_spectra_grid.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== position-resolved matrix analysis ==="
"${ROOT_BIN}" -l -b -q "macros/matrix_positions.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== bare-holder contact-fraction scan ==="
"${ROOT_BIN}" -l -b -q "macros/contact_scan.C+(\"${CONTACT_DIR}\",\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== aluminium backing: grease vs air ==="
"${ROOT_BIN}" -l -b -q "macros/al_backing.C+(\"${AL_DIR}\",\"${AL_MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== efficiency-grid maps, one per config ==="
# plot_collection_efficiency writes <inDir>/plots/efficiency_grid_cfg<C>_all.{pdf,png}
"${ROOT_BIN}" -l -b -q "macros/plot_collection_efficiency.C+(\"${MATRIX_DIR}\", 5.0, \"\", 125.0)"
for cfg in 1 2 3 4; do
    cp -f "${MATRIX_DIR}/plots/efficiency_grid_cfg${cfg}_all.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${MATRIX_DIR}/plots/efficiency_grid_cfg${cfg}_all.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab maps, one per config (order 1,2,12,3,4) ==="
for cfg in 1 2 12 3 4; do
    "${ROOT_BIN}" -l -b -q "macros/plot_whole_slab.C+(\"${WHOLE_DIR}\", ${cfg}, 5.0, 125.0, true)"
    cp -f "${WHOLE_DIR}/plots/whole_slab_cfg${cfg}.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${WHOLE_DIR}/plots/whole_slab_cfg${cfg}.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab uniformity (polar + 2-D cell) ==="
"${ROOT_BIN}" -l -b -q "macros/whole_slab_uniformity.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== nearest-sensor collapse (efficiency vs. distance to the nearest sensor) ==="
"${ROOT_BIN}" -l -b -q "macros/nearest_sensor_collapse.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== detection efficiency vs threshold (paper observable, Eq. 1 of Rao et al.) ==="
"${ROOT_BIN}" -l -b -q "macros/de_threshold.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== whole-plate yield table (p.e. per event, config x wrapper) ==="
"${ROOT_BIN}" -l -b -q "macros/whole_yield_table.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== arrival-time persistence and wavelength-vs-time (glossy, cfg 1-4) ==="
"${ROOT_BIN}" -l -b -q "macros/persistence_map_grid.C(\"${WHOLE_DIR}\",\"${FIGDIR}\",3)"
"${ROOT_BIN}" -l -b -q "macros/wavelength_time_grid.C(\"${WHOLE_DIR}\",\"${FIGDIR}\",3)"
"${ROOT_BIN}" -l -b -q "macros/avg_waveform.C(\"${WHOLE_DIR}\",\"${FIGDIR}\",3)"

echo "=== position reconstruction from the SiPM pattern (config 1, whole slab) ==="
# the constant-fraction (alpha) scan adds ~50 min of k-NN runs; skip it with
# ALPHA_SCAN=0 when the figure and the csv are already in place
ALPHA_SCAN="${ALPHA_SCAN:-1}"
"${ROOT_BIN}" -l -b -q "macros/position_reconstruction.C+(\"${WHOLE_DIR}\",\"${FIGDIR}\",25000,${ALPHA_SCAN})"

# Config 2 and Config 2 (mod) were reconstructed with the same macro and are
# stored as separate CSVs; the summary figure only needs those tables.
if [ -s "${FIGDIR}/position_reconstruction_c2.csv" ] && \
   [ -s "${FIGDIR}/position_reconstruction_c12.csv" ]; then
    echo "=== reconstructed four-sensor layouts (Config 2, Config 2 mod) ==="
    "${ROOT_BIN}" -l -b -q "macros/recon_layouts.C+(\"${FIGDIR}\")"
fi

# ---------------------------------------------------------------- design study
# The ten-design campaign of Sec. 10 lives in its own tree (data_v7_opt) and is
# regenerated only when that tree is present; OPT_DIR overrides the location.
OPT_DIR="${OPT_DIR:-${DATA_ROOT}/data_v7_opt/whole}"
if [ -d "${OPT_DIR}" ]; then
    echo "=== design study: yield, spectra, maps, detection efficiency, timing ==="
    "${ROOT_BIN}" -l -b -q "macros/opt_yield.C(\"${OPT_DIR}\",\"${FIGDIR}\")"
    "${ROOT_BIN}" -l -b -q "macros/opt_maps.C(\"${OPT_DIR}\",\"${FIGDIR}\")"
    "${ROOT_BIN}" -l -b -q "macros/opt_de.C(\"${OPT_DIR}\",\"${FIGDIR}\")"
    "${ROOT_BIN}" -l -b -q "macros/opt_waveform.C(\"${OPT_DIR}\",\"${FIGDIR}\")"
    # Geometry of the ten designs, Sec. 10.1: plan views (needs no data) and
    # the head-less Geant4 close-ups.  RENDERS=0 skips the OpenGL/TSG step,
    # which needs a built binary at ${BIN:-${ROOTDIR}/build/G4DCSmonitor}.
    "${ROOT_BIN}" -l -b -q "macros/opt_layouts.C(\"${FIGDIR}\")"
    if [ "${RENDERS:-1}" = "1" ]; then
        bash "${ROOTDIR}/macros/render_designs.sh" || true
    else
        echo "  (design renders not redrawn: RENDERS=0)"
    fi
    # The position-resolution sweep is CPU-bound (~30 min), so it is only
    # redrawn from an existing raw table; produce it with macros/run_opt_recon.sh.
    OPT_RECON_RAW="${OPT_RECON_RAW:-${DATA_ROOT}/data_v7_opt/opt_recon_raw.csv}"
    if [ -f "${OPT_RECON_RAW}" ]; then
        "${ROOT_BIN}" -l -b -q "macros/opt_recon_fig.C+(\"${OPT_RECON_RAW}\",\"${FIGDIR}\",\"${FIGDIR}/opt_yield.csv\")"
    else
        echo "  (fig_opt_recon not redrawn: no ${OPT_RECON_RAW}; run macros/run_opt_recon.sh)"
    fi
else
    echo "=== design study: skipped (no ${OPT_DIR}) ==="
fi

echo "=== LaTeX (two passes for refs/toc) ==="
( cd "${ROOTDIR}/report" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )
( cd "${ROOTDIR}/report" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )

echo "=== done: ${ROOTDIR}/report/matrix_report.pdf ==="
