#!/usr/bin/env bash
# run_opt_recon.sh -- position-reconstruction sweep over the v7_opt designs.
#
# The proven tool macros/position_reconstruction.C+ already knows the SiPM
# layouts (presets 1, 8, 10 and 11), but it reads a four-reflector
# directory:
#   <dir>/muon_whole_out_c<cfg>_r0.root ... _r3.root
# and reports one result per wrapper.  The v7_opt campaign has one whole-plate
# file per *design*, so for every design this script builds a scratch directory
# containing four symlinks to the SAME campaign file, named r0..r3.  All four
# wrapper blocks then describe the identical design; only the r0 ("bare") block
# is kept in the combined CSV.  No source change is made to
# position_reconstruction.C.
#
# Usage:
#   macros/run_opt_recon.sh [DATA_DIR] [OUTDIR] [SCRATCH] [NEVENTS]
#
#   DATA_DIR  directory holding muon_whole_out_c<cfg>_r<refl>_<tag>.root
#             (default: the v7_opt whole tree in ~/bigdata; READ-ONLY)
#   OUTDIR    directory for the per-design diagnostic figures (default
#             report/figures; each design gets its own OUTDIR/<tag>/ subdir)
#   SCRATCH   working area (symlinks, per-design outputs, combined raw CSV);
#             default /tmp/opencode/recon.  Left in place after the run.
#   NEVENTS   events per design (default 25000 = all events in each file)
#
# Output:
#   $SCRATCH/opt_recon_raw.csv   tag,cfg,preset + the r0 rows of
#                                position_reconstruction.csv (5 estimators)
#   $SCRATCH/out/<tag>/          all five figures + the three CSVs of the
#                                underlying macro for that design
#   $OUTDIR/<tag>/               copy of the same diagnostic figures
#
# The design table below mirrors macros/opt_designs.h (the source of truth);
# the corner-cut design is a standard Config-2 mod and is not a v7_opt row.
# Fields: tag cfg refl preset.
#
# Run from anywhere; the script cd's to the repository root because the macro
# is passed as the relative path macros/position_reconstruction.C+.
#
# The ACLiC build and the k-NN regression are compute-bound.  Measured on this
# login node: 2500 events ~ 4.3 s, 25000 events ~ 45 s per design.

set -euo pipefail

# Numeric formatting is parsed with '.' as the decimal separator below.
export LC_ALL=C

DATA_DIR="${1:-/home/turko46/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole}"
OUTDIR="${2:-report/figures}"
SCRATCH="${3:-/tmp/opencode/recon}"
NEVENTS="${4:-25000}"

ROOT_BIN="${ROOT_BIN:-$HOME/programs/root/build/bin/root}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ ! -x "$ROOT_BIN" ]; then
    echo "run_opt_recon.sh: ROOT binary not found/executable: $ROOT_BIN" >&2
    exit 1
fi

# tag cfg refl preset   (manifest order; refl only locates the file)
DESIGNS=(
    "c1_split_black    1  3  1"
    "c8_split_black    8  3  8"
    "c10_wls4_tio2    10  1 10"
    "c11_wls8_tio2    11  1 11"
    "c1_edge_diffuse   1  3  1"
    "c1_6mm_diffuse    1  3  1"
    "c8_6mm_diffuse    8  3  8"
    "c10_wls4_glossy  10  3 10"
    "c11_wls8_glossy  11  3 11"
    "c1_same_1p55      1  3  1"
)

mkdir -p "$SCRATCH" "$SCRATCH/out" "$OUTDIR"
RAW="$SCRATCH/opt_recon_raw.csv"
printf 'tag,cfg,preset,wrapper,estimator,neighbours,ntrain,ntest,sigma_x_mm,sigma_y_mm,sigma_r_mm,bias_x_mm,bias_y_mm,r68_mm\n' > "$RAW"

cd "$REPO_ROOT"

echo "run_opt_recon.sh: DATA_DIR=$DATA_DIR"
echo "                  OUTDIR=$OUTDIR  SCRATCH=$SCRATCH  NEVENTS=$NEVENTS"
echo "                  ROOT=$ROOT_BIN"
echo

n_ok=0
n_skip=0
fail=0

for spec in "${DESIGNS[@]}"; do
    read -r tag cfg refl preset <<< "$spec"
    src="$DATA_DIR/muon_whole_out_c${cfg}_r${refl}_${tag}.root"

    if [ ! -s "$src" ]; then
        printf '  SKIP %-20s  (missing or empty: %s)\n' "$tag" "$src" >&2
        n_skip=$((n_skip + 1))
        continue
    fi
    # A TTree writer leaves a small partial file for the position currently
    # being written; reject anything ROOT cannot open as a live TFile.
    if ! "$ROOT_BIN" -l -b -q -e \
            "TFile p(\"$src\"); if (p.IsZombie() || !p.IsOpen()) gSystem->Exit(1);" \
            >/dev/null 2>&1; then
        printf '  SKIP %-20s  (ROOT cannot open: %s)\n' "$tag" "$src" >&2
        n_skip=$((n_skip + 1))
        continue
    fi

    work="$SCRATCH/$tag"
    out="$SCRATCH"/out/"$tag"
    mkdir -p "$work" "$out"

    # Four names -> the same design file.  Absolute targets so the symlinks
    # survive being read from any cwd.
    for r in 0 1 2 3; do
        ln -sf "$src" "$work/muon_whole_out_c${cfg}_r${r}.root"
    done

    printf '  RUN  %-20s  preset %-2s  %s\n' "$tag" "$preset" "$src"
    t0=$SECONDS
    if ! "$ROOT_BIN" -l -b -q \
            "macros/position_reconstruction.C+(\"$work\",\"$out\",$NEVENTS,0,\"\",$preset)"; then
        printf '  FAIL %-20s  position_reconstruction.C returned non-zero\n' "$tag"
        fail=1
        continue
    fi
    dt=$((SECONDS - t0))

    csv="$out/position_reconstruction.csv"
    if [ ! -s "$csv" ]; then
        printf '  FAIL %-20s  no %s\n' "$tag" "$csv"
        fail=1
        continue
    fi

    # All four wrappers read the same file, so they must agree exactly.  A
    # mismatch means the symlink workaround did not do what it should.
    mismatch=$(awk -F, 'NR > 1 {
            key = $2
            if (!(key in x)) { x[key] = $6; n[key] = 1 }
            else if ($6 != x[key]) { bad = 1 }
        } END { print bad + 0 }' "$csv")
    if [ "$mismatch" != "0" ]; then
        printf '  FAIL %-20s  wrapper rows disagree (same file!) -- stopping\n' "$tag"
        fail=1
        break
    fi

    # r0 rows only, prefixed with the design identity.
    awk -F, -v tag="$tag" -v cfg="$cfg" -v pre="$preset" \
        'NR > 1 && $1 == "bare" { print tag "," cfg "," pre "," $0 }' \
        "$csv" >> "$RAW"

    combined=$(awk -F, '$1 == "bare" && $2 == "combined" { print $6 }' "$csv")
    printf '  DONE %-20s  sigma_x(combined) = %6.2f mm  (%d s)\n' \
        "$tag" "$combined" "$dt"

    # Keep the per-design diagnostic figures next to the report figures too.
    mkdir -p "$OUTDIR/$tag"
    cp -f "$out"/fig_recon_*.pdf "$out"/fig_recon_*.png "$OUTDIR/$tag/" 2>/dev/null || true

    n_ok=$((n_ok + 1))
    echo
done

echo "run_opt_recon.sh: $n_ok design(s) processed, $n_skip skipped"
echo "                 raw table: $RAW ($(($(wc -l < "$RAW") - 1)) rows)"

if [ "$fail" -ne 0 ]; then
    echo "run_opt_recon.sh: at least one design failed -- see above" >&2
    exit 1
fi
if [ "$n_ok" -eq 0 ]; then
    echo "run_opt_recon.sh: no designs processed" >&2
    exit 1
fi
