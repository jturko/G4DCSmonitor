#!/usr/bin/env bash
# =====================================================================
# verify_p15.sh [--deep] [--jobs N]
#
# Sanity-check the p1.5 campaign: walk MACDIR/manifest.txt (line i =
# array task i-1; field 3 = merged file stem) and run verify_meta.C on
# each merged library file.  Reports OK / FAIL / MISSING counts and the
# failing config list; the full output is kept in
# ${DATADIR}/logs/verify_p15.log next to the data.
#
# Env knobs:
#   MACDIR  default ${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/SectorBkgd}/macros_${VERSION:-v1}/p15
#   DATADIR default ${DATAROOT}/data_${VERSION:-v1}
#   DEEP    default 0; --deep = 1  (also reads the cellId column of every
#           file -- one full pass per file, slow; prefer a manifest subset)
#   JOBS    default 8 (concurrent verify_meta.C processes)
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/SectorBkgd}"
VERSION="${VERSION:-v1}"
MACDIR="${MACDIR:-${DATAROOT}/macros_${VERSION}/p15}"
DATADIR="${DATADIR:-${DATAROOT}/data_${VERSION}}"
LOGDIR="${DATADIR}/logs"
DEEP="${DEEP:-0}"
JOBS="${JOBS:-8}"

DEEP_ARG=0
for a in "$@"; do
    case "${a}" in
        --deep) DEEP=1 ;;
        --jobs=*) JOBS="${a#--jobs=}" ;;
        *) echo "usage: verify_p15.sh [--deep] [--jobs=N]" >&2; exit 1 ;;
    esac
done
DEEP_ARG="${DEEP}"

MANIFEST="${MACDIR}/manifest.txt"
[ -f "${MANIFEST}" ] || { echo "FATAL: no manifest at ${MANIFEST}" >&2; exit 1; }
mkdir -p "${LOGDIR}"
LOGFILE="${LOGDIR}/verify_p15.log"

ROOT_BIN="$(command -v root || true)"
if [ -z "${ROOT_BIN}" ] && [ -n "${ROOTSYS:-}" ] && [ -x "${ROOTSYS}/bin/root" ]; then
    ROOT_BIN="${ROOTSYS}/bin/root"
fi
[ -n "${ROOT_BIN}" ] || {
    echo "FATAL: 'root' not found (load the ROOT module, or export ROOTSYS)" >&2
    exit 2
}

MISSING=()
N_TOTAL=0
while IFS='|' read -r _mac _cfgdir STEM _nchunks; do
    [ -n "${STEM}" ] || continue
    N_TOTAL=$(( N_TOTAL + 1 ))
    if [ -f "${STEM}.root" ]; then
        printf '%s\n' "${STEM}.root"
    else
        MISSING+=("${STEM}.root")
    fi
done < "${MANIFEST}" > "${LOGDIR}/verify_files.list"

N_MISSING=${#MISSING[@]}
echo "verify_p15.sh: ${N_TOTAL} configs in manifest, ${N_MISSING} missing file(s), verifying the rest (${JOBS} parallel, deep=${DEEP_ARG})"
[ "${N_MISSING}" -eq 0 ] || printf '%s\n' "${MISSING[@]}" | sed 's/^/  MISSING: /'

: > "${LOGFILE}.tmp"
xargs -0 -P "${JOBS}" -I{} "${ROOT_BIN}" -b -q "${SCRIPT_DIR}/verify_meta.C(\"{}\", ${DEEP_ARG})" \
    < <(tr '\n' '\0' < "${LOGDIR}/verify_files.list") >> "${LOGFILE}.tmp" || true

# consolidate results; verify_meta.C prints one VERIFY_OK / VERIFY_FAIL line
N_OK="$(grep -c '^VERIFY_OK' "${LOGFILE}.tmp" || true)"
N_FAIL="$(grep -c '^VERIFY_FAIL' "${LOGFILE}.tmp" || true)"
cat "${LOGFILE}.tmp" >> "${LOGFILE}"
mv "${LOGFILE}.tmp" "${LOGFILE}" 2>/dev/null || true

echo
echo "== verify_p15.sh summary =="
echo "  configs in manifest : ${N_TOTAL}"
echo "  VERIFY_OK           : ${N_OK}"
echo "  VERIFY_FAIL         : ${N_FAIL}  (details: grep '^VERIFY_FAIL' ${LOGFILE})"
echo "  MISSING library file: ${N_MISSING}"
echo "  full log            : ${LOGFILE}"

if [ "${N_FAIL}" -gt 0 ] || [ "${N_MISSING}" -gt 0 ]; then
    echo
    echo "Rerun failed array tasks with: slurm/rerun_failed.sh <array-job-id>"
    exit 1
fi
exit 0
