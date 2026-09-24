import os 
import json
import glob

# Run ranges used to group runs (with comment headers) in the YAML file config
CALONU_RUN_RANGE = (15821, 16924)

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


FILE_CONFIG_HEADER = """\
# ─────────────────────────────────────────────────────────────────────────────
# nuAnalysis file config: input files for each run
#
# runs:
#   <run number>:
#     data_paths:      physics NTuples (tree 'nt'). Wildcards are expanded by TChain.
#     waveform_paths:  aux NTuples (tree 'tree') with the VetoNu reduced charge, looked up
#                      by eventID. Only needed for data NTuples that predate the reduced
#                      charge branches; leave empty ([]) otherwise.
#
# Select a different file config with: ./analysis --file-config <path>
# ─────────────────────────────────────────────────────────────────────────────
"""


def write_file_config_yaml(run_cfg, path, header=FILE_CONFIG_HEADER, extra_comment=None):
    """
    Write the file config as YAML, grouped by data-taking period with comment headers.
    Written by hand (no PyYAML needed) so that the comments and layout are preserved.
    Strings are double-quoted (json.dumps quoting is valid YAML).
    """
    lines = [header.rstrip("\n")]
    if extra_comment:
        lines += ["#", *[f"# {l}" if l else "#" for l in extra_comment.splitlines()]]
    lines += ["", "runs:"]

    def fmt_list(key, values):
        if not values:
            return [f"    {key}: []"]
        return [f"    {key}:"] + [f"      - {json.dumps(v)}" for v in values]

    current_period = None
    for run_number in sorted(run_cfg, key=int):
        period = run_period(int(run_number))
        if period != current_period:
            lines += ["", f"  # ── {period} " + "─" * max(3, 70 - len(period))]
            current_period = period
        cfg = run_cfg[run_number]
        lines.append(f"  {int(run_number)}:")
        lines += fmt_list("data_paths", cfg["data_paths"])
        lines += fmt_list("waveform_paths", cfg["waveform_paths"])

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

def preappend_to_filepaths(paths, preappend=""):
    if isinstance(paths, str):
        return (preappend + paths).replace("/eos/home-b/", "/eos/user/b/")
    elif isinstance(paths, list):
        return [(preappend + path).replace("/eos/home-b/", "/eos/user/b/") for path in paths]
    return paths


def get_parse_GRL(grl_path):

    run_lumi_dict = {}
    with open(grl_path, 'r') as f:
        for i, line in enumerate(f):
            if i == 0: continue
            if line.startswith('#'): continue

            spline = line.split(',')
            run_number = int(spline[0])
            lumi_rec = float(spline[3])

            run_lumi_dict[run_number] = lumi_rec / 1000 # pb^-1 -> fb^-1
    
    return run_lumi_dict


def main():


    data_2022_files = "/eos/experiment/faser/data0/phys/2022_back/r0021/*"
    data_2023_files = "/eos/experiment/faser/data0/phys/2023_back/r0021/*"
    data_2024_files = "/eos/experiment/faser/data0/phys/2024_back/r0022/*"

    waveforms_2022_files = "/eos/home-b/bewilson/El9CalypsoForWaveForm/waveforms-2022/*"
    waveforms_2023_files = "/eos/home-b/bewilson/El9CalypsoForWaveForm/Waveforms-2023/*"

    waveforms_2024_files = "/eos/home-b/bewilson/El9CalypsoForWaveForm/waveforms-5FilesPerChunk/*"
    waveforms_2024_CaloNu_files = "/eos/home-b/bewilson/El9CalypsoForWaveForm/WaveformsCaloNuPeriod/*"
    waveforms_2024_postCaloNu_files = "/eos/home-b/bewilson/El9CalypsoForWaveForm/waveforms_postCaloNu/*"

    caloNu_run_range = CALONU_RUN_RANGE


    data_2022_cfg = {}
    data_2023_cfg = {}
    data_2024_CaloNu_cfg = {}
    data_2024_NoCaloNu_cfg = {}

    waveforms_2022_cfg = {}
    waveforms_2023_cfg = {}
    waveforms_2024_NoCaloNu_cfg = {}
    waveforms_2024_CaloNu_cfg = {}
    waveforms_2024_PostCaloNu_cfg = {}

    for file in glob.glob(data_2022_files):

        if not os.path.isdir(file):
            continue

        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        data_2022_cfg[run_number] = preappend_to_filepaths(files, "root://eospublic.cern.ch/")

    for file in glob.glob(data_2023_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        data_2023_cfg[run_number] = preappend_to_filepaths(files, "root://eospublic.cern.ch/")

    for file in glob.glob(data_2024_files):
        if not os.path.isdir(file):
            continue

        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] <= run_number <= caloNu_run_range[1]:
            data_2024_CaloNu_cfg[run_number] = preappend_to_filepaths(files, "root://eospublic.cern.ch/")
        else:
            data_2024_NoCaloNu_cfg[run_number] = preappend_to_filepaths(files, "root://eospublic.cern.ch/")
        
    
    for file in glob.glob(waveforms_2022_files):
        if not os.path.isdir(file):
            continue
        
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        waveforms_2022_cfg[run_number] = preappend_to_filepaths(files, "root://eosuser.cern.ch/")
    
    for file in glob.glob(waveforms_2023_files):
        if not os.path.isdir(file):
            continue
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        waveforms_2023_cfg[run_number] = preappend_to_filepaths(files, "root://eosuser.cern.ch/")
    
    for file in glob.glob(waveforms_2024_files):
        if not os.path.isdir(file):
            continue
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] > run_number:
            waveforms_2024_NoCaloNu_cfg[run_number] = preappend_to_filepaths(files, "root://eosuser.cern.ch/")
        else:
            continue

    for file in glob.glob(waveforms_2024_CaloNu_files):
        if not os.path.isdir(file):
            continue
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] <= run_number <= caloNu_run_range[1]:
            waveforms_2024_CaloNu_cfg[run_number] = preappend_to_filepaths(files, "root://eosuser.cern.ch/")
        else:
            continue

    for file in glob.glob(waveforms_2024_postCaloNu_files):
        if not os.path.isdir(file):
            continue
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if run_number > caloNu_run_range[1]:
            waveforms_2024_PostCaloNu_cfg[run_number] = preappend_to_filepaths(files, "root://eosuser.cern.ch/")
        else:
            continue

    data_all = data_2022_cfg | data_2023_cfg | data_2024_CaloNu_cfg | data_2024_NoCaloNu_cfg
    waveforms_all = waveforms_2022_cfg | waveforms_2023_cfg | waveforms_2024_CaloNu_cfg | waveforms_2024_NoCaloNu_cfg | waveforms_2024_PostCaloNu_cfg

    run_cfg = {}

    for run_number in data_all.keys():
        run_cfg[run_number] = {
            "data_paths": [data_all[run_number]],
            # [] if there are no waveform files for this run (previously this gave [[]])
            "waveform_paths": [waveforms_all[run_number]] if run_number in waveforms_all else []
        }

    for run_number, cfg in run_cfg.items():
        
        data_paths = cfg["data_paths"]
        waveform_paths = cfg["waveform_paths"]

        if len(data_paths) == 0:
            print(f"Warning: No data paths for run {run_number}")
        if len(waveform_paths) == 0:
            print(f"Warning: No waveform paths for run {run_number}")


    run_list = list(run_cfg.keys())
    with open("../config/run_list.txt", "w") as f:
        for run_number in run_list:
            f.write(f"{run_number}\n")

    # NOTE: only data runs are found by the directory scan above. MC runs (2000xx) in the
    # existing config/file_config.yaml were added by hand and must be copied across.
    write_file_config_yaml(run_cfg, "../config/file_config.yaml",
                           extra_comment="Generated by scripts/make_fileconfig.py")

if __name__ == "__main__":
    main()