#!/usr/bin/env sh
set -eu

test -x build/tests/test

out="${TMPDIR:-/tmp}/yvex-gguf-artifact-abi.$$"
trap 'rm -f "$out"' EXIT

build/tests/test >"$out" 2>&1
grep -nF 'test: gguf_artifact_abi' "$out" >/dev/null
