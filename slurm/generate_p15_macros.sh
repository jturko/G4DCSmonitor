#!/usr/bin/env bash
# =====================================================================
# generate_p15_macros.sh -- p1.5 background-sector production macros
#
# One macro per (baseFuel, source, cask, hexant) config.  Each macro
# builds the 12-cask array + concrete hall (NO detector placed) once,
# then runs its config as NCHUNKS blocks of CHUNK_S seconds each
# (default: 180 s per config in 10 x 18 s chunks; 18 s chunks are
# ~1.4 GB for gamma 662 keV, while a single 2.7 GB file proved
# unreadable in the sector validation -- report/sector_implementation).
#
# Manifest line format (line i -> array task i-1):
#   run_baseFuel<B>_<SRC>_cask<C>_gFuel<G>.mac|<CFGDIR>|<STEM>|<NCHUNKS>
#   CFGDIR = absolute per-config output dir (chunk files + merged file)
#   STEM   = absolute merged-file stem, no .root
#
# Env knobs:
#   MACDIR       (required) output dir for the macros + manifest.txt
#   DATADIR      (required) absolute data root baked into the macros
#   P1_BASE_DIR  default /home/turko46/data/SurfaceFlux_v3 (HPC home)
#   MEAS_TIME    default 180     (seconds per config)
#   CHUNK_S      default 18      (seconds per chunk; must divide MEAS_TIME)
#   THREADS      default 32      (/run/numberOfThreads; keep equal to the
#                                sbatch --cpus-per-task)
#   BASES_ONLY / CASKS_ONLY / SOURCES_ONLY / HEXANTS_ONLY
#                comma lists restricting generation (used by run_local.sh)
#
# Sources (decay rates from ZLN_setup_v4; sourceType tags are free labels,
# set to each p1 generation's nShells value):
#   label            p1 tag                     nShells  decayRate  sourceType
#   neutron_Watt     neutron-Watt-a-0p85-b-3p4  10       4.16e6     10
#   gamma_662keV     gamma-662keV               28       1.4e14     28
#   gamma_1173keV    gamma-1173keV              22       1.0e11     22
#   gamma_1332keV    gamma-1332keV              20       1.0e11     20
#   isotope_Eu154    isotope-Eu154              25       7.0e10     25
#
# Neutron is first on purpose: array tasks run roughly in index order,
# longest tasks first minimises the tail of the campaign.
# =====================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Single source of truth for the (baseFuel, hexantRot) -> globalFuel map:
# the repo's orbit table, identical to the ZLN_setup_v4 copy (diff-verified).
# shellcheck source=../examples/p2-detector_response/_orbit_map.sh
. "${SCRIPT_DIR}/../examples/p2-detector_response/_orbit_map.sh"

MACDIR="${MACDIR:?export MACDIR (dir for macros + manifest.txt)}"
DATADIR="${DATADIR:?export DATADIR (absolute data root baked into the macros)}"
P1_BASE_DIR="${P1_BASE_DIR:-/home/turko46/data/SurfaceFlux_v3}"
MEAS_TIME="${MEAS_TIME:-180}"
CHUNK_S="${CHUNK_S:-18}"
THREADS="${THREADS:-32}"

# NCHUNKS = MEAS_TIME / CHUNK_S, exact division required
NCHUNKS="$(awk -v t="${MEAS_TIME}" -v c="${CHUNK_S}" \
    'BEGIN { if (c <= 0 || c > t) { print "bad chunk seconds" > "/dev/stderr"; exit 1 }
             r = t / c; if (r != int(r)) { print "CHUNK_S must divide MEAS_TIME" > "/dev/stderr"; exit 2 }
             print int(r) }')"

BASE_FUELS=(0 1 2 3 5 6 7 8 13 14 15 22 23 32)   # 14 assemblies carrying sources
ROTS=(0 60 120 180 240 300)                      # 6 hexants, degrees
CASKS=(1 2 3 4 5 6 7 8 9 10 11)                  # background casks; cask 0 = target (passive)

# label|p1 tag|nShells|decayRate|sourceType
SOURCES=(
    "neutron_Watt|neutron-Watt-a-0p85-b-3p4|10|4.16e6|10"
    "gamma_662keV|gamma-662keV|28|1.4e14|28"
    "gamma_1173keV|gamma-1173keV|22|1.0e11|22"
    "gamma_1332keV|gamma-1332keV|20|1.0e11|20"
    "isotope_Eu154|isotope-Eu154|25|7.0e10|25"
)

# optional restrictions (comma lists)
expand_list() {
    local IFS=','
    local -a out=()
    read -ra out <<< "${1-}"
    printf '%s\n' "${out[@]-}"
}

WANT_BASES="$(expand_list "${BASES_ONLY:-}" || true)"
WANT_CASKS="$(expand_list "${CASKS_ONLY:-}" || true)"
WANT_SOURCES="$(expand_list "${SOURCES_ONLY:-}" || true)"
WANT_ROTS="$(expand_list "${HEXANTS_ONLY:-}" || true)"

want() {  # want <needle> <haystack-multiline>
    local n="$1"
    [ -z "$2" ] && return 0
    printf '%s\n' "$2" | grep -qx -- "$n"
}

CHUNK_IDX_FMT="%0${#NCHUNKS}d"

mkdir -p "${MACDIR}"
MANIFEST="${MACDIR}/manifest.txt"
MANIFEST_TMP="${MANIFEST}.tmp"
: > "${MANIFEST_TMP}"

N_TOTAL=0
N_GENERATED=0
declare -A N_PER_SOURCE=()
declare -A N_MISSING=()

for SRC_LINE in "${SOURCES[@]}"; do
    IFS='|' read -r SRC TAG NSHELLS RATE STYPE <<< "${SRC_LINE}"
    want "${SRC}" "${WANT_SOURCES}" || continue
    # sourcePid is a no-op filter for these pid-homogeneous p1 files (ZLN v4 sets 0)
    SOURCE_PID=0
    case "${SRC}" in neutron_*) SOURCE_PID=2112 ;; esac

    for BB in "${BASE_FUELS[@]}"; do
        want "${BB}" "${WANT_BASES}" || continue
        BBP="$(printf '%02d' "${BB}")"
        # p1 file: unpadded fuel index in the SurfaceFlux_v3 names
        GLOB="${P1_BASE_DIR}/${SRC}/using/surface_${TAG}_fuel${BB}_nShells${NSHELLS}_nEvts*.root"
        MAPFILE=()
        if compgen -G "${GLOB}" > /dev/null; then
            mapfile -t MAPFILE < <(compgen -G "${GLOB}")
        fi
        if [ "${#MAPFILE[@]}" -eq 0 ]; then
            echo "WARNING: no p1 file for ${SRC} baseFuel ${BB}: ${GLOB}" >&2
            N_MISSING[${SRC}]=$(( ${N_MISSING[${SRC}]:-0} + 1 ))
            continue
        fi
        if [ "${#MAPFILE[@]}" -gt 1 ]; then
            echo "WARNING: ambiguous p1 files for ${SRC} baseFuel ${BB}; taking first of:" >&2
            printf '  %s\n' "${MAPFILE[@]}" >&2
        fi
        SRC_FILE="${MAPFILE[0]}"
        BASENAME="$(basename "${SRC_FILE}")"
        if [[ "${BASENAME}" =~ nEvts([0-9]+)\.root$ ]]; then
            NEVTS="${BASH_REMATCH[1]}"
        else
            echo "WARNING: cannot read nEvts from ${BASENAME}; skipping ${SRC} baseFuel ${BB}" >&2
            N_MISSING[${SRC}]=$(( ${N_MISSING[${SRC}]:-0} + 1 ))
            continue
        fi

        for C in "${CASKS[@]}"; do
            want "${C}" "${WANT_CASKS}" || continue
            for ROT in "${ROTS[@]}"; do
                want "${ROT}" "${WANT_ROTS}" || continue
                GG="$(lookup_global_fuel "${BB}" "${ROT}")"
                GGP="$(printf '%02d' "${GG}")"
                CFGDIR="${DATADIR}/p15/${SRC}/cask${C}/gFuel${GGP}"
                STEM="${CFGDIR}/p15_${SRC}_cask${C}_gFuel${GGP}"
                MAC_BASE="run_baseFuel${BBP}_${SRC}_cask${C}_gFuel${GGP}.mac"
                MAC="${MACDIR}/${MAC_BASE}"

                cat > "${MAC}" <<EOF
# ===================================================================
# Auto-generated by slurm/generate_p15_macros.sh -- DO NOT EDIT BY HAND
#   p1.5 background-sector production, one config per macro
#   base fuel : ${BB}   source : ${SRC}   cask : ${C}   hexant rot : ${ROT} deg
#   globalFuel : ${GG} (lookup_global_fuel ${BB} ${ROT}, orbit map)
#   p1 source  : ${SRC_FILE}
#   numPrimaries : ${NEVTS} (from p1 file name)
#   T_gen = ${MEAS_TIME} s per config, ${NCHUNKS} chunk(s) of ${CHUNK_S} s
#   Geometry: 12 CASTOR 440/84 casks + concrete hall, NO detector
#   placed.  The 64-cell sector shell (8 sectors x 8 z-layers)
#   is built in a parallel world around target cask 0 (passive);
#   requires --mode background-sector on the G4DCSmonitor command line.
# ===================================================================

/control/verbose 0
/run/verbose     0
/tracking/verbose 0
/event/verbose   0
/process/verbose 0
/process/em/verbose 0
/process/had/verbose 0

/run/numberOfThreads                ${THREADS}
/dcs-monitor/det/useBiasing         false
/dcs-monitor/det/setBiasingShells   1
# 8 z-layers over the cask height -> 64 cells (must be set pre-init)
/dcs-monitor/det/setSectorZLayers   8

##  ## --- detector template for reference (intentionally NOT placed) ---
##  /dcs-monitor/det/setPosition 2930.00 1580.00 150 mm
##  /dcs-monitor/det/setRotation 0 -90 0
##  /dcs-monitor/det/clyc/add

# =====================================================================
#  THREE CASTOR 440/84 clusters, side-facing 2x2 squares, SYMMETRIC
#  about the y-axis. Intra pitch 3160 mm (500 mm gaps); inter-cluster
#  gap 2000 mm (centroid pitch 7820 mm). All rot 0 (hexant on source).
#  Add-order == caskNum: central=0-3, +x=4-7, -x=8-11.
# =====================================================================
/dcs-monitor/det/setRotation           0     0     0
# --- CENTRAL cluster (caskNum 0-3); cask 0 = detector target ---
/dcs-monitor/det/setPosition        1580  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition        1580 -1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition       -1580  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition       -1580 -1580     0 mm
/dcs-monitor/det/castor440/add
# --- +x cluster (caskNum 4-7) ---
/dcs-monitor/det/setPosition        6240  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition        6240 -1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition        9400  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition        9400 -1580     0 mm
/dcs-monitor/det/castor440/add
# --- -x cluster (caskNum 8-11) ---
/dcs-monitor/det/setPosition       -6240  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition       -6240 -1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition       -9400  1580     0 mm
/dcs-monitor/det/castor440/add
/dcs-monitor/det/setPosition       -9400 -1580     0 mm
/dcs-monitor/det/castor440/add

# --- storage hall (floor + 2 walls) for room return ---
/dcs-monitor/det/setPosition           0     0     0 mm
/dcs-monitor/det/setRotation           0     0     0
/dcs-monitor/det/hall/add
/dcs-monitor/det/hall/setLength      22000 mm
/dcs-monitor/det/hall/setWallInnerY   5410 mm
/dcs-monitor/det/hall/setFloorTopZ   -2040 mm
/dcs-monitor/det/hall/setUseCeiling   false

/run/initialize
/process/had/rdm/thresholdForVeryLongDecayTime 1e60 year

# source config (set ONCE; p1 load happens lazily on first startRun)
/dcs-monitor/gun/sourceMode     CASTOR440_surface_from_TTree
/dcs-monitor/surf/sourcePid     ${SOURCE_PID}
/dcs-monitor/surf/smearPhi      0.005
/dcs-monitor/surf/smearZ        10.0 mm
/dcs-monitor/surf/smearAngle    0.02
/dcs-monitor/surf/smearEfrac    0.0
/dcs-monitor/surf/numPrimaries  ${NEVTS}
/dcs-monitor/surf/sourceFile    ${SRC_FILE}

/dcs-monitor/run/writePrimaryOnlyOnHit  false
/dcs-monitor/run/writePrimary           false
/dcs-monitor/run/writeSurfaceFlux       false
/dcs-monitor/run/writeFluxMap           false

# --- p1.5 config: cask ${C}, hexant rot ${ROT} deg, globalFuel ${GG} ---
/dcs-monitor/surf/caskNum           ${C}
/dcs-monitor/surf/sourceRotZ        ${ROT} deg
/dcs-monitor/surf/globalFuel        ${GG}
/dcs-monitor/surf/batchId           0
/dcs-monitor/surf/sourceType        ${STYPE}
/dcs-monitor/surf/decayRate         ${RATE}
EOF

                # chunk blocks: one fresh output file per chunk (T_gen per
                # chunk; the merged library file carries the summed row)
                local_k=1
                while [ "${local_k}" -le "${NCHUNKS}" ]; do
                    CHUNK_IDX="$(printf "${CHUNK_IDX_FMT}" "${local_k}")"
                    cat >> "${MAC}" <<EOF
/analysis/setFileName               ${STEM}_chunk${CHUNK_IDX}
/dcs-monitor/surf/startRun          ${CHUNK_S} s
EOF
                    local_k=$(( local_k + 1 ))
                done

                printf '%s|%s|%s|%s\n' "${MAC_BASE}" "${CFGDIR}" "${STEM}" "${NCHUNKS}" >> "${MANIFEST_TMP}"
                N_GENERATED=$(( N_GENERATED + 1 ))
                N_PER_SOURCE[${SRC}]=$(( ${N_PER_SOURCE[${SRC}]:-0} + 1 ))
                N_TOTAL=$(( N_TOTAL + 1 ))
            done
        done
    done
done

mv "${MANIFEST_TMP}" "${MANIFEST}"

echo "generate_p15_macros.sh: generated ${N_GENERATED} macros + manifest in ${MACDIR}"
for SRC_LINE in "${SOURCES[@]}"; do
    IFS='|' read -r SRC _ <<< "${SRC_LINE}"
    if [ -n "${N_PER_SOURCE[${SRC}]:-}" ]; then
        echo "  ${SRC}: ${N_PER_SOURCE[${SRC}]} configs"
    fi
    if [ -n "${N_MISSING[${SRC}]:-}" ]; then
        echo "  ${SRC}: ${N_MISSING[${SRC}]} base fuels MISSING p1 files (skipped)"
    fi
done
[ "${N_GENERATED}" -gt 0 ] || { echo "FATAL: no macros generated" >&2; exit 1; }
