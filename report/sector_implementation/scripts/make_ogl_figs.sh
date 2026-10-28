#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
bin="${BIN:-$repo/build/G4DCSmonitor}"
gs_bin="${GS:-gs}"
dpi="${DPI:-120}"
outdir="$repo/report/sector_implementation/figures"

if [[ ! -x "$bin" ]]; then
  echo "error: executable not found: $bin" >&2
  exit 1
fi

mkdir -p "$outdir"

render_one() {
  local name="$1"
  local macro="$2"
  local log="/tmp/dcs_report_ogl_${name}.log"
  local raw
  local out="$outdir/fig_ogl_${name}.pdf"

  "$bin" "$macro" >"$log" 2>&1

  raw="$(ls -t /tmp/dcs_report_ogl_${name}_*.pdf 2>/dev/null | head -n 1 || true)"
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

render_one top "$repo/report/sector_implementation/vis/ogl_full_top.mac"
render_one oblique "$repo/report/sector_implementation/vis/ogl_full_oblique.mac"

ls -lh "$outdir"/fig_ogl_*.pdf
