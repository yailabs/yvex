#!/usr/bin/env sh
set -eu

test -x build/tests/test

out="${TMPDIR:-/tmp}/yvex-gguf-layout-integrity.$$"
trap 'rm -f "$out"' EXIT

build/tests/test >"$out" 2>&1
grep -nF 'test: gguf_layout_integrity' "$out" >/dev/null

# One domain owner validates ordered padded layout. Reports and consumers must
# project it instead of reconstructing range policy or hashing by default.
test -f include/yvex/gguf.h
test -f src/gguf/layout_integrity.c
grep -nF 'int yvex_gguf_layout_validate(' include/yvex/gguf.h >/dev/null
grep -nF 'yvex_gguf_layout_validate(artifact, gguf' \
  src/artifact/integrity.c >/dev/null
grep -nF 'yvex_gguf_layout_validate(artifact, gguf' \
  src/gguf/report.c >/dev/null
grep -nF 'yvex_gguf_range_fact_from_layout' src/gguf/descriptor.c >/dev/null
grep -nF 'yvex_artifact_integrity_validate' src/graph/guard.c >/dev/null
grep -nF 'yvex_artifact_integrity_validate' src/runtime/core.c >/dev/null
grep -nF 'yvex_artifact_integrity_validate' \
  src/model/artifacts/gate.c >/dev/null
grep -nF 'yvex_gguf_layout_validate(artifact, gguf' \
  src/model/core.c >/dev/null

grep -nF '(out->alignment & (out->alignment - 1u)) != 0u' \
  src/gguf/layout_integrity.c >/dev/null
grep -nF 'tensor_payload_bytes_read = 0ull' \
  src/gguf/layout_integrity.c >/dev/null
grep -nF '69187ull' tests/unit/gguf_layout_integrity.c >/dev/null
grep -nF '24u' tests/unit/gguf_layout_integrity.c >/dev/null
grep -nF 'YVEX_GGUF_LAYOUT_NONCANONICAL_TAIL' \
  tests/unit/gguf_layout_integrity.c >/dev/null

if grep -nF 'strstr' src/gguf/layout_integrity.c \
  src/artifact/integrity.c >/dev/null; then
  echo 'GGUF layout guard: semantic policy depends on error strings' >&2
  exit 1
fi
if grep -nE '\b(qsort|heapsort|mergesort)\s*\(' \
  src/gguf/layout_integrity.c >/dev/null; then
  echo 'GGUF layout guard: directory order is being normalized before admission' >&2
  exit 1
fi
if grep -nF 'yvex_artifact_identity_read(path' \
  src/artifact/integrity.c >/dev/null; then
  echo 'GGUF layout guard: integrity reopened the path for implicit hashing' >&2
  exit 1
fi
grep -nF 'yvex_artifact_identity_read_open' \
  src/artifact/integrity.c >/dev/null
