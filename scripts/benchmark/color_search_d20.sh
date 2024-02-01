#!/bin/bash

# Run the benchmark_main several times and save the results to a file. Called by
# scipts/sbatch/benchmark.sbatch It is expected that the \*.tdbg file is within
# benchmark_objects/colors folder

#! do not run this script on its own, it will delete everything in themisto_results

if [ $# -ne 2 ]; then
  echo "Usage: ./scripts/benchmark/index_search_d20.sh <output_file> <nvidia|amd>"
  exit 1
fi

## Set loglevel to trace because we use this to get timing statistics
export SPDLOG_LEVEL=TRACE

benchmark_out="$1"

if [ ! -d "benchmark_objects/index" ] || [ ! -f "benchmark_objects/index/index_d20.tcolors" ]; then
  echo "This script expects a 'benchmark_objects/index' folder with 'index_d20.tcolors' to be present within that folder"
  exit 1
fi

colors_file="benchmark_objects/index/index_d20.tcolors"
input_files=(
  # "benchmark_objects/list_files/input/index_search_results_d20_ascii.list"
  # "benchmark_objects/list_files/input/index_search_results_d20_binary.list"
  # "benchmark_objects/list_files/input/index_search_results_d20_packedint.list"
  "benchmark_objects/list_files/input/index_search_results_d20_packedint_DEBUG.list"
)
input_files_aliases=(
  # "AsciiIndexes"
  # "BinaryIndexes"
  "PackedIntIndexes"
)

# output_file="benchmark_objects/list_files/output/color_search_results_running.list"
output_file="benchmark_objects/list_files/output/color_search_results_running_DEBUG.list"
printing_modes=(
  "ascii"
  "binary"
  "packedint"
  # "csv"
)
if [ $2 = "nvidia" ]; then
  devices=("nvidia")
elif [ $2 = "amd" ]; then
  devices=("amd")
else
  echo "2nd argument is incorrect"
fi

streams_options=(1 2 3 4 5 6 7 8)

# . scripts/build/release.sh ${devices[0]} >&2

# ./build/bin/sbwt_search index \
#   -i "benchmark_objects/index/index.tdbg" \
#   -q "benchmark_objects/list_files/input/unzipped_seqs.list" \
#   -o "benchmark_objects/list_files/output/index_search_results_d20_ascii.list" \
#   -s "4" \
#   -p "ascii" \
#   -u 10GB \
#   -k "benchmark_objects/index/index_d20.tcolors" \
#   >> /dev/null

# ./build/bin/sbwt_search index \
#   -i "benchmark_objects/index/index.tdbg" \
#   -q "benchmark_objects/list_files/input/unzipped_seqs.list" \
#   -o "benchmark_objects/list_files/output/index_search_results_d20_binary.list" \
#   -s "4" \
#   -p "binary" \
#   -u 10GB \
#   -k "benchmark_objects/index/index_d20.tcolors" \
#   >> /dev/null


#!remove in case the index is trusted
rm -f benchmark_objects/index_search_results_d20_packedint/*

  # -q "benchmark_objects/list_files/input/unzipped_seqs.list" \
  # -o "benchmark_objects/list_files/output/index_search_results_d20_packedint.list" \

./build/bin/sbwt_search index \
  -i "benchmark_objects/index/index.tdbg" \
  -q "benchmark_objects/list_files/input/unzipped_seqs_DEBUG.list" \
  -o "benchmark_objects/list_files/output/index_search_results_d20_packedint_DEBUG.list" \
  -s "4" \
  -p "packedint" \
  -u 10GB \
  -k "benchmark_objects/index/index_d20.tcolors" \
  >> /dev/null


running_dir="benchmark_objects/running"
color_search_results_dir="benchmark_objects/color_search_results"
diff_dir="benchmark_objects/color_search_DIFF"
themisto_dir="benchmark_objects/themisto_results"

rm -f "${running_dir}"/*
rm -f "${color_search_results_dir}"/*
rm -f "${diff_dir}"/*

#!will delete everything in themisto_results
mv "${themisto_dir}"/* "${color_search_results_dir}"
first_run=true
#400GB
max_size=400000000000

for device in "${devices[@]}"; do
  # . scripts/build/release.sh ${device} >&2
  for input_file_idx in "${!input_files[@]}"; do
    for printing_mode in "${printing_modes[@]}"; do
      for streams in "${streams_options[@]}"; do
        echo "Now running: File ${input_files_aliases[input_file_idx]} with ${streams} streams in ${printing_mode} format on ${device} device"
        echo "Now running: File ${input_files_aliases[input_file_idx]} with ${streams} streams in ${printing_mode} format on ${device} device" >> "${benchmark_out}"
        ./build/bin/sbwt_search colors \
          -k "${colors_file}" \
          -q "${input_files[input_file_idx]}" \
          -o "${output_file}" \
          -s "${streams}" \
          -p "${printing_mode}" \
          -u 10GB \
          -t 0.7 \
          >> "${benchmark_out}"
        printf "Size of outputs: "
        # ls -lh "benchmark_objects/running" | head -1
        du -bs "benchmark_objects/running" | awk '{print $1}'

        if [ "$first_run" = true ]; then
          first_run=false
          last_printing_mode="themisto"
          last_streams="0"
        fi

        declare -A dict
        l1=($(ls $color_search_results_dir))
        l2=($(ls $running_dir))

        for x in "${l1[@]}"; do
            y=${x%%.*}
            dict[$y]="$color_search_results_dir/$x"
        done
        
        for x in "${l2[@]}"; do
          y=${x%%.*}
          if [ -z "${dict[$y]}" ]; then
              echo "error: unmatched file $x"
          fi

          error_output=$(./formatdiff/diff_tool "${last_printing_mode}" "${printing_mode}" "${dict[$y]}" "$running_dir/$x" 2>&1)
          if [ $? -ne 0 ]; then
            
            echo "Error comparing ${dict[$y]} and $running_dir/$x"
            echo "$error_output"
            
            diff_folder_size=$(du -bs "$diff_dir" | awk '{print $1}')
            file1_size=$(du -b "${dict[$y]}" | awk '{print $1}')
            file2_size=$(du -b "$running_dir/$x" | awk '{print $1}')
            total_size=$((diff_folder_size + file1_size + file2_size))

            if [ $total_size -lt $max_size ]; then
              cp "${dict[$y]}" "$diff_dir/$(basename ${dict[$y]})_${last_streams}"
              cp "$running_dir/$x" "$diff_dir/$(basename $x)_${streams}"
            else
              echo "Folder size is $total_size KB, which is larger than $max_size KB. Not copying the diff"
            fi
            
            rm "$color_search_results_dir"/*
            mv "$running_dir"/* "$color_search_results_dir"
            last_streams=$streams
            last_printing_mode=$printing_mode
            break
          
          fi
        
        done
        rm -f "$running_dir"/*
      done
    done
  done
done

# rm benchmark_objects/index_search_results_d20_binary/*
rm benchmark_objects/color_search_results/*
