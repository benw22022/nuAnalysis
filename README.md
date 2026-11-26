# nuAnalysis

## Introduction

Analysis framework for running the electron muon neutrino analysis without the need to produce intermidiary parquet files

## Getting started

The framework creates an executable called `analysis` which is configured to run over one run at a time (TODO: add multirun functionality)
The filepaths to the NTuple data and waveform data are defined in the file configs in `AnalysisZipFramework/config/files/` (see [File configs](#file-configs)) and the GRL files which are used to get the good times and the luminosity are listed in `AnalysisZipFramework/config/grl_config.yaml`. All framework configs are YAML (so they can contain comments) and are read with yaml-cpp; the official FASER GRL files themselves are JSON and are read with nlohmann_json. Both libraries come with the LCG view sourced by `setup.sh`.

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

To run on a directory of NTuples without adding them to the file config (e.g. for tests):

```bash
./analysis --input-dir /path/to/ntuples -o test.root [--isMC]
./analysis --input-dir /path/to/data_ntuples --aux-dir /path/to/waveform_ntuples -o test.root
```

All `.root` files directly in the directory are used (sorted by name); they must all have the same format (e.g. not data and MC mixed). The run numbers are read from the files and used for the GRL and the `meta` tree as with `-r`.

Other options:

| Option | Description |
|---|---|
| `--input-dir <dir>` | Instead of `-r`: run on all `.root` files in this directory (not subdirectories; can be given more than once). The file config is not used and the run number(s) are read from the files |
| `--aux-dir <dir>` | With `--input-dir`: aux (waveform) files for the reduced charge, for data that needs them. Only for input from a single run |
| `--analysis-config <yaml>` | Analysis config: which configs to use (default `config/analyses/muon_neutrino.yaml`, see [Analysis configs](#analysis-configs)). The `--*-config` options below override its sections |
| `--file-config <yaml>` | File config to use instead of the analysis config's file configs, e.g. `config/files/late_tracks/2024.yaml` |
| `--grl-config <yaml>` | GRL config to use (default `config/grl_config.yaml`) |
| `--output-config <yaml>` | Which columns to save in the output `nt` tree (default `config/output_columns.yaml`) |
| `--definitions-config <yaml>` | New column definitions (default `config/definitions.yaml`) |
| `--categories-config <yaml>` | MC categories (signal / background components) used to split histograms and cutflows on MC (default `config/categories.yaml`) |
| `--cuts-config <yaml>` | Cuts and histograms (default `config/cuts_muon_neutrino.yaml`) |
| `--isMC` | MC mode: GRL, BCID and trigger cuts are skipped; the MC truth selection is done with the MC categories (see below) |
| `--no-reduced-charge` | Do not use the VetoNu reduced charge; veto on the raw VetoNu charge instead |
| `-j [n]` | Enable multithreading (all cores, or `n` threads) |
| `-v` | Verbose output |

*Note*: By default the configs are read from `build/config`. The `*.yaml` files are copied there from `AnalysisZipFramework/config` when `cmake` is run, so re-run `cmake` after editing them (or point `--file-config` / `--grl-config` at the source files).

## Analysis configs

An analysis config lists the configs an analysis uses, so one option selects a whole setup:

```bash
./analysis -r 16000 --analysis-config config/analyses/early_trigger.yaml -o out.root
```

```yaml
file_configs:
- ../files/backward/2024.yaml       # paths are relative to the analysis config's directory (or absolute)
- ../files/mc/2024_calonu.yaml
GRL:
- ../grl_config.yaml
Output:
- ../output_columns.yaml
- keep: [PS_TriggeredEarly]         # inline entry
Definitions:
- ../definitions.yaml
- PS_TriggeredEarly:                # inline entry
    expression: "((inputBits & 64) == 64) && ((inputBits & 4) == 0) && ((inputBitsNext & 4) == 4)"
    data_type: DATA
Categories:
- ../categories.yaml
Cuts:
- ../cuts_earlyTrigger.yaml
```

- Sections: `file_configs`, `GRL`, `Output`, `EventID_pass`, `Definitions`, `Categories`, `Cuts` (any case). Each is a list of config files and/or inline entries. An inline entry is written like the contents of that file type under its section key (`Cuts` / `Definitions` / `Categories`: `<name>: {...}`; `file_configs`: `<run>: {data_paths: ...}`; `Output`: the `nt:` settings; `GRL`: `grl_jsons` / `grl_csvs`).
- Entries are concatenated in order (definitions are created in that order). Names must be unique across all entries of a section (cuts, histograms, definitions, categories, run numbers); a duplicate is an error. `Output`: the `save_all_*` switches are on if on in any entry, the `keep` / `drop` lists are concatenated.
- A section that is left out means no config of that type (e.g. no `Categories`: no MC split; no `Output`: `nt` only has `run` and `eventID`). Without `--analysis-config`, `config/analyses/muon_neutrino.yaml` is used.
- A `--*-config` option on the command line replaces the whole section.
- `EventID_pass`: extra columns for the `eventID_pass` tree, e.g. `- keep_mc: [nu_interaction_type]` (`keep`: all samples, `keep_mc`: MC only, `keep_data`: data only; column names or glob patterns; a name matching no column gives a warning). Without it, `eventID_pass` has `run`, `eventID`, the `passed_<cut>` flags and `mc_category`.
- `GRL` is only used for data and is optional: without it a data job gives a warning, the luminosity in `meta` is -1, and `GoodTimes` / `ExcludedTimes` are not defined, so a cut or definition using them stops the job with an error. MC jobs do not use the GRL. `Categories` are only used for MC, so data-only analyses (e.g. `early_trigger`) can leave them out.
- `config/analyses/` has one analysis config per setup: `muon_neutrino.yaml` (the defaults), `dimuon.yaml`, `dimuon_full.yaml`, `dimuon_signal_only.yaml`, `early_trigger.yaml`.

Every config used (the command line, the analysis config, each config file as read and each inline entry) is saved in the `config` tree of the output file.

## File configs

The input files are listed in `config/files/`, one file config per data-taking year and reconstruction type, and one per MC geometry:

| File configs | Content |
|---|---|
| `files/backward/{2022,2023,2024}.yaml` | Data: official reconstruction with backward tracking, preselection of at least one track |
| `files/unfilt/{2022,2023,2024}.yaml` | Data: official reconstruction with forward tracking, no preselection (unfiltered) |
| `files/late_tracks/{2022,2023,2024}.yaml` | Data: private reconstruction with tracks from late hits |
| `files/mc/2022_23.yaml`, `files/mc/2024_fasernu.yaml`, `files/mc/2024_calonu.yaml` | GENIE MC for the 2022/23, 2024 FaserNu and 2024 CaloNu geometries (FaserMC TWiki) |

An analysis config lists the ones it uses (the same run number must not appear twice, so use one reconstruction type per job). Each run has `data_paths`, and optionally `waveform_paths` (aux NTuples with the VetoNu reduced charge, data only), `description`, `is_nominal` (nominal MC sample or a variation) and `lumi_sample` (generated luminosity of an MC sample in fb^-1):

```yaml
runs:
  200139:
    data_paths:
      - "root://eospublic.cern.ch//eos/experiment/fasernu-data0/faser/sim/mc24/genie/200139/bck/s0015/*"
    description: "2024 CaloNu EPOS pi"
    is_nominal: true
    lumi_sample: 10000   # fb^-1
```

`description`, `is_nominal` and `lumi_sample` are written to the `meta` tree (`""` / -1 if not given). The nominal MC samples are EPOS pi, EPOS K and POWHEG+Pythia charm central (default flux prediction); all GENIE samples have 10 ab^-1 = 10,000 fb^-1.

The data file configs are regenerated with `AnalysisZipFramework/scripts/make_fileconfig.py` (run where `/eos` is mounted, e.g. lxplus; `--types` selects reconstruction types). The MC file configs are written by hand.

## NTuple compatibility (older and 2024 NTuples)

- Old NTuples name the veto scintillator branches `VetoSt<N>_<var>`. Each gets an alias `Veto<N>_<var>`, so the code (and configs) can always use the new names, e.g. `Veto10_raw_charge`.
- Veto11 was not read out in 2022-2023 (and some MC has no Veto11 branches). If the input has no `Veto11_*` branches, every `Veto11_<var>` falls back to `Veto10_<var>`. A warning is printed at the end of the job for each `Veto11_*` column that was actually used, with the number of events. The fallback columns are not included by `save_all_*` in the output config, only by explicit `keep` entries.
- From 2024 the calorimeter has a dual readout: `CaloLo<N>_<var>` / `CaloHi<N>_<var>` (and `CaloLo_total_<var>` / `CaloHi_total_<var>`) instead of the single readout `Calo<N>_<var>` / `Calo_total_<var>` used in earlier data and in the MC. If the input has `CaloLo` branches but no single-readout `Calo` branches, every `Calo...` column falls back to the corresponding `CaloLo...` (low-gain) column, so the same code and configs work for all years. As for Veto11, a warning is printed at the end of the job for each `Calo` column that was actually used, and these columns are only saved by explicit `keep` entries. MC-only variables such as `Calo_total_*_fudged` have no data equivalent.

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
- `meta`: Contains run number, luminosity (`lumi`, from the GRL, pb^-1), number of input events, and from the file config `description`, `is_nominal` (1/0, -1 if not given) and `lumi_sample` (generated MC luminosity, fb^-1, -1 if not given)
- `cutflow`: Contains the cutflow
- `eventID_passed`: Contains run number, eventIDs and boolean flags for each cut defined in the analysis
- `histograms/`: Histograms booked in the cuts config, with the N-1 histograms in `histograms/n_minus_1/`
- `config`: every config used for this output (branches `section`, `origin`, `content`): the command line, the analysis config, and the full text of each config file and inline entry

On MC with categories (see below) there is also:

- `histograms/<category>/`, `histograms/signal/`, `histograms/background/` (and the same under `histograms/n_minus_1/`): each histogram per MC category and summed over the signal / background categories
- `cutflow_by_category/`: one cutflow tree per category, and for `signal` and `background`
- `categories`: name, type, LaTeX label, colour and expression of each category
- `mc_category` in `nt` and `eventID_pass`: index of the event's category (-1: none, -2: more than one)

## Batch submission

`AnalysisZipFramework/submission/run_submission.sh` submits one HTCondor job per run number (no paths to edit; it finds the repository from its own location):

```bash
cd AnalysisZipFramework/submission
./run_submission.sh -o /path/to/output_v1                                   # runs in run_list.txt
./run_submission.sh -o /path/to/mc_v1 -r mc_runs.txt -- --isMC -j 4        # analysis arguments after --
./run_submission.sh -o /path/to/output_v1 --only-missing                    # resubmit failed / missing runs
./run_submission.sh -o /path/to/output_v1 -n                                # dry run: checks only
```

Each job writes `<output_dir>/<run>.root`. Everything after `--` is passed to `./analysis` (`-j N` also sets `request_cpus`), e.g. `-- --analysis-config config/analyses/dimuon.yaml`. An analysis config must be in `build/config` (i.e. in `AnalysisZipFramework/config`, copied by `cmake`): the jobs use its copy in the job directory, together with the copies of the configs it lists. Run with `-h` for all options (build directory, LCG setup, memory, job flavour, grid proxy, ...).

Before submitting, the script checks that `build/config` matches `AnalysisZipFramework/config` (re-run `cmake` otherwise) and `build/include/RDFDefines.h` matches `include/RDFDefines.h` (run `make` otherwise) and warns if sources are newer than the binary or the repository has uncommitted changes. For every submission it makes a job directory (default `<output_dir>/submission_<date>_<time>`) with a snapshot of the binary, `build/config`, `build/include/RDFDefines.h` and any config passed as an argument (so rebuilding or editing configs or `RDFDefines.h` does not affect running jobs), the run list, the generated submit file, `submission_info.txt` (command, git commit, binary checksum, ...), `git_diff.patch` for uncommitted changes, and the job logs. Jobs write to the worker node's scratch disk and copy the output only if `analysis` succeeded, so failed jobs leave no partial files.

## Cuts and histograms

Cuts and histograms are configured in a cuts config, e.g. `config/cuts_muon_neutrino.yaml` (select one with `--cuts-config` or an analysis config). Cuts are applied in the order of the file:

```yaml
Histograms:                       # booked on all events, before the first cut
  - name: pz_all_events
    x: {variable: LeadTrack_pz0, edges: [0, 50, 100, 200, 500, 1000]}

Cuts:
  Track pz > 100 GeV:             # name in the cutflow (eventID_pass flag: passed_<name>)
    expression: "LeadTrack_pz0 > 100"
    data_type: ALL                # optional: ALL, DATA or MC
    requires: [reduced_charge]    # optional: reduced_charge, aux_reduced_charge, native_reduced_charge, no_reduced_charge
    histograms:                   # optional, booked after this cut
      - name: theta_vs_pz
        title: "Theta vs pz;p_{z} [GeV];#theta [mrad]"
        data_type: ALL            # optional
        x: {variable: LeadTrack_pz0, bins: 100, min: 0, max: 5000}
        y: {variable: LeadTrack_Theta, edges: [0, 5, 10, 25, 50]}   # optional: makes it 2D
    n_minus_1:                    # optional: N-1 histograms (all other applied cuts, not this one)
      - name: LeadTrack_pz0_Nminus1
        x: {variable: LeadTrack_pz0, bins: 100, min: 0, max: 5000}
```

Each histogram axis has either `bins`/`min`/`max` or `edges`. Histogram names must be unique (across `Histograms`, `histograms` and `n_minus_1`).

`n_minus_1` histograms are N-1 plots: they are filled with every event that passes all the *other* cuts applied to the sample (anywhere in the list, before or after this cut), with only this cut removed. They take the same options as other histograms and are written to `histograms/n_minus_1/` in the output file. They are filled in the same event loop as everything else and do not change the cutflow. If the cut itself is not applied to the sample (`data_type`/`requires`), its N-1 histograms are skipped. Histograms attached to a cut are booked at that point of the selection even if the cut itself is not applied to the current sample; they follow their own `data_type`/`requires`. Quote expressions, especially ones starting with `!` (YAML reads an unquoted leading `!` as a tag). The header of `config/cuts.yaml` documents all options.

## MC categories (signal and background)

MC events are sorted into categories defined in `config/categories.yaml` (select another file with `--categories-config`):

```yaml
Categories:
  CC_nu_mu_fiducial:
    expression: "is_CC_num && (decay_box || decay_lead) && truth_dec_r < 100 && truth_pz_nu > 100"
    type: signal                  # signal or background
    latex: 'CC $\nu_\mu$ fid.'    # optional label for plots: use SINGLE quotes for LaTeX
    colour: red                   # optional colour for plots
  NC_nu:
    expression: "is_nc"
    type: background
```

The categories replace the MC truth selection cuts (and the old `--isAsimov` mode): all MC events go through the reconstruction selection and are sorted into signal and background. On MC every histogram, including the N-1 histograms, is also filled once per category, plus `other` for events in no category, and summed over the signal and over the background categories (`other` counts as background). The cutflow is also written per category. Categories should not overlap; events in more than one category are reported with a warning at the end of the job. Add `split: false` to a histogram to book only the total (each copy costs memory, more with `-j`). Cuts and histograms can also use `data_type: signal`, `background`, `other` or a category name: a cut then only applies to events of those categories, and a histogram is only filled with them. On data the categories are not used. A categories file with no categories (`Categories:`) switches the splitting off.

Write LaTeX labels in single quotes: in double quotes YAML reads backslashes as escapes (`"\nu"` becomes a newline followed by `u`, and `"\mu"` is a parse error).

## Definitions

New columns are defined in `config/definitions.yaml` (select another file with `--definitions-config`), in the order of the file:

```yaml
Definitions:
  LeadTrack_pz0:                     # new column name
    expression: "SafeAt(Track_pz0, LeadTrack_Idx) / 1000"
  truth_pz_nu:
    expression: "SafeAt(truth_pz, 0) / 1000"
    data_type: MC                    # optional: ALL, DATA or MC (as in cuts.yaml)
  VetoNu_total_reduced_charge:
    expression: "VetoNu0_reduced_charge + VetoNu1_reduced_charge"
    requires: [reduced_charge]       # optional (as in cuts.yaml)
```

The definitions are created after the built-in columns, so they can use them: the run period flags `isCaloNuPeriod` and `is2024Period`, the scintillator status flags, the reduced charge and its helper columns, `GoodTimes`/`ExcludedTimes` and the Veto aliases/fallbacks. These built-ins stay in C++ (`Analysis::BuildDataFrame()` in `AnalysisZipFramework/source/Analyis.cxx`) because the reduced-charge code relies on them. The header of `config/definitions.yaml` lists them.

When adding columns in C++, use `Analysis::Define`, `Analysis::Redefine` and `Analysis::Alias` rather than `m_node->Define(...)`: they apply the same `data_type` meaning as the YAML configs, give a clear error if the name already exists, and record every column (name, expression, `data_type`, origin) in the definition log, which is summarised in the job output (full list with `-v`).

Expressions can use the helper functions in `AnalysisZipFramework/include/RDFDefines.h` (e.g. `SafeAt`, `Radius`, `Theta`, `inFaserNuBox`), for example `expression: "Theta(Track_px0, Track_py0, Track_pz0)"`. For more complicated logic, add a C++ function there and call it from the YAML expression.

*Note*: `RDFDefines.h` is compiled at run time, not into the binary. `make` copies it to `build/include` and `./analysis` uses `include/RDFDefines.h` in the working directory (falling back, with a warning, to the source file), and batch jobs use a copy in their job directory, so you can edit it while jobs are running. Run `make` after editing it. The file used is saved in the `config` tree of the output (section `rdf_defines`).