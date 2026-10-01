#!/usr/bin/env python3
"""Write BSD archive members with source paths, then let native ar own the symbol index."""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess


def archive(output, objects, archiver):
    # Darwin ar strips parent paths on insertion. BSD extended names are supported
    # by both its reader/indexer and the native linker, and keep owner identities unique.
    with output.open('wb') as stream:
        stream.write(b'!<arch>\n')
        for path in objects:
            name = str(path).encode('utf-8')
            if b'\0' in name or b'\n' in name:
                raise ValueError('invalid archive member name')
            # Header is 60 bytes; align each Mach-O payload to an eight-byte boundary.
            width = (len(name) + 3) // 8 * 8 + 4
            size = path.stat().st_size + width
            if size >= 10**10:
                raise ValueError('object exceeds BSD archive size field')
            header = (f'#1/{width:<13}'+f'{0:<12}{0:<6}{0:<6}' +
                      f'{"100644":<8}{size:<10}'+'`\n')
            if len(header) != 60:
                raise ValueError('archive member header exceeds its field width')
            stream.write(header.encode('ascii'))
            stream.write(name.ljust(width, b'\0'))
            with path.open('rb') as incoming:
                shutil.copyfileobj(incoming, stream)
            if size % 2:
                stream.write(b'\n')
    subprocess.run([*shlex.split(archiver), '-s', str(output)], check=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ar', required=True)
    parser.add_argument('output', type=Path)
    parser.add_argument('objects', nargs='+', type=Path)
    args = parser.parse_args()
    archive(args.output, args.objects, args.ar)
