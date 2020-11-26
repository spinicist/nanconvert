#!/bin/bash -eux
vcpkg/bootstrap-vcpkg.sh -allowAppleClang -useSystemBinaries
cmake -B build -S . \
    -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_TARGET_TRIPLET=x64-osx-asan -DCMAKE_BUILD_TYPE=Release \
    -GNinja
cmake --build build