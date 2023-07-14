#!/bin/bash

# Meant to be run on Mahti supercomputer by CSC: https://research.csc.fi/-/mahti


#SBATCH --job-name=SBWT_indexes_d1
#SBATCH --account=dongelr1
#SBATCH --error indexes_d1_err.txt
#SBATCH --output indexes_d1_out.txt
#SBATCH --nodes 1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=32
#SBATCH --mem-per-cpu=14G
#SBATCH --gres=gpu:a100:1,nvme:350

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
export OUTPUT_FOLDER="index_d1_${DATETIME}"
## We want to perform our work in LOCAL_SCRATCH
export OLD_PWD="${PWD}"
mkdir -p "${LOCAL_SCRATCH}/SBWT-Search/"
cd "${LOCAL_SCRATCH}/SBWT-Search"
rm -rf build

# time the copy and build
t1=$(date +%s%3N)
cp -r "${OLD_PWD}/src/" "${LOCAL_SCRATCH}/SBWT-Search/src" &
cp -r "${OLD_PWD}/cmake/" "${LOCAL_SCRATCH}/SBWT-Search/cmake" &
cp -r "${OLD_PWD}/scripts/" "${LOCAL_SCRATCH}/SBWT-Search/scripts" &
cp "${OLD_PWD}/CMakeLists.txt" "${LOCAL_SCRATCH}/SBWT-Search/CMakeLists.txt" &
wait < <(jobs -p) # wait for jobs to finish
cp -r "${OLD_PWD}/benchmark_objects/" "${LOCAL_SCRATCH}/SBWT-Search/benchmark_objects" &
bash ./scripts/build/release.sh nvidia >&2
wait < <(jobs -p) # wait for jobs to finish
t2=$(date +%s%3N)

mkdir -p "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}"
echo "Time taken to copy and build in LOCAL_SCRATCH: $((t2-t1)) ms" >> "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}/benchmark_out.txt"

bash -x scripts/benchmark/index_search_d1.sh "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}/benchmark_out.txt" "nvidia"
cd "${OLD_PWD}"
