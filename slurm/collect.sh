#!/usr/bin/env bash
# =============================================================================
# Count / list the ROOT files produced by the SLURM arrays and show an rsync
# line to pull them back for local analysis.
#
#   matrix scan      : 16 tasks x 3 positions = 48 files in data_<VERSION>/matrix/
#   whole-slab scan  : 16 tasks x 1 file      = 16 files in data_<VERSION>/whole/
#   contact scan     : 21 tasks x 3 positions = 63 files in data_<VERSION>/contact/
#                      (one subdirectory per cfg/fraction case)
#   Al backing scan  :  6 tasks x 3 positions = 18 files in data_<VERSION>/al/
#                      (one subdirectory per cfg/backing case)
#
# The expected contact/Al counts are derived from the generated manifests when
# present, so overriding CONTACT_FRACTIONS / CONTACT_CFGS / AL_BACKINGS /
# AL_CFGS does not stale this script.
#
# Usage:  slurm/collect.sh [USER@HOST]
#   VERSION (default v2.2) selects the data-set directory tree.
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATAROOT="${DATAROOT:-${HERE}}"
VERSION="${VERSION:-v2.2}"
DATADIR="${DATAROOT}/data_${VERSION}/matrix"
WHOLEDIR="${DATAROOT}/data_${VERSION}/whole"
CONTACTDIR="${DATAROOT}/data_${VERSION}/contact"
ALDIR="${DATAROOT}/data_${VERSION}/al"

manifest_tasks() {
    local manifest="$1" fallback="$2"
    if [[ -f "${manifest}" ]]; then
        wc -l < "${manifest}"
    else
        printf '%s\n' "${fallback}"
    fi
}

n_contact_tasks="$(manifest_tasks "${HERE}/muon_contact/manifest.txt" 21)"
n_al_tasks="$(manifest_tasks "${HERE}/muon_al/manifest.txt" 6)"
expected_contact=$((n_contact_tasks * 3))
expected_al=$((n_al_tasks * 3))

n=$(find "${DATADIR}" -maxdepth 1 -type f -name 'muon_out_*.root' 2>/dev/null | wc -l)
echo "Found ${n} / 48 expected matrix ROOT files in ${DATADIR}"
find "${DATADIR}" -maxdepth 1 -type f -name 'muon_out_*.root' 2>/dev/null | sort

nw=$(find "${WHOLEDIR}" -maxdepth 1 -type f -name 'muon_whole_out_*.root' 2>/dev/null | wc -l)
echo
echo "Found ${nw} / 16 expected whole-slab ROOT files in ${WHOLEDIR}"
find "${WHOLEDIR}" -maxdepth 1 -type f -name 'muon_whole_out_*.root' 2>/dev/null | sort

nc=$(find "${CONTACTDIR}" -type f -name 'muon_out_*.root' 2>/dev/null | wc -l)
echo
echo "Found ${nc} / ${expected_contact} expected contact-scan ROOT files in ${CONTACTDIR}"
if [[ -d "${CONTACTDIR}" ]]; then
    for d in "${CONTACTDIR}"/c*; do
        [[ -d "${d}" ]] || continue
        k=$(find "${d}" -maxdepth 1 -type f -name 'muon_out_*.root' 2>/dev/null | wc -l)
        printf '  %-16s %s/3\n' "$(basename "${d}")" "${k}"
    done | sort
fi

na=$(find "${ALDIR}" -type f -name 'muon_out_*.root' 2>/dev/null | wc -l)
echo
echo "Found ${na} / ${expected_al} expected Al-backing ROOT files in ${ALDIR}"
if [[ -d "${ALDIR}" ]]; then
    for d in "${ALDIR}"/c*; do
        [[ -d "${d}" ]] || continue
        k=$(find "${d}" -maxdepth 1 -type f -name 'muon_out_*.root' 2>/dev/null | wc -l)
        printf '  %-16s %s/3\n' "$(basename "${d}")" "${k}"
    done | sort
fi

if [[ $# -ge 1 ]]; then
    echo
    echo "To fetch into the local repo, run from the repo root:"
    echo "  rsync -avP \"$1\":${DATADIR}/ ./data_${VERSION}/matrix/"
    echo "  rsync -avP \"$1\":${WHOLEDIR}/ ./data_${VERSION}/whole/"
    echo "  rsync -avP \"$1\":${CONTACTDIR}/ ./data_${VERSION}/contact/"
    echo "  rsync -avP \"$1\":${ALDIR}/ ./data_${VERSION}/al/"
fi
