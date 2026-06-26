#!/bin/bash
# run_analysis.sh

# Source LCG view
source /cvmfs/sft.cern.ch/lcg/views/LCG_109/x86_64-el9-gcc15-opt/setup.sh

# Move to the build directory where the binary and config/ live
cd /afs/cern.ch/user/b/bewilson/work/SaturationRateChecks/nuAnalysis/build

echo "Starting analysis for run $1"
echo "Output: $2"

./analysis --run $1 -o $2

echo "Exit code: $?"