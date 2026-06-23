#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./build/bin/yvex}"
ROOT="build/tests/model-aliases"
REG="$ROOT/models.local.json"
MODEL="$ROOT/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
ALIAS="deepseek4-v4-flash-selected-embed"

rm -rf "$ROOT"
mkdir -p "$ROOT"

"$YVEX_BIN" gguf-emit controlled \
  --out "$MODEL" \
  --model-name alias-test \
  --arch llama \
  --overwrite >/dev/null

"$YVEX_BIN" models add \
  --path "$MODEL" \
  --support-level selected-tensor-materialized \
  --registry "$REG" >/dev/null

"$YVEX_BIN" models use "$ALIAS" --registry "$REG" >/dev/null

export YVEX_MODELS_REGISTRY="$REG"

"$YVEX_BIN" inspect "$ALIAS" > "$ROOT/inspect.out"
grep 'format: gguf' "$ROOT/inspect.out"
grep 'tensor_count: 1' "$ROOT/inspect.out"

"$YVEX_BIN" metadata "$ALIAS" > "$ROOT/metadata.out"
grep 'format: gguf' "$ROOT/metadata.out"

"$YVEX_BIN" tensors "$ALIAS" > "$ROOT/tensors.out"
grep 'token_embd.weight' "$ROOT/tensors.out"

"$YVEX_BIN" engine "$ALIAS" > "$ROOT/engine.out"
grep 'status: engine-descriptor' "$ROOT/engine.out"

"$YVEX_BIN" graph "$ALIAS" > "$ROOT/graph.out"
grep 'status: graph-partial' "$ROOT/graph.out"

"$YVEX_BIN" plan "$ALIAS" --backend cpu > "$ROOT/plan.out"
grep 'status: plan-only' "$ROOT/plan.out"

"$YVEX_BIN" materialize --model "$ALIAS" --backend cpu > "$ROOT/materialize.out"
grep 'status: weights-materialized' "$ROOT/materialize.out"

"$YVEX_BIN" model-gate check \
  --model "$ALIAS" \
  --label alias-selected \
  --family deepseek4 \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4,8 \
  --expect-dtype F32 \
  --expect-bytes 128 \
  --backend cpu \
  --require-cpu \
  --report-out "$ROOT/model-gate.txt" >/dev/null
grep 'status: model-gate-pass' "$ROOT/model-gate.txt"

"$YVEX_BIN" materialize-gate check \
  --model "$ALIAS" \
  --label alias-selected \
  --family deepseek4 \
  --scope selected-tensor \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4,8 \
  --expect-dtype F32 \
  --expect-bytes 128 \
  --backend cpu \
  --require-cpu \
  --repeat 2 \
  --check-cleanup \
  --report-out "$ROOT/materialize-gate.txt" >/dev/null
grep 'status: materialize-gate-pass' "$ROOT/materialize-gate.txt"

"$YVEX_BIN" inspect missing-alias > "$ROOT/missing.out" 2> "$ROOT/missing.err" && exit 1 || true
grep 'model reference not found' "$ROOT/missing.err"
grep 'models list' "$ROOT/missing.err"
grep "$ALIAS" "$ROOT/missing.err"
