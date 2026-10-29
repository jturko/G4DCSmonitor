#!/usr/bin/env bash
# =====================================================================
# run_p15.sh -- generate the p1.5 background-sector production macros
# and submit the SLURM array (one task per config).
#
# Runs on the HPC.  The campaign lives under DATAROOT on the native
# /bigdata path (never the ~/bigdata sshfs mirror):
#
#   ${DATAROOT}/macros_${VERSION}/p15/   macros + manifest.txt
#   ${DATAROOT}/data_${VERSION}/p15/     per-config output dirs
#   ${DATAROOT}/data_${VERSION}/logs/    job logs, next to the data
#
# Env knobs (all optional unless noted):
#   DATAROOT    default /bigdata/DCS-Monitor-II/turko46/SectorBkgd
#   VERSION     default v1
#   MACDIR      default ${DATAROOT}/macros_${VERSION}/p15
#   BIN         default ${REPO}/build/G4DCSmonitor (absolute, never $PATH)
#   P1_BASE_DIR default /home/turko46/data/SurfaceFlux_v3
#   MEAS_TIME / CHUNK_S   default 180 / 18  (see generate_p15_macros.sh)
#   THREADS     default 32   (= --cpus-per-task; macro numberOfThreads)
#   MEM         default 64G
#   TIME        default 06:00:00
#   ARRAY_LIMIT default 128  (max simultaneously running array tasks)
#   SUBSET      empty = all manifest tasks.  Explicit --array spec instead,
#               e.g. SUBSET=0-5 (one cask's six hexants) or SUBSET=924-1847
#               (see README "manifest task index layout" for source ranges).
#   MERGE       default 1    (in-task hadd + sectorMeta consolidation)
#   KEEP_CHUNKS default 1    (1 = keep chunk files after verified merge)
#   PARTITION / ACCOUNT   empty = cluster default profile
#   DRY_RUN     1 = generate macros + print the sbatch command, don't submit
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"

DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/SectorBkgd}"
VERSION="${VERSION:-v1}"
MACDIR="${MACDIR:-${DATAROOT}/macros_${VERSION}/p15}"
DATADIR="${DATAROOT}/data_${VERSION}"
LOGDIR="${DATADIR}/logs"
P1_BASE_DIR="${P1_BASE_DIR:-/home/turko46/data/SurfaceFlux_v3}"
MEAS_TIME="${MEAS_TIME:-180}"
CHUNK_S="${CHUNK_S:-18}"
THREADS="${THREADS:-32}"
MEM="${MEM:-64G}"
TIME_LIMIT="${TIME:-06:00:00}"
ARRAY_LIMIT="${ARRAY_LIMIT:-128}"
MERGE="${MERGE:-1}"
KEEP_CHUNKS="${KEEP_CHUNKS:-1}"
DRY_RUN="${DRY_RUN:-0}"
BIN="${BIN:-${REPO}/build/G4DCSmonitor}"

echo "== run_p15.sh =="
echo "  repo       : ${REPO}"
echo "  binary     : ${BIN}"
echo "  data root  : ${DATAROOT} (version ${VERSION})"
echo "  p1 sources : ${P1_BASE_DIR}"
echo "  exposure   : ${MEAS_TIME} s per config in chunks of ${CHUNK_S} s"
echo "  array      : ${SUBSET:-(all manifest tasks)}"
echo "  resources  : ${THREADS} CPUs, ${MEM}, time ${TIME_LIMIT}, array limit ${ARRAY_LIMIT}"
echo "  merge      : ${MERGE} (keep chunks: ${KEEP_CHUNKS})"

if [ ! -x "${BIN}" ]; then
    if [ "${DRY_RUN}" = "1" ]; then
        echo "WARNING: ${BIN} not executable (build first: sbatch ${SCRIPT_DIR}/build.sbatch)"
    else
        echo "FATAL: ${BIN} is not executable -- build it on the HPC with sbatch ${SCRIPT_DIR}/build.sbatch" >&2
        exit 1
    fi
fi

if [ "${DRY_RUN}" != "1" ]; then
    mkdir -p "${MACDIR}" "${LOGDIR}"
    cd "${DATAROOT}"   # submit dir = campaign root
else
    echo "[dry-run] would mkdir -p ${MACDIR} ${LOGDIR} && cd ${DATAROOT}"
fi

# --- generate macros + manifest ---------------------------------------
export MACDIR DATADIR P1_BASE_DIR MEAS_TIME CHUNK_S THREADS
"${SCRIPT_DIR}/generate_p15_macros.sh"

N_TASKS="$(wc -l < "${MACDIR}/manifest.txt")"
N_TASKS="${N_TASKS//[[:space:]]/}"
if [ "${N_TASKS}" -le 0 ]; then
    echo "FATAL: empty manifest" >&2
    exit 1
fi
ARRAY="0-$(( N_TASKS - 1 ))%${ARRAY_LIMIT}"
if [ -n "${SUBSET:-}" ]; then
    ARRAY="${SUBSET}%${ARRAY_LIMIT}"
fi

# ---------------------------------------------------------------------
# Submit.  Default partition/account: no --partition/--account lines;
# export PARTITION=... / ACCOUNT=... to override.
# ---------------------------------------------------------------------
SBATCH_ARGS=(
    --parsable
    --job-name="p15_${VERSION}"
    --array="${ARRAY}"
    --ntasks=1
    --nodes=1
    --cpus-per-task="${THREADS}"
    --mem="${MEM}"
    --time="${TIME_LIMIT}"
    --output="${LOGDIR}/p15_%A_%a.out"
    --error="${LOGDIR}/p15_%A_%a.err"
    --export=ALL,DATAROOT="${DATAROOT}",VERSION="${VERSION}",MACDIR="${MACDIR}",REPO="${REPO}",MERGE="${MERGE}",KEEP_CHUNKS="${KEEP_CHUNKS}"
)
if [ -n "${PARTITION:-}" ]; then
    SBATCH_ARGS+=(--partition="${PARTITION}")
fi
if [ -n "${ACCOUNT:-}" ]; then
    SBATCH_ARGS+=(--account="${ACCOUNT}")
fi

SBATCH_CMD="sbatch $(printf '%q ' "${SBATCH_ARGS[@]}") ${SCRIPT_DIR}/p15_job.sbatch"

if [ "${DRY_RUN}" = "1" ]; then
    echo "[dry-run] ${N_TASKS} macros in manifest; would run:"
    echo "  ${SBATCH_CMD}"
    exit 0
fi

JOB_ID="$(sbatch "${SBATCH_ARGS[@]}" "${SCRIPT_DIR}/p15_job.sbatch")"

cat <<EOF

== submitted ==
  array job : ${JOB_ID}  (${N_TASKS} tasks, ${ARRAY})
  logs      : ${LOGDIR}/p15_${JOB_ID}_<task>.out / .err
  data      : ${DATADIR}/p15/<source>/cask<C>/gFuel<G>/
  macros    : ${MACDIR}/
  rerun failed tasks later with:  ${SCRIPT_DIR}/rerun_failed.sh ${JOB_ID}
  verify when done:  ${SCRIPT_DIR}/verify_p15.sh
EOF
