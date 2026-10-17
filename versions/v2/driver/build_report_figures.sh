#!/usr/bin/env bash
# =============================================================================
# versions/v2/driver/build_report_figures.sh
#
# Regenerate every figure used by versions/v2/matrix_report.tex from the
# simulation output, then compile the PDF.  Archived, self-contained copy of
# macros/build_report_figures.sh with paths re-pointed at this bundle:
#
#   macros  -> versions/v2/macros/plot/   (style.h in versions/v2/macros/common/)
#   figures -> versions/v2/figures/
#   TeX     -> versions/v2/matrix_report.tex
#
# Usage:
#   versions/v2/driver/build_report_figures.sh [MATRIX_DIR] [WHOLE_DIR] [TIMING_DIR]
#
# The archived report was built from a MIX of two data sets (both on the bigdata
# volume, NOT the repo-local slurm/data_v2.2):
#   * every figure/table EXCEPT the two timing figures -> data_v2.3
#     (matrix_positions.C and whole_slab_uniformity.C reproduce the archived
#      report/figures/*.csv byte-for-byte from data_v2.3/{matrix,whole});
#   * the arrival-time figures (fig_persistence_grid, fig_wavelength_time_grid)
#     -> data_v3, whose sipmHits tree adds the tDetected / wlDetected_nm vector
#     branches.  Those branches are absent from data_v2.2 and data_v2.3, so the
#     timing figures cannot be regenerated from v2.3.
#
# Defaults point at the production data sets under $DCS_DATA_ROOT
# (default ${HOME}/bigdata/DCS-Monitor-II/turko46/MuonScinti):
#   <root>/data_v2.3/matrix   <root>/data_v2.3/whole   <root>/data_v3/whole
# Override with positional args or by exporting DCS_DATA_ROOT.
#
# Reads:
#   <MATRIX_DIR>/muon_out_c<C>_r<R>_{centre,edge,corner}.root
#   <WHOLE_DIR>/muon_whole_out_c<C>_r<R>.root
#   <TIMING_DIR>/muon_whole_out_c<C>_r<R>.root   (needs tDetected/wlDetected_nm)
# Writes:
#   versions/v2/figures/*.{pdf,png}   and   versions/v2/matrix_report.pdf
#
# The raw ROOT data is NOT bundled in versions/v2 (it is large and lives on the
# bigdata volume).  Pass the three dirs explicitly, or regenerate the data first
# (see ../macros/sim/ and the README).
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLE="$(cd "${HERE}/.." && pwd)"           # versions/v2
ROOT_BIN="${ROOT_BIN:-${HOME}/programs/root/build/bin/root}"
DATA_ROOT="${DCS_DATA_ROOT:-${HOME}/bigdata/DCS-Monitor-II/turko46/MuonScinti}"

MATRIX_DIR="${1:-${DATA_ROOT}/data_v2.3/matrix}"
WHOLE_DIR="${2:-${DATA_ROOT}/data_v2.3/whole}"
TIMING_DIR="${3:-${DATA_ROOT}/data_v3/whole}"
FIGDIR="${BUNDLE}/figures"
PLOTDIR="${BUNDLE}/macros/plot"

# The figure macros #include "style.h" (bundled in versions/v2/macros/common).
export ROOT_INCLUDE_PATH="${BUNDLE}/macros/common${ROOT_INCLUDE_PATH:+:${ROOT_INCLUDE_PATH}}"

mkdir -p "${FIGDIR}"

echo "=== sipm layouts ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/sipm_layouts.C(\"${FIGDIR}\")"

echo "=== irradiation sites ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/irradiation_sites.C(\"${FIGDIR}\")"

echo "=== optical property tables (scint / WLS / SiPM) ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/optical_spectra.C(\"${FIGDIR}\")"

echo "=== report figures (yield/wavelength) ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/report_figures.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== photoelectron spectra grid (Rao Fig. 10 layout) ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/pe_spectra_grid.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

echo "=== position-resolved matrix analysis ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/matrix_positions.C(\"${MATRIX_DIR}\",\"${FIGDIR}\")"

# plot_collection_efficiency.C and plot_whole_slab.C write their output into
# "<dir>/plots/", so they are run against a throwaway staging dir of symlinks to
# avoid mutating the (production, possibly read-only) data directories.
STAGE="$(mktemp -d)"
trap 'rm -rf "${STAGE}"' EXIT
MATRIX_STAGE="${STAGE}/matrix"
WHOLE_STAGE="${STAGE}/whole"
mkdir -p "${MATRIX_STAGE}" "${WHOLE_STAGE}"
ln -sf "${MATRIX_DIR}"/*.root "${MATRIX_STAGE}/" 2>/dev/null
ln -sf "${WHOLE_DIR}"/*.root "${WHOLE_STAGE}/" 2>/dev/null

echo "=== efficiency-grid maps, one per config ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/plot_collection_efficiency.C+(\"${MATRIX_STAGE}\", 5.0, \"\", 125.0)"
for cfg in 1 2 3 4; do
    cp -f "${MATRIX_STAGE}/plots/efficiency_grid_cfg${cfg}_all.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${MATRIX_STAGE}/plots/efficiency_grid_cfg${cfg}_all.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab maps, one per config ==="
for cfg in 1 2 3 4; do
    "${ROOT_BIN}" -l -b -q "${PLOTDIR}/plot_whole_slab.C+(\"${WHOLE_STAGE}\", ${cfg}, 5.0, 125.0, true)"
    cp -f "${WHOLE_STAGE}/plots/whole_slab_cfg${cfg}.pdf" "${FIGDIR}/" 2>/dev/null
    cp -f "${WHOLE_STAGE}/plots/whole_slab_cfg${cfg}.png" "${FIGDIR}/" 2>/dev/null
done

echo "=== whole-slab uniformity (contrast + 2-D cell maps) ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/whole_slab_uniformity.C(\"${WHOLE_DIR}\",\"${FIGDIR}\")"

echo "=== arrival-time persistence and wavelength-vs-time (glossy, cfg 1-4) ==="
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/persistence_map_grid.C(\"${TIMING_DIR}\",\"${FIGDIR}\",3)"
"${ROOT_BIN}" -l -b -q "${PLOTDIR}/wavelength_time_grid.C(\"${TIMING_DIR}\",\"${FIGDIR}\",3)"

echo "=== LaTeX (two passes for refs/toc) ==="
( cd "${BUNDLE}" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )
( cd "${BUNDLE}" && pdflatex -interaction=nonstopmode matrix_report.tex >/dev/null 2>&1 )

echo "=== done: ${BUNDLE}/matrix_report.pdf ==="
