#!/usr/bin/env sh
set -eu

test -f tests/test.c
test -f tests/test_cuda.c
test -f tests/cli.sh
test -f tests/test.h
test -d tests/fixtures
test -d tests/vectors
test -f tests/vectors/manifest.json
test -f tests/vectors/tokenizer.jsonl
test -f tests/vectors/gguf.jsonl
test -f tests/vectors/cuda.jsonl

test -f tests/test_code_natural.sh
test -f tests/test_docs_surface.sh
test -f tests/test_source_layout.sh
test -f include/yvex/decode.h

c_count="$(find tests -maxdepth 1 -type f \( -name 'test.c' -o -name 'test_*.c' \) | wc -l | tr -d ' ')"
if [ "$c_count" -gt 2 ]; then
    echo "too many top-level test C runners: $c_count"
    find tests -maxdepth 1 -type f -name 'test_*.c' | sort
    exit 1
fi

cli_count="$(find tests -maxdepth 1 -type f -name 'test_cli*.sh' | wc -l | tr -d ' ')"
if [ "$cli_count" -ne 0 ]; then
    echo "old CLI test scripts remain"
    find tests -maxdepth 1 -type f -name 'test_cli*.sh' | sort
    exit 1
fi

test -d tests/unit
test -d tests/unit/cuda

echo "test surface: ok"
