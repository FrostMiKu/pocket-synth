#!/bin/sh
# Verify current ARM64 binaries in an isolated directory.
set -eu
[ "$#" -ge 2 ] || { echo 'Usage: check-device.sh build-directory user@host'; exit 2; }
build=$(cd "$1" && pwd)
host=$2
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
eval "$(python3 "$project/scripts/product.py" --shell)"
manifest="$project/tests/device-tests.txt"
stage=$(ssh -o BatchMode=yes -o ConnectTimeout=8 "$host" "mktemp -d /roms/ports/.$PS_SLUG-check.XXXXXX")
case "$stage" in /roms/ports/."$PS_SLUG"-check.*) ;; *) exit 1;; esac
scp -o BatchMode=yes -o ConnectTimeout=8 "$manifest" "$host:$stage/device-tests.txt"
scp -o BatchMode=yes -o ConnectTimeout=8 -C "$build/$PS_BINARY" "$host:$stage/$PS_BINARY"
while IFS= read -r name; do scp -o BatchMode=yes -o ConnectTimeout=8 -C "$build/$name" "$host:$stage/$name"; done <"$manifest"
ssh -o BatchMode=yes -o ConnectTimeout=8 "$host" sh -s -- "$stage" "$PS_SLUG" "$PS_BINARY" <<'REMOTE'
set -eu
stage=$1 slug=$2 binary=$3
case "$stage" in /roms/ports/."$slug"-check.*) ;; *) exit 1;; esac
cd "$stage"
chmod +x "$binary"
while IFS= read -r name; do chmod +x "$name"; "./$name"; done <device-tests.txt
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "./$binary" --smoke
for run in 1 2; do
    XDG_RUNTIME_DIR=/var/run/0-runtime-dir SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=pulseaudio \
      "./$binary" --loop-demo --seconds 18 >"performance-$run.log" 2>&1
    cat "performance-$run.log"
    grep -q 'queue_overruns=0 .*late_callbacks=0' "performance-$run.log"
done
cd /roms/ports
rm -rf "$stage"
REMOTE
