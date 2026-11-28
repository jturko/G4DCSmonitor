#!/bin/bash
# Reproduce the absolute-scale diagnostic: six 10M-record sequential samples
# plus the full-statistics F1 maps, then write the CSV table.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$REPO/scripts/wssa/output_diag"
mkdir -p "$OUT"
: > "$OUT/progress.log"

cd "$REPO"
for cask in Cask1 Cask2 Cask3; do
  for kind in neutrons gamma; do
    tag="${cask}_${kind}"
    particle="photon"
    if [ "$kind" = "neutrons" ]; then particle="neutron"; fi
    root -l -b -q \
      "scripts/wssa/read_wssa.C(\"/home/turko46/data/wti/${cask}/${tag}.wssa\",\"${OUT}/${tag}_seq10m\",10000000,false,1,true,72,82,4100.0,120,1e-9,20.0,12345,\"${particle}\")" \
      > "$OUT/${tag}_seq10m.log" 2>&1
    echo "done ${tag}" >> "$OUT/progress.log"
  done
done

root -l -b -q 'scripts/wssa/check_absolute_scale.C()' \
  > "$OUT/check_absolute_scale.log" 2>&1
echo "ALL DONE" >> "$OUT/progress.log"
