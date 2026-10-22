#!/usr/bin/env bash
# =============================================================================
# MuonScint corner-cut design driver (preset 12).
#
# Preset 12 is the Config-2 variant with the four slab corners truncated at
# 45 deg (12 mm cut leg) and one 4 x 4 mm SiPM on each chamfered face.  This
# driver runs only that design:
#
#   1. matrix  : cfg 12 x reflectors 0-3, centre/edge/corner in one process
#                -> ${DATAROOT}/data_${VERSION}/matrix/muon_out_c12_r<R>_<pos>.root
#   2. whole   : cfg 12 x reflectors 0-3, full 250 x 250 mm^2 plate
#                -> ${DATAROOT}/data_${VERSION}/whole/muon_whole_out_c12_r<R>.root
#   3. opt     : the c2_cornercut whole-plate entry of the v7_opt campaign
#                -> ${DATAROOT}/data_${OPT_VERSION}/whole/
#                   muon_whole_out_c12_r3_c2_cornercut.root
#
# Only the c2_cornercut array index of the v7_opt manifest is submitted, so the
# ten existing designs are never re-run.
#
# Usage:
#   slurm/run_cornercut.sh
#
# Environment (all optional):
#   DRY_RUN=1              generate everything and PRINT the sbatch commands
#                          without submitting (safe on a login node).
#   DATAROOT               default /bigdata/DCS-Monitor-II/turko46/MuonScinti
#   VERSION                default v7      (matrix + whole data tag)
#   OPT_VERSION            default v7_opt  (optimization whole-plate tag)
#   MATRIX_EVENTS          default 1000    events per beam position
#   WHOLE_EVENTS           default 10000   events for the v7 full-plate runs
#   OPT_EVENTS             default 25000   events for the v7_opt design run (table row)
#   MATRIX_THREADS         default 32      must match --cpus-per-task
#   WHOLE_THREADS          default 32      must match --cpus-per-task
#   MATRIX_PARTITION/ACCOUNT/TIME/MEM  default low / low / 01:00:00 / 4G
#   WHOLE_PARTITION/ACCOUNT/TIME/MEM   default low / low / 02:00:00 / 4G
#   BIN                    absolute path to a G4DCSmonitor built on the CLUSTER
#                          module stack (default: pre-built HZDR path, else
#                          <repo>/build/G4DCSmonitor)
#   SKIP_MATRIX=1 / SKIP_WHOLE=1 / SKIP_OPT=1   skip an individual stage
#
# Typical HPC use:
#   export BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
#   slurm/run_cornercut.sh
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"

DRY_RUN="${DRY_RUN:-0}"

# ---- nominal surface calibration (same values as the other drivers) ---------
ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.045}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

# ---- campaign / resource knobs ----------------------------------------------
DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/MuonScinti}"
VERSION="${VERSION:-v7}"
OPT_VERSION="${OPT_VERSION:-v7_opt}"

MATRIX_EVENTS="${MATRIX_EVENTS:-1000}"
WHOLE_EVENTS="${WHOLE_EVENTS:-10000}"
OPT_EVENTS="${OPT_EVENTS:-25000}"
MATRIX_THREADS="${MATRIX_THREADS:-32}"
WHOLE_THREADS="${WHOLE_THREADS:-32}"

MATRIX_PARTITION="${MATRIX_PARTITION:-low}"
MATRIX_ACCOUNT="${MATRIX_ACCOUNT:-low}"
MATRIX_TIME="${MATRIX_TIME:-01:00:00}"
MATRIX_MEM="${MATRIX_MEM:-4G}"

WHOLE_PARTITION="${WHOLE_PARTITION:-low}"
WHOLE_ACCOUNT="${WHOLE_ACCOUNT:-low}"
WHOLE_TIME="${WHOLE_TIME:-02:00:00}"
WHOLE_MEM="${WHOLE_MEM:-4G}"

SKIP_MATRIX="${SKIP_MATRIX:-0}"
SKIP_WHOLE="${SKIP_WHOLE:-0}"
SKIP_OPT="${SKIP_OPT:-0}"

# ---- the design under study -------------------------------------------------
CFG=12
PRESET=12
TAG=c2_cornercut
OPT_REFL=3

MATRIX_OUT="${HERE}/muon_fast_c12"
WHOLE_OUT="${HERE}/muon_whole_c12"
OPT_OUT="${HERE}/muon_whole_opt"

MATRIX_DATA="${DATAROOT}/data_${VERSION}/matrix"
WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"
OPT_DATA="${DATAROOT}/data_${OPT_VERSION}/whole"

MATRIX_TEMPLATE="${HERE}/muon_scint_template_fast.mac"
WHOLE_TEMPLATE="${HERE}/whole_slab_template.mac"

# ---- executable -------------------------------------------------------------
# BIN env > pre-built HZDR binary > repo build.  On the HPC BIN MUST point at a
# binary built against the cluster module stack (see slurm/build.sbatch).
if [[ -z "${BIN:-}" ]]; then
    if [[ -x /home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ]]; then
        BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
    else
        BIN="${ROOTDIR}/build/G4DCSmonitor"
    fi
fi
export BIN
if [[ ! -x "${BIN}" ]]; then
    if [[ "${DRY_RUN}" == "1" ]]; then
        echo "WARNING: BIN is not executable here: ${BIN}" >&2
        echo "         (fine for DRY_RUN; on the HPC set BIN to a cluster-built binary)" >&2
    else
        echo "ERROR: executable not found or not runnable: ${BIN}" >&2
        echo "       export BIN=/abs/path/to/G4DCSmonitor (built on the cluster)" >&2
        exit 1
    fi
fi

# ---- resolve helpers --------------------------------------------------------
print_cmd() {
    printf '  [dry-run]'
    printf ' %q' "$@"
    printf '\n'
}

# Run the given command via sbatch, or print it under DRY_RUN.  Sets JID.
submit_cmd() {
    if [[ "${DRY_RUN}" == "1" ]]; then
        print_cmd "$@"
        JID="<dry-run>"
    else
        JID="$("$@")"
        if [[ -z "${JID}" ]]; then
            echo "ERROR: sbatch returned no job id for: $*" >&2
            exit 1
        fi
    fi
}

for f in "${MATRIX_TEMPLATE}" "${WHOLE_TEMPLATE}" "${HERE}/generate_opt_whole.sh"; do
    [[ -f "${f}" ]] || { echo "ERROR: required file missing: ${f}" >&2; exit 1; }
done

echo "=== MuonScint corner-cut campaign (preset ${PRESET}, tag ${TAG}) ==="
echo "  mode          : $([[ "${DRY_RUN}" == "1" ]] && echo 'DRY-RUN (nothing submitted)' || echo 'SUBMIT')"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}"
echo "  matrix data   : ${MATRIX_DATA}  (${MATRIX_EVENTS} ev/position, ${MATRIX_THREADS} threads)"
echo "  whole data    : ${WHOLE_DATA}   (${WHOLE_EVENTS} ev, ${WHOLE_THREADS} threads)"
echo "  opt data      : ${OPT_DATA}     (${OPT_EVENTS} ev, ${WHOLE_THREADS} threads)"

mkdir -p "${HERE}/logs"

# The .sbatch scripts resolve their macros via ${SLURM_SUBMIT_DIR}/<MACDIR> and
# the drivers pass an absolute MACDIR, so submit from this slurm/ directory.
cd "${HERE}"

jid_matrix="skipped"
jid_whole="skipped"
jid_opt="skipped"
opt_idx="n/a"

# =============================================================================
# 1. matrix: cfg 12 x reflectors 0-3 (4 macros, array 0-3)
# =============================================================================
if [[ "${SKIP_MATRIX}" != "1" ]]; then
    echo
    echo "--- [1/3] generating matrix macros -> ${MATRIX_OUT}/ ---"
    mkdir -p "${MATRIX_OUT}"
    : > "${MATRIX_OUT}/manifest.txt"
    i=0
    for refl in 0 1 2 3; do
        out="scan_c${CFG}_r${refl}.mac"
        sed -e "s/@CONFIG@/${CFG}/g"    \
            -e "s/@PRESET@/${PRESET}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${MATRIX_EVENTS}/g" \
            -e "s/@THREADS@/${MATRIX_THREADS}/g" \
            -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
            -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
            -e "s#@DATADIR@#${MATRIX_DATA}#g" \
            "${MATRIX_TEMPLATE}" > "${MATRIX_OUT}/${out}"
        printf '%s\n' "${out}" >> "${MATRIX_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done

    cmd=(env "MACDIR=${MATRIX_OUT}" "DATAROOT=${DATAROOT}" "VERSION=${VERSION}" \
         sbatch --parsable \
         -p "${MATRIX_PARTITION}" -A "${MATRIX_ACCOUNT}" \
         -t "${MATRIX_TIME}" --array=0-3 \
         --cpus-per-task="${MATRIX_THREADS}" --mem="${MATRIX_MEM}" \
         "${HERE}/run_matrix_fast.sbatch")
    submit_cmd "${cmd[@]}"
    jid_matrix="${JID}"
    echo "  matrix array : job ${jid_matrix}"
else
    echo
    echo "--- [1/3] matrix skipped (SKIP_MATRIX=1) ---"
fi

# =============================================================================
# 2. whole-slab: cfg 12 x reflectors 0-3 (4 macros, array 0-3)
# =============================================================================
if [[ "${SKIP_WHOLE}" != "1" ]]; then
    echo
    echo "--- [2/3] generating whole-slab macros -> ${WHOLE_OUT}/ ---"
    mkdir -p "${WHOLE_OUT}"
    : > "${WHOLE_OUT}/manifest.txt"
    i=0
    for refl in 0 1 2 3; do
        out="whole_c${CFG}_r${refl}.mac"
        sed -e "s/@CONFIG@/${CFG}/g"    \
            -e "s/@PRESET@/${PRESET}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${WHOLE_EVENTS}/g" \
            -e "s/@THREADS@/${WHOLE_THREADS}/g" \
            -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
            -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
            -e "s#@DATADIR@#${WHOLE_DATA}#g" \
            "${WHOLE_TEMPLATE}" > "${WHOLE_OUT}/${out}"
        printf '%s\n' "${out}" >> "${WHOLE_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done

    cmd=(env "MACDIR=${WHOLE_OUT}" "DATAROOT=${DATAROOT}" "VERSION=${VERSION}" \
         sbatch --parsable \
         -p "${WHOLE_PARTITION}" -A "${WHOLE_ACCOUNT}" \
         -t "${WHOLE_TIME}" --array=0-3 \
         --cpus-per-task="${WHOLE_THREADS}" --mem="${WHOLE_MEM}" \
         "${HERE}/run_whole_fast.sbatch")
    submit_cmd "${cmd[@]}"
    jid_whole="${JID}"
    echo "  whole array  : job ${jid_whole}"
else
    echo
    echo "--- [2/3] whole-slab skipped (SKIP_WHOLE=1) ---"
fi

# =============================================================================
# 3. optimization campaign: submit ONLY the c2_cornercut manifest entry
# =============================================================================
if [[ "${SKIP_OPT}" != "1" ]]; then
    echo
    echo "--- [3/3] generating optimization macros -> ${OPT_OUT}/ ---"
    # Regenerates the full v7_opt macro table, but only one array index is
    # submitted below; the other ten designs are NOT re-run.
    "${HERE}/generate_opt_whole.sh" "${OPT_EVENTS}" "${WHOLE_THREADS}" "${OPT_DATA}"

    opt_base="muon_whole_opt_${TAG}.mac"
    opt_line="$(awk -v b="${opt_base}" '$0 == b { print NR; exit }' "${OPT_OUT}/manifest.txt")"
    if [[ -z "${opt_line}" ]]; then
        echo "ERROR: ${TAG} not found in ${OPT_OUT}/manifest.txt" >&2
        exit 1
    fi
    opt_idx=$((opt_line - 1))
    if [[ ! -f "${OPT_OUT}/${opt_base}" ]]; then
        echo "ERROR: expected macro missing: ${OPT_OUT}/${opt_base}" >&2
        exit 1
    fi
    echo "  ${TAG} -> manifest line ${opt_line} (array index ${opt_idx})"

    cmd=(env "MACDIR=${OPT_OUT}" "DATAROOT=${DATAROOT}" "VERSION=${OPT_VERSION}" \
         sbatch --parsable \
         -p "${WHOLE_PARTITION}" -A "${WHOLE_ACCOUNT}" \
         -t "${WHOLE_TIME}" --array="${opt_idx}" \
         --cpus-per-task="${WHOLE_THREADS}" --mem="${WHOLE_MEM}" \
         "${HERE}/run_opt_whole.sbatch")
    submit_cmd "${cmd[@]}"
    jid_opt="${JID}"
    echo "  opt array    : job ${jid_opt} (index ${opt_idx} only)"
else
    echo
    echo "--- [3/3] optimization skipped (SKIP_OPT=1) ---"
fi

# =============================================================================
# Summary
# =============================================================================
cat <<EOF

=== corner-cut campaign summary ===
  matrix output : ${MATRIX_DATA}
                  muon_out_c12_r{0,1,2,3}_{centre,edge,corner}.root  (12 files)
                  job: ${jid_matrix}
  whole output  : ${WHOLE_DATA}
                  muon_whole_out_c12_r{0,1,2,3}.root  (4 files)
                  job: ${jid_whole}
  opt output    : ${OPT_DATA}
                  muon_whole_out_c${CFG}_r${OPT_REFL}_${TAG}.root  (1 file)
                  job: ${jid_opt}  (array index ${opt_idx})

  REMINDER: BIN must point at a G4DCSmonitor built on the cluster module stack
            (e.g. via slurm/build.sbatch); the login-node build is NOT usable
            on the compute nodes.  Currently BIN=${BIN}

  Watch:   squeue -u \$USER -o "%.10i %.14P %.13j %.2t %.10M %.6D %R"
  Logs:    ${HERE}/logs/
EOF

if [[ "${DRY_RUN}" == "1" ]]; then
    echo
    echo "DRY_RUN=1: no jobs were submitted."
fi
