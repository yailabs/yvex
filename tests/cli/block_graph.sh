#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli/block-graph}

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

run_ok block_fixture_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-block --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_fixture_cpu.out" "status: graph-block"
contains "$OUT_DIR/block_fixture_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/block_fixture_cpu.out" "graph_execution_phase: complete"
contains "$OUT_DIR/block_fixture_cpu.out" "graph_kind: controlled-block-fixture"
contains "$OUT_DIR/block_fixture_cpu.out" "block: fixture"
contains "$OUT_DIR/block_fixture_cpu.out" "backend: cpu"
contains "$OUT_DIR/block_fixture_cpu.out" "backend_status: ready"
contains "$OUT_DIR/block_fixture_cpu.out" "backend_op_status: supported"
contains "$OUT_DIR/block_fixture_cpu.out" "seq_len: 4"
contains "$OUT_DIR/block_fixture_cpu.out" "position: 3"
contains "$OUT_DIR/block_fixture_cpu.out" "hidden_dim: 8"
contains "$OUT_DIR/block_fixture_cpu.out" "head_dim: 8"
contains "$OUT_DIR/block_fixture_cpu.out" "ffn_dim: 16"
contains "$OUT_DIR/block_fixture_cpu.out" "causal: true"
contains "$OUT_DIR/block_fixture_cpu.out" "gated: true"
contains "$OUT_DIR/block_fixture_cpu.out" "op_count:"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: input"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: attn_norm"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: q_projection"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: k_projection"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: v_projection"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: rope"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: attention"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: o_projection"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: residual_attn"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: post_attn_norm"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: mlp"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: residual_mlp"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: output"
contains "$OUT_DIR/block_fixture_cpu.out" "phase: cleanup"
contains "$OUT_DIR/block_fixture_cpu.out" "scratch_planned_bytes:"
contains "$OUT_DIR/block_fixture_cpu.out" "backend_allocated_bytes:"
contains "$OUT_DIR/block_fixture_cpu.out" "checksum:"
contains "$OUT_DIR/block_fixture_cpu.out" "output_checksum:"
contains "$OUT_DIR/block_fixture_cpu.out" "reference_checksum:"
contains "$OUT_DIR/block_fixture_cpu.out" "max_abs_diff:"
contains "$OUT_DIR/block_fixture_cpu.out" "cleanup: pass"
contains "$OUT_DIR/block_fixture_cpu.out" "cleanup_attempted: true"
contains "$OUT_DIR/block_fixture_cpu.out" "cleanup_status: pass"
contains "$OUT_DIR/block_fixture_cpu.out" "execution_ready: false"
contains "$OUT_DIR/block_fixture_cpu.out" "graph_execution_ready: false"
contains "$OUT_DIR/block_fixture_cpu.out" "generation_ready: false"
contains "$OUT_DIR/block_fixture_cpu.out" "generation: unsupported"
not_contains "$OUT_DIR/block_fixture_cpu.out" "execution_""ready: true"
not_contains "$OUT_DIR/block_fixture_cpu.out" "generation_rea""dy: true"

run_fail block_after_backend_alloc \
  env YVEX_TEST_FAIL_BLOCK_AFTER_BACKEND_ALLOC=1 \
    "$YVEX_BIN" graph --backend cpu --execute-block --block fixture \
      --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_after_backend_alloc.out" "status: graph-block-fail"
contains "$OUT_DIR/block_after_backend_alloc.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/block_after_backend_alloc.out" "graph_execution_phase: backend-allocation"
contains "$OUT_DIR/block_after_backend_alloc.out" "backend_status: ready"
contains "$OUT_DIR/block_after_backend_alloc.out" "backend_op_status: supported"
contains "$OUT_DIR/block_after_backend_alloc.out" "cleanup_attempted: true"
contains "$OUT_DIR/block_after_backend_alloc.out" "cleanup_status: pass"
contains "$OUT_DIR/block_after_backend_alloc.out" "execution_ready: false"
contains "$OUT_DIR/block_after_backend_alloc.out" "graph_execution_ready: false"
contains "$OUT_DIR/block_after_backend_alloc.out" "generation_ready: false"
contains "$OUT_DIR/block_after_backend_alloc.out" "generation: unsupported"
contains "$OUT_DIR/block_after_backend_alloc.err" "test block failure after backend allocation"

run_ok layer_fixture_cpu \
  "$YVEX_BIN" graph --backend cpu --execute-layers --layers 2 --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layer_fixture_cpu.out" "status: graph-layers"
contains "$OUT_DIR/layer_fixture_cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/layer_fixture_cpu.out" "graph_kind: controlled-layer-fixture"
contains "$OUT_DIR/layer_fixture_cpu.out" "layers: 2"
contains "$OUT_DIR/layer_fixture_cpu.out" "layer_handoff: selected-position-row"
contains "$OUT_DIR/layer_fixture_cpu.out" "sequence_rebuild: deterministic-with-previous-position-row"
contains "$OUT_DIR/layer_fixture_cpu.out" "total_op_count: 24"
contains "$OUT_DIR/layer_fixture_cpu.out" "layer_0_checksum:"
contains "$OUT_DIR/layer_fixture_cpu.out" "layer_1_checksum:"
contains "$OUT_DIR/layer_fixture_cpu.out" "final_max_abs_diff:"
contains "$OUT_DIR/layer_fixture_cpu.out" "execution_ready: false"
contains "$OUT_DIR/layer_fixture_cpu.out" "graph_execution_ready: false"
contains "$OUT_DIR/layer_fixture_cpu.out" "generation_ready: false"
contains "$OUT_DIR/layer_fixture_cpu.out" "generation: unsupported"
not_contains "$OUT_DIR/layer_fixture_cpu.out" "execution_""ready: true"
not_contains "$OUT_DIR/layer_fixture_cpu.out" "generation_rea""dy: true"

run_fail layers_after_layer_0 \
  env YVEX_TEST_FAIL_LAYERS_AFTER_LAYER_0=1 \
    "$YVEX_BIN" graph --backend cpu --execute-layers --layers 2 --block fixture \
      --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layers_after_layer_0.out" "status: graph-layers-failed-cleaned"
contains "$OUT_DIR/layers_after_layer_0.out" "cleanup_attempted: true"
contains "$OUT_DIR/layers_after_layer_0.out" "cleanup_status: pass"
contains "$OUT_DIR/layers_after_layer_0.out" "generation: unsupported"

run_fail layers_missing_layers \
  "$YVEX_BIN" graph --backend cpu --execute-layers --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layers_missing_layers.err" "--execute-layers requires --layers N"

run_fail layers_zero \
  "$YVEX_BIN" graph --backend cpu --execute-layers --layers 0 --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layers_zero.err" "--layers requires a positive integer"

run_fail layers_too_many \
  "$YVEX_BIN" graph --backend cpu --execute-layers --layers 17 --block fixture \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layers_too_many.err" "requires 1 <= --layers <= 16"

run_fail layers_unsupported_block \
  "$YVEX_BIN" graph --backend cpu --execute-layers --layers 2 --block nope \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/layers_unsupported_block.err" "unsupported block: nope"

run_fail block_position_oob \
  "$YVEX_BIN" graph --backend cpu --execute-block --block fixture \
    --seq-len 4 --position 4 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_position_oob.err" "position must be less than seq-len"

run_fail block_bad_divisibility \
  "$YVEX_BIN" graph --backend cpu --execute-block --block fixture \
    --seq-len 4 --position 3 --hidden-dim 12 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_bad_divisibility.err" "hidden-dim must be divisible by head-dim"

run_fail block_missing_block \
  "$YVEX_BIN" graph --backend cpu --execute-block \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_missing_block.err" "--execute-block requires --block fixture"

run_fail block_unsupported_real \
  "$YVEX_BIN" graph --backend cpu --execute-block --block real \
    --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
contains "$OUT_DIR/block_unsupported_real.err" "unsupported block: real"

printf 'cli block graph: ok\n'
