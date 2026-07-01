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
grep 'fullmodel[[:space:]]*full model inventory and materialization planning' "$ROOT/commands.out"

"$YVEX_BIN" help fullmodel > "$ROOT/help.out"
grep 'usage: yvex fullmodel report --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'fullmodel report:' "$ROOT/help.out"
grep 'fullmodel materialization-plan:' "$ROOT/help.out"
grep 'fullmodel materialization proof:' "$ROOT/help.out"
grep 'not implemented until FULLMODEL.2' "$ROOT/help.out"
grep 'does not materialize' "$ROOT/help.out"
grep 'no full allocation' "$ROOT/help.out"
grep 'no full model execution' "$ROOT/help.out"
grep 'no inference' "$ROOT/help.out"
grep 'decode, logits, sampling, generation' "$ROOT/help.out"
grep 'no benchmark' "$ROOT/help.out"
grep 'no throughput' "$ROOT/help.out"

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

"$YVEX_BIN" fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed --backend cpu --registry "$REG" > "$ROOT/selected-plan.out"
grep 'fullmodel: materialization-plan' "$ROOT/selected-plan.out"
grep 'status: fullmodel-materialization-plan' "$ROOT/selected-plan.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-plan.out"
grep 'plan_status: partial' "$ROOT/selected-plan.out"
grep 'materialization_plan_ready: false' "$ROOT/selected-plan.out"
grep 'materialization_attempted: false' "$ROOT/selected-plan.out"
grep 'full_materialization_proof: false' "$ROOT/selected-plan.out"
grep 'full_model_execution: unsupported' "$ROOT/selected-plan.out"
grep 'generation_ready: false' "$ROOT/selected-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-plan.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-plan.out"
grep 'plan_kind: full-model-materialization' "$ROOT/selected-plan.out"
grep 'plan_source: tensor-inventory' "$ROOT/selected-plan.out"
grep 'plan_cleanup_required: true' "$ROOT/selected-plan.out"
grep 'backend_allocation_attempted: false' "$ROOT/selected-plan.out"
grep 'collection.embedding.present: true' "$ROOT/selected-plan.out"
grep 'collection.attention.blocker: attention collection missing' "$ROOT/selected-plan.out"
grep 'collection.mlp.blocker: MLP collection missing' "$ROOT/selected-plan.out"
grep 'collection.moe.blocker: MoE collection missing or not identified' "$ROOT/selected-plan.out"
grep 'collection.output.blocker: output collection missing' "$ROOT/selected-plan.out"
grep 'real transformer prefill not implemented' "$ROOT/selected-plan.out"
grep 'real attention-backed KV not implemented' "$ROOT/selected-plan.out"
grep 'real DeepSeek decode not implemented' "$ROOT/selected-plan.out"
grep 'real output-head logits not implemented' "$ROOT/selected-plan.out"
grep 'real vocabulary sampling not implemented' "$ROOT/selected-plan.out"
grep 'full runtime descriptor not implemented' "$ROOT/selected-plan.out"
grep 'proof_ready_for_fullmodel_2: false' "$ROOT/selected-plan.out"

"$YVEX_BIN" fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --registry "$REG" > "$ROOT/rmsnorm.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm.out"
grep 'normalization_tensors: 1' "$ROOT/rmsnorm.out"
grep 'missing_required_roles:' "$ROOT/rmsnorm.out"
grep 'real-transformer-prefill' "$ROOT/rmsnorm.out"

"$YVEX_BIN" fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --registry "$REG" > "$ROOT/rmsnorm-plan.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-plan.out"
grep 'target_class: selected-runtime-slice' "$ROOT/rmsnorm-plan.out"
grep 'plan_status: partial' "$ROOT/rmsnorm-plan.out"
grep 'materialization_plan_ready: false' "$ROOT/rmsnorm-plan.out"
grep 'collection.normalization.present: true' "$ROOT/rmsnorm-plan.out"
grep 'collection.normalization.tensor_count: 1' "$ROOT/rmsnorm-plan.out"
grep 'collection.attention.blocker: attention collection missing' "$ROOT/rmsnorm-plan.out"
grep 'collection.output.blocker: output collection missing' "$ROOT/rmsnorm-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-plan.out"
grep 'benchmark_status: not-measured' "$ROOT/rmsnorm-plan.out"

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

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-tensors 20 > "$ROOT/fullish-plan.out"
grep 'target_id: deepseek4-v4-flash' "$ROOT/fullish-plan.out"
grep 'target_class: full-runtime-model-planned' "$ROOT/fullish-plan.out"
grep 'plan_status: partial' "$ROOT/fullish-plan.out"
grep 'materialization_plan_ready: true' "$ROOT/fullish-plan.out"
grep 'materialization_attempted: false' "$ROOT/fullish-plan.out"
grep 'full_materialization_proof: false' "$ROOT/fullish-plan.out"
grep 'phase.0.name: preflight' "$ROOT/fullish-plan.out"
grep 'phase.1.name: artifact-identity' "$ROOT/fullish-plan.out"
grep 'phase.2.name: tensor-directory' "$ROOT/fullish-plan.out"
grep 'phase.3.name: tensor-range-validation' "$ROOT/fullish-plan.out"
grep 'phase.4.name: collection-grouping' "$ROOT/fullish-plan.out"
grep 'phase.5.name: backend-capability' "$ROOT/fullish-plan.out"
grep 'phase.6.name: host-residency' "$ROOT/fullish-plan.out"
grep 'phase.7.name: backend-residency' "$ROOT/fullish-plan.out"
grep 'phase.8.name: kv-residency' "$ROOT/fullish-plan.out"
grep 'phase.9.name: scratch-residency' "$ROOT/fullish-plan.out"
grep 'phase.10.name: cleanup' "$ROOT/fullish-plan.out"
grep 'collection.embedding.tensor_count: 1' "$ROOT/fullish-plan.out"
grep 'collection.normalization.tensor_count: 3' "$ROOT/fullish-plan.out"
grep 'collection.attention.tensor_count: 4' "$ROOT/fullish-plan.out"
grep 'collection.mlp.tensor_count: 3' "$ROOT/fullish-plan.out"
grep 'collection.output.tensor_count: 1' "$ROOT/fullish-plan.out"
grep 'collection.tokenizer-runtime-input.present: true' "$ROOT/fullish-plan.out"
grep 'backend_available: true' "$ROOT/fullish-plan.out"
grep 'backend_fit_status: unknown' "$ROOT/fullish-plan.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-plan.out"
grep 'cleanup_plan_required: true' "$ROOT/fullish-plan.out"
grep 'next_required_row: FULLMODEL.2' "$ROOT/fullish-plan.out"
grep 'proof_ready_for_fullmodel_2: false' "$ROOT/fullish-plan.out"
grep 'full_model_execution: unsupported' "$ROOT/fullish-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-plan.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-plan.out"

"$YVEX_BIN" fullmodel plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu > "$ROOT/fullish-plan-alias.out"
grep 'fullmodel: materialization-plan' "$ROOT/fullish-plan-alias.out"
grep 'plan_kind: full-model-materialization' "$ROOT/fullish-plan-alias.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cuda > "$ROOT/fullish-plan-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-plan-cuda.out"
grep 'backend_available:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_memory_known:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_fit_status:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-plan-cuda.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --residency resident > "$ROOT/resident.out"
grep 'residency: resident' "$ROOT/resident.out"
grep 'collection.embedding.placement: cpu-resident' "$ROOT/resident.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --residency host-staged > "$ROOT/host-staged.out"
grep 'residency: host-staged' "$ROOT/host-staged.out"
grep 'collection.embedding.placement: host-staged' "$ROOT/host-staged.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --residency ssd-staged > "$ROOT/ssd-staged.out"
grep 'residency: ssd-staged' "$ROOT/ssd-staged.out"
grep 'collection.embedding.placement: ssd-staged' "$ROOT/ssd-staged.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --residency hybrid > "$ROOT/hybrid.out"
grep 'residency: hybrid' "$ROOT/hybrid.out"
grep 'collection.embedding.placement: hybrid' "$ROOT/hybrid.out"

"$YVEX_BIN" fullmodel report --model glm-5.2-official-safetensors > "$ROOT/glm.out"
grep 'status: fullmodel-report-unsupported' "$ROOT/glm.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm.out"
grep 'generation: unsupported-full-model' "$ROOT/glm.out"

"$YVEX_BIN" fullmodel materialization-plan --model glm-5.2-official-safetensors > "$ROOT/glm-plan.out" && exit 1 || true
grep 'status: fullmodel-materialization-plan-unsupported' "$ROOT/glm-plan.out"
grep 'materialization_attempted: false' "$ROOT/glm-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-plan.out"

"$YVEX_BIN" fullmodel report --model "$ROOT/missing.gguf" > "$ROOT/missing.out" 2> "$ROOT/missing.err" && exit 1 || true
grep 'status: fullmodel-report-fail' "$ROOT/missing.out"
grep 'artifact_exists: false' "$ROOT/missing.out"
grep 'generation: unsupported-full-model' "$ROOT/missing.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$ROOT/missing.gguf" > "$ROOT/missing-plan.out" 2> "$ROOT/missing-plan.err" && exit 1 || true
grep 'status: fullmodel-materialization-plan-fail' "$ROOT/missing-plan.out"
grep 'artifact_exists: false' "$ROOT/missing-plan.out"
grep 'materialization_attempted: false' "$ROOT/missing-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/missing-plan.out"

printf 'not-gguf' > "$CORRUPT"
"$YVEX_BIN" fullmodel report --model "$CORRUPT" > "$ROOT/corrupt.out" 2> "$ROOT/corrupt.err" && exit 1 || true
grep 'status: fullmodel-report-fail' "$ROOT/corrupt.out"
grep 'tensor_inventory_status: failed' "$ROOT/corrupt.out"
grep 'generation_ready: false' "$ROOT/corrupt.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$CORRUPT" > "$ROOT/corrupt-plan.out" 2> "$ROOT/corrupt-plan.err" && exit 1 || true
grep 'status: fullmodel-materialization-plan-fail' "$ROOT/corrupt-plan.out"
grep 'tensor_inventory_status: failed' "$ROOT/corrupt-plan.out"
grep 'materialization_attempted: false' "$ROOT/corrupt-plan.out"
grep 'generation_ready: false' "$ROOT/corrupt-plan.out"

"$YVEX_BIN" fullmodel report --model "$FULLISH" --limit-tensors 0 > "$ROOT/invalid.out" 2> "$ROOT/invalid.err" && exit 1 || true
grep 'requires a positive integer' "$ROOT/invalid.err"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --residency magic > "$ROOT/bad-residency.out" 2> "$ROOT/bad-residency.err" && exit 1 || true
grep 'fullmodel --residency must be' "$ROOT/bad-residency.err"

for file in \
  "$ROOT/selected-plan.out" \
  "$ROOT/rmsnorm-plan.out" \
  "$ROOT/fullish-plan.out" \
  "$ROOT/fullish-plan-cuda.out" \
  "$ROOT/resident.out" \
  "$ROOT/host-staged.out" \
  "$ROOT/ssd-staged.out" \
  "$ROOT/hybrid.out"
do
  pattern='full_model_execution: tr''ue'
  ! grep "$pattern" "$file"
  pattern='full_model_materialization: tr''ue'
  ! grep "$pattern" "$file"
  pattern='full_model_generation: tr''ue'
  ! grep "$pattern" "$file"
  pattern='real_deepseek_generation: tr''ue'
  ! grep "$pattern" "$file"
  pattern='generation_rea''dy: true'
  ! grep "$pattern" "$file"
  pattern='execution_rea''dy: true'
  ! grep "$pattern" "$file"
  pattern='benchmark_status: mea''sured'
  ! grep "$pattern" "$file"
  pattern='tokens/''sec'
  ! grep "$pattern" "$file"
  pattern='DeepSeek generation imple''mented'
  ! grep "$pattern" "$file"
  pattern='inference rea''dy'
  ! grep "$pattern" "$file"
done
