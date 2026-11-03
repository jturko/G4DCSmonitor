#!/usr/bin/env bash
#
# Convert the 2026-10-27 CLYC validation CNF spectra to ROOT for the report
# analysis.  The CNF files are read-only input; the ROOT copies are written to
# a build directory.
#
# Usage:
#   report/eff_clyc/scripts/convert_exp_spectra.sh [CNF_DIR] [OUT_DIR]
#
# Environment:
#   CNF2ROOT   path to the cnf2root executable
#              (default: $HOME/programs/cnf2root/build/cnf2root)
#
# The output names are the ones expected by analyze_exp_spectra.C and
# make_exp_sim_figs.C:
#   Hintergrund.root
#   Meas1_200_CS_Clyc2.root, Meas1_300_CS_Clyc2.root, Meas1_400_CS_Clyc2.root
#   Meas1_200_Co_Clyc2.root, Meas1_300_Co_Clyc2.root, Meas1_400_Co_Clyc2.root
#   Abzug_200CS.root
#
set -euo pipefail

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
CNF_DIR="${1:-${HOME}/data/CLYC_validation/20261027_Effizienz_Messungen}"
OUT_DIR="${2:-${BASE}/build/analysis/clyc_exp_2026}"
CNF2ROOT="${CNF2ROOT:-${HOME}/programs/cnf2root/build/cnf2root}"

if [[ ! -x "${CNF2ROOT}" ]]; then
    echo "ERROR: cnf2root not found or not executable: ${CNF2ROOT}" >&2
    exit 1
fi
if [[ ! -d "${CNF_DIR}" ]]; then
    echo "ERROR: CNF directory not found: ${CNF_DIR}" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"

convert() {
    local src="$1"
    local dst="$2"
    echo "==> ${src} -> ${dst}"
    "${CNF2ROOT}" --no-complete -o "${OUT_DIR}/${dst}" "${CNF_DIR}/${src}" >/dev/null
}

convert "Hintergrund.CNF" "Hintergrund.root"
for d in 200 300 400; do
    convert "Meas1, ${d} CS, Clyc2.CNF" "Meas1_${d}_CS_Clyc2.root"
    convert "Meas1, ${d} Co, Clyc2.CNF" "Meas1_${d}_Co_Clyc2.root"
done
convert "Abzug_Hintergrund/Abzug Hintergrund Meas1, 200 CS, Clyc2.CNF" \
        "Abzug_200CS.root"

echo
echo "Converted spectra written to ${OUT_DIR}"
