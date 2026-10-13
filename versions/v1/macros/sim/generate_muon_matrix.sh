#!/usr/bin/env bash
# =============================================================================
# Generate the MuonScint configuration matrix from a single template.
#
# Scans, for Rao et al. (J. Appl. Phys. 138, 114504):
#   CONFIG    in {1,2,3}          (SiPM layouts; Config 4/WLS excluded)
#   REFLECTOR in {0,1,2,3}        (none/TiO2/Al/glossy)
# giving 3 * 4 = 12 macros, written to macros/muon/.
#
# Each macro runs all THREE beam positions (centre/edge/corner) in one process
# after a single /run/initialize, so only 12 processes are needed instead of 36.
#
# The template is macros/muon_scint_template.mac; edit that, not the outputs.
#
# Usage:  macros/generate_muon_matrix.sh [NEVENTS] [THREADS]
#         (default NEVENTS=1000, THREADS=8)
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${HERE}/muon_scint_template.mac"
OUTDIR="${HERE}/muon"
DATADIR="$(cd "${HERE}/.." && pwd)/data_output"
NEVENTS="${1:-1000}"
THREADS="${2:-8}"

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}" "${DATADIR}"

for cfg in 1 2 3; do
    for refl in 0 1 2 3; do
        out="scan_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${NEVENTS}/g" \
            -e "s/@THREADS@/${THREADS}/g" \
            -e "s#@DATADIR@#${DATADIR}#g" \
            "${TEMPLATE}" > "${OUTDIR}/${out}"
    done
done

echo "Wrote $(ls -1 "${OUTDIR}"/scan_*.mac | wc -l) macros to ${OUTDIR}/ (nevts=${NEVENTS}, ${THREADS} threads)"
