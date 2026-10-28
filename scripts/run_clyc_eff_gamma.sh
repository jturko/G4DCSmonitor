#!/usr/bin/env bash
#
# Run the CLYC monoenergetic-gamma efficiency scan.
#
# Examples:
#   scripts/run_clyc_eff_gamma.sh --pilot
#   scripts/run_clyc_eff_gamma.sh --nev 100000000
#   scripts/run_clyc_eff_gamma.sh --nev 10000000 --nopuck --distances 200 --energies 661.64
#
# The output ROOT files are written to build/validation/clyc_eff/.
set -euo pipefail

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${BASE}/build/G4DCSmonitor}"
TEMPLATE_PUCK="${BASE}/macros/validation/clyc_eff_gamma_template.mac"
TEMPLATE_NOPUCK="${BASE}/macros/validation/clyc_eff_gamma_nopuck_template.mac"
OUTDIR="${OUTDIR:-${BASE}/build/validation/clyc_eff}"

MODE="production"
NEV=""
DISTANCES="200 300 400"
ENERGIES="661.64 1173.23 1332.51"
NOPUCK=0

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --pilot                1e6 events, 200 mm, 661.64 keV only
  --production           All configured distances and energies (default)
  --nopuck               Omit the epoxy source puck (control run)
  --nev N                Events per point (default: 1e6 pilot, 1e7 production)
  --distances "LIST"     Space-separated source distances in mm (default: "200 300 400")
  --energies "LIST"      Space-separated gamma energies in keV
  -h, --help             Show this help

Environment:
  BIN                    Override the G4DCSmonitor binary path
  OUTDIR                 Override the output directory
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --pilot)       MODE="pilot"; shift ;;
        --production)  MODE="production"; shift ;;
        --nopuck)      NOPUCK=1; shift ;;
        --nev)         NEV="${2:?--nev requires a value}"; shift 2 ;;
        --distances)   DISTANCES="${2:?--distances requires a value}"; shift 2 ;;
        --energies)    ENERGIES="${2:?--energies requires a value}"; shift 2 ;;
        -h|--help)     usage; exit 0 ;;
        *)             echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ "$MODE" == "pilot" ]]; then
    [[ -z "$NEV" ]] && NEV=1000000
    DISTANCES="${PILOT_DISTANCES:-200}"
    ENERGIES="${PILOT_ENERGIES:-661.64}"
else
    [[ -z "$NEV" ]] && NEV=10000000
fi

TEMPLATE="$TEMPLATE_PUCK"
if [[ "$NOPUCK" -eq 1 ]]; then
    TEMPLATE="$TEMPLATE_NOPUCK"
fi

if [[ ! -x "$BIN" ]]; then
    echo "ERROR: binary not found or not executable: $BIN" >&2
    echo "Build with: cmake --build ${BASE}/build -j" >&2
    exit 1
fi

mkdir -p "${OUTDIR}/macros" "${OUTDIR}/logs"

echo "Binary:   $BIN"
echo "Template: $TEMPLATE"
echo "Output:   $OUTDIR"
echo "Events:   $NEV per point"
echo "Distances: $DISTANCES mm"
echo "Energies:  $ENERGIES keV"
echo

for dist in $DISTANCES; do
    for energy in $ENERGIES; do
        etag="${energy/./p}"
        tag="clyc_eff_${dist}mm_${etag}keV"
        if [[ "$NOPUCK" -eq 1 ]]; then
            tag="${tag}_nopuck"
        fi

        macro="${OUTDIR}/macros/${tag}.mac"
        out="${OUTDIR}/${tag}"
        log="${OUTDIR}/logs/${tag}.log"

        sed -e "s/@DIST_MM@/${dist}/g" \
            -e "s/@ENERGY_KEV@/${energy}/g" \
            -e "s|@OUTPUT@|${out}|g" \
            -e "s/@NEVENTS@/${NEV}/g" \
            "$TEMPLATE" > "$macro"

        echo "=== ${tag}: N=${NEV}, distance=${dist} mm, energy=${energy} keV ==="
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
done

echo
echo "All requested runs finished."
