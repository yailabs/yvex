#!/usr/bin/env python3
"""Refuse symlink ancestors before native build cleanup or package replacement."""
import os
from pathlib import Path
import stat
import sys


def safe(path):
    if not path or any(part in ('.', '..') for part in path.split('/')):
        return False
    current = Path('/')
    for part in Path(os.path.abspath(path)).parts[1:]:
        current /= part
        try:
            value = current.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(value.st_mode):
            # macOS installs these exact root-owned aliases. Do not resolve
            # caller-controlled links in any remaining path component.
            if (sys.platform == 'darwin' and current in (Path('/tmp'), Path('/var'))
                    and value.st_uid == 0 and os.readlink(current) == 'private' + str(current)):
                current = Path('/private') / current.name
            else:
                return False
    return True


if __name__ == '__main__':
    try:
        accepted = len(sys.argv) == 2 and safe(sys.argv[1])
    except OSError:
        accepted = False
    raise SystemExit(0 if accepted else 1)
