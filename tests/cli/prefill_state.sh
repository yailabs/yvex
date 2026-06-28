#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/prefill-state}
SEGMENT_MODEL="$OUT_DIR/prefill-segment-F16.gguf"
MISSING_RMS="$OUT_DIR/prefill-missing-rmsnorm.gguf"

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
    ! grep -F -- "$text" "$file" >/dev/null || fail "$file unexpectedly contained: $text"
}

expect_fail() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" && fail "$name unexpectedly passed" || true
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

python3 - "$SEGMENT_MODEL" "$MISSING_RMS" <<'PY'
import pathlib
import struct
import sys

segment = pathlib.Path(sys.argv[1])
missing = pathlib.Path(sys.argv[2])
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

def file_bytes(metadata, tensors, payload, alignment=32):
    data = u32(magic) + u32(version) + u64(len(tensors)) + u64(len(metadata))
    data += b"".join(metadata) + b"".join(tensors)
    data += b"\0" * ((-len(data)) % alignment)
    return data + payload

metadata = [
    kv_string("general.architecture", "deepseek"),
    kv_string("general.name", "prefill-segment-fixture"),
    kv_u32("general.alignment", 32),
    kv_u32("general.file_type", 1),
    kv_f32("deepseek2.attention.layer_norm_rms_epsilon", 0.000001),
]
embed_payload = b"".join(f16(i) for i in range(32))
rms_payload = b"".join(f16(1) for _ in range(4))

segment.write_bytes(
    file_bytes(
        metadata,
        [
            tensor("token_embd.weight", [4, 8], ggml_f16, 0),
            tensor("blk.0.attn_norm.weight", [4], ggml_f16, 64),
        ],
        embed_payload + rms_payload,
    )
)
missing.write_bytes(
    file_bytes(
        metadata,
        [tensor("token_embd.weight", [4, 8], ggml_f16, 0)],
        embed_payload,
    )
)
PY

"$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2 \
  >"$OUT_DIR/prefill-cpu.out" 2>"$OUT_DIR/prefill-cpu.err"
contains "$OUT_DIR/prefill-cpu.out" "prefill: state"
contains "$OUT_DIR/prefill-cpu.out" "prefill_state_created: true"
contains "$OUT_DIR/prefill-cpu.out" "prefill_state_kind: segment-summary"
contains "$OUT_DIR/prefill-cpu.out" "sequence_execution_mode: independent-token-segments"
contains "$OUT_DIR/prefill-cpu.out" "backend: cpu"
contains "$OUT_DIR/prefill-cpu.out" "segment: embedding-rmsnorm"
contains "$OUT_DIR/prefill-cpu.out" "token_input_status: pass"
contains "$OUT_DIR/prefill-cpu.out" "token_count: 3"
contains "$OUT_DIR/prefill-cpu.out" "tokens_processed: 3"
contains "$OUT_DIR/prefill-cpu.out" "position_start: 0"
contains "$OUT_DIR/prefill-cpu.out" "position_end: 2"
contains "$OUT_DIR/prefill-cpu.out" "segment_graph_executions: 3"
contains "$OUT_DIR/prefill-cpu.out" "segment_output_count: 4"
contains "$OUT_DIR/prefill-cpu.out" "segment_output_bytes: 16"
contains "$OUT_DIR/prefill-cpu.out" "prefill_total_output_bytes: 48"
contains "$OUT_DIR/prefill-cpu.out" "prefill_max_abs_diff: 0"
contains "$OUT_DIR/prefill-cpu.out" "kv_ready: false"
contains "$OUT_DIR/prefill-cpu.out" "kv_bound_to_prefill: false"
contains "$OUT_DIR/prefill-cpu.out" "kv_status: not-requested"
contains "$OUT_DIR/prefill-cpu.out" "full_transformer_prefill_ready: false"
contains "$OUT_DIR/prefill-cpu.out" "decode_ready: false"
contains "$OUT_DIR/prefill-cpu.out" "logits_ready: false"
contains "$OUT_DIR/prefill-cpu.out" "generation_ready: false"
contains "$OUT_DIR/prefill-cpu.out" "generation: unsupported"
contains "$OUT_DIR/prefill-cpu.out" "status: prefill-state-created"
not_contains "$OUT_DIR/prefill-cpu.out" "kv_ready: true"
not_contains "$OUT_DIR/prefill-cpu.out" "logits_ready: true"

"$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8 \
  >"$OUT_DIR/prefill-kv-cpu.out" 2>"$OUT_DIR/prefill-kv-cpu.err"
contains "$OUT_DIR/prefill-kv-cpu.out" "prefill_state_created: true"
contains "$OUT_DIR/prefill-kv-cpu.out" "tokens_processed: 3"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_ready: true"
contains "$OUT_DIR/prefill-kv-cpu.out" "session_kv_owned: true"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_bound_to_prefill: true"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_binding_kind: minimal-diagnostic"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_status: allocated"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_owner: session"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_dtype: F32"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_layers: 1"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_heads: 2"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_head_dim: 4"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_capacity: 8"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_values_per_position: 16"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_bytes_per_position: 64"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_planned_bytes: 512"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_allocated_bytes: 512"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_positions_written: 3"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_append_count: 3"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_read_count: 1"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_read_position: 0"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_read_value_count: 16"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_read_checksum:"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_read_sample_values:"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_overflow: not-overflowed"
contains "$OUT_DIR/prefill-kv-cpu.out" "kv_cleanup_status: pass"
contains "$OUT_DIR/prefill-kv-cpu.out" "full_transformer_prefill_ready: false"
contains "$OUT_DIR/prefill-kv-cpu.out" "decode_ready: false"
contains "$OUT_DIR/prefill-kv-cpu.out" "logits_ready: false"
contains "$OUT_DIR/prefill-kv-cpu.out" "generation_ready: false"
contains "$OUT_DIR/prefill-kv-cpu.out" "generation: unsupported"
contains "$OUT_DIR/prefill-kv-cpu.out" "status: prefill-state-created"

"$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 3 \
  >"$OUT_DIR/prefill-kv-capacity-equal.out" 2>"$OUT_DIR/prefill-kv-capacity-equal.err"
contains "$OUT_DIR/prefill-kv-capacity-equal.out" "kv_bound_to_prefill: true"
contains "$OUT_DIR/prefill-kv-capacity-equal.out" "kv_capacity: 3"
contains "$OUT_DIR/prefill-kv-capacity-equal.out" "kv_positions_written: 3"
contains "$OUT_DIR/prefill-kv-capacity-equal.out" "kv_planned_bytes: 192"

expect_fail kv-capacity-too-small "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1,2 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 2
contains "$OUT_DIR/kv-capacity-too-small.out" "prefill_state_created: false"
contains "$OUT_DIR/kv-capacity-too-small.out" "prefill_phase: kv-preflight"
contains "$OUT_DIR/kv-capacity-too-small.out" "tokens_processed: 0"
contains "$OUT_DIR/kv-capacity-too-small.out" "kv_bound_to_prefill: false"
contains "$OUT_DIR/kv-capacity-too-small.out" "kv_capacity: 2"
contains "$OUT_DIR/kv-capacity-too-small.out" "kv_overflow: capacity-too-small"
contains "$OUT_DIR/kv-capacity-too-small.out" "status: prefill-state-fail"
contains "$OUT_DIR/kv-capacity-too-small.err" "KV capacity is smaller than token count"

expect_fail kv-zero-capacity "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 0
contains "$OUT_DIR/kv-zero-capacity.err" "--kv-capacity requires a positive integer"

expect_fail malformed-token-list "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 1,,2
contains "$OUT_DIR/malformed-token-list.out" "prefill_state_created: false"
contains "$OUT_DIR/malformed-token-list.out" "token_input_status: fail"
contains "$OUT_DIR/malformed-token-list.out" "tokens_processed: 0"
contains "$OUT_DIR/malformed-token-list.out" "status: prefill-state-fail"
contains "$OUT_DIR/malformed-token-list.err" "token-parse-invalid"

expect_fail token-out-of-vocab "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 8
contains "$OUT_DIR/token-out-of-vocab.out" "prefill_state_created: false"
contains "$OUT_DIR/token-out-of-vocab.out" "token_input_status: fail"
contains "$OUT_DIR/token-out-of-vocab.out" "tokens_processed: 0"
contains "$OUT_DIR/token-out-of-vocab.out" "status: prefill-state-fail"
contains "$OUT_DIR/token-out-of-vocab.err" "token-out-of-vocab"

expect_fail missing-rms "$YVEX_BIN" prefill --model "$MISSING_RMS" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1
contains "$OUT_DIR/missing-rms.out" "prefill_state_created: false"
contains "$OUT_DIR/missing-rms.out" "tokens_processed: 0"
contains "$OUT_DIR/missing-rms.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/missing-rms.out" "dispatch_attempted: false"
contains "$OUT_DIR/missing-rms.out" "status: prefill-state-fail"
contains "$OUT_DIR/missing-rms.err" "rmsnorm-tensor-missing"

expect_fail missing-rms-kv "$YVEX_BIN" prefill --model "$MISSING_RMS" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
contains "$OUT_DIR/missing-rms-kv.out" "prefill_state_created: false"
contains "$OUT_DIR/missing-rms-kv.out" "tokens_processed: 0"
contains "$OUT_DIR/missing-rms-kv.out" "kv_bound_to_prefill: false"
contains "$OUT_DIR/missing-rms-kv.out" "kv_status: planned"
contains "$OUT_DIR/missing-rms-kv.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/missing-rms-kv.out" "dispatch_attempted: false"
contains "$OUT_DIR/missing-rms-kv.out" "status: prefill-state-fail"

expect_fail injected-after-token-0 env YVEX_TEST_FAIL_PREFILL_AFTER_TOKEN_0=1 \
  "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1
contains "$OUT_DIR/injected-after-token-0.out" "prefill_state_created: false"
contains "$OUT_DIR/injected-after-token-0.out" "prefill_phase: token-execution"
contains "$OUT_DIR/injected-after-token-0.out" "failed_token_index: 1"
contains "$OUT_DIR/injected-after-token-0.out" "tokens_processed: 1"
contains "$OUT_DIR/injected-after-token-0.out" "cleanup_attempted: true"
contains "$OUT_DIR/injected-after-token-0.out" "cleanup_status: pass"
contains "$OUT_DIR/injected-after-token-0.out" "status: prefill-state-fail"

expect_fail injected-kv-after-append env YVEX_TEST_FAIL_PREFILL_KV_AFTER_APPEND_0=1 \
  "$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
contains "$OUT_DIR/injected-kv-after-append.out" "prefill_state_created: false"
contains "$OUT_DIR/injected-kv-after-append.out" "prefill_phase: kv-append"
contains "$OUT_DIR/injected-kv-after-append.out" "failed_token_index: 1"
contains "$OUT_DIR/injected-kv-after-append.out" "tokens_processed: 1"
contains "$OUT_DIR/injected-kv-after-append.out" "kv_positions_written: 1"
contains "$OUT_DIR/injected-kv-after-append.out" "cleanup_attempted: true"
contains "$OUT_DIR/injected-kv-after-append.out" "cleanup_status: pass"
contains "$OUT_DIR/injected-kv-after-append.out" "kv_cleanup_status: pass"
contains "$OUT_DIR/injected-kv-after-append.out" "status: prefill-state-fail"

"$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1 \
  >"$OUT_DIR/repeat-after-failure.out" 2>"$OUT_DIR/repeat-after-failure.err"
contains "$OUT_DIR/repeat-after-failure.out" "prefill_state_created: true"
contains "$OUT_DIR/repeat-after-failure.out" "tokens_processed: 2"
contains "$OUT_DIR/repeat-after-failure.out" "status: prefill-state-created"

"$YVEX_BIN" prefill --model "$SEGMENT_MODEL" --backend cpu \
  --segment embedding-rmsnorm --tokens 0,1 \
  --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8 \
  >"$OUT_DIR/repeat-kv-after-failure.out" 2>"$OUT_DIR/repeat-kv-after-failure.err"
contains "$OUT_DIR/repeat-kv-after-failure.out" "prefill_state_created: true"
contains "$OUT_DIR/repeat-kv-after-failure.out" "tokens_processed: 2"
contains "$OUT_DIR/repeat-kv-after-failure.out" "kv_bound_to_prefill: true"
contains "$OUT_DIR/repeat-kv-after-failure.out" "kv_positions_written: 2"
contains "$OUT_DIR/repeat-kv-after-failure.out" "status: prefill-state-created"

"$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1 \
  >"$OUT_DIR/m7-token-graph-unchanged.out" 2>"$OUT_DIR/m7-token-graph-unchanged.err"
contains "$OUT_DIR/m7-token-graph-unchanged.out" "token_input_status: pass"
contains "$OUT_DIR/m7-token-graph-unchanged.out" "status: real-segment-graph-executed"

echo "cli prefill state: ok"
