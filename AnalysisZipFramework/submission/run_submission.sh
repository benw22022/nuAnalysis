#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# run_submission.sh: submit one HTCondor job per run number.
#
# Usage: run_submission.sh -o <output_dir> [options] [-- <analysis arguments>]
# Run with -h for all options. Examples:
#   ./run_submission.sh -o /path/to/output_v1
#   ./run_submission.sh -o /path/to/mc_v1 -r mc_runs.txt -- --isMC --cuts-config my_cuts.yaml
#   ./run_submission.sh -o /path/to/output_v1 --only-missing        # resubmit failed / missing runs
#   ./run_submission.sh -o /path/to/output_v1 -n                    # dry run: check, submit nothing
#
# For every submission a job directory is made (default <output_dir>/submission_<date>_<time>) with
#   - a snapshot of the analysis binary, build/config/ and any config passed as an argument, so the
#     jobs are not affected by rebuilding or editing configs while they run
#   - the run list, the generated submit file (analysis.sub) and the job script
#   - submission_info.txt (command line, git commit, versions, ...) and git_diff.patch if the code
#     has uncommitted changes, to make the output reproducible
#   - logs/ with the HTCondor log, stdout and stderr of each job
# Output: <output_dir>/<run>.root
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
FRAMEWORK_DIR=$(cd "${SCRIPT_DIR}/.." && pwd)        # AnalysisZipFramework
REPO_DIR=$(cd "${FRAMEWORK_DIR}/.." && pwd)

# ── Defaults ──────────────────────────────────────────────────────────────────
OUTPUT_DIR=""
RUN_LIST="${SCRIPT_DIR}/run_list.txt"
BUILD_DIR="${REPO_DIR}/build"
JOB_DIR=""
LCG_SETUP="/cvmfs/sft.cern.ch/lcg/views/LCG_109/x86_64-el9-gcc15-opt/setup.sh"
FLAVOUR="espresso"
MEMORY_MB=8000      # measured peak ~6.5 GB on a data run, plus ~20% headroom
DISK_MB=2000
USE_PROXY=1
PROXY=""            # default: ${SCRIPT_DIR}/x509up_u<uid> (shared filesystem, ignored by git)
DRY_RUN=0
ONLY_MISSING=0
OVERWRITE=0
ANALYSIS_ARGS=()

usage() {
    cat <<EOF
Usage: $(basename "$0") -o <output_dir> [options] [-- <analysis arguments>]

Submits one HTCondor job per run number in the run list. Each job runs
  ./analysis --run <run> -o <output_dir>/<run>.root <analysis arguments>

Required:
  -o, --output-dir DIR    directory for the output ROOT files (created if needed)

Options:
  -r, --run-list FILE     run numbers, one per line; '#' comments allowed
                          (default: ${RUN_LIST})
  -b, --build-dir DIR     build directory with the analysis binary and config/
                          (default: ${BUILD_DIR})
  -w, --job-dir DIR       job directory: snapshot, submit file, logs, submission record
                          (default: <output_dir>/submission_<date>_<time>). It must be on a shared
                          filesystem the worker nodes can read (at CERN: not EOS, use AFS)
      --lcg-setup FILE    LCG view setup script sourced by each job, or 'none'
                          (default: ${LCG_SETUP})
      --flavour NAME      +JobFlavour (default: ${FLAVOUR})
      --memory MB         request_memory in MB (default: ${MEMORY_MB})
      --disk MB           request_disk in MB (default: ${DISK_MB})
      --only-missing      only submit runs whose output file does not exist yet (resubmission)
      --overwrite         submit even if output files already exist (they are replaced)
      --proxy FILE        grid proxy (x509) file, created with voms-proxy-init if missing or valid for
                          less than 24 h. It must be on a shared filesystem the worker nodes can read
                          (default: ${SCRIPT_DIR}/x509up_u<uid>)
      --no-proxy          do not create / use a grid proxy
  -n, --dry-run           do all checks and print the first job, but submit nothing
  -h, --help              show this help

Analysis arguments (everything after --) are passed to ./analysis, e.g.
  -- --isMC --cuts-config my_cuts.yaml -j 4
  - Config files (--*-config) are copied into the job directory. Relative paths are looked up in
    the build directory first (as when running ./analysis from build/), then in the current directory.
  - -j N also sets request_cpus = N. A bare -j (all cores) is not allowed on the batch system.
  - --run, -r, --output, -o, --input-dir and --aux-dir are set by this script / not supported.
EOF
}

die()  { echo "ERROR: $*" >&2; exit 1; }
warn() { echo "WARNING: $*" >&2; }

# ── Command line ─────────────────────────────────────────────────────────────
ORIGINAL_COMMAND=$(printf '%q ' "$0" "$@")
while [ $# -gt 0 ]; do
    case "$1" in
        -o|--output-dir) OUTPUT_DIR=${2:?"$1 needs a value"}; shift 2 ;;
        -r|--run-list)   RUN_LIST=${2:?"$1 needs a value"}; shift 2 ;;
        -b|--build-dir)  BUILD_DIR=${2:?"$1 needs a value"}; shift 2 ;;
        -w|--job-dir)    JOB_DIR=${2:?"$1 needs a value"}; shift 2 ;;
        --lcg-setup)     LCG_SETUP=${2:?"$1 needs a value"}; shift 2 ;;
        --flavour)       FLAVOUR=${2:?"$1 needs a value"}; shift 2 ;;
        --memory)        MEMORY_MB=${2:?"$1 needs a value"}; shift 2 ;;
        --disk)          DISK_MB=${2:?"$1 needs a value"}; shift 2 ;;
        --only-missing)  ONLY_MISSING=1; shift ;;
        --overwrite)     OVERWRITE=1; shift ;;
        --proxy)         PROXY=${2:?"$1 needs a value"}; shift 2 ;;
        --no-proxy)      USE_PROXY=0; shift ;;
        -n|--dry-run)    DRY_RUN=1; shift ;;
        -h|--help)       usage; exit 0 ;;
        --)              shift; ANALYSIS_ARGS=("$@"); break ;;
        *)               usage >&2; die "unknown option '$1' (analysis arguments go after --)" ;;
    esac
done

[ -n "${OUTPUT_DIR}" ] || { usage >&2; die "the output directory is required (-o <dir>)"; }
[[ "${MEMORY_MB}" =~ ^[0-9]+$ ]] || die "--memory must be a number of MB, got '${MEMORY_MB}'"
[[ "${DISK_MB}" =~ ^[0-9]+$ ]]   || die "--disk must be a number of MB, got '${DISK_MB}'"
[ ${ONLY_MISSING} -eq 1 ] && [ ${OVERWRITE} -eq 1 ] && die "--only-missing and --overwrite cannot be used together"

OUTPUT_DIR=$(realpath -m "${OUTPUT_DIR}")
BUILD_DIR=$(realpath -m "${BUILD_DIR}")

# ── Build directory: binary and configs ──────────────────────────────────────
[ -x "${BUILD_DIR}/analysis" ] || die "no analysis binary in ${BUILD_DIR} (build it, or give --build-dir)"
[ -d "${BUILD_DIR}/config" ]   || die "no config/ directory in ${BUILD_DIR} (run cmake)"

# The jobs use build/config, which cmake copies from AnalysisZipFramework/config: check it is up to date
CONFIG_PROBLEM=0
for src in "${FRAMEWORK_DIR}"/config/*.yaml; do
    name=$(basename "${src}")
    if [ ! -f "${BUILD_DIR}/config/${name}" ]; then
        echo "ERROR: ${name} is missing in ${BUILD_DIR}/config" >&2; CONFIG_PROBLEM=1
    elif ! cmp -s "${src}" "${BUILD_DIR}/config/${name}"; then
        echo "ERROR: ${BUILD_DIR}/config/${name} differs from AnalysisZipFramework/config/${name}" >&2; CONFIG_PROBLEM=1
    fi
done
[ ${CONFIG_PROBLEM} -eq 0 ] || die "build/config is not up to date: re-run cmake in ${BUILD_DIR} (it copies the configs)"

# Sources newer than the binary: probably not rebuilt
NEWER=$(find "${FRAMEWORK_DIR}" \( -name '*.cxx' -o -name '*.h' -o -name '*.hpp' \) -newer "${BUILD_DIR}/analysis" | head -5)
[ -z "${NEWER}" ] || warn "these sources are newer than ${BUILD_DIR}/analysis (rebuild with make?):"$'\n'"${NEWER}"

# ── Analysis arguments ───────────────────────────────────────────────────────
REQUEST_CPUS=1
FINAL_ARGS=()
CONFIG_ARG_SRC=()   # (option, resolved source path) of configs given as arguments, copied below
i=0
while [ $i -lt ${#ANALYSIS_ARGS[@]} ]; do
    arg=${ANALYSIS_ARGS[$i]}
    case "${arg}" in
        # Options of this script given after -- by mistake (e.g. "-- --cuts-config x.yaml -n") would be
        # passed to ./analysis and every job would fail: stop here instead
        -n|--dry-run|-h|--help|--only-missing|--overwrite|--no-proxy|--proxy|-b|--build-dir|-w|--job-dir|\
        --run-list|--output-dir|--lcg-setup|--flavour|--memory|--disk)
            die "${arg} is an option of $(basename "$0"), but it comes after --, so it would be passed to ./analysis. Put it before the --" ;;
        -o|-r)
            die "${arg} comes after --, so it would be passed to ./analysis. Put it before the -- (the run list and output directory are options of $(basename "$0"))" ;;
        --run|--output)
            die "do not pass ${arg} as an analysis argument: the run comes from the run list and the output from -o" ;;
        --input-dir|--aux-dir)
            die "${arg} is not supported by this script (jobs are one run number each)" ;;
        --isAsimov)
            die "--isAsimov was removed from the analysis, use --isMC" ;;
        -j)
            next=${ANALYSIS_ARGS[$((i + 1))]:-}
            [[ "${next}" =~ ^[0-9]+$ ]] && [ "${next}" -gt 0 ] || die "give the number of threads with -j (e.g. -j 4): a bare -j would use every core of the worker node"
            REQUEST_CPUS=${next}
            FINAL_ARGS+=(-j "${next}"); i=$((i + 2)); continue ;;
        --*-config)
            value=${ANALYSIS_ARGS[$((i + 1))]:-}
            [ -n "${value}" ] || die "${arg} needs a file"
            if   [[ "${value}" = /* ]];               then path=${value}
            elif [ -f "${BUILD_DIR}/${value}" ];      then path="${BUILD_DIR}/${value}"
            else                                           path="${PWD}/${value}"
            fi
            [ -f "${path}" ] || die "${arg} ${value}: file not found (looked in ${BUILD_DIR} and ${PWD})"
            CONFIG_ARG_SRC+=("${arg}" "$(realpath "${path}")")
            FINAL_ARGS+=("${arg}" "@CONFIG_${#CONFIG_ARG_SRC[@]}@")   # replaced by the snapshot path below
            i=$((i + 2)); continue ;;
    esac
    FINAL_ARGS+=("${arg}"); i=$((i + 1))
done

# ── Run list ─────────────────────────────────────────────────────────────────
[ -f "${RUN_LIST}" ] || die "run list ${RUN_LIST} not found"
RUN_LIST=$(realpath "${RUN_LIST}")
mapfile -t ALL_RUNS < <(sed -e 's/#.*//' -e 's/[[:space:]]//g' "${RUN_LIST}" | grep -v '^$' || true)
[ ${#ALL_RUNS[@]} -gt 0 ] || die "no run numbers in ${RUN_LIST}"
for r in "${ALL_RUNS[@]}"; do [[ "${r}" =~ ^[0-9]+$ ]] || die "'${r}' in ${RUN_LIST} is not a run number"; done
DUPLICATES=$(printf '%s\n' "${ALL_RUNS[@]}" | sort | uniq -d | tr '\n' ' ')
# Two jobs of the same run would write the same output file at the same time
[ -z "${DUPLICATES}" ] || warn "runs listed more than once in ${RUN_LIST} (submitted once): ${DUPLICATES}"
mapfile -t ALL_RUNS < <(printf '%s\n' "${ALL_RUNS[@]}" | awk '!seen[$0]++')

RUNS=(); EXISTING=()
for r in "${ALL_RUNS[@]}"; do
    if [ -e "${OUTPUT_DIR}/${r}.root" ]; then
        EXISTING+=("${r}")
        [ ${ONLY_MISSING} -eq 1 ] && continue
    fi
    RUNS+=("${r}")
done
if [ ${#EXISTING[@]} -gt 0 ] && [ ${ONLY_MISSING} -eq 0 ] && [ ${OVERWRITE} -eq 0 ]; then
    die "${#EXISTING[@]} of ${#ALL_RUNS[@]} runs already have an output file in ${OUTPUT_DIR} (e.g. ${EXISTING[0]}.root). Use --only-missing to submit only the others, --overwrite to replace them, or another -o"
fi
[ ${#RUNS[@]} -gt 0 ] || { echo "Nothing to submit: all ${#ALL_RUNS[@]} runs have an output file in ${OUTPUT_DIR}."; exit 0; }

# ── HTCondor and grid proxy ──────────────────────────────────────────────────
command -v condor_submit > /dev/null || die "condor_submit not found (run this on a submission node)"
if [ ${USE_PROXY} -eq 0 ]; then
    PROXY=""
else
    # The jobs fall back to this file if HTCondor's copy of the proxy is missing on the worker node
    # (e.g. no file transfer on a shared filesystem), so it must be readable from the worker nodes
    PROXY=$(realpath -m "${PROXY:-${SCRIPT_DIR}/x509up_u$(id -u)}")
    case "${PROXY}" in
        /tmp/*|/var/tmp/*) warn "the grid proxy ${PROXY} is in a local directory of this node: jobs may not be able to read it (use --proxy)" ;;
    esac
    if voms-proxy-info -file "${PROXY}" -exists -valid 24:00 > /dev/null 2>&1; then
        echo "Using the existing grid proxy ${PROXY} (valid for more than 24 h)."
    elif [ ${DRY_RUN} -eq 1 ]; then
        echo "Dry run: a new grid proxy would be created in ${PROXY}."
    else
        echo "Creating a grid proxy in ${PROXY}..."
        voms-proxy-init -valid 96:00 -out "${PROXY}" || die "voms-proxy-init failed (or use --no-proxy if the jobs do not need one)"
    fi
fi

# ── Job directory: snapshot of everything the jobs use ───────────────────────
STAMP=$(date +%Y%m%d_%H%M%S)
TEMP_JOB_DIR=0
if [ -z "${JOB_DIR}" ]; then
    if [ ${DRY_RUN} -eq 1 ]; then JOB_DIR=$(mktemp -d); TEMP_JOB_DIR=1; else JOB_DIR="${OUTPUT_DIR}/submission_${STAMP}"; fi
fi
JOB_DIR=$(realpath -m "${JOB_DIR}")
[ ! -e "${JOB_DIR}" ] || [ -z "$(ls -A "${JOB_DIR}")" ] || die "job directory ${JOB_DIR} already exists and is not empty"
[ ${DRY_RUN} -eq 1 ] || mkdir -p "${OUTPUT_DIR}"
mkdir -p "${JOB_DIR}/logs" "${JOB_DIR}/config_args"

cp -p "${BUILD_DIR}/analysis" "${JOB_DIR}/"
cp -rp "${BUILD_DIR}/config" "${JOB_DIR}/config"
cp -p "${SCRIPT_DIR}/run_analysis.sh" "${JOB_DIR}/"
printf '%s\n' "${RUNS[@]}" > "${JOB_DIR}/run_list.txt"

# Configs given as analysis arguments: copy them and point the arguments to the copies
n=0
while [ $n -lt ${#CONFIG_ARG_SRC[@]} ]; do
    option=${CONFIG_ARG_SRC[$n]#--}; src=${CONFIG_ARG_SRC[$((n + 1))]}
    copy="${JOB_DIR}/config_args/${option}_$(basename "${src}")"
    cp -p "${src}" "${copy}"
    for k in "${!FINAL_ARGS[@]}"; do
        [ "${FINAL_ARGS[$k]}" = "@CONFIG_$((n + 2))@" ] && FINAL_ARGS[$k]=${copy}
    done
    n=$((n + 2))
done

# ── Submit file ──────────────────────────────────────────────────────────────
# New-style HTCondor arguments: the whole list in double quotes, each argument in single quotes
# (a literal ' is written '' and a literal " is written ""), so arguments may contain spaces
condor_quote() { local s=${1//\'/\'\'}; s=${s//\"/\"\"}; printf "'%s'" "${s}"; }
JOB_ARGS=("${JOB_DIR}" "${LCG_SETUP}" '$(runNumber)' "${OUTPUT_DIR}/\$(runNumber).root" ${FINAL_ARGS[@]+"${FINAL_ARGS[@]}"})
CONDOR_ARGS=""
for a in "${JOB_ARGS[@]}"; do CONDOR_ARGS+="${CONDOR_ARGS:+ }$(condor_quote "${a}")"; done

SUB="${JOB_DIR}/analysis.sub"
{
    echo "# Generated by run_submission.sh on $(date) (see submission_info.txt)"
    echo "executable     = ${JOB_DIR}/run_analysis.sh"
    echo "arguments      = \"${CONDOR_ARGS}\""
    echo ""
    echo "request_memory = ${MEMORY_MB}M"
    echo "request_cpus   = ${REQUEST_CPUS}"
    echo "request_disk   = ${DISK_MB}M"
    echo ""
    echo "log            = ${JOB_DIR}/logs/analysis_\$(ClusterId).log"
    echo "output         = ${JOB_DIR}/logs/analysis_\$(ClusterId).\$(ProcId)-\$(runNumber).out"
    echo "error          = ${JOB_DIR}/logs/analysis_\$(ClusterId).\$(ProcId)-\$(runNumber).err"
    echo ""
    echo "+JobFlavour    = \"${FLAVOUR}\""
    if [ -n "${PROXY}" ]; then
        echo "use_x509userproxy = True"
        echo "x509userproxy  = ${PROXY}"
        echo "environment    = \"X509_USER_PROXY_SHARED=${PROXY}\""
    fi
    echo ""
    echo "queue runNumber from ${JOB_DIR}/run_list.txt"
} > "${SUB}"

# ── Submission record (reproducibility) ──────────────────────────────────────
GIT() { git -C "${REPO_DIR}" --no-optional-locks "$@" 2>/dev/null; }
{
    echo "date:            $(date --iso-8601=seconds)"
    echo "user@host:       $(id -un)@$(hostname)"
    echo "working dir:     ${PWD}"
    echo "command:         ${ORIGINAL_COMMAND}"
    echo "repository:      ${REPO_DIR}"
    echo "git commit:      $(GIT rev-parse HEAD || echo unknown)"
    echo "git describe:    $(GIT describe --always --dirty || echo unknown)"
    echo "build dir:       ${BUILD_DIR}"
    echo "binary md5:      $(md5sum "${BUILD_DIR}/analysis" | cut -d' ' -f1)"
    echo "LCG setup:       ${LCG_SETUP}"
    echo "output dir:      ${OUTPUT_DIR}"
    echo "run list:        ${RUN_LIST} (${#RUNS[@]} runs submitted, ${#ALL_RUNS[@]} in the list)"
    echo "analysis args:   ${FINAL_ARGS[*]-}"
    echo "request:         ${REQUEST_CPUS} cpus, ${MEMORY_MB} MB memory, ${DISK_MB} MB disk, flavour ${FLAVOUR}"
    echo ""
    echo "git status:"
    GIT status --short || true
} > "${JOB_DIR}/submission_info.txt"
if [ -n "$(GIT status --porcelain --untracked-files=no)" ]; then
    warn "the repository has uncommitted changes: saved to ${JOB_DIR}/git_diff.patch"
    GIT diff HEAD > "${JOB_DIR}/git_diff.patch" || true
fi

# ── Submit ───────────────────────────────────────────────────────────────────
echo "Job directory: ${JOB_DIR}"
echo "Output:        ${OUTPUT_DIR}/<run>.root"
echo "Runs:          ${#RUNS[@]}$([ ${ONLY_MISSING} -eq 1 ] && echo " (skipped ${#EXISTING[@]} with existing output)")"
echo "Analysis args: ${FINAL_ARGS[*]-}"

if [ ${DRY_RUN} -eq 1 ]; then
    echo ""
    echo "Dry run: the first job would run"
    FIRST=("${JOB_ARGS[@]//\$(runNumber)/${RUNS[0]}}")
    echo "  $(printf '%q ' "${JOB_DIR}/run_analysis.sh" "${FIRST[@]}")"
    echo "Checking the submit file with condor_submit -dry-run..."
    if condor_submit -dry-run "${JOB_DIR}/dry_run.ads" "${SUB}" > "${JOB_DIR}/dry_run.txt" 2>&1; then
        tail -n 1 "${JOB_DIR}/dry_run.txt"
    else
        cat "${JOB_DIR}/dry_run.txt" >&2; die "condor_submit -dry-run failed"
    fi
    if [ ${TEMP_JOB_DIR} -eq 1 ]; then
        rm -rf "${JOB_DIR}"
        echo "Dry run: nothing submitted (give -w <dir> to keep the generated job directory)."
    else
        echo "Dry run: nothing submitted. Submit file: ${SUB}"
    fi
    exit 0
fi

(cd "${JOB_DIR}" && condor_submit "${SUB}") | tee "${JOB_DIR}/condor_submit.txt"
echo ""
echo "Monitor:        condor_q    (held jobs: condor_q -hold, details: condor_q -better-analyze <ClusterId>)"
echo "Resubmit failed or missing runs with the same arguments:"
RESUBMIT=("$0" -o "${OUTPUT_DIR}" -r "${RUN_LIST}" --only-missing)
[ ${#ANALYSIS_ARGS[@]} -eq 0 ] || RESUBMIT+=(-- "${ANALYSIS_ARGS[@]}")
echo "  $(printf '%q ' "${RESUBMIT[@]}")"
