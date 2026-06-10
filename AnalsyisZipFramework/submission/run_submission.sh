# Create the logs directory first (HTCondor won't create it)
mkdir -p logs

mkdir -p /eos/home-b/bewilson/analysis_output

# Dry run to check expansion
condor_submit -dry-run - analysis.sub outputDir=/eos/home-b/bewilson/analysis_output

# # Real submission
condor_submit analysis.sub outputDir=/eos/home-b/bewilson/analysis_output

# # Monitor
# condor_q
# condor_q -better-analyze <ClusterId>   # if jobs go on hold