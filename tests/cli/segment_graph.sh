#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/segment-graph}
SEGMENT_MODEL="$OUT_DIR/deepseek-test-segment-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
MISSING_RMS="$OUT_DIR/missing-rmsnorm.gguf"
WRONG_SHAPE="$OUT_DIR/wrong-rmsnorm-shape.gguf"
WRONG_DTYPE="$OUT_DIR/wrong-rmsnorm-dtype.gguf"
EPSILON_MISSING="$OUT_DIR/epsilon-missing.gguf"
M5_MODEL="$OUT_DIR/deepseek-test-partial-embed-F16-noimatrix-yvex-v1.gguf"

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
    ! grep -F "$text" "$file" >/dev/null || fail "$file unexpectedly contained: $text"
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

python3 - "$OUT_DIR" <<'PY'
import pathlib
import struct
import sys

out = pathlib.Path(sys.argv[1])
magic = 0x46554747
version = 3
string = 8
uint32 = 4
float32 = 6
ggml_f16 = 1
ggml_i32 = 26

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
    data += b"".join(metadata)
    data += b"".join(tensors)
    data += b"\0" * ((-len(data)) % alignment)
    return data + payload + (b"\0" * ((-len(payload)) % alignment))

base_meta = [
    kv_string("general.architecture", "deepseek"),
    kv_string("general.name", "m6-segment-fixture"),
    kv_u32("general.alignment", 32),
    kv_u32("general.file_type", 1),
]
epsilon_meta = base_meta + [
    kv_f32("deepseek2.attention.layer_norm_rms_epsilon", 0.000001),
]

embed_payload = b"".join(f16(i) for i in range(32))
rms_f16_payload = b"".join(f16(1) for _ in range(4))
rms_i32_payload = b"".join(struct.pack("<i", i + 1) for i in range(4))

(out / "deepseek-test-segment-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf").write_bytes(
    file_bytes(
        epsilon_meta,
        [
            tensor("token_embd.weight", [4, 8], ggml_f16, 0),
            tensor("blk.0.attn_norm.weight", [4], ggml_f16, 64),
        ],
        embed_payload + rms_f16_payload,
    )
)
(out / "missing-rmsnorm.gguf").write_bytes(
    file_bytes(
        epsilon_meta,
        [tensor("token_embd.weight", [4, 8], ggml_f16, 0)],
        embed_payload,
    )
)
(out / "wrong-rmsnorm-shape.gguf").write_bytes(
    file_bytes(
        epsilon_meta,
        [
            tensor("token_embd.weight", [4, 8], ggml_f16, 0),
            tensor("blk.0.attn_norm.weight", [3], ggml_f16, 64),
        ],
        embed_payload + b"".join(f16(1) for _ in range(3)),
    )
)
(out / "wrong-rmsnorm-dtype.gguf").write_bytes(
    file_bytes(
        epsilon_meta,
        [
            tensor("token_embd.weight", [4, 8], ggml_f16, 0),
            tensor("blk.0.attn_norm.weight", [4], ggml_i32, 64),
        ],
        embed_payload + rms_i32_payload,
    )
)
(out / "epsilon-missing.gguf").write_bytes(
    file_bytes(
        base_meta,
        [
            tensor("token_embd.weight", [4, 8], ggml_f16, 0),
            tensor("blk.0.attn_norm.weight", [4], ggml_f16, 64),
        ],
        embed_payload + rms_f16_payload,
    )
)
PY

"$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/segment-cpu.out" 2>"$OUT_DIR/segment-cpu.err"
contains "$OUT_DIR/segment-cpu.out" "graph_integrity_guard: pass"
contains "$OUT_DIR/segment-cpu.out" "graph_kind: selected-embedding-rmsnorm"
contains "$OUT_DIR/segment-cpu.out" "segment_graph_executed: true"
contains "$OUT_DIR/segment-cpu.out" "segment_backend: cpu"
contains "$OUT_DIR/segment-cpu.out" "segment_ops: 2"
contains "$OUT_DIR/segment-cpu.out" "segment_op_0: embed"
contains "$OUT_DIR/segment-cpu.out" "segment_op_1: rms_norm"
contains "$OUT_DIR/segment-cpu.out" "rmsnorm_tensor: blk.0.attn_norm.weight"
contains "$OUT_DIR/segment-cpu.out" "rmsnorm_epsilon_key: deepseek2.attention.layer_norm_rms_epsilon"
contains "$OUT_DIR/segment-cpu.out" "segment_memory_plan: explicit"
contains "$OUT_DIR/segment-cpu.out" "segment_intermediate_bytes: 16"
contains "$OUT_DIR/segment-cpu.out" "segment_output_count: 4"
contains "$OUT_DIR/segment-cpu.out" "segment_output_bytes: 16"
contains "$OUT_DIR/segment-cpu.out" "segment_reference_bytes: 16"
contains "$OUT_DIR/segment-cpu.out" "segment_max_abs_diff: 0"
contains "$OUT_DIR/segment-cpu.out" "execution_ready: false"
contains "$OUT_DIR/segment-cpu.out" "prefill_ready: false"
contains "$OUT_DIR/segment-cpu.out" "logits_ready: false"
contains "$OUT_DIR/segment-cpu.out" "generation: unsupported"
contains "$OUT_DIR/segment-cpu.out" "status: real-segment-graph-executed"

"$YVEX_BIN" graph --model "$MISSING_RMS" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/missing-rms.out" 2>"$OUT_DIR/missing-rms.err" && fail "missing RMSNorm unexpectedly passed" || true
contains "$OUT_DIR/missing-rms.out" "graph_integrity_guard: fail"
contains "$OUT_DIR/missing-rms.out" "dispatch_attempted: false"
contains "$OUT_DIR/missing-rms.out" "status: graph-integrity-fail"
contains "$OUT_DIR/missing-rms.err" "rmsnorm-tensor-missing"

"$YVEX_BIN" graph --model "$WRONG_SHAPE" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/wrong-shape.out" 2>"$OUT_DIR/wrong-shape.err" && fail "wrong RMSNorm shape unexpectedly passed" || true
contains "$OUT_DIR/wrong-shape.out" "dispatch_attempted: false"
contains "$OUT_DIR/wrong-shape.err" "rmsnorm-shape-invalid"

"$YVEX_BIN" graph --model "$WRONG_DTYPE" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/wrong-dtype.out" 2>"$OUT_DIR/wrong-dtype.err" && fail "wrong RMSNorm dtype unexpectedly passed" || true
contains "$OUT_DIR/wrong-dtype.out" "dispatch_attempted: false"
contains "$OUT_DIR/wrong-dtype.err" "rmsnorm-dtype-invalid"

"$YVEX_BIN" graph --model "$EPSILON_MISSING" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/epsilon-missing.out" 2>"$OUT_DIR/epsilon-missing.err" && fail "missing epsilon unexpectedly passed" || true
contains "$OUT_DIR/epsilon-missing.out" "dispatch_attempted: false"
contains "$OUT_DIR/epsilon-missing.err" "rmsnorm-epsilon-missing"

YVEX_TEST_SEGMENT_MEMORY_PLAN_OVERFLOW=1 \
  "$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
    --execute-segment --segment embedding-rmsnorm --partial-token 0 \
    >"$OUT_DIR/memory-overflow.out" 2>"$OUT_DIR/memory-overflow.err" && fail "memory overflow unexpectedly passed" || true
contains "$OUT_DIR/memory-overflow.out" "output_allocation_attempted: false"
contains "$OUT_DIR/memory-overflow.err" "segment-memory-plan-overflow"

YVEX_TEST_FAIL_GRAPH_AFTER_OP0=1 \
  "$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
    --execute-segment --segment embedding-rmsnorm --partial-token 0 \
    >"$OUT_DIR/fail-after-op0.out" 2>"$OUT_DIR/fail-after-op0.err" && fail "op0 failure unexpectedly passed" || true
contains "$OUT_DIR/fail-after-op0.out" "dispatch_attempted: true"
contains "$OUT_DIR/fail-after-op0.out" "cleanup_attempted: true"
contains "$OUT_DIR/fail-after-op0.out" "cleanup_status: pass"
contains "$OUT_DIR/fail-after-op0.out" "status: graph-failed-cleaned"

"$YVEX_BIN" graph --model "$SEGMENT_MODEL" --backend cpu \
  --execute-segment --segment embedding-rmsnorm --partial-token 0 \
  >"$OUT_DIR/repeat-after-failure.out" 2>"$OUT_DIR/repeat-after-failure.err"
contains "$OUT_DIR/repeat-after-failure.out" "status: real-segment-graph-executed"

"$YVEX_BIN" gguf-emit controlled \
  --out "$M5_MODEL" \
  --model-name m6-m5-regression-f16 \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit-m5.out" 2>"$OUT_DIR/emit-m5.err"
"$YVEX_BIN" graph --model "$M5_MODEL" --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/m5-partial.out" 2>"$OUT_DIR/m5-partial.err"
contains "$OUT_DIR/m5-partial.out" "status: real-partial-graph-executed"
contains "$OUT_DIR/m5-partial.out" "partial_output_checksum: 10881421815077182739"
contains "$OUT_DIR/m5-partial.out" "partial_reference_checksum: 10881421815077182739"
contains "$OUT_DIR/m5-partial.out" "partial_max_abs_diff: 0"
not_contains "$OUT_DIR/m5-partial.out" "segment_graph_executed: true"

echo "cli segment graph: ok"
