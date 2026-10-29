#!/usr/bin/env bash
# =====================================================================
# rerun_failed.sh <array-job-id> [--time=HH:MM:SS]
#
# Resubmit the failed tasks of a p15 array job.  Port of ZLN_setup_v4's
# rerun_failed.sh: query sacct for terminal failure states, drop the
# indices still queued/running in squeue, then sbatch the same manifest
# (array index i = manifest line i+1) with a throttle.
#
# Env knobs as for run_p15.sh (DATAROOT/VERSION/MACDIR/REPO/BIN/THREADS/
# MEM/TIME/ARRAY_LIMIT/MERGE/KEEP_CHUNKS/PARTITION/ACCOUNT); TIME may
# also be given as --time=...
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="${REPO:-$(cd "${SCRIPT_DIR}/.." && pwd)}"

JOB=""
for a in "$@"; do
    case "${a}" in
        --time=*) TIME_LIMIT="${a#--time=}" ;;
        *) JOB="${a}" ;;
    esac
done
[ -n "${JOB}" ] || { echo "usage: rerun_failed.sh <array-job-id> [--time=HH:MM:SS]" >&2; exit 1; }

DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/SectorBkgd}"
VERSION="${VERSION:-v1}"
MACDIR="${MACDIR:-${DATAROOT}/macros_${VERSION}/p15}"
DATADIR="${DATAROOT}/data_${VERSION}"
LOGDIR="${DATADIR}/logs"
THREADS="${THREADS:-32}"
MEM="${MEM:-64G}"
TIME_LIMIT="${TIME_LIMIT:-06:00:00}"
ARRAY_LIMIT="${ARRAY_LIMIT:-128}"

MANIFEST="${MACDIR}/manifest.txt"
[ -f "${MANIFEST}" ] || { echo "FATAL: no manifest at ${MANIFEST}" >&2; exit 1; }
N_TASKS="$(wc -l < "${MANIFEST}")"
N_TASKS="${N_TASKS//[[:space:]]/}"

# failed terminal states (sacct), excluding anything still in squeue
FAILED_STATES_RE='^(FAILED|TIMEOUT|OUT_OF_MEMORY|NODE_FAIL|BOOT_FAIL|DEADLINE|PREEMPTED|CANCELLED)$'
declare -A ACTIVE=()
while read -r qid; do
    qid="${qid//[[:space:]]/}"
    ACTIVE["${qid}"]=1
done < <(squeue -h -j "${JOB}" --array -o '%i' 2>/dev/null || true)

FAILED=()
while IFS='|' read -r jid state; do
    jid="${jid//[[:space:]]/}"
    state="${state//[[:space:]]/}"
    if [[ "${jid}" =~ ^${JOB}_([0-9]+)$ ]]; then
        idx="${BASH_REMATCH[1]}"
        if [ -n "${ACTIVE[${jid}]:-}" ]; then
            continue   # still queued/running
        fi
        if [[ "${state}" =~ ${FAILED_STATES_RE} ]]; then
            FAILED+=("${idx}")
        fi
    fi
done < <(sacct -j "${JOB}" --noheader --parsable2 -o JobID,State 2>/dev/null || true)

if [ "${#FAILED[@]}" -eq 0 ]; then
    echo "rerun_failed.sh: no failed tasks for array job ${JOB}"
    exit 0
fi

# sanity: drop indices outside the current manifest (campaign regenerated?)
OK_LIST=()
for idx in "${FAILED[@]}"; do
    if [ "${idx}" -lt "${N_TASKS}" ]; then
        OK_LIST+=("${idx}")
    fi
done
if [ "${#OK_LIST[@]}" -eq 0 ]; then
    echo "rerun_failed.sh: all ${#FAILED[@]} failed indices are beyond the current manifest (${N_TASKS} lines); nothing to rerun"
    exit 0
fi

# resubmit in chunks of 1000 indices (sbatch --array spec length limits)
GROUP=1000
N_SUBMIT=$(( ( ${#OK_LIST[@]} + GROUP - 1 ) / GROUP ))
I=0
JOB_IDS=()
while [ "${I}" -lt "${#OK_LIST[@]}" ]; do
    CHUNK=("${OK_LIST[@]:${I}:${GROUP}}")
    I=$(( I + GROUP ))
    SPEC="$(IFS=,; echo "${CHUNK[*]}")%${ARRAY_LIMIT}"
    SBATCH_ARGS=(
        --parsable
        --job-name="p15_${VERSION}_rerun"
        --array="${SPEC}"
        --ntasks=1
        --nodes=1
        --cpus-per-task="${THREADS}"
        --mem="${MEM}"
        --time="${TIME_LIMIT}"
        --output="${LOGDIR}/p15_%A_%a.out"
        --error="${LOGDIR}/p15_%A_%a.err"
        --export=ALL,DATAROOT="${DATAROOT}",VERSION="${VERSION}",MACDIR="${MACDIR}",REPO="${REPO}",MERGE="${MERGE:-1}",KEEP_CHUNKS="${KEEP_CHUNKS:-1}"
    )
    if [ -n "${PARTITION:-}" ]; then
        SBATCH_ARGS+=(--partition="${PARTITION}")
    fi
    if [ -n "${ACCOUNT:-}" ]; then
        SBATCH_ARGS+=(--account="${ACCOUNT}")
    fi
    JOB_IDS+=("$(sbatch "${SBATCH_ARGS[@]}" "${SCRIPT_DIR}/p15_job.sbatch")")
done

echo "rerun_failed.sh: resubmitted ${#OK_LIST[@]} task(s) as ${JOB_IDS[*]}"
echo "  logs: ${LOGDIR}/"
