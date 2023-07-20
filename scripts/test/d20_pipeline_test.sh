#!/bin/bash

# Build the main executable, run the executable on
# test_objects/full_pipeline/color_search/ and verify that the outputs are
# correct (ie equal to the expected values, which can be found in the same
# folder). The temporary files are kept in tmp/d20_pipeline_test and are
# subsequently delted after the test is finished.

if [ $# -ne 1 ] || ( [ "${1,,}" != "nvidia" ] && [ "${1,,}" != "amd" ] && [ "${1,,}" != "cpu" ]); then
  echo "Usage: ./scripts/test/d20_pipeline_test.sh <NVIDIA|AMD|CPU>"
  exit 1
fi

# build
./scripts/build/release.sh $1

index_modes=(
  "ascii"
  "binary"
  "packedint"
)
index_extensions=(
  ".txt"
  ".bin"
  ".pint"
)
colors_modes=(
  "ascii"
  "binary"
  "csv"
  "packedint"
)
colors_extensions=(
  ".txt"
  ".bin"
  ".csv"
  ".pint"
)

bad_exits=0

function run_tests() {
  for file in ${fna_files}
  do
    no_extension="${file%.*}"
    for extension in ${colors_extensions[@]}
    do
      expected="test_objects/full_pipeline/color_search/expected/${file%.*}.colors.txt"
      actual="tmp/d20_pipeline_test/actual/${file%.*}.colors${extension}"
      python3 scripts/test/verify_color_results_equal.py \
        -x ${expected} \
        -y ${actual} \
        --quiet
      last_exit=$?
      if [ ${last_exit} -ne 0 ]; then
        echo ${expected} and ${actual} do not match
        echo ""
      fi
      ((bad_exits+=${last_exit}))
    done
  done

  rm tmp/d20_pipeline_test/actual/*
}

mkdir -p tmp/d20_pipeline_test/actual

index_input_file=tmp/d20_pipeline_test/index_inputs.list
index_output_file=tmp/d20_pipeline_test/index_outputs.list
colors_input_file=tmp/d20_pipeline_test/colors_inputs.list
colors_output_file=tmp/d20_pipeline_test/colors_outputs.list
printf "" > ${index_input_file}
printf "" > ${index_output_file}
printf "" > ${colors_input_file}
printf "" > ${colors_output_file}

fna_files=`cd test_objects/full_pipeline/color_search && ls *.fna`

for file in ${fna_files[@]}; do
  echo test_objects/full_pipeline/color_search/${file} >> ${index_input_file}
  echo tmp/d20_pipeline_test/${file%.*}.indexes >> ${index_output_file}
  echo tmp/d20_pipeline_test/actual/${file%.*}.colors >> ${colors_output_file}
done

for streams in {1..5}; do
  echo "Running combined with streams = ${streams}"
  for index_mode_idx in ${!index_modes[@]}; do
    ./build/bin/sbwt_search index \
      -o ${index_output_file} \
      -i test_objects/themisto_example/GCA_combined.tdbg \
      -k test_objects/themisto_example/GCA_combined_d20.tcolors \
      -q ${index_input_file} \
      -p ${index_modes[index_mode_idx]} \
      -s ${streams} \
      -c 0.1
    printf "" > ${colors_input_file}
    for file in ${fna_files[@]}; do
      echo tmp/d20_pipeline_test/${file%.*}.indexes${index_extensions[index_mode_idx]} >> ${colors_input_file}
    done
    for colors_mode in ${colors_modes[@]}; do
      ./build/bin/sbwt_search colors \
        -o ${colors_output_file} \
        -k test_objects/themisto_example/GCA_combined_d20.tcolors \
        -q ${colors_input_file} \
        -p ${colors_mode} \
        -t 0.7 \
        -s ${streams} \
        -c 0.1
    done
    run_tests
  done
done

rm -r tmp/d20_pipeline_test

if [[ ${bad_exits} -gt 0 ]]; then
  exit 1
else
  echo "d20 pipeline test passed!"
fi
