cd build
cmake ../AnalysisZipFramework
make

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

./analysis -r 200091 -o 200091.bck.root --isAsimov &
./analysis -r 200095 -o 200095.bck.root --isAsimov &
./analysis -r 200097 -o 200097.bck.root --isAsimov &
./analysis -r 200092 -o 200092.bck.root --isAsimov &
./analysis -r 200096 -o 200096.bck.root --isAsimov &
./analysis -r 200098 -o 200098.bck.root --isAsimov &
./analysis -r 200099 -o 200099.bck.root --isAsimov &
./analysis -r 200100 -o 200100.bck.root --isAsimov &
./analysis -r 200101 -o 200101.bck.root --isAsimov &
wait

./analysis -r 200137 -o 200137.bck.root --isAsimov &
./analysis -r 200141 -o 200141.bck.root --isAsimov &
./analysis -r 200143 -o 200143.bck.root --isAsimov &
./analysis -r 200138 -o 200138.bck.root --isAsimov &
./analysis -r 200142 -o 200142.bck.root --isAsimov &
./analysis -r 200144 -o 200144.bck.root --isAsimov &
./analysis -r 200145 -o 200145.bck.root --isAsimov &
./analysis -r 200147 -o 200147.bck.root --isAsimov &
wait
