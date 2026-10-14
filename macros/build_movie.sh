#!/usr/bin/env bash
# =============================================================================
# build_movie.sh -- assemble the muon_movie.mac frames into a GIF and an MP4.
#
# The Geant4 X11 OpenGL driver can only export VECTOR formats (eps/ps/pdf/svg),
# so macros/muon_movie.mac writes one rasterised EPS frame per time step.  This
# script rasterises those EPS frames to PNG (GraphicsMagick/Ghostscript), trims
# the photon-free tail, then stitches them into an animated GIF and an H.264 MP4
# (ffmpeg).
#
# Usage:
#   macros/build_movie.sh [FRAME_DIR] [OUT_BASENAME] [FPS]
#
#   FRAME_DIR    default report/movie
#   OUT_BASENAME default <FRAME_DIR>/muon_movie   (-> .gif and .mp4)
#   FPS          default 12                       (playback rate, not physical)
#
# Environment:
#   SKIP_SIM=1   do not run the simulation first (use the EPS frames already there)
#   CLEAN=0      keep the intermediate EPS/PNG frames (removed by default; the
#                EPS set is large, ~1.7 GB for the default ~380-frame sweep)
#   TRIM=0       keep the frames after the last light (default trims them; the
#                sweep runs to ~100 ns but events go dark by ~40-60 ns)
#   TRIM_PAD     dark frames kept after the last light frame (default 4)
#   STEM         frame basename inside FRAME_DIR  (default muon_frame)
#   BIN          path to the G4DCSmonitor binary  (default <repo>/build/G4DCSmonitor)
#
# Example:
#   macros/build_movie.sh                 # run the sim, then build gif+mp4
#   SKIP_SIM=1 macros/build_movie.sh      # just rebuild the movie from frames
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"

FRAME_DIR="${1:-${ROOTDIR}/report/movie}"
OUT="${2:-${FRAME_DIR}/muon_movie}"
FPS="${3:-12}"
STEM="${STEM:-muon_frame}"

if [[ "${SKIP_SIM:-0}" != "1" ]]; then
    if [[ ! -x "${BIN}" ]]; then
        echo "ERROR: binary not found: ${BIN} (build: cmake --build ${ROOTDIR}/build -j)" >&2
        exit 1
    fi
    echo "=== running simulation: macros/muon_movie.mac ==="
    ( cd "${ROOTDIR}" && "${BIN}" macros/muon_movie.mac )
fi

shopt -s nullglob
EPS_FRAMES=("${FRAME_DIR}/${STEM}"_*.eps)
if (( ${#EPS_FRAMES[@]} == 0 )); then
    echo "ERROR: no EPS frames found: ${FRAME_DIR}/${STEM}_*.eps" >&2
    echo "       run the simulation first (or drop SKIP_SIM=1)." >&2
    exit 1
fi
echo "=== ${#EPS_FRAMES[@]} EPS frames in ${FRAME_DIR} ==="

echo "=== rasterising EPS -> PNG ==="
mogrify -format png "${FRAME_DIR}/${STEM}"_*.eps

# The sweep is run out to a safe terminal time so every photon is gone, but most
# events go dark sooner; drop the photon-free remainder so the movie ends on the
# last light.  TRIM=0 disables; TRIM_PAD = dark frames kept after the last light.
if [[ "${TRIM:-1}" == "1" ]] && command -v python3 >/dev/null 2>&1; then
    echo "=== trimming dark tail ==="
    python3 "${HERE}/movie_trim.py" "${FRAME_DIR}" "${STEM}" "${TRIM_PAD:-4}" \
        || echo "(trim skipped)"
fi

echo "=== building GIF: ${OUT}.gif ==="
DELAY=$(( 100 / FPS ))
convert -delay "${DELAY}" -loop 0 "${FRAME_DIR}/${STEM}"_*.png "${OUT}.gif"

if command -v ffmpeg >/dev/null 2>&1; then
    echo "=== building MP4: ${OUT}.mp4 ==="
    # yuv420p + even dimensions so the file plays everywhere
    ffmpeg -y -loglevel error -framerate "${FPS}" -start_number 0 \
        -i "${FRAME_DIR}/${STEM}_%04d.png" \
        -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -pix_fmt yuv420p "${OUT}.mp4"
else
    echo "(ffmpeg not found: skipping MP4)"
fi

echo "=== cleaning intermediate frames (CLEAN=0 to keep them) ==="
if [[ "${CLEAN:-1}" == "1" ]]; then
    rm -f "${FRAME_DIR}/${STEM}"_*.eps "${FRAME_DIR}/${STEM}"_*.png
fi

echo "=== done: ${OUT}.gif ${OUT}.mp4 ==="
