#!/usr/bin/env python3
"""Authenticate and build the pinned independent GGUF reader for the Rust shell.

The C computational library remains independent. Reuse verifies every installed
header/library and the build inputs; stale prefixes refuse, never get erased.
"""
import argparse
import fcntl
import hashlib
import json
import platform
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def command(args):
    return subprocess.check_output(args, text=True).strip()


def inventory(prefix):
    result = {}
    for path in sorted(prefix.rglob('*')):
        if path.is_symlink():
            raise RuntimeError('GGUF reference prefix contains a symlink')
        if path.is_file() and path.name != 'receipt.json':
            result[path.relative_to(prefix).as_posix()] = digest(path)
    return result


def prepare(destination):
    pin = json.loads((ROOT / 'config/gguf_reference.json').read_text())
    identity = dict(pin=pin, system=platform.system(), machine=platform.machine(),
                    cc=command(['cc', '--version']), cxx=command(['c++', '--version']),
                    cmake=command(['cmake', '--version']), script=digest(Path(__file__)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.with_suffix('.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if destination.is_symlink():
            raise RuntimeError('GGUF reference prefix must not be a symlink')
        if destination.exists():
            record = json.loads((destination / 'receipt.json').read_text())
            if record['build'] != identity or record['sha256'] != inventory(destination):
                raise RuntimeError('GGUF reference prefix is stale or altered; choose a fresh build directory')
            return
        with tempfile.TemporaryDirectory(prefix='gguf-reference-', dir=destination.parent) as temp:
            work = Path(temp)
            archive = work / 'source.tar.gz'
            url = f"https://codeload.github.com/ggml-org/ggml/tar.gz/{pin['revision']}"
            with urllib.request.urlopen(url, timeout=60) as incoming, archive.open('wb') as output:
                shutil.copyfileobj(incoming, output)
            if digest(archive) != pin['archive_sha256']:
                raise RuntimeError('GGUF reference archive integrity failure')
            with tarfile.open(archive) as bundle:
                bundle.extractall(work, filter='data')
            source = work / ('ggml-' + pin['revision'])
            build = work / 'build'
            subprocess.run(['cmake', '-S', str(source), '-B', str(build),
                            '-DCMAKE_C_COMPILER=cc', '-DCMAKE_CXX_COMPILER=c++',
                            '-DGGML_BUILD_TESTS=OFF', '-DGGML_BUILD_EXAMPLES=OFF',
                            '-DGGML_BUILD_TOOLS=OFF', '-DGGML_BUILD_SERVER=OFF',
                            '-DGGML_CUDA=OFF', '-DGGML_METAL=OFF', '-DGGML_OPENMP=OFF',
                            '-DBUILD_SHARED_LIBS=OFF', '-DCMAKE_BUILD_TYPE=Release',
                            '-DCMAKE_C_FLAGS=', '-DCMAKE_CXX_FLAGS='], check=True)
            subprocess.run(['cmake', '--build', str(build), '--target', 'ggml-base', '-j4'], check=True)
            staged = work / 'prefix'
            (staged / 'lib').mkdir(parents=True)
            shutil.copytree(source / 'include', staged / 'include')
            shutil.copy2(source / 'LICENSE', staged / 'LICENSE')
            shutil.copy2(build / 'src/libggml-base.a', staged / 'lib/libggml-base.a')
            record = dict(build=identity, sha256=inventory(staged))
            (staged / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
            staged.rename(destination)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    prepare(parser.parse_args().prefix.absolute())
