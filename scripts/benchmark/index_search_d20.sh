#!/bin/bash

# Run the benchmark_main several times and save the results to a file. Called by
# scipts/sbatch/benchmark.sbatch It is expected that the \*.tdbg file is within
# benchmark_objects/index folder

if [ $# -ne 2 ]; then
  echo "Usage: ./scripts/benchmark/index_search_d20.sh <output_file> <nvidia|amd>"
  exit 1
fi

## Set loglevel to trace because we use this to get timing statistics
export SPDLOG_LEVEL=TRACE

benchmark_out="$1"

if [ ! -d "benchmark_objects/index" ] || [ ! -f "benchmark_objects/index/index.tdbg" ]; then
  echo "This script expects a 'benchmark_objects/index' folder with 'index.tdbg' to be present within that folder"
  exit 1
fi

dbg_file="benchmark_objects/index/index.tdbg"
input_files=(
  # "benchmark_objects/list_files/input/unzipped_seqs_DEBUG.list"
  "benchmark_objects/list_files/input/unzipped_seqs.list"
  # "benchmark_objects/list_files/input/zipped_seqs.list"
)
# output_file="benchmark_objects/list_files/output/index_search_results_running_DEBUG.list"
output_file="benchmark_objects/list_files/output/index_search_results_running.list"
printing_modes=(
  # "ascii"
  # "binary"
  # "bool"
  "packedint"
)

if [ $2 = "nvidia" ]; then
  devices=("nvidia")
elif [ $2 = "amd" ]; then
  devices=("amd")
else
  echo "2nd argument is incorrect"
fi

# streams_options=(1 2 3 4 5 6 7 8)
streams_options=(2)

running_dir="benchmark_objects/running"
index_search_results_dir="benchmark_objects/index_search_results"
diff_dir="benchmark_objects/index_search_DIFF"

rm -f benchmark_objects/running/*
rm -f benchmark_objects/index_search_DIFF/*
rm -f benchmark_objects/index_search_results/*
first_run=true
#400GB
max_size=400000000000
for device in "${devices[@]}"; do
  # . scripts/build/release.sh ${device} >&2
  for input_file in "${input_files[@]}"; do
    for printing_mode in "${printing_modes[@]}"; do
      for streams in "${streams_options[@]}"; do
        echo "Now running: File ${input_file} with ${streams} streams in ${printing_mode} format on ${device} device"
        echo "Now running: File ${input_file} with ${streams} streams in ${printing_mode} format on ${device} device" >> "${benchmark_out}"
        ./build/bin/sbwt_search index \
          -i "${dbg_file}" \
          -q "${input_files}" \
          -o "${output_file}" \
          -p "${printing_mode}" \
          -s "${streams}" \
          -u 10GB \
          -k "benchmark_objects/index/index_d20.tcolors" \
          >> "${benchmark_out}"
        printf "Size of outputs: "
        # ls -lh "benchmark_objects/running" | head -1
        du -bs "benchmark_objects/running" | awk '{print $1}'

        if [ "$first_run" = true ]; then
          first_run=false
          last_printing_mode=$printing_mode
          last_streams=$streams
          #!REMOVE LATER
          cp "$running_dir"/* "$diff_dir"
          mv "$running_dir"/* "$index_search_results_dir"
        else

          declare -A dict
          l1=($(ls $index_search_results_dir))
          l2=($(ls $running_dir))

          rm -f lockfile
          rm -f pidfile
          rm -rf lockfolder
          touch pidfile
          touch lockfile

          for x in "${l1[@]}"; do
              y=${x%%.*}
              dict[$y]="$index_search_results_dir/$x"
          done
          
          for x in "${l2[@]}"; do
            y=${x%%.*}
            if [ -z "${dict[$y]}" ]; then
                echo "error: unmatched file $x"
            fi

            {
              error_output=$(./formatdiff/diff_tool "${last_printing_mode}" "${printing_mode}" "${dict[$y]}" "$running_dir/$x" 2>&1)

              if [ $? -ne 0 ]; then
                if mkdir lockfolder; then
                  if ! [ -s lockfile ]; then
                    echo "Error comparing ${dict[$y]} and $running_dir/$x"
                    while read -r pid; do
                      if [ "$pid" != "$BASHPID" ]; then
                        kill -TERM $pid
                      fi
                    done < pidfile
                    echo "$error_output"
                    echo $x > lockfile
                    echo $y >> lockfile
                  else
                    echo "WARNING: mkdir is not atomic"
                  fi
                fi
              fi
            } &
            echo $! >> pidfile
          done
          
          wait
          rm -rf lockfolder
          rm -f pidfile


          if [ -s lockfile ]; then
            {
                read -r x
                read -r y
            } < lockfile
            rm -f lockfile
            
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
              echo "Folder size is $total_size B, which is larger than $max_size B. Not copying the diff"
            fi
            
            rm "$index_search_results_dir"/*
            mv "$running_dir"/* "$index_search_results_dir"
            last_streams=$streams
            last_printing_mode=$printing_mode
          
          fi
                
        fi
        rm -f "$running_dir"/*
      done
    done
  done
done

rm benchmark_objects/index_search_results/*


