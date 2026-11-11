#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${PS_SYSROOT:?Set PS_SYSROOT to a directory with include/SDL2 and lib/libSDL2.so}"
: "${PS_ZIG:?Set PS_ZIG to Zig 0.13.0}"
build=${1:-"$project/build-aarch64"}
cmake -S "$project" -B "$build" -DCMAKE_TOOLCHAIN_FILE="$project/cmake/aarch64-zig.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build" --parallel 4
