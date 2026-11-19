"""
Write the data file configs: one per data-taking year and reconstruction type

    config/files/<type>/<year>.yaml      type: backward, unfilt, late_tracks; year: 2022, 2023, 2024

by scanning the run directories on EOS, and config/run_list.txt (all runs with backward-tracking
data). The MC file configs (config/files/mc/*.yaml) are written by hand.

Run where /eos is mounted (e.g. lxplus), from this directory:

    python3 make_fileconfig.py                      # writes ../config/files/... and ../config/run_list.txt
    python3 make_fileconfig.py --types unfilt       # only one reconstruction type

Options --eos-mount and --output-dir are for testing (scan a copy of the directory tree, write elsewhere).
"""
import argparse
import glob
import json
import os

# Runs of the 2024 CaloNu period (the waveform NTuples are in a different directory for these)
CALONU_RUN_RANGE = (15821, 16924)

# Reconstruction types: description (for the file header), xrootd server and the run directories per year.
# Each run directory (e.g. .../007729) contains the NTuples of that run.
RECO_TYPES = {
    "backward": {
        "description": "Official reconstruction with backward tracking, preselection: at least one track",
        "server": "root://eospublic.cern.ch/",
        "dirs": {
            2022: "/eos/experiment/faser/data0/phys/2022_back/r0021",
            2023: "/eos/experiment/faser/data0/phys/2023_back/r0021",
            2024: "/eos/experiment/faser/data0/phys/2024_back/r0022",
        },
    },
    "unfilt": {
        "description": "Official reconstruction with forward tracking, no preselection (unfiltered)",
        "server": "root://eospublic.cern.ch/",
        "dirs": {
            2022: "/eos/experiment/faser/data0/phys/2022_unfilt/r0021-r0023",
            2023: "/eos/experiment/faser/data0/phys/2023_unfilt/r0021-r0023",
            2024: "/eos/experiment/faser/data0/phys/2024_unfilt/r0022-r0023",
        },
    },
    "late_tracks": {
        "description": "Private reconstruction with tracks from late hits",
        "server": "root://eosuser.cern.ch/",
        "dirs": {
            2022: "/eos/user/b/bewilson/LateTracks/NTuples-Full/2022",
            2023: "/eos/user/b/bewilson/LateTracks/NTuples-Full/2023",
            2024: "/eos/user/b/bewilson/LateTracks/NTuples-Full/2024",
        },
    },
}

# Aux (waveform) NTuples with the VetoNu reduced charge: (directory, server, runs it is used for).
# The same for every reconstruction type.
WAVEFORMS = [
    ("/eos/user/b/bewilson/El9CalypsoForWaveForm/waveforms-2022", "root://eosuser.cern.ch/",
     lambda run: run < 10000),
    ("/eos/user/b/bewilson/El9CalypsoForWaveForm/Waveforms-2023", "root://eosuser.cern.ch/",
     lambda run: 10000 <= run < 14000),
    ("/eos/user/b/bewilson/El9CalypsoForWaveForm/waveforms-5FilesPerChunk", "root://eosuser.cern.ch/",
     lambda run: 14000 <= run < CALONU_RUN_RANGE[0]),
    ("/eos/user/b/bewilson/El9CalypsoForWaveForm/WaveformsCaloNuPeriod", "root://eosuser.cern.ch/",
     lambda run: CALONU_RUN_RANGE[0] <= run <= CALONU_RUN_RANGE[1]),
    ("/eos/user/b/bewilson/El9CalypsoForWaveForm/waveforms_postCaloNu", "root://eosuser.cern.ch/",
     lambda run: run > CALONU_RUN_RANGE[1]),
]

FILE_CONFIG_HEADER = """\
# ─────────────────────────────────────────────────────────────────────────────
# nuAnalysis file config: input files for each run
#
# runs:
#   <run number>:
#     data_paths:      physics NTuples (tree 'nt'). Wildcards are expanded by TChain.
#     waveform_paths:  aux NTuples (tree 'tree') with the VetoNu reduced charge, looked up
#                      by eventID. Only for data NTuples that predate the reduced charge
#                      branches (optional).
#     description:     what the sample is (optional, saved in the meta tree)
#     is_nominal:      true for the nominal MC samples, false for variations (optional, saved in the meta tree)
#     lumi_sample:     generated luminosity of an MC sample in fb^-1 (optional, saved in the meta tree)
#
# File configs are listed in an analysis config (config/analyses/*.yaml), or given with
# ./analysis --file-config <path>. A run number must not appear in two file configs of one job.
# ─────────────────────────────────────────────────────────────────────────────
"""


def run_period(run_number):
    """Human-readable data-taking period for a run number (used for comments in the YAML)."""
    if run_number >= 200000:
        return "MC"
    if run_number < 10000:
        return "2022 data"
    if run_number < 14000:
        return "2023 data"
    if run_number < CALONU_RUN_RANGE[0]:
        return "2024 data (before CaloNu period)"
    if run_number <= CALONU_RUN_RANGE[1]:
        return f"2024 data (CaloNu period, runs {CALONU_RUN_RANGE[0]}-{CALONU_RUN_RANGE[1]})"
    return "2024 data (after CaloNu period)"


def write_file_config_yaml(groups, path, header=FILE_CONFIG_HEADER, extra_comment=None):
    """
    Write a file config as YAML.
      groups: list of (group comment, {run number: run config}); each group gets a comment header.
      run config: data_paths (list); optional waveform_paths (list, omitted if empty), description (str),
                  is_nominal (bool), lumi_sample (number, fb^-1) and comment (str, written above the run).
    Written by hand (no PyYAML needed) so that the comments and layout are preserved.
    Strings are double-quoted (json.dumps quoting is valid YAML).
    """
    lines = [header.rstrip("\n")]
    if extra_comment:
        lines += ["#", *[f"# {l}" if l else "#" for l in extra_comment.splitlines()]]
    lines += ["", "runs:"]

    for group, runs in groups:
        if not runs:
            continue
        lines += ["", f"  # ── {group} " + "─" * max(3, 70 - len(group))]
        for run_number in sorted(runs, key=int):
            cfg = runs[run_number]
            if cfg.get("comment"):
                lines.append(f"  # {cfg['comment']}")
            lines.append(f"  {int(run_number)}:")
            lines.append("    data_paths:")
            lines += [f"      - {json.dumps(p)}" for p in cfg["data_paths"]]
            if cfg.get("waveform_paths"):
                lines.append("    waveform_paths:")
                lines += [f"      - {json.dumps(p)}" for p in cfg["waveform_paths"]]
            if "description" in cfg:
                lines.append(f"    description: {json.dumps(cfg['description'])}")
            if "is_nominal" in cfg:
                lines.append(f"    is_nominal: {'true' if cfg['is_nominal'] else 'false'}")
            if "lumi_sample" in cfg:
                lines.append(f"    lumi_sample: {cfg['lumi_sample']:g}   # fb^-1")

    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


def group_by_period(run_cfg):
    """Groups for write_file_config_yaml: data runs grouped by data-taking period."""
    groups = {}
    for run_number in sorted(run_cfg, key=int):
        groups.setdefault(run_period(int(run_number)), {})[run_number] = run_cfg[run_number]
    return list(groups.items())


def run_dirs(directory, eos_mount=""):
    """{run number: directory} for the run directories (named by run number) in `directory`."""
    runs = {}
    for d in glob.glob(eos_mount + directory + "/*"):
        name = os.path.basename(d)
        if os.path.isdir(d) and name.isdigit():
            runs[int(name)] = directory + "/" + name   # path on EOS (without the test mount prefix)
    return runs


def scan_waveforms(eos_mount=""):
    """{run number: xrootd path of the aux NTuples}"""
    waveforms = {}
    for directory, server, used_for in WAVEFORMS:
        for run, d in run_dirs(directory, eos_mount).items():
            if used_for(run):
                waveforms[run] = server + d + "/*.root"
    return waveforms


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--types", nargs="+", choices=list(RECO_TYPES), default=list(RECO_TYPES),
                        help="reconstruction types to write (default: all)")
    parser.add_argument("--output-dir", default="../config", help="config directory (default: ../config)")
    parser.add_argument("--eos-mount", default="", help="prefix of a copy of the EOS directory tree (testing)")
    args = parser.parse_args()

    waveforms = scan_waveforms(args.eos_mount)

    for reco_type in args.types:
        info = RECO_TYPES[reco_type]
        for year, directory in info["dirs"].items():
            run_cfg = {}
            for run, d in run_dirs(directory, args.eos_mount).items():
                run_cfg[run] = {"data_paths": [info["server"] + d + "/*.root"],
                                "waveform_paths": [waveforms[run]] if run in waveforms else []}
                if run not in waveforms:
                    print(f"Warning: no waveform (aux) files for {reco_type} run {run}")
            path = os.path.join(args.output_dir, "files", reco_type, f"{year}.yaml")
            write_file_config_yaml(group_by_period(run_cfg), path,
                                   extra_comment=f"{year} data: {info['description']}.\n"
                                                 "Generated by scripts/make_fileconfig.py")
            print(f"Wrote {path}: {len(run_cfg)} runs")

            if reco_type == "backward":
                with open(os.path.join(args.output_dir, "run_list.txt"), "a" if year != min(info["dirs"]) else "w") as f:
                    f.writelines(f"{run}\n" for run in sorted(run_cfg))


if __name__ == "__main__":
    main()
