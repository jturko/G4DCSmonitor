#!/bin/bash
set -eu

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
BIN="${BIN:-$REPO/build/G4DCSmonitor}"
OUT="${OUT:-$PWD/benchmark_output}"

if [ "$#" -gt 0 ]; then
    MACROS=("$@")
else
    MACROS=(
        "$REPO/macros/benchmark/run_cask3_neutron.mac"
        "$REPO/macros/benchmark/run_cask3_gamma.mac"
    )
fi

mkdir -p "$OUT/logs"
for MAC in "${MACROS[@]}"; do
    tag="$(basename "$MAC" .mac)"
    mkdir -p "$OUT/$tag"
    sbatch \
        --job-name="bench_$tag" \
        --output="$OUT/logs/${tag}_%j.out" \
        --error="$OUT/logs/${tag}_%j.err" \
        --export=ALL,BIN="$BIN" \
        "$HERE/benchmark_job.sbatch" "$MAC" "$OUT/$tag"
done
