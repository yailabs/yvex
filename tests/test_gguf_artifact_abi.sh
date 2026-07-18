#!/usr/bin/env sh
set -eu

test -x build/tests/test

out="${TMPDIR:-/tmp}/yvex-gguf-artifact-abi.$$"
trap 'rm -f "$out"' EXIT

build/tests/test >"$out" 2>&1
grep -nF 'test: gguf_artifact_abi' "$out" >/dev/null

# The operational reader owns one typed, file-backed parse; the completed
# layout owner now hands the foundation sequence to CUDA fail-closed repair.
# Reports and consumers may not restore whole-file parsing or string-derived
# failure policy.
grep -nF '#define YVEX_GGUF_ABI_NEXT_ROW "V010.CUDA.FAILCLOSED.0"' \
  src/gguf/private.h >/dev/null
grep -nF 'int yvex_artifact_read_at(' include/yvex/artifact.h >/dev/null
grep -nF 'int yvex_gguf_open_ex(' include/yvex/gguf.h >/dev/null
grep -nF 'yvex_gguf_parse_result parse_result;' \
  src/gguf/private.h >/dev/null
grep -nF 'yvex_gguf_reader_stats reader_stats;' \
  src/gguf/private.h >/dev/null

if grep -nE '\b(fseek|ftell|fread)\b' src/artifact/core.c >/dev/null; then
  echo 'artifact ABI guard: whole-file stdio path returned' >&2
  exit 1
fi
if grep -nF 'yvex_artifact_data' src/gguf/*.c >/dev/null; then
  echo 'artifact ABI guard: GGUF parser depends on whole-file artifact data' >&2
  exit 1
fi
if grep -nF 'strstr' src/gguf/reader.c >/dev/null; then
  echo 'artifact ABI guard: reader failure policy depends on error strings' >&2
  exit 1
fi
if grep -nE 'for \([^;]+;[^;]+< i;' src/artifact/integrity.c >/dev/null; then
  echo 'artifact ABI guard: integrity restored quadratic duplicate detection' >&2
  exit 1
fi
if grep -R -nF 'required_key_count' src/gguf include/yvex >/dev/null; then
  echo 'artifact ABI guard: synthetic required-key count returned' >&2
  exit 1
fi

test "$(grep -cF 'yvex_gguf_open_ex(' src/gguf/report.c)" -eq 1
if grep -nF 'yvex_gguf_read_header' src/gguf/report.c >/dev/null; then
  echo 'artifact ABI guard: report reparses the GGUF header' >&2
  exit 1
fi
grep -nF '69187ull' tests/unit/gguf_artifact_abi.c >/dev/null
grep -nF '129280ull' tests/unit/gguf_artifact_abi.c >/dev/null
grep -nF '160ull * 1024ull * 1024ull * 1024ull' \
  tests/unit/gguf_artifact_abi.c >/dev/null
