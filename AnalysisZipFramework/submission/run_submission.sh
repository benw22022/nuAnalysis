# Create the logs directory first (HTCondor won't create it)
mkdir -p logs
mkdir -p /home/ppd/bewilson/work/nuAnalysis/AnalysisZipFramework/submission/analysis_output-noVetoNu

# Dry run to check expansion
# condor_submit -dry-run - analysis.sub outputDir=/eos/home-b/bewilson/analysis_output

# # Real submission
voms-proxy-init -valid 96:0 -out ./x509up_u$(id -u)
condor_submit analysis.sub outputDir=/home/ppd/bewilson/work/nuAnalysis/AnalysisZipFramework/submission/analysis_output-noVetoNu

# # Monitor
# condor_q
# condor_q -better-analyze <ClusterId>   # if jobs go on hold