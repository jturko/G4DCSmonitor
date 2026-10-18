#!/usr/bin/env bash
# =============================================================================
# Generate the aluminium-foil backing comparison (grease vs air) for SLURM.
#
# Uses reflector 2 (Al foil) and sweeps only the backing index:
#   grease : n=1.46 (default model)
#   air    : n=1.0003 (evaporated Al against air)
#
#   AL_BACKINGS : space-separated values (default "grease air")
#   AL_CFGS     : configs (default "1 2 3"; Config 4 is WLS-dominated)
#   FLUXMAP     : on|off (default off; the fast variant)
#
# Each macro runs centre/edge/corner back-to-back.  Outputs go to distinct case
# subdirectories so the standard muon_out_c<C>_r2_<pos>.root names remain
# compatible with analyze_muon.C:
#
#   <DATADIR>/c1_grease/muon_out_c1_r2_centre.root
#   <DATADIR>/c1_air/...
#
# Usage:  slurm/generate_al_backing.sh [NEVENTS] [THREADS] [DATADIR]
#         defaults: 1000 32 data_v2.2/al
#
# The template is slurm/muon_characterization_template.mac; edit that, not the
# generated macros.  The manifest lists one macro per line, in array order.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${AL_TEMPLATE:-${HERE}/muon_characterization_template.mac}"
OUTDIR="${HERE}/muon_al"
DATADIR="${3:-data_v2.2/al}"
NEVENTS="${1:-1000}"
THREADS="${2:-32}"

AL_BACKINGS="${AL_BACKINGS:-grease air}"
AL_CFGS="${AL_CFGS:-1 2 3}"
AL_BARECONTACT="${AL_BARECONTACT:-0.000}"
FLUXMAP="${FLUXMAP:-off}"

case "${FLUXMAP}" in
    on|true|1)   FLUXMAP_LINE="/dcs-monitor/run/writeOpticalFluxMap true" ;;
    off|false|0) FLUXMAP_LINE="# (flux map off)" ;;
    *) echo "ERROR: FLUXMAP must be on or off, got '${FLUXMAP}'" >&2; exit 1 ;;
esac

for backing in ${AL_BACKINGS}; do
    if [[ "${backing}" != "grease" && "${backing}" != "air" ]]; then
        echo "ERROR: AL_BACKINGS entries must be grease or air, got '${backing}'" >&2
        exit 1
    fi
done

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}"

if [[ "${DATADIR}" == /* ]]; then
    DATA_ROOT="${DATADIR}"
else
    DATA_ROOT="${HERE}/${DATADIR}"
fi

MANIFEST="${OUTDIR}/manifest.txt"
: > "${MANIFEST}"
i=0
for cfg in ${AL_CFGS}; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for backing in ${AL_BACKINGS}; do
        case_dir="${DATADIR}/c${cfg}_${backing}"
        mkdir -p "${DATA_ROOT}/c${cfg}_${backing}"

        out="al_c${cfg}_${backing}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"                 \
            -e "s/@PRESET@/${preset}/g"              \
            -e "s/@REFLECTOR@/2/g"                   \
            -e "s/@BARECONTACT@/${AL_BARECONTACT}/g" \
            -e "s/@ALBACKING@/${backing}/g"          \
            -e "s/@NEVENTS@/${NEVENTS}/g"            \
            -e "s/@THREADS@/${THREADS}/g"            \
            -e "s#@DATADIR@#${case_dir}#g"           \
            -e "s|@FLUXMAP@|${FLUXMAP_LINE}|g"       \
            "${TEMPLATE}" > "${OUTDIR}/${out}"
        printf '%s\n' "${out}" >> "${MANIFEST}"
        printf 'array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

echo
echo "Wrote ${i} macros to ${OUTDIR}/"
echo "  manifest=${MANIFEST}"
echo "  nevts=${NEVENTS}, threads=${THREADS}, datadir=${DATADIR}"
echo "  backings='${AL_BACKINGS}', configs='${AL_CFGS}', fluxmap=${FLUXMAP}"
