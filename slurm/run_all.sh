#!/usr/bin/env bash
# =============================================================================
# MuonScint SLURM driver -- the one script to run on the cluster.
#
# Generates FOUR macro sets and submits FOUR job arrays, then prints the job
# IDs and the collect path. It does NOT build (run `sbatch build.sbatch`
# once first) and does NOT run the local ROOT analysis.
#
#   Matrix scan        : centre/edge/corner, 4 configs x 4 reflectors
#                        -> slurm/muon/         -> run_matrix.sbatch (16 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/matrix/
#   Whole-slab scan    : full 250 x 250 mm^2 plate, same 16 pairs
#                        -> slurm/muon_whole/   -> run_whole.sbatch  (16 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/whole/
#   Contact scan       : bare plate, f=0..0.03, configs 1-3
#                        -> slurm/muon_contact/ -> run_contact.sbatch (21 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/contact/
#   Al backing scan    : Al foil, grease vs air, configs 1-3
#                        -> slurm/muon_al/      -> run_al.sbatch      (6 tasks)
#                        -> ${DATAROOT}/data_<VERSION>/al/
#
# The collection-efficiency map comes from the whole-slab run (offline, from
# the sipmHits tree). The matrix scan is for the centre/edge/corner yield and
# photon-budget comparisons.  The contact and Al arrays use the fast
# characterization template (flux map OFF) because their yield also comes from
# the sipmHits tree.
#
# WORKFLOW (typical cluster use):
#   cd /path/to/the/dir/that/should/hold/the/output
#   /abs/path/to/repo/slurm/run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS] \
#                                     [NEVENTS_CONTACT] [NEVENTS_AL]
#   Output lands in  $PWD/data_<VERSION>/{matrix,whole,contact,al}
#   (DATAROOT defaults to the CURRENT DIRECTORY; set DATAROOT=/abs/path to write
#    elsewhere. Logs and generated macros stay in slurm/.)
#   BIN=/abs/path/to/G4DCSmonitor  selects a specific binary version.
#
# Usage:
#   run_all.sh [NEVENTS_MATRIX] [NEVENTS_WHOLE] [THREADS] \
#              [NEVENTS_CONTACT] [NEVENTS_AL]
#
#   NEVENTS_MATRIX  : events per position for the centre/edge/corner scan
#                     (default 200)
#   NEVENTS_WHOLE   : events for the whole-slab run -- needs much higher
#                     statistics for a smooth map (default 10000)
#   THREADS         : Geant4 threads == SLURM --cpus-per-task (default 32)
#   NEVENTS_CONTACT : events per position for the f=0..0.03 scan (default 1000)
#   NEVENTS_AL      : events per position for the grease/air scan (default 1000)
#
# All event counts are independent knobs; e.g.
#   slurm/run_all.sh 500 20000 48 2000 2000
#
# Environment:
#   BIN=/abs/path/to/G4DCSmonitor   override the executable (passed through)
#   DATAROOT=/bigdata/.../muon      write all ROOT output here (default $PWD)
#                                   (logs and generated macros stay in slurm/)
#   VERSION=v2.2                    data-set version tag (default v2.2). Output
#                                   goes to ${DATAROOT}/data_${VERSION}/{matrix,whole,contact,al}
#   MATRIX_MEM / WHOLE_MEM          --mem per task (default 64G each; these
#                                   arrays use the flux-map templates)
#   CONTACT_MEM / AL_MEM            --mem per task (default 4G each; these
#                                   arrays use the fast characterization template)
#   CONTACT_FRACTIONS               default "0.000 0.005 0.010 0.015 0.020 0.025 0.030"
#   CONTACT_CFGS / AL_CFGS          default "1 2 3" (Config 4 has no black
#                                   holder; Al there is WLS-dominated)
#   AL_BACKINGS                     default "grease air"
#   CONTACT_THREADS / AL_THREADS    default THREADS
#   ARRAY_LIMIT     throttle all four arrays, e.g. ARRAY_LIMIT=4
#   THREADS is also honoured via ${SLURM_THREADS:-...} if set.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

NEVENTS_MATRIX="${1:-${NEVENTS_MATRIX:-200}}"
NEVENTS_WHOLE="${2:-${NEVENTS_WHOLE:-10000}}"
THREADS="${3:-${THREADS:-${SLURM_THREADS:-32}}}"
NEVENTS_CONTACT="${4:-${NEVENTS_CONTACT:-1000}}"
NEVENTS_AL="${5:-${NEVENTS_AL:-1000}}"

# Memory per array task (passed to sbatch as --mem, overriding the #SBATCH
# default).  Matrix/whole use the flux-map templates; contact/Al use the fast
# characterization template, so 4G is ample there.
MATRIX_MEM="${MATRIX_MEM:-64G}"
WHOLE_MEM="${WHOLE_MEM:-64G}"
CONTACT_MEM="${CONTACT_MEM:-4G}"
AL_MEM="${AL_MEM:-4G}"

CONTACT_THREADS="${CONTACT_THREADS:-${THREADS}}"
AL_THREADS="${AL_THREADS:-${THREADS}}"
CONTACT_FRACTIONS="${CONTACT_FRACTIONS:-0.000 0.005 0.010 0.015 0.020 0.025 0.030}"
CONTACT_CFGS="${CONTACT_CFGS:-1 2 3}"
AL_BACKINGS="${AL_BACKINGS:-grease air}"
AL_CFGS="${AL_CFGS:-1 2 3}"
export CONTACT_FRACTIONS CONTACT_CFGS AL_BACKINGS AL_CFGS
ARRAY_LIMIT="${ARRAY_LIMIT:-}"

# Where the ROOT files are written.  Defaults to the CURRENT DIRECTORY (run
# from the drive/dir you want the data in); set DATAROOT=/abs/path to override.
# All arrays inherit it (sbatch forwards the submitting environment).
DATAROOT="${DATAROOT:-${PWD}}"
DATAROOT="$(cd "${DATAROOT}" 2>/dev/null && pwd || printf '%s' "${DATAROOT}")"
export DATAROOT

# Version tag for the data directory tree.  Output is laid out as
#   ${DATAROOT}/data_<VERSION>/matrix/   (centre/edge/corner scan)
#   ${DATAROOT}/data_<VERSION>/whole/    (full-plate map)
#   ${DATAROOT}/data_<VERSION>/contact/  (bare f=0..0.03)
#   ${DATAROOT}/data_<VERSION>/al/       (Al grease vs air)
# The default v2.2 is the current data set (v2.1 = the previous, pre-WLS-flip
# data).  Export VERSION so all arrays and the generated macros agree.
VERSION="${VERSION:-v2.2}"
export VERSION
MATRIX_DATA="${DATAROOT}/data_${VERSION}/matrix"
WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"
CONTACT_DATA="${DATAROOT}/data_${VERSION}/contact"
AL_DATA="${DATAROOT}/data_${VERSION}/al"

# Resolve the executable once and export it so all arrays use the same binary
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
echo "  contact events: ${NEVENTS_CONTACT}  (threads ${CONTACT_THREADS}, mem ${CONTACT_MEM}; f='${CONTACT_FRACTIONS}'; cfg='${CONTACT_CFGS}')"
echo "  Al events     : ${NEVENTS_AL}  (threads ${AL_THREADS}, mem ${AL_MEM}; backings='${AL_BACKINGS}'; cfg='${AL_CFGS}')"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}/data_${VERSION}"
echo "  macros/logs   : ${HERE}"

mkdir -p "${HERE}/logs" "${MATRIX_DATA}" "${WHOLE_DATA}" "${CONTACT_DATA}" "${AL_DATA}"

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

echo
echo "--- generating contact-fraction macros ---"
FLUXMAP=off CONTACT_FRACTIONS="${CONTACT_FRACTIONS}" CONTACT_CFGS="${CONTACT_CFGS}" \
    "${HERE}/generate_contact_scan.sh" "${NEVENTS_CONTACT}" "${CONTACT_THREADS}" "${CONTACT_DATA}"

echo
echo "--- generating Al-backing macros ---"
FLUXMAP=off AL_BACKINGS="${AL_BACKINGS}" AL_CFGS="${AL_CFGS}" \
    "${HERE}/generate_al_backing.sh" "${NEVENTS_AL}" "${AL_THREADS}" "${AL_DATA}"

# Auto array bounds unless the user asked for a subset.
CONTACT_ARRAY="${CONTACT_ARRAY:-}"
AL_ARRAY="${AL_ARRAY:-}"
if [[ -z "${CONTACT_ARRAY}" ]]; then
    n=$(wc -l < "${HERE}/muon_contact/manifest.txt")
    CONTACT_ARRAY="0-$((n - 1))"
fi
if [[ -z "${AL_ARRAY}" ]]; then
    n=$(wc -l < "${HERE}/muon_al/manifest.txt")
    AL_ARRAY="0-$((n - 1))"
fi

# ---- 2) submit all four arrays ----------------------------------------------
matrix_array="0-15"
whole_array="0-15"
ct_array="${CONTACT_ARRAY}"
at_array="${AL_ARRAY}"
if [[ -n "${ARRAY_LIMIT}" ]]; then
    matrix_array="0-15%${ARRAY_LIMIT}"
    whole_array="0-15%${ARRAY_LIMIT}"
    ct_array="${CONTACT_ARRAY%%%*}%${ARRAY_LIMIT}"
    at_array="${AL_ARRAY%%%*}%${ARRAY_LIMIT}"
fi

echo
echo "--- submitting job arrays ---"
jid_matrix="$(sbatch --parsable --array="${matrix_array}" \
    --cpus-per-task="${THREADS}" --mem="${MATRIX_MEM}" \
    "${HERE}/run_matrix.sbatch")"
jid_whole="$(sbatch --parsable --array="${whole_array}" \
    --cpus-per-task="${THREADS}" --mem="${WHOLE_MEM}" \
    "${HERE}/run_whole.sbatch")"
jid_contact="$(sbatch --parsable --array="${ct_array}" \
    --cpus-per-task="${CONTACT_THREADS}" --mem="${CONTACT_MEM}" \
    "${HERE}/run_contact.sbatch")"
jid_al="$(sbatch --parsable --array="${at_array}" \
    --cpus-per-task="${AL_THREADS}" --mem="${AL_MEM}" \
    "${HERE}/run_al.sbatch")"

cat <<EOF

=== submitted ===
  matrix array     : job ${jid_matrix}   (output: ${MATRIX_DATA}/)
  whole-slab array : job ${jid_whole}   (output: ${WHOLE_DATA}/)
  contact array    : job ${jid_contact}   (output: ${CONTACT_DATA}/)
  Al-backing array : job ${jid_al}   (output: ${AL_DATA}/)

Watch:        squeue -u \$USER
Logs:         ${HERE}/logs/muonscan_*_*.out
              ${HERE}/logs/muonwhole_*_*.out
              ${HERE}/logs/muoncontact_*_*.out
              ${HERE}/logs/muonal_*_*.out
Collect:      DATAROOT=${DATAROOT} VERSION=${VERSION} ${HERE}/collect.sh
Analyse:      root -l -b -q 'macros/plot_collection_efficiency.C+("${WHOLE_DATA}")'
              root -l -b -q 'macros/plot_muon.C+("${MATRIX_DATA}")'
Report:       macros/build_report_figures.sh "${MATRIX_DATA}" "${WHOLE_DATA}"
EOF
