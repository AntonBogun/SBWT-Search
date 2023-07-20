#!/bin/bash

# Download the data used for benchmarking from public databases. The dataset
# file list is contained `scripts/configure/benchmark_datra_list.txt`. This
# file contains all the files obtained from
# https://www.ebi.ac.uk/ena/browser/view/PRJEB32631, from this paper:
# doi:10.1038/s41467-022-35178-5. The original dataset is 2TB big, so I have
# chosen to only have the first 20 files. The index contains ecoli genomes,
# found at https://zenodo.org/record/6656897#.ZASdeR9BwQ8, created using
# themisto using the genomes compiled in these papers:
# https://doi.org/10.1099/mgen.0.000499, https://doi.org/10.1099/mgen.0.000499
# and https://doi.org/10.1038/s41586-019-1560-1

sbwt_executable=$1
themisto_executable=$2

if [ ! -f "${sbwt_executable}" ]; then
  echo "sbwt executable not found, required as the first 1st to this script" >&2
  exit 1
fi
if [ ! -f "${themisto_executable}" ]; then
  echo "themisto executable not found, required as the 2nd argument to this script" >&2
  exit 1
fi
if [ ! -d "benchmark_objects/index" ] || [ ! -f "benchmark_objects/index/index.tdbg" ] || [ ! -f "benchmark_objects/index/index_d1.tcolors" ]; then
  echo "This script expects index.tdbg and index_d1.tcolors in the benchmark_objects/index folder"
  exit 1
fi

download_files=(`head -22 scripts/configure/full_benchmark_data_list.txt | tail -20`)

mkdir -p "benchmark_objects/color_search_results_t0.7"
mkdir -p "benchmark_objects/index"
mkdir -p "benchmark_objects/index_search_results_d1_ascii"
mkdir -p "benchmark_objects/index_search_results_d1_binary"
mkdir -p "benchmark_objects/index_search_results_d1_packedint"
mkdir -p "benchmark_objects/index_search_results_d20_ascii"
mkdir -p "benchmark_objects/index_search_results_d20_binary"
mkdir -p "benchmark_objects/index_search_results_d20_packedint"
mkdir -p "benchmark_objects/list_files/input"
mkdir -p "benchmark_objects/list_files/output"
mkdir -p "benchmark_objects/running"
mkdir -p "benchmark_objects/unzipped_seqs"
mkdir -p "benchmark_objects/zipped_seqs"
cd benchmark_objects/zipped_seqs

# download the files
for file in "${download_files[@]}"; do
 wget -nc -q "${file}" -o /dev/null &
done
# kill $(jobs -p)  # IN CASE OF EMERGENCY, USE THIS
wait < <(jobs -p) # wait for jobs to finish

# unzip the downloaded files to proper folder
files=(`ls`)
cd ..
for file in "${files[@]}"; do
  if [ ! -f "unzipped_seqs/${file%.*}" ]; then
    gunzip -k -c "zipped_seqs/${file}" > "unzipped_seqs/${file%.*}" &
  fi
done
wait < <(jobs -p)

cd ..

# create sbwt_index from index
python3 scripts/modifiers/themisto_tdbg_to_sbwt.py -i benchmark_objects/index/index.tdbg -o benchmark_objects/running/index.sbwt

# run sbwt on the sequence files
files=(`cd benchmark_objects/unzipped_seqs && ls`)
for file in "${files[@]}"; do
  if [ ! -f "benchmark_objects/index_search_results_d1_ascii/${file%.*}.indexes.txt" ]; then
    ${sbwt_executable} search -i benchmark_objects/running/index.sbwt -q "benchmark_objects/unzipped_seqs/${file}" -o "benchmark_objects/index_search_results_d1_ascii/${file%.*}.indexes.sbwt_txt" > /dev/null 2>&1 &
  fi
done
wait < <(jobs -p)

# convert the sbwt output
files=(`cd benchmark_objects/index_search_results_d1_ascii && ls *.sbwt_txt`)
for file in "${files[@]}"; do
  if [ ! -f "benchmark_objects/index_search_results_d1_ascii/${file%.*}.txt" ]; then
    python3 scripts/modifiers/sbwt_index_results_to_ascii.py -i "benchmark_objects/index_search_results_d1_ascii/${file}" -o "benchmark_objects/index_search_results_d1_ascii/${file%.*}.txt" > /dev/null
  fi
done
wait < <(jobs -p)
rm -f benchmark_objects/index_search_results_d1_ascii/*.sbwt_txt

# run themisto on sequence files
files=(`cd benchmark_objects/unzipped_seqs && ls`)
cp benchmark_objects/index/index_d1.tcolors benchmark_objects/index/index.tcolors
for file in "${files[@]}"; do
  if [ ! -f "benchmark_objects/color_search_results_t0.7/${file%.*}.colors.txt" ]; then
    ${themisto_executable} pseudoalign -i benchmark_objects/index/index -q "benchmark_objects/unzipped_seqs/${file}" -o "benchmark_objects/color_search_results_t0.7/${file%.*}.colors.themisto_txt" --threshold 0.7 --temp-dir benchmark_objects/running -t 10 --sort-output
  fi
done
mv benchmark_objects/index/index.tcolors benchmark_objects/index/index_d1.tcolors

# convert the colors to this program's format
files=(`cd benchmark_objects/color_search_results_t0.7/ && ls *.themisto_txt`)
for file in "${files[@]}"; do
  if [ ! -f "benchmark_objects/color_search_results_t0.7/${file%.*}.txt" ]; then
    python3 scripts/modifiers/themisto_colors_to_ascii.py -i "benchmark_objects/color_search_results_t0.7/${file}" -o "benchmark_objects/color_search_results_t0.7/${file%.*}.txt" &
  fi
done
wait < <(jobs -p)

rm benchmark_objects/color_search_results_t0.7/*.themisto_txt

# populate input list files
find benchmark_objects/zipped_seqs/* > benchmark_objects/list_files/input/zipped_seqs.list
find benchmark_objects/unzipped_seqs/* > benchmark_objects/list_files/input/unzipped_seqs.list
find benchmark_objects/index_search_results_d1_ascii/* > benchmark_objects/list_files/input/index_search_results_d1_ascii.list
cp benchmark_objects/list_files/input/index_search_results_d1_ascii.list benchmark_objects/list_files/input/index_search_results_d1_binary.list
cp benchmark_objects/list_files/input/index_search_results_d1_ascii.list benchmark_objects/list_files/input/index_search_results_d1_packedint.list
sed -i "s/ascii/binary/g" benchmark_objects/list_files/input/index_search_results_d1_binary.list
sed -i "s/\.txt/\.bin/g" benchmark_objects/list_files/input/index_search_results_d1_binary.list
sed -i "s/ascii/packedint/g" benchmark_objects/list_files/input/index_search_results_d1_packedint.list
sed -i "s/\.txt/\.pint/g" benchmark_objects/list_files/input/index_search_results_d1_packedint.list
cp benchmark_objects/list_files/input/index_search_results_d1_ascii.list benchmark_objects/list_files/input/index_search_results_d20_ascii.list
sed -i "s/d1/d20/g" benchmark_objects/list_files/input/index_search_results_d20_ascii.list
cp benchmark_objects/list_files/input/index_search_results_d1_ascii.list benchmark_objects/list_files/input/index_search_results_d20_binary.list
sed -i "s/ascii/binary/g" benchmark_objects/list_files/input/index_search_results_d20_binary.list
sed -i "s/\.txt/\.bin/g" benchmark_objects/list_files/input/index_search_results_d20_binary.list
sed -i "s/d1/d20/g" benchmark_objects/list_files/input/index_search_results_d20_binary.list
cp benchmark_objects/list_files/input/index_search_results_d1_ascii.list benchmark_objects/list_files/input/index_search_results_d20_packedint.list
sed -i "s/ascii/packedint/g" benchmark_objects/list_files/input/index_search_results_d20_packedint.list
sed -i "s/\.txt/\.pint/g" benchmark_objects/list_files/input/index_search_results_d20_packedint.list
sed -i "s/d1/d20/g" benchmark_objects/list_files/input/index_search_results_d20_packedint.list

# populate output list files
files=(`cd benchmark_objects/unzipped_seqs && ls`)
printf "" > benchmark_objects/list_files/output/color_search_results_running.list
printf "" > benchmark_objects/list_files/output/index_search_results_running.list
printf "" > benchmark_objects/list_files/output/index_search_results_d1_ascii.list
printf "" > benchmark_objects/list_files/output/index_search_results_d20_ascii.list
printf "" > benchmark_objects/list_files/output/index_search_results_d1_binary.list
printf "" > benchmark_objects/list_files/output/index_search_results_d20_binary.list
printf "" > benchmark_objects/list_files/output/index_search_results_d1_packedint.list
printf "" > benchmark_objects/list_files/output/index_search_results_d20_packedint.list
for file in "${files[@]}"; do
  echo benchmark_objects/running/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_running.list
  echo benchmark_objects/running/${file%.*}.colors >> benchmark_objects/list_files/output/color_search_results_running.list
  echo benchmark_objects/index_search_results_d1_ascii/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d1_ascii.list
  echo benchmark_objects/index_search_results_d20_ascii/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d20_ascii.list
  echo benchmark_objects/index_search_results_d1_binary/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d1_binary.list
  echo benchmark_objects/index_search_results_d20_binary/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d20_binary.list
  echo benchmark_objects/index_search_results_d1_packedint/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d1_packedint.list
  echo benchmark_objects/index_search_results_d20_packedint/${file%.*}.indexes >> benchmark_objects/list_files/output/index_search_results_d20_packedint.list
done

rm benchmark_objects/running/*
