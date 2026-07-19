#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/attention-graph}
mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

if "$YVEX_BIN" graph --backend cpu --execute-op --op attention \
    --seq-len 4 --position 3 --head-dim 8 --causal \
    >"$OUT_DIR/attention.out" 2>"$OUT_DIR/attention.err"; then
    fail "retired production attention proof unexpectedly executed"
fi
contains "$OUT_DIR/attention.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/attention.out" "graph_execution_phase: admission"
contains "$OUT_DIR/attention.out" "execution_ready: false"
contains "$OUT_DIR/attention.out" "attention_execution_supported: false"
contains "$OUT_DIR/attention.out" "generation_ready: false"
contains "$OUT_DIR/attention.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/attention.out" "status: graph-proof-retired"
contains "$OUT_DIR/attention.err" "production graph fixtures were retired to focused tests"

printf 'cli attention graph: retired proof refusal ok\n'
