#!/usr/bin/env bash
# =============================================================================
# MuonScint FAST SLURM driver -- generate + submit the low-latency runs.
#
# This is a SELF-CONTAINED companion to run_all.sh; it does not modify any
# existing .sbatch or driver script.  It writes isolated matrix/whole macro
# sets (muon_fast/, muon_whole_fast/) and regenerates the shared contact/Al
# sets (muon_contact/, muon_al/), and uses up to FOUR job arrays:
#   1. matrix        -- 4 configs x 4 reflectors, centre/edge/corner
#   2. whole-slab    -- 4 configs x 4 reflectors, full 250 x 250 mm^2 plate
#   3. contact scan  -- bare plate, effective holder contact f=0..0.05
#                       (v6 nominal f=0.045; f=0 is the clean-air reference)
#   4. Al backing    -- Al foil with grease (n=1.46) vs air (n=1.0003)
#
# The contact/Al arrays are calibration scans: skip them once the nominal
# surface parameters are locked.  Use SKIP_AL=1 for Al only, or
# SKIP_CALIBRATION=1 to skip both and run only matrix + whole-slab.
#
# Differences vs run_all.sh:
#   * matrix + characterization flux map OFF (3x faster; efficiency comes from
#     the sipmHits tree)
#   * --mem=4G (measured peak RSS ~0.4 GB)
#   * MATRIX --time=01:00:00 (~24 s/task at 32 cores), WHOLE --time=02:00:00
#     (~9-11 min/task at 32 cores; the 2h ceiling keeps margin at fewer threads,
#     since walltime scales ~1/N with thread count)
#   * routed to the `low` partition/account (145 idle nodes, fair-share 0.64),
#     overridable per array from the arguments / environment below.
#
# WORKFLOW (typical cluster use):
#   cd /path/to/the/dir/that/should/hold/the/output
#   /abs/path/to/repo/slurm/run_fast.sh [MATRIX_EVENTS] [WHOLE_EVENTS] \
#                                       [MATRIX_THREADS] [WHOLE_THREADS]
#   Output lands in  $PWD/data_<VERSION>/{matrix,whole,contact,al}
#   (DATAROOT defaults to the CURRENT DIRECTORY, so run it from where you want
#    the data; set DATAROOT=/abs/path to write somewhere else. Logs and the
#    generated macros stay in slurm/ next to the scripts.)
#   BIN=/abs/path/to/G4DCSmonitor  selects a specific binary (see below).
#
# Usage:
#   run_fast.sh [MATRIX_EVENTS] [WHOLE_EVENTS] [MATRIX_THREADS] [WHOLE_THREADS]
#
# Defaults: 200 / 10000 / 32 / 32.  Each can also be set by environment:
# MATRIX_EVENTS / WHOLE_EVENTS / MATRIX_THREADS / WHOLE_THREADS.  Threads MUST
# match --cpus-per-task, which this script also passes to sbatch, so they stay
# in sync automatically.
#
# Characterization arrays (environment):
#   CONTACT_EVENTS  (default 1000)      events per beam position
#   CONTACT_THREADS (default MATRIX_THREADS)
#   CONTACT_FRACTIONS (default "0.000 0.005 ... 0.050", eleven values)
#   CONTACT_CFGS    (default "1 2 3"; Config 4 has no black holder)
#   AL_EVENTS       (default 1000)
#   AL_THREADS      (default MATRIX_THREADS)
#   AL_BACKINGS     (default "grease air")
#   AL_CFGS         (default "1 2 3"; Config 4 is WLS-dominated)
#   SKIP_CONTACT    (default 0)         1 = skip the contact scan
#   SKIP_AL         (default 0)         1 = skip the Al backing scan
#   SKIP_CALIBRATION(default 0)         1 = skip both calibration scans
#
# Routing/resource knobs (environment, all optional):
#   MATRIX_PARTITION (default low)      MATRIX_ACCOUNT (default low)
#   MATRIX_TIME      (default 01:00:00) MATRIX_ARRAY   (default 0-15)
#   MATRIX_MEM       (default 4G)       matrix --mem per task
#   WHOLE_PARTITION  (default low)      WHOLE_ACCOUNT (default low)
#   WHOLE_TIME       (default 02:00:00) WHOLE_ARRAY    (default 0-15)
#   WHOLE_MEM        (default 4G)       whole  --mem per task
#   CONTACT_PARTITION/ACCOUNT/TIME/ARRAY/MEM (default low/low/01:00:00/auto/4G)
#   AL_PARTITION/ACCOUNT/TIME/ARRAY/MEM      (default low/low/01:00:00/auto/4G)
#     (*_ARRAY default is auto-computed from the generated manifest; override
#      only if you want a subset.)
#   ARRAY_LIMIT      (default empty)    throttle all arrays, e.g. ARRAY_LIMIT=4
#   DATAROOT         (default $PWD)     ROOT output root (absolute path)
#   VERSION          (default v2.2)     data-set tag -> data_<VERSION>/{..}
#   BIN              (default pre-built HZDR binary, else ../build/G4DCSmonitor)
#
# Examples:
#   # main production phase after the surface calibration is locked:
#   SKIP_CALIBRATION=1 ./run_fast.sh 2500 25000
#   # 500 matrix / 20000 whole-slab events, 16 threads, 8 GB/task, to CWD:
#   MATRIX_THREADS=16 WHOLE_THREADS=16 MATRIX_MEM=8G WHOLE_MEM=8G ./run_fast.sh 500 20000
#   # 2000 events/position for the contact and Al characterization scans:
#   CONTACT_EVENTS=2000 AL_EVENTS=2000 ./run_fast.sh
#   # fast whole-slab on the guaranteed default queue (no preemption):
#   WHOLE_PARTITION=cpu-genoa WHOLE_ACCOUNT=default WHOLE_TIME=02:00:00 ./run_fast.sh
#   # everything on `short` (15-min partition limit -> keep time <= 00:12:00):
#   MATRIX_PARTITION=short MATRIX_ACCOUNT=short MATRIX_TIME=00:12:00 ./run_fast.sh
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Load nominal surface calibration values, if available.
ENVFILE="${HERE}/nominal_surface.env"
[[ -f "${ENVFILE}" ]] && source "${ENVFILE}"
NOMINAL_BARE_CONTACT="${NOMINAL_BARE_CONTACT:-0.045}"
NOMINAL_AL_BACKING="${NOMINAL_AL_BACKING:-air}"

MATRIX_EVENTS="${1:-${MATRIX_EVENTS:-200}}"
WHOLE_EVENTS="${2:-${WHOLE_EVENTS:-10000}}"
MATRIX_THREADS="${3:-${MATRIX_THREADS:-32}}"
WHOLE_THREADS="${4:-${WHOLE_THREADS:-32}}"

MATRIX_PARTITION="${MATRIX_PARTITION:-low}"
MATRIX_ACCOUNT="${MATRIX_ACCOUNT:-low}"
MATRIX_TIME="${MATRIX_TIME:-01:00:00}"
MATRIX_ARRAY="${MATRIX_ARRAY:-0-15}"
MATRIX_MEM="${MATRIX_MEM:-4G}"

WHOLE_PARTITION="${WHOLE_PARTITION:-low}"
WHOLE_ACCOUNT="${WHOLE_ACCOUNT:-low}"
WHOLE_TIME="${WHOLE_TIME:-02:00:00}"
WHOLE_ARRAY="${WHOLE_ARRAY:-0-15}"
WHOLE_MEM="${WHOLE_MEM:-4G}"

CONTACT_EVENTS="${CONTACT_EVENTS:-1000}"
CONTACT_THREADS="${CONTACT_THREADS:-${MATRIX_THREADS}}"
CONTACT_PARTITION="${CONTACT_PARTITION:-low}"
CONTACT_ACCOUNT="${CONTACT_ACCOUNT:-low}"
CONTACT_TIME="${CONTACT_TIME:-01:00:00}"
CONTACT_ARRAY="${CONTACT_ARRAY:-}"
CONTACT_MEM="${CONTACT_MEM:-4G}"
CONTACT_FRACTIONS="${CONTACT_FRACTIONS:-0.000 0.005 0.010 0.015 0.020 0.025 0.030 0.035 0.040 0.045 0.050}"
CONTACT_CFGS="${CONTACT_CFGS:-1 2 3}"

AL_EVENTS="${AL_EVENTS:-1000}"
AL_THREADS="${AL_THREADS:-${MATRIX_THREADS}}"
AL_PARTITION="${AL_PARTITION:-low}"
AL_ACCOUNT="${AL_ACCOUNT:-low}"
AL_TIME="${AL_TIME:-01:00:00}"
AL_ARRAY="${AL_ARRAY:-}"
AL_MEM="${AL_MEM:-4G}"
AL_BACKINGS="${AL_BACKINGS:-grease air}"
AL_CFGS="${AL_CFGS:-1 2 3}"

SKIP_CONTACT="${SKIP_CONTACT:-0}"
SKIP_AL="${SKIP_AL:-0}"
SKIP_CALIBRATION="${SKIP_CALIBRATION:-0}"

if [[ "${SKIP_CALIBRATION}" == "1" ]]; then
    SKIP_CONTACT=1
    SKIP_AL=1
fi

export CONTACT_FRACTIONS CONTACT_CFGS AL_BACKINGS AL_CFGS

ARRAY_LIMIT="${ARRAY_LIMIT:-}"

# Output root: default to the directory this script is invoked FROM (so "cd to
# where you want the data, then run it" works), resolved to an absolute path.
DATAROOT="${DATAROOT:-${PWD}}"
DATAROOT="$(cd "${DATAROOT}" 2>/dev/null && pwd || printf '%s' "${DATAROOT}")"
export DATAROOT

# Data-set version tag (see run_all.sh).  Output is laid out as
#   ${DATAROOT}/data_<VERSION>/matrix/   and   .../whole/
VERSION="${VERSION:-v2.2}"
export VERSION

# Executable: BIN env > pre-built HZDR binary > repo build.
if [[ -z "${BIN:-}" ]]; then
    if [[ -x /home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor ]]; then
        BIN=/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor
    else
        BIN="${HERE}/../build/G4DCSmonitor"
    fi
fi
export BIN
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: executable not found or not runnable: ${BIN}" >&2
    echo "       export BIN=/abs/path/to/G4DCSmonitor and re-run." >&2
    exit 1
fi

echo "=== MuonScint FAST SLURM driver ==="
echo "  matrix events : ${MATRIX_EVENTS}  (threads ${MATRIX_THREADS})"
echo "  whole  events : ${WHOLE_EVENTS}  (threads ${WHOLE_THREADS})"
if [[ "${SKIP_CONTACT}" == "1" ]]; then
    echo "  contact events: skipped"
else
    echo "  contact events: ${CONTACT_EVENTS}  (threads ${CONTACT_THREADS}; f='${CONTACT_FRACTIONS}'; cfg='${CONTACT_CFGS}')"
fi
if [[ "${SKIP_AL}" == "1" ]]; then
    echo "  Al events     : skipped"
else
    echo "  Al events     : ${AL_EVENTS}  (threads ${AL_THREADS}; backings='${AL_BACKINGS}'; cfg='${AL_CFGS}')"
fi
echo "  matrix route  : -p ${MATRIX_PARTITION} -A ${MATRIX_ACCOUNT} -t ${MATRIX_TIME} --array=${MATRIX_ARRAY} --mem=${MATRIX_MEM}"
echo "  whole  route  : -p ${WHOLE_PARTITION} -A ${WHOLE_ACCOUNT} -t ${WHOLE_TIME} --array=${WHOLE_ARRAY} --mem=${WHOLE_MEM}"
if [[ "${SKIP_CONTACT}" != "1" ]]; then
    echo "  contact route : -p ${CONTACT_PARTITION} -A ${CONTACT_ACCOUNT} -t ${CONTACT_TIME} --array=${CONTACT_ARRAY:-auto} --mem=${CONTACT_MEM}"
fi
if [[ "${SKIP_AL}" != "1" ]]; then
    echo "  Al route      : -p ${AL_PARTITION} -A ${AL_ACCOUNT} -t ${AL_TIME} --array=${AL_ARRAY:-auto} --mem=${AL_MEM}"
fi
echo "  throttle      : ${ARRAY_LIMIT:-<none>}"
echo "  binary        : ${BIN}"
echo "  data root     : ${DATAROOT}/data_${VERSION}"

MATRIX_TEMPLATE="${HERE}/muon_scint_template_fast.mac"
WHOLE_TEMPLATE="${HERE}/whole_slab_template.mac"
CHARACTERIZATION_TEMPLATE="${HERE}/muon_characterization_template.mac"
MATRIX_OUT="${HERE}/muon_fast"
WHOLE_OUT="${HERE}/muon_whole_fast"
CONTACT_OUT="${HERE}/muon_contact"
AL_OUT="${HERE}/muon_al"
MATRIX_DATA="${DATAROOT}/data_${VERSION}/matrix"
WHOLE_DATA="${DATAROOT}/data_${VERSION}/whole"
CONTACT_DATA="${DATAROOT}/data_${VERSION}/contact"
AL_DATA="${DATAROOT}/data_${VERSION}/al"

for f in "${MATRIX_TEMPLATE}" "${WHOLE_TEMPLATE}" "${CHARACTERIZATION_TEMPLATE}"; do
    [[ -f "${f}" ]] || { echo "ERROR: template missing: ${f}" >&2; exit 1; }
done

mkdir -p "${HERE}/logs" "${MATRIX_OUT}" "${WHOLE_OUT}" \
         "${MATRIX_DATA}" "${WHOLE_DATA}"
if [[ "${SKIP_CONTACT}" != "1" ]]; then
    mkdir -p "${CONTACT_OUT}" "${CONTACT_DATA}"
fi
if [[ "${SKIP_AL}" != "1" ]]; then
    mkdir -p "${AL_OUT}" "${AL_DATA}"
fi
cd "${HERE}"

# ---- generate matrix macros (flux map OFF, fast template) -------------------
echo
echo "--- generating FAST matrix macros (${MATRIX_EVENTS} ev/position) ---"
: > "${MATRIX_OUT}/manifest.txt"
i=0
for cfg in 1 2 3 4; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="scan_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${MATRIX_EVENTS}/g" \
            -e "s/@THREADS@/${MATRIX_THREADS}/g" \
            -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
            -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
            -e "s#@DATADIR@#${MATRIX_DATA}#g" \
            "${MATRIX_TEMPLATE}" > "${MATRIX_OUT}/${out}"
        printf '%s\n' "${out}" >> "${MATRIX_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

# ---- generate whole-slab macros (existing whole template) -------------------
echo
echo "--- generating FAST whole-slab macros (${WHOLE_EVENTS} ev) ---"
: > "${WHOLE_OUT}/manifest.txt"
i=0
for cfg in 1 2 3 4; do
    if [[ "${cfg}" == "4" ]]; then preset=7; else preset="${cfg}"; fi
    for refl in 0 1 2 3; do
        out="whole_c${cfg}_r${refl}.mac"
        sed -e "s/@CONFIG@/${cfg}/g"    \
            -e "s/@PRESET@/${preset}/g" \
            -e "s/@REFLECTOR@/${refl}/g" \
            -e "s/@NEVENTS@/${WHOLE_EVENTS}/g" \
            -e "s/@THREADS@/${WHOLE_THREADS}/g" \
            -e "s/@BARECONTACT@/${NOMINAL_BARE_CONTACT}/g" \
            -e "s/@ALBACKING@/${NOMINAL_AL_BACKING}/g" \
            -e "s#@DATADIR@#${WHOLE_DATA}#g" \
            "${WHOLE_TEMPLATE}" > "${WHOLE_OUT}/${out}"
        printf '%s\n' "${out}" >> "${WHOLE_OUT}/manifest.txt"
        printf '  array %2d -> %s\n' "${i}" "${out}"
        i=$((i + 1))
    done
done

# ---- generate characterization macros (contact + Al backing) ----------------
if [[ "${SKIP_CONTACT}" != "1" ]]; then
    echo
    echo "--- generating FAST contact-fraction macros (${CONTACT_EVENTS} ev/position) ---"
    FLUXMAP=off CONTACT_FRACTIONS="${CONTACT_FRACTIONS}" CONTACT_CFGS="${CONTACT_CFGS}" \
        "${HERE}/generate_contact_scan.sh" "${CONTACT_EVENTS}" "${CONTACT_THREADS}" "${CONTACT_DATA}"
fi

if [[ "${SKIP_AL}" != "1" ]]; then
    echo
    echo "--- generating FAST Al-backing macros (${AL_EVENTS} ev/position) ---"
    FLUXMAP=off AL_BACKINGS="${AL_BACKINGS}" AL_CFGS="${AL_CFGS}" \
        "${HERE}/generate_al_backing.sh" "${AL_EVENTS}" "${AL_THREADS}" "${AL_DATA}"
fi

# Auto array bounds unless the user asked for a subset via CONTACT_ARRAY /
# AL_ARRAY.  The manifest is the single source of truth.
if [[ "${SKIP_CONTACT}" != "1" ]]; then
    if [[ -z "${CONTACT_ARRAY}" ]]; then
        n=$(wc -l < "${CONTACT_OUT}/manifest.txt")
        CONTACT_ARRAY="0-$((n - 1))"
    fi
fi
if [[ "${SKIP_AL}" != "1" ]]; then
    if [[ -z "${AL_ARRAY}" ]]; then
        n=$(wc -l < "${AL_OUT}/manifest.txt")
        AL_ARRAY="0-$((n - 1))"
    fi
fi

# ---- submit requested arrays with routing overrides ------------------------
echo
echo "--- submitting FAST job arrays ---"

mt_array="${MATRIX_ARRAY}"
wt_array="${WHOLE_ARRAY}"
ct_array="${CONTACT_ARRAY}"
at_array="${AL_ARRAY}"
if [[ -n "${ARRAY_LIMIT}" ]]; then
    mt_array="${MATRIX_ARRAY%%%*}%${ARRAY_LIMIT}"
    wt_array="${WHOLE_ARRAY%%%*}%${ARRAY_LIMIT}"
    if [[ "${SKIP_CONTACT}" != "1" ]]; then
        ct_array="${CONTACT_ARRAY%%%*}%${ARRAY_LIMIT}"
    fi
    if [[ "${SKIP_AL}" != "1" ]]; then
        at_array="${AL_ARRAY%%%*}%${ARRAY_LIMIT}"
    fi
fi

jid_matrix="$(sbatch --parsable \
    -p "${MATRIX_PARTITION}" -A "${MATRIX_ACCOUNT}" \
    -t "${MATRIX_TIME}" --array="${mt_array}" \
    --cpus-per-task="${MATRIX_THREADS}" --mem="${MATRIX_MEM}" \
    "${HERE}/run_matrix_fast.sbatch")"

jid_whole="$(sbatch --parsable \
    -p "${WHOLE_PARTITION}" -A "${WHOLE_ACCOUNT}" \
    -t "${WHOLE_TIME}" --array="${wt_array}" \
    --cpus-per-task="${WHOLE_THREADS}" --mem="${WHOLE_MEM}" \
    "${HERE}/run_whole_fast.sbatch")"

if [[ "${SKIP_CONTACT}" != "1" ]]; then
    jid_contact="$(sbatch --parsable \
        -p "${CONTACT_PARTITION}" -A "${CONTACT_ACCOUNT}" \
        -t "${CONTACT_TIME}" --array="${ct_array}" \
        --cpus-per-task="${CONTACT_THREADS}" --mem="${CONTACT_MEM}" \
        "${HERE}/run_contact.sbatch")"
else
    jid_contact="(skipped)"
fi

if [[ "${SKIP_AL}" != "1" ]]; then
    jid_al="$(sbatch --parsable \
        -p "${AL_PARTITION}" -A "${AL_ACCOUNT}" \
        -t "${AL_TIME}" --array="${at_array}" \
        --cpus-per-task="${AL_THREADS}" --mem="${AL_MEM}" \
        "${HERE}/run_al.sbatch")"
else
    jid_al="(skipped)"
fi

if [[ "${SKIP_CONTACT}" == "1" ]]; then
    contact_summary="  contact array    : skipped"
else
    contact_summary="  contact array    : job ${jid_contact}   (${CONTACT_EVENTS} ev/pos, ${CONTACT_THREADS} threads, ${CONTACT_MEM}, -p ${CONTACT_PARTITION})
                     output: ${CONTACT_DATA}/"
fi
if [[ "${SKIP_AL}" == "1" ]]; then
    al_summary="  Al-backing array : skipped"
else
    al_summary="  Al-backing array : job ${jid_al}   (${AL_EVENTS} ev/pos, ${AL_THREADS} threads, ${AL_MEM}, -p ${AL_PARTITION})
                     output: ${AL_DATA}/"
fi

cat <<EOF

=== submitted (FAST) ===
  matrix array     : job ${jid_matrix}   (${MATRIX_EVENTS} ev/pos, ${MATRIX_THREADS} threads, ${MATRIX_MEM}, -p ${MATRIX_PARTITION})
                     output: ${MATRIX_DATA}/
  whole-slab array : job ${jid_whole}   (${WHOLE_EVENTS} ev, ${WHOLE_THREADS} threads, ${WHOLE_MEM}, -p ${WHOLE_PARTITION})
                     output: ${WHOLE_DATA}/
${contact_summary}
${al_summary}

Watch:   squeue -u \$USER -o "%.10i %.14P %.13j %.2t %.10M %.6D %R"
Logs:    ${HERE}/logs/muonscanfast_*_*.out
         ${HERE}/logs/muonwholefast_*_*.out
         ${HERE}/logs/muoncontact_*_*.out
         ${HERE}/logs/muonal_*_*.out
Collect: DATAROOT=${DATAROOT} VERSION=${VERSION} ${HERE}/collect.sh
Analyse: root -l -b -q 'macros/plot_collection_efficiency.C+("${WHOLE_DATA}")'
         root -l -b -q 'macros/plot_muon.C+("${MATRIX_DATA}")'
Report:  macros/build_report_figures.sh "${MATRIX_DATA}" "${WHOLE_DATA}"
EOF
