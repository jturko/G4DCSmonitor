#!/usr/bin/env bash
# =====================================================================
# run_local.sh -- smoke-test the p1.5 production path on THIS machine
# (no SLURM, local build/G4DCSmonitor + local ROOT).
#
# It stages a symlink tree so generate_p15_macros.sh discovers a local
# p1 file, generates ONE config macro (default: baseFuel 0, cask 1,
# hexant rot 0, tiny exposure), runs the binary with
# --mode background-sector, then exercises merge_config.sh
# (hadd + sectorMeta consolidation) and verify_p15.sh.
#
# Knobs:
#   P1_FILE       local p1 .root
#                 default: ${REPO}/build/validation/surface_gamma662_cask4_1Mevt.root
#   NUM_PRIMARIES number of primaries in that p1 file; parsed from the
#                 file name (*_nEvts<k>.root or *_1Mevt* -> 1000000);
#                 export explicitly if the name carries neither pattern
#   SOURCE_LABEL / SRC_TAG / NSHELLS / STYPE / RATE
#                 staging + macro source-table fields
#                 (default gamma_662keV / gamma-662keV / 28 / 28 / 1.4e14)
#   BASE / CASK / ROT      config selectors (default 0 / 1 / 0)
#   MEAS_TIME / CHUNK_S    default 0.02 / 0.02 (seconds; N ~ lambda*T*r)
#   THREADS       default 4
#   LOCALROOT     default ${SCRIPT_DIR}/data_local (gitignored)
#   BIN           default ${REPO}/build/G4DCSmonitor
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "${SCRIPT_DIR}/.." && pwd)"

P1_FILE="${P1_FILE:-${REPO}/build/validation/surface_gamma662_cask4_1Mevt.root}"
BASE="${BASE:-0}"
CASK="${CASK:-1}"
ROT="${ROT:-0}"
MEAS_TIME="${MEAS_TIME:-0.02}"
CHUNK_S="${CHUNK_S:-0.02}"
THREADS="${THREADS:-4}"
SOURCE_LABEL="${SOURCE_LABEL:-gamma_662keV}"
SRC_TAG="${SRC_TAG:-gamma-662keV}"
NSHELLS="${NSHELLS:-28}"
STYPE="${STYPE:-28}"
RATE="${RATE:-1.4e14}"
LOCALROOT="${LOCALROOT:-${SCRIPT_DIR}/data_local}"

BIN="${BIN:-${REPO}/build/G4DCSmonitor}"
if [ ! -x "${BIN}" ]; then
    echo "FATAL: ${BIN} not executable -- cmake --build ${REPO}/build -j" >&2
    exit 1
fi

# number of primaries in the p1 file, for /dcs-monitor/surf/numPrimaries
NUM_PRIMARIES="${NUM_PRIMARIES:-}"
if [ -z "${NUM_PRIMARIES}" ]; then
    B="$(basename "${P1_FILE}")"
    if [[ "${B}" =~ nEvts([0-9]+)\.root$ ]]; then
        NUM_PRIMARIES="${BASH_REMATCH[1]}"
    elif [[ "${B}" =~ 1Mevt ]]; then
        NUM_PRIMARIES=1000000
    else
        echo "FATAL: cannot infer primaries from ${B}; export NUM_PRIMARIES" >&2
        exit 1
    fi
fi

# --- stage the p1 file under the SurfaceFlux_v3 naming scheme ---------
STAGE="${LOCALROOT}/SurfaceFlux_local"
STAGED_DIR="${STAGE}/${SOURCE_LABEL}/using"
STAGED_NAME="surface_${SRC_TAG}_fuel${BASE}_nShells${NSHELLS}_nEvts${NUM_PRIMARIES}.root"
mkdir -p "${STAGED_DIR}"
ln -sfn "$(cd "$(dirname "${P1_FILE}")" && pwd)/$(basename "${P1_FILE}")" \
         "${STAGED_DIR}/${STAGED_NAME}"

echo "== run_local.sh =="
echo "  p1 file : ${P1_FILE} (numPrimaries ${NUM_PRIMARIES})"
echo "  config  : baseFuel ${BASE}, ${SOURCE_LABEL}, cask ${CASK}, hexant rot ${ROT} deg"
echo "  exposure: ${MEAS_TIME} s in chunks of ${CHUNK_S} s, ${THREADS} threads"
echo "  outputs : ${LOCALROOT}/data/"

# --- generate ONE macro ----------------------------------------------
env MACDIR="${LOCALROOT}/macros" \
    DATADIR="${LOCALROOT}/data" \
    P1_BASE_DIR="${STAGE}" \
    MEAS_TIME="${MEAS_TIME}" CHUNK_S="${CHUNK_S}" THREADS="${THREADS}" \
    SOURCES_ONLY="${SOURCE_LABEL}" BASES_ONLY="${BASE}" \
    CASKS_ONLY="${CASK}" HEXANTS_ONLY="${ROT}" \
    "${SCRIPT_DIR}/generate_p15_macros.sh"

IFS='|' read -r MAC_BASE CFGDIR STEM NCHUNKS < "${LOCALROOT}/macros/manifest.txt"

echo "== running ${MAC_BASE} =="
set +e
"${BIN}" --mode background-sector "${LOCALROOT}/macros/${MAC_BASE}"
RC=$?
set -e
if [ "${RC}" -ne 0 ]; then
    echo "FATAL: G4DCSmonitor rc=${RC}" >&2
    exit "${RC}"
fi

# --- merge + consolidate + verify (same code path as the HPC task) ---
"${SCRIPT_DIR}/merge_config.sh" "${CFGDIR}" "${STEM}" "${NCHUNKS}" 1

echo "== verify =="
env MACDIR="${LOCALROOT}/macros" DATADIR="${LOCALROOT}/data" \
    DEEP="${DEEP:-0}" JOBS="1" \
    "${SCRIPT_DIR}/verify_p15.sh"

echo
echo "== run_local.sh OK =="
echo "  macro   : ${LOCALROOT}/macros/${MAC_BASE}"
echo "  library : ${STEM}.root (sectorFlux + consolidated sectorMeta)"
