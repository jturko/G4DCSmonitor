#!/usr/bin/env bash
# =============================================================================
# Generate the MuonScint configuration matrix from a single template (SLURM).
#
# Scans, for Rao et al. (J. Appl. Phys. 138, 114504):
#   CONFIG    in {1,2,3,4}        (SiPM layouts; 4 = single SiPM + WLS clamp)
#   REFLECTOR in {0,1,2,3}        (none/TiO2/Al/glossy)
# giving 4 * 4 = 16 macros, written to slurm/muon/.
#
# CONFIG 4 maps onto geometry preset 7 (the analytic G4CutTubs round fibres;
# presets 1-3 stay Configs 1-3, preset 5 is the tessellated round vis variant,
# preset 6 the square-fibre A/B).
#
# Each macro runs all THREE beam positions (centre/edge/corner) in one process
# after a single /run/initialize, so the 16 macros map 1:1 onto a SLURM job
# array (array index 0..15).  Each macro requests @THREADS@ Geant4 threads,
# which must match the SLURM --cpus-per-task.
#
# The template is slurm/muon_scint_template.mac; edit that, not the outputs.
#
# Usage:  slurm/generate_muon_matrix.sh [NEVENTS] [THREADS] [DATADIR]
#         (default NEVENTS=200, THREADS=32, DATADIR=data_v2.2/matrix)
#
# DATADIR is written verbatim into the macros.  The default is RELATIVE, so
# run_matrix.sbatch cd's into this slurm/ directory before running (portable
# across clusters); pass an absolute path here to write elsewhere.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${MATRIX_TEMPLATE:-${HERE}/muon_scint_template.mac}"
OUTDIR="${HERE}/muon"
DATADIR="${3:-data_v2.2/matrix}"
NEVENTS="${1:-200}"
THREADS="${2:-32}"

# Load nominal surface calibration values, if available.
ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.040}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}"
[[ "${DATADIR}" == /* ]] && mkdir -p "${DATADIR}"

# Emit an ordered index so the SLURM array maps deterministically, and write a
# manifest (one macro filename per line) that run_matrix.sbatch reads:
#   0=c1_r0  1=c1_r1  2=c1_r2  3=c1_r3  4=c2_r0 ...  15=c4_r3
MANIFEST="${OUTDIR}/manifest.txt"
: > "${MANIFEST}"
i=0
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
            -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
            -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
            -e "s#@DATADIR@#${DATADIR}#g" \
            "${TEMPLATE}" > "${OUTDIR}/${out}"
        printf '%s\n' "${out}" >> "${MANIFEST}"
        printf 'array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

echo
echo "Wrote $(ls -1 "${OUTDIR}"/scan_c?_r?.mac | wc -l) macros to ${OUTDIR}/"
echo "  manifest=${MANIFEST}"
echo "  nevts=${NEVENTS}, threads=${THREADS}, datadir=${DATADIR}"
