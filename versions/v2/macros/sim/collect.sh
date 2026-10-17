#!/usr/bin/env bash
# =============================================================================
# Count / list the ROOT files produced by the SLURM arrays and show an rsync
# line to pull them back for local analysis.
#
#   matrix scan     : 16 tasks x 3 positions = 48 files in data_<VERSION>/matrix/
#   whole-slab scan : 16 tasks x 1 file      = 16 files in data_<VERSION>/whole/
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

n=$(ls -1 "${DATADIR}"/muon_out_*.root 2>/dev/null | wc -l)
echo "Found ${n} / 48 expected matrix ROOT files in ${DATADIR}"
ls -1 "${DATADIR}"/muon_out_*.root 2>/dev/null | sort

nw=$(ls -1 "${WHOLEDIR}"/muon_whole_out_*.root 2>/dev/null | wc -l)
echo
echo "Found ${nw} / 16 expected whole-slab ROOT files in ${WHOLEDIR}"
ls -1 "${WHOLEDIR}"/muon_whole_out_*.root 2>/dev/null | sort

if [[ $# -ge 1 ]]; then
    echo
    echo "To fetch into the local repo, run from the repo root:"
    echo "  rsync -avP \"$1\":${DATADIR}/ ./data_${VERSION}/matrix/"
    echo "  rsync -avP \"$1\":${WHOLEDIR}/ ./data_${VERSION}/whole/"
fi
