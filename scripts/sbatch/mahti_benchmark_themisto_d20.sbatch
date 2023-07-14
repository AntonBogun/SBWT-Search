#!/bin/bash

# Meant to be run on Mahti supercomputer by CSC: https://research.csc.fi/-/mahti


#SBATCH --job-name=themisto_d20
#SBATCH --account=dongelr1
#SBATCH --error themisto_d20_err.txt
#SBATCH --output themisto_d20_out.txt
#SBATCH --nodes 1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=32
#SBATCH --mem-per-cpu=14G
#SBATCH --gres=gpu:a100:0,nvme:350

##SBATCH --partition=gputest
##SBATCH --time=00:15:00

#SBATCH --time=5:00:00
#SBATCH --partition=gpusmall

## Load in modules
module load cmake gcc cuda python-data bzip2 git

## By default this is 1, which would halt our program
unset OMP_NUM_THREADS

chmod +777 scripts/**/*
export DATETIME="$(date +"%Y-%m-%d_%H-%M-%S_%z")"
export OUTPUT_FOLDER="colors_d20_${DATETIME}"
## We want to perform our work in LOCAL_SCRATCH
export OLD_PWD="${PWD}"
mkdir -p "${LOCAL_SCRATCH}/SBWT-Search/"
cd "${LOCAL_SCRATCH}/SBWT-Search"
rm -rf build

# time the copy and build
t1=$(date +%s%3N)
cp -r "${OLD_PWD}/benchmark_objects/" "${LOCAL_SCRATCH}/SBWT-Search/benchmark_objects"
t2=$(date +%s%3N)

echo "Time taken to copy and build in LOCAL_SCRATCH: $((t2-t1)) ms"

# get themisto executable
cd benchmark_objects
export THEMISTO_VERSION="3.1.2"
export THEMISTO_FOLDER="themisto_linux-v${THEMISTO_VERSION}"
wget -nc https://github.com/algbio/themisto/releases/download/v${THEMISTO_VERSION}/${THEMISTO_FOLDER}.tar.gz
tar -xvzf ${THEMISTO_FOLDER}.tar.gz
rm ${THEMISTO_FOLDER}.tar.gz
mv ${THEMISTO_FOLDER} themisto
cd ..
mkdir -p themisto_temp

cp benchmark_objects/index/index_d20.tcolors benchmark_objects/index/index.tcolors

./benchmark_objects/themisto/themisto pseudoalign \
  --n-threads 32 \
  --sort-output \
  --temp-dir themisto_temp \
  --out-file-list benchmark_objects/list_files/output/color_search_results_running.list \
  --query-file-list benchmark_objects/list_files/input/unzipped_seqs.list \
  --index-prefix benchmark_objects/index/index \
  --verbose \
  --threshold 0.7

cd "${OLD_PWD}"
