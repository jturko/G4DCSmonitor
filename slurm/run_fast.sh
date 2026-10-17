#!/usr/bin/env bash
# =============================================================================
# MuonScint FAST SLURM driver -- generate + submit the low-latency runs.
#
# This is a SELF-CONTAINED companion to run_all.sh; it does not modify or
# overwrite any of the existing slurm scripts.  It writes its own macro sets
# (muon_fast/, muon_whole_fast/) and uses two new job arrays.
#
# Differences vs run_all.sh:
#   * matrix flux map OFF (3x faster; efficiency comes from the sipmHits tree)
#   * --mem=4G (measured peak RSS ~0.4 GB)
#   * MATRIX --time=01:00:00 (~24 s/task at 32 cores), WHOLE --time=02:00:00
#     (~9-11 min/task at 32 cores; the 2h ceiling keeps margin at fewer threads,
#     since walltime scales ~1/N with thread count)
#   * routed to the `low` partition/account (145 idle nodes, fair-share 0.64),
#     overridable per array from the arguments / environment below.
#
# WORKFLOW (typical cluster use):
#   cd /path/to/the/dir/that/should/hold/the/output
#   /abs/path/to/repo/slurm/run_fast.sh [MATRIX_EVENTS] [WHOLE_EVENTS] \
#                                       [MATRIX_THREADS] [WHOLE_THREADS]
#   Output lands in  $PWD/data_<VERSION>/{matrix,whole}
#   (DATAROOT defaults to the CURRENT DIRECTORY, so run it from where you want
#    the data; set DATAROOT=/abs/path to write somewhere else. Logs and the
#    generated macros stay in slurm/ next to the scripts.)
#   BIN=/abs/path/to/G4DCSmonitor  selects a specific binary (see below).
#
# Usage:
#   run_fast.sh [MATRIX_EVENTS] [WHOLE_EVENTS] [MATRIX_THREADS] [WHOLE_THREADS]
#
# Defaults: 200 / 10000 / 32 / 32.  Each can also be set by environment:
# MATRIX_EVENTS / WHOLE_EVENTS / MATRIX_THREADS / WHOLE_THREADS.  Threads MUST
# match --cpus-per-task, which this script also passes to sbatch, so they stay
# in sync automatically.
#
# Routing/resource knobs (environment, all optional):
#   MATRIX_PARTITION (default low)      MATRIX_ACCOUNT (default low)
#   MATRIX_TIME      (default 01:00:00) MATRIX_ARRAY   (default 0-15)
#   MATRIX_MEM       (default 4G)       matrix --mem per task
#   WHOLE_PARTITION  (default low)      WHOLE_ACCOUNT (default low)
#   WHOLE_TIME       (default 02:00:00) WHOLE_ARRAY    (default 0-15)
#   WHOLE_MEM        (default 4G)       whole  --mem per task
#   ARRAY_LIMIT      (default empty)    throttle both arrays, e.g. ARRAY_LIMIT=4
#   DATAROOT         (default $PWD)     ROOT output root (absolute path)
#   VERSION          (default v2.2)     data-set tag -> data_<VERSION>/{matrix,whole}
#   BIN              (default pre-built HZDR binary, else ../build/G4DCSmonitor)
#
# Examples:
#   # 500 matrix / 20000 whole-slab events, 16 threads, 8 GB/task, to CWD:
#   MATRIX_THREADS=16 WHOLE_THREADS=16 MATRIX_MEM=8G WHOLE_MEM=8G ./run_fast.sh 500 20000
#   # fast whole-slab on the guaranteed default queue (no preemption):
#   WHOLE_PARTITION=cpu-genoa WHOLE_ACCOUNT=default WHOLE_TIME=02:00:00 ./run_fast.sh
#   # everything on `short` (15-min partition limit -> keep time <= 00:12:00):
#   MATRIX_PARTITION=short MATRIX_ACCOUNT=short MATRIX_TIME=00:12:00 ./run_fast.sh
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MATRIX_EVENTS="${1:-${MATRIX_EVENTS:-200}}"
WHOLE_EVENTS="${2:-${WHOLE_EVENTS:-10000}}"
MATRIX_THREADS="${3:-${MATRIX_THREADS:-32}}"
WHOLE_THREADS="${4:-${WHOLE_THREADS:-32}}"

MATRIX_PARTITION="${MATRIX_PARTITION:-low}"
MATRIX_ACCOUNT="${MATRIX_ACCOUNT:-low}"
MATRIX_TIME="${MATRIX_TIME:-01:00:00}"
MATRIX_ARRAY="${MATRIX_ARRAY:-0-15}"
MATRIX_MEM="${MATRIX_MEM:-4G}"

WHOLE_PARTITION="${WHOLE_PARTITION:-low}"
WHOLE_ACCOUNT="${WHOLE_ACCOUNT:-low}"
WHOLE_TIME="${WHOLE_TIME:-02:00:00}"
WHOLE_ARRAY="${WHOLE_ARRAY:-0-15}"
WHOLE_MEM="${WHOLE_MEM:-4G}"

ARRAY_LIMIT="${ARRAY_LIMIT:-}"

# Output root: default to the directory this script is invoked FROM (so "cd to
# where you want the data, then run it" works), resolved to an absolute path.
DATAROOT="${DATAROOT:-${PWD}}"
DATAROOT="$(cd "${DATAROOT}" 2>/dev/null && pwd || printf '%s' "${DATAROOT}")"
export DATAROOT

# Data-set version tag (see run_all.sh).  Output is laid out as
#   ${DATAROOT}/data_<VERSION>/matrix/   and   .../whole/
VERSION="${VERSION:-v2.2}"
export VERSION

# Executable: BIN env > pre-built HZDR binary > repo build.
if [[ -z "${BIN:-}" ]]; then
    if [[ -x /home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ]]; then
        BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
    else
        BIN="${HERE}/../build/G4DCSmonitor"
    fi
fi
export BIN
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: executable not found or not runnable: ${BIN}" >&2
    echo "       export BIN=/abs/path/to/G4DCSmonitor and re-run." >&2
    exit 1
fi

echo "=== MuonScint FAST SLURM driver ==="
echo "  matrix events : ${MATRIX_EVENTS}  (threads ${MATRIX_THREADS})"
echo "  whole  events : ${WHOLE_EVENTS}  (threads ${WHOLE_THREADS})"
echo "  matrix route  : -p ${MATRIX_PARTITION} -A ${MATRIX_ACCOUNT} -t ${MATRIX_TIME} --array=${MATRIX_ARRAY} --mem=${MATRIX_MEM}"
echo "  whole  route  : -p ${WHOLE_PARTITION} -A ${WHOLE_ACCOUNT} -t ${WHOLE_TIME} --array=${WHOLE_ARRAY} --mem=${WHOLE_MEM}"
echo "  throttle      : ${ARRAY_LIMIT:-<none>}"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}/data_${VERSION}"

MATRIX_TEMPLATE="${HERE}/muon_scint_template_fast.mac"
WHOLE_TEMPLATE="${HERE}/whole_slab_template.mac"
MATRIX_OUT="${HERE}/muon_fast"
WHOLE_OUT="${HERE}/muon_whole_fast"
MATRIX_DATA="${DATAROOT}/data_${VERSION}/matrix"
WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"

for f in "${MATRIX_TEMPLATE}" "${WHOLE_TEMPLATE}"; do
    [[ -f "${f}" ]] || { echo "ERROR: template missing: ${f}" >&2; exit 1; }
done

mkdir -p "${HERE}/logs" "${MATRIX_OUT}" "${WHOLE_OUT}" "${MATRIX_DATA}" "${WHOLE_DATA}"
cd "${HERE}"

# ---- generate matrix macros (flux map OFF, fast template) -------------------
echo
echo "--- generating FAST matrix macros (${MATRIX_EVENTS} ev/position) ---"
: > "${MATRIX_OUT}/manifest.txt"
i=0
for cfg in 1 2 3 4; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="scan_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${MATRIX_EVENTS}/g" \
            -e "s/@THREADS@/${MATRIX_THREADS}/g" \
            -e "s#@DATADIR@#${MATRIX_DATA}#g" \
            "${MATRIX_TEMPLATE}" > "${MATRIX_OUT}/${out}"
        printf '%s\n' "${out}" >> "${MATRIX_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

# ---- generate whole-slab macros (existing whole template) -------------------
echo
echo "--- generating FAST whole-slab macros (${WHOLE_EVENTS} ev) ---"
: > "${WHOLE_OUT}/manifest.txt"
i=0
for cfg in 1 2 3 4; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="whole_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${WHOLE_EVENTS}/g" \
            -e "s/@THREADS@/${WHOLE_THREADS}/g" \
            -e "s#@DATADIR@#${WHOLE_DATA}#g" \
            "${WHOLE_TEMPLATE}" > "${WHOLE_OUT}/${out}"
        printf '%s\n' "${out}" >> "${WHOLE_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

# ---- submit both arrays with routing overrides ------------------------------
echo
echo "--- submitting FAST job arrays ---"

mt_array="${MATRIX_ARRAY}"
wt_array="${WHOLE_ARRAY}"
if [[ -n "${ARRAY_LIMIT}" ]]; then
    mt_array="${MATRIX_ARRAY%%%*}%${ARRAY_LIMIT}"
    wt_array="${WHOLE_ARRAY%%%*}%${ARRAY_LIMIT}"
fi

jid_matrix="$(sbatch --parsable \
    -p "${MATRIX_PARTITION}" -A "${MATRIX_ACCOUNT}" \
    -t "${MATRIX_TIME}" --array="${mt_array}" \
    --cpus-per-task="${MATRIX_THREADS}" --mem="${MATRIX_MEM}" \
    "${HERE}/run_matrix_fast.sbatch")"

jid_whole="$(sbatch --parsable \
    -p "${WHOLE_PARTITION}" -A "${WHOLE_ACCOUNT}" \
    -t "${WHOLE_TIME}" --array="${wt_array}" \
    --cpus-per-task="${WHOLE_THREADS}" --mem="${WHOLE_MEM}" \
    "${HERE}/run_whole_fast.sbatch")"

cat <<EOF

=== submitted (FAST) ===
  matrix array     : job ${jid_matrix}   (${MATRIX_EVENTS} ev/pos, ${MATRIX_THREADS} threads, ${MATRIX_MEM}, -p ${MATRIX_PARTITION})
                     output: ${MATRIX_DATA}/
  whole-slab array : job ${jid_whole}   (${WHOLE_EVENTS} ev, ${WHOLE_THREADS} threads, ${WHOLE_MEM}, -p ${WHOLE_PARTITION})
                     output: ${WHOLE_DATA}/

Watch:   squeue -u \$USER -o "%.10i %.14P %.13j %.2t %.10M %.6D %R"
Logs:    ${HERE}/logs/muonscanfast_*_*.out
         ${HERE}/logs/muonwholefast_*_*.out
Collect: DATAROOT=${DATAROOT} VERSION=${VERSION} ${HERE}/collect.sh
Analyse: root -l -b -q 'macros/plot_collection_efficiency.C+("${WHOLE_DATA}")'
         root -l -b -q 'macros/plot_muon.C+("${MATRIX_DATA}")'
Report:  macros/build_report_figures.sh "${MATRIX_DATA}" "${WHOLE_DATA}"
EOF
