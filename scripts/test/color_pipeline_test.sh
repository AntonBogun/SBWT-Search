#!/bin/bash

# Build the main executable, run the executable on
# test_objects/full_pipeline/color_search/ and verify that the outputs are
# correct (ie equal to the expected values, which can be found in the same
# folder)

if [ $# -ne 1 ] || ( [ "${1,,}" != "nvidia" ] && [ "${1,,}" != "amd" ] && [ "${1,,}" != "cpu" ]); then
  echo "Usage: ./scripts/test/color_pipeline_test.sh <NVIDIA|AMD|CPU>"
  exit 1
fi

# build
./scripts/build/release.sh "$1"

files=`cd test_objects/full_pipeline/color_search && ls *.fna`

modes=(
  "ascii"
  "binary"
  "csv"
  "packedint"
)
extensions=(
  ".txt"
  ".bin"
  ".csv"
  ".pint"
)

bad_exits=0

function run_tests() {
  for file in ${files}
  do
    for extension in ${extensions[@]}
    do
      expected="test_objects/full_pipeline/color_search/expected/${file%.*}.colors.txt"
      actual="tmp/color_pipeline_test/actual/${file%.*}.colors${extension}"
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

  rm tmp/color_pipeline_test/actual/*
}

mkdir -p tmp/color_pipeline_test/actual/

# populate combined list
input_file=tmp/color_pipeline_test/combined_input.list
output_file=tmp/color_pipeline_test/combined_output.list
printf "" > ${input_file}
printf "" > ${output_file}
files=(`cd test_objects/full_pipeline/color_search/ && ls *.fna`)
for file in ${files[@]}; do
  echo test_objects/full_pipeline/color_search/${file%.*}.indexes.txt >> ${input_file}
  echo tmp/color_pipeline_test/actual/${file%.*}.colors >> ${output_file}
done

# build
for streams in {1..5}; do
  echo "Running combined with streams = ${streams}"
  for mode in ${modes[@]}; do
    ./build/bin/sbwt_search colors \
      -o ${output_file} \
      -k test_objects/themisto_example/GCA_combined_d1.tcolors \
      -q ${input_file} \
      -p ${mode} \
      -t 0.7 \
      -s ${streams} \
      -c 0.1
  done
  run_tests
done

echo "Running individually"
for mode in ${modes[@]}; do
  for file in ${files[@]}; do
    ./build/bin/sbwt_search colors \
      -q "test_objects/full_pipeline/color_search/${file%.*}.indexes.txt" \
      -k test_objects/themisto_example/GCA_combined_d1.tcolors \
      -o "tmp/color_pipeline_test/actual/${file%.*}.colors"  \
      -p ${mode} \
      -t 0.7 \
      -s 1 \
      -c 0.1
  done
done
run_tests

rm -r tmp/color_pipeline_test

if [[ ${bad_exits} -gt 0 ]]; then
  exit 1
else
  echo "Color pipeline test passed!"
fi
