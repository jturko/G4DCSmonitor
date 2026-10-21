#!/usr/bin/env bash
#SBATCH --job-name=muonwhole-opt
#SBATCH --array=0-9
#SBATCH --cpus-per-task=32
#SBATCH --mem=4G
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --partition=low
#SBATCH --account=low
#SBATCH --time=02:00:00
#SBATCH --output=logs/muonwholeopt_%A_%a.out
#SBATCH --error=logs/muonwholeopt_%A_%a.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =============================================================================
# MuonScint whole-slab irradiation -- Phase 2 optimization campaign.
#
#   10 candidate designs from HANDOFF.md §6.2 (9 designs + one coupling
#   control), full 250 x 250 mm^2 plate.
#   Routed to `low` by default; --cpus-per-task MUST equal the
#   /run/numberOfThreads baked into the macros by generate_opt_whole.sh.
#
#   Output lands in /bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole
#   (override with DATAROOT).
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
MACDIR="${HERE}/muon_whole_opt"
DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/MuonScinti}"
VERSION="${VERSION:-v7_opt}"
DATADIR="${DATAROOT}/data_${VERSION}/whole"
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
    echo "ERROR: manifest missing: ${MANIFEST}  (run generate_opt_whole.sh first)" >&2
    exit 1
fi
if [[ -z "${BASE}" ]]; then
    echo "ERROR: empty manifest entry for array id ${IDX}" >&2
    exit 1
fi

MAC="${MACDIR}/${BASE}"
if [[ ! -f "${MAC}" ]]; then
    echo "ERROR: macro missing: ${MAC}  (run generate_opt_whole.sh first)" >&2
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
