#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
bin="${BIN:-$repo/build/G4DCSmonitor}"
gs_bin="${GS:-gs}"
dpi="${DPI:-150}"
outdir="$repo/report/eff_clyc/figures"

if [[ ! -x "$bin" ]]; then
  echo "error: executable not found: $bin" >&2
  exit 1
fi

mkdir -p "$outdir"

render_one() {
  local name="$1"
  local macro="$2"
  local log="/tmp/eff_vis_${name}.log"
  local raw
  local out="$outdir/fig_vis_${name}.pdf"

  "$bin" "$macro" >"$log" 2>&1

  raw="$(ls -t /tmp/eff_vis_${name}*.pdf 2>/dev/null | head -n 1 || true)"
  if [[ -z "$raw" ]]; then
    echo "error: no OGL export found for $name (see $log)" >&2
    exit 1
  fi

  "$gs_bin" -q -dNOPAUSE -dBATCH \
    -sDEVICE=pdfimage24 -r"$dpi" \
    -dFirstPage=1 -dLastPage=1 \
    -sOutputFile="$out" "$raw"

  echo "$name: $raw -> $out"
}

# Three-quarter cutaway of the half-sectioned detector with the flush puck.
# The OGL view is vertical; rotate -90 degrees so the source is at the left,
# matching the schematic, and trim the white margin.
render_flush_cutaway() {
  local macro="$repo/report/eff_clyc/vis/flush_cutaway.mac"
  local log="/tmp/eff_vis_flush_cutaway.log"
  local raw
  local png="/tmp/eff_vis_flush_cutaway_render"
  local out="$outdir/fig_vis_flush_cutaway.png"

  "$bin" "$macro" >"$log" 2>&1

  raw="$(ls -t /tmp/eff_vis_flush_cutaway*.pdf 2>/dev/null | head -n 1 || true)"
  if [[ -z "$raw" ]]; then
    echo "error: no OGL export found for flush_cutaway (see $log)" >&2
    exit 1
  fi

  pdftoppm -png -r 300 -singlefile "$raw" "$png"
  convert "$png.png" -trim +repage -bordercolor white -border 24 \
    -rotate -90 "$out"
  echo "flush_cutaway: $raw -> $out"
}

render_one side "$repo/report/eff_clyc/vis/eff_events_side.mac"
render_one oblique "$repo/report/eff_clyc/vis/eff_events_oblique.mac"
render_flush_cutaway

ls -lh "$outdir"/fig_vis_*
