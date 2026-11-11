#!/bin/sh
# Verify and install this application's current package, preserving its data.
set -eu
[ "$#" -ge 3 ] || { echo 'Usage: install.sh package.zip build-directory user@host'; exit 2; }
archive=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
build=$(cd "$2" && pwd)
host=$3
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
eval "$(python3 "$project/scripts/product.py" --shell)"
manifest="$project/tests/device-tests.txt"
while IFS= read -r name; do [ -f "$build/$name" ]; done <"$manifest"
stage=$(ssh -o BatchMode=yes -o ConnectTimeout=8 "$host" "mktemp -d /roms/ports/.$PS_SLUG-install.XXXXXX")
case "$stage" in /roms/ports/."$PS_SLUG"-install.*) ;; *) exit 1;; esac
expected=$(python3 - "$archive" "$PS_SLUG/$PS_BINARY.aarch64" <<'PY'
import hashlib, sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    print(hashlib.sha256(z.read(sys.argv[2])).hexdigest())
PY
)
scp -o BatchMode=yes -o ConnectTimeout=8 -C "$archive" "$host:$stage/package.zip"
scp -o BatchMode=yes -o ConnectTimeout=8 "$manifest" "$host:$stage/device-tests.txt"
while IFS= read -r name; do
    scp -o BatchMode=yes -o ConnectTimeout=8 -C "$build/$name" "$host:$stage/$name"
done <"$manifest"
scp -o BatchMode=yes -o ConnectTimeout=8 "$project/tests/app_cli.py" "$host:$stage/app_cli.py"
ssh -o BatchMode=yes -o ConnectTimeout=8 "$host" sh -s -- "$stage" "$expected" "$PS_SLUG" "$PS_BINARY" <<'REMOTE'
set -eu
stage=$1 expected=$2 slug=$3 binary=$4
case "$stage" in /roms/ports/."$slug"-install.*) ;; *) exit 1;; esac
cd "$stage"
unzip -q package.zip
actual=$(sha256sum "$slug/$binary.aarch64" | cut -d ' ' -f 1)
[ "$actual" = "$expected" ]
launcher=$(python3 - "$slug/product.json" "$slug" "$binary" <<'PY'
import json, pathlib, sys
p=json.loads(pathlib.Path(sys.argv[1]).read_text())
if p['slug']!=sys.argv[2] or p['binary']!=sys.argv[3]: raise SystemExit('Package identity differs from this project')
name=p['launcher']
if pathlib.Path(name).name!=name: raise SystemExit('Invalid launcher name')
print(name)
PY
)
chmod +x "$slug/$binary.aarch64"
while IFS= read -r name; do chmod +x "$name"; "./$name" >"$name.log" 2>&1; done <device-tests.txt
python3 app_cli.py "./$slug/$binary.aarch64" >cli.log 2>&1
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "./$slug/$binary.aarch64" --smoke >smoke.log 2>&1
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "./$slug/$binary.aarch64" --smoke --loop-demo --seconds .6 >loop-smoke.log 2>&1
app="/roms/ports/$slug"
if [ -e "$app" ]; then
    python3 - "$app/product.json" "$slug/product.json" <<'PYOWN'
import json, pathlib, sys
existing=json.loads(pathlib.Path(sys.argv[1]).read_text())
current=json.loads(pathlib.Path(sys.argv[2]).read_text())
if any(existing[key]!=current[key] for key in ('identifier','slug','binary')): raise SystemExit('Destination belongs to another application')
PYOWN
elif [ -e "/roms/ports/$launcher" ]; then
    echo 'Destination launcher already belongs to another installation.'
    exit 1
fi
python3 - "$app/$binary.aarch64" <<'PY'
from pathlib import Path
import os, signal, sys, time
exe=str(Path(sys.argv[1]).resolve()); pids=[]
for process in Path('/proc').glob('[0-9]*'):
    try:
        if os.readlink(process/'exe')==exe:
            os.kill(int(process.name),signal.SIGTERM); pids.append(process)
    except (OSError,ValueError): pass
for _ in range(150):
    if all(not p.exists() for p in pids): break
    time.sleep(.02)
else: raise SystemExit('Close the application, then install again.')
PY
settings_before=$(sha256sum "$app/conf/settings.cfg" 2>/dev/null || true)
mkdir -p "$app/conf" "$app/licenses"
mv "$slug/$binary.aarch64" "$app/$binary.aarch64"
cp "$launcher" "/roms/ports/$launcher"
chmod +x "/roms/ports/$launcher"
cp "$slug/licenses/"* "$app/licenses/"
for name in README.md screenshot.png VERSION product.json; do cp "$slug/$name" "$app/$name"; done
settings_after=$(sha256sum "$app/conf/settings.cfg" 2>/dev/null || true)
[ "$settings_before" = "$settings_after" ]
version=$(cat "$app/VERSION")
case "$version" in ''|*[!0-9.]*) exit 1;; esac
while IFS= read -r name; do cat "$name.log"; done <device-tests.txt >"$app/install-$version.log"
cat cli.log smoke.log loop-smoke.log >>"$app/install-$version.log"
echo "Installed $version. Application data preserved."
sha256sum "$app/$binary.aarch64"
cat "$app/install-$version.log"
cd "$app"
rm -rf "$stage"
REMOTE
