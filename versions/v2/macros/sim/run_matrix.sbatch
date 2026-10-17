#!/usr/bin/env bash
#SBATCH --job-name=muonscan
#SBATCH --array=0-15
#SBATCH --cpus-per-task=32
#SBATCH --mem=64G
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --time=2-00:00:00
#SBATCH --output=logs/%x_%A_%a.out
#SBATCH --error=logs/%x_%A_%a.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =============================================================================
# MuonScint scan matrix -- one SLURM array task per (config, reflector) macro.
#
#   16 tasks = 4 configs x 4 reflectors, each running centre/edge/corner
#   back-to-back in a single Geant4 process (3 runs, 1 geometry build).
#
# Each task uses @cpus-per-task@ Geant4 threads.  This MUST match the
# /run/numberOfThreads baked into the macro by generate_muon_matrix.sh
# (both default to 32).  If you change one, change the other.
#
# Adapt the "USER CONFIG" block below to your cluster (modules / env paths),
# then from this slurm/ directory run:
#
#   ./generate_muon_matrix.sh 200 32          # write slurm/muon/*.mac + manifest
#   sbatch run_matrix.sbatch                  # submit the 16-task array
#
# Output ROOT files land in ${DATAROOT}/data_<VERSION>/matrix/ (VERSION defaults
# to v2.2, the current data set), logs in slurm/logs/.
# =============================================================================

set -uo pipefail

# ---------------------------------------------------------------------------
# USER CONFIG -- adapt to your SLURM setup
# ---------------------------------------------------------------------------
# Option A: your own single modulefile that purges + loads everything, e.g.
#   module load hzdr-sim/1.0
#
# Option B: the explicit list (kept in sync with the modulefile you showed):
module purge
module load gcc/15.2.0
module load root/6.32.14
module load cmake/4.0.3
module load xerces/3.3.0
module load qtbase/6.10.1
module load geant4/11.4.0
# ---------------------------------------------------------------------------

# Run from the submit directory regardless of where sbatch was invoked.
HERE="${SLURM_SUBMIT_DIR:-$(pwd)}"
cd "${HERE}"
MACDIR="${HERE}/muon"
# ROOT output goes under DATAROOT (defaults to this slurm/ dir); the macros are
# generated with absolute output paths, so this is only used for the file check.
DATAROOT="${DATAROOT:-${HERE}}"
VERSION="${VERSION:-v2.2}"
DATADIR="${DATAROOT}/data_${VERSION}/matrix"
LOGDIR="${HERE}/logs"
mkdir -p "${DATADIR}" "${LOGDIR}"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-32}"

# Path to the G4DCSmonitor executable.  Override with
#   BIN=/abs/path sbatch run_matrix.sbatch   (or export BIN=... before run_all.sh)
# Defaults to the pre-built HZDR binary, then to the repo build next to slurm/.
if [[ -z "${BIN:-}" ]]; then
    if [[ -x /home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ]]; then
        BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
    else
        BIN="${HERE}/../build/G4DCSmonitor"
    fi
fi

if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: executable not found or not runnable: ${BIN}" >&2
    echo "       edit BIN in $0 or export BIN=/abs/path/to/G4DCSmonitor" >&2
    exit 1
fi

# Manifest emitted by generate_muon_matrix.sh: one macro (basename) per line,
# in deterministic array order.  Falls back to the built-in mapping below.
MANIFEST="${MACDIR}/manifest.txt"
if [[ -f "${MANIFEST}" ]]; then
    BASE="$(sed -n "$(( ${SLURM_ARRAY_TASK_ID:-0} + 1 ))p" "${MANIFEST}")"
else
    MAP=(scan_c1_r0 scan_c1_r1 scan_c1_r2 scan_c1_r3
         scan_c2_r0 scan_c2_r1 scan_c2_r2 scan_c2_r3
         scan_c3_r0 scan_c3_r1 scan_c3_r2 scan_c3_r3
         scan_c4_r0 scan_c4_r1 scan_c4_r2 scan_c4_r3)
    IDX="${SLURM_ARRAY_TASK_ID:-0}"
    BASE="${MAP[$IDX]}"
fi

if [[ -z "${BASE}" ]]; then
    echo "ERROR: empty manifest entry for array id ${SLURM_ARRAY_TASK_ID:-0}" >&2
    exit 1
fi

MAC="${MACDIR}/${BASE}"
if [[ ! -f "${MAC}" ]]; then
    echo "ERROR: macro missing: ${MAC}  (run ./generate_muon_matrix.sh first)" >&2
    exit 1
fi

echo "=== task ${SLURM_ARRAY_TASK_ID:-0} : ${BASE} ==="
echo "    host      : $(hostname)"
echo "    cpus/task : ${SLURM_CPUS_PER_TASK:-?}"
echo "    binary    : ${BIN}"
echo "    macro     : ${MAC}"
echo "    start     : $(date -Is)"

# Output filenames are set inside the macro (/analysis/setFileName), so no
# fileNum argument is passed.  Geant4 threads come from the macro's
# /run/numberOfThreads line.
"${BIN}" "${MAC}"
rc=$?

echo "    end       : $(date -Is)  rc=${rc}"
exit ${rc}
