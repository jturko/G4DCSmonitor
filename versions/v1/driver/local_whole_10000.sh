#!/usr/bin/env bash
set -uo pipefail
ROOT=/home/turko46/geant4-sims/G4DCSmonitor
BIN="$ROOT/build/G4DCSmonitor"
MACDIR="$ROOT/slurm/muon_whole_fast"
OUT="$ROOT/slurm/data_output_whole"
LOG="$ROOT/slurm/logs/local_whole_10000"
mkdir -p "$OUT" "$LOG"
# Hard per-task ceiling, and a no-progress (stall) timeout.  The intermittent
# multi-thread race freezes the progress log with one thread spinning at 100%
# CPU, so we watch the log mtime and kill/retry if it stops advancing.
TIMEOUT="${TIMEOUT:-9000}"
STALL="${STALL:-600}"
MAXTRY="${MAXTRY:-5}"
# The sipmHits tree holds one row per (event, SiPM) for events whose muon
# entered the slab.  A handful of events miss the slab (the beam matches the
# plate exactly), so the true count is slightly BELOW nEvents*nSiPM.  Accept a
# completed run (rc==0) once the row count is within ~2% of the nominal max;
# requiring exact equality wrongly discarded a finished 66-minute run.
nominal_rows() { case "$1" in 1) echo 80000;; 2) echo 40000;; 3) echo 20000;; esac; }
min_rows() {
  case "$1" in
    1) echo 78400;;
    2) echo 39200;;
    3) echo 19600;;
  esac
}
rows_of() {
  root -l -b -q -e "TFile f(\"$OUT/muon_whole_out_c$1_r$2.root\"); TTree*t=(TTree*)f.Get(\"sipmHits\"); printf(\"%lld\n\",t?t->GetEntries():-1)" 2>/dev/null | grep -oE '^-?[0-9]+' | tail -1
}
is_complete() { # $1=cfg $2=refl ; echoes 1/0
  local n min
  n=$(rows_of "$1" "$2"); min=$(min_rows "$1")
  if [ "${n:-0}" -ge "$min" ] 2>/dev/null; then echo 1; else echo 0; fi
}
i=0
for c in 1 2 3; do
  for r in 0 1 2 3; do
    b="whole_c${c}_r${r}"
    want=$(nominal_rows "$c")
    if [ "$(is_complete "$c" "$r")" = "1" ]; then echo "=== skip $b (already done) ==="; continue; fi
    try=1
    while [ "$try" -le "$MAXTRY" ]; do
      slog="$LOG/${b}.log"
      echo "=== [$(date -Is)] $b attempt $try/$MAXTRY (hard ${TIMEOUT}s, stall ${STALL}s) ==="
      start=$(date +%s)
      "$BIN" "$MACDIR/${b}.mac" > "$slog" 2>&1 < /dev/null &
      simpid=$!
      # watchdog
      last_size=$(stat -c %s "$slog" 2>/dev/null || echo 0)
      last_change=$start
      while kill -0 "$simpid" 2>/dev/null; do
        sleep 30
        now=$(date +%s)
        cur_size=$(stat -c %s "$slog" 2>/dev/null || echo 0)
        if [ "$cur_size" != "$last_size" ]; then last_size=$cur_size; last_change=$now; fi
        if [ $(( now - last_change )) -ge "$STALL" ]; then
          echo "    !! no progress for ${STALL}s (size=${cur_size}) -> killing stalled sim (pid ${simpid})"
          kill -TERM "$simpid" 2>/dev/null
          sleep 5
          kill -KILL "$simpid" 2>/dev/null
          break
        fi
        if [ $(( now - start )) -ge "$TIMEOUT" ]; then
          echo "    !! hard timeout ${TIMEOUT}s -> killing sim"
          kill -TERM "$simpid" 2>/dev/null
          sleep 5
          kill -KILL "$simpid" 2>/dev/null
          break
        fi
      done
      wait "$simpid" 2>/dev/null
      rc=$?
      wall=$(( $(date +%s) - start ))
      n=$(rows_of "$c" "$r")
      complete=$(is_complete "$c" "$r")
      echo "=== [$(date -Is)] $b attempt $try rc=$rc wall=${wall}s complete=$complete (rows=${n:-?}) ==="
      if [ "$rc" -eq 0 ] && [ "$complete" = "1" ]; then break; fi
      try=$((try+1))
    done
    i=$((i+1))
  done
done
echo "ALL DONE [$(date -Is)] tasks_run=$i"
