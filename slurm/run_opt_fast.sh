#!/usr/bin/env bash
# =============================================================================
# MuonScint Phase-2 optimization SLURM driver.
#
# Self-contained driver for the v7_opt whole-slab campaign.  It generates the
# 9 candidate macros from HANDOFF.md §6.2 and submits a single job array.
#
# Usage:
#   slurm/run_opt_fast.sh [WHOLE_EVENTS] [WHOLE_THREADS]
#
# Defaults: 10000 events / 32 threads.  Override via environment:
#   WHOLE_EVENTS / WHOLE_THREADS / WHOLE_PARTITION / WHOLE_ACCOUNT /
#   WHOLE_TIME / WHOLE_ARRAY / WHOLE_MEM / ARRAY_LIMIT /
#   DATAROOT / VERSION / BIN
#
# Output lands in ${DATAROOT}/data_${VERSION}/whole (default
# /bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole).
#
# No matrix, contact, or Al arrays are submitted by this driver.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load nominal surface calibration values, if available.
ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.045}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

WHOLE_EVENTS="${1:-${WHOLE_EVENTS:-10000}}"
WHOLE_THREADS="${2:-${WHOLE_THREADS:-32}}"

WHOLE_PARTITION="${WHOLE_PARTITION:-low}"
WHOLE_ACCOUNT="${WHOLE_ACCOUNT:-low}"
WHOLE_TIME="${WHOLE_TIME:-02:00:00}"
WHOLE_ARRAY="${WHOLE_ARRAY:-0-8}"
WHOLE_MEM="${WHOLE_MEM:-4G}"

ARRAY_LIMIT="${ARRAY_LIMIT:-}"

# Output root: HPC native path by default.
DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/MuonScinti}"
DATAROOT="$(cd "${DATAROOT}" 2>/dev/null && pwd || printf '%s' "${DATAROOT}")"
export DATAROOT

VERSION="${VERSION:-v7_opt}"
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

WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"

mkdir -p "${HERE}/logs" "${WHOLE_DATA}"

echo "=== MuonScint Phase-2 optimization SLURM driver ==="
echo "  whole  events : ${WHOLE_EVENTS}  (threads ${WHOLE_THREADS})"
echo "  whole  route  : -p ${WHOLE_PARTITION} -A ${WHOLE_ACCOUNT} -t ${WHOLE_TIME} --array=${WHOLE_ARRAY} --mem=${WHOLE_MEM}"
echo "  throttle      : ${ARRAY_LIMIT:-<none>}"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}/data_${VERSION}"

# ---- generate optimization whole-slab macros --------------------------------
echo
echo "--- generating Phase-2 whole-slab macros (${WHOLE_EVENTS} ev) ---"
"${HERE}/generate_opt_whole.sh" "${WHOLE_EVENTS}" "${WHOLE_THREADS}" "${WHOLE_DATA}"

# ---- submit the optimization array ------------------------------------------
echo
echo "--- submitting Phase-2 optimization array ---"

# The array tasks locate the macros through ${SLURM_SUBMIT_DIR}/muon_whole_opt
# (run_opt_whole.sbatch), so the submit directory MUST be this slurm/ directory.
cd "${HERE}"

# ---- sanity: the array must not ask for more tasks than the manifest has -----
N_MAC="$(wc -l < "${HERE}/muon_whole_opt/manifest.txt")"
MAX_IDX="${WHOLE_ARRAY##*-}"
if [[ "${MAX_IDX}" =~ ^[0-9]+$ ]] && (( MAX_IDX >= N_MAC )); then
    echo "ERROR: WHOLE_ARRAY=${WHOLE_ARRAY} requests task ${MAX_IDX} but" >&2
    echo "       the manifest only has ${N_MAC} entries.  Fix WHOLE_ARRAY." >&2
    exit 1
fi

wt_array="${WHOLE_ARRAY}"
if [[ -n "${ARRAY_LIMIT}" ]]; then
    wt_array="${WHOLE_ARRAY%%%*}%${ARRAY_LIMIT}"
fi

jid_whole="$(sbatch --parsable \
    -p "${WHOLE_PARTITION}" -A "${WHOLE_ACCOUNT}" \
    -t "${WHOLE_TIME}" --array="${wt_array}" \
    --cpus-per-task="${WHOLE_THREADS}" --mem="${WHOLE_MEM}" \
    "${HERE}/run_opt_whole.sbatch")"

cat <<EOF

=== submitted (Phase-2 optimization) ===
  whole-slab array : job ${jid_whole}   (${WHOLE_EVENTS} ev, ${WHOLE_THREADS} threads, ${WHOLE_MEM}, -p ${WHOLE_PARTITION})
                     output: ${WHOLE_DATA}/

Watch:   squeue -u \$USER -o "%.10i %.14P %.13j %.2t %.10M %.6D %R"
Logs:    ${HERE}/logs/muonwholeopt_*_*.out
Collect: DATAROOT=${DATAROOT} VERSION=${VERSION} ${HERE}/collect.sh
EOF
