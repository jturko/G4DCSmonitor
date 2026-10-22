#!/usr/bin/env bash
# =============================================================================
# Generate the MuonScint Phase-2 optimization whole-slab macros.
#
# Emits the ten designs of the report campaign (nine candidates plus the
# coupling control) and the corner-cut variant (11 macros), writing them to
# slurm/muon_whole_opt/ and producing a manifest for run_opt_whole.sbatch.
#
#   DESIGN    : human-readable tag (also the ROOT filename token)
#   PRESET    : geometry preset (1, 8, 9, 10, 11, 12)
#   REFLECTOR : 0=none, 1=TiO2, 2=Al, 3=glossy
#   EDGEREFL  : same|black|diffuse|mirror
#   COUPINDEX : coupling-grease refractive index
#   CLAMPSURF : white|black|mirror (relevant for WLS presets; harmless otherwise)
#   SIPMSIZE  : optional "6 6 0.6 mm" override
#
# Each macro irradiates the full 250 x 250 mm^2 plate and requests @THREADS@
# Geant4 threads (must match the SLURM --cpus-per-task). The collection-
# efficiency map is reconstructed offline from the sipmHits tree.
#
# Usage:  slurm/generate_opt_whole.sh [NEVENTS] [THREADS] [DATADIR]
#         (default NEVENTS=10000, THREADS=32,
#          DATADIR=/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole)
#
# DATADIR is written verbatim into the macros; the HPC-native absolute path
# /bigdata/DCS-Monitor-II/turko46/MuonScinti is used by default.
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMPLATE="${HERE}/whole_opt_template.mac"
OUTDIR="${HERE}/muon_whole_opt"
DATADIR="${3:-/bigdata/DCS-Monitor-II/turko46/MuonScinti/data_v7_opt/whole}"
NEVENTS="${1:-10000}"
THREADS="${2:-32}"

# Load nominal surface calibration values, if available.
ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.045}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

if [[ ! -f "${TEMPLATE}" ]]; then
    echo "ERROR: template not found: ${TEMPLATE}" >&2
    exit 1
fi
mkdir -p "${OUTDIR}"
# Do NOT create DATADIR here: macro generation usually runs on the login node,
# where the HPC-native /bigdata root is not writable.  The sbatch drivers
# create the data directory on the compute node.

# Manifest: one macro basename per line, deterministic 0-indexed array order.
MANIFEST="${OUTDIR}/manifest.txt"
: > "${MANIFEST}"

# Helper: emit one macro from the current positional parameters.
#   $1 tag  $2 preset  $3 reflector  $4 edgeRefl  $5 coupIndex  $6 clampSurf  $7 sipmSize
emit_macro() {
    local tag="$1" preset="$2" refl="$3" edge="$4" coup="$5" clamp="$6" sipm="$7"
    local out="muon_whole_opt_${tag}.mac"

    local sipm_line
    if [[ -n "${sipm}" ]]; then
        sipm_line="/dcs-monitor/det/muonScint/setSiPMSize ${sipm}"
    else
        sipm_line="# /dcs-monitor/det/muonScint/setSiPMSize 4 4 0.6 mm"
    fi

    sed -e "s/@TAG@/${tag}/g"    \
        -e "s/@CONFIG@/${preset}/g" \
        -e "s/@PRESET@/${preset}/g" \
        -e "s/@REFLECTOR@/${refl}/g" \
        -e "s/@EDGEREFL@/${edge}/g" \
        -e "s/@COUPINDEX@/${coup}/g" \
        -e "s/@CLAMPSURF@/${clamp}/g" \
        -e "s|@SIPMSIZE@|${sipm_line}|g" \
        -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
        -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
        -e "s#@DATADIR@#${DATADIR}#g" \
        -e "s/@NEVENTS@/${NEVENTS}/g" \
        -e "s/@THREADS@/${THREADS}/g" \
        "${TEMPLATE}" > "${OUTDIR}/${out}"

    printf '%s\n' "${out}" >> "${MANIFEST}"
}

i=0
# Stem 1: position-resolution candidates
emit_macro c1_split_black   1  3 black   1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c1_split_black
emit_macro c8_split_black   8  3 black   1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c8_split_black
emit_macro c10_wls4_tio2    10 1 same    1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c10_wls4_tio2
emit_macro c11_wls8_tio2    11 1 same    1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c11_wls8_tio2

# Stem 2: max-PE and Pareto candidates
emit_macro c1_edge_diffuse  1  3 diffuse 1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c1_edge_diffuse
emit_macro c1_6mm_diffuse   1  3 diffuse 1.55 white "6 6 0.6 mm"
printf 'array %2d -> %s\n' "$((i++))" c1_6mm_diffuse
emit_macro c8_6mm_diffuse   8  3 diffuse 1.55 white "6 6 0.6 mm"
printf 'array %2d -> %s\n' "$((i++))" c8_6mm_diffuse
emit_macro c10_wls4_glossy  10 3 same    1.55 mirror ""
printf 'array %2d -> %s\n' "$((i++))" c10_wls4_glossy
emit_macro c11_wls8_glossy 11  3 same    1.55 mirror ""
printf 'array %2d -> %s\n' "$((i++))" c11_wls8_glossy

# Control: the v7 production layout at the new coupling index.  Without it the
# difference between the data_v7 numbers (edge "same", n_coup 1.46) and the new
# designs mixes the coupling step with the design knob, because setCouplingIndex
# 1.55 is applied to every design above.
emit_macro c1_same_1p55     1  3 same    1.55 white ""
printf 'array %2d -> %s\n' "$((i++))" c1_same_1p55

# Corner-cut variant of Config 2: the four slab corners are truncated at 45 deg
# (12 mm leg, preset 12), with one 4 x 4 mm SiPM on each chamfered face.
emit_macro c2_cornercut     12 3 same    1.55 white "4 4 0.6 mm"
printf 'array %2d -> %s\n' "$((i++))" c2_cornercut

echo
echo "Wrote $(wc -l < "${MANIFEST}") macros to ${OUTDIR}/"
echo "  manifest=${MANIFEST}"
echo "  nevts=${NEVENTS}, threads=${THREADS}, datadir=${DATADIR}"
