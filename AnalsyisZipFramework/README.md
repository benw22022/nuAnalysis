# Analysis Framework

## Introduction

Analysis framework for running the electron muon neutrino analysis without the need to produce intermidiary parquet files

## Getting started

The framework creates an executable called `analysis` which is configured to run over one run at a time (TODO: add multirun functionality)
The filepaths to the NTuple data and waveform data are defined in `AnalsyisZipFramework/config/file_config.json` and the grl files which are used to get the good times and the luminosity are stored in `AnalsyisZipFramework/config/grl_config.json`

To get started do:

- Clone repository

```bash
git clone https://github.com/benw22022/nuAnalysis.git
```

- Setup environment 

```bash
source AnalysisZipFramework/setup.sh
```

- Compile code
```
mkdir build
cd build
cmake ../AnalysisZipFramework
make
```

- Run analysis

```bash
./analysis -r <run_number> -o <output_file>
```
