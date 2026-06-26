#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/model-aliases}
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

"$YVEX_BIN" models add \
  --path "$MODEL" \
  --alias controlled \
  --registry "$REG" > "$ROOT/bad-alias.out" 2> "$ROOT/bad-alias.err" && exit 1 || true
grep 'alias must include family, model, scope, and artifact class' "$ROOT/bad-alias.err"
grep 'deepseek4-v4-flash-selected-embed' "$ROOT/bad-alias.err"

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

"$YVEX_BIN" engine --model "$MODEL" --backend cpu > "$ROOT/engine-path-attach.out"
grep 'weights_attached: true' "$ROOT/engine-path-attach.out"
grep 'weights_backend: cpu' "$ROOT/engine-path-attach.out"
grep 'weight_tensor_count: 1' "$ROOT/engine-path-attach.out"
grep 'weight_total_bytes: 128' "$ROOT/engine-path-attach.out"
grep 'graph_execution_ready: false' "$ROOT/engine-path-attach.out"
grep 'status: engine-weights-attached' "$ROOT/engine-path-attach.out"

"$YVEX_BIN" engine --model "$ALIAS" --backend cpu > "$ROOT/engine-alias-attach.out"
grep 'weights_attached: true' "$ROOT/engine-alias-attach.out"
grep 'attached_weight_0: token_embd.weight' "$ROOT/engine-alias-attach.out"
grep 'execution_ready: false' "$ROOT/engine-alias-attach.out"

"$YVEX_BIN" session "$ALIAS" --backend cpu > "$ROOT/session-alias-attach.out"
grep 'weights_attached: true' "$ROOT/session-alias-attach.out"
grep 'weights_backend: cpu' "$ROOT/session-alias-attach.out"
grep 'weight_tensor_count: 1' "$ROOT/session-alias-attach.out"
grep 'execution_ready: false' "$ROOT/session-alias-attach.out"
grep 'graph_execution_ready: false' "$ROOT/session-alias-attach.out"

for i in 1 2 3; do
  "$YVEX_BIN" engine --model "$MODEL" --backend cpu > "$ROOT/engine-repeat-$i.out"
  grep 'status: engine-weights-attached' "$ROOT/engine-repeat-$i.out"
done

"$YVEX_BIN" engine --model "$MODEL" --backend missing > "$ROOT/engine-bad-backend.out" 2> "$ROOT/engine-bad-backend.err" && exit 1 || true
grep 'unknown backend kind: missing' "$ROOT/engine-bad-backend.err"

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

YVEX_MODELS_REGISTRY="$ROOT/no-registry/models.local.json" \
  "$YVEX_BIN" inspect "$ALIAS" > "$ROOT/no-registry.out" 2> "$ROOT/no-registry.err" && exit 1 || true
grep 'model registry unavailable' "$ROOT/no-registry.err"
grep 'YVEX_MODELS_REGISTRY=' "$ROOT/no-registry.err"
grep 'unset YVEX_MODELS_REGISTRY' "$ROOT/no-registry.err"
