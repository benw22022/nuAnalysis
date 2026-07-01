# nuAnalysis

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

*Note*: The filepaths and GRLs are obtained from `build/config`, where the configs are obtained at runtime. They are copied from the source folder to the build directory when executing `cmake`.

## Output file structure
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