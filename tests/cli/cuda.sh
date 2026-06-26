#!/bin/sh
set -u

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-${OUT_DIR:-build/tests/cli/cuda}}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf
CONTROLLED="$OUT_DIR/cuda-controlled.gguf"
PARTIAL="$OUT_DIR/cuda-controlled-f16.gguf"

mkdir -p "$OUT_DIR"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

"$YVEX_BIN" cuda-info >"$OUT_DIR/cuda_info.out" 2>"$OUT_DIR/cuda_info.err"
rc=$?
if [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/cuda_info.out" "cuda: unavailable"
    contains "$OUT_DIR/cuda_info.out" "status: cuda-unavailable"
    echo "cli cuda smoke: skip"
    exit 77
fi
if [ "$rc" -ne 0 ]; then
    fail "cuda-info exit code was $rc"
fi
contains "$OUT_DIR/cuda_info.out" "cuda: available"
contains "$OUT_DIR/cuda_info.out" "status: cuda-info"

"$YVEX_BIN" backend cuda >"$OUT_DIR/backend_cuda_ready.out" 2>"$OUT_DIR/backend_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "backend cuda exit code was $rc"
fi
contains "$OUT_DIR/backend_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/backend_cuda_ready.out" "status: ready"
contains "$OUT_DIR/backend_cuda_ready.out" "tensor_alloc: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "tensor_read_write: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_embed: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_matmul: no"
contains "$OUT_DIR/backend_cuda_ready.out" "status: backend-ready"

"$YVEX_BIN" plan "$FIXTURE" --backend cuda >"$OUT_DIR/plan_cuda_ready.out" 2>"$OUT_DIR/plan_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "plan cuda exit code was $rc"
fi
contains "$OUT_DIR/plan_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/plan_cuda_ready.out" "backend_status: available"
contains "$OUT_DIR/plan_cuda_ready.out" "op_embed: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "execution_ready: false"
contains "$OUT_DIR/plan_cuda_ready.out" "status: plan-only"

"$YVEX_BIN" materialize --model "$FIXTURE" --backend cuda >"$OUT_DIR/materialize_cuda_ready.out" 2>"$OUT_DIR/materialize_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "materialize cuda exit code was $rc"
fi
contains "$OUT_DIR/materialize_cuda_ready.out" "materialization status: materialized"
contains "$OUT_DIR/materialize_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/materialize_cuda_ready.out" "tensors_materialized: 1"
contains "$OUT_DIR/materialize_cuda_ready.out" "bytes_materialized: 128"
contains "$OUT_DIR/materialize_cuda_ready.out" "execution_ready: false"
contains "$OUT_DIR/materialize_cuda_ready.out" "status: weights-materialized"

"$YVEX_BIN" engine --model "$FIXTURE" --backend cuda >"$OUT_DIR/engine_cuda_ready.out" 2>"$OUT_DIR/engine_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "engine cuda exit code was $rc"
fi
contains "$OUT_DIR/engine_cuda_ready.out" "weights_attached: true"
contains "$OUT_DIR/engine_cuda_ready.out" "weights_backend: cuda"
contains "$OUT_DIR/engine_cuda_ready.out" "weight_tensor_count: 1"
contains "$OUT_DIR/engine_cuda_ready.out" "weight_total_bytes: 128"
contains "$OUT_DIR/engine_cuda_ready.out" "graph_execution_ready: false"
contains "$OUT_DIR/engine_cuda_ready.out" "status: engine-weights-attached"

"$YVEX_BIN" session "$FIXTURE" --backend cuda >"$OUT_DIR/session_cuda_ready.out" 2>"$OUT_DIR/session_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "session cuda exit code was $rc"
fi
contains "$OUT_DIR/session_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/session_cuda_ready.out" "weights_attached: true"
contains "$OUT_DIR/session_cuda_ready.out" "weights_backend: cuda"
contains "$OUT_DIR/session_cuda_ready.out" "weight_tensor_count: 1"
contains "$OUT_DIR/session_cuda_ready.out" "execution_ready: false"

"$YVEX_BIN" gguf-emit controlled \
  --out "$CONTROLLED" \
  --model-name cuda-fixture-graph \
  --arch deepseek \
  --overwrite >"$OUT_DIR/emit_controlled.out" 2>"$OUT_DIR/emit_controlled.err"

"$YVEX_BIN" graph --model "$CONTROLLED" --backend cuda --execute-fixture --fixture-token 0 \
  >"$OUT_DIR/fixture_graph_cuda.out" 2>"$OUT_DIR/fixture_graph_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "fixture graph cuda exit code was $rc"
fi
contains "$OUT_DIR/fixture_graph_cuda.out" "fixture_graph_executed: true"
contains "$OUT_DIR/fixture_graph_cuda.out" "fixture_backend: cuda"
contains "$OUT_DIR/fixture_graph_cuda.out" "fixture_output_values: 0,4,8,12"
contains "$OUT_DIR/fixture_graph_cuda.out" "execution_ready: false"
contains "$OUT_DIR/fixture_graph_cuda.out" "graph_execution_ready: false"
contains "$OUT_DIR/fixture_graph_cuda.out" "status: fixture-graph-executed"

"$YVEX_BIN" gguf-emit controlled \
  --out "$PARTIAL" \
  --model-name cuda-partial-graph \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit_partial.out" 2>"$OUT_DIR/emit_partial.err"

"$YVEX_BIN" graph --model "$PARTIAL" --backend cuda --execute-partial --partial-token 0 \
  >"$OUT_DIR/partial_graph_cuda.out" 2>"$OUT_DIR/partial_graph_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "partial graph cuda exit code was $rc"
fi
contains "$OUT_DIR/partial_graph_cuda.out" "real_partial_graph_executed: true"
contains "$OUT_DIR/partial_graph_cuda.out" "partial_backend: cuda"
contains "$OUT_DIR/partial_graph_cuda.out" "partial_weight_dtype: F16"
contains "$OUT_DIR/partial_graph_cuda.out" "partial_output_sample_values: 0,4,8,12"
contains "$OUT_DIR/partial_graph_cuda.out" "partial_max_abs_diff: 0"
contains "$OUT_DIR/partial_graph_cuda.out" "execution_ready: false"
contains "$OUT_DIR/partial_graph_cuda.out" "graph_execution_ready: false"
contains "$OUT_DIR/partial_graph_cuda.out" "status: real-partial-graph-executed"

"$YVEX_BIN" help cuda-info >"$OUT_DIR/help_cuda_info.out" 2>"$OUT_DIR/help_cuda_info.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "help cuda-info exit code was $rc"
fi
contains "$OUT_DIR/help_cuda_info.out" "usage: yvex cuda-info"

echo "cli cuda smoke: ok"
