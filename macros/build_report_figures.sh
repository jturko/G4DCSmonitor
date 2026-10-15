#!/usr/bin/env bash
# =============================================================================
# build_report_figures.sh -- regenerate every figure used by
# report/matrix_report.tex from the simulation output and (re)build the PDF.
#
# Usage:
#   macros/build_report_figures.sh [MATRIX_DATA_DIR] [WHOLE_DATA_DIR]
#
# Defaults: slurm/data_v2.2/matrix  slurm/data_v2.2/whole
# (DATA_VERSION selects the tree; override the two dirs positionally if needed)
#
# Reads:
#   <MATRIX_DATA_DIR>/muon_out_c<C>_r<R>_{centre,edge,corner}.root
#   <WHOLE_DATA_DIR>/muon_whole_out_c<C>_r<R>.root
# Writes:
#   report/figures/{sipm_layouts,irradiation_sites,optical_spectra,
#                   fig_yield,fig_detected,fig_wavelength}.{pdf,png}
#   report/figures/{fig_pos_layouts,fig_pos_refl,fig_uniformity,
#                   fig_sipm_sharing}.{pdf,png}
#   report/figures/fig_pe_grid.{pdf,png}
#   report/figures/efficiency_grid_cfg<C>_all.{pdf,png}   (cfg 1..4)
#   report/figures/whole_slab_cfg<C>.{pdf,png}            (cfg 1..4)
#   report/figures/fig_whole_polar.{pdf,png}
#   report/figures/fig_eff_bars.{pdf,png}
#   report/figures/fig_whole_uniformity_map_cfg<C>.{pdf,png}   (cfg 1..4)
#   report/figures/fig_persistence_grid.{pdf,png}
#   report/figures/fig_wavelength_time_grid.{pdf,png}
#   report/figures/whole_uniformity.csv
#   report/matrix_report.pdf
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
ROOT_BIN="${ROOT_BIN:-${HOME}/programs/root/build/bin/root}"
DATA_VERSION="${DATA_VERSION:-v2.2}"
MATRIX_DIR="${1:-${ROOTDIR}/slurm/data_${DATA_VERSION}/matrix}"
WHOLE_DIR="${2:-${ROOTDIR}/slurm/data_${DATA_VERSION}/whole}"
FIGDIR="${ROOTDIR}/report/figures"

# Figure macros #include "style.h" (shared fonts/ticks + PetroffPalette P6).
# It lives outside the repo, so expose its directory to the ROOT interpreter and
# ACLiC.  Override with ROOT_STYLE_DIR if it lives elsewhere.
export ROOT_INCLUDE_PATH="${ROOT_STYLE_DIR:-${HOME}/root-work}${ROOT_INCLUDE_PATH:+:${ROOT_INCLUDE_PATH}}"

mkdir -p "${FIGDIR}"

echo "=== sipm layouts ==="
"${ROOT_BIN}" -l -b -q "macros/sipm_layouts.C(\"${FIGDIR}\")"

echo "=== irradiation sites ==="
"${ROOT_BIN}" -l -b -q "macros/irradiation_sites.C(\"${FIGDIR}\")"

echo "=== optical property tables (scint / WLS / SiPM) ==="
"${ROOT_BIN}" -l -b -q "macros/optical_spectra.C(\"${FIGDIR}\")"

echo "=== report figures (yield/wavelength) ==="
"${ROOT_BIN}" -l -b -q "macros/report_figures.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== photoelectron spectra grid (Rao Fig. 10 layout) ==="
"${ROOT_BIN}" -l -b -q "macros/pe_spectra_grid.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== position-resolved matrix analysis ==="
"${ROOT_BIN}" -l -b -q "macros/matrix_positions.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== efficiency-grid maps, one per config ==="
# plot_collection_efficiency writes <inDir>/plots/efficiency_grid_cfg<C>_all.{pdf,png}
"${ROOT_BIN}" -l -b -q "macros/plot_collection_efficiency.C+(\"${MATRIX_DIR}\", 5.0, \"\", 125.0)"
for cfg in 1 2 3 4; do
    cp -f "${MATRIX_DIR}/plots/efficiency_grid_cfg${cfg}_all.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${MATRIX_DIR}/plots/efficiency_grid_cfg${cfg}_all.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab maps, one per config ==="
for cfg in 1 2 3 4; do
    "${ROOT_BIN}" -l -b -q "macros/plot_whole_slab.C+(\"${WHOLE_DIR}\", ${cfg}, 5.0, 125.0, true)"
    cp -f "${WHOLE_DIR}/plots/whole_slab_cfg${cfg}.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${WHOLE_DIR}/plots/whole_slab_cfg${cfg}.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab uniformity (polar + 2-D cell) ==="
"${ROOT_BIN}" -l -b -q "macros/whole_slab_uniformity.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== arrival-time persistence and wavelength-vs-time (glossy, cfg 1-4) ==="
"${ROOT_BIN}" -l -b -q "macros/persistence_map_grid.C(\"${WHOLE_DIR}\",\"${FIGDIR}\",3)"
"${ROOT_BIN}" -l -b -q "macros/wavelength_time_grid.C(\"${WHOLE_DIR}\",\"${FIGDIR}\",3)"

echo "=== LaTeX (two passes for refs/toc) ==="
( cd "${ROOTDIR}/report" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )
( cd "${ROOTDIR}/report" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )

echo "=== done: ${ROOTDIR}/report/matrix_report.pdf ==="
