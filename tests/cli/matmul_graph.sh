#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/matmul-graph}

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

run_ok matmul_projection_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_projection_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/matmul_projection_cpu.out" "graph_execution_phase: complete"
contains "$OUT_DIR/matmul_projection_cpu.out" "graph_kind: matmul-projection"
contains "$OUT_DIR/matmul_projection_cpu.out" "op: matmul"
contains "$OUT_DIR/matmul_projection_cpu.out" "backend: cpu"
contains "$OUT_DIR/matmul_projection_cpu.out" "dtype: f32"
contains "$OUT_DIR/matmul_projection_cpu.out" "m: 1"
contains "$OUT_DIR/matmul_projection_cpu.out" "k: 8"
contains "$OUT_DIR/matmul_projection_cpu.out" "n: 8"
contains "$OUT_DIR/matmul_projection_cpu.out" "projection_shape: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "non_projection_shape: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "backend_status: ready"
contains "$OUT_DIR/matmul_projection_cpu.out" "backend_op_status: supported"
contains "$OUT_DIR/matmul_projection_cpu.out" "dispatch_attempted: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "reference_read_attempted: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "reference_attempted: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "output_allocation_attempted: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "cleanup_status: not-needed"
contains "$OUT_DIR/matmul_projection_cpu.out" "input_bytes: 32"
contains "$OUT_DIR/matmul_projection_cpu.out" "weight_bytes: 256"
contains "$OUT_DIR/matmul_projection_cpu.out" "output_bytes: 32"
contains "$OUT_DIR/matmul_projection_cpu.out" "input_total_bytes: 288"
contains "$OUT_DIR/matmul_projection_cpu.out" "max_abs_diff: 0"
contains "$OUT_DIR/matmul_projection_cpu.out" "cpu_reference_max_abs_diff: 0"
contains "$OUT_DIR/matmul_projection_cpu.out" "matmul_primitive_executed: true"
contains "$OUT_DIR/matmul_projection_cpu.out" "qkv_projection_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "transformer_block_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "full_prefill_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "decode_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "logits_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "generation_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "generation: unsupported"
contains "$OUT_DIR/matmul_projection_cpu.out" "execution_ready: false"
contains "$OUT_DIR/matmul_projection_cpu.out" "status: graph-op-executed"

run_ok matmul_matrix_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 2 --k 4 --n 3
contains "$OUT_DIR/matmul_matrix_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/matmul_matrix_cpu.out" "graph_kind: matmul-matrix"
contains "$OUT_DIR/matmul_matrix_cpu.out" "projection_shape: false"
contains "$OUT_DIR/matmul_matrix_cpu.out" "non_projection_shape: true"
contains "$OUT_DIR/matmul_matrix_cpu.out" "input_bytes: 32"
contains "$OUT_DIR/matmul_matrix_cpu.out" "weight_bytes: 48"
contains "$OUT_DIR/matmul_matrix_cpu.out" "output_bytes: 24"
contains "$OUT_DIR/matmul_matrix_cpu.out" "input_total_bytes: 80"
contains "$OUT_DIR/matmul_matrix_cpu.out" "max_abs_diff: 0"
contains "$OUT_DIR/matmul_matrix_cpu.out" "status: graph-op-executed"

run_fail matmul_zero_m \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 0 --k 8 --n 8
contains "$OUT_DIR/matmul_zero_m.err" "--m requires a positive integer"

run_fail matmul_zero_k \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 0 --n 8
contains "$OUT_DIR/matmul_zero_k.err" "--k requires a positive integer"

run_fail matmul_zero_n \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 0
contains "$OUT_DIR/matmul_zero_n.err" "--n requires a positive integer"

run_fail matmul_byte_overflow \
  env YVEX_TEST_MATMUL_BYTE_OVERFLOW=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_byte_overflow.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/matmul_byte_overflow.out" "dispatch_attempted: false"
contains "$OUT_DIR/matmul_byte_overflow.out" "output_allocation_attempted: false"
contains "$OUT_DIR/matmul_byte_overflow.out" "reason: byte-count-overflow"
contains "$OUT_DIR/matmul_byte_overflow.out" "status: graph-op-fail"

run_fail matmul_backend_op_unsupported \
  env YVEX_TEST_MATMUL_BACKEND_OP_UNSUPPORTED=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_backend_op_unsupported.out" "backend_op_status: unsupported"
contains "$OUT_DIR/matmul_backend_op_unsupported.out" "dispatch_attempted: false"
contains "$OUT_DIR/matmul_backend_op_unsupported.out" "reason: backend-op-matmul-unsupported"
contains "$OUT_DIR/matmul_backend_op_unsupported.out" "status: graph-op-fail"

run_fail matmul_after_input_alloc \
  env YVEX_TEST_FAIL_MATMUL_AFTER_INPUT_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_after_input_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/matmul_after_input_alloc.out" "output_allocation_attempted: false"
contains "$OUT_DIR/matmul_after_input_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/matmul_after_input_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/matmul_after_input_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/matmul_after_input_alloc.out" "status: graph-op-failed-cleaned"

run_fail matmul_after_output_alloc \
  env YVEX_TEST_FAIL_MATMUL_AFTER_OUTPUT_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_after_output_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/matmul_after_output_alloc.out" "output_allocation_attempted: true"
contains "$OUT_DIR/matmul_after_output_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/matmul_after_output_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/matmul_after_output_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/matmul_after_output_alloc.out" "status: graph-op-failed-cleaned"

run_ok matmul_repeat_after_output_alloc \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_repeat_after_output_alloc.out" "status: graph-op-executed"

run_fail matmul_after_dispatch \
  env YVEX_TEST_FAIL_MATMUL_AFTER_DISPATCH=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_after_dispatch.out" "graph_execution_phase: dispatch"
contains "$OUT_DIR/matmul_after_dispatch.out" "dispatch_attempted: true"
contains "$OUT_DIR/matmul_after_dispatch.out" "cleanup_attempted: true"
contains "$OUT_DIR/matmul_after_dispatch.out" "cleanup_status: pass"
contains "$OUT_DIR/matmul_after_dispatch.out" "status: graph-op-failed-cleaned"

run_ok matmul_repeat_after_dispatch \
  "$YVEX_BIN" graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
contains "$OUT_DIR/matmul_repeat_after_dispatch.out" "status: graph-op-executed"
not_contains "$OUT_DIR/matmul_repeat_after_dispatch.out" "qkv_projection_ready: true"

printf 'cli matmul graph: ok\n'
