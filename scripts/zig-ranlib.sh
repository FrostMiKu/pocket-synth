#!/bin/sh
set -eu
exec "${PS_ZIG:?Set PS_ZIG}" ranlib "$@"
