#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/fixture-graph}
MODEL="$OUT_DIR/deepseek4-v4-flash-selected-embed-F32-noimatrix-yvex-v1.gguf"
REG="$OUT_DIR/models.local.json"
ALIAS="deepseek4-v4-flash-selected-embed"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" gguf-emit controlled \
  --out "$MODEL" \
  --model-name m4-controlled \
  --arch deepseek \
  --overwrite >"$OUT_DIR/emit.out" 2>"$OUT_DIR/emit.err"

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-fixture --fixture-token 0 \
  >"$OUT_DIR/graph-token-0.out" 2>"$OUT_DIR/graph-token-0.err" && \
  fail "retired production fixture unexpectedly executed" || true
contains "$OUT_DIR/graph-token-0.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/graph-token-0.out" "graph_execution_phase: admission"
contains "$OUT_DIR/graph-token-0.out" "execution_ready: false"
contains "$OUT_DIR/graph-token-0.out" "attention_execution_supported: false"
contains "$OUT_DIR/graph-token-0.out" "generation_ready: false"
contains "$OUT_DIR/graph-token-0.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/graph-token-0.out" "status: graph-proof-retired"
contains "$OUT_DIR/graph-token-0.err" "production graph fixtures were retired to focused tests"

"$YVEX_BIN" models add \
  --path "$MODEL" \
  --support-level selected-tensor-materialized \
  --registry "$REG" >/dev/null

YVEX_MODELS_REGISTRY="$REG" \
  "$YVEX_BIN" graph --model "$ALIAS" --backend cpu --execute-fixture --fixture-token 0 \
  >"$OUT_DIR/graph-alias.out" 2>"$OUT_DIR/graph-alias.err" && \
  fail "retired alias fixture unexpectedly executed" || true
contains "$OUT_DIR/graph-alias.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/graph-alias.out" "status: graph-proof-retired"

echo "cli fixture graph: ok"
