#!/usr/bin/env bash
# =====================================================================
# merge_config.sh <cfgdir> <stem-no-ext> <nchunks> [keep_chunks 0|1]
#
# Merge one p1.5 config's chunk files into its library file:
#   1. hadd -f  <stem>.root  <stem>_chunk01..chunk<k>.root
#   2. consolidate the hadded sectorMeta (one row per chunk) into a
#      SINGLE row with T_gen = sum(T_gen_i) and M = sum(M_i)
#      -- SectorFluxSampler::DoLoad reads only sectorMeta entry 0, so
#      a merged file with per-chunk rows would replay T_gen = one chunk.
#   3. consolidate_meta.C verifies the result by re-reading the file;
#      chunks are deleted only when KEEP=0 AND consolidation succeeded.
#
# Used by p15_job.sbatch (HPC, ROOT from the module stack) and by
# run_local.sh (needs 'root' or $ROOTSYS/bin/root on this machine).
# =====================================================================
set -euo pipefail

CFGDIR="${1:?usage: merge_config.sh <cfgdir> <stem-no-ext> <nchunks> [keep_chunks]}"
STEM="${2:?stem}"
NCHUNKS="${3:?nchunks}"
KEEP="${4:-1}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

cd "${CFGDIR}"

ROOT_BIN="$(command -v root || true)"
if [ -z "${ROOT_BIN}" ] && [ -n "${ROOTSYS:-}" ] && [ -x "${ROOTSYS}/bin/root" ]; then
    ROOT_BIN="${ROOTSYS}/bin/root"
fi
[ -n "${ROOT_BIN}" ] || {
    echo "FATAL: 'root' not found (load the ROOT module, or export ROOTSYS)" >&2
    exit 2
}

# G4's wroot writer corrupts files that cross START_BIG_FILE = 2.0e9 B;
# the result is a garbage header and an enormous (sparse) apparent size.
# Never hand such a file to hadd.
MAX_CHUNK_BYTES="${MAX_CHUNK_BYTES:-4000000000}"

# chunk list, zero-padded exactly like generate_p15_macros.sh (width of <nchunks>)
CHUNKS=()
for k in $(seq -w 1 "${NCHUNKS}"); do
    f="${STEM}_chunk${k}.root"
    [ -f "${f}" ] || {
        echo "FATAL: missing chunk ${f} -- task incomplete; chunks kept" >&2
        exit 1
    }
    size="$(stat -c %s "${f}")"
    if [ "${size}" -gt "${MAX_CHUNK_BYTES}" ]; then
        echo "FATAL: ${f} is ${size} bytes (> MAX_CHUNK_BYTES=${MAX_CHUNK_BYTES})." >&2
        echo "       A wroot chunk that crosses 2.0e9 B is corrupt." >&2
        echo "       Delete the chunk files, regenerate the macro with" >&2
        echo "       CHUNK_S=10 (18 chunks), then rerun this config." >&2
        exit 1
    fi
    CHUNKS+=("${f}")
done

echo "[merge] hadd ${#CHUNKS[@]} chunks -> ${STEM}.root"
hadd -f "${STEM}.root" "${CHUNKS[@]}"

echo "[merge] consolidating sectorMeta into one row (T_gen = sum, M = sum)"
"${ROOT_BIN}" -b -q "${SCRIPT_DIR}/consolidate_meta.C(\"${STEM}.root\", ${NCHUNKS})"

if [ "${KEEP}" = "0" ]; then
    rm -f "${CHUNKS[@]}"
    echo "[merge] removed ${#CHUNKS[@]} chunk files"
fi
echo "[merge] OK: ${STEM}.root"
