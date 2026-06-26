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
  >"$OUT_DIR/graph-token-0.out" 2>"$OUT_DIR/graph-token-0.err"
contains "$OUT_DIR/graph-token-0.out" "fixture_graph_executed: true"
contains "$OUT_DIR/graph-token-0.out" "fixture_backend: cpu"
contains "$OUT_DIR/graph-token-0.out" "fixture_op: embed"
contains "$OUT_DIR/graph-token-0.out" "fixture_weight: token_embd.weight"
contains "$OUT_DIR/graph-token-0.out" "fixture_token_id: 0"
contains "$OUT_DIR/graph-token-0.out" "fixture_node_count: 1"
contains "$OUT_DIR/graph-token-0.out" "fixture_output_count: 4"
contains "$OUT_DIR/graph-token-0.out" "fixture_output_bytes: 16"
contains "$OUT_DIR/graph-token-0.out" "fixture_output_values: 0,4,8,12"
contains "$OUT_DIR/graph-token-0.out" "execution_ready: false"
contains "$OUT_DIR/graph-token-0.out" "graph_execution_ready: false"
contains "$OUT_DIR/graph-token-0.out" "status: fixture-graph-executed"

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-fixture --fixture-token 1 \
  >"$OUT_DIR/graph-token-1.out" 2>"$OUT_DIR/graph-token-1.err"
contains "$OUT_DIR/graph-token-1.out" "fixture_token_id: 1"
contains "$OUT_DIR/graph-token-1.out" "fixture_output_values: 16,20,24,28"

if cmp -s "$OUT_DIR/graph-token-0.out" "$OUT_DIR/graph-token-1.out"; then
    fail "token 0 and token 1 fixture outputs should differ"
fi

"$YVEX_BIN" models add \
  --path "$MODEL" \
  --support-level selected-tensor-materialized \
  --registry "$REG" >/dev/null

YVEX_MODELS_REGISTRY="$REG" \
  "$YVEX_BIN" graph --model "$ALIAS" --backend cpu --execute-fixture --fixture-token 0 \
  >"$OUT_DIR/graph-alias.out" 2>"$OUT_DIR/graph-alias.err"
contains "$OUT_DIR/graph-alias.out" "fixture_graph_executed: true"
contains "$OUT_DIR/graph-alias.out" "fixture_backend: cpu"
contains "$OUT_DIR/graph-alias.out" "fixture_output_values: 0,4,8,12"

for i in 1 2 3; do
  "$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-fixture --fixture-token 0 \
    >"$OUT_DIR/graph-repeat-$i.out" 2>"$OUT_DIR/graph-repeat-$i.err"
  contains "$OUT_DIR/graph-repeat-$i.out" "status: fixture-graph-executed"
done

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-fixture --fixture-token 99 \
  >"$OUT_DIR/graph-bad-token.out" 2>"$OUT_DIR/graph-bad-token.err" && exit 1 || true
contains "$OUT_DIR/graph-bad-token.err" "fixture token id 99 exceeds embedding vocab size 8"

"$YVEX_BIN" graph --model "$MODEL" --backend missing --execute-fixture \
  >"$OUT_DIR/graph-bad-backend.out" 2>"$OUT_DIR/graph-bad-backend.err" && exit 1 || true
contains "$OUT_DIR/graph-bad-backend.err" "unknown backend kind: missing"

"$YVEX_BIN" graph --model "$OUT_DIR/missing.gguf" --backend cpu --execute-fixture \
  >"$OUT_DIR/graph-missing.out" 2>"$OUT_DIR/graph-missing.err" && exit 1 || true
contains "$OUT_DIR/graph-missing.err" "failed to open"

echo "cli fixture graph: ok"
