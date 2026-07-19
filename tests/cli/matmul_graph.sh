#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/matmul-graph}
mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

if "$YVEX_BIN" graph --backend cpu --execute-op --op matmul \
    --m 1 --k 8 --n 8 >"$OUT_DIR/matmul.out" 2>"$OUT_DIR/matmul.err"; then
    fail "retired production matmul proof unexpectedly executed"
fi
contains "$OUT_DIR/matmul.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/matmul.out" "graph_execution_phase: admission"
contains "$OUT_DIR/matmul.out" "execution_ready: false"
contains "$OUT_DIR/matmul.out" "attention_execution_supported: false"
contains "$OUT_DIR/matmul.out" "generation_ready: false"
contains "$OUT_DIR/matmul.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/matmul.out" "status: graph-proof-retired"
contains "$OUT_DIR/matmul.err" "production graph fixtures were retired to focused tests"

printf 'cli matmul graph: retired proof refusal ok\n'
