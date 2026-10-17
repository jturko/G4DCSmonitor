#!/usr/bin/env bash
#SBATCH --job-name=muonscan-fast
#SBATCH --array=0-15
#SBATCH --cpus-per-task=32
#SBATCH --mem=4G
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --partition=low
#SBATCH --account=low
#SBATCH --time=01:00:00
#SBATCH --output=logs/muonscanfast_%A_%a.out
#SBATCH --error=logs/muonscanfast_%A_%a.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =============================================================================
# MuonScint matrix scan -- FAST variant (flux map OFF).
#
#   Matrix: 4 configs x 4 reflectors, centre/edge/corner in one process.
#   Measured ~24 s/task at 32 cores (no flux map).  Routed to the `low`
#   partition/account by default (huge idle count, fair-share 0.64); override
#   with MATRIX_PARTITION / MATRIX_ACCOUNT / MATRIX_TIME in the driver, or
#   directly:   sbatch -p short -A short --time=00:12:00 run_matrix_fast.sbatch
#
# NOTE on `short`: its partition TIMELIMIT is 15 minutes, so if you route there
# keep --time <= 00:12:00 (it has very few idle nodes; `low` is usually better).
#
# --cpus-per-task MUST equal the /run/numberOfThreads baked into the macros by
# generate_fast.sh (MATRIX_THREADS, default 32).
# =============================================================================

set -uo pipefail

# ---------------------------------------------------------------------------
# USER CONFIG -- adapt to your SLURM setup (same module stack as build.sbatch)
# ---------------------------------------------------------------------------
module purge
module load gcc/15.2.0
module load root/6.32.14
module load cmake/4.0.3
module load xerces/3.3.0
module load qtbase/6.10.1
module load geant4/11.4.0
# ---------------------------------------------------------------------------

HERE="${SLURM_SUBMIT_DIR:-$(pwd)}"
cd "${HERE}"
MACDIR="${HERE}/muon_fast"
DATAROOT="${DATAROOT:-${HERE}}"
VERSION="${VERSION:-v2.2}"
DATADIR="${DATAROOT}/data_${VERSION}/matrix"
LOGDIR="${HERE}/logs"
mkdir -p "${DATADIR}" "${LOGDIR}"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-32}"

# Executable: BIN env > pre-built HZDR binary > repo build.
if [[ -z "${BIN:-}" ]]; then
    if [[ -x /home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ]]; then
        BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
    else
        BIN="${HERE}/../build/G4DCSmonitor"
    fi
fi
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: executable not found or not runnable: ${BIN}" >&2
    echo "       export BIN=/abs/path/to/G4DCSmonitor" >&2
    exit 1
fi
PATH_BIN="$(command -v G4DCSmonitor 2>/dev/null || true)"
if [[ -n "${PATH_BIN}" && "${PATH_BIN}" != "${BIN}" ]]; then
    echo "note: PATH has a different G4DCSmonitor (${PATH_BIN}); using BIN=${BIN}"
fi

# Manifest: one macro (basename) per line, deterministic array order.
IDX="${SLURM_ARRAY_TASK_ID:-0}"
MANIFEST="${MACDIR}/manifest.txt"
if [[ -f "${MANIFEST}" ]]; then
    BASE="$(sed -n "$(( IDX + 1 ))p" "${MANIFEST}")"
else
    MAP=(scan_c1_r0 scan_c1_r1 scan_c1_r2 scan_c1_r3
         scan_c2_r0 scan_c2_r1 scan_c2_r2 scan_c2_r3
         scan_c3_r0 scan_c3_r1 scan_c3_r2 scan_c3_r3
         scan_c4_r0 scan_c4_r1 scan_c4_r2 scan_c4_r3)
    BASE="${MAP[$IDX]}"
fi
if [[ -z "${BASE}" ]]; then
    echo "ERROR: empty manifest entry for array id ${IDX}" >&2
    exit 1
fi

MAC="${MACDIR}/${BASE}"
if [[ ! -f "${MAC}" ]]; then
    echo "ERROR: macro missing: ${MAC}  (run generate_fast.sh first)" >&2
    exit 1
fi

echo "=== task ${IDX} : ${BASE} ==="
echo "    host      : $(hostname)"
echo "    partition : ${SLURM_JOB_PARTITION:-?}"
echo "    cpus/task : ${SLURM_CPUS_PER_TASK:-?}"
echo "    binary    : ${BIN}"
echo "    macro     : ${MAC}"
echo "    start     : $(date -Is)"

"${BIN}" "${MAC}"
rc=$?

echo "    end       : $(date -Is)  rc=${rc}"
exit ${rc}
