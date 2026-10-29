#!/usr/bin/env bash
#SBATCH --job-name=p15-build
#SBATCH --cpus-per-task=16
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --mem=64G
#SBATCH --time=01:00:00
#SBATCH --output=logs/%x_%j.out
#SBATCH --error=logs/%x_%j.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =============================================================================
# One-off build of the main-tree G4DCSmonitor on the cluster (short job).
#
# From the slurm/ directory:   sbatch build.sbatch
#
# Produces ../build/G4DCSmonitor, which run_p15.sh / p15_job.sbatch use by
# default (BIN=... overrides).  Adapt the "USER CONFIG" block to the module
# stack your cluster provides.
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# USER CONFIG -- adapt to your SLURM setup
# ---------------------------------------------------------------------------
# Option A: your own single modulefile that purges + loads everything, e.g.
#   module load hzdr-sim/1.0
#
# Option B: the explicit list (kept in sync with p15_job.sbatch):
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
REPO="$(cd "${HERE}/.." && pwd)"
mkdir -p "${HERE}/logs"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-16}"

echo "=== building ${REPO} on $(hostname) ==="
echo "    gcc:    $(command -v gcc)  ($(gcc --version | head -1))"
echo "    cmake:  $(command -v cmake)"
echo "    Geant4: ${Geant4_DIR:-<module default>}"
echo "    ROOT:   ${ROOTSYS:-<module default>}"

# --fresh: a build/ directory copied over from another machine pins the old
# CMAKE_HOME_DIRECTORY and the login-node Geant4/ROOT paths in CMakeCache.txt,
# which silently breaks the configure on the cluster.
cmake --fresh -S "${REPO}" -B "${REPO}/build"
cmake --build "${REPO}/build" -j "${SLURM_CPUS_PER_TASK:-16}"

echo "=== built: ${REPO}/build/G4DCSmonitor ==="
ls -la "${REPO}/build/G4DCSmonitor"
