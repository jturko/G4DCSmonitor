#!/usr/bin/env bash
# =============================================================================
# build_movie_wls.sh -- Config-4 WLS movie of detected optical photons and
# their ancestors (pre-WLS creator photons + muon).
#
# Runs macros/muon_movie_wls.mac (preset 7 analytic fibres, glossy reflector
# by default), which prunes each event's trajectory container to the optical-
# photon tracks that produced a SiPM hit plus their full parent chain.  The
# sparse detected tracks and their pre-WLS creators are drawn green and swept
# with a wide comet tail, because WLS re-emission is exponentially delayed
# (tau ~ 7 ns) and detections can occur out to ~60-70 ns.  The camera starts
# wide to show the muon crossing and eases into the fibre/SiPM close-up.
#
# Usage:
#   macros/build_movie_wls.sh [WLS_BASE] [OUT_BASENAME] [FPS]
#
#   WLS_BASE      default <repo>/report/movie_wls
#   OUT_BASENAME  default <WLS_BASE>/muon_movie_wls   (-> .gif and .mp4)
#   FPS           default 12   (playback rate, not physical)
#
# Environment knobs (also forwarded to the macro):
#   MOVIE_END       hard-cap time of the LAST frame, ns     (default 80)
#   MOVIE_STEP      CONSTANT time between frames, ns        (default 0.25)
#   MOVIE_TAIL      fixed comet-tail width, ns (>= step)    (default 10)
#   MUON_REFLECTOR  0=none/bare 1=TiO2 2=Al foil 3=glossy   (default 3)
#   WLS_PRESET      7=analytic fibres 5=legacy tessellated    (default 7)
#   LY_SCALE        display-only light-yield scale
#                   (default 0.4 for preset 5, 0.1 for preset 7)
#   USE_SEED        seed the RNG (same event every run)     (default 1)
#   SEED1 SEED2     /random/setSeeds values                 (default 12345 67890)
#   SKIP_SIM=1      reuse existing EPS frames (skip the sim)
#   CLEAN=0         keep intermediate EPS/PNG frames (default removed)
#   TRIM=0          keep dark frames after the last light
#   STEM            frame basename inside WLS_BASE          (default wframe)
#   BIN             path to G4DCSmonitor                    (default <repo>/build/G4DCSmonitor)
#
# The OpenGL stored viewer needs an X11 display.  On this machine:
#   DISPLAY=:0 XAUTHORITY=/run/user/42382/.mutter-Xwaylandauth.MKK2W3 \
#   macros/build_movie_wls.sh
#
# Example:
#   macros/build_movie_wls.sh
#   MOVIE_STEP=0.1 MOVIE_TAIL=4 macros/build_movie_wls.sh   # finer, same span
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"

WLS_BASE="${1:-${ROOTDIR}/report/movie_wls}"
OUT="${2:-${WLS_BASE}/muon_movie_wls}"
FPS="${3:-12}"
STEM="${STEM:-wframe}"

MOVIE_END="${MOVIE_END:-80}"
MOVIE_STEP="${MOVIE_STEP:-0.25}"
MOVIE_TAIL="${MOVIE_TAIL:-10}"
MUON_REFLECTOR="${MUON_REFLECTOR:-3}"
WLS_PRESET="${WLS_PRESET:-7}"
if [[ -z "${LY_SCALE:-}" ]]; then
    if [[ "${WLS_PRESET}" == "5" ]]; then
        LY_SCALE=0.4
    else
        LY_SCALE=0.1
    fi
fi
USE_SEED="${USE_SEED:-1}"
SEED1="${SEED1:-12345}"
SEED2="${SEED2:-67890}"

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    if [[ ! -x "${BIN}" ]]; then
        echo "ERROR: binary not found: ${BIN} (build: cmake --build ${ROOTDIR}/build -j)" >&2
        exit 1
    fi
    echo "=== running simulation: macros/muon_movie_wls.mac ==="
    mkdir -p "${WLS_BASE}"
    ( cd "${ROOTDIR}" && \
      WLS_DIR="${WLS_BASE}" WLS_STEM="${STEM}" \
      MOVIE_END="${MOVIE_END}" MOVIE_STEP="${MOVIE_STEP}" MOVIE_TAIL="${MOVIE_TAIL}" \
      MUON_REFLECTOR="${MUON_REFLECTOR}" WLS_PRESET="${WLS_PRESET}" \
      LY_SCALE="${LY_SCALE}" \
      USE_SEED="${USE_SEED}" SEED1="${SEED1}" SEED2="${SEED2}" \
      "${BIN}" macros/muon_movie_wls.mac )
fi

shopt -s nullglob
EPS_FRAMES=("${WLS_BASE}/${STEM}"_*.eps)
if (( ${#EPS_FRAMES[@]} == 0 )); then
    echo "ERROR: no EPS frames found: ${WLS_BASE}/${STEM}_*.eps" >&2
    echo "       run the simulation first (or drop SKIP_SIM=1)." >&2
    exit 1
fi
echo "=== ${#EPS_FRAMES[@]} EPS frames in ${WLS_BASE} ==="

echo "=== rasterising EPS -> PNG ==="
mogrify -format png "${WLS_BASE}/${STEM}"_*.eps

if [[ "${TRIM:-1}" == "1" ]] && command -v python3 >/dev/null 2>&1; then
    echo "=== trimming dark tail ==="
    python3 "${HERE}/movie_trim.py" "${WLS_BASE}" "${STEM}" "${TRIM_PAD:-4}" green \
        || echo "(trim skipped)"
fi

echo "=== building GIF: ${OUT}.gif ==="
DELAY=$(( 100 / FPS ))
convert -delay "${DELAY}" -loop 0 "${WLS_BASE}/${STEM}"_*.png "${OUT}.gif"

if command -v ffmpeg >/dev/null 2>&1; then
    echo "=== building MP4: ${OUT}.mp4 ==="
    ffmpeg -y -loglevel error -framerate "${FPS}" -start_number 0 \
        -i "${WLS_BASE}/${STEM}_%04d.png" \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -pix_fmt yuv420p "${OUT}.mp4"
else
    echo "(ffmpeg not found: skipping MP4)"
fi

echo "=== cleaning intermediate frames (CLEAN=0 to keep them) ==="
if [[ "${CLEAN:-1}" == "1" ]]; then
    rm -f "${WLS_BASE}/${STEM}"_*.eps "${WLS_BASE}/${STEM}"_*.png
fi

echo "=== done: ${OUT}.gif ${OUT}.mp4 ==="
