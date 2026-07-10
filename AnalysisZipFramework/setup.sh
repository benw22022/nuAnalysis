## get software stack from LCG in CVMFS
## this setups both ROOT and Geant4
## docs: https://lcginfo.cern.ch/
echo "Setting up LGC_109 software stack..."
source /cvmfs/sft.cern.ch/lcg/views/LCG_109/x86_64-el9-gcc15-opt/setup.sh

## for CERNBOX eos access
export EOS_MGM_URL=root://eosuser.cern.ch

## Supress ROOT RDF snapshot info messages
export ROOT_RDF_SNAPSHOT_INFO=0

echo "Setup completed!"