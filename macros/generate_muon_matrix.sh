#!/usr/bin/env bash
# =============================================================================
# Generate the MuonScint configuration matrix from a single template.
#
# Scans, for Rao et al. (J. Appl. Phys. 138, 114504):
#   CONFIG    in {1,2,3,4}        (SiPM layouts; 4 = single SiPM + WLS clamp)
#   REFLECTOR in {0,1,2,3}        (none/TiO2/Al/glossy)
# giving 4 * 4 = 16 macros, written to macros/muon/.
#
# CONFIG 4 is the WLS fibre clamp, which maps onto geometry preset 7 (the
# analytic G4CutTubs round fibres; presets 1-3 stay Configs 1-3, preset 5 is the
# tessellated round vis variant, preset 6 the square-fibre A/B).
#
# Each macro runs all THREE beam positions (centre/edge/corner) in one process
# after a single /run/initialize, so only 16 processes are needed instead of 48.
#
# The template is macros/muon_scint_template.mac; edit that, not the outputs.
#
# Output goes to <repo>/data_<VERSION>/matrix (VERSION default v2.2).
#
# Usage:  macros/generate_muon_matrix.sh [NEVENTS] [THREADS]
#         (default NEVENTS=1000, THREADS=8; env VERSION=v2.2)
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${HERE}/muon_scint_template.mac"
OUTDIR="${HERE}/muon"
VERSION="${VERSION:-v2.2}"
DATADIR="$(cd "${HERE}/.." && pwd)/data_${VERSION}/matrix"
NEVENTS="${1:-1000}"
THREADS="${2:-8}"

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}" "${DATADIR}"

for cfg in 1 2 3 4; do
    # preset: cfg 1-3 -> 1-3, cfg 4 (WLS) -> preset 7 (analytic round)
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="scan_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${NEVENTS}/g" \
            -e "s/@THREADS@/${THREADS}/g" \
            -e "s#@DATADIR@#${DATADIR}#g" \
            "${TEMPLATE}" > "${OUTDIR}/${out}"
    done
done

echo "Wrote $(ls -1 "${OUTDIR}"/scan_*.mac | wc -l) macros to ${OUTDIR}/ (nevts=${NEVENTS}, ${THREADS} threads)"
