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
| `--definitions-config <yaml>` | New column definitions (default `config/definitions.yaml`) |
| `--cuts-config <yaml>` | Cuts and histograms (default `config/cuts.yaml`) |
| `--isMC` | MC mode: GRL, BCID and trigger cuts are skipped |
| `--isAsimov` | Asimov mode: MC without the truth selection cuts |
| `--no-reduced-charge` | Do not use the VetoNu reduced charge; veto on the raw VetoNu charge instead |
| `-j [n]` | Enable multithreading (all cores, or `n` threads) |
| `-v` | Verbose output |

*Note*: By default the configs are read from `build/config`. The `*.yaml` files are copied there from `AnalysisZipFramework/config` when `cmake` is run, so re-run `cmake` after editing them (or point `--file-config` / `--grl-config` at the source files).

The file config can be regenerated with `AnalysisZipFramework/scripts/make_fileconfig.py` (data runs only; MC runs are added by hand).

## Older NTuples (backwards compatibility)

- Old NTuples name the veto scintillator branches `VetoSt<N>_<var>`. Each gets an alias `Veto<N>_<var>`, so the code (and configs) can always use the new names, e.g. `Veto10_raw_charge`.
- Veto11 was not read out in 2022-2023 (and some MC has no Veto11 branches). If the input has no `Veto11_*` branches, every `Veto11_<var>` falls back to `Veto10_<var>`. A warning is printed at the end of the job for each `Veto11_*` column that was actually used, with the number of events. The fallback columns are not included by `save_all_*` in the output config, only by explicit `keep` entries.

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

## Cuts and histograms

Cuts and histograms are configured in `config/cuts.yaml` (select another file with `--cuts-config`). Cuts are applied in the order of the file:

```yaml
Histograms:                       # booked on all events, before the first cut
  - name: pz_all_events
    x: {variable: LeadTrack_pz0, edges: [0, 50, 100, 200, 500, 1000]}

Cuts:
  Track pz > 100 GeV:             # name in the cutflow (eventID_pass flag: passed_<name>)
    expression: "LeadTrack_pz0 > 100"
    data_type: ALL                # optional: ALL, DATA, MC (not in Asimov mode) or ASIMOV (all MC)
    requires: [reduced_charge]    # optional: reduced_charge, aux_reduced_charge, native_reduced_charge, no_reduced_charge
    histograms:                   # optional, booked after this cut
      - name: theta_vs_pz
        title: "Theta vs pz;p_{z} [GeV];#theta [mrad]"
        data_type: ALL            # optional
        x: {variable: LeadTrack_pz0, bins: 100, min: 0, max: 5000}
        y: {variable: LeadTrack_Theta, edges: [0, 5, 10, 25, 50]}   # optional: makes it 2D
```

Each histogram axis has either `bins`/`min`/`max` or `edges`. Histogram names must be unique. Histograms attached to a cut are booked at that point of the selection even if the cut itself is not applied to the current sample; they follow their own `data_type`/`requires`. Quote expressions, especially ones starting with `!` (YAML reads an unquoted leading `!` as a tag). The header of `config/cuts.yaml` documents all options.

## Definitions

New columns are defined in `config/definitions.yaml` (select another file with `--definitions-config`), in the order of the file:

```yaml
Definitions:
  LeadTrack_pz0:                     # new column name
    expression: "SafeAt(Track_pz0, LeadTrack_Idx) / 1000"
  truth_pz_nu:
    expression: "SafeAt(truth_pz, 0) / 1000"
    data_type: ASIMOV                # optional: ALL, DATA, MC or ASIMOV (as in cuts.yaml)
  VetoNu_total_reduced_charge:
    expression: "VetoNu0_reduced_charge + VetoNu1_reduced_charge"
    requires: [reduced_charge]       # optional (as in cuts.yaml)
```

The definitions are created after the built-in columns, so they can use them: the run period flags `isCaloNuPeriod` and `is2024Period`, the scintillator status flags, the reduced charge and its helper columns, `GoodTimes`/`ExcludedTimes` and the Veto aliases/fallbacks. These built-ins stay in C++ (`Analysis::BuildDataFrame()` in `AnalysisZipFramework/source/Analyis.cxx`) because the reduced-charge code relies on them. The header of `config/definitions.yaml` lists them.

Expressions can use the helper functions in `AnalysisZipFramework/include/RDFDefines.h` (e.g. `SafeAt`, `Radius`, `Theta`, `inFaserNuBox`), for example `expression: "Theta(Track_px0, Track_py0, Track_pz0)"`. For more complicated logic, add a C++ function there and call it from the YAML expression.