#!/bin/bash

# Helper script called in cmake/SetHipTargetDevice.cmake to automatically
# download the necessary hip extras alongside the already installed target
# device compilers. Example: the user is expected to already have nvcc
# installed on an NVIDIA machine, so the script then simply downloads and
# installs any extras related to HIP. Similarly on AMD. To install the
# necessary items on each platform, the user is encouraged to look at the
# :ref:`Tools` section of the documentation

#nvidia
#rocm-5.4.x

TARGET_DEVICE=$1
ROCM_BRANCH=$2

mkdir -p hip
cd hip

if [ "${TARGET_DEVICE,,}" = "nvidia" ]; then
  mkdir -p hip_nvidia
  cd hip_nvidia
  if [ ! -d "hip" ]; then
      git clone -b "${ROCM_BRANCH}" https://github.com/ROCm-Developer-Tools/hip.git
  fi
  if [ ! -d "hipamd" ]; then
      git clone -b "${ROCM_BRANCH}" https://github.com/ROCm-Developer-Tools/hipamd.git
  fi
  HIP_DIR="$(readlink -f hip)"
  HIPAMD_DIR="$(readlink -f hipamd)"
  if [ ! -d ${HIPAMD_DIR}/build ]; then  # otherwise it means it was already built
    cd "${HIPAMD_DIR}"
    mkdir -p build; cd build
    cmake -DHIP_COMMON_DIR=$HIP_DIR -DHIP_PLATFORM=nvidia -DCMAKE_INSTALL_PREFIX=$PWD/install ..
    make -j$(nproc)
  fi
fi
