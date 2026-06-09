## get software stack from LCG in CVMFS
## this setups both ROOT and Geant4
## docs: https://lcginfo.cern.ch/
echo "Setting up LGC_107 software stack..."
source /cvmfs/sft.cern.ch/lcg/views/LCG_110/x86_64-el9-gcc15-opt/setup.sh

## for CERNBOX eos access
export EOS_MGM_URL=root://eosuser.cern.ch

echo "Setup completed!"