#!/usr/bin/env python3
"""Shared product identity for build and packaging tools."""
import argparse
import json
import re
import shlex
from pathlib import Path

def load_product():
    root = Path(__file__).resolve().parents[1]
    product = json.loads((root / 'product.json').read_text())
    for key in ('slug', 'binary', 'identifier'):
        if not re.fullmatch(r'[A-Za-z][A-Za-z0-9-]*', product[key]):
            raise ValueError(f'Invalid product {key}')
    product['launcher'] = product['name'] + '.sh'
    return product

def write_changed(path, content):
    if not path.exists() or path.read_text() != content:
        path.write_text(content)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('field', nargs='?')
    parser.add_argument('--generate', type=Path)
    parser.add_argument('--shell', action='store_true')
    args = parser.parse_args()
    product = load_product()
    if args.generate:
        args.generate.mkdir(parents=True, exist_ok=True)
        header = '#pragma once\nnamespace ps::product {\n'
        cmake = ''
        for key, value in product.items():
            header += f'inline constexpr char {key}[] = {json.dumps(value)};\n'
            cmake += f'set(PS_{key.upper()} {json.dumps(value)})\n'
        write_changed(args.generate / 'product.hpp', header + '}\n')
        write_changed(args.generate / 'product.cmake', cmake)
    elif args.shell:
        for key, value in product.items():
            print(f'PS_{key.upper()}={shlex.quote(value)}')
    elif args.field:
        print(product[args.field])

if __name__ == '__main__':
    main()
