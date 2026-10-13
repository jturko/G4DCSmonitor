#!/usr/bin/env bash
set -uo pipefail
ROOT=/home/turko46/geant4-sims/G4DCSmonitor
BIN="$ROOT/build/G4DCSmonitor"
MACDIR="$ROOT/slurm/muon_fast"
OUT="$ROOT/slurm/data_output"
LOG="$ROOT/slurm/logs/local_matrix_newpos"
mkdir -p "$OUT" "$LOG"
TIMEOUT="${TIMEOUT:-1800}"
MAXTRY="${MAXTRY:-3}"
i=0
for c in 1 2 3; do
  for r in 0 1 2 3; do
    b="scan_c${c}_r${r}"
    try=1
    while [ "$try" -le "$MAXTRY" ]; do
      echo "=== [$(date -Is)] $b attempt $try/$MAXTRY (timeout ${TIMEOUT}s) ==="
      start=$(date +%s)
      timeout "$TIMEOUT" "$BIN" "$MACDIR/${b}.mac" > "$LOG/${b}.log" 2>&1
      rc=$?
      wall=$(( $(date +%s) - start ))
      n=$(ls "$OUT"/muon_out_c${c}_r${r}_*.root 2>/dev/null | wc -l)
      echo "=== [$(date -Is)] $b attempt $try rc=$rc wall=${wall}s files=${n}/3 ==="
      if [ "$rc" -eq 0 ] && [ "$n" -eq 3 ]; then break; fi
      try=$((try+1))
    done
    i=$((i+1))
  done
done
echo "ALL DONE [$(date -Is)] tasks=$i"
