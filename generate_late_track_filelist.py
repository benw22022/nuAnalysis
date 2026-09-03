import json
import os
import glob
from copy import deepcopy


def main():

    path_lt_2022 = "/eos/user/b/bewilson/LateTracks/NTuples-Full/2022"
    path_lt_2023 = "/eos/user/b/bewilson/LateTracks/NTuples-Full/2023"
    path_lt_2024 = "/eos/user/b/bewilson/LateTracks/NTuples-Full/2024"


    # Load the existing file config
    with open("AnalysisZipFramework/config/file_config.json", "r") as f:
        file_config = json.load(f)

    new_file_config = deepcopy(file_config)

    for run_num, file_info in file_config.items():

        data_path = file_info["data_paths"][0]  # Assuming there's only one data path per run

        run_num_fmted = data_path.split("/")[-2]  # Extract the run number from the path

        if "2022" in data_path:
            new_data_path = f"{path_lt_2022}/{run_num_fmted}/*.root"
        elif "2023" in data_path:
            new_data_path = f"{path_lt_2023}/{run_num_fmted}/*.root"
        elif "2024" in data_path:
            new_data_path = f"{path_lt_2024}/{run_num_fmted}/*.root"
        else:
            print(f"Unknown run number: {run_num}")
            continue

        new_data_path = f"root://eosuser.cern.ch/{new_data_path}"
        
        # Update the file_info with the new file list
        new_file_config[run_num]["data_paths"] = [new_data_path]

    with open("AnalysisZipFramework/config/file_config_late_tracks.json", "w") as f:
        json.dump(new_file_config, f, indent=4)

if __name__ == "__main__":
    main()