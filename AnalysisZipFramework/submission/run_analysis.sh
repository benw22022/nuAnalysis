#!/bin/bash
# run_analysis.sh

# Source LCG view
source /cvmfs/sft.cern.ch/lcg/views/LCG_109/x86_64-el9-gcc15-opt/setup.sh

## Supress ROOT RDF snapshot info messages
export ROOT_RDF_SNAPSHOT_INFO=0

# Move to the build directory where the binary and config/ live
cd /home/ppd/bewilson/work/nuAnalysis/build

echo "Starting analysis for run $1"
echo "Output: $2"

./analysis --run $1 -o $2

echo "Exit code: $?"