#!/usr/bin/env bash
# =============================================================================
# Generate the MuonScint whole-slab irradiation macros from a single template.
#
# Emits, for Rao et al. (J. Appl. Phys. 138, 114504):
#   CONFIG    in {1,2,3,4}   (SiPM layouts; 4 = single SiPM + WLS clamp)
#   REFLECTOR in {0,1,2,3}   (none/TiO2/Al/glossy)
# giving 4 * 4 = 16 macros, written to slurm/muon_whole/.
#
# CONFIG 4 maps onto geometry preset 5 (the WLS clamp); presets 1-3 stay
# Configs 1-3 (preset 4 is the legacy 4-corner layout and is unused).
#
# Each macro irradiates the full 250 x 250 mm^2 plate and requests @THREADS@
# Geant4 threads (must match the SLURM --cpus-per-task). The collection-
# efficiency map is reconstructed offline from the sipmHits tree, so no
# histogram is booked here.
#
# Usage:  slurm/generate_whole_slab.sh [NEVENTS] [THREADS] [DATADIR]
#         (default NEVENTS=10000, THREADS=32, DATADIR=data_v2.2/whole)
#
# DATADIR is written verbatim into the macros; a relative path is resolved
# against the run_whole.sbatch working directory (slurm/).
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${HERE}/whole_slab_template.mac"
OUTDIR="${HERE}/muon_whole"
DATADIR="${3:-data_v2.2/whole}"
NEVENTS="${1:-10000}"
THREADS="${2:-32}"

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}"
[[ "${DATADIR}" == /* ]] && mkdir -p "${DATADIR}"

# Ordered index so the SLURM array maps deterministically, plus a manifest
# (one macro filename per line) that run_whole.sbatch reads:
#   0=c1_r0  1=c1_r1  2=c1_r2  3=c1_r3  4=c2_r0 ...  15=c4_r3
MANIFEST="${OUTDIR}/manifest.txt"
: > "${MANIFEST}"
i=0
for cfg in 1 2 3 4; do
    # preset: cfg 1-3 -> 1-3, cfg 4 (WLS) -> preset 5
    if [[ "${cfg}" == "4" ]]; then preset=5; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="whole_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${NEVENTS}/g" \
            -e "s/@THREADS@/${THREADS}/g" \
            -e "s#@DATADIR@#${DATADIR}#g" \
            "${TEMPLATE}" > "${OUTDIR}/${out}"
        printf '%s\n' "${out}" >> "${MANIFEST}"
        printf 'array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

echo
echo "Wrote $(ls -1 "${OUTDIR}"/whole_c?_r?.mac | wc -l) macros to ${OUTDIR}/"
echo "  manifest=${MANIFEST}"
echo "  nevts=${NEVENTS}, threads=${THREADS}, datadir=${DATADIR}"
