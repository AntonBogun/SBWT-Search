#!/bin/bash
# Build the main executable, tests as well as documentation in debug mode

mkdir -p build
cd build
cmake \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_CLANG_TIDY=OFF \
  -DENABLE_HEADER_GUARDS_CHECK=OFF \
  -DENABLE_CLANG_FORMAT_CHECK=OFF \
  -DBUILD_VERIFY=ON \
  -DBUILD_MAIN=ON \
  -DBUILD_TESTS=ON \
  -DBUILD_DOCS=OFF \
  -DENABLE_PROFILING=ON \
  ..
if [[ $# > 0 ]];
then
  cmake \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_CLANG_TIDY=ON \
    -DENABLE_HEADER_GUARDS_CHECK=ON \
    -DENABLE_CLANG_FORMAT_CHECK=ON \
    -DBUILD_VERIFY=OFF \
    -DBUILD_MAIN=OFF \
    -DBUILD_TESTS=OFF \
    -DBUILD_DOCS=OFF \
    -DENABLE_PROFILING=OFF \
    ..
fi
cd ..


./scripts/build/release.sh a
# ./scripts/build/debug.sh
./scripts/build/docs.sh a
./scripts/build/verify.sh a
./scripts/build/tests.sh a
