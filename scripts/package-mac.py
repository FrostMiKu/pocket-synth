#!/usr/bin/env python3
"""Package a host build for local Mac playtesting."""
from product import load_product
import argparse
import plistlib
import shutil
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('binary', type=Path)
p.add_argument('destination', type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
product = load_product()
app = a.destination.resolve() / (product['name'] + '.app')
mac = app / 'Contents/MacOS'
resources = app / 'Contents/Resources'
libs = resources / 'lib'
libs.mkdir(parents=True, exist_ok=True)
mac.mkdir(parents=True, exist_ok=True)
binary = mac / product['identifier']
for file in [*libs.glob("*.dylib"), binary]:
    if file.exists():
        file.chmod(0o755)
shutil.copy2(a.binary, binary)
linked = subprocess.check_output(['otool', '-L', str(binary)], text=True)
sdl = next(line.strip().split(' (')[0] for line in linked.splitlines() if '/libSDL2' in line)
shutil.copy2(sdl, libs / 'libSDL2.dylib')
subprocess.run(['install_name_tool', '-change', sdl, '@executable_path/../Resources/lib/libSDL2.dylib', str(binary)], check=True)
licenses = resources / 'licenses'
licenses.mkdir(exist_ok=True)
shutil.copy2(root / 'LICENSE', licenses / (product['identifier'] + '.txt'))
shutil.copy2(root / 'vendor/DaisySP/LICENSE', licenses / 'DaisySP.txt')
for license_file in (root / 'vendor/runtime').glob('*.txt'):
    shutil.copy2(license_file, licenses / license_file.name)
sdl_prefix = Path(sdl).parent.parent
shutil.copy2(sdl_prefix / 'LICENSE.txt', licenses / 'SDL2.txt')
if 'sdl2-compat' in sdl:
    prefix = Path(subprocess.check_output(['brew', '--prefix', 'sdl3'], text=True).strip())
    shutil.copy2(prefix / 'lib/libSDL3.dylib', libs / 'libSDL3.dylib')
    shutil.copy2(prefix / 'LICENSE.txt', licenses / 'SDL3.txt')
for file in [*libs.glob('*.dylib'), binary]:
    subprocess.run(['codesign', '--force', '--sign', '-', str(file)], check=True)
with (app / 'Contents/Info.plist').open('wb') as out:
    plistlib.dump({
        'CFBundleExecutable': product['identifier'],
        'CFBundleIdentifier': 'org.' + product['slug'] + '.mac',
        'CFBundleName': product['name'],
        'CFBundleDisplayName': product['name'],
        'CFBundlePackageType': 'APPL',
        'CFBundleVersion': (root / 'VERSION').read_text().strip(),
        'NSHighResolutionCapable': True,
        'NSHumanReadableCopyright': next((line for line in (root / 'LICENSE').read_text().splitlines() if line.startswith('Copyright')), ''),
    }, out)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
print(app)
