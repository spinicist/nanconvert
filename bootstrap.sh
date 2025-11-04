#!/bin/bash -e

USAGE="Usage: $0 [options]

Bootstraps the build

Options:
  -i DIR Install riesling to this directory, e.g. $HOME/.local
  -j N   Restrict parallel build to this many threads
  -h     Print this message
  -w     Add warnings
"
PAR=""
PREFIX=""
ALL=""
DEBUG=""
RELDBG="-O2 -g -DNDEBUG"
while getopts "hi:j:w" opt; do
    case $opt in
        i) PREFIX="-DCMAKE_INSTALL_PREFIX=$OPTARG";;
        j) export VCPKG_MAX_CONCURRENCY=$OPTARG
           PAR="-j $OPTARG";;
        h) echo "$USAGE"
           return;;
        w) ALL="-Wall -Wshadow"
           DEBUG="-g -fsanitize=address,undefined"
           RELDBG="-O2 -g -DNDEBUG -fsanitize=address,undefined";;
    esac
done
shift $((OPTIND - 1))

# Use Ninja if available, otherwise CMake default
if [ -x "$( command -v ninja )" ]; then
  GEN="-GNinja"
else
  GEN=""
fi

# If vcpkg is not installed, install it
if [[ (-x "$( command -v vcpkg )") && (-n $VCPKG_ROOT) ]]; then
  echo "vcpkg installed"
else
  git clone https://github.com/microsoft/vcpkg.git .vcpkg
  cd .vcpkg && ./bootstrap-vcpkg.sh && cd ..
  export VCPKG_ROOT="$PWD/.vcpkg"
  export PATH="$VCPKG_ROOT:$PATH"
fi

mkdir -p build
cmake -S . -B build $GEN \
  -DOX_INSTALL_DIRECTORY="${1}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O2 -g -fsanitize=address,undefined" \
  -DVCPKG_INSTALL_OPTIONS="--no-print-usage" \
  -DCMAKE_CXX_FLAGS="$ALL" -DCMAKE_CXX_FLAGS_DEBUG="$DEBUG" -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="$RELDBG"\
  "$PREFIX"

cmake --build build $PAR

if [ -n "$PREFIX" ]; then
  cmake --install build
fi
