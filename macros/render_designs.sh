#!/usr/bin/env bash
# =============================================================================
# render_designs.sh -- head-less OpenGL renders of the geometric v7_opt designs.
#
# Runs four geometry-only macros (no beam, no trajectories) with the
# TSG_OFFSCREEN driver and writes five 1200 x 1200 PNGs into report/figures/:
#
#   vis_design_c1_4mm.mac  -> design_ogl_c1_4mm.png        Config 1, 4 mm SiPM
#   vis_design_c1_6mm.mac  -> design_ogl_c1_6mm.png        Config 1, 6 mm SiPM
#   vis_design_wls10.mac   -> design_ogl_wls10.png         preset 10, clamp hidden
#                          -> design_ogl_wls10_clamp.png   preset 10, clamp shown
#   vis_design_wls11.mac   -> design_ogl_wls11.png         preset 11, dual-ended
#
# The 4 mm / 6 mm pair uses one identical camera, and so does the 10 / 11 pair,
# so each pair can be compared (or diffed) directly.  Every macro hides the
# volumes it does not want to show with /vis/geometry/set/visibility and forces
# opacity 1 on everything drawn, so no appearance depends on the alpha channel.
# No source code is modified and nothing is written outside report/figures/.
#
# Usage:
#   macros/render_designs.sh
#   BIN=/abs/path/G4DCSmonitor macros/render_designs.sh   # pin a build
#
# Environment:
#   BIN   path to G4DCSmonitor (default <repo>/build/G4DCSmonitor)
#
# Renders take a few seconds each (no physics, no tracking).  Exit status is
# non-zero if any macro fails or any expected PNG is missing/empty.
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"
OUTDIR="${ROOTDIR}/report/figures"

MACROS=(
    vis_design_c1_4mm.mac
    vis_design_c1_6mm.mac
    vis_design_wls10.mac
    vis_design_wls11.mac
)
PNGS=(
    design_ogl_c1_4mm.png
    design_ogl_c1_6mm.png
    design_ogl_wls10.png
    design_ogl_wls11.png
    design_ogl_wls10_clamp.png
)

if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: binary not found: ${BIN}" >&2
    echo "       build it with: cmake --build ${ROOTDIR}/build -j" >&2
    exit 1
fi

mkdir -p "${OUTDIR}"

FAIL=0
for m in "${MACROS[@]}"; do
    echo "=== running macros/${m} ==="
    if ! ( cd "${ROOTDIR}" && "${BIN}" "macros/${m}" ); then
        echo "ERROR: ${m} failed" >&2
        FAIL=1
    fi
done

echo "=== output files in ${OUTDIR} ==="
for p in "${PNGS[@]}"; do
    f="${OUTDIR}/${p}"
    if [[ ! -s "${f}" ]]; then
        echo "  MISSING/EMPTY: ${p}" >&2
        FAIL=1
        continue
    fi
    # A blank 1200x1200 PNG of a white background is only a few kB; the real
    # renders are 35-200 kB.  Treat anything under 20 kB as degenerate.
    size=$(stat -c%s "${f}")
    if (( size < 20000 )); then
        echo "  DEGENERATE (${size} bytes): ${p}" >&2
        FAIL=1
    else
        printf '  %-28s %8d bytes\n' "${p}" "${size}"
    fi
done

if (( FAIL != 0 )); then
    echo "=== FAILED ===" >&2
    exit 1
fi
echo "=== done: ${#PNGS[@]} PNGs written ==="
