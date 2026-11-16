#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# run_analysis.sh: runs ONE analysis job on a worker node (called by HTCondor).
# Submit with run_submission.sh, which writes the submit file that calls this script.
#
# Usage: run_analysis.sh <job_dir> <lcg_setup> <run_number> <output_file> [analysis args...]
#   job_dir      directory with the snapshot of the analysis binary and config/ (made by run_submission.sh)
#   lcg_setup    LCG view setup script to source ("none" to use the current environment)
#   run_number   run to process (--run)
#   output_file  final output ROOT file
#   analysis args are passed on to ./analysis unchanged (e.g. --isMC --cuts-config config/cuts.yaml)
# ─────────────────────────────────────────────────────────────────────────────

if [ $# -lt 4 ]; then
    echo "Usage: $0 <job_dir> <lcg_setup> <run_number> <output_file> [analysis args...]" >&2
    exit 2
fi
JOB_DIR=$1
LCG_SETUP=$2
RUN=$3
OUTPUT=$4
shift 4

echo "Run ${RUN} on $(hostname) at $(date)"
echo "Job dir:       ${JOB_DIR}"
echo "Output:        ${OUTPUT}"
echo "Analysis args: $*"

# Software environment
if [ "${LCG_SETUP}" != "none" ]; then
    if [ ! -f "${LCG_SETUP}" ]; then
        echo "ERROR: LCG setup script ${LCG_SETUP} not found (is cvmfs mounted on this node?)" >&2
        exit 3
    fi
    # shellcheck disable=SC1090  # the setup script is given at run time
    source "${LCG_SETUP}"
fi
export ROOT_RDF_SNAPSHOT_INFO=0   # suppress RDataFrame snapshot info messages

# Grid proxy (for xrootd input): HTCondor copies it into the job's sandbox and sets X509_USER_PROXY.
# If that copy is missing (e.g. no file transfer on a shared filesystem), use the proxy file on the
# shared filesystem that run_submission.sh passes in X509_USER_PROXY_SHARED
if [ -n "${X509_USER_PROXY_SHARED:-}" ] && { [ -z "${X509_USER_PROXY:-}" ] || [ ! -f "${X509_USER_PROXY}" ]; }; then
    echo "Grid proxy ${X509_USER_PROXY:-(X509_USER_PROXY not set)} not found, using ${X509_USER_PROXY_SHARED}"
    export X509_USER_PROXY=${X509_USER_PROXY_SHARED}
fi
if [ -n "${X509_USER_PROXY:-}" ]; then
    if [ -f "${X509_USER_PROXY}" ]; then
        echo "Grid proxy:    ${X509_USER_PROXY}$(command -v voms-proxy-info > /dev/null && echo " (time left: $(voms-proxy-info -file "${X509_USER_PROXY}" -timeleft 2>/dev/null) s)")"
    else
        echo "WARNING: grid proxy ${X509_USER_PROXY} does not exist: reading files over xrootd will fail" >&2
    fi
fi

# The binary reads its configs relative to the working directory (config/...)
cd "${JOB_DIR}" || { echo "ERROR: cannot cd to ${JOB_DIR}" >&2; exit 3; }

# Write to the node's local scratch directory and copy the file to its final place only if the
# job succeeded, so a failed job never leaves a partial file in the output directory
WORK_OUTPUT=${OUTPUT}
if [ -n "${_CONDOR_SCRATCH_DIR}" ] && [ -d "${_CONDOR_SCRATCH_DIR}" ]; then
    WORK_OUTPUT="${_CONDOR_SCRATCH_DIR}/$(basename "${OUTPUT}")"
fi

./analysis --run "${RUN}" -o "${WORK_OUTPUT}" "$@"
STATUS=$?
echo "analysis exit code: ${STATUS}"

if [ ${STATUS} -eq 0 ] && [ "${WORK_OUTPUT}" != "${OUTPUT}" ]; then
    # copy under a temporary name, then rename: the final name only appears when the copy is complete
    if cp "${WORK_OUTPUT}" "${OUTPUT}.part" && mv -f "${OUTPUT}.part" "${OUTPUT}"; then
        echo "Copied output to ${OUTPUT}"
    else
        echo "ERROR: could not copy ${WORK_OUTPUT} to ${OUTPUT}" >&2
        rm -f "${OUTPUT}.part"
        STATUS=4
    fi
fi

echo "Finished at $(date) with exit code ${STATUS}"
# A non-zero exit code marks the job as failed in HTCondor (condor_history, the job log)
exit ${STATUS}
