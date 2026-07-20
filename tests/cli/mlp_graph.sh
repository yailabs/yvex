#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/mlp-graph}
mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

if "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated \
    >"$OUT_DIR/mlp.out" 2>"$OUT_DIR/mlp.err"; then
    fail "retired production MLP proof unexpectedly executed"
fi
contains "$OUT_DIR/mlp.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/mlp.out" "graph_execution_phase: admission"
contains "$OUT_DIR/mlp.out" "execution_ready: false"
contains "$OUT_DIR/mlp.out" "attention_execution_supported: true"
contains "$OUT_DIR/mlp.out" "generation_ready: false"
contains "$OUT_DIR/mlp.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/mlp.out" "status: graph-proof-retired"
contains "$OUT_DIR/mlp.err" "production graph fixtures were retired to focused tests"

printf 'cli mlp graph: retired proof refusal ok\n'
