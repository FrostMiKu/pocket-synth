#!/usr/bin/env python3
"""Package a host build for local Mac playtesting."""
from product import load_product
import argparse
import plistlib
import re
import shutil
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('binary', type=Path)
p.add_argument('destination', type=Path)
a = p.parse_args()
if not a.binary.is_file():
    p.error("Provide a compiled macOS executable")
root = Path(__file__).resolve().parents[1]
product = load_product()
app = a.destination.resolve() / (product['name'] + '.app')
if app.exists():
    shutil.rmtree(app)
mac = app / 'Contents/MacOS'
resources = app / 'Contents/Resources'
libs = resources / 'lib'
libs.mkdir(parents=True, exist_ok=True)
mac.mkdir(parents=True, exist_ok=True)
binary = mac / product['identifier']
shutil.copy2(a.binary, binary)
linked = subprocess.check_output(['otool', '-L', str(binary)], text=True)
sdl_link = next(line.strip().split(' (')[0] for line in linked.splitlines() if '/libSDL2' in line)
metadata = subprocess.check_output(['otool', '-l', str(binary)], text=True)
rpaths = re.findall(r'cmd LC_RPATH.*?path (.*?) \(offset', metadata, re.DOTALL)
def expand_path(path):
    return Path(path.replace('@loader_path', str(a.binary.resolve().parent)).replace('@executable_path', str(a.binary.resolve().parent)))
candidates = [expand_path(path) / sdl_link[len('@rpath/'):] for path in rpaths] if sdl_link.startswith('@rpath/') else [expand_path(sdl_link)]
sdl = next((path.resolve() for path in candidates if path.is_file()), None)
if sdl is None:
    p.error('Locate SDL2 through an absolute library path or an executable RPATH')
shutil.copy2(sdl, libs / 'libSDL2.dylib')
subprocess.run(['install_name_tool', '-change', sdl_link, '@executable_path/../Resources/lib/libSDL2.dylib', str(binary)], check=True)
for rpath in rpaths:
    subprocess.run(['install_name_tool', '-delete_rpath', rpath, str(binary)], check=True)
licenses = resources / 'licenses'
licenses.mkdir(exist_ok=True)
shutil.copy2(root / 'LICENSE', licenses / (product['identifier'] + '.txt'))
shutil.copy2(root / 'vendor/DaisySP/LICENSE', licenses / 'DaisySP.txt')
for license_file in (root / 'vendor/runtime').glob('*.txt'):
    shutil.copy2(license_file, licenses / license_file.name)
sdl_prefix = sdl.parent.parent
shutil.copy2(sdl_prefix / 'LICENSE.txt', licenses / 'SDL2.txt')
if 'sdl2-compat' in str(sdl):
    prefix = Path(subprocess.check_output(['brew', '--prefix', 'sdl3'], text=True).strip())
    shutil.copy2(prefix / 'lib/libSDL3.dylib', libs / 'libSDL3.dylib')
    shutil.copy2(prefix / 'LICENSE.txt', licenses / 'SDL3.txt')
for file in libs.glob('*.dylib'):
    file.chmod(0o755)
    subprocess.run(['install_name_tool', '-id', '@rpath/' + file.name, str(file)], check=True)
minimums = []
for file in [*libs.glob('*.dylib'), binary]:
    metadata = subprocess.check_output(['otool', '-l', str(file)], text=True)
    minimums.extend(re.findall(r'^\s*minos\s+([0-9.]+)', metadata, re.MULTILINE))
    subprocess.run(['codesign', '--force', '--sign', '-', str(file)], check=True)
with (app / 'Contents/Info.plist').open('wb') as out:
    plistlib.dump({
        'CFBundleExecutable': product['identifier'],
        'CFBundleIdentifier': 'org.' + product['slug'] + '.mac',
        'CFBundleName': product['name'],
        'CFBundleDisplayName': product['name'],
        'CFBundlePackageType': 'APPL',
        'CFBundleVersion': (root / 'VERSION').read_text().strip(),
        **({'LSMinimumSystemVersion': max(minimums, key=lambda version: tuple(map(int, version.split('.'))))} if minimums else {}),
        'NSHighResolutionCapable': True,
        'NSHumanReadableCopyright': next((line for line in (root / 'LICENSE').read_text().splitlines() if line.startswith('Copyright')), ''),
    }, out)
subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
print(app)
