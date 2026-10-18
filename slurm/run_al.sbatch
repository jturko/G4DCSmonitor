#!/usr/bin/env bash
#SBATCH --job-name=muonal
#SBATCH --array=0-5
#SBATCH --cpus-per-task=32
#SBATCH --mem=4G
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --partition=low
#SBATCH --account=low
#SBATCH --time=01:00:00
#SBATCH --output=logs/muonal_%A_%a.out
#SBATCH --error=logs/muonal_%A_%a.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =============================================================================
# Aluminium-foil backing comparison: grease (n=1.46) vs air (n=1.0003).
#
# Default matrix: 3 configs x 2 backings = 6 tasks (0-5), each running
# centre/edge/corner back-to-back.  Configs/backings are controlled by the
# generator (AL_CFGS / AL_BACKINGS); the driver overrides --array from the
# generated manifest, so use the driver rather than editing this line.
#
# Flux map is OFF in the characterization template by default (yield comes from
# the sipmHits tree).  If you regenerate with FLUXMAP=on, raise --mem/--time.
#
# --cpus-per-task MUST equal /run/numberOfThreads in the generated macros.
# =============================================================================

set -uo pipefail

# ---------------------------------------------------------------------------
# USER CONFIG -- same module stack as build.sbatch / run_matrix_fast.sbatch
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
MACDIR="${HERE}/muon_al"
DATAROOT="${DATAROOT:-${HERE}}"
VERSION="${VERSION:-v2.2}"
DATADIR="${DATAROOT}/data_${VERSION}/al"
LOGDIR="${HERE}/logs"
mkdir -p "${DATADIR}" "${LOGDIR}"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-32}"

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

IDX="${SLURM_ARRAY_TASK_ID:-0}"
MANIFEST="${MACDIR}/manifest.txt"
if [[ ! -f "${MANIFEST}" ]]; then
    echo "ERROR: manifest missing: ${MANIFEST}" >&2
    echo "       run generate_al_backing.sh (or a driver) first" >&2
    exit 1
fi
BASE="$(sed -n "$(( IDX + 1 ))p" "${MANIFEST}")"
if [[ -z "${BASE}" ]]; then
    echo "ERROR: empty manifest entry for array id ${IDX}" >&2
    exit 1
fi

MAC="${MACDIR}/${BASE}"
if [[ ! -f "${MAC}" ]]; then
    echo "ERROR: macro missing: ${MAC}" >&2
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
