#!/usr/bin/env bash
set -uo pipefail
cd /home/turko46/geant4-sims/G4DCSmonitor/report/figures || exit 1
for f in *.pdf; do
  gs -q -o "/tmp/opencode/flat_$f" -sDEVICE=pdfwrite \
     -dAutoRotatePages=/None -dPDFSETTINGS=/prepress -f "$f" >/dev/null 2>&1
  if [ -s "/tmp/opencode/flat_$f" ]; then
    cp "/tmp/opencode/flat_$f" "$f"
    echo "flattened $f"
  else
    echo "FAILED $f"
  fi
done
