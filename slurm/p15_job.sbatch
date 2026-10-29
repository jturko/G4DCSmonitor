#!/usr/bin/env bash
#SBATCH --job-name=p15
#SBATCH --array=0-4619
#SBATCH --ntasks=1
#SBATCH --nodes=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=64G
#SBATCH --time=06:00:00
#SBATCH --output=logs/p15_%A_%a.out
#SBATCH --error=logs/p15_%A_%a.err
#SBATCH --mail-user=j.turko@hzdr.de
#SBATCH --mail-type=FAIL
# =====================================================================
# One p1.5 background-sector production task = one config
# (baseFuel x source x cask x hexant).
#
# Normally submitted by run_p15.sh, which overrides the header defaults
# (--array range, absolute --output/--error inside the data tree,
# env knobs).  For a manual submission cd to the campaign data root and
# export REPO (repo root containing build/G4DCSmonitor) plus the env
# knobs documented in slurm/README.md first.
#
# The macro contains the geometry, the p1 file and all chunk blocks;
# the binary must be started with --mode background-sector so that the
# sector-scoring parallel world is registered before the macro runs.
# =====================================================================

set -uo pipefail

HERE="${SLURM_SUBMIT_DIR:-$(pwd)}"
cd "${HERE}"
mkdir -p "${HERE}/logs"

# ---------------------------------------------------------------------
# Module stack (USER CONFIG -- keep in sync with build.sbatch)
# ---------------------------------------------------------------------
module purge
module load gcc/15.2.0
module load root/6.32.14
module load cmake/4.0.3
module load xerces/3.3.0
module load qtbase/6.10.1
module load geant4/11.4.0

DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/SectorBkgd}"
VERSION="${VERSION:-v1}"
MACDIR="${MACDIR:-${DATAROOT}/macros_${VERSION}/p15}"
DATADIR="${DATAROOT}/data_${VERSION}"
LOGDIR="${DATADIR}/logs"
mkdir -p "${LOGDIR}"

export OMP_NUM_THREADS="${SLURM_CPUS_PER_TASK:-32}"

# ---------------------------------------------------------------------
# Binary: BIN env (absolute) > ${REPO}/build/G4DCSmonitor.  Never $PATH.
# ---------------------------------------------------------------------
if [ -z "${BIN:-}" ] && [ -z "${REPO:-}" ]; then
    echo "FATAL: neither BIN nor REPO is exported (submit via run_p15.sh," >&2
    echo "       or export REPO=/path/to/G4DCSmonitor first)." >&2
    exit 2
fi
BIN="${BIN:-${REPO}/build/G4DCSmonitor}"
if command -v G4DCSmonitor >/dev/null 2>&1; then
    echo "WARNING: a G4DCSmonitor is on PATH; using ${BIN} anyway."
fi
if [ ! -x "${BIN}" ]; then
    echo "FATAL: ${BIN} is not executable -- run slurm/build.sbatch on the HPC first." >&2
    exit 2
fi

# Helper scripts live next to this sbatch in the repo's slurm/ directory.
SLURM_DIR="${SLURM_DIR:-${REPO}/slurm}"

# ---------------------------------------------------------------------
# Task = manifest line (array index i -> line i+1)
#   <macro basename>|<config dir>|<merged stem>|<nchunks>
# ---------------------------------------------------------------------
IDX="${SLURM_ARRAY_TASK_ID:-0}"
MANIFEST="${MACDIR}/manifest.txt"
if [ ! -f "${MANIFEST}" ]; then
    echo "FATAL: no manifest at ${MANIFEST} -- run slurm/run_p15.sh (or generate_p15_macros.sh)" >&2
    exit 2
fi
LINE="$(sed -n "$((IDX + 1))p" "${MANIFEST}")"
if [ -z "${LINE}" ]; then
    echo "FATAL: array index ${IDX} beyond ${MANIFEST} ($(wc -l < "${MANIFEST}") lines)" >&2
    exit 2
fi
IFS='|' read -r MAC_BASE CFGDIR STEM NCHUNKS <<< "${LINE}"
MAC="${MACDIR}/${MAC_BASE}"
if [ ! -f "${MAC}" ]; then
    echo "FATAL: missing macro ${MAC} -- regenerate with slurm/run_p15.sh" >&2
    exit 2
fi

echo "[p15] task ${IDX} of manifest: ${MAC_BASE}"
echo "[p15] config dir: ${CFGDIR}"
echo "[p15] host $(hostname), start $(date -Iseconds)"
df -B1G --output=avail "${DATAROOT}" | tail -1 | sed 's/^/[p15] disk avail (GiB): /'

"${BIN}" --mode background-sector "${MAC}"
RC=$?
echo "[p15] G4DCSmonitor rc=${RC}, end $(date -Iseconds)"
if [ "${RC}" -ne 0 ]; then
    exit "${RC}"
fi

# ---------------------------------------------------------------------
# In-task merge: hadd the chunks + consolidate sectorMeta (T_gen = sum).
# Chunk files are deleted only if KEEP_CHUNKS=0 AND the consolidation
# verified; on any merge failure the task exits 1 and the chunks stay.
# ---------------------------------------------------------------------
if [ "${MERGE:-1}" = "1" ]; then
    if [ ! -f "${SLURM_DIR}/merge_config.sh" ]; then
        echo "FATAL: ${SLURM_DIR}/merge_config.sh not found -- set SLURM_DIR to the repo's slurm/ dir." >&2
        exit 2
    fi
    "${SLURM_DIR}/merge_config.sh" "${CFGDIR}" "${STEM}" "${NCHUNKS}" "${KEEP_CHUNKS:-1}"
    MRG=$?
    echo "[p15] merge rc=${MRG}"
    if [ "${MRG}" -ne 0 ]; then
        exit "${MRG}"
    fi
fi

echo "[p15] task ${IDX} done $(date -Iseconds)"
