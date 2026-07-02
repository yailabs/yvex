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
grep 'fullmodel[[:space:]]*full model inventory, materialization, and runtime descriptor reports' "$ROOT/commands.out"

"$YVEX_BIN" help fullmodel > "$ROOT/help.out"
grep 'usage: yvex fullmodel report --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel materialize --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel descriptor --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'fullmodel report:' "$ROOT/help.out"
grep 'fullmodel materialization-plan:' "$ROOT/help.out"
grep 'fullmodel materialization proof:' "$ROOT/help.out"
grep 'fullmodel descriptor:' "$ROOT/help.out"
grep 'controlled proof over a tiny full-ish GGUF tensor set' "$ROOT/help.out"
grep 'allocates and releases only the bounded required proof tensors' "$ROOT/help.out"
grep 'planning/reporting boundary for tensor roles' "$ROOT/help.out"
grep 'does not execute the model' "$ROOT/help.out"
grep 'does not.*generate' "$ROOT/help.out"
grep 'does not.*benchmark' "$ROOT/help.out"
grep 'no full model execution' "$ROOT/help.out"
grep 'no inference readiness' "$ROOT/help.out"
grep 'no DeepSeek generation' "$ROOT/help.out"
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
grep 'runtime family adapter not implemented' "$ROOT/selected-plan.out"
grep 'proof_ready_for_fullmodel_2: false' "$ROOT/selected-plan.out"

"$YVEX_BIN" fullmodel materialize --model deepseek4-v4-flash-selected-embed --backend cpu --registry "$REG" > "$ROOT/selected-materialize.out"
grep 'fullmodel: materialize' "$ROOT/selected-materialize.out"
grep 'status: fullmodel-materialize-refused' "$ROOT/selected-materialize.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-materialize.out"
grep 'required_role_coverage: partial' "$ROOT/selected-materialize.out"
grep 'full_model_materialization: refused-selected-runtime-slice' "$ROOT/selected-materialize.out"
grep 'full_model_materialization_proof: refused' "$ROOT/selected-materialize.out"
grep 'full_model_execution: unsupported' "$ROOT/selected-materialize.out"
grep 'generation_ready: false' "$ROOT/selected-materialize.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-materialize.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-materialize.out"
grep 'failed_phase: role-coverage' "$ROOT/selected-materialize.out"
grep 'failed_reason: selected-runtime-slice' "$ROOT/selected-materialize.out"
grep 'cleanup_attempted: true' "$ROOT/selected-materialize.out"
grep 'owned_state_released: true' "$ROOT/selected-materialize.out"

"$YVEX_BIN" fullmodel descriptor --model deepseek4-v4-flash-selected-embed --backend cpu --registry "$REG" > "$ROOT/selected-descriptor.out"
grep 'fullmodel: descriptor' "$ROOT/selected-descriptor.out"
grep 'status: fullmodel-descriptor' "$ROOT/selected-descriptor.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-descriptor.out"
grep 'runtime_descriptor: report-only' "$ROOT/selected-descriptor.out"
grep 'runtime_descriptor_status: partial' "$ROOT/selected-descriptor.out"
grep 'runtime_descriptor_kind: fullmodel-planning' "$ROOT/selected-descriptor.out"
grep 'full_runtime_model: false' "$ROOT/selected-descriptor.out"
grep 'full_model_execution: unsupported' "$ROOT/selected-descriptor.out"
grep 'generation_ready: false' "$ROOT/selected-descriptor.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-descriptor.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-descriptor.out"
grep 'tensor_role_map_status: partial' "$ROOT/selected-descriptor.out"
grep 'tensor_collection_map_status: partial' "$ROOT/selected-descriptor.out"
grep 'required_role_coverage: partial' "$ROOT/selected-descriptor.out"
grep 'role.token_embedding.status: present' "$ROOT/selected-descriptor.out"
grep 'role.q_projection.status: missing' "$ROOT/selected-descriptor.out"
grep 'role.output_head.status: missing' "$ROOT/selected-descriptor.out"
grep 'embedding_descriptor: present' "$ROOT/selected-descriptor.out"
grep 'attention_descriptor: missing' "$ROOT/selected-descriptor.out"
grep 'mlp_descriptor: missing' "$ROOT/selected-descriptor.out"
grep 'output_descriptor: missing' "$ROOT/selected-descriptor.out"
grep 'graph.attention_primitive: implemented-fixture' "$ROOT/selected-descriptor.out"
grep 'graph.full_transformer_attention: missing-tensor' "$ROOT/selected-descriptor.out"
grep 'prefill_descriptor: unsupported-full-transformer-prefill' "$ROOT/selected-descriptor.out"
grep 'kv_write_ready: false' "$ROOT/selected-descriptor.out"
grep 'real_output_head_logits: false' "$ROOT/selected-descriptor.out"
grep 'logits_ready: false' "$ROOT/selected-descriptor.out"
grep 'prefill_ready: false' "$ROOT/selected-descriptor.out"
grep 'decode_ready: false' "$ROOT/selected-descriptor.out"
grep 'sampling_ready: false' "$ROOT/selected-descriptor.out"
grep 'full runtime tensor set incomplete' "$ROOT/selected-descriptor.out"

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

"$YVEX_BIN" fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --registry "$REG" > "$ROOT/rmsnorm-materialize.out"
grep 'status: fullmodel-materialize-refused' "$ROOT/rmsnorm-materialize.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-materialize.out"
grep 'full_model_materialization_proof: refused' "$ROOT/rmsnorm-materialize.out"
grep 'failed_reason: selected-runtime-slice' "$ROOT/rmsnorm-materialize.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-materialize.out"

"$YVEX_BIN" fullmodel descriptor --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --registry "$REG" > "$ROOT/rmsnorm-descriptor.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-descriptor.out"
grep 'runtime_descriptor_status: partial' "$ROOT/rmsnorm-descriptor.out"
grep 'role.token_embedding.status: present' "$ROOT/rmsnorm-descriptor.out"
grep 'role.attention_norm.status: present' "$ROOT/rmsnorm-descriptor.out"
grep 'normalization_descriptor: present' "$ROOT/rmsnorm-descriptor.out"
grep 'attention_descriptor: missing' "$ROOT/rmsnorm-descriptor.out"
grep 'output_descriptor: missing' "$ROOT/rmsnorm-descriptor.out"
grep 'prefill_ready: false' "$ROOT/rmsnorm-descriptor.out"
grep 'decode_ready: false' "$ROOT/rmsnorm-descriptor.out"
grep 'logits_ready: false' "$ROOT/rmsnorm-descriptor.out"
grep 'sampling_ready: false' "$ROOT/rmsnorm-descriptor.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-descriptor.out"

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

"$YVEX_BIN" fullmodel descriptor --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-tensors 40 --format text --include-blockers --include-placement --include-graph --include-kv --include-logits > "$ROOT/fullish-descriptor.out"
grep 'status: fullmodel-descriptor' "$ROOT/fullish-descriptor.out"
grep 'target_id: deepseek4-v4-flash' "$ROOT/fullish-descriptor.out"
grep 'target_class: full-runtime-model-planned' "$ROOT/fullish-descriptor.out"
grep 'runtime_descriptor_status: complete' "$ROOT/fullish-descriptor.out"
grep 'runtime_descriptor_kind: fullmodel-planning' "$ROOT/fullish-descriptor.out"
grep 'required_role_coverage: complete' "$ROOT/fullish-descriptor.out"
grep 'missing_required_roles: none' "$ROOT/fullish-descriptor.out"
grep 'role.q_projection.status: present' "$ROOT/fullish-descriptor.out"
grep 'role.k_projection.status: present' "$ROOT/fullish-descriptor.out"
grep 'role.v_projection.status: present' "$ROOT/fullish-descriptor.out"
grep 'role.o_projection.status: present' "$ROOT/fullish-descriptor.out"
grep 'role.mlp_gate.status: present' "$ROOT/fullish-descriptor.out"
grep 'role.output_head.status: present' "$ROOT/fullish-descriptor.out"
grep 'attention_descriptor: present' "$ROOT/fullish-descriptor.out"
grep 'mlp_descriptor: present' "$ROOT/fullish-descriptor.out"
grep 'output_descriptor: present' "$ROOT/fullish-descriptor.out"
grep 'tokenizer_descriptor: present' "$ROOT/fullish-descriptor.out"
grep 'graph_requirements_status: blocked' "$ROOT/fullish-descriptor.out"
grep 'graph.attention_primitive: implemented-fixture' "$ROOT/fullish-descriptor.out"
grep 'graph.full_transformer_attention: unsupported' "$ROOT/fullish-descriptor.out"
grep 'prefill.requires_real_kv_writes: true' "$ROOT/fullish-descriptor.out"
grep 'kv.runtime_status: unsupported' "$ROOT/fullish-descriptor.out"
grep 'decode.current_status: unsupported' "$ROOT/fullish-descriptor.out"
grep 'output_head_present: true' "$ROOT/fullish-descriptor.out"
grep 'logits_buffer_required: true' "$ROOT/fullish-descriptor.out"
grep 'real_output_head_logits: false' "$ROOT/fullish-descriptor.out"
grep 'backend.full_transformer_integration: unsupported' "$ROOT/fullish-descriptor.out"
grep 'full_model_execution: unsupported' "$ROOT/fullish-descriptor.out"
grep 'generation_ready: false' "$ROOT/fullish-descriptor.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-descriptor.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-descriptor.out"

"$YVEX_BIN" fullmodel descriptor --model "$FULLISH" --target deepseek4-v4-flash --backend cuda > "$ROOT/fullish-descriptor-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend.cuda.available:' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend.full_transformer_integration: unsupported' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-descriptor-cuda.out"

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

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-bytes 1048576 > "$ROOT/fullish-materialize.out"
grep 'fullmodel: materialize' "$ROOT/fullish-materialize.out"
grep 'status: fullmodel-materialize-pass' "$ROOT/fullish-materialize.out"
grep 'target_id: deepseek4-v4-flash' "$ROOT/fullish-materialize.out"
grep 'target_class: full-runtime-model-planned' "$ROOT/fullish-materialize.out"
grep 'required_role_coverage: complete' "$ROOT/fullish-materialize.out"
grep 'missing_required_roles: none' "$ROOT/fullish-materialize.out"
grep 'memory_budget_status: pass' "$ROOT/fullish-materialize.out"
grep 'backend_preflight_status: pass' "$ROOT/fullish-materialize.out"
grep 'materialization_mode: controlled-fullmodel-proof' "$ROOT/fullish-materialize.out"
grep 'full_model_materialization: controlled-tiny-proof' "$ROOT/fullish-materialize.out"
grep 'full_model_materialization_proof: pass' "$ROOT/fullish-materialize.out"
grep 'full_model_execution: unsupported' "$ROOT/fullish-materialize.out"
grep 'generation_ready: false' "$ROOT/fullish-materialize.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-materialize.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-materialize.out"
grep 'phase: complete' "$ROOT/fullish-materialize.out"
grep 'failed_phase: none' "$ROOT/fullish-materialize.out"
grep 'cleanup_attempted: true' "$ROOT/fullish-materialize.out"
grep 'cleanup_status: pass' "$ROOT/fullish-materialize.out"
grep 'cleanup_idempotent: true' "$ROOT/fullish-materialize.out"
grep 'owned_state_released: true' "$ROOT/fullish-materialize.out"
grep 'partial_materialization: false' "$ROOT/fullish-materialize.out"
grep 'materialized_tensor_count: 12' "$ROOT/fullish-materialize.out"
grep 'required_tensor_count: 12' "$ROOT/fullish-materialize.out"

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --dry-run --limit-bytes 1048576 > "$ROOT/fullish-materialize-dry-run.out"
grep 'status: fullmodel-materialize-dry-run' "$ROOT/fullish-materialize-dry-run.out"
grep 'dry_run: true' "$ROOT/fullish-materialize-dry-run.out"
grep 'full_model_materialization_proof: planned' "$ROOT/fullish-materialize-dry-run.out"
grep 'materialized_tensor_count: 0' "$ROOT/fullish-materialize-dry-run.out"

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --plan-only --limit-bytes 1048576 > "$ROOT/fullish-materialize-plan-only.out"
grep 'status: fullmodel-materialize-plan-only' "$ROOT/fullish-materialize-plan-only.out"
grep 'plan_only: true' "$ROOT/fullish-materialize-plan-only.out"
grep 'full_model_materialization_proof: planned' "$ROOT/fullish-materialize-plan-only.out"

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-bytes 1024 > "$ROOT/fullish-materialize-limit.out" 2> "$ROOT/fullish-materialize-limit.err" && exit 1 || true
grep 'status: fullmodel-materialize-fail' "$ROOT/fullish-materialize-limit.out"
grep 'memory_budget_status: fail' "$ROOT/fullish-materialize-limit.out"
grep 'failed_phase: memory-budget' "$ROOT/fullish-materialize-limit.out"
grep 'failed_reason: byte-limit' "$ROOT/fullish-materialize-limit.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-materialize-limit.out"

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cpu --limit-bytes 1048576 --fail-after-phase backend-preflight > "$ROOT/fullish-materialize-injected.out" 2> "$ROOT/fullish-materialize-injected.err" && exit 1 || true
grep 'status: fullmodel-materialize-fail' "$ROOT/fullish-materialize-injected.out"
grep 'failed_phase: backend-preflight' "$ROOT/fullish-materialize-injected.out"
grep 'failed_reason: injected-failure' "$ROOT/fullish-materialize-injected.out"
grep 'cleanup_attempted: true' "$ROOT/fullish-materialize-injected.out"

"$YVEX_BIN" fullmodel plan --model "$FULLISH" --target deepseek4-v4-flash --backend cpu > "$ROOT/fullish-plan-alias.out"
grep 'fullmodel: materialization-plan' "$ROOT/fullish-plan-alias.out"
grep 'plan_kind: full-model-materialization' "$ROOT/fullish-plan-alias.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --target deepseek4-v4-flash --backend cuda > "$ROOT/fullish-plan-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-plan-cuda.out"
grep 'backend_available:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_memory_known:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_fit_status:' "$ROOT/fullish-plan-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-plan-cuda.out"

if "$YVEX_BIN" cuda-info >/dev/null 2>&1; then
  "$YVEX_BIN" fullmodel materialize --model "$FULLISH" --target deepseek4-v4-flash --backend cuda --limit-bytes 1048576 > "$ROOT/fullish-materialize-cuda.out"
  grep 'status: fullmodel-materialize-pass' "$ROOT/fullish-materialize-cuda.out"
  grep 'backend: cuda' "$ROOT/fullish-materialize-cuda.out"
  grep 'cuda_resident_bytes:' "$ROOT/fullish-materialize-cuda.out"
  grep 'full_model_materialization_proof: pass' "$ROOT/fullish-materialize-cuda.out"
  grep 'generation: unsupported-full-model' "$ROOT/fullish-materialize-cuda.out"
fi

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

"$YVEX_BIN" fullmodel materialize --model glm-5.2-official-safetensors > "$ROOT/glm-materialize.out" && exit 1 || true
grep 'status: fullmodel-materialize-unsupported' "$ROOT/glm-materialize.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-materialize.out"
grep 'full_model_materialization_proof: unsupported' "$ROOT/glm-materialize.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-materialize.out"

"$YVEX_BIN" fullmodel descriptor --model glm-5.2-official-safetensors --backend cpu > "$ROOT/glm-descriptor.out" && exit 1 || true
grep 'status: fullmodel-descriptor-unsupported' "$ROOT/glm-descriptor.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-descriptor.out"
grep 'tensor_inventory_status: not-performed-source-only-target' "$ROOT/glm-descriptor.out"
grep 'runtime_descriptor_status: unsupported' "$ROOT/glm-descriptor.out"
grep 'full_model_execution: unsupported' "$ROOT/glm-descriptor.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-descriptor.out"

"$YVEX_BIN" fullmodel report --model "$ROOT/missing.gguf" > "$ROOT/missing.out" 2> "$ROOT/missing.err" && exit 1 || true
grep 'status: fullmodel-report-fail' "$ROOT/missing.out"
grep 'artifact_exists: false' "$ROOT/missing.out"
grep 'generation: unsupported-full-model' "$ROOT/missing.out"

"$YVEX_BIN" fullmodel materialization-plan --model "$ROOT/missing.gguf" > "$ROOT/missing-plan.out" 2> "$ROOT/missing-plan.err" && exit 1 || true
grep 'status: fullmodel-materialization-plan-fail' "$ROOT/missing-plan.out"
grep 'artifact_exists: false' "$ROOT/missing-plan.out"
grep 'materialization_attempted: false' "$ROOT/missing-plan.out"
grep 'generation: unsupported-full-model' "$ROOT/missing-plan.out"

"$YVEX_BIN" fullmodel materialize --model "$ROOT/missing.gguf" > "$ROOT/missing-materialize.out" 2> "$ROOT/missing-materialize.err" && exit 1 || true
grep 'status: fullmodel-materialize-fail' "$ROOT/missing-materialize.out"
grep 'failed_phase: resolve-model' "$ROOT/missing-materialize.out"
grep 'failed_reason: artifact path does not exist' "$ROOT/missing-materialize.out"
grep 'generation: unsupported-full-model' "$ROOT/missing-materialize.out"

"$YVEX_BIN" fullmodel descriptor --model "$ROOT/missing.gguf" > "$ROOT/missing-descriptor.out" 2> "$ROOT/missing-descriptor.err" && exit 1 || true
grep 'status: fullmodel-descriptor-fail' "$ROOT/missing-descriptor.out"
grep 'runtime_descriptor_status: fail' "$ROOT/missing-descriptor.out"
grep 'descriptor_phase.1.name: resolve-model' "$ROOT/missing-descriptor.out"
grep 'descriptor_phase.1.status: fail' "$ROOT/missing-descriptor.out"
grep 'reason: artifact path does not exist' "$ROOT/missing-descriptor.out"

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

"$YVEX_BIN" fullmodel materialize --model "$CORRUPT" > "$ROOT/corrupt-materialize.out" 2> "$ROOT/corrupt-materialize.err" && exit 1 || true
grep 'status: fullmodel-materialize-fail' "$ROOT/corrupt-materialize.out"
grep 'failed_phase: tensor-inventory' "$ROOT/corrupt-materialize.out"
grep 'generation_ready: false' "$ROOT/corrupt-materialize.out"

"$YVEX_BIN" fullmodel descriptor --model "$CORRUPT" > "$ROOT/corrupt-descriptor.out" 2> "$ROOT/corrupt-descriptor.err" && exit 1 || true
grep 'status: fullmodel-descriptor-fail' "$ROOT/corrupt-descriptor.out"
grep 'tensor_inventory_status: failed' "$ROOT/corrupt-descriptor.out"
grep 'runtime_descriptor_status: fail' "$ROOT/corrupt-descriptor.out"
grep 'descriptor_phase.3.name: tensor-inventory' "$ROOT/corrupt-descriptor.out"
grep 'descriptor_phase.3.status: fail' "$ROOT/corrupt-descriptor.out"

"$YVEX_BIN" fullmodel report --model "$FULLISH" --limit-tensors 0 > "$ROOT/invalid.out" 2> "$ROOT/invalid.err" && exit 1 || true
grep 'requires a positive integer' "$ROOT/invalid.err"

"$YVEX_BIN" fullmodel materialization-plan --model "$FULLISH" --residency magic > "$ROOT/bad-residency.out" 2> "$ROOT/bad-residency.err" && exit 1 || true
grep 'fullmodel --residency must be' "$ROOT/bad-residency.err"

"$YVEX_BIN" fullmodel materialize --model "$FULLISH" --limit-bytes 0 > "$ROOT/bad-limit-bytes.out" 2> "$ROOT/bad-limit-bytes.err" && exit 1 || true
grep 'limit-bytes requires a positive integer' "$ROOT/bad-limit-bytes.err"

"$YVEX_BIN" fullmodel descriptor --model "$FULLISH" --format json > "$ROOT/bad-format.out" 2> "$ROOT/bad-format.err" && exit 1 || true
grep 'descriptor currently supports --format text only' "$ROOT/bad-format.err"

for file in \
  "$ROOT/selected-plan.out" \
  "$ROOT/selected-materialize.out" \
  "$ROOT/selected-descriptor.out" \
  "$ROOT/rmsnorm-plan.out" \
  "$ROOT/rmsnorm-materialize.out" \
  "$ROOT/rmsnorm-descriptor.out" \
  "$ROOT/fullish-plan.out" \
  "$ROOT/fullish-plan-cuda.out" \
  "$ROOT/fullish-descriptor.out" \
  "$ROOT/fullish-descriptor-cuda.out" \
  "$ROOT/fullish-materialize.out" \
  "$ROOT/fullish-materialize-dry-run.out" \
  "$ROOT/fullish-materialize-plan-only.out" \
  "$ROOT/fullish-materialize-limit.out" \
  "$ROOT/fullish-materialize-injected.out" \
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
  pattern='full model sup''port'
  ! grep "$pattern" "$file"
done
