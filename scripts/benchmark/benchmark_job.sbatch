#!/bin/bash
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=32
#SBATCH --mem=128G
#SBATCH --time=1-00:00:00

set -eu
MACRO="${1:?usage: benchmark_job.sbatch <macro> <outdir>}"
OUTDIR="${2:?usage: benchmark_job.sbatch <macro> <outdir>}"
BIN="${BIN:?set BIN to the G4DCSmonitor binary}"

mkdir -p "$OUTDIR"
cd "$OUTDIR"
echo "[benchmark] $(basename "$MACRO") -> $OUTDIR on $(hostname) at $(date)"
"$BIN" "$MACRO"
