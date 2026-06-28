#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/attention-graph}

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

run_ok attention_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/attention_cpu.out" "graph_execution_phase: complete"
contains "$OUT_DIR/attention_cpu.out" "graph_kind: attention-primitive"
contains "$OUT_DIR/attention_cpu.out" "op: attention"
contains "$OUT_DIR/attention_cpu.out" "backend: cpu"
contains "$OUT_DIR/attention_cpu.out" "dtype: f32"
contains "$OUT_DIR/attention_cpu.out" "seq_len: 4"
contains "$OUT_DIR/attention_cpu.out" "position: 3"
contains "$OUT_DIR/attention_cpu.out" "head_dim: 8"
contains "$OUT_DIR/attention_cpu.out" "mask: causal"
contains "$OUT_DIR/attention_cpu.out" "backend_status: ready"
contains "$OUT_DIR/attention_cpu.out" "backend_op_status: supported"
contains "$OUT_DIR/attention_cpu.out" "dispatch_attempted: true"
contains "$OUT_DIR/attention_cpu.out" "reference_read_attempted: true"
contains "$OUT_DIR/attention_cpu.out" "reference_attempted: true"
contains "$OUT_DIR/attention_cpu.out" "output_allocation_attempted: true"
contains "$OUT_DIR/attention_cpu.out" "cleanup_status: not-needed"
contains "$OUT_DIR/attention_cpu.out" "query_bytes: 32"
contains "$OUT_DIR/attention_cpu.out" "key_bytes: 128"
contains "$OUT_DIR/attention_cpu.out" "value_bytes: 128"
contains "$OUT_DIR/attention_cpu.out" "input_bytes: 288"
contains "$OUT_DIR/attention_cpu.out" "score_scratch_bytes: 16"
contains "$OUT_DIR/attention_cpu.out" "probability_scratch_bytes: 16"
contains "$OUT_DIR/attention_cpu.out" "output_bytes: 32"
contains "$OUT_DIR/attention_cpu.out" "reference_bytes: 32"
contains "$OUT_DIR/attention_cpu.out" "visible_keys: 4"
contains "$OUT_DIR/attention_cpu.out" "masked_keys: 0"
contains "$OUT_DIR/attention_cpu.out" "last_position_full_prefix: true"
contains "$OUT_DIR/attention_cpu.out" "softmax_max_abs_diff: 0"
contains "$OUT_DIR/attention_cpu.out" "attention_primitive_executed: true"
contains "$OUT_DIR/attention_cpu.out" "qkv_projection_ready: false"
contains "$OUT_DIR/attention_cpu.out" "transformer_block_ready: false"
contains "$OUT_DIR/attention_cpu.out" "full_prefill_ready: false"
contains "$OUT_DIR/attention_cpu.out" "decode_ready: false"
contains "$OUT_DIR/attention_cpu.out" "logits_ready: false"
contains "$OUT_DIR/attention_cpu.out" "generation_ready: false"
contains "$OUT_DIR/attention_cpu.out" "generation: unsupported"
contains "$OUT_DIR/attention_cpu.out" "execution_ready: false"
contains "$OUT_DIR/attention_cpu.out" "status: graph-op-executed"

run_ok attention_zero \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 0 --head-dim 8 --causal
contains "$OUT_DIR/attention_zero.out" "visible_keys: 1"
contains "$OUT_DIR/attention_zero.out" "masked_keys: 3"
contains "$OUT_DIR/attention_zero.out" "causal_mask_future_prob_zero: true"
contains "$OUT_DIR/attention_zero.out" "position_zero_single_key: true"
contains "$OUT_DIR/attention_zero.out" "status: graph-op-executed"

run_ok attention_prefix \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 2 --head-dim 8 --causal
contains "$OUT_DIR/attention_prefix.out" "visible_keys: 3"
contains "$OUT_DIR/attention_prefix.out" "masked_keys: 1"
contains "$OUT_DIR/attention_prefix.out" "causal_mask_future_prob_zero: true"
contains "$OUT_DIR/attention_prefix.out" "last_position_full_prefix: false"
contains "$OUT_DIR/attention_prefix.out" "status: graph-op-executed"

run_fail attention_position_out_of_range \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 4 --head-dim 8 --causal
contains "$OUT_DIR/attention_position_out_of_range.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/attention_position_out_of_range.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/attention_position_out_of_range.out" "slice_range_status: fail"
contains "$OUT_DIR/attention_position_out_of_range.out" "dispatch_attempted: false"
contains "$OUT_DIR/attention_position_out_of_range.out" "output_allocation_attempted: false"
contains "$OUT_DIR/attention_position_out_of_range.out" "reason: position-out-of-range"
contains "$OUT_DIR/attention_position_out_of_range.out" "status: graph-op-fail"

run_fail attention_zero_seq \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 0 --position 0 --head-dim 8 --causal
contains "$OUT_DIR/attention_zero_seq.err" "--seq-len requires a positive integer"

run_fail attention_zero_head_dim \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 0 --head-dim 0 --causal
contains "$OUT_DIR/attention_zero_head_dim.err" "--head-dim requires a positive integer"

run_fail attention_byte_overflow \
  env YVEX_TEST_ATTENTION_BYTE_OVERFLOW=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_byte_overflow.out" "graph_execution_phase: preflight"
contains "$OUT_DIR/attention_byte_overflow.out" "dispatch_attempted: false"
contains "$OUT_DIR/attention_byte_overflow.out" "output_allocation_attempted: false"
contains "$OUT_DIR/attention_byte_overflow.out" "reason: byte-count-overflow"
contains "$OUT_DIR/attention_byte_overflow.out" "status: graph-op-fail"

run_fail attention_after_alloc \
  env YVEX_TEST_FAIL_ATTENTION_AFTER_ALLOC=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_after_alloc.out" "graph_execution_phase: output"
contains "$OUT_DIR/attention_after_alloc.out" "output_allocation_attempted: true"
contains "$OUT_DIR/attention_after_alloc.out" "dispatch_attempted: false"
contains "$OUT_DIR/attention_after_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/attention_after_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/attention_after_alloc.out" "status: graph-op-failed-cleaned"

run_ok attention_repeat_after_alloc \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_repeat_after_alloc.out" "status: graph-op-executed"

run_fail attention_after_dispatch \
  env YVEX_TEST_FAIL_ATTENTION_AFTER_DISPATCH=1 \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_after_dispatch.out" "graph_execution_phase: dispatch"
contains "$OUT_DIR/attention_after_dispatch.out" "dispatch_attempted: true"
contains "$OUT_DIR/attention_after_dispatch.out" "cleanup_attempted: true"
contains "$OUT_DIR/attention_after_dispatch.out" "cleanup_status: pass"
contains "$OUT_DIR/attention_after_dispatch.out" "status: graph-op-failed-cleaned"

run_ok attention_repeat_after_dispatch \
  "$YVEX_BIN" graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
contains "$OUT_DIR/attention_repeat_after_dispatch.out" "status: graph-op-executed"
not_contains "$OUT_DIR/attention_repeat_after_dispatch.out" "transformer_block_ready: true"

printf 'cli attention graph: ok\n'
