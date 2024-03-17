#!/usr/bin/env bash
# Note: this has to be called from a Dockerfile!

set -eux

whoami
apt-get update
apt-get install -y --no-install-recommends \
  git \
  python3-pip \
  python3-dev \
  pkg-config \
  gawk \
  ninja-build \
  gdb-multiarch \
  cmake \
  gcc-arm-none-eabi \
  libnewlib-arm-none-eabi \
  libstdc++-arm-none-eabi-newlib 
  
apt-get clean
rm -rf /var/lib/apt/lists/*

# Install Pico-sdk needed 
pwd


if [[ -d ${PICO_SDK_PATH} ]]; then
    cd ${PICO_SDK_PATH}
    git pull 
    git submodule update
else    
    mkdir -p ${PICO_SDK_PATH}
    git clone --depth 1 --recurse-submodules --shallow-submodules ${PICO_SDK_URL} ${PICO_SDK_PATH}
fi

 

