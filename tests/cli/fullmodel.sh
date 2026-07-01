#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/fullmodel-cli}
REG="$ROOT/registry/models.local.json"
SELECTED="$ROOT/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
RMSNORM="$ROOT/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
FULLISH="$ROOT/deepseek4-v4-flash-fullish-F32-noimatrix-yvex-v1.gguf"
CORRUPT="$ROOT/corrupt.gguf"

rm -rf "$ROOT"
mkdir -p "$ROOT" "$ROOT/registry"

"$YVEX_BIN" gguf-emit controlled \
  --out "$SELECTED" \
  --model-name fullmodel-selected-test \
  --arch deepseek \
  --overwrite >/dev/null

python3 - "$RMSNORM" "$FULLISH" <<'PY'
import struct
import sys

STRING = 8
ARRAY = 9
UINT32 = 4
FLOAT32 = 6
INT32 = 5
GGML_TYPE_F32 = 0


def u32(v):
    return struct.pack("<I", v)


def u64(v):
    return struct.pack("<Q", v)


def gguf_string(v):
    if isinstance(v, str):
        v = v.encode("utf-8")
    return u64(len(v)) + v


def scalar(kind, value):
    if kind == STRING:
        return gguf_string(value)
    if kind == UINT32:
        return u32(value)
    if kind == FLOAT32:
        return struct.pack("<f", value)
    if kind == INT32:
        return struct.pack("<i", value)
    raise ValueError(kind)


def kv(key, kind, value):
    return gguf_string(key) + u32(kind) + scalar(kind, value)


def kv_array(key, kind, values):
    out = gguf_string(key) + u32(ARRAY) + u32(kind) + u64(len(values))
    for value in values:
        out += scalar(kind, value)
    return out


def tensor_info(name, dims, kind, offset):
    out = gguf_string(name) + u32(len(dims))
    for dim in dims:
        out += u64(dim)
    return out + u32(kind) + u64(offset)


def align(n, a=32):
    return (n + (a - 1)) // a * a


def write_gguf(path, tensors):
    metadata = [
        kv("general.architecture", STRING, "deepseek"),
        kv("general.name", STRING, "yvex-fullmodel-test"),
        kv("deepseek.context_length", UINT32, 128),
        kv("general.file_type", UINT32, 0),
        kv("general.alignment", UINT32, 32),
        kv("tokenizer.ggml.model", STRING, "yvex-fixture-simple"),
        kv_array("tokenizer.ggml.tokens", STRING, ["<unk>", "<bos>", "<eos>", "hello"]),
        kv_array("tokenizer.ggml.scores", FLOAT32, [0.0, 0.0, 0.0, 0.0]),
        kv_array("tokenizer.ggml.token_type", INT32, [2, 3, 3, 1]),
    ]
    infos = []
    offset = 0
    data = bytearray()
    for name, dims in tensors:
        elems = 1
        for dim in dims:
            elems *= dim
        size = elems * 4
        offset = align(offset)
        if len(data) < offset:
            data.extend(b"\x00" * (offset - len(data)))
        infos.append(tensor_info(name, dims, GGML_TYPE_F32, offset))
        data.extend(b"\x00" * size)
        offset += size

    blob = b"GGUF" + u32(3) + u64(len(infos)) + u64(len(metadata))
    blob += b"".join(metadata) + b"".join(infos)
    blob += b"\x00" * (align(len(blob)) - len(blob))
    blob += bytes(data)
    with open(path, "wb") as f:
        f.write(blob)


write_gguf(sys.argv[1], [
    ("token_embd.weight", [4, 8]),
    ("blk.0.attn_norm.weight", [8]),
])

write_gguf(sys.argv[2], [
    ("token_embd.weight", [4, 8]),
    ("blk.0.attn_norm.weight", [8]),
    ("blk.0.attn_q.weight", [8, 8]),
    ("blk.0.attn_k.weight", [8, 8]),
    ("blk.0.attn_v.weight", [8, 8]),
    ("blk.0.attn_output.weight", [8, 8]),
    ("blk.0.ffn_norm.weight", [8]),
    ("blk.0.ffn_gate.weight", [8, 16]),
    ("blk.0.ffn_up.weight", [8, 16]),
    ("blk.0.ffn_down.weight", [16, 8]),
    ("output_norm.weight", [8]),
    ("output.weight", [8, 4]),
])
PY

"$YVEX_BIN" models add --path "$SELECTED" --alias deepseek4-v4-flash-selected-embed --support-level selected-tensor-materialized --registry "$REG" > "$ROOT/add-selected.out"
"$YVEX_BIN" models add --path "$RMSNORM" --alias deepseek4-v4-flash-selected-embed-rmsnorm --support-level selected-tensor-materialized --registry "$REG" > "$ROOT/add-rmsnorm.out"
grep 'status: models-added' "$ROOT/add-selected.out"
grep 'status: models-added' "$ROOT/add-rmsnorm.out"

"$YVEX_BIN" commands > "$ROOT/commands.out"
grep 'fullmodel[[:space:]]*full model tensor inventory and placement report' "$ROOT/commands.out"

"$YVEX_BIN" help fullmodel > "$ROOT/help.out"
grep 'usage: yvex fullmodel report --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'does not materialize' "$ROOT/help.out"
grep 'decode, logits, sampling, generation' "$ROOT/help.out"

"$YVEX_BIN" fullmodel report --model deepseek4-v4-flash-selected-embed --backend cpu --registry "$REG" > "$ROOT/selected.out"
grep 'status: fullmodel-report' "$ROOT/selected.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected.out"
grep 'fullmodel_inventory: incomplete' "$ROOT/selected.out"
grep 'required_role_coverage: partial' "$ROOT/selected.out"
grep 'full_runtime_model: false' "$ROOT/selected.out"
grep 'collection_detected: yes' "$ROOT/selected.out"
grep 'runtime_consumer: unsupported' "$ROOT/selected.out"
grep 'full_model_execution: unsupported' "$ROOT/selected.out"
grep 'generation_ready: false' "$ROOT/selected.out"
grep 'generation: unsupported-full-model' "$ROOT/selected.out"
grep 'benchmark_status: not-measured' "$ROOT/selected.out"

"$YVEX_BIN" fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --registry "$REG" > "$ROOT/rmsnorm.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm.out"
grep 'normalization_tensors: 1' "$ROOT/rmsnorm.out"
grep 'missing_required_roles:' "$ROOT/rmsnorm.out"
grep 'real-transformer-prefill' "$ROOT/rmsnorm.out"

"$YVEX_BIN" fullmodel report --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-tensors 3 > "$ROOT/fullish.out"
grep 'target_id: deepseek4-v4-flash' "$ROOT/fullish.out"
grep 'target_class: full-runtime-model-planned' "$ROOT/fullish.out"
grep 'architecture: deepseek' "$ROOT/fullish.out"
grep 'qtype_summary: F32:' "$ROOT/fullish.out"
grep 'dtype_summary: F32:' "$ROOT/fullish.out"
grep 'attention_tensors: 4' "$ROOT/fullish.out"
grep 'mlp_tensors: 3' "$ROOT/fullish.out"
grep 'output_tensors: 1' "$ROOT/fullish.out"
grep 'tokenizer_tensors: 1' "$ROOT/fullish.out"
grep 'largest_tensor_2:' "$ROOT/fullish.out"
! grep 'largest_tensor_3:' "$ROOT/fullish.out"
grep 'full_model_materialization: planned' "$ROOT/fullish.out"

"$YVEX_BIN" fullmodel report --model "$FULLISH" --target deepseek4-v4-flash --backend cuda > "$ROOT/fullish-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-cuda.out"
grep 'cuda_available:' "$ROOT/fullish-cuda.out"
grep 'cuda_memory_status:' "$ROOT/fullish-cuda.out"
grep 'cuda_placement:' "$ROOT/fullish-cuda.out"
grep 'residency_plan: report-only-no-allocation' "$ROOT/fullish-cuda.out"

"$YVEX_BIN" fullmodel report --model glm-5.2-official-safetensors > "$ROOT/glm.out"
grep 'status: fullmodel-report-unsupported' "$ROOT/glm.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm.out"
grep 'generation: unsupported-full-model' "$ROOT/glm.out"

"$YVEX_BIN" fullmodel report --model "$ROOT/missing.gguf" > "$ROOT/missing.out" 2> "$ROOT/missing.err" && exit 1 || true
grep 'status: fullmodel-report-fail' "$ROOT/missing.out"
grep 'artifact_exists: false' "$ROOT/missing.out"
grep 'generation: unsupported-full-model' "$ROOT/missing.out"

printf 'not-gguf' > "$CORRUPT"
"$YVEX_BIN" fullmodel report --model "$CORRUPT" > "$ROOT/corrupt.out" 2> "$ROOT/corrupt.err" && exit 1 || true
grep 'status: fullmodel-report-fail' "$ROOT/corrupt.out"
grep 'tensor_inventory_status: failed' "$ROOT/corrupt.out"
grep 'generation_ready: false' "$ROOT/corrupt.out"

"$YVEX_BIN" fullmodel report --model "$FULLISH" --limit-tensors 0 > "$ROOT/invalid.out" 2> "$ROOT/invalid.err" && exit 1 || true
grep 'requires a positive integer' "$ROOT/invalid.err"
