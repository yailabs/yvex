#!/usr/bin/env sh
set -eu

test -x build/tests/test
test -f include/yvex/gguf_qtype.h

out="${TMPDIR:-/tmp}/yvex-gguf-qtype-abi.$$"
trap 'rm -f "$out"' EXIT

build/tests/test >"$out" 2>&1
grep -nF 'test: gguf_qtype_abi' "$out" >/dev/null

grep -nF 'af97976c7810cdabb1863172f31c432dab767de7' \
  include/yvex/gguf_qtype.h docs/reference-architecture.md >/dev/null
grep -nF 'yvex_gguf_qtype_tensor_storage' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_qtype_tensor_storage' src/gguf/gguf.c >/dev/null
grep -nF 'yvex_gguf_qtype_tensor_storage' \
  src/gguf/yvex_gguf_layout_integrity.c >/dev/null
grep -nF 'layout->raw_tensor_bytes' src/gguf/yvex_gguf_range_map.c >/dev/null
if grep -nF 'yvex_gguf_qtype_tensor_storage' src/gguf/yvex_gguf_range_map.c >/dev/null; then
  echo 'range projection must consume canonical reader storage facts once' >&2
  exit 1
fi
grep -nF 'yvex_dtype_tensor_storage_bytes' \
  src/model/yvex_model.c \
  src/artifact/yvex_artifact_integrity.c \
  src/graph/yvex_memory_plan.c >/dev/null
grep -nF 'yvex_gguf_qtype_geometry_find' src/gguf/conversion.c >/dev/null
grep -nF 'yvex_gguf_qtype_geometry_find_by_name' src/gguf/tools.c >/dev/null
grep -nF '#define YVEX_GGUF_QTYPE_ABI_NEXT_ROW "V010.GGUF.ARTIFACT.ABI.1"' \
  src/gguf/yvex_gguf_private.h >/dev/null

if grep -nE 'block_elems|block_bytes|scalar_bytes|is_supported_for_storage_accounting' \
    include/yvex/dtype.h src/model/yvex_model.c; then
  echo 'dtype owner must project canonical GGUF storage geometry' >&2
  exit 1
fi

if grep -nF 'ceil-block' src/gguf/yvex_gguf_qtype.c src/model/yvex_model.c; then
  echo 'partial block padding must not re-enter qtype accounting' >&2
  exit 1
fi

if grep -nF 'static const char *ggml_type_name' src/gguf/gguf.c; then
  echo 'GGUF parser must use the canonical qtype identity owner' >&2
  exit 1
fi

if grep -nF 'qtype_to_ggml' src/gguf/conversion.c; then
  echo 'conversion must not recreate a qtype ID/width registry' >&2
  exit 1
fi

if grep -nF 'yvex_dtype_storage_bytes' \
    src/artifact/yvex_artifact_integrity.c src/graph/yvex_memory_plan.c; then
  echo 'multidimensional consumers must not use the single-row compatibility API' >&2
  exit 1
fi
