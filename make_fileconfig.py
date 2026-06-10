import os 
import json
import glob

def preappend_to_filepaths(paths, preappend="root://eospublic.cern.ch/"):
    # return [preappend + path for path in paths]
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

    waveforms_2022_files = "/eos/user/b/bewilson/El9CalypsoForWaveForm/WaveForms-2022/*"
    waveforms_2023_files = "/eos/user/b/bewilson/El9CalypsoForWaveForm/Waveforms-2023/*"
    waveforms_2024_files = "/eos/user/b/bewilson/El9CalypsoForWaveForm/waveforms-5FilesPerChunk/*"
    waveforms_2024_CaloNu_files = "/eos/user/b/bewilson/El9CalypsoForWaveForm/WaveformsCaloNuPeriod/*"

    caloNu_run_range = [15821, 16924]


    data_2022_cfg = {}
    data_2023_cfg = {}
    data_2024_CaloNu_cfg = {}
    data_2024_NoCaloNu_cfg = {}

    waveforms_2022_cfg = {}
    waveforms_2023_cfg = {}
    waveforms_2024_NoCaloNu_cfg = {}
    waveforms_2024_CaloNu_cfg = {}

    for file in glob.glob(data_2022_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        data_2022_cfg[run_number] = preappend_to_filepaths(files)

    for file in glob.glob(data_2023_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        data_2023_cfg[run_number] = preappend_to_filepaths(files)

    for file in glob.glob(data_2024_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] <= run_number <= caloNu_run_range[1]:
            data_2024_CaloNu_cfg[run_number] = preappend_to_filepaths(files)
        else:
            data_2024_NoCaloNu_cfg[run_number] = preappend_to_filepaths(files)
        
    
    for file in glob.glob(waveforms_2022_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        waveforms_2022_cfg[run_number] = preappend_to_filepaths(files)
    
    for file in glob.glob(waveforms_2023_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")
        waveforms_2023_cfg[run_number] = preappend_to_filepaths(files)
    
    for file in glob.glob(waveforms_2024_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] <= run_number <= caloNu_run_range[1]:
            continue
        else:
            waveforms_2024_NoCaloNu_cfg[run_number] = preappend_to_filepaths(files)

    for file in glob.glob(waveforms_2024_CaloNu_files):
        run_number = int(file.split("/")[-1])
        files = os.path.join(file, "*.root")

        if caloNu_run_range[0] <= run_number <= caloNu_run_range[1]:
            waveforms_2024_CaloNu_cfg[run_number] = preappend_to_filepaths(files)
        else:
            continue

    data_all = data_2022_cfg | data_2023_cfg | data_2024_CaloNu_cfg | data_2024_NoCaloNu_cfg
    waveforms_all = waveforms_2022_cfg | waveforms_2023_cfg | waveforms_2024_CaloNu_cfg | waveforms_2024_NoCaloNu_cfg

    run_cfg = {}

    for run_number in data_all.keys():
        run_cfg[run_number] = {
            "data_paths": [data_all[run_number]],
            "waveform_paths": [waveforms_all.get(run_number, [])]
        }

    for run_number, cfg in run_cfg.items():
        
        data_paths = cfg["data_paths"]
        waveform_paths = cfg["waveform_paths"]

        if len(data_paths) == 0:
            print(f"Warning: No data paths for run {run_number}")
        if len(waveform_paths) == 0:
            print(f"Warning: No waveform paths for run {run_number}")


    run_list = list(run_cfg.keys())
    with open("run_list.txt", "w") as f:
        for run_number in run_list:
            f.write(f"{run_number}\n")

    with open("file_config.json", "w") as f:
        json.dump(run_cfg, f, indent=4)

if __name__ == "__main__":
    main()