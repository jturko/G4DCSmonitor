#!/usr/bin/env bash
# =============================================================================
# MuonScint LOCAL runner -- no SLURM, single machine.
#
# Companion to run_all.sh / run_fast.sh for hosts without sbatch.  Generates the
# macro set (flux map OFF, via the *_fast / whole templates) and runs the
# selected (config, reflector) tasks with the repo build.
#
# Usage:
#   slurm/run_local.sh <matrix|whole|all> [NEVENTS] [THREADS]
#
#     matrix : centre/edge/corner scan  -> <DATAROOT>/data_output<SUFFIX>/
#     whole  : full 250x250 mm^2 plate  -> <DATAROOT>/data_output_whole<SUFFIX>/
#
#   CFGS   (default "1 2 3 4")  selects configs (4 -> preset 7, analytic round).
#   REFLS  (default "0 1 2 3")  selects reflectors.
#   MATRIX_EVENTS / WHOLE_EVENTS override the positional NEVENTS per stage
#                               (e.g. MATRIX_EVENTS=500 WHOLE_EVENTS=10000).
#   SUFFIX (default "_v2")      version tag on the data dir names (matches the
#                               SLURM runs; the original dirs stay intact).
#   PARALLEL (default 2)        number of tasks run concurrently.  Each task
#                               uses THREADS threads, so pick PARALLEL*THREADS
#                               <= nproc.  Whole-slab scaling saturates near 4
#                               threads, so 2 tasks x 4 threads beats 1 x 8.
#   DATAROOT (default slurm/)   where the ROOT output lands.
#
# Examples:
#   slurm/run_local.sh matrix 1000 4
#   PARALLEL=2 CFGS="1 2 3 4" slurm/run_local.sh all 1000 4
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"

ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.040}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

WHAT="${1:-all}"
NEVENTS="${2:-1000}"
THREADS="${3:-8}"
CFGS="${CFGS:-1 2 3 4}"
REFLS="${REFLS:-0 1 2 3}"
SUFFIX="${SUFFIX:-_v2}"
PARALLEL="${PARALLEL:-2}"
DATAROOT="${DATAROOT:-${HERE}}"

if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: binary not found: ${BIN}" >&2
    echo "       build first: cmake --build ${ROOTDIR}/build -j" >&2
    exit 1
fi

mkdir -p "${HERE}/logs"
cd "${HERE}"

# gen_and_run <label> <outdir> <datadir> <template> <namestem> <nev>
gen_and_run() {
    local label="$1" outdir="$2" datadir="$3" template="$4" stem="$5" nev="$6"
    echo
    echo "=== ${label}: cfg {${CFGS}} x refl {${REFLS}}, ${nev} ev, ${THREADS} threads, ${PARALLEL} parallel ==="
    mkdir -p "${outdir}" "${datadir}"
    local running=0
    for cfg in ${CFGS}; do
        if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
        for refl in ${REFLS}; do
            local out="${outdir}/${stem}_c${cfg}_r${refl}.mac"
            sed -e "s/@CONFIG@/${cfg}/g"    \
                -e "s/@PRESET@/${preset}/g" \
                -e "s/@REFLECTOR@/${refl}/g" \
                -e "s/@NEVENTS@/${nev}/g" \
                -e "s/@THREADS@/${THREADS}/g" \
                -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
                -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
                -e "s#@DATADIR@#${datadir}#g" \
                "${template}" > "${out}"

            echo "--- ${stem}_c${cfg}_r${refl} ---  start $(date -Is)"
            (
                "${BIN}" "${out}" > "${HERE}/logs/${stem}_c${cfg}_r${refl}.local.log" 2>&1
                echo "--- ${stem}_c${cfg}_r${refl} ---  rc=$?  done $(date -Is)"
            ) &
            running=$((running + 1))
            if (( running >= PARALLEL )); then
                wait -n
                running=$((running - 1))
            fi
        done
    done
    wait
}

if [[ "${WHAT}" == "matrix" || "${WHAT}" == "all" ]]; then
    gen_and_run matrix "${HERE}/muon_local" "${DATAROOT}/data_output${SUFFIX}" \
                "${HERE}/muon_scint_template_fast.mac" scan "${MATRIX_EVENTS:-${NEVENTS}}"
fi

if [[ "${WHAT}" == "whole" || "${WHAT}" == "all" ]]; then
    gen_and_run whole "${HERE}/muon_whole_local" "${DATAROOT}/data_output_whole${SUFFIX}" \
                "${HERE}/whole_slab_template.mac" whole "${WHOLE_EVENTS:-${NEVENTS}}"
fi

echo
echo "Local run complete."
