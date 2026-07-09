#!/usr/bin/env sh
set -eu

test -x build/tests/test

out="${TMPDIR:-/tmp}/yvex-gguf-qtype-abi.$$"
trap 'rm -f "$out"' EXIT

build/tests/test >"$out" 2>&1
grep -nF 'test: gguf_qtype_abi' "$out" >/dev/null
