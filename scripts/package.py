#!/usr/bin/env python3
"""Build a manual-install zip and a PortMaster submission directory."""
from product import load_product
import argparse
import json
import shutil
import struct
import zipfile
import xml.etree.ElementTree as ET
from pathlib import Path

root = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('binary', type=Path)
p.add_argument('destination', type=Path)
args = p.parse_args()
header = args.binary.read_bytes()[:64]
if header[:4] != b'\x7fELF' or header[4] != 2 or struct.unpack_from('<H', header, 18)[0] != 183:
    p.error('Provide an aarch64 ELF binary')
product = load_product()
meta = json.loads((root / 'port/metadata.json').read_text())
meta['name'] = product['slug'] + '.zip'
meta['items'] = [product['launcher'], product['slug']]
meta['attr']['title'] = product['name']
meta['attr']['porter'] = [product['name'] + ' contributors']
meta['attr']['inst'] = 'Extract into your ports folder and open ' + product['name'] + '.'
staging = args.destination / 'portmaster' / product['slug']
if staging.exists():
    p.error(f'Use a fresh destination: {staging}')
app = staging / product['slug']
binary_name = product['binary'] + '.aarch64'
(app / 'licenses').mkdir(parents=True)
(app / 'conf').mkdir()
shutil.copy2(args.binary, app / binary_name)
(app / binary_name).chmod(0o755)
launcher = (root / 'port/launcher.sh.in').read_text()
launcher = launcher.replace('@SLUG@', product['slug']).replace('@BINARY@', product['binary'])
(staging / product['launcher']).write_text(launcher)
(staging / 'port.json').write_text(json.dumps(meta, indent=2) + '\n')
(app / 'product.json').write_text(json.dumps(product, indent=2) + '\n')
info = ET.parse(root / 'port/gameinfo.xml')
info.find('./game/name').text = meta['attr']['title']
info.find('./game/path').text = './' + product['launcher']
info.find('./game/image').text = './' + product['slug'] + '/screenshot.png'
info.find('./game/developer').text = meta['attr']['porter'][0]
info.find('./game/desc').text = meta['attr']['desc']
info.write(staging / 'gameinfo.xml', encoding='utf-8', xml_declaration=True)
(staging / product['launcher']).chmod(0o755)
shutil.copy2(root / 'VERSION', app / 'VERSION')
for name in ['README.md', 'LICENSE']:
    shutil.copy2(root / name, staging / name)
shutil.copy2(root / 'LICENSE', app / 'licenses' / (product['identifier'] + '.txt'))
shutil.copy2(root / 'vendor/DaisySP/LICENSE', app / 'licenses/DaisySP.txt')
for license_file in (root / 'vendor/runtime').glob('*.txt'):
    shutil.copy2(license_file, app / 'licenses' / license_file.name)
for dest in [staging, app]:
    shutil.copy2(root / 'docs/guitar.png', dest / 'screenshot.png')
shutil.copy2(root / 'docs/快速上手.md', app / 'README.md')
with zipfile.ZipFile(args.destination / meta['name'], 'w', zipfile.ZIP_DEFLATED) as z:
    for name in meta['items']:
        path = staging / name
        files = sorted(path.rglob('*')) if path.is_dir() else [path]
        for file in files:
            if file.is_file():
                z.write(file, file.relative_to(staging))
print(args.destination / meta['name'])
