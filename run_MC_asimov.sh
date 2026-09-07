#!/bin/bash

cd build
make

# 2022-23
./analysis -r 200093 -o 200093.bck.root --isAsimov &
./analysis -r 200094 -o 200094.bck.root --isAsimov &
./analysis -r 200100 -o 200100.bck.root --isAsimov &
./analysis -r 200160 -o 200160.bck.root --isAsimov &
./analysis -r 200161 -o 200161.bck.root --isAsimov &
./analysis -r 200162 -o 200162.bck.root --isAsimov &
./analysis -r 200163 -o 200163.bck.root --isAsimov &
./analysis -r 200164 -o 200164.bck.root --isAsimov &
./analysis -r 200165 -o 200165.bck.root --isAsimov &
./analysis -r 200166 -o 200166.bck.root --isAsimov &
./analysis -r 200167 -o 200167.bck.root --isAsimov &
./analysis -r 200168 -o 200168.bck.root --isAsimov &
./analysis -r 200169 -o 200169.bck.root --isAsimov &
./analysis -r 200170 -o 200170.bck.root --isAsimov &
./analysis -r 200171 -o 200171.bck.root --isAsimov &
wait

# 2024 CaloNu
./analysis -r 200139 -o 200139.bck.root --isAsimov &
./analysis -r 200140 -o 200140.bck.root --isAsimov &
./analysis -r 200146 -o 200146.bck.root --isAsimov &
./analysis -r 200172 -o 200172.bck.root --isAsimov &
./analysis -r 200173 -o 200173.bck.root --isAsimov &
./analysis -r 200174 -o 200174.bck.root --isAsimov &
./analysis -r 200175 -o 200175.bck.root --isAsimov &
./analysis -r 200176 -o 200176.bck.root --isAsimov &
./analysis -r 200177 -o 200177.bck.root --isAsimov &
./analysis -r 200178 -o 200178.bck.root --isAsimov &
./analysis -r 200179 -o 200179.bck.root --isAsimov &
./analysis -r 200180 -o 200180.bck.root --isAsimov &
./analysis -r 200181 -o 200181.bck.root --isAsimov &
./analysis -r 200182 -o 200182.bck.root --isAsimov &
./analysis -r 200183 -o 200183.bck.root --isAsimov &
wait

# 2024 No CaloNu
./analysis -r 200082 -o 200082.bck.root --isAsimov &
./analysis -r 200083 -o 200083.bck.root --isAsimov &
./analysis -r 200089 -o 200089.bck.root --isAsimov &
./analysis -r 200104 -o 200104.bck.root --isAsimov &
./analysis -r 200105 -o 200105.bck.root --isAsimov &
./analysis -r 200106 -o 200106.bck.root --isAsimov &
./analysis -r 200107 -o 200107.bck.root --isAsimov &
./analysis -r 200108 -o 200108.bck.root --isAsimov &
./analysis -r 200109 -o 200109.bck.root --isAsimov &
./analysis -r 200110 -o 200110.bck.root --isAsimov &
./analysis -r 200111 -o 200111.bck.root --isAsimov &
./analysis -r 200112 -o 200112.bck.root --isAsimov &
./analysis -r 200113 -o 200113.bck.root --isAsimov &
./analysis -r 200114 -o 200114.bck.root --isAsimov &
./analysis -r 200115 -o 200115.bck.root --isAsimov &
wait

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