#!/usr/bin/env bash
#
# Run the CLYC monoenergetic-gamma efficiency scan at 200 mm with the GPS
# emission restricted to a 15 deg half-angle forward cone toward the detector.
#
# This raises the number of gammas that reach the detector per generated
# event.  The cone solid-angle fraction is f = (1 - cos(15 deg))/2 = 0.017037.
# Because all N_gen gammas are generated inside the cone, the equivalent
# isotropic incident count is N_inc,eff = N_gen / f, not N_gen * f.
#
# Outputs:
#   build/validation/clyc_eff/clyc_eff_200mm_<etag>keV_cone.root
#   build/validation/clyc_eff/logs/clyc_eff_200mm_<etag>keV_cone.log
#
# Environment:
#   BIN      Override the G4DCSmonitor binary path
#   OUTDIR   Override the output directory
set -euo pipefail

BASE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${BIN:-${BASE}/build/G4DCSmonitor}"
TEMPLATE="${BASE}/macros/validation/clyc_eff_gamma_cone_template.mac"
OUTDIR="${OUTDIR:-${BASE}/build/validation/clyc_eff}"
NEV=10000000
DISTANCES="200"
ENERGIES="661.64 1173.23 1332.51"

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
echo "Energies:  $ENERGIES keV (15 deg forward cone)"
echo

for dist in $DISTANCES; do
    for energy in $ENERGIES; do
        etag="${energy/./p}"
        tag="clyc_eff_${dist}mm_${etag}keV_cone"

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
        if ! "$BIN" "$macro" > "$log" 2>&1; then
            end=$(date +%s)
            echo "ERROR: run failed after $((end - start)) s" >&2
            echo "Log tail (${log}):" >&2
            tail -n 40 "$log" >&2
            exit 1
        fi
        end=$(date +%s)

        if [[ ! -f "${out}.root" ]]; then
            echo "ERROR: expected output not found: ${out}.root" >&2
            echo "Log tail (${log}):" >&2
            tail -n 40 "$log" >&2
            exit 1
        fi
        echo "  wall=$((end - start)) s  output=${out}.root  log=${log}"
    done
done

echo
echo "All requested runs finished."
