#!/bin/bash

# Meant to be run on LUMI supercomputer by CSC: https://www.csc.fi/lumi

#SBATCH --job-name=SBWT_colors_d1
#SBATCH --account=project_462000116
#SBATCH --error colors_d1_err.txt
#SBATCH --output colors_d1_out.txt
#SBATCH --nodes 1
#SBATCH --gres=gpu:1

## Maximum time
#SBATCH --time=10:00:00
#SBATCH --partition=standard-g

## Load in modules
## Note: on lumi, you will need to download and install your own cmake
module swap craype-x86-rome craype-x86-trento
module load gcc rocm craype-accel-amd-gfx90a cray-python

## By default this is 1, which would halt our program
unset OMP_NUM_THREADS

chmod +777 scripts/**/*
export DATETIME="$(date +"%Y-%m-%d_%H-%M-%S_%z")"
export OUTPUT_FOLDER="colors_d1_${DATETIME}"
## We want to perform our work in LOCAL_SCRATCH
### We want to perform our work in LOCAL_SCRATCH
mkdir -p "/flash/project_462000116/cauchida/colors_search_d1"
export LOCAL_SCRATCH="/flash/project_462000116/cauchida/colors_search_d1"
export OLD_PWD="${PWD}"
mkdir -p "${LOCAL_SCRATCH}/SBWT-Search/"
cd "${LOCAL_SCRATCH}/SBWT-Search"
rm -rf build

# Install new cmake on lumi
curl -O -L https://github.com/Kitware/CMake/releases/download/v3.26.3/cmake-3.26.3-linux-x86_64.tar.gz
mv cmake-* cmake.tar.gz
tar -xvf cmake.tar.gz
rm cmake.tar.gz
mv cmake-* cmake_dir
export PATH="${PWD}/cmake_dir/bin:${PATH}"

# time the copy and build
t1=$(date +%s%3N)
cp -r "${OLD_PWD}/src/" "${LOCAL_SCRATCH}/SBWT-Search/src" &
cp -r "${OLD_PWD}/cmake/" "${LOCAL_SCRATCH}/SBWT-Search/cmake" &
cp -r "${OLD_PWD}/scripts/" "${LOCAL_SCRATCH}/SBWT-Search/scripts" &
cp "${OLD_PWD}/CMakeLists.txt" "${LOCAL_SCRATCH}/SBWT-Search/CMakeLists.txt" &
wait < <(jobs -p) # wait for jobs to finish
cp -r "${OLD_PWD}/benchmark_objects/" "${LOCAL_SCRATCH}/SBWT-Search/benchmark_objects" &
bash ./scripts/build/release.sh amd >&2
wait < <(jobs -p) # wait for jobs to finish
t2=$(date +%s%3N)

mkdir -p "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}"
echo "Time taken to copy and build in LOCAL_SCRATCH: $((t2-t1)) ms" >> "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}/benchmark_out.txt"

bash -x scripts/benchmark/color_search_d1.sh "${OLD_PWD}/benchmark_results/${OUTPUT_FOLDER}/benchmark_out.txt" "amd"
cd "${OLD_PWD}"

rm -rf ${LOCAL_SCRATCH}
