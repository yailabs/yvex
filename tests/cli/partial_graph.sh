#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/partial-graph}
MODEL="$OUT_DIR/deepseek-test-partial-embed-F16-noimatrix-yvex-v1.gguf"
F32_MODEL="$OUT_DIR/deepseek-test-partial-embed-F32-noimatrix-yvex-v1.gguf"
REG="$OUT_DIR/models.local.json"
ALIAS="deepseek4-v4-flash-partial-embed"

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
  --model-name m5-controlled-f16 \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit-f16.out" 2>"$OUT_DIR/emit-f16.err"

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial-token-0.out" 2>"$OUT_DIR/partial-token-0.err"
contains "$OUT_DIR/partial-token-0.out" "real_partial_graph_executed: true"
contains "$OUT_DIR/partial-token-0.out" "partial_graph_kind: token-embedding"
contains "$OUT_DIR/partial-token-0.out" "partial_backend: cpu"
contains "$OUT_DIR/partial-token-0.out" "partial_weight: token_embd.weight"
contains "$OUT_DIR/partial-token-0.out" "partial_weight_dtype: F16"
contains "$OUT_DIR/partial-token-0.out" "partial_token: 0"
contains "$OUT_DIR/partial-token-0.out" "partial_node_count: 1"
contains "$OUT_DIR/partial-token-0.out" "partial_output_dtype: F32"
contains "$OUT_DIR/partial-token-0.out" "partial_output_count: 4"
contains "$OUT_DIR/partial-token-0.out" "partial_output_bytes: 16"
contains "$OUT_DIR/partial-token-0.out" "partial_reference_checksum: 10881421815077182739"
contains "$OUT_DIR/partial-token-0.out" "partial_max_abs_diff: 0"
contains "$OUT_DIR/partial-token-0.out" "partial_output_sample_values: 0,4,8,12"
contains "$OUT_DIR/partial-token-0.out" "execution_ready: false"
contains "$OUT_DIR/partial-token-0.out" "graph_execution_ready: false"
contains "$OUT_DIR/partial-token-0.out" "prefill_ready: false"
contains "$OUT_DIR/partial-token-0.out" "logits_ready: false"
contains "$OUT_DIR/partial-token-0.out" "generation: unsupported"
contains "$OUT_DIR/partial-token-0.out" "status: real-partial-graph-executed"

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-partial --partial-token 1 \
  >"$OUT_DIR/partial-token-1.out" 2>"$OUT_DIR/partial-token-1.err"
contains "$OUT_DIR/partial-token-1.out" "partial_token: 1"
contains "$OUT_DIR/partial-token-1.out" "partial_output_sample_values: 16,20,24,28"

if cmp -s "$OUT_DIR/partial-token-0.out" "$OUT_DIR/partial-token-1.out"; then
    fail "token 0 and token 1 partial outputs should differ"
fi

"$YVEX_BIN" models add \
  --path "$MODEL" \
  --alias "$ALIAS" \
  --support-level selected-tensor-materialized \
  --registry "$REG" >/dev/null

YVEX_MODELS_REGISTRY="$REG" \
  "$YVEX_BIN" graph --model "$ALIAS" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial-alias.out" 2>"$OUT_DIR/partial-alias.err"
contains "$OUT_DIR/partial-alias.out" "real_partial_graph_executed: true"
contains "$OUT_DIR/partial-alias.out" "partial_backend: cpu"
contains "$OUT_DIR/partial-alias.out" "partial_output_sample_values: 0,4,8,12"

for i in 1 2 3; do
  "$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-partial --partial-token 0 \
    >"$OUT_DIR/partial-repeat-$i.out" 2>"$OUT_DIR/partial-repeat-$i.err"
  contains "$OUT_DIR/partial-repeat-$i.out" "status: real-partial-graph-executed"
  contains "$OUT_DIR/partial-repeat-$i.out" "partial_output_checksum: 10881421815077182739"
done

"$YVEX_BIN" graph --model "$MODEL" --backend cpu --execute-partial --partial-token 99 \
  >"$OUT_DIR/partial-bad-token.out" 2>"$OUT_DIR/partial-bad-token.err" && exit 1 || true
contains "$OUT_DIR/partial-bad-token.err" "partial token out of range"

"$YVEX_BIN" gguf-emit controlled \
  --out "$F32_MODEL" \
  --model-name m5-controlled-f32-incompatible \
  --arch deepseek \
  --overwrite >"$OUT_DIR/emit-f32.out" 2>"$OUT_DIR/emit-f32.err"
"$YVEX_BIN" graph --model "$F32_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial-f32.out" 2>"$OUT_DIR/partial-f32.err" && exit 1 || true
contains "$OUT_DIR/partial-f32.err" "real partial embedding segment requires F16 token_embd.weight"

"$YVEX_BIN" graph --model tests/fixtures/gguf/valid-minimal.gguf --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial-missing-tensor.out" 2>"$OUT_DIR/partial-missing-tensor.err" && exit 1 || true
contains "$OUT_DIR/partial-missing-tensor.err" "required tensor not found: token_embd.weight"

"$YVEX_BIN" graph --model "$MODEL" --backend missing --execute-partial \
  >"$OUT_DIR/partial-bad-backend.out" 2>"$OUT_DIR/partial-bad-backend.err" && exit 1 || true
contains "$OUT_DIR/partial-bad-backend.err" "unknown backend kind: missing"

echo "cli partial graph: ok"
