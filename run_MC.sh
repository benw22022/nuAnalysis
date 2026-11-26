#!/bin/bash
# Run the analysis on all MC samples (--isMC), then merge the outputs per sample.
# Replaces run_MC_truth.sh and run_MC_asimov.sh: the MC truth selection (signal / background)
# is now done with the MC categories (config/categories.yaml), so one MC mode covers both.

cd build
cmake ../AnalysisZipFramework
make

# Each run appears once (two parallel jobs writing the same output file would corrupt it)

./analysis -r 200093 --analysis-config config/analyses/muon_neutrino.yaml -o 200093.bck.root --isMC &
./analysis -r 200094 --analysis-config config/analyses/muon_neutrino.yaml -o 200094.bck.root --isMC &
./analysis -r 200100 --analysis-config config/analyses/muon_neutrino.yaml -o 200100.bck.root --isMC &
./analysis -r 200160 --analysis-config config/analyses/muon_neutrino.yaml -o 200160.bck.root --isMC &
./analysis -r 200161 --analysis-config config/analyses/muon_neutrino.yaml -o 200161.bck.root --isMC &
./analysis -r 200162 --analysis-config config/analyses/muon_neutrino.yaml -o 200162.bck.root --isMC &
./analysis -r 200163 --analysis-config config/analyses/muon_neutrino.yaml -o 200163.bck.root --isMC &
./analysis -r 200164 --analysis-config config/analyses/muon_neutrino.yaml -o 200164.bck.root --isMC &
./analysis -r 200165 --analysis-config config/analyses/muon_neutrino.yaml -o 200165.bck.root --isMC &
./analysis -r 200166 --analysis-config config/analyses/muon_neutrino.yaml -o 200166.bck.root --isMC &
./analysis -r 200167 --analysis-config config/analyses/muon_neutrino.yaml -o 200167.bck.root --isMC &
./analysis -r 200168 --analysis-config config/analyses/muon_neutrino.yaml -o 200168.bck.root --isMC &
./analysis -r 200169 --analysis-config config/analyses/muon_neutrino.yaml -o 200169.bck.root --isMC &
./analysis -r 200170 --analysis-config config/analyses/muon_neutrino.yaml -o 200170.bck.root --isMC &
./analysis -r 200171 --analysis-config config/analyses/muon_neutrino.yaml -o 200171.bck.root --isMC &
wait

./analysis -r 200080 --analysis-config config/analyses/muon_neutrino.yaml -o 200080.bck.root --isMC &
./analysis -r 200084 --analysis-config config/analyses/muon_neutrino.yaml -o 200084.bck.root --isMC &
./analysis -r 200086 --analysis-config config/analyses/muon_neutrino.yaml -o 200086.bck.root --isMC &
./analysis -r 200081 --analysis-config config/analyses/muon_neutrino.yaml -o 200081.bck.root --isMC &
./analysis -r 200085 --analysis-config config/analyses/muon_neutrino.yaml -o 200085.bck.root --isMC &
./analysis -r 200087 --analysis-config config/analyses/muon_neutrino.yaml -o 200087.bck.root --isMC &
./analysis -r 200088 --analysis-config config/analyses/muon_neutrino.yaml -o 200088.bck.root --isMC &
./analysis -r 200090 --analysis-config config/analyses/muon_neutrino.yaml -o 200090.bck.root --isMC &
wait

# 2024 CaloNu
./analysis -r 200139 --analysis-config config/analyses/muon_neutrino.yaml -o 200139.bck.root --isMC &
./analysis -r 200140 --analysis-config config/analyses/muon_neutrino.yaml -o 200140.bck.root --isMC &
./analysis -r 200146 --analysis-config config/analyses/muon_neutrino.yaml -o 200146.bck.root --isMC &
./analysis -r 200172 --analysis-config config/analyses/muon_neutrino.yaml -o 200172.bck.root --isMC &
./analysis -r 200173 --analysis-config config/analyses/muon_neutrino.yaml -o 200173.bck.root --isMC &
./analysis -r 200174 --analysis-config config/analyses/muon_neutrino.yaml -o 200174.bck.root --isMC &
./analysis -r 200175 --analysis-config config/analyses/muon_neutrino.yaml -o 200175.bck.root --isMC &
./analysis -r 200176 --analysis-config config/analyses/muon_neutrino.yaml -o 200176.bck.root --isMC &
./analysis -r 200177 --analysis-config config/analyses/muon_neutrino.yaml -o 200177.bck.root --isMC &
./analysis -r 200178 --analysis-config config/analyses/muon_neutrino.yaml -o 200178.bck.root --isMC &
./analysis -r 200179 --analysis-config config/analyses/muon_neutrino.yaml -o 200179.bck.root --isMC &
./analysis -r 200180 --analysis-config config/analyses/muon_neutrino.yaml -o 200180.bck.root --isMC &
./analysis -r 200181 --analysis-config config/analyses/muon_neutrino.yaml -o 200181.bck.root --isMC &
./analysis -r 200182 --analysis-config config/analyses/muon_neutrino.yaml -o 200182.bck.root --isMC &
./analysis -r 200183 --analysis-config config/analyses/muon_neutrino.yaml -o 200183.bck.root --isMC &
wait

./analysis -r 200091 --analysis-config config/analyses/muon_neutrino.yaml -o 200091.bck.root --isMC &
./analysis -r 200095 --analysis-config config/analyses/muon_neutrino.yaml -o 200095.bck.root --isMC &
./analysis -r 200097 --analysis-config config/analyses/muon_neutrino.yaml -o 200097.bck.root --isMC &
./analysis -r 200092 --analysis-config config/analyses/muon_neutrino.yaml -o 200092.bck.root --isMC &
./analysis -r 200096 --analysis-config config/analyses/muon_neutrino.yaml -o 200096.bck.root --isMC &
./analysis -r 200098 --analysis-config config/analyses/muon_neutrino.yaml -o 200098.bck.root --isMC &
./analysis -r 200099 --analysis-config config/analyses/muon_neutrino.yaml -o 200099.bck.root --isMC &
./analysis -r 200101 --analysis-config config/analyses/muon_neutrino.yaml -o 200101.bck.root --isMC &
wait

# 2022-23 / 2024 no CaloNu
./analysis -r 200082 --analysis-config config/analyses/muon_neutrino.yaml -o 200082.bck.root --isMC &
./analysis -r 200083 --analysis-config config/analyses/muon_neutrino.yaml -o 200083.bck.root --isMC &
./analysis -r 200089 --analysis-config config/analyses/muon_neutrino.yaml -o 200089.bck.root --isMC &
./analysis -r 200104 --analysis-config config/analyses/muon_neutrino.yaml -o 200104.bck.root --isMC &
./analysis -r 200105 --analysis-config config/analyses/muon_neutrino.yaml -o 200105.bck.root --isMC &
./analysis -r 200106 --analysis-config config/analyses/muon_neutrino.yaml -o 200106.bck.root --isMC &
./analysis -r 200107 --analysis-config config/analyses/muon_neutrino.yaml -o 200107.bck.root --isMC &
./analysis -r 200108 --analysis-config config/analyses/muon_neutrino.yaml -o 200108.bck.root --isMC &
./analysis -r 200109 --analysis-config config/analyses/muon_neutrino.yaml -o 200109.bck.root --isMC &
./analysis -r 200110 --analysis-config config/analyses/muon_neutrino.yaml -o 200110.bck.root --isMC &
./analysis -r 200111 --analysis-config config/analyses/muon_neutrino.yaml -o 200111.bck.root --isMC &
./analysis -r 200112 --analysis-config config/analyses/muon_neutrino.yaml -o 200112.bck.root --isMC &
./analysis -r 200113 --analysis-config config/analyses/muon_neutrino.yaml -o 200113.bck.root --isMC &
./analysis -r 200114 --analysis-config config/analyses/muon_neutrino.yaml -o 200114.bck.root --isMC &
./analysis -r 200115 --analysis-config config/analyses/muon_neutrino.yaml -o 200115.bck.root --isMC &
wait

./analysis -r 200137 --analysis-config config/analyses/muon_neutrino.yaml -o 200137.bck.root --isMC &
./analysis -r 200141 --analysis-config config/analyses/muon_neutrino.yaml -o 200141.bck.root --isMC &
./analysis -r 200143 --analysis-config config/analyses/muon_neutrino.yaml -o 200143.bck.root --isMC &
./analysis -r 200138 --analysis-config config/analyses/muon_neutrino.yaml -o 200138.bck.root --isMC &
./analysis -r 200142 --analysis-config config/analyses/muon_neutrino.yaml -o 200142.bck.root --isMC &
./analysis -r 200144 --analysis-config config/analyses/muon_neutrino.yaml -o 200144.bck.root --isMC &
./analysis -r 200145 --analysis-config config/analyses/muon_neutrino.yaml -o 200145.bck.root --isMC &
./analysis -r 200147 --analysis-config config/analyses/muon_neutrino.yaml -o 200147.bck.root --isMC &
wait

# ── Merge per sample (from run_MC_truth.sh) ──────────────────────────────────
hadd -f 2024_noCaloNu.bck.root                     200093.bck.root 200094.bck.root 200100.bck.root &
hadd -f 2024_noCaloNu_FTF_BIC.bck.root             200160.bck.root 200161.bck.root 200162.bck.root &
hadd -f 2024_noCaloNu_FTFP_INCLXX.bck.root         200163.bck.root 200164.bck.root 200165.bck.root &
hadd -f 2024_noCaloNu_FTFP_BERT_HP_JEFF33.bck.root 200166.bck.root 200167.bck.root 200168.bck.root &
hadd -f 2024_noCaloNu_FTFP_BERT_HP_ENDFB8.bck.root 200169.bck.root 200170.bck.root 200171.bck.root &
wait 

hadd -f 2024_CaloNu.bck.root                     200139.bck.root 200140.bck.root 200146.bck.root &
hadd -f 2024_CaloNu_FTF_BIC.bck.root             200172.bck.root 200173.bck.root 200174.bck.root &
hadd -f 2024_CaloNu_FTFP_INCLXX.bck.root         200175.bck.root 200176.bck.root 200177.bck.root &
hadd -f 2024_CaloNu_FTFP_BERT_HP_JEFF33.bck.root 200178.bck.root 200179.bck.root 200180.bck.root &
hadd -f 2024_CaloNu_FTFP_BERT_HP_ENDFB8.bck.root 200181.bck.root 200182.bck.root 200183.bck.root &
wait

hadd -f 2022_2023.bck.root                     200082.bck.root 200083.bck.root 200089.bck.root &
hadd -f 2022_2023_FTF_BIC.bck.root             200104.bck.root 200105.bck.root 200106.bck.root &
hadd -f 2022_2023_FTFP_INCLXX.bck.root         200107.bck.root 200108.bck.root 200109.bck.root &
hadd -f 2022_2023_FTFP_BERT_HP_JEFF33.bck.root 200110.bck.root 200111.bck.root 200112.bck.root &
hadd -f 2022_2023_FTFP_BERT_HP_ENDFB8.bck.root 200113.bck.root 200114.bck.root 200115.bck.root &
wait

# ── Merge of the nominal samples (from run_MC_asimov.sh) ─────────────────────
hadd -f merged_all.root 200080.bck.root 200082.bck.root 200084.bck.root 200086.bck.root 200081.bck.root 200083.bck.root 200085.bck.root 200087.bck.root 200088.bck.root 200089.bck.root 200090.bck.root 200091.bck.root 200093.bck.root 200095.bck.root 200097.bck.root 200092.bck.root 200094.bck.root 200096.bck.root 200098.bck.root 200099.bck.root 200100.bck.root 200101.bck.root 200137.bck.root 200139.bck.root 200141.bck.root 200143.bck.root 200138.bck.root 200140.bck.root 200142.bck.root 200144.bck.root 200145.bck.root 200146.bck.root 200147.bck.root
wait
