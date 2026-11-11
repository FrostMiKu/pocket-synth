#!/bin/sh
set -eu
: "${PS_ZIG:?Set PS_ZIG to the Zig 0.13.0 executable}"
exec "$PS_ZIG" c++ -target aarch64-linux-gnu.2.28 "$@"
