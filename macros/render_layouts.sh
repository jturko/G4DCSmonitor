#!/usr/bin/env bash
# =============================================================================
# render_layouts.sh -- OpenGL overview renders of the five SiPM layouts.
#
# Writes report/figures/layout_ogl_c<C>.png for C = 1, 2, 3, 12 and, for the
# WLS layout, layout_ogl_c4.png as a read-out close-up.  The close-up uses the
# tessellated vis variant (preset 5), not the analytic production preset: with
# preset 7 the clamp and grease are G4SubtractionSolids cut by a union of
# G4CutTubs legs, and their visual polyhedron cannot be built, so the scene
# handler drops them to a point cloud (or, for the last cut, to nothing) --
# measured on G4 11.3.2: the clamp and ~86 % of the grease disappear.
# Also writes the five-panel montage layout_ogl_all.png.
#
# The four plan renders use the report plan view (top +y, right +x) seen from
# an oblique three-quarter view: camera theta 50 / phi -25 with upVector
# -1 0 0, plate at 35 % opacity so
# the sensors on the far edge show through.  `/vis/viewer/set/edge true` switches
# the TSG viewer to hlhsr, which draws the plate's silhouette and its sharp
# edges (surface style alone draws no lines at all in TSG).  Sensor
# labels follow the report convention (T1/T2 top, R1/R2 right, B1/B2 bottom,
# L1/L2 left); they sit outside the plate so the thickness band does not run
# through the letters, the bottom pair a little further out to clear the
# enlarged sensor rings.  Text is centre-justified on its coordinate and the
# scene extent is fixed, so all four plan panels share one camera and scale.
# The clipped-corner design uses its report-frame channel names BR/TR/TL/BL.
#
# Usage:
#   macros/render_layouts.sh
#   BIN=/abs/path/G4DCSmonitor macros/render_layouts.sh
# =============================================================================
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOTDIR="$(cd "${HERE}/.." && pwd)"
BIN="${BIN:-${ROOTDIR}/build/G4DCSmonitor}"
OUTDIR="${ROOTDIR}/report/figures"
TMP="$(mktemp -d)"

[[ -x "${BIN}" ]] || { echo "ERROR: binary not found: ${BIN}" >&2; exit 1; }
mkdir -p "${OUTDIR}"

PREAMBLE='# -----------------------------------------------------------------------------
/run/numberOfThreads 1
/dcs-monitor/det/setPosition 0 0 0 mm
/dcs-monitor/det/muonScint/add
/dcs-monitor/det/muonScint/setSize 250 250 10 mm
/dcs-monitor/det/muonScint/preset @PRESET@
/dcs-monitor/det/muonScint/setReflector 3
/dcs-monitor/det/muonScint/setWrap true
/dcs-monitor/det/muonScint/setEdgeReflector same
/dcs-monitor/det/muonScint/setCouplingIndex 1.55
/dcs-monitor/det/muonScint/setClampSurface white
/dcs-monitor/det/muonScint/setBareContactFraction 0.045
/dcs-monitor/det/muonScint/setAlBacking air
/run/initialize
/vis/open TSG_OFFSCREEN 2000x1500
/vis/tsg/offscreen/set/format zb_png
/vis/tsg/offscreen/set/size 1000 1000
/vis/verbose errors
/vis/viewer/set/autoRefresh false
/vis/viewer/set/background white
/vis/drawVolume
/vis/viewer/set/style surface
/vis/viewer/set/edge true
/vis/viewer/set/auxiliaryEdge false
/vis/viewer/set/upVector -1 0 0
/vis/viewer/set/lightsVector 0.5 0 1
/vis/tsg/offscreen/set/transparency true
/vis/geometry/set/colour MuonScintLV 0 0.45 0.75 1.0 0.35
/vis/geometry/set/visibility GreaseLV 0 false
/vis/viewer/set/viewpointThetaPhi 50 -25
/vis/viewer/set/targetPoint 0 0 0 mm
/vis/set/textColour black
/vis/set/textLayout centre
/vis/scene/add/text 0 0 6 mm 34 0 0 @TITLE@'

# ---- plan views: config 1, 2, 3, 12 ------------------------------------------
run_plan() {
    local cfg="$1" preset="$2" title="$3"; shift 3
    local mac="${TMP}/plan_c${cfg}.mac" png="${OUTDIR}/layout_ogl_c${cfg}.png"
    {
        echo "${PREAMBLE}" | sed "s/@PRESET@/${preset}/; s/@TITLE@/${title}/"
        for l in "$@"; do echo "$l"; done
        # A fixed scene extent makes the auto-framing independent of the label
        # set, so every plan panel has exactly the same scale (auto-framing on
        # Config 3's two labels magnified it by ~13 %).
        echo "/vis/scene/add/extent -160 160 -160 160 -12 12 mm"
        echo "/vis/viewer/zoomTo 1.18"
        echo "/vis/tsg/offscreen/set/file ${png}"
        echo "/vis/viewer/flush"
        echo "/vis/verbose confirmations"
    } > "${mac}"
    "${BIN}" "${mac}" > "${TMP}/log_c${cfg}.txt" 2>&1 \
        || { echo "ERROR: render failed for config ${cfg} (${TMP}/log_c${cfg}.txt)" >&2; return 1; }
    [[ -s "${png}" ]] || { echo "ERROR: ${png} missing/empty" >&2; return 1; }
    echo "wrote ${png}"
}

# Label coordinates are in simulation coordinates: report (X,Y) = sim (y,-x),
# so report top (+y) is sim -X and report right (+x) is sim +Y.  The 10 mm
# offset beyond the plate edge (152, or 160 for the bottom pair to clear
# the enlarged sensor rings) keeps the labels off the 3D thickness band and
# the tilted silhouette.
run_plan 1 1 "Config 1" \
    "/vis/scene/add/text -152  62.5 6 mm 26 0 0 T1" \
    "/vis/scene/add/text -152 -62.5 6 mm 26 0 0 T2" \
    "/vis/scene/add/text   62.5 152 6 mm 26 0 0 R1" \
    "/vis/scene/add/text  -62.5 152 6 mm 26 0 0 R2" \
    "/vis/scene/add/text  166  62.5 6 mm 26 0 0 B2" \
    "/vis/scene/add/text  166 -62.5 6 mm 26 0 0 B1" \
    "/vis/scene/add/text  -62.5 -152 6 mm 26 0 0 L1" \
    "/vis/scene/add/text   62.5 -152 6 mm 26 0 0 L2"

run_plan 2 2 "Config 2" \
    "/vis/scene/add/text -152  62.5 6 mm 26 0 0 T1" \
    "/vis/scene/add/text   62.5 152 6 mm 26 0 0 R1" \
    "/vis/scene/add/text  166 -62.5 6 mm 26 0 0 B1" \
    "/vis/scene/add/text  -62.5 -152 6 mm 26 0 0 L1"

run_plan 3 3 "Config 3" \
    "/vis/scene/add/text -152  62.5 6 mm 26 0 0 T1" \
    "/vis/scene/add/text -152 -62.5 6 mm 26 0 0 T2"

run_plan 12 12 "Config 2 (mod)" \
    "/vis/scene/add/text  150  150 6 mm 26 0 0 BR" \
    "/vis/scene/add/text -150  150 6 mm 26 0 0 TR" \
    "/vis/scene/add/text -150 -150 6 mm 26 0 0 TL" \
    "/vis/scene/add/text  150 -150 6 mm 26 0 0 BL"

# ---- WLS read-out close-up (tessellated vis variant, preset 5) --------------
C4="${OUTDIR}/layout_ogl_c4.png"
{
    echo "${PREAMBLE}" | sed "s/@PRESET@/5/; s/@TITLE@//"
    cat <<'EOF'
/vis/viewer/set/edge true
/vis/viewer/set/upVector 0 0 1
/vis/viewer/set/viewpointThetaPhi 60 285
/vis/viewer/set/targetPoint -125.8 -108 0 mm
/vis/viewer/zoomTo 15
EOF
    echo "/vis/tsg/offscreen/set/file ${C4}"
    echo "/vis/viewer/flush"
    echo "/vis/verbose confirmations"
} > "${TMP}/plan_c4.mac"
"${BIN}" "${TMP}/plan_c4.mac" > "${TMP}/log_c4.txt" 2>&1 \
    || { echo "ERROR: render failed for config 4 (${TMP}/log_c4.txt)" >&2; }
[[ -s "${C4}" ]] && echo "wrote ${C4}"

# ---- post-processing --------------------------------------------------------
# A 4 mm sensor is only a thin tick in a 250 mm plan view, so ring every
# rendered sensor (found by its red colour) with a marker of ~10 px radius and
# annotate the Config 4 close-up with leader lines.  The rings are schematic,
# like the 2.5x oversized sensor markers of macros/sipm_layouts.C.
python3 - "${OUTDIR}" <<'PY'
import sys, numpy as np
from PIL import Image, ImageDraw, ImageFont

out = sys.argv[1]
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

def blob_centres(im):
    """Centres of the red (SiPM) blobs, greedily clustered."""
    a = np.asarray(im.convert("RGB")).astype(int)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    ys, xs = np.nonzero((r > 140) & (g < 90) & (b < 90))
    pts = list(zip(xs.tolist(), ys.tolist()))
    groups = []
    for p in pts:
        for grp in groups:
            if abs(grp[0][0] - p[0]) < 25 and abs(grp[0][1] - p[1]) < 25:
                grp.append(p)
                break
        else:
            groups.append([p])
    return [(int(np.mean([q[0] for q in g])), int(np.mean([q[1] for q in g])))
            for g in groups if len(g) >= 8]

for cfg in (1, 2, 3, 12):
    p = f"{out}/layout_ogl_c{cfg}.png"
    im = Image.open(p).convert("RGB")
    d = ImageDraw.Draw(im)
    cs = blob_centres(im)
    for cx, cy in cs:
        d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], outline=(190, 0, 0), width=2)
    im.save(p)
    print(f"marked {len(cs)} sensors in {p}")

p = f"{out}/layout_ogl_c4.png"
im = Image.open(p).convert("RGB")
d = ImageDraw.Draw(im)
f = ImageFont.truetype(FONT, 28)
for xy, text in (((20, 842), "SiPM"), ((20, 527), "4 Y-11 fibres"),
                 ((20, 217), "white clamp")):
    d.text(xy, text, fill=(0, 0, 0), font=f)
d.line([140, 862, 236, 872], fill=(0, 0, 0), width=1)
d.line([300, 540, 505, 470], fill=(0, 0, 0), width=1)
d.line([300, 250, 505, 225], fill=(0, 0, 0), width=1)
d.text((20, 52), "Config 4 - WLS read-out (close-up)", fill=(0, 0, 0), font=f)
im.save(p)
print(f"annotated {p}")
PY

# ---- montage: 2 x 3, five panels, last cell white ---------------------------
# Each render has a wide white margin (the camera frame is square while the
# plate is not).  The four plan views share one camera, so crop them to ONE
# common content box and scale them by ONE factor: fitting each panel to its
# own bounding box magnifies Config 3 (fewer labels) by ~13 % relative to the
# rest.  Config 4 is a genuine close-up at a different magnification and keeps
# its own fit.
python3 - "${OUTDIR}" <<'PY2'
import sys
from PIL import Image, ImageChops, ImageDraw, ImageFont

out = sys.argv[1]
CELL, GAP = 520, 22
WHITE = (255, 255, 255)

def content_box(im):
    bg = Image.new("RGB", im.size, WHITE)
    return ImageChops.difference(im, bg).getbbox() or (0, 0, im.width, im.height)

plans, boxes = [], []
for c in (1, 2, 3, 12):
    im = Image.open(f"{out}/layout_ogl_c{c}.png").convert("RGB")
    plans.append(im)
    boxes.append(content_box(im))
PAD = 12
x0 = max(0, min(b[0] for b in boxes) - PAD)
y0 = max(0, min(b[1] for b in boxes) - PAD)
x1 = min(plans[0].width, max(b[2] for b in boxes) + PAD)
y1 = min(plans[0].height, max(b[3] for b in boxes) + PAD)
cw, ch = x1 - x0, y1 - y0
k = min(CELL / cw, CELL / ch)
tiles = [im.crop((x0, y0, x1, y1)).resize(
             (max(1, int(cw * k)), max(1, int(ch * k))), Image.LANCZOS)
         for im in plans]

im4 = Image.open(f"{out}/layout_ogl_c4.png").convert("RGB")
im4 = im4.crop(content_box(im4))
k4 = min(CELL / im4.width, CELL / im4.height)
tiles.append(im4.resize((max(1, int(im4.width * k4)),
                         max(1, int(im4.height * k4))), Image.LANCZOS))
tiles.append(None)                      # empty sixth cell

# Lay the rows out by their actual tile heights.  Pasting the letterboxed plan
# tiles into fixed 520-px cells leaves ~70 px of white above and below each,
# which doubles the gutters and pushes the whole block against the bottom.
rows = [tiles[0:2], tiles[2:4], tiles[4:6]]
row_h = [max(t.height for t in row if t is not None) for row in rows]
W = 2 * CELL + 3 * GAP
H = sum(row_h) + (len(rows) + 1) * GAP
sheet = Image.new("RGB", (W, H), WHITE)
y = GAP
for row, h in zip(rows, row_h):
    for i, t in enumerate(row):
        if t is None:
            continue
        cx = GAP + i * (CELL + GAP)
        sheet.paste(t, (cx + (CELL - t.width) // 2, y + (h - t.height) // 2))
    y += h + GAP
# note in the otherwise empty cell
d = ImageDraw.Draw(sheet)
f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20)
tx = GAP + CELL + GAP + 10
ty = GAP + row_h[0] + GAP + row_h[1] + GAP + 20
for line in ("SiPMs are 4 x 4 mm; the rings",
             "are drawn enlarged so they",
             "stay visible at print size.",
             "",
             "Plan panels share one scale",
             "(report frame, top +y,",
             "right +x, tilted 50 and yawed",
             "25 deg to the left).",
             "Config 4 is a close-up of",
             "the WLS read-out on -x."):
    d.text((tx, ty), line, fill=(60, 60, 60), font=f)
    ty += 26

p = f"{out}/layout_ogl_all.png"
sheet.save(p)
print(f"wrote {p}")
PY2

rm -rf "${TMP}"
