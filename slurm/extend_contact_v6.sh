#!/usr/bin/env bash
# =============================================================================
# v6 contact-extension driver.
#
# Extends the bare-plate / black-holder contact-fraction scan to
# f = 0.035, 0.040, 0.045, 0.050 for configs 1-3.  Stale macros in
# slurm/muon_contact/ and the redundant slurm/muon_contact_v6/ directory are
# removed, the extension macros are regenerated, and the SLURM array is
# submitted.
#
# Run on the cluster (any CWD; the script operates from slurm/):
#   ./slurm/extend_contact_v6.sh
#
# Data must live on the HPC-native /bigdata mount, not the login-node sshfs
# path (~/bigdata/...).  The default DATAROOT below is the native path.
#
# Override defaults via environment:
#   DATAROOT, VERSION, BIN, CONTACT_FRACTIONS, CONTACT_CFGS,
#   NEVENTS, THREADS, CONTACT_PARTITION, CONTACT_ACCOUNT, CONTACT_TIME,
#   CONTACT_MEM, ARRAY_LIMIT
# =============================================================================
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Submit from slurm/ so run_contact.sbatch resolves muon_contact/manifest.txt
# via SLURM_SUBMIT_DIR.
cd "${HERE}"

DATAROOT="${DATAROOT:-/bigdata/DCS-Monitor-II/turko46/MuonScinti}"
VERSION="${VERSION:-v6}"
BIN="${BIN:-/home/turko46/geant4/muon_trigger_test/build/G4DCSmonitor}"
CONTACT_FRACTIONS="${CONTACT_FRACTIONS:-0.035 0.040 0.045 0.050}"
CONTACT_CFGS="${CONTACT_CFGS:-1 2 3}"
NEVENTS="${NEVENTS:-1000}"
THREADS="${THREADS:-32}"

CONTACT_PARTITION="${CONTACT_PARTITION:-low}"
CONTACT_ACCOUNT="${CONTACT_ACCOUNT:-low}"
CONTACT_TIME="${CONTACT_TIME:-01:00:00}"
CONTACT_MEM="${CONTACT_MEM:-4G}"
ARRAY_LIMIT="${ARRAY_LIMIT:-}"

export DATAROOT VERSION BIN

echo "=== v6 contact extension ==="
echo "  dataroot : ${DATAROOT}"
echo "  version  : ${VERSION}"
echo "  binary   : ${BIN}"
echo "  f values : ${CONTACT_FRACTIONS}"
echo "  configs  : ${CONTACT_CFGS}"
echo "  events   : ${NEVENTS}"
echo "  threads  : ${THREADS}"
echo "  route    : -p ${CONTACT_PARTITION} -A ${CONTACT_ACCOUNT} -t ${CONTACT_TIME} --mem=${CONTACT_MEM}"

# Remove redundant directory and any stale macros still in muon_contact/.
rm -rf "${HERE}/muon_contact_v6"
rm -f "${HERE}/muon_contact"/contact_c*_f0p0{00..30}.mac

# Generate extension macros into the canonical slurm/muon_contact/ directory.
FLUXMAP=off \
CONTACT_FRACTIONS="${CONTACT_FRACTIONS}" \
CONTACT_CFGS="${CONTACT_CFGS}" \
    "${HERE}/generate_contact_scan.sh" "${NEVENTS}" "${THREADS}" \
        "${DATAROOT}/data_${VERSION}/contact"

# Submit using the generated manifest (override the default 0-20 array).
n=$(wc -l < "${HERE}/muon_contact/manifest.txt")
array="0-$((n - 1))"
if [[ -n "${ARRAY_LIMIT}" ]]; then
    array="${array}%${ARRAY_LIMIT}"
fi

echo
echo "Submitting ${n} contact-extension tasks (array ${array})..."
sbatch --parsable \
    -p "${CONTACT_PARTITION}" -A "${CONTACT_ACCOUNT}" \
    -t "${CONTACT_TIME}" --array="${array}" \
    --cpus-per-task="${THREADS}" --mem="${CONTACT_MEM}" \
    "${HERE}/run_contact.sbatch"
