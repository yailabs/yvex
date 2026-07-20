#!/bin/sh
set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-graph-integrity-guard}
F32_MODEL="$OUT_DIR/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"
F16_MODEL="$OUT_DIR/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
REG="$OUT_DIR/models.local.json"
DRIFT_REG="$OUT_DIR/drift.models.local.json"
STALE_REG="$OUT_DIR/stale.models.local.json"
ALIAS=deepseek4-v4-flash-selected-embed

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

not_contains() {
    file=$1
    text=$2
    if grep -F "$text" "$file" >/dev/null; then
        fail "$file unexpectedly contained: $text"
    fi
}

mutate_registry() {
    src=$1
    dst=$2
    key=$3
    value=$4
    python3 - "$src" "$dst" "$key" "$value" <<'PY'
import json
import sys

src, dst, key, value = sys.argv[1:]
with open(src, "r", encoding="utf-8") as fh:
    data = json.load(fh)
data["models"][0][key] = value
with open(dst, "w", encoding="utf-8") as fh:
    json.dump(data, fh, indent=2)
    fh.write("\n")
PY
}

mutate_file_byte() {
    path=$1
    python3 - "$path" <<'PY'
import sys

path = sys.argv[1]
with open(path, "r+b") as fh:
    fh.seek(-1, 2)
    b = fh.read(1)
    fh.seek(-1, 2)
    fh.write(bytes([b[0] ^ 1]))
PY
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" gguf-emit controlled \
  --out "$F32_MODEL" \
  --model-name graph-guard-f32 \
  --arch deepseek \
  --overwrite >"$OUT_DIR/emit-f32.out" 2>"$OUT_DIR/emit-f32.err"

"$YVEX_BIN" gguf-emit controlled \
  --out "$F16_MODEL" \
  --model-name graph-guard-f16 \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit-f16.out" 2>"$OUT_DIR/emit-f16.err"

"$YVEX_BIN" graph --model "$F32_MODEL" --backend cpu --execute-fixture --fixture-token 0 \
  >"$OUT_DIR/fixture-retired.out" 2>"$OUT_DIR/fixture-retired.err" && \
  fail "retired production fixture graph unexpectedly passed" || true
contains "$OUT_DIR/fixture-retired.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/fixture-retired.out" "graph_execution_phase: admission"
contains "$OUT_DIR/fixture-retired.out" "execution_ready: false"
contains "$OUT_DIR/fixture-retired.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/fixture-retired.out" "status: graph-proof-retired"

"$YVEX_BIN" graph --model "$F16_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial-pass.out" 2>"$OUT_DIR/partial-pass.err"
contains "$OUT_DIR/partial-pass.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/partial-pass.out" "graph_kind: selected-embedding-partial"
contains "$OUT_DIR/partial-pass.out" "slice_range_status: pass"
contains "$OUT_DIR/partial-pass.out" "reference_read_attempted: true"
contains "$OUT_DIR/partial-pass.out" "real_partial_graph_executed: true"
contains "$OUT_DIR/partial-pass.out" "status: real-partial-graph-executed"

"$YVEX_BIN" graph --model tests/fixtures/gguf/bad-magic.gguf --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/bad-magic.out" 2>"$OUT_DIR/bad-magic.err" && \
  fail "bad magic graph unexpectedly passed" || true
contains "$OUT_DIR/bad-magic.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/bad-magic.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/bad-magic.out" "integrity_status: fail"
contains "$OUT_DIR/bad-magic.out" "dispatch_attempted: false"
contains "$OUT_DIR/bad-magic.out" "output_allocation_attempted: false"
contains "$OUT_DIR/bad-magic.out" "status: graph-integrity-fail"

"$YVEX_BIN" graph --model tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/range-bad.out" 2>"$OUT_DIR/range-bad.err" && \
  fail "range-bad graph unexpectedly passed" || true
contains "$OUT_DIR/range-bad.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/range-bad.out" "dispatch_attempted: false"
contains "$OUT_DIR/range-bad.out" "status: graph-integrity-fail"

"$YVEX_BIN" graph --model "$F32_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/dtype-bad.out" 2>"$OUT_DIR/dtype-bad.err" && \
  fail "dtype-bad partial graph unexpectedly passed" || true
contains "$OUT_DIR/dtype-bad.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/dtype-bad.out" "shape_status: fail"
contains "$OUT_DIR/dtype-bad.out" "dispatch_attempted: false"
contains "$OUT_DIR/dtype-bad.out" "status: graph-integrity-fail"

"$YVEX_BIN" graph --model "$F16_MODEL" --backend cpu --execute-partial --partial-token 99 \
  >"$OUT_DIR/token-bad.out" 2>"$OUT_DIR/token-bad.err" && \
  fail "token-bad partial graph unexpectedly passed" || true
contains "$OUT_DIR/token-bad.out" "slice_range_status: fail"
contains "$OUT_DIR/token-bad.out" "dispatch_attempted: false"
contains "$OUT_DIR/token-bad.out" "status: graph-integrity-fail"

"$YVEX_BIN" models add \
  --path "$F16_MODEL" \
  --alias "$ALIAS" \
  --support-level selected-tensor-materialized \
  --registry "$REG" >"$OUT_DIR/models-add.out" 2>"$OUT_DIR/models-add.err"

cp "$REG" "$STALE_REG"
mutate_file_byte "$F16_MODEL"
YVEX_MODELS_REGISTRY="$STALE_REG" "$YVEX_BIN" graph \
  --model "$ALIAS" \
  --backend cpu \
  --execute-partial \
  --partial-token 0 \
  >"$OUT_DIR/stale-alias.out" 2>"$OUT_DIR/stale-alias.err" && \
  fail "stale alias graph unexpectedly passed" || true
contains "$OUT_DIR/stale-alias.out" "identity_status: fail"
contains "$OUT_DIR/stale-alias.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/stale-alias.out" "dispatch_attempted: false"
contains "$OUT_DIR/stale-alias.out" "status: graph-integrity-fail"

"$YVEX_BIN" gguf-emit controlled \
  --out "$F16_MODEL" \
  --model-name graph-guard-f16 \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit-f16-reset.out" 2>"$OUT_DIR/emit-f16-reset.err"

mutate_registry "$REG" "$DRIFT_REG" "primary_tensor_dtype" "F32"
YVEX_MODELS_REGISTRY="$DRIFT_REG" "$YVEX_BIN" graph \
  --model "$ALIAS" \
  --backend cpu \
  --execute-partial \
  --partial-token 0 \
  >"$OUT_DIR/metadata-drift.out" 2>"$OUT_DIR/metadata-drift.err" && \
  fail "metadata drift graph unexpectedly passed" || true
contains "$OUT_DIR/metadata-drift.out" "metadata_status: fail"
contains "$OUT_DIR/metadata-drift.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/metadata-drift.out" "dispatch_attempted: false"
contains "$OUT_DIR/metadata-drift.out" "status: graph-integrity-fail"
not_contains "$OUT_DIR/metadata-drift.out" "real_partial_graph_executed: true"

YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED=1 "$YVEX_BIN" graph \
  --model "$F16_MODEL" \
  --backend cpu \
  --execute-partial \
  --partial-token 0 \
  >"$OUT_DIR/op-unsupported.out" 2>"$OUT_DIR/op-unsupported.err" && \
  fail "unsupported op graph unexpectedly passed" || true
contains "$OUT_DIR/op-unsupported.out" "backend_op_status: unsupported"
contains "$OUT_DIR/op-unsupported.out" "dispatch_attempted: false"
contains "$OUT_DIR/op-unsupported.out" "status: graph-integrity-fail"

YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC=1 "$YVEX_BIN" graph \
  --model "$F32_MODEL" \
  --backend cpu \
  --execute-fixture \
  --fixture-token 0 \
  >"$OUT_DIR/output-alloc-retired.out" 2>"$OUT_DIR/output-alloc-retired.err" && \
  fail "retired fixture graph fault path unexpectedly passed" || true
contains "$OUT_DIR/output-alloc-retired.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/output-alloc-retired.out" "graph_execution_phase: admission"
contains "$OUT_DIR/output-alloc-retired.out" "execution_ready: false"
contains "$OUT_DIR/output-alloc-retired.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/output-alloc-retired.out" "status: graph-proof-retired"

YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH=1 "$YVEX_BIN" graph \
  --model "$F16_MODEL" \
  --backend cpu \
  --execute-partial \
  --partial-token 0 \
  >"$OUT_DIR/dispatch-fail.out" 2>"$OUT_DIR/dispatch-fail.err" && \
  fail "dispatch injected graph unexpectedly passed" || true
contains "$OUT_DIR/dispatch-fail.out" "graph_execution_phase: dispatch"
contains "$OUT_DIR/dispatch-fail.out" "dispatch_attempted: true"
contains "$OUT_DIR/dispatch-fail.out" "reference_read_attempted: true"
contains "$OUT_DIR/dispatch-fail.out" "output_allocation_attempted: true"
contains "$OUT_DIR/dispatch-fail.out" "cleanup_attempted: true"
contains "$OUT_DIR/dispatch-fail.out" "cleanup_status: pass"
contains "$OUT_DIR/dispatch-fail.out" "status: graph-failed-cleaned"

"$YVEX_BIN" graph --model "$F16_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/repeat-after-failure.out" 2>"$OUT_DIR/repeat-after-failure.err"
contains "$OUT_DIR/repeat-after-failure.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/repeat-after-failure.out" "status: real-partial-graph-executed"

echo "cli graph integrity guard: ok"
