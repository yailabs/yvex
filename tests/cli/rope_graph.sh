#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/rope-graph}
mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

if "$YVEX_BIN" graph --backend cpu --execute-op --op rope \
    --position 7 --head-dim 8 >"$OUT_DIR/rope.out" 2>"$OUT_DIR/rope.err"; then
    fail "retired production RoPE proof unexpectedly executed"
fi
contains "$OUT_DIR/rope.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/rope.out" "graph_execution_phase: admission"
contains "$OUT_DIR/rope.out" "execution_ready: false"
contains "$OUT_DIR/rope.out" "attention_execution_supported: true"
contains "$OUT_DIR/rope.out" "generation_ready: false"
contains "$OUT_DIR/rope.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/rope.out" "status: graph-proof-retired"
contains "$OUT_DIR/rope.err" "production graph fixtures were retired to focused tests"

printf 'cli rope graph: retired proof refusal ok\n'
