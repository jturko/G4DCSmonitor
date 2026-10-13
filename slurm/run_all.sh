#!/usr/bin/env bash
# =============================================================================
# MuonScint SLURM driver -- the one script to run on the cluster.
#
# Generates BOTH macro sets and submits BOTH job arrays, then prints the job
# IDs and the collect path. It does NOT build (run `sbatch build.sbatch`
# once first) and does NOT run the local ROOT analysis.
#
#   Matrix scan        : centre/edge/corner, 4 configs x 4 reflectors
#                        -> slurm/muon/       -> run_matrix.sbatch (16 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/matrix/
#   Whole-slab scan    : full 250 x 250 mm^2 plate, same 16 pairs
#                        -> slurm/muon_whole/ -> run_whole.sbatch  (16 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/whole/
#
# The collection-efficiency map comes from the whole-slab run (offline, from
# the sipmHits tree). The matrix scan is for the centre/edge/corner yield and
# photon-budget comparisons.
#
# WORKFLOW (typical cluster use):
#   cd /path/to/the/dir/that/should/hold/the/output
#   /abs/path/to/repo/slurm/run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS]
#   Output lands in  $PWD/data_<VERSION>/{matrix,whole}
#   (DATAROOT defaults to the CURRENT DIRECTORY; set DATAROOT=/abs/path to write
#    elsewhere. Logs and generated macros stay in slurm/.)
#   BIN=/abs/path/to/G4DCSmonitor  selects a specific binary version.
#
# Usage:
#   run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS]
#
#   NEVENTS_MATRIX : events per position for the centre/edge/corner scan
#                    (default 200)
#   NEVENTS_WHOLE  : events for the whole-slab run -- needs much higher
#                    statistics for a smooth map (default 10000)
#   THREADS        : Geant4 threads == SLURM --cpus-per-task (default 32)
#
# Both event counts are independent knobs; e.g.
#   slurm/run_all.sh 500 20000 48
#
# Environment:
#   BIN=/abs/path/to/G4DCSmonitor   override the executable (passed through)
#   DATAROOT=/bigdata/.../muon      write all ROOT output here (default $PWD)
#                                   (logs and generated macros stay in slurm/)
#   VERSION=v2.2                    data-set version tag (default v2.2). Output
#                                   goes to ${DATAROOT}/data_${VERSION}/{matrix,whole}
#   MATRIX_MEM / WHOLE_MEM          --mem per task (default 64G each; these
#                                   arrays use the flux-map templates)
#   THREADS is also honoured via ${SLURM_THREADS:-...} if set.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

NEVENTS_MATRIX="${1:-${NEVENTS_MATRIX:-200}}"
NEVENTS_WHOLE="${2:-${NEVENTS_WHOLE:-10000}}"
THREADS="${3:-${THREADS:-${SLURM_THREADS:-32}}}"

# Memory per array task (passed to sbatch as --mem, overriding the #SBATCH
# default).  These arrays use the flux-map templates, so keep them generous.
MATRIX_MEM="${MATRIX_MEM:-64G}"
WHOLE_MEM="${WHOLE_MEM:-64G}"

# Where the ROOT files are written.  Defaults to the CURRENT DIRECTORY (run
# from the drive/dir you want the data in); set DATAROOT=/abs/path to override.
# Both arrays inherit it (sbatch forwards the submitting environment).
DATAROOT="${DATAROOT:-${PWD}}"
DATAROOT="$(cd "${DATAROOT}" 2>/dev/null && pwd || printf '%s' "${DATAROOT}")"
export DATAROOT

# Version tag for the data directory tree.  Output is laid out as
#   ${DATAROOT}/data_<VERSION>/matrix/   (centre/edge/corner scan)
#   ${DATAROOT}/data_<VERSION>/whole/    (full-plate map)
# The default v2.2 is the current data set (v2.1 = the previous, pre-WLS-flip
# data).  Export VERSION so both arrays and the generated macros agree.
VERSION="${VERSION:-v2.2}"
export VERSION
MATRIX_DATA="${DATAROOT}/data_${VERSION}/matrix"
WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"

# Resolve the executable once and export it so both arrays use the same binary
# (sbatch forwards the submitting environment).  Override with BIN=/abs/path.
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
    echo "       set BIN=/abs/path/to/G4DCSmonitor and re-run." >&2
    exit 1
fi

# Guard against a shadowing 'G4DCSmonitor' on PATH.  We ALWAYS launch the
# absolute ${BIN}, so the PATH copy is never used -- this is just a warning.
PATH_BIN="$(command -v G4DCSmonitor 2>/dev/null || true)"
if [[ -n "${PATH_BIN}" && "${PATH_BIN}" != "${BIN}" ]]; then
    echo "note: PATH has a different G4DCSmonitor (${PATH_BIN}); using BIN=${BIN}"
fi

echo "=== MuonScint SLURM driver ==="
echo "  matrix events : ${NEVENTS_MATRIX}  (threads ${THREADS}, mem ${MATRIX_MEM})"
echo "  whole  events : ${NEVENTS_WHOLE}  (threads ${THREADS}, mem ${WHOLE_MEM})"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}/data_${VERSION}"
echo "  macros/logs   : ${HERE}"

mkdir -p "${HERE}/logs" "${MATRIX_DATA}" "${WHOLE_DATA}"

# Submit from this directory so the relative #SBATCH --output=logs/... paths in
# the .sbatch files resolve into slurm/logs/ regardless of the caller's CWD.
cd "${HERE}"

# ---- 1) generate macros ------------------------------------------------------
# Datadirs are passed as ABSOLUTE paths so the jobs write to DATAROOT no matter
# what directory geant4 runs from.
echo
echo "--- generating matrix macros ---"
"${HERE}/generate_muon_matrix.sh" "${NEVENTS_MATRIX}" "${THREADS}" "${MATRIX_DATA}"

echo
echo "--- generating whole-slab macros ---"
"${HERE}/generate_whole_slab.sh" "${NEVENTS_WHOLE}" "${THREADS}" "${WHOLE_DATA}"

# ---- 2) submit both arrays ---------------------------------------------------
echo
echo "--- submitting job arrays ---"
jid_matrix="$(sbatch --parsable \
    --cpus-per-task="${THREADS}" --mem="${MATRIX_MEM}" \
    "${HERE}/run_matrix.sbatch")"
jid_whole="$(sbatch --parsable \
    --cpus-per-task="${THREADS}" --mem="${WHOLE_MEM}" \
    "${HERE}/run_whole.sbatch")"

cat <<EOF

=== submitted ===
  matrix array     : job ${jid_matrix}   (output: ${MATRIX_DATA}/)
  whole-slab array : job ${jid_whole}   (output: ${WHOLE_DATA}/)

Watch:        squeue -u \$USER
Logs:         ${HERE}/logs/muonscan_*_*.out
              ${HERE}/logs/muonwhole_*_*.out
Collect:      DATAROOT=${DATAROOT} VERSION=${VERSION} ${HERE}/collect.sh
Analyse:      root -l -b -q 'macros/plot_collection_efficiency.C+("${WHOLE_DATA}")'
              root -l -b -q 'macros/plot_muon.C+("${MATRIX_DATA}")'
Report:       macros/build_report_figures.sh "${MATRIX_DATA}" "${WHOLE_DATA}"
EOF
