#!/usr/bin/env bash
# =============================================================================
# Generate the bare-plate / black-holder contact-fraction scan (SLURM).
#
# Sweeps the effective contact fraction f = 0 .. 0.03 on bare (reflector 0)
# plates.  Configs 1-3 sit in the black holder; Config 4 has no holder shell,
# but its bare faces now carry the same contact-loss skin, so it is excluded by
# default only because it reuses the global nominal f (no dedicated scan).
#
#   CONTACT_FRACTIONS : space-separated f values (default 0.000 .. 0.030)
#   CONTACT_CFGS      : configs (default "1 2 3"; 4 reuses the global nominal)
#   FLUXMAP           : on|off (default off; the fast variant)
#
# Each macro runs centre/edge/corner back-to-back, like the matrix template.
# Outputs go to distinct case subdirectories so the standard
# muon_out_c<C>_r0_<pos>.root names remain compatible with analyze_muon.C:
#
#   <DATADIR>/c1_f0p000/muon_out_c1_r0_centre.root
#   <DATADIR>/c1_f0p020/...
#
# Usage:  slurm/generate_contact_scan.sh [NEVENTS] [THREADS] [DATADIR]
#         defaults: 1000 32 data_v2.2/contact
#
# The template is slurm/muon_characterization_template.mac; edit that, not the
# generated macros.  The manifest lists one macro per line, in array order.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${CONTACT_TEMPLATE:-${HERE}/muon_characterization_template.mac}"
OUTDIR="${CONTACT_OUTDIR:-${HERE}/muon_contact}"
DATADIR="${3:-data_v2.2/contact}"
NEVENTS="${1:-1000}"
THREADS="${2:-32}"

CONTACT_FRACTIONS="${CONTACT_FRACTIONS:-0.000 0.005 0.010 0.015 0.020 0.025 0.030}"
CONTACT_CFGS="${CONTACT_CFGS:-1 2 3}"
CONTACT_ALBACKING="${CONTACT_ALBACKING:-grease}"
FLUXMAP="${FLUXMAP:-off}"

case "${FLUXMAP}" in
    on|true|1)   FLUXMAP_LINE="/dcs-monitor/run/writeOpticalFluxMap true" ;;
    off|false|0) FLUXMAP_LINE="# (flux map off)" ;;
    *) echo "ERROR: FLUXMAP must be on or off, got '${FLUXMAP}'" >&2; exit 1 ;;
esac

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}"

# Resolve relative DATADIR against slurm/ for directory creation; the macro
# keeps the string exactly as given, and the sbatch scripts cd into slurm/.
if [[ "${DATADIR}" == /* ]]; then
    DATA_ROOT="${DATADIR}"
else
    DATA_ROOT="${HERE}/${DATADIR}"
fi

MANIFEST="${OUTDIR}/manifest.txt"
: > "${MANIFEST}"
i=0
for cfg in ${CONTACT_CFGS}; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for f in ${CONTACT_FRACTIONS}; do
        tag="f${f//./p}"
        case_dir="${DATADIR}/c${cfg}_${tag}"
        mkdir -p "${DATA_ROOT}/c${cfg}_${tag}"

        out="contact_c${cfg}_${tag}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"                  \
            -e "s/@PRESET@/${preset}/g"               \
            -e "s/@REFLECTOR@/0/g"                    \
            -e "s/@BARECONTACT@/${f}/g"               \
            -e "s/@ALBACKING@/${CONTACT_ALBACKING}/g" \
            -e "s/@NEVENTS@/${NEVENTS}/g"             \
            -e "s/@THREADS@/${THREADS}/g"             \
            -e "s#@DATADIR@#${case_dir}#g"            \
            -e "s|@FLUXMAP@|${FLUXMAP_LINE}|g"        \
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
echo "  fractions='${CONTACT_FRACTIONS}', configs='${CONTACT_CFGS}', fluxmap=${FLUXMAP}"
