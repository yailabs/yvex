#!/usr/bin/env python3
"""Independent literal binary64 and integer/Fraction sampling-sum controls.

This directly tests the producer's private implementation in a standalone CUDA
translation unit, not a test-only production export. Exact-real rounding is a
second oracle, not a replacement for the contract's literal compensated sum.
"""
import argparse
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile


def bits(value):
    return struct.unpack('<Q', struct.pack('<d', value))[0]


def verify(path):
    counts, certified, fallback, literal_not_real = [], 0, 0, 0
    with path.open('rb') as stream:
        while header := stream.read(48):
            assert len(header) == 48
            count, pattern, literal, actual, accepted, admitted = struct.unpack('<6Q', header)
            assert 0 < count <= 129283 and pattern < 10 and admitted in (0,1)
            raw = stream.read(count * 8)
            assert len(raw) == count * 8
            values = struct.unpack('<'+str(count)+'d', raw)
            high, low, total, valid = 0.0, 0.0, 0, True
            for value in values:
                if not math.isfinite(value) or value < 0:
                    valid = False
                    continue
                following = high + value
                low += (high-following)+value if abs(high) >= abs(value) else (value-following)+high
                high = following
                encoded = bits(value)
                exponent, mantissa = (encoded >> 52) & 2047, encoded & ((1 << 52)-1)
                if exponent:
                    mantissa |= 1 << 52
                    total += mantissa << (exponent-1)
                else:
                    total += mantissa
            assert valid == bool(admitted)
            assert bits(high+low) == literal == actual
            exact = bits(float(Fraction(total, 1 << 1074)))
            if accepted:
                assert admitted and actual == exact, (count, pattern, 'invalid certificate')
                certified += 1
            else:
                fallback += 1
            if admitted and literal != exact:
                assert not accepted
                literal_not_real += 1
            if pattern == 6 and count >= 4:
                assert admitted and literal != exact and not accepted
            counts.append(dict(count=count,pattern=pattern,input_sha256=hashlib.sha256(raw).hexdigest(),
                literal_bits=f'{literal:016x}',exact_real_bits=f'{exact:016x}',certificate=bool(accepted),
                valid=bool(admitted)))
    assert len(counts) == 100 and certified and fallback and literal_not_real
    return dict(schema='yvex.diagnostic.sampling-certificate-oracle.v1',
        input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        reference_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        cases=counts,certified=certified,fallback=fallback,literal_differs_from_real=literal_not_real,
        result='PASS',scope='Exact binary64 numerical primitive; not model conformance or performance')


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--tool', choices=('memcheck', 'racecheck', 'synccheck'))
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    fixture = root / 'tests/reference/sampling_certificate.cu'
    with tempfile.TemporaryDirectory(prefix='yvex-sampling-certificate-') as directory:
        work = Path(directory)
        binary, raw = work / 'probe', work / 'cases.bin'
        command = ['nvcc', '-O3', '-lineinfo', '-arch=native',
                   '-I'+str(root/'include'), '-I'+str(root),
                   '--compiler-options=-fno-fast-math,-ffp-contract=off',
                   str(fixture), '-o', str(binary)]
        subprocess.run(command, check=True, timeout=120)
        runner = [str(binary), str(raw)]
        if args.tool:
            runner = ['compute-sanitizer', '--tool', args.tool,
                      '--error-exitcode', '99'] + runner
        run = subprocess.run(runner, capture_output=True, text=True, timeout=1800)
        if run.returncode:
            raise RuntimeError(f'private numerical control failed: {run.stdout}\n{run.stderr}')
        report = verify(raw)
        report.update(fixture_sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
                      producer_sha256=hashlib.sha256(
                          (root/'src/backend/cuda/sampling_kernels.cu').read_bytes()).hexdigest(),
                      probe_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                      sanitizer=args.tool, compiler_arguments=command[:-1],
                      initialization_controls=40, rounding_cell_controls=8)
        print(json.dumps(report, indent=2, allow_nan=False))


if __name__ == '__main__':
    main()
