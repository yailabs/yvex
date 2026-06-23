#!/usr/bin/env sh
set -eu

test ! -d src
test ! -d cli
test ! -d server

test -x ./yvex
test -x ./yvexd

test -f yvex_cli.c
test -f yvexd.c
test -f yvex_core.c
test -f yvex_model_tools.c
test -f yvex_quant.c
test -f yvex_source.c

test -d include/yvex
test -d backends/cuda
test -d docs
test -d tests

root_c_count="$(find . -maxdepth 1 -type f -name 'yvex*.c' | wc -l | tr -d ' ')"

if [ "$root_c_count" -gt 30 ]; then
  echo "too many root C files: $root_c_count"
  exit 1
fi

echo "source layout: ok"
