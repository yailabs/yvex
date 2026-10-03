#!/bin/sh
# CUDA CLI smoke: device admission, backend facts, and bounded materialization.

set -u

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-${OUT_DIR:-build/tests/cli/cuda}}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    value=$2
    python3 tests/support/human_field.py "$file" "$value" || fail "$file missing: $value"
}

matches() {
    python3 tests/support/human_field.py --regex "$1" "$2" || fail "$1 missing pattern: $2"
}

"$YVEX_BIN" inspect cuda >"$OUT_DIR/cuda_info.out" 2>"$OUT_DIR/cuda_info.err"
rc=$?
if [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/cuda_info.out" "BACKEND  cuda · unavailable"
    printf 'cli cuda smoke: skip\n'
    exit 77
fi
[ "$rc" -eq 0 ] || fail "cuda-info exit code was $rc"
contains "$OUT_DIR/cuda_info.out" "BACKEND  cuda · ready"
matches "$OUT_DIR/cuda_info.out" 'CUDA  context available · bundle admitted · [^ ·]+ · (native|PTX)'
matches "$OUT_DIR/cuda_info.out" 'BUNDLE  [0-9a-f]{64} · none'

"$YVEX_BIN" inspect cuda bandwidth >"$OUT_DIR/bandwidth.out" 2>"$OUT_DIR/bandwidth.err"
rc=$?
[ "$rc" -eq 0 ] || fail "cuda bandwidth exit code was $rc"
matches "$OUT_DIR/bandwidth.out" 'BANDWIDTH  [0-9]+ B working set · [0-9]+ iterations · 5 samples'
matches "$OUT_DIR/bandwidth.out" 'RATE  read [0-9.]+ GB/s · copy [0-9.]+ GB/s · coherent host [0-9.]+ GB/s'

"$YVEX_BIN" inspect backend cuda >"$OUT_DIR/backend.out" 2>"$OUT_DIR/backend.err"
rc=$?
[ "$rc" -eq 0 ] || fail "backend cuda exit code was $rc"
contains "$OUT_DIR/backend.out" "BACKEND  cuda · ready"
matches "$OUT_DIR/backend.out" 'CUDA  context available · bundle admitted · [^ ·]+ · (native|PTX)'
matches "$OUT_DIR/backend.out" 'BUNDLE  [0-9a-f]{64} · none'

"$YVEX_BIN" artifact materialize --model "$FIXTURE" --backend cuda \
    >"$OUT_DIR/materialize.out" 2>"$OUT_DIR/materialize.err"
rc=$?
[ "$rc" -eq 0 ] || fail "materialize cuda exit code was $rc"
contains "$OUT_DIR/materialize.out" "materialization_status: materialized"
contains "$OUT_DIR/materialize.out" "backend: cuda"
contains "$OUT_DIR/materialize.out" "status: weights-materialized"

"$YVEX_BIN" help --advanced >"$OUT_DIR/help.out" 2>"$OUT_DIR/help.err"
rc=$?
[ "$rc" -eq 0 ] || fail "help cuda-info exit code was $rc"
contains "$OUT_DIR/help.out" "yvex inspect backend"
contains "$OUT_DIR/help.out" "yvex inspect moe"
contains "$OUT_DIR/help.out" "yvex inspect target"

"$YVEX_BIN" help inspect >"$OUT_DIR/inspect_help.out" 2>"$OUT_DIR/inspect_help.err"
rc=$?
[ "$rc" -eq 0 ] || fail "help inspect exit code was $rc"
contains "$OUT_DIR/inspect_help.out" "yvex inspect cuda"

printf 'cli cuda smoke: ok\n'
