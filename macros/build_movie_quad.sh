#!/usr/bin/env bash
# =============================================================================
# build_movie_quad.sh -- 4-reflector (2x2) time-sliced muon movie.
#
# Renders the SAME 30 MeV muon event for each reflector finish (0=none/bare,
# 1=TiO2, 2=Al foil, 3=glossy paper) with a HARD-CAPPED, CONSTANT time step,
# then composites the four panes into one 2x2 movie so all four reflectors
# advance through time together.
#
# Self-contained sibling of build_movie.sh; it does NOT touch the
# single-reflector movie or its files.
#
# Usage:
#   macros/build_movie_quad.sh [QUAD_BASE] [OUT_BASENAME] [FPS]
#
#   QUAD_BASE     default <repo>/report/movie_quad
#   OUT_BASENAME  default <QUAD_BASE>/muon_movie_quad   (-> .gif and .mp4)
#   FPS           default 12   (playback rate, not physical)
#
# Environment knobs (also forwarded to the macro):
#   MOVIE_END     hard-cap time of the LAST frame, ns     (default 5.00)
#   MOVIE_STEP    CONSTANT time between frames, ns        (default 0.03)
#   MOVIE_TAIL    fixed comet-tail width, ns (>= step)    (default 0.25)
#   REFLECTORS    panes to render                         (default "0 1 2 3")
#   USE_SEED      seed the RNG (same event every pane)    (default 1)
#   SEED1 SEED2   /random/setSeeds values                 (default 12345 67890)
#   SCALE         composite resize factor (GIF size knob) (default 1)
#   SKIP_SIM=1    reuse existing EPS frames (skip the 4 sims)
#   CLEAN=0       keep intermediate EPS/PNG/composite frames (default removed)
#   STEM          per-pane frame basename                 (default qframe)
#   BIN           path to G4DCSmonitor                    (default <repo>/build/G4DCSmonitor)
#
# Example:
#   macros/build_movie_quad.sh
#   SCALE=0.75 macros/build_movie_quad.sh          # smaller GIF/MP4
#   MOVIE_STEP=0.05 macros/build_movie_quad.sh     # coarser, fewer frames
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"

QUAD_BASE="${1:-${ROOTDIR}/report/movie_quad}"
OUT="${2:-${QUAD_BASE}/muon_movie_quad}"
FPS="${3:-12}"
STEM="${STEM:-qframe}"

MOVIE_END="${MOVIE_END:-5.00}"
MOVIE_STEP="${MOVIE_STEP:-0.03}"
MOVIE_TAIL="${MOVIE_TAIL:-0.25}"
REFLECTORS="${REFLECTORS:-0 1 2 3}"
USE_SEED="${USE_SEED:-1}"
SEED1="${SEED1:-12345}"
SEED2="${SEED2:-67890}"
SCALE="${SCALE:-1}"

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    if [[ ! -x "${BIN}" ]]; then
        echo "ERROR: binary not found: ${BIN} (build: cmake --build ${ROOTDIR}/build -j)" >&2
        exit 1
    fi
    for r in ${REFLECTORS}; do
        d="${QUAD_BASE}/refl${r}"
        echo "=== reflector ${r} -> ${d} (end ${MOVIE_END} ns, step ${MOVIE_STEP} ns) ==="
        mkdir -p "${d}"
        ( cd "${ROOTDIR}" && \
          MUON_REFLECTOR="${r}" QUAD_DIR="${d}" QUAD_STEM="${STEM}" \
          MOVIE_END="${MOVIE_END}" MOVIE_STEP="${MOVIE_STEP}" MOVIE_TAIL="${MOVIE_TAIL}" \
          USE_SEED="${USE_SEED}" SEED1="${SEED1}" SEED2="${SEED2}" \
          "${BIN}" macros/muon_movie_quad.mac )
    done
fi

shopt -s nullglob
for r in ${REFLECTORS}; do
    EPS=("${QUAD_BASE}/refl${r}/${STEM}"_*.eps)
    if (( ${#EPS[@]} == 0 )); then
        echo "ERROR: no EPS frames: ${QUAD_BASE}/refl${r}/${STEM}_*.eps" >&2
        echo "       run the simulation first (or drop SKIP_SIM=1)." >&2
        exit 1
    fi
    echo "=== rasterising ${#EPS[@]} EPS -> PNG (reflector ${r}) ==="
    mogrify -format png "${QUAD_BASE}/refl${r}/${STEM}"_*.eps
done

echo "=== compositing 2x2 panes ==="
if ! command -v python3 >/dev/null 2>&1; then
    echo "ERROR: python3 not found (needed to composite the 2x2 panes)" >&2
    exit 1
fi
python3 "${HERE}/montage_quad.py" "${QUAD_BASE}" "${QUAD_BASE}/frames" "${STEM}" "${SCALE}"

echo "=== building GIF: ${OUT}.gif ==="
DELAY=$(( 100 / FPS ))
convert -delay "${DELAY}" -loop 0 "${QUAD_BASE}/frames/quad"_*.png "${OUT}.gif"

if command -v ffmpeg >/dev/null 2>&1; then
    echo "=== building MP4: ${OUT}.mp4 ==="
    ffmpeg -y -loglevel error -framerate "${FPS}" -start_number 0 \
        -i "${QUAD_BASE}/frames/quad_%04d.png" \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -pix_fmt yuv420p "${OUT}.mp4"
else
    echo "(ffmpeg not found: skipping MP4)"
fi

echo "=== cleaning intermediate frames (CLEAN=0 to keep them) ==="
if [[ "${CLEAN:-1}" == "1" ]]; then
    rm -f "${QUAD_BASE}/refl"*/"${STEM}"_*.eps "${QUAD_BASE}/refl"*/"${STEM}"_*.png
    rm -f "${QUAD_BASE}/frames/quad"_*.png
fi

echo "=== done: ${OUT}.gif ${OUT}.mp4 ==="
