#!/bin/sh
set -u

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-${OUT_DIR:-build/tests/cli/cuda}}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf
CONTROLLED="$OUT_DIR/cuda-controlled.gguf"
PARTIAL="$OUT_DIR/cuda-controlled-f16.gguf"
SEGMENT="$OUT_DIR/cuda-segment-f16.gguf"

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
contains "$OUT_DIR/backend_cuda_ready.out" "op_rms_norm: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_rope: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_attention: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_matmul: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_mlp: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "status: backend-ready"

"$YVEX_BIN" plan "$FIXTURE" --backend cuda >"$OUT_DIR/plan_cuda_ready.out" 2>"$OUT_DIR/plan_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "plan cuda exit code was $rc"
fi
contains "$OUT_DIR/plan_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/plan_cuda_ready.out" "backend_status: available"
contains "$OUT_DIR/plan_cuda_ready.out" "op_embed: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "op_rms_norm: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "op_rope: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "op_attention: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "op_matmul: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "op_mlp: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "execution_ready: false"
contains "$OUT_DIR/plan_cuda_ready.out" "status: plan-only"

"$YVEX_BIN" graph --backend cuda --execute-op --op rope --position 7 --head-dim 8 \
  >"$OUT_DIR/rope_graph_cuda.out" 2>"$OUT_DIR/rope_graph_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "rope graph cuda exit code was $rc"
fi
contains "$OUT_DIR/rope_graph_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/rope_graph_cuda.out" "graph_execution_phase: complete"
contains "$OUT_DIR/rope_graph_cuda.out" "graph_kind: rope-position-op"
contains "$OUT_DIR/rope_graph_cuda.out" "op: rope"
contains "$OUT_DIR/rope_graph_cuda.out" "backend: cuda"
contains "$OUT_DIR/rope_graph_cuda.out" "position: 7"
contains "$OUT_DIR/rope_graph_cuda.out" "head_dim: 8"
contains "$OUT_DIR/rope_graph_cuda.out" "backend_op_status: supported"
contains "$OUT_DIR/rope_graph_cuda.out" "dispatch_attempted: true"
contains "$OUT_DIR/rope_graph_cuda.out" "reference_attempted: true"
contains "$OUT_DIR/rope_graph_cuda.out" "rope_cuda_parity: pass"
contains "$OUT_DIR/rope_graph_cuda.out" "attention_ready: false"
contains "$OUT_DIR/rope_graph_cuda.out" "decode_ready: false"
contains "$OUT_DIR/rope_graph_cuda.out" "generation_ready: false"
contains "$OUT_DIR/rope_graph_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal \
  >"$OUT_DIR/attention_graph_cuda.out" 2>"$OUT_DIR/attention_graph_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "attention graph cuda exit code was $rc"
fi
contains "$OUT_DIR/attention_graph_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/attention_graph_cuda.out" "graph_execution_phase: complete"
contains "$OUT_DIR/attention_graph_cuda.out" "graph_kind: attention-primitive"
contains "$OUT_DIR/attention_graph_cuda.out" "op: attention"
contains "$OUT_DIR/attention_graph_cuda.out" "backend: cuda"
contains "$OUT_DIR/attention_graph_cuda.out" "seq_len: 4"
contains "$OUT_DIR/attention_graph_cuda.out" "position: 3"
contains "$OUT_DIR/attention_graph_cuda.out" "head_dim: 8"
contains "$OUT_DIR/attention_graph_cuda.out" "mask: causal"
contains "$OUT_DIR/attention_graph_cuda.out" "backend_op_status: supported"
contains "$OUT_DIR/attention_graph_cuda.out" "dispatch_attempted: true"
contains "$OUT_DIR/attention_graph_cuda.out" "reference_attempted: true"
contains "$OUT_DIR/attention_graph_cuda.out" "attention_cuda_parity: pass"
contains "$OUT_DIR/attention_graph_cuda.out" "transformer_block_ready: false"
contains "$OUT_DIR/attention_graph_cuda.out" "decode_ready: false"
contains "$OUT_DIR/attention_graph_cuda.out" "generation_ready: false"
contains "$OUT_DIR/attention_graph_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-op --op matmul --m 1 --k 8 --n 8 \
  >"$OUT_DIR/matmul_projection_cuda.out" 2>"$OUT_DIR/matmul_projection_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "matmul projection cuda exit code was $rc"
fi
contains "$OUT_DIR/matmul_projection_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/matmul_projection_cuda.out" "graph_execution_phase: complete"
contains "$OUT_DIR/matmul_projection_cuda.out" "graph_kind: matmul-projection"
contains "$OUT_DIR/matmul_projection_cuda.out" "op: matmul"
contains "$OUT_DIR/matmul_projection_cuda.out" "backend: cuda"
contains "$OUT_DIR/matmul_projection_cuda.out" "m: 1"
contains "$OUT_DIR/matmul_projection_cuda.out" "k: 8"
contains "$OUT_DIR/matmul_projection_cuda.out" "n: 8"
contains "$OUT_DIR/matmul_projection_cuda.out" "projection_shape: true"
contains "$OUT_DIR/matmul_projection_cuda.out" "backend_op_status: supported"
contains "$OUT_DIR/matmul_projection_cuda.out" "dispatch_attempted: true"
contains "$OUT_DIR/matmul_projection_cuda.out" "reference_attempted: true"
contains "$OUT_DIR/matmul_projection_cuda.out" "matmul_cuda_parity: pass"
contains "$OUT_DIR/matmul_projection_cuda.out" "qkv_projection_ready: false"
contains "$OUT_DIR/matmul_projection_cuda.out" "transformer_block_ready: false"
contains "$OUT_DIR/matmul_projection_cuda.out" "decode_ready: false"
contains "$OUT_DIR/matmul_projection_cuda.out" "generation_ready: false"
contains "$OUT_DIR/matmul_projection_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-op --op matmul --m 2 --k 4 --n 3 \
  >"$OUT_DIR/matmul_matrix_cuda.out" 2>"$OUT_DIR/matmul_matrix_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "matmul matrix cuda exit code was $rc"
fi
contains "$OUT_DIR/matmul_matrix_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/matmul_matrix_cuda.out" "graph_kind: matmul-matrix"
contains "$OUT_DIR/matmul_matrix_cuda.out" "projection_shape: false"
contains "$OUT_DIR/matmul_matrix_cuda.out" "non_projection_shape: true"
contains "$OUT_DIR/matmul_matrix_cuda.out" "matmul_cuda_parity: pass"
contains "$OUT_DIR/matmul_matrix_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated \
  >"$OUT_DIR/mlp_dense_cuda.out" 2>"$OUT_DIR/mlp_dense_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "mlp dense cuda exit code was $rc"
fi
contains "$OUT_DIR/mlp_dense_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/mlp_dense_cuda.out" "graph_execution_phase: complete"
contains "$OUT_DIR/mlp_dense_cuda.out" "graph_kind: mlp-feed-forward"
contains "$OUT_DIR/mlp_dense_cuda.out" "op: mlp"
contains "$OUT_DIR/mlp_dense_cuda.out" "backend: cuda"
contains "$OUT_DIR/mlp_dense_cuda.out" "hidden_dim: 8"
contains "$OUT_DIR/mlp_dense_cuda.out" "ffn_dim: 16"
contains "$OUT_DIR/mlp_dense_cuda.out" "routed_expert_mode: false"
contains "$OUT_DIR/mlp_dense_cuda.out" "backend_op_status: supported"
contains "$OUT_DIR/mlp_dense_cuda.out" "dispatch_attempted: true"
contains "$OUT_DIR/mlp_dense_cuda.out" "reference_attempted: true"
contains "$OUT_DIR/mlp_dense_cuda.out" "mlp_cuda_parity: pass"
contains "$OUT_DIR/mlp_dense_cuda.out" "transformer_block_ready: false"
contains "$OUT_DIR/mlp_dense_cuda.out" "decode_ready: false"
contains "$OUT_DIR/mlp_dense_cuda.out" "generation_ready: false"
contains "$OUT_DIR/mlp_dense_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1 \
  >"$OUT_DIR/mlp_routed_cuda.out" 2>"$OUT_DIR/mlp_routed_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "mlp routed cuda exit code was $rc"
fi
contains "$OUT_DIR/mlp_routed_cuda.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/mlp_routed_cuda.out" "graph_kind: mlp-routed-expert"
contains "$OUT_DIR/mlp_routed_cuda.out" "routed_expert_mode: true"
contains "$OUT_DIR/mlp_routed_cuda.out" "expert_count: 2"
contains "$OUT_DIR/mlp_routed_cuda.out" "expert_id: 1"
contains "$OUT_DIR/mlp_routed_cuda.out" "mlp_cuda_parity: pass"
contains "$OUT_DIR/mlp_routed_cuda.out" "status: graph-op-executed"

"$YVEX_BIN" graph --backend cuda --execute-block --block fixture \
  --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16 \
  >"$OUT_DIR/block_fixture_cuda.out" 2>"$OUT_DIR/block_fixture_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "block fixture cuda exit code was $rc"
fi
contains "$OUT_DIR/block_fixture_cuda.out" "status: graph-block"
contains "$OUT_DIR/block_fixture_cuda.out" "block: fixture"
contains "$OUT_DIR/block_fixture_cuda.out" "backend: cuda"
contains "$OUT_DIR/block_fixture_cuda.out" "seq_len: 4"
contains "$OUT_DIR/block_fixture_cuda.out" "position: 3"
contains "$OUT_DIR/block_fixture_cuda.out" "hidden_dim: 8"
contains "$OUT_DIR/block_fixture_cuda.out" "head_dim: 8"
contains "$OUT_DIR/block_fixture_cuda.out" "ffn_dim: 16"
contains "$OUT_DIR/block_fixture_cuda.out" "phase: attn_norm"
contains "$OUT_DIR/block_fixture_cuda.out" "phase: attention"
contains "$OUT_DIR/block_fixture_cuda.out" "phase: residual_mlp"
contains "$OUT_DIR/block_fixture_cuda.out" "phase: cleanup"
contains "$OUT_DIR/block_fixture_cuda.out" "checksum:"
contains "$OUT_DIR/block_fixture_cuda.out" "reference_checksum:"
contains "$OUT_DIR/block_fixture_cuda.out" "max_abs_diff:"
contains "$OUT_DIR/block_fixture_cuda.out" "cleanup: pass"
contains "$OUT_DIR/block_fixture_cuda.out" "block_cuda_parity: pass"
contains "$OUT_DIR/block_fixture_cuda.out" "execution_ready: false"
contains "$OUT_DIR/block_fixture_cuda.out" "generation_ready: false"

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

python3 - "$SEGMENT" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
magic = 0x46554747
version = 3
string = 8
uint32 = 4
float32 = 6
ggml_f16 = 1

def u32(value):
    return struct.pack("<I", value)

def u64(value):
    return struct.pack("<Q", value)

def gguf_string(value):
    data = value.encode("utf-8")
    return u64(len(data)) + data

def kv_string(key, value):
    return gguf_string(key) + u32(string) + gguf_string(value)

def kv_u32(key, value):
    return gguf_string(key) + u32(uint32) + u32(value)

def kv_f32(key, value):
    return gguf_string(key) + u32(float32) + struct.pack("<f", value)

def tensor(name, dims, ggml_type, offset):
    data = gguf_string(name) + u32(len(dims))
    for dim in dims:
        data += u64(dim)
    return data + u32(ggml_type) + u64(offset)

def f16(value):
    return struct.pack("<e", float(value))

metadata = [
    kv_string("general.architecture", "deepseek"),
    kv_string("general.name", "cuda-segment-fixture"),
    kv_u32("general.alignment", 32),
    kv_u32("general.file_type", 1),
    kv_f32("deepseek2.attention.layer_norm_rms_epsilon", 0.000001),
]
tensors = [
    tensor("token_embd.weight", [4, 8], ggml_f16, 0),
    tensor("blk.0.attn_norm.weight", [4], ggml_f16, 64),
]
payload = b"".join(f16(i) for i in range(32)) + b"".join(f16(1) for _ in range(4))
data = u32(magic) + u32(version) + u64(len(tensors)) + u64(len(metadata))
data += b"".join(metadata) + b"".join(tensors)
data += b"\0" * ((-len(data)) % 32)
path.write_bytes(data + payload)
PY

"$YVEX_BIN" graph --model "$SEGMENT" --backend cuda \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/segment_graph_cuda.out" 2>"$OUT_DIR/segment_graph_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "segment graph cuda exit code was $rc"
fi
contains "$OUT_DIR/segment_graph_cuda.out" "graph_kind: selected-embedding-rmsnorm"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_graph_executed: true"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_backend: cuda"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_ops: 2"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_op_0: embed"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_op_1: rms_norm"
contains "$OUT_DIR/segment_graph_cuda.out" "segment_cuda_parity: pass"
contains "$OUT_DIR/segment_graph_cuda.out" "status: real-segment-graph-executed"

"$YVEX_BIN" graph --model "$SEGMENT" --backend cuda \
  --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1 \
  >"$OUT_DIR/segment_graph_cuda_token_input.out" 2>"$OUT_DIR/segment_graph_cuda_token_input.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "segment graph cuda token input exit code was $rc"
fi
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "token_input_status: pass"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "selected_token_index: 1"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "selected_token_id: 1"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "graph_kind: selected-embedding-rmsnorm"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "segment_backend: cuda"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "partial_token: 1"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "segment_cuda_parity: pass"
contains "$OUT_DIR/segment_graph_cuda_token_input.out" "status: real-segment-graph-executed"

"$YVEX_BIN" prefill --model "$SEGMENT" --backend cuda \
  --segment embedding-rmsnorm --tokens 0,1 \
  >"$OUT_DIR/prefill_cuda.out" 2>"$OUT_DIR/prefill_cuda.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "prefill cuda exit code was $rc"
fi
contains "$OUT_DIR/prefill_cuda.out" "prefill_state_created: true"
contains "$OUT_DIR/prefill_cuda.out" "backend: cuda"
contains "$OUT_DIR/prefill_cuda.out" "token_count: 2"
contains "$OUT_DIR/prefill_cuda.out" "tokens_processed: 2"
contains "$OUT_DIR/prefill_cuda.out" "segment_graph_executions: 2"
contains "$OUT_DIR/prefill_cuda.out" "prefill_cuda_parity: pass"
contains "$OUT_DIR/prefill_cuda.out" "kv_ready: false"
contains "$OUT_DIR/prefill_cuda.out" "logits_ready: false"
contains "$OUT_DIR/prefill_cuda.out" "generation: unsupported"
contains "$OUT_DIR/prefill_cuda.out" "status: prefill-state-created"

"$YVEX_BIN" prefill --model "$SEGMENT" --backend cuda \
  --segment embedding-rmsnorm --tokens 0,1 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8 \
  >"$OUT_DIR/prefill_cuda_kv.out" 2>"$OUT_DIR/prefill_cuda_kv.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "prefill cuda kv exit code was $rc"
fi
contains "$OUT_DIR/prefill_cuda_kv.out" "prefill_state_created: true"
contains "$OUT_DIR/prefill_cuda_kv.out" "backend: cuda"
contains "$OUT_DIR/prefill_cuda_kv.out" "tokens_processed: 2"
contains "$OUT_DIR/prefill_cuda_kv.out" "prefill_cuda_parity: pass"
contains "$OUT_DIR/prefill_cuda_kv.out" "kv_ready: true"
contains "$OUT_DIR/prefill_cuda_kv.out" "session_kv_owned: true"
contains "$OUT_DIR/prefill_cuda_kv.out" "kv_bound_to_prefill: true"
contains "$OUT_DIR/prefill_cuda_kv.out" "kv_positions_written: 2"
contains "$OUT_DIR/prefill_cuda_kv.out" "kv_read_count: 1"
contains "$OUT_DIR/prefill_cuda_kv.out" "full_transformer_prefill_ready: false"
contains "$OUT_DIR/prefill_cuda_kv.out" "decode_ready: false"
contains "$OUT_DIR/prefill_cuda_kv.out" "logits_ready: false"
contains "$OUT_DIR/prefill_cuda_kv.out" "generation_ready: false"
contains "$OUT_DIR/prefill_cuda_kv.out" "generation: unsupported"
contains "$OUT_DIR/prefill_cuda_kv.out" "status: prefill-state-created"

"$YVEX_BIN" help cuda-info >"$OUT_DIR/help_cuda_info.out" 2>"$OUT_DIR/help_cuda_info.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "help cuda-info exit code was $rc"
fi
contains "$OUT_DIR/help_cuda_info.out" "usage: yvex cuda-info"

echo "cli cuda smoke: ok"
