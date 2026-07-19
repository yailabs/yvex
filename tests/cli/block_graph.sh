#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/block-graph}
mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

assert_retired() {
    name=$1
    shift
    if "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err"; then
        fail "$name unexpectedly executed"
    fi
    contains "$OUT_DIR/$name.out" "graph_integrity_guard: refused"
    contains "$OUT_DIR/$name.out" "graph_execution_phase: admission"
    contains "$OUT_DIR/$name.out" "execution_ready: false"
    contains "$OUT_DIR/$name.out" "attention_execution_supported: false"
    contains "$OUT_DIR/$name.out" "generation_ready: false"
    contains "$OUT_DIR/$name.out" "reason: production-fixtures-are-test-owned"
    contains "$OUT_DIR/$name.out" "status: graph-proof-retired"
    contains "$OUT_DIR/$name.err" "production graph fixtures were retired to focused tests"
}

assert_retired block "$YVEX_BIN" graph --backend cpu --execute-block --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
assert_retired layers "$YVEX_BIN" graph --backend cpu --execute-layers --layers 2 \
    --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
assert_retired check "$YVEX_BIN" graph check --backend cpu --suite all --layers 2

printf 'cli block graph: retired proof refusals ok\n'
