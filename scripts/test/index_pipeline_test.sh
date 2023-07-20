#!/bin/bash

# Build the main executable, run the executable on
# test_objects/full_pipeline/index_pipeline_search/ and verify that the outputs are
# correct (ie equal to the expected values, which can be found in the same folder)

if [ $# -ne 1 ] || ( [ "${1,,}" != "nvidia" ] && [ "${1,,}" != "amd" ] && [ "${1,,}" != "cpu" ]); then
  echo "Usage: ./scripts/test/index_pipeline_test.sh <NVIDIA|AMD|CPU>"
  exit 1
fi

# build
./scripts/build/release.sh "$1"

cd test_objects/full_pipeline/index_search/expected
files=`ls *.txt`
cd -

modes=(
  "ascii"
  "binary"
  "bool"
  "packedint"
)
extensions=(
  ".txt"
  ".bin"
  ".bool"
  ".pint"
)

bad_exits=0

function run_tests() {
  for file in ${files}
  do
    no_extension="${file%%.*}"
    for extension in ${extensions[@]}
    do
      expected="test_objects/full_pipeline/index_search/expected/${file}"
      actual="tmp/index_pipeline_test/actual/${no_extension}${extension}"
      python3 scripts/test/verify_index_results_equal.py \
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

  rm tmp/index_pipeline_test/actual/*
}

mkdir -p tmp/index_pipeline_test/actual

# populate combined list
input_file=tmp/index_pipeline_test/combined_input.list
output_file=tmp/index_pipeline_test/combined_output.list
printf "" > ${input_file}
printf "" > ${output_file}
input_files=(`cd test_objects/full_pipeline/index_search/ && find *.fna *.fnq`)
for file in ${input_files[@]}; do
  echo test_objects/full_pipeline/index_search/${file} >> ${input_file}
  echo tmp/index_pipeline_test/actual/${file%.*} >> ${output_file}
done

for streams in {1..5}; do
  echo "Running combined with streams = ${streams}"
  for mode in ${modes[@]}; do
    ./build/bin/sbwt_search index \
      -o ${output_file} \
      -i test_objects/search_test_index.sbwt \
      -q ${input_file} \
      -p ${mode} \
      -s ${streams} \
      -c 0.1
  done
  run_tests
done

echo "Running individually"
for mode in ${modes[@]}; do
  for file in ${input_files[@]}; do
    ./build/bin/sbwt_search index \
      -q "test_objects/full_pipeline/index_search/${file}" \
      -i test_objects/search_test_index.sbwt \
      -o "tmp/index_pipeline_test/actual/${file%.*}"  \
      -p ${mode} \
      -s 1 \
      -c 0.1
  done
done
run_tests

rm -r tmp/index_pipeline_test

if [[ ${bad_exits} -gt 0 ]]; then
  exit 1
else
  echo "Index pipeline test passed!"
fi
