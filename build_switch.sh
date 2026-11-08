#!/usr/bin/env bash
set -e

export DEVKITPRO=/opt/devkitpro
export DEVKITARM=/opt/devkitpro/devkitARM
export DEVKITA64=/opt/devkitpro/devkitA64

mkdir -p build
cd build

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -mcpu=cortex-a57+crypto -mtune=cortex-a57" \
  -DGBARECOMP_SCANLINE_IMPLEMENTATION=HLE

make -j$(nproc)
