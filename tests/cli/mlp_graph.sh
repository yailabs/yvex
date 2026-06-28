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
    file=$1
    text=$2
    grep -F -- "$text" "$file" >/dev/null || fail "$file missing: $text"
}

not_contains() {
    file=$1
    text=$2
    if grep -F -- "$text" "$file" >/dev/null; then
        fail "$file unexpectedly contained: $text"
    fi
}

run_ok() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" || fail "$name exited non-zero"
}

run_fail() {
    name=$1
    shift
    if "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err"; then
        fail "$name unexpectedly succeeded"
    fi
}

run_ok mlp_dense_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_dense_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/mlp_dense_cpu.out" "graph_execution_phase: complete"
contains "$OUT_DIR/mlp_dense_cpu.out" "graph_kind: mlp-feed-forward"
contains "$OUT_DIR/mlp_dense_cpu.out" "op: mlp"
contains "$OUT_DIR/mlp_dense_cpu.out" "backend: cpu"
contains "$OUT_DIR/mlp_dense_cpu.out" "dtype: f32"
contains "$OUT_DIR/mlp_dense_cpu.out" "batch: 1"
contains "$OUT_DIR/mlp_dense_cpu.out" "hidden_dim: 8"
contains "$OUT_DIR/mlp_dense_cpu.out" "ffn_dim: 16"
contains "$OUT_DIR/mlp_dense_cpu.out" "activation: silu"
contains "$OUT_DIR/mlp_dense_cpu.out" "gated: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "routed_expert_mode: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "expert_count: 0"
contains "$OUT_DIR/mlp_dense_cpu.out" "expert_id: 0"
contains "$OUT_DIR/mlp_dense_cpu.out" "backend_status: ready"
contains "$OUT_DIR/mlp_dense_cpu.out" "backend_op_status: supported"
contains "$OUT_DIR/mlp_dense_cpu.out" "dispatch_attempted: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "reference_read_attempted: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "reference_attempted: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "output_allocation_attempted: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "cleanup_status: not-needed"
contains "$OUT_DIR/mlp_dense_cpu.out" "input_bytes: 32"
contains "$OUT_DIR/mlp_dense_cpu.out" "gate_weight_bytes: 512"
contains "$OUT_DIR/mlp_dense_cpu.out" "up_weight_bytes: 512"
contains "$OUT_DIR/mlp_dense_cpu.out" "down_weight_bytes: 512"
contains "$OUT_DIR/mlp_dense_cpu.out" "intermediate_bytes: 64"
contains "$OUT_DIR/mlp_dense_cpu.out" "output_bytes: 32"
contains "$OUT_DIR/mlp_dense_cpu.out" "reference_bytes: 32"
contains "$OUT_DIR/mlp_dense_cpu.out" "max_abs_diff: 0"
contains "$OUT_DIR/mlp_dense_cpu.out" "cpu_reference_max_abs_diff: 0"
contains "$OUT_DIR/mlp_dense_cpu.out" "mlp_primitive_executed: true"
contains "$OUT_DIR/mlp_dense_cpu.out" "router_logits_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "top_k_routing_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "transformer_block_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "full_prefill_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "decode_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "logits_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "generation_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "generation: unsupported"
contains "$OUT_DIR/mlp_dense_cpu.out" "execution_ready: false"
contains "$OUT_DIR/mlp_dense_cpu.out" "status: graph-op-executed"

run_ok mlp_routed_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1
contains "$OUT_DIR/mlp_routed_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/mlp_routed_cpu.out" "graph_kind: mlp-routed-expert"
contains "$OUT_DIR/mlp_routed_cpu.out" "routed_expert_mode: true"
contains "$OUT_DIR/mlp_routed_cpu.out" "expert_count: 2"
contains "$OUT_DIR/mlp_routed_cpu.out" "expert_id: 1"
contains "$OUT_DIR/mlp_routed_cpu.out" "gate_weight_bytes: 1024"
contains "$OUT_DIR/mlp_routed_cpu.out" "up_weight_bytes: 1024"
contains "$OUT_DIR/mlp_routed_cpu.out" "down_weight_bytes: 1024"
contains "$OUT_DIR/mlp_routed_cpu.out" "max_abs_diff: 0"
contains "$OUT_DIR/mlp_routed_cpu.out" "status: graph-op-executed"
not_contains "$OUT_DIR/mlp_routed_cpu.out" "top_k_routing_ready: true"

run_fail mlp_zero_hidden \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 0 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_zero_hidden.err" "--hidden-dim requires a positive integer"

run_fail mlp_zero_ffn \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 0 --activation silu --gated
contains "$OUT_DIR/mlp_zero_ffn.err" "--ffn-dim requires a positive integer"

run_fail mlp_unsupported_activation \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation relu --gated
contains "$OUT_DIR/mlp_unsupported_activation.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/mlp_unsupported_activation.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_unsupported_activation.out" "reason: unsupported-activation"
contains "$OUT_DIR/mlp_unsupported_activation.out" "status: graph-op-fail"

run_fail mlp_missing_expert_id \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2
contains "$OUT_DIR/mlp_missing_expert_id.err" "--experts and --expert-id must be provided together"

run_fail mlp_expert_out_of_range \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 2
contains "$OUT_DIR/mlp_expert_out_of_range.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/mlp_expert_out_of_range.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_expert_out_of_range.out" "reason: expert-id-out-of-range"
contains "$OUT_DIR/mlp_expert_out_of_range.out" "status: graph-op-fail"

run_fail mlp_byte_overflow \
  env YVEX_TEST_MLP_BYTE_OVERFLOW=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_byte_overflow.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/mlp_byte_overflow.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_byte_overflow.out" "output_allocation_attempted: false"
contains "$OUT_DIR/mlp_byte_overflow.out" "reason: byte-count-overflow"
contains "$OUT_DIR/mlp_byte_overflow.out" "status: graph-op-fail"

run_fail mlp_backend_op_unsupported \
  env YVEX_TEST_MLP_BACKEND_OP_UNSUPPORTED=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_backend_op_unsupported.out" "backend_op_status: unsupported"
contains "$OUT_DIR/mlp_backend_op_unsupported.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_backend_op_unsupported.out" "reason: backend-op-mlp-unsupported"
contains "$OUT_DIR/mlp_backend_op_unsupported.out" "status: graph-op-fail"

run_fail mlp_after_gate_alloc \
  env YVEX_TEST_FAIL_MLP_AFTER_GATE_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_after_gate_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/mlp_after_gate_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_after_gate_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/mlp_after_gate_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/mlp_after_gate_alloc.out" "status: graph-op-failed-cleaned"

run_fail mlp_after_intermediate_alloc \
  env YVEX_TEST_FAIL_MLP_AFTER_INTERMEDIATE_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_after_intermediate_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/mlp_after_intermediate_alloc.out" "output_allocation_attempted: false"
contains "$OUT_DIR/mlp_after_intermediate_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/mlp_after_intermediate_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/mlp_after_intermediate_alloc.out" "status: graph-op-failed-cleaned"

run_fail mlp_after_output_alloc \
  env YVEX_TEST_FAIL_MLP_AFTER_OUTPUT_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_after_output_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/mlp_after_output_alloc.out" "output_allocation_attempted: true"
contains "$OUT_DIR/mlp_after_output_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/mlp_after_output_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/mlp_after_output_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/mlp_after_output_alloc.out" "status: graph-op-failed-cleaned"

run_ok mlp_repeat_after_output_alloc \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_repeat_after_output_alloc.out" "status: graph-op-executed"

run_fail mlp_after_dispatch \
  env YVEX_TEST_FAIL_MLP_AFTER_DISPATCH=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_after_dispatch.out" "graph_execution_phase: dispatch"
contains "$OUT_DIR/mlp_after_dispatch.out" "dispatch_attempted: true"
contains "$OUT_DIR/mlp_after_dispatch.out" "cleanup_attempted: true"
contains "$OUT_DIR/mlp_after_dispatch.out" "cleanup_status: pass"
contains "$OUT_DIR/mlp_after_dispatch.out" "status: graph-op-failed-cleaned"

run_fail mlp_after_reference \
  env YVEX_TEST_FAIL_MLP_AFTER_REFERENCE=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_after_reference.out" "graph_execution_phase: reference"
contains "$OUT_DIR/mlp_after_reference.out" "reference_read_attempted: true"
contains "$OUT_DIR/mlp_after_reference.out" "cleanup_attempted: true"
contains "$OUT_DIR/mlp_after_reference.out" "cleanup_status: pass"
contains "$OUT_DIR/mlp_after_reference.out" "status: graph-op-failed-cleaned"

run_ok mlp_repeat_after_failure \
  "$YVEX_BIN" graph --backend cpu --execute-op --op mlp \
    --hidden-dim 8 --ffn-dim 16 --activation silu --gated
contains "$OUT_DIR/mlp_repeat_after_failure.out" "status: graph-op-executed"
not_contains "$OUT_DIR/mlp_repeat_after_failure.out" "transformer_block_ready: true"

printf 'cli mlp graph: ok\n'
