#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/rope-graph}

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

run_ok rope_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/rope_cpu.out" "graph_execution_phase: complete"
contains "$OUT_DIR/rope_cpu.out" "graph_kind: rope-position-op"
contains "$OUT_DIR/rope_cpu.out" "op: rope"
contains "$OUT_DIR/rope_cpu.out" "backend: cpu"
contains "$OUT_DIR/rope_cpu.out" "position: 7"
contains "$OUT_DIR/rope_cpu.out" "head_dim: 8"
contains "$OUT_DIR/rope_cpu.out" "dtype: f32"
contains "$OUT_DIR/rope_cpu.out" "backend_status: ready"
contains "$OUT_DIR/rope_cpu.out" "backend_op_status: supported"
contains "$OUT_DIR/rope_cpu.out" "dispatch_attempted: true"
contains "$OUT_DIR/rope_cpu.out" "reference_read_attempted: true"
contains "$OUT_DIR/rope_cpu.out" "reference_attempted: true"
contains "$OUT_DIR/rope_cpu.out" "output_allocation_attempted: true"
contains "$OUT_DIR/rope_cpu.out" "cleanup_status: not-needed"
contains "$OUT_DIR/rope_cpu.out" "input_bytes: 32"
contains "$OUT_DIR/rope_cpu.out" "output_bytes: 32"
contains "$OUT_DIR/rope_cpu.out" "reference_bytes: 32"
contains "$OUT_DIR/rope_cpu.out" "position_dependent_output: true"
contains "$OUT_DIR/rope_cpu.out" "attention_ready: false"
contains "$OUT_DIR/rope_cpu.out" "transformer_block_ready: false"
contains "$OUT_DIR/rope_cpu.out" "decode_ready: false"
contains "$OUT_DIR/rope_cpu.out" "logits_ready: false"
contains "$OUT_DIR/rope_cpu.out" "generation_ready: false"
contains "$OUT_DIR/rope_cpu.out" "generation: unsupported"
contains "$OUT_DIR/rope_cpu.out" "status: graph-op-executed"

run_ok rope_zero \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 0 --head-dim 8
contains "$OUT_DIR/rope_zero.out" "position_zero_identity: true"
contains "$OUT_DIR/rope_zero.out" "input_output_max_abs_diff: 0"
contains "$OUT_DIR/rope_zero.out" "status: graph-op-executed"

run_ok rope_large_position \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 1024 --head-dim 8
contains "$OUT_DIR/rope_large_position.out" "position: 1024"
contains "$OUT_DIR/rope_large_position.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/rope_large_position.out" "status: graph-op-executed"

run_fail rope_odd \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 7
contains "$OUT_DIR/rope_odd.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/rope_odd.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/rope_odd.out" "dispatch_attempted: false"
contains "$OUT_DIR/rope_odd.out" "output_allocation_attempted: false"
contains "$OUT_DIR/rope_odd.out" "reason: rope-head-dim-odd"
contains "$OUT_DIR/rope_odd.out" "status: graph-op-fail"

run_fail rope_zero_dim \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 0
contains "$OUT_DIR/rope_zero_dim.err" "--head-dim requires a positive integer"

run_fail rope_byte_overflow \
  env YVEX_TEST_ROPE_BYTE_OVERFLOW=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_byte_overflow.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/rope_byte_overflow.out" "dispatch_attempted: false"
contains "$OUT_DIR/rope_byte_overflow.out" "output_allocation_attempted: false"
contains "$OUT_DIR/rope_byte_overflow.out" "reason: byte-count-overflow"
contains "$OUT_DIR/rope_byte_overflow.out" "status: graph-op-fail"

run_fail rope_after_alloc \
  env YVEX_TEST_FAIL_ROPE_AFTER_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_after_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/rope_after_alloc.out" "output_allocation_attempted: true"
contains "$OUT_DIR/rope_after_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/rope_after_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/rope_after_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/rope_after_alloc.out" "status: graph-op-failed-cleaned"

run_ok rope_repeat_after_alloc \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_repeat_after_alloc.out" "status: graph-op-executed"

run_fail rope_after_dispatch \
  env YVEX_TEST_FAIL_ROPE_AFTER_DISPATCH=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_after_dispatch.out" "graph_execution_phase: dispatch"
contains "$OUT_DIR/rope_after_dispatch.out" "dispatch_attempted: true"
contains "$OUT_DIR/rope_after_dispatch.out" "cleanup_attempted: true"
contains "$OUT_DIR/rope_after_dispatch.out" "cleanup_status: pass"
contains "$OUT_DIR/rope_after_dispatch.out" "status: graph-op-failed-cleaned"

run_ok rope_repeat_after_dispatch \
  "$YVEX_BIN" graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
contains "$OUT_DIR/rope_repeat_after_dispatch.out" "status: graph-op-executed"
not_contains "$OUT_DIR/rope_repeat_after_dispatch.out" "attention_ready: true"

printf 'cli rope graph: ok\n'
