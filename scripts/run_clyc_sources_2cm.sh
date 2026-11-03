#!/usr/bin/env bash
#
# Simulate the calibration sources in eff_clyc1.csv at a fixed 2 cm distance.
#
# The ion is placed at the centre of the epoxy source puck and decays at rest
# (G4RadioactiveDecayPhysics). Output ROOT files are written to
# build/validation/clyc_sources_2cm/.
#
# Examples:
#   scripts/run_clyc_sources_2cm.sh --pilot
#   scripts/run_clyc_sources_2cm.sh --nev 10000000
#
set -euo pipefail

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${BASE}/build/G4DCSmonitor}"
TEMPLATE="${BASE}/macros/validation/clyc_source_ion_template.mac"
OUTDIR="${OUTDIR:-${BASE}/build/validation/clyc_sources_2cm}"
DISTANCE="${DISTANCE:-20}"
SOURCES="${SOURCES:-Cs137 Pb210 Am241 Eu152 Co60}"
NEV=10000000

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --pilot                1e5 events per source
  --nev N                Events per source (default: 1e7)
  --distance MM          Source-puck centre distance (default: 20 mm)
  -h, --help             Show this help

Environment:
  BIN                    Override the G4DCSmonitor binary path
  OUTDIR                 Override the output directory
  DISTANCE               Override the source distance in mm
  SOURCES                Space-separated subset of Cs137 Pb210 Am241 Eu152 Co60
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --pilot)      NEV=100000; shift ;;
        --nev)        NEV="${2:?--nev requires a value}"; shift 2 ;;
        --distance)   DISTANCE="${2:?--distance requires a value}"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *)            echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! -x "$BIN" ]]; then
    echo "ERROR: binary not found or not executable: $BIN" >&2
    echo "Build with: cmake --build ${BASE}/build -j" >&2
    exit 1
fi

mkdir -p "${OUTDIR}/macros" "${OUTDIR}/logs"

names=(Cs137 Pb210 Am241 Eu152 Co60)
zs=(55 82 95 63 27)
as=(137 210 241 152 60)

echo "Binary:   $BIN"
echo "Template: $TEMPLATE"
echo "Output:   $OUTDIR"
echo "Events:   $NEV per source"
echo "Distance: ${DISTANCE} mm"
echo

for i in "${!names[@]}"; do
    name="${names[$i]}"
    if [[ " ${SOURCES} " != *" ${name} "* ]]; then
        continue
    fi
    z="${zs[$i]}"
    a="${as[$i]}"
    tag="clyc_source_${name}_${DISTANCE}mm"
    macro="${OUTDIR}/macros/${tag}.mac"
    out="${OUTDIR}/${tag}"
    log="${OUTDIR}/logs/${tag}.log"

    sed -e "s/@Z@/${z}/g" \
        -e "s/@A@/${a}/g" \
        -e "s/@NAME@/${name}/g" \
        -e "s/@DIST_MM@/${DISTANCE}/g" \
        -e "s|@OUTPUT@|${out}|g" \
        -e "s/@NEVENTS@/${NEV}/g" \
        "$TEMPLATE" > "$macro"

    echo "=== ${name}: N=${NEV}, distance=${DISTANCE} mm ==="
    start=$(date +%s)
    "$BIN" "$macro" > "$log" 2>&1
    end=$(date +%s)

    if [[ ! -f "${out}.root" ]]; then
        echo "ERROR: expected output not found: ${out}.root" >&2
        echo "See log: $log" >&2
        exit 1
    fi
    echo "  wall=$((end - start)) s  output=${out}.root  log=${log}"
done

echo
echo "All source runs finished."
