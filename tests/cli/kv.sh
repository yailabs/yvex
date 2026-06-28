#!/bin/sh
#
# YVEX - KV CLI tests
#
# File: tests/cli/kv.sh
# Layer: test
#
# Purpose:
#   Proves that the CLI exposes the minimal session-owned KV append/read
#   boundary without claiming decode, logits, sampling, generation, or prefill.

set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/kv}

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

run_ok() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" || fail "$name exited non-zero"
}

run_ok owned "$YVEX_BIN" kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
contains "$OUT_DIR/owned.out" "kv: ownership"
contains "$OUT_DIR/owned.out" "kv_created: true"
contains "$OUT_DIR/owned.out" "session_owned: true"
contains "$OUT_DIR/owned.out" "layers: 1"
contains "$OUT_DIR/owned.out" "heads: 2"
contains "$OUT_DIR/owned.out" "head_dim: 4"
contains "$OUT_DIR/owned.out" "capacity: 8"
contains "$OUT_DIR/owned.out" "dtype: F32"
contains "$OUT_DIR/owned.out" "values_per_position: 16"
contains "$OUT_DIR/owned.out" "bytes_per_position: 64"
contains "$OUT_DIR/owned.out" "planned_bytes: 512"
contains "$OUT_DIR/owned.out" "allocated_bytes: 512"
contains "$OUT_DIR/owned.out" "append_count: 2"
contains "$OUT_DIR/owned.out" "read_count: 1"
contains "$OUT_DIR/owned.out" "written_positions: 2"
contains "$OUT_DIR/owned.out" "last_appended_position: 1"
contains "$OUT_DIR/owned.out" "read_position: 0"
contains "$OUT_DIR/owned.out" "read_value_count: 16"
contains "$OUT_DIR/owned.out" "read_sample_values: 0,1,2,3,4,5,6,7"
contains "$OUT_DIR/owned.out" "overflow_status: not-overflowed"
contains "$OUT_DIR/owned.out" "cleanup_attempted: true"
contains "$OUT_DIR/owned.out" "cleanup_status: pass"
contains "$OUT_DIR/owned.out" "decode_ready: false"
contains "$OUT_DIR/owned.out" "logits_ready: false"
contains "$OUT_DIR/owned.out" "generation_ready: false"
contains "$OUT_DIR/owned.out" "generation: unsupported"
contains "$OUT_DIR/owned.out" "status: kv-owned"

set +e
"$YVEX_BIN" kv --layers 0 --heads 2 --head-dim 4 --capacity 8 >"$OUT_DIR/zero_shape.out" 2>"$OUT_DIR/zero_shape.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "zero shape exit code was $rc, expected 2"
fi
contains "$OUT_DIR/zero_shape.err" "yvex: --layers requires a positive integer"

set +e
"$YVEX_BIN" kv --layers 1 --heads 2 --head-dim 4 --capacity 1 --append-demo --read-position 1 >"$OUT_DIR/read_unwritten.out" 2>"$OUT_DIR/read_unwritten.err"
rc=$?
set -e
if [ "$rc" -ne 4 ]; then
    fail "read unwritten exit code was $rc, expected 4"
fi
contains "$OUT_DIR/read_unwritten.err" "read position 1 exceeds KV capacity 1"

printf 'cli: kv ok\n'
