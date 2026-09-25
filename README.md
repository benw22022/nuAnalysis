# nuAnalysis

## Introduction

Analysis framework for running the electron muon neutrino analysis without the need to produce intermidiary parquet files

## Getting started

The framework creates an executable called `analysis` which is configured to run over one run at a time (TODO: add multirun functionality)
The filepaths to the NTuple data and waveform data are defined in `AnalysisZipFramework/config/file_config.yaml` and the GRL files which are used to get the good times and the luminosity are listed in `AnalysisZipFramework/config/grl_config.yaml`. All framework configs are YAML (so they can contain comments) and are read with yaml-cpp; the official FASER GRL files themselves are JSON and are read with nlohmann_json. Both libraries come with the LCG view sourced by `setup.sh`.

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

```bash
mkdir build
cd build
cmake ../AnalysisZipFramework
make
```

- Run analysis

```bash
./analysis -r <run_number> -o <output_file>
```

Other options:

| Option | Description |
|---|---|
| `--file-config <yaml>` | File config to use (default `config/file_config.yaml`), e.g. `config/file_config_late_tracks.yaml` |
| `--grl-config <yaml>` | GRL config to use (default `config/grl_config.yaml`) |
| `--output-config <yaml>` | Which columns to save in the output `nt` tree (default `config/output_columns.yaml`) |
| `--isMC` | MC mode: GRL, BCID and trigger cuts are skipped |
| `--isAsimov` | Asimov mode: MC without the truth selection cuts |
| `--no-reduced-charge` | Do not use the VetoNu reduced charge; veto on the raw VetoNu charge instead |
| `-j [n]` | Enable multithreading (all cores, or `n` threads) |
| `-v` | Verbose output |

*Note*: By default the configs are read from `build/config`. The `*.yaml` files are copied there from `AnalysisZipFramework/config` when `cmake` is run, so re-run `cmake` after editing them (or point `--file-config` / `--grl-config` at the source files).

The file config can be regenerated with `AnalysisZipFramework/scripts/make_fileconfig.py` (data runs only; MC runs are added by hand).

## Choosing the columns saved in `nt`

`config/output_columns.yaml` selects which columns are written to the `nt` tree:

```yaml
nt:
  save_all_input_columns: false   # every branch of the input NTuple
  save_all_defined_columns: true  # every column created at runtime with Define/Redefine
  keep: [eventTime, "Veto*_charge"]   # exact names or glob patterns (*, ?, [...])
  keep_mc: ["truth_*"]                # MC only
  keep_data: []                       # data only
  drop: ["Track_*"]                   # applied last
```

A warning is printed for any entry that matches no column. `run` and `eventID` are always saved. The default config saves all columns. Saving fewer columns reduces the output file size, the memory used by the output buffers and the time spent writing.

## Output file structure
The output file contains the following trees:

- `nt`: Contains the kinematics from the physics NTuples as well as any additional variable definitions
- `meta`: Contains run number and luminosity
- `cutflow`: Contains the cutflow
- `eventID_passed`: Contains run number, eventIDs and boolean flags for each cut defined in the analysis

## Batch submission 

An example condor submission script can be found here `AnalysisZipFramework/submission/run_submission.sh`. This file, and `AnalysisZipFramework/submission/run_analysis.sh` need to be modified so that file paths align with your own.

## Modifying cuts

Currently definitions and cuts are hardcoded in `AnalysisZipFramework/source/Analyis.cxx`. The member variable `m_node` is the main ROOT `RDataFrame` object.

New variable definitions should be added in `Analysis::BuildDataFrame()` using `m_node = m_node->Define(...)`

New cuts should be defined in `Analysis::Run()`. Rather than calling `m_node->Filter(...)` directly, one should use the `applyCut` method, which handles some additional book keeping. The syntax is: `applyCut(<cut expression>, <cut name>);`, e.g. `applyCut("LeadTrack_Theta < 25", "Leading track theta < 25 mrad");`. The cut expression follows the usual RDF syntax.

The RDF Definition syntax is extended through this header file `AnalysisZipFramework/include/RDFDefines.h` which defines some addtional shorthand methods, for example: `m_node = m_node->Define("Track_Theta", "Theta(Track_px0, Track_py0, Track_pz0)");`.