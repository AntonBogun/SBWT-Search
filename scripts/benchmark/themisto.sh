#!/bin/bash



if [ ! -d "benchmark_objects/index" ] || [ ! -f "benchmark_objects/index/index_d20.tdbg" ] || [ ! -f "benchmark_objects/index/index_d20.tcolors" ]; then
  echo "This script expects a 'benchmark_objects/index' folder with 'index.tdbg' to be present within that folder"
  exit 1
fi

if [ ! -d "themisto" ] || [ ! -f "themisto/themisto" ]; then
  echo "This script expects a 'themisto' folder with 'themisto' executable to be present within that folder"
  exit 1
fi

# mv benchmark_objects/index/index.tdbg benchmark_objects/index/index_d20.tdbg
mkdir -p benchmark_objects/themisto_results
rm -f benchmark_objects/themisto_results/*

index="benchmark_objects/index/index_d20"
input_file="benchmark_objects/list_files/input/unzipped_seqs.list"
output_file="benchmark_objects/list_files/output/themisto_output.list"
convert_output="benchmark_objects/list_files/output/themisto_output_converted.list"
convert_script="scripts/modifiers/convert.py"

input_lines=$(wc -l < "${input_file}")
output_lines=$(wc -l < "${output_file}")
output_lines_converted=$(wc -l < "${convert_output}")

if [ "${input_lines}" -ne "${output_lines}" ] || [ "${input_lines}" -ne "${output_lines_converted}" ]; then
    echo "Error: The number of lines in the input and output and output_converted files is not the same."
    exit 1
fi


themisto/themisto pseudoalign -i "${index}" --query-file-list "${input_file}" --out-file-list "${output_file}" --threshold 0.7 --temp-dir benchmark_objects/running2 -t 20 --sort-output

# apply convert.py to each file in output_file

# python "${convert_script}" "${file}" "${convert_output}" --list
# Read the input and output files line by line
while read -r input_line <&3 && read -r output_line <&4; do
    python "${convert_script}" "${input_line}" "${output_line}" &
done 3< "${output_file}" 4< "${convert_output}"

# Wait for all processes to finish
wait

rm -f benchmark_objects/running/*
# mv benchmark_objects/index/index_d20.tdbg benchmark_objects/index/index.tdbg

