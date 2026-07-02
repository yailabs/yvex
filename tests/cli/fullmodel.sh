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
grep 'fullmodel[[:space:]]*full model inventory, materialization, descriptor, and family-runtime reports' "$ROOT/commands.out"
grep 'attention[[:space:]]*attention class and KV requirement reports' "$ROOT/commands.out"
grep 'context[[:space:]]*context class and runtime boundary reports' "$ROOT/commands.out"
grep 'kv[[:space:]]*KV diagnostics and KV cache class reports' "$ROOT/commands.out"

"$YVEX_BIN" help fullmodel > "$ROOT/help.out"
grep 'usage: yvex fullmodel report --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel materialize --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel descriptor --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS' "$ROOT/help.out"
grep 'fullmodel report:' "$ROOT/help.out"
grep 'fullmodel materialization-plan:' "$ROOT/help.out"
grep 'fullmodel materialization proof:' "$ROOT/help.out"
grep 'fullmodel descriptor:' "$ROOT/help.out"
grep 'fullmodel family-runtime:' "$ROOT/help.out"
grep 'controlled proof over a tiny full-ish GGUF tensor set' "$ROOT/help.out"
grep 'allocates and releases only the bounded required proof tensors' "$ROOT/help.out"
grep 'planning/reporting boundary for tensor roles' "$ROOT/help.out"
grep 'maps descriptor facts into model-family runtime adapter facts' "$ROOT/help.out"
grep 'do not execute the model' "$ROOT/help.out"
grep 'do not.*generate' "$ROOT/help.out"
grep 'do not.*benchmark' "$ROOT/help.out"
grep 'no full model execution' "$ROOT/help.out"
grep 'no inference readiness' "$ROOT/help.out"
grep 'no DeepSeek generation' "$ROOT/help.out"
grep 'no benchmark' "$ROOT/help.out"
grep 'no throughput' "$ROOT/help.out"

"$YVEX_BIN" help attention > "$ROOT/attention-help.out"
grep 'usage: yvex attention report --model FILE_OR_ALIAS' "$ROOT/attention-help.out"
grep 'attention report:' "$ROOT/attention-help.out"
grep 'classifies attention requirements' "$ROOT/attention-help.out"
grep 'report-only boundary' "$ROOT/attention-help.out"
grep 'does not run full attention' "$ROOT/attention-help.out"
grep 'does not run transformer prefill' "$ROOT/attention-help.out"
grep 'does not write real attention-backed KV' "$ROOT/attention-help.out"
grep 'does not generate' "$ROOT/attention-help.out"
grep 'does not benchmark' "$ROOT/attention-help.out"
grep 'standalone RoPE and attention primitives' "$ROOT/attention-help.out"
grep 'not full transformer attention' "$ROOT/attention-help.out"

"$YVEX_BIN" help kv > "$ROOT/kv-help.out"
grep 'usage: yvex kv report --model FILE_OR_ALIAS' "$ROOT/kv-help.out"
grep 'KV cache class and requirements report' "$ROOT/kv-help.out"
grep 'report-only boundary' "$ROOT/kv-help.out"
grep 'does not allocate full runtime KV' "$ROOT/kv-help.out"
grep 'write real attention-backed KV' "$ROOT/kv-help.out"
grep 'execute decode' "$ROOT/kv-help.out"
grep 'generate' "$ROOT/kv-help.out"
grep 'benchmark' "$ROOT/kv-help.out"
grep 'diagnostic/minimal' "$ROOT/kv-help.out"

"$YVEX_BIN" help context > "$ROOT/context-help.out"
grep 'usage: yvex context report --model FILE_OR_ALIAS' "$ROOT/context-help.out"
grep 'context report:' "$ROOT/context-help.out"
grep 'report-only boundary' "$ROOT/context-help.out"
grep 'model/requested/active context' "$ROOT/context-help.out"
grep 'chunking policy' "$ROOT/context-help.out"
grep 'overflow behavior' "$ROOT/context-help.out"
grep 'does not run full transformer prefill' "$ROOT/context-help.out"
grep 'does not execute real decode' "$ROOT/context-help.out"
grep 'does not generate' "$ROOT/context-help.out"
grep 'does not benchmark' "$ROOT/context-help.out"
grep 'no long-context runtime support' "$ROOT/context-help.out"

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

"$YVEX_BIN" fullmodel family-runtime --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu --registry "$REG" > "$ROOT/selected-family.out"
grep 'family_runtime: report' "$ROOT/selected-family.out"
grep 'status: fullmodel-family-runtime' "$ROOT/selected-family.out"
grep 'target_id: deepseek4-v4-flash-selected-embed' "$ROOT/selected-family.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-family.out"
grep 'family: deepseek' "$ROOT/selected-family.out"
grep 'family_detected: deepseek' "$ROOT/selected-family.out"
grep 'family_requested: deepseek' "$ROOT/selected-family.out"
grep 'family_adapter: deepseek-runtime-report' "$ROOT/selected-family.out"
grep 'family_adapter_status: partial' "$ROOT/selected-family.out"
grep 'family_runtime_stage: report-only' "$ROOT/selected-family.out"
grep 'runtime_claim: unsupported' "$ROOT/selected-family.out"
grep 'token_embedding_role: present' "$ROOT/selected-family.out"
grep 'attention_norm_role: missing' "$ROOT/selected-family.out"
grep 'q_projection_role: missing' "$ROOT/selected-family.out"
grep 'k_projection_role: missing' "$ROOT/selected-family.out"
grep 'v_projection_role: missing' "$ROOT/selected-family.out"
grep 'o_projection_role: missing' "$ROOT/selected-family.out"
grep 'output_head_role: missing' "$ROOT/selected-family.out"
grep 'attention_rules: blocked-missing-qkv' "$ROOT/selected-family.out"
grep 'graph.attention_primitive: implemented-fixture' "$ROOT/selected-family.out"
grep 'graph.full_attention_from_model_tensors: unsupported' "$ROOT/selected-family.out"
grep 'graph.full_transformer_block_from_model_tensors: unsupported' "$ROOT/selected-family.out"
grep 'kv_write_ready: false' "$ROOT/selected-family.out"
grep 'kv_read_ready: false' "$ROOT/selected-family.out"
grep 'logits_projection_ready: false' "$ROOT/selected-family.out"
grep 'real_output_head_logits: false' "$ROOT/selected-family.out"
grep 'prefill_ready: false' "$ROOT/selected-family.out"
grep 'decode_ready: false' "$ROOT/selected-family.out"
grep 'logits_ready: false' "$ROOT/selected-family.out"
grep 'sampling_ready: false' "$ROOT/selected-family.out"
grep 'runtime_execution_ready: false' "$ROOT/selected-family.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-family.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-family.out"
grep 'next_required_rows: ATTENTION.CLASS.0' "$ROOT/selected-family.out"

"$YVEX_BIN" attention report --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu --registry "$REG" > "$ROOT/selected-attention.out"
grep 'attention: report' "$ROOT/selected-attention.out"
grep 'status: attention-report' "$ROOT/selected-attention.out"
grep 'target_id: deepseek4-v4-flash-selected-embed' "$ROOT/selected-attention.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-attention.out"
grep 'backend: cpu' "$ROOT/selected-attention.out"
grep 'family: deepseek' "$ROOT/selected-attention.out"
grep 'family_detected: deepseek' "$ROOT/selected-attention.out"
grep 'family_requested: deepseek' "$ROOT/selected-attention.out"
grep 'family_runtime_status: partial' "$ROOT/selected-attention.out"
grep 'attention_class_status: partial' "$ROOT/selected-attention.out"
grep 'attention_stage: report-only' "$ROOT/selected-attention.out"
grep 'runtime_claim: unsupported' "$ROOT/selected-attention.out"
grep 'attention_family: model-family-specific' "$ROOT/selected-attention.out"
grep 'attention_type: unknown' "$ROOT/selected-attention.out"
grep 'attention_support_status: report-only' "$ROOT/selected-attention.out"
grep 'full_transformer_attention: unsupported' "$ROOT/selected-attention.out"
grep 'attention_runtime_ready: false' "$ROOT/selected-attention.out"
grep 'attention_backend_ready: false' "$ROOT/selected-attention.out"
grep 'q_projection_status: missing' "$ROOT/selected-attention.out"
grep 'k_projection_status: missing' "$ROOT/selected-attention.out"
grep 'v_projection_status: missing' "$ROOT/selected-attention.out"
grep 'o_projection_status: missing' "$ROOT/selected-attention.out"
grep 'head_layout_status: unknown' "$ROOT/selected-attention.out"
grep 'rope_required: true' "$ROOT/selected-attention.out"
grep 'rope_runtime_ready: false' "$ROOT/selected-attention.out"
grep 'mask_runtime_ready: false' "$ROOT/selected-attention.out"
grep 'kv_required: true' "$ROOT/selected-attention.out"
grep 'kv_layout: planned' "$ROOT/selected-attention.out"
grep 'kv_write_ready: false' "$ROOT/selected-attention.out"
grep 'kv_read_ready: false' "$ROOT/selected-attention.out"
grep 'attention_kv_runtime_ready: false' "$ROOT/selected-attention.out"
grep 'graph_rope_primitive: implemented' "$ROOT/selected-attention.out"
grep 'graph_attention_primitive: implemented-fixture' "$ROOT/selected-attention.out"
grep 'graph_qkv_projection: missing-tensor' "$ROOT/selected-attention.out"
grep 'graph_full_transformer_attention: unsupported' "$ROOT/selected-attention.out"
grep 'backend_attention_status: implemented-fixture-full-transformer-unsupported' "$ROOT/selected-attention.out"
grep 'q projection tensor missing' "$ROOT/selected-attention.out"
grep 'real attention-backed KV writes unsupported' "$ROOT/selected-attention.out"
grep 'next_required_rows: KV.CACHE.0' "$ROOT/selected-attention.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-attention.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-attention.out"

"$YVEX_BIN" kv report --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu --registry "$REG" > "$ROOT/selected-kv.out"
grep 'kv: report' "$ROOT/selected-kv.out"
grep 'status: kv-report' "$ROOT/selected-kv.out"
grep 'target_id: deepseek4-v4-flash-selected-embed' "$ROOT/selected-kv.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-kv.out"
grep 'backend: cpu' "$ROOT/selected-kv.out"
grep 'family: deepseek' "$ROOT/selected-kv.out"
grep 'family_detected: deepseek' "$ROOT/selected-kv.out"
grep 'family_requested: deepseek' "$ROOT/selected-kv.out"
grep 'family_runtime_status: partial' "$ROOT/selected-kv.out"
grep 'attention_class_status: partial' "$ROOT/selected-kv.out"
grep 'kv_class_status: partial' "$ROOT/selected-kv.out"
grep 'kv_stage: report-only' "$ROOT/selected-kv.out"
grep 'runtime_claim: unsupported' "$ROOT/selected-kv.out"
grep 'diagnostic_kv_available: true' "$ROOT/selected-kv.out"
grep 'diagnostic_kv_boundary: segment-summary/minimal diagnostic KV' "$ROOT/selected-kv.out"
grep 'real_attention_kv: unsupported' "$ROOT/selected-kv.out"
grep 'real_attention_kv_write_ready: false' "$ROOT/selected-kv.out"
grep 'real_attention_kv_read_ready: false' "$ROOT/selected-kv.out"
grep 'decode_kv_consumer_ready: false' "$ROOT/selected-kv.out"
grep 'kv_required: true' "$ROOT/selected-kv.out"
grep 'kv_source: attention-qkv-requirements' "$ROOT/selected-kv.out"
grep 'kv_layout: planned' "$ROOT/selected-kv.out"
grep 'kv_dtype: planned' "$ROOT/selected-kv.out"
grep 'kv_heads: unknown' "$ROOT/selected-kv.out"
grep 'kv_head_dim: unknown' "$ROOT/selected-kv.out"
grep 'kv_capacity_status: planned' "$ROOT/selected-kv.out"
grep 'kv_indexing: layer-head-position-token-order' "$ROOT/selected-kv.out"
grep 'kv_residency_class: planned' "$ROOT/selected-kv.out"
grep 'context_required: true' "$ROOT/selected-kv.out"
grep 'attention_dependency_status: blocked-missing-qkv' "$ROOT/selected-kv.out"
grep 'attention_q_status: missing' "$ROOT/selected-kv.out"
grep 'attention_k_status: missing' "$ROOT/selected-kv.out"
grep 'attention_v_status: missing' "$ROOT/selected-kv.out"
grep 'prefill_kv_write_ready: false' "$ROOT/selected-kv.out"
grep 'decode_kv_read_ready: false' "$ROOT/selected-kv.out"
grep 'q projection tensor missing' "$ROOT/selected-kv.out"
grep 'k projection tensor missing' "$ROOT/selected-kv.out"
grep 'v projection tensor missing' "$ROOT/selected-kv.out"
grep 'next_required_rows: ATTENTION.CLASS.0 complete,CONTEXT.CLASS.0,RUNTIME.KV.1' "$ROOT/selected-kv.out"
grep 'backend_allocation_attempted: false' "$ROOT/selected-kv.out"
grep 'full_kv_allocation_proof: false' "$ROOT/selected-kv.out"
grep 'runtime_execution_ready: false' "$ROOT/selected-kv.out"
grep 'generation_ready: false' "$ROOT/selected-kv.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-kv.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-kv.out"

"$YVEX_BIN" context report --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu --registry "$REG" > "$ROOT/selected-context.out"
grep 'context: report' "$ROOT/selected-context.out"
grep 'status: context-report' "$ROOT/selected-context.out"
grep 'target_id: deepseek4-v4-flash-selected-embed' "$ROOT/selected-context.out"
grep 'target_class: selected-runtime-slice' "$ROOT/selected-context.out"
grep 'backend: cpu' "$ROOT/selected-context.out"
grep 'family: deepseek' "$ROOT/selected-context.out"
grep 'family_detected: deepseek' "$ROOT/selected-context.out"
grep 'family_runtime_status: partial' "$ROOT/selected-context.out"
grep 'attention_class_status: partial' "$ROOT/selected-context.out"
grep 'kv_class_status: partial' "$ROOT/selected-context.out"
grep 'context_class_status: report-only' "$ROOT/selected-context.out"
grep 'context_stage: report-only' "$ROOT/selected-context.out"
grep 'runtime_claim: unsupported' "$ROOT/selected-context.out"
grep 'model_max_context: 8' "$ROOT/selected-context.out"
grep 'active_context: diagnostic' "$ROOT/selected-context.out"
grep 'token_input_status: not-provided' "$ROOT/selected-context.out"
grep 'full_transformer_prefill_ready: false' "$ROOT/selected-context.out"
grep 'decode_context_ready: false' "$ROOT/selected-context.out"
grep 'real_attention_kv_ready: false' "$ROOT/selected-context.out"
grep 'full_generation_context_ready: false' "$ROOT/selected-context.out"
grep 'generation_ready: false' "$ROOT/selected-context.out"
grep 'generation: unsupported-full-model' "$ROOT/selected-context.out"
grep 'benchmark_status: not-measured' "$ROOT/selected-context.out"

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

"$YVEX_BIN" fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --include-blockers --include-roles --include-graph --include-kv --include-moe --include-output > "$ROOT/rmsnorm-family.out"
grep 'family_runtime: report' "$ROOT/rmsnorm-family.out"
grep 'status: fullmodel-family-runtime' "$ROOT/rmsnorm-family.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-family.out"
grep 'family_adapter_status: partial' "$ROOT/rmsnorm-family.out"
grep 'token_embedding_role: present' "$ROOT/rmsnorm-family.out"
grep 'attention_norm_role: present' "$ROOT/rmsnorm-family.out"
grep 'q_projection_role: missing' "$ROOT/rmsnorm-family.out"
grep 'k_projection_role: missing' "$ROOT/rmsnorm-family.out"
grep 'v_projection_role: missing' "$ROOT/rmsnorm-family.out"
grep 'o_projection_role: missing' "$ROOT/rmsnorm-family.out"
grep 'output_head_role: missing' "$ROOT/rmsnorm-family.out"
grep 'attention_rule_status: blocked-missing-qkv' "$ROOT/rmsnorm-family.out"
grep 'moe_dispatch_ready: false' "$ROOT/rmsnorm-family.out"
grep 'real_output_head_logits: false' "$ROOT/rmsnorm-family.out"
grep 'runtime_blockers:' "$ROOT/rmsnorm-family.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-family.out"

"$YVEX_BIN" fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --registry "$REG" > "$ROOT/rmsnorm-family-auto.out"
grep 'family_requested: auto' "$ROOT/rmsnorm-family-auto.out"
grep 'family_detected: deepseek' "$ROOT/rmsnorm-family-auto.out"
grep 'status: fullmodel-family-runtime' "$ROOT/rmsnorm-family-auto.out"

"$YVEX_BIN" attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --include-kv --include-context --include-graph --include-blockers > "$ROOT/rmsnorm-attention.out"
grep 'attention: report' "$ROOT/rmsnorm-attention.out"
grep 'status: attention-report' "$ROOT/rmsnorm-attention.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-attention.out"
grep 'family: deepseek' "$ROOT/rmsnorm-attention.out"
grep 'attention_class_status: partial' "$ROOT/rmsnorm-attention.out"
grep 'attention_norm_role: present' "$ROOT/rmsnorm-attention.out"
grep 'q_projection_status: missing' "$ROOT/rmsnorm-attention.out"
grep 'k_projection_status: missing' "$ROOT/rmsnorm-attention.out"
grep 'v_projection_status: missing' "$ROOT/rmsnorm-attention.out"
grep 'o_projection_status: missing' "$ROOT/rmsnorm-attention.out"
grep 'full_transformer_attention: unsupported' "$ROOT/rmsnorm-attention.out"
grep 'attention_runtime_ready: false' "$ROOT/rmsnorm-attention.out"
grep 'rope_runtime_ready: false' "$ROOT/rmsnorm-attention.out"
grep 'mask_runtime_ready: false' "$ROOT/rmsnorm-attention.out"
grep 'attention_kv_runtime_ready: false' "$ROOT/rmsnorm-attention.out"
grep 'graph_attention_kv_write: unsupported' "$ROOT/rmsnorm-attention.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-attention.out"
grep 'benchmark_status: not-measured' "$ROOT/rmsnorm-attention.out"

"$YVEX_BIN" attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --registry "$REG" > "$ROOT/rmsnorm-attention-auto.out"
grep 'family_requested: auto' "$ROOT/rmsnorm-attention-auto.out"
grep 'family_detected: deepseek' "$ROOT/rmsnorm-attention-auto.out"
grep 'status: attention-report' "$ROOT/rmsnorm-attention-auto.out"

"$YVEX_BIN" kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --include-attention --include-context --include-residency --include-blockers > "$ROOT/rmsnorm-kv.out"
grep 'kv: report' "$ROOT/rmsnorm-kv.out"
grep 'status: kv-report' "$ROOT/rmsnorm-kv.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-kv.out"
grep 'target_class: selected-runtime-slice' "$ROOT/rmsnorm-kv.out"
grep 'family: deepseek' "$ROOT/rmsnorm-kv.out"
grep 'family_runtime_status: partial' "$ROOT/rmsnorm-kv.out"
grep 'attention_class_status: partial' "$ROOT/rmsnorm-kv.out"
grep 'kv_class_status: partial' "$ROOT/rmsnorm-kv.out"
grep 'report_options.include_attention: true' "$ROOT/rmsnorm-kv.out"
grep 'report_options.include_context: true' "$ROOT/rmsnorm-kv.out"
grep 'report_options.include_residency: true' "$ROOT/rmsnorm-kv.out"
grep 'report_options.include_blockers: true' "$ROOT/rmsnorm-kv.out"
grep 'diagnostic_kv_available: true' "$ROOT/rmsnorm-kv.out"
grep 'real_attention_kv: unsupported' "$ROOT/rmsnorm-kv.out"
grep 'role.attention_norm.status: present' "$ROOT/rmsnorm-kv.out"
grep 'role.q_projection.status: missing' "$ROOT/rmsnorm-kv.out"
grep 'attention_dependency_status: blocked-missing-qkv' "$ROOT/rmsnorm-kv.out"
grep 'prefill_kv_write_ready: false' "$ROOT/rmsnorm-kv.out"
grep 'decode_kv_read_ready: false' "$ROOT/rmsnorm-kv.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-kv.out"
grep 'benchmark_status: not-measured' "$ROOT/rmsnorm-kv.out"

"$YVEX_BIN" kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --registry "$REG" > "$ROOT/rmsnorm-kv-auto.out"
grep 'family_requested: auto' "$ROOT/rmsnorm-kv-auto.out"
grep 'family_detected: deepseek' "$ROOT/rmsnorm-kv-auto.out"
grep 'status: kv-report' "$ROOT/rmsnorm-kv-auto.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-kv-auto.out"

"$YVEX_BIN" context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --tokens 0,1,2,3 --include-attention --include-kv --include-prefill --include-decode --include-blockers > "$ROOT/rmsnorm-context.out"
grep 'context: report' "$ROOT/rmsnorm-context.out"
grep 'status: context-report' "$ROOT/rmsnorm-context.out"
grep 'target_id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/rmsnorm-context.out"
grep 'target_class: selected-runtime-slice' "$ROOT/rmsnorm-context.out"
grep 'family: deepseek' "$ROOT/rmsnorm-context.out"
grep 'family_runtime_status: partial' "$ROOT/rmsnorm-context.out"
grep 'context_class_status: report-only' "$ROOT/rmsnorm-context.out"
grep 'report_options.include_attention: true' "$ROOT/rmsnorm-context.out"
grep 'report_options.include_kv: true' "$ROOT/rmsnorm-context.out"
grep 'report_options.include_prefill: true' "$ROOT/rmsnorm-context.out"
grep 'report_options.include_decode: true' "$ROOT/rmsnorm-context.out"
grep 'token_input_status: available' "$ROOT/rmsnorm-context.out"
grep 'token_count: 4' "$ROOT/rmsnorm-context.out"
grep 'prompt_token_count: 4' "$ROOT/rmsnorm-context.out"
grep 'prefill_token_count: 4' "$ROOT/rmsnorm-context.out"
grep 'generated_token_count: 0' "$ROOT/rmsnorm-context.out"
grep 'token_range_start: 0' "$ROOT/rmsnorm-context.out"
grep 'token_range_end: 3' "$ROOT/rmsnorm-context.out"
grep 'prefill_position_start: 0' "$ROOT/rmsnorm-context.out"
grep 'prefill_position_end: 4' "$ROOT/rmsnorm-context.out"
grep 'decode_position_policy: prefill-end-token-count' "$ROOT/rmsnorm-context.out"
grep 'decode_start_position: 4' "$ROOT/rmsnorm-context.out"
grep 'bounded_generation_context_policy: context-limit-pre-append' "$ROOT/rmsnorm-context.out"
grep 'full_generation_context_ready: false' "$ROOT/rmsnorm-context.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-context.out"
grep 'benchmark_status: not-measured' "$ROOT/rmsnorm-context.out"

"$YVEX_BIN" context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --registry "$REG" --tokens 0,1,2,3 > "$ROOT/rmsnorm-context-auto.out"
grep 'family_requested: auto' "$ROOT/rmsnorm-context-auto.out"
grep 'family_detected: deepseek' "$ROOT/rmsnorm-context-auto.out"
grep 'status: context-report' "$ROOT/rmsnorm-context-auto.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-context-auto.out"

"$YVEX_BIN" context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --tokens 0,1,2,3 --chunk-size 2 > "$ROOT/rmsnorm-context-chunk.out"
grep 'chunking_required: true' "$ROOT/rmsnorm-context-chunk.out"
grep 'chunk_size: 2' "$ROOT/rmsnorm-context-chunk.out"
grep 'chunk_count: 2' "$ROOT/rmsnorm-context-chunk.out"
grep 'last_chunk_size: 2' "$ROOT/rmsnorm-context-chunk.out"
grep 'chunking_policy: explicit-token-chunks-report-only' "$ROOT/rmsnorm-context-chunk.out"
grep 'chunking_status: report-only' "$ROOT/rmsnorm-context-chunk.out"
grep 'full_transformer_prefill_ready: false' "$ROOT/rmsnorm-context-chunk.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-context-chunk.out"

"$YVEX_BIN" context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --registry "$REG" --tokens 0,1,2,3 --context-length 2 > "$ROOT/rmsnorm-context-overflow.out"
grep 'requested_context: 2' "$ROOT/rmsnorm-context-overflow.out"
grep 'active_context: 2' "$ROOT/rmsnorm-context-overflow.out"
grep 'context_overflow: overflow' "$ROOT/rmsnorm-context-overflow.out"
grep 'overflow_check_status: pass' "$ROOT/rmsnorm-context-overflow.out"
grep 'overflow_stop_reason: context-limit' "$ROOT/rmsnorm-context-overflow.out"
grep 'overflow_mutates_state: false' "$ROOT/rmsnorm-context-overflow.out"
grep 'generation: unsupported-full-model' "$ROOT/rmsnorm-context-overflow.out"

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

"$YVEX_BIN" fullmodel family-runtime --model "$FULLISH" --target deepseek4-v4-flash --family deepseek --backend cpu > "$ROOT/fullish-family.out"
grep 'family_runtime: report' "$ROOT/fullish-family.out"
grep 'status: fullmodel-family-runtime' "$ROOT/fullish-family.out"
grep 'target_id: deepseek4-v4-flash' "$ROOT/fullish-family.out"
grep 'family_adapter_status: complete' "$ROOT/fullish-family.out"
grep 'role_adapter_status: complete' "$ROOT/fullish-family.out"
grep 'collection_adapter_status: complete' "$ROOT/fullish-family.out"
grep 'q_projection_role: present' "$ROOT/fullish-family.out"
grep 'k_projection_role: present' "$ROOT/fullish-family.out"
grep 'v_projection_role: present' "$ROOT/fullish-family.out"
grep 'o_projection_role: present' "$ROOT/fullish-family.out"
grep 'mlp_gate_role: present' "$ROOT/fullish-family.out"
grep 'output_head_role: present' "$ROOT/fullish-family.out"
grep 'attention_rule_status: blocked-full-transformer-integration' "$ROOT/fullish-family.out"
grep 'graph.attention_primitive: implemented-fixture' "$ROOT/fullish-family.out"
grep 'graph.full_transformer_block_from_model_tensors: unsupported' "$ROOT/fullish-family.out"
grep 'full_transformer_graph_ready: false' "$ROOT/fullish-family.out"
grep 'logits_projection_ready: false' "$ROOT/fullish-family.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-family.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-family.out"

"$YVEX_BIN" attention report --model "$FULLISH" --family deepseek --backend cpu > "$ROOT/fullish-attention.out"
grep 'attention: report' "$ROOT/fullish-attention.out"
grep 'status: attention-report' "$ROOT/fullish-attention.out"
grep 'target_class: candidate-GGUF-path' "$ROOT/fullish-attention.out"
grep 'backend: cpu' "$ROOT/fullish-attention.out"
grep 'family: deepseek' "$ROOT/fullish-attention.out"
grep 'family_runtime_status: complete' "$ROOT/fullish-attention.out"
grep 'attention_class_status: complete' "$ROOT/fullish-attention.out"
grep 'q_projection_status: present' "$ROOT/fullish-attention.out"
grep 'k_projection_status: present' "$ROOT/fullish-attention.out"
grep 'v_projection_status: present' "$ROOT/fullish-attention.out"
grep 'o_projection_status: present' "$ROOT/fullish-attention.out"
grep 'graph_qkv_projection: planned' "$ROOT/fullish-attention.out"
grep 'graph_o_projection: planned' "$ROOT/fullish-attention.out"
grep 'graph_full_transformer_attention: unsupported' "$ROOT/fullish-attention.out"
grep 'attention_runtime_ready: false' "$ROOT/fullish-attention.out"
grep 'attention_backend_ready: false' "$ROOT/fullish-attention.out"
grep 'real QKV projection over model tensors unsupported' "$ROOT/fullish-attention.out"
grep 'attention-backed KV write unsupported' "$ROOT/fullish-attention.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-attention.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-attention.out"

"$YVEX_BIN" fullmodel descriptor --model "$FULLISH" --target deepseek4-v4-flash --backend cuda > "$ROOT/fullish-descriptor-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend.cuda.available:' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend.full_transformer_integration: unsupported' "$ROOT/fullish-descriptor-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-descriptor-cuda.out"

"$YVEX_BIN" fullmodel family-runtime --model "$FULLISH" --target deepseek4-v4-flash --family deepseek --backend cuda > "$ROOT/fullish-family-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-family-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-family-cuda.out"
grep 'graph.full_attention_from_model_tensors: unsupported' "$ROOT/fullish-family-cuda.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-family-cuda.out"

"$YVEX_BIN" attention report --model "$FULLISH" --family deepseek --backend cuda > "$ROOT/fullish-attention-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-attention-cuda.out"
grep 'backend_attention_status: implemented-fixture-full-transformer-unsupported' "$ROOT/fullish-attention-cuda.out"
grep 'graph_full_transformer_attention: unsupported' "$ROOT/fullish-attention-cuda.out"
grep 'attention_backend_ready: false' "$ROOT/fullish-attention-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-attention-cuda.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-attention-cuda.out"

"$YVEX_BIN" kv report --model "$FULLISH" --family deepseek --backend cpu > "$ROOT/fullish-kv.out"
grep 'kv: report' "$ROOT/fullish-kv.out"
grep 'status: kv-report' "$ROOT/fullish-kv.out"
grep 'target_class: candidate-GGUF-path' "$ROOT/fullish-kv.out"
grep 'backend: cpu' "$ROOT/fullish-kv.out"
grep 'family: deepseek' "$ROOT/fullish-kv.out"
grep 'family_runtime_status: complete' "$ROOT/fullish-kv.out"
grep 'attention_class_status: complete' "$ROOT/fullish-kv.out"
grep 'kv_class_status: complete' "$ROOT/fullish-kv.out"
grep 'kv_stage: report-only' "$ROOT/fullish-kv.out"
grep 'role.q_projection.status: present' "$ROOT/fullish-kv.out"
grep 'role.k_projection.status: present' "$ROOT/fullish-kv.out"
grep 'role.v_projection.status: present' "$ROOT/fullish-kv.out"
grep 'role.o_projection.status: present' "$ROOT/fullish-kv.out"
grep 'qkv_role_coverage: present' "$ROOT/fullish-kv.out"
grep 'attention_dependency_status: blocked-runtime-integration' "$ROOT/fullish-kv.out"
grep 'real_attention_kv: unsupported' "$ROOT/fullish-kv.out"
grep 'real_attention_kv_write_ready: false' "$ROOT/fullish-kv.out"
grep 'decode_kv_consumer_ready: false' "$ROOT/fullish-kv.out"
grep 'context_length_source: model-metadata' "$ROOT/fullish-kv.out"
grep 'max_context: 128' "$ROOT/fullish-kv.out"
grep 'kv_blockers: real attention-backed KV writes unsupported' "$ROOT/fullish-kv.out"
grep 'full_kv_allocation_proof: false' "$ROOT/fullish-kv.out"
grep 'paged_kv_implementation: false' "$ROOT/fullish-kv.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-kv.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-kv.out"

"$YVEX_BIN" kv report --model "$FULLISH" --family deepseek --backend cuda > "$ROOT/fullish-kv-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-kv-cuda.out"
grep 'cuda_full_kv_allocation_proof: false' "$ROOT/fullish-kv-cuda.out"
grep 'backend_allocation_attempted: false' "$ROOT/fullish-kv-cuda.out"
grep 'real_attention_kv: unsupported' "$ROOT/fullish-kv-cuda.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-kv-cuda.out"

"$YVEX_BIN" context report --model "$FULLISH" --family deepseek --backend cpu --tokens 0,1,2,3 --context-length 8 --chunk-size 2 > "$ROOT/fullish-context.out"
grep 'context: report' "$ROOT/fullish-context.out"
grep 'status: context-report' "$ROOT/fullish-context.out"
grep 'target_class: candidate-GGUF-path' "$ROOT/fullish-context.out"
grep 'backend: cpu' "$ROOT/fullish-context.out"
grep 'family: deepseek' "$ROOT/fullish-context.out"
grep 'family_runtime_status: complete' "$ROOT/fullish-context.out"
grep 'attention_class_status: complete' "$ROOT/fullish-context.out"
grep 'kv_class_status: complete' "$ROOT/fullish-context.out"
grep 'context_class_status: report-only' "$ROOT/fullish-context.out"
grep 'model_max_context: 128' "$ROOT/fullish-context.out"
grep 'model_max_context_source: deepseek.context_length' "$ROOT/fullish-context.out"
grep 'requested_context: 8' "$ROOT/fullish-context.out"
grep 'active_context: 8' "$ROOT/fullish-context.out"
grep 'token_count: 4' "$ROOT/fullish-context.out"
grep 'chunk_count: 2' "$ROOT/fullish-context.out"
grep 'last_chunk_size: 2' "$ROOT/fullish-context.out"
grep 'context_overflow: none' "$ROOT/fullish-context.out"
grep 'attention_dependency_status: blocked-runtime-integration' "$ROOT/fullish-context.out"
grep 'kv_dependency_status: blocked-runtime-integration' "$ROOT/fullish-context.out"
grep 'kv_context_positions: 8' "$ROOT/fullish-context.out"
grep 'full_transformer_prefill_ready: false' "$ROOT/fullish-context.out"
grep 'decode_context_ready: false' "$ROOT/fullish-context.out"
grep 'full_generation_context_ready: false' "$ROOT/fullish-context.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-context.out"
grep 'benchmark_status: not-measured' "$ROOT/fullish-context.out"

"$YVEX_BIN" context report --model "$FULLISH" --family deepseek --backend cuda --tokens 0,1,2,3 > "$ROOT/fullish-context-cuda.out"
grep 'backend: cuda' "$ROOT/fullish-context-cuda.out"
grep 'status: context-report' "$ROOT/fullish-context-cuda.out"
grep 'model_max_context: 128' "$ROOT/fullish-context-cuda.out"
grep 'full_transformer_prefill_ready: false' "$ROOT/fullish-context-cuda.out"
grep 'decode_context_ready: false' "$ROOT/fullish-context-cuda.out"
grep 'generation: unsupported-full-model' "$ROOT/fullish-context-cuda.out"

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

"$YVEX_BIN" fullmodel family-runtime --model glm-5.2-official-safetensors --family glm --backend cpu > "$ROOT/glm-family.out" && exit 1 || true
grep 'status: fullmodel-family-runtime-unsupported' "$ROOT/glm-family.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-family.out"
grep 'family_detected: glm' "$ROOT/glm-family.out"
grep 'family_requested: glm' "$ROOT/glm-family.out"
grep 'tensor_inventory_status: not-performed-source-only-target' "$ROOT/glm-family.out"
grep 'runtime_claim: unsupported' "$ROOT/glm-family.out"
grep 'full_model_execution: unsupported' "$ROOT/glm-family.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-family.out"

"$YVEX_BIN" attention report --model glm-5.2-official-safetensors --family glm --backend cpu > "$ROOT/glm-attention.out" && exit 1 || true
grep 'attention: report' "$ROOT/glm-attention.out"
grep 'status: attention-report-unsupported' "$ROOT/glm-attention.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-attention.out"
grep 'family: glm' "$ROOT/glm-attention.out"
grep 'family_requested: glm' "$ROOT/glm-attention.out"
grep 'tensor_inventory_status: not-performed-source-only-target' "$ROOT/glm-attention.out"
grep 'source_artifact_class: official safetensors' "$ROOT/glm-attention.out"
grep 'runtime_claim: unsupported' "$ROOT/glm-attention.out"
grep 'full_transformer_attention: unsupported' "$ROOT/glm-attention.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-attention.out"
grep 'benchmark_status: not-measured' "$ROOT/glm-attention.out"

"$YVEX_BIN" kv report --model glm-5.2-official-safetensors --family glm --backend cpu > "$ROOT/glm-kv.out" && exit 1 || true
grep 'kv: report' "$ROOT/glm-kv.out"
grep 'status: kv-report-unsupported' "$ROOT/glm-kv.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-kv.out"
grep 'family: glm' "$ROOT/glm-kv.out"
grep 'family_detected: glm' "$ROOT/glm-kv.out"
grep 'tensor_inventory_status: not-performed-source-only-target' "$ROOT/glm-kv.out"
grep 'source_artifact_class: official safetensors' "$ROOT/glm-kv.out"
grep 'target_artifact_class: future YVEX-produced GGUF' "$ROOT/glm-kv.out"
grep 'runtime_claim: unsupported' "$ROOT/glm-kv.out"
grep 'real_attention_kv: unsupported' "$ROOT/glm-kv.out"
grep 'attention_dependency_status: unsupported-source-only' "$ROOT/glm-kv.out"
grep 'prefill_kv_write_ready: false' "$ROOT/glm-kv.out"
grep 'decode_kv_read_ready: false' "$ROOT/glm-kv.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-kv.out"
grep 'benchmark_status: not-measured' "$ROOT/glm-kv.out"

"$YVEX_BIN" context report --model glm-5.2-official-safetensors --family glm --backend cpu > "$ROOT/glm-context.out" && exit 1 || true
grep 'context: report' "$ROOT/glm-context.out"
grep 'status: context-report-unsupported' "$ROOT/glm-context.out"
grep 'target_class: official-source-huge-model' "$ROOT/glm-context.out"
grep 'family: glm' "$ROOT/glm-context.out"
grep 'family_detected: glm' "$ROOT/glm-context.out"
grep 'model_max_context_status: unsupported-source-only' "$ROOT/glm-context.out"
grep 'token_input_status: not-performed-source-only-target' "$ROOT/glm-context.out"
grep 'active_context: unsupported' "$ROOT/glm-context.out"
grep 'prefill_context_ready: false' "$ROOT/glm-context.out"
grep 'decode_context_ready: false' "$ROOT/glm-context.out"
grep 'real_attention_kv_ready: false' "$ROOT/glm-context.out"
grep 'generation: unsupported-full-model' "$ROOT/glm-context.out"
grep 'benchmark_status: not-measured' "$ROOT/glm-context.out"

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

"$YVEX_BIN" fullmodel family-runtime --model "$FULLISH" --family unknown-family --backend cpu > "$ROOT/bad-family.out" 2> "$ROOT/bad-family.err" && exit 1 || true
grep 'status: fullmodel-family-runtime-unsupported' "$ROOT/bad-family.out"
grep 'family_requested: unknown-family' "$ROOT/bad-family.out"
grep 'runtime_claim: unsupported' "$ROOT/bad-family.out"
grep 'generation: unsupported-full-model' "$ROOT/bad-family.out"

"$YVEX_BIN" attention report --model "$FULLISH" --family unknown-family --backend cpu > "$ROOT/bad-attention-family.out" 2> "$ROOT/bad-attention-family.err" && exit 1 || true
grep 'status: attention-report-unsupported' "$ROOT/bad-attention-family.out"
grep 'family_requested: unknown-family' "$ROOT/bad-attention-family.out"
grep 'runtime_claim: unsupported' "$ROOT/bad-attention-family.out"
grep 'full_transformer_attention: unsupported' "$ROOT/bad-attention-family.out"
grep 'generation: unsupported-full-model' "$ROOT/bad-attention-family.out"

"$YVEX_BIN" kv report --model "$FULLISH" --family unknown-family --backend cpu > "$ROOT/bad-kv-family.out" 2> "$ROOT/bad-kv-family.err" && exit 1 || true
grep 'status: kv-report-unsupported' "$ROOT/bad-kv-family.out"
grep 'family_requested: unknown-family' "$ROOT/bad-kv-family.out"
grep 'runtime_claim: unsupported' "$ROOT/bad-kv-family.out"
grep 'real_attention_kv: unsupported' "$ROOT/bad-kv-family.out"
grep 'kv_layout: unsupported' "$ROOT/bad-kv-family.out"
grep 'generation: unsupported-full-model' "$ROOT/bad-kv-family.out"

"$YVEX_BIN" context report --model "$FULLISH" --family unknown-family --backend cpu > "$ROOT/bad-context-family.out" 2> "$ROOT/bad-context-family.err" && exit 1 || true
grep 'status: context-report-unsupported' "$ROOT/bad-context-family.out"
grep 'family_requested: unknown-family' "$ROOT/bad-context-family.out"
grep 'runtime_claim: unsupported' "$ROOT/bad-context-family.out"
grep 'active_context: unsupported' "$ROOT/bad-context-family.out"
grep 'decode_context_ready: false' "$ROOT/bad-context-family.out"
grep 'full_generation_context_ready: false' "$ROOT/bad-context-family.out"
grep 'generation: unsupported-full-model' "$ROOT/bad-context-family.out"

for file in \
  "$ROOT/selected-plan.out" \
  "$ROOT/selected-materialize.out" \
  "$ROOT/selected-descriptor.out" \
	  "$ROOT/selected-family.out" \
	  "$ROOT/selected-attention.out" \
	  "$ROOT/selected-kv.out" \
	  "$ROOT/selected-context.out" \
	  "$ROOT/rmsnorm-plan.out" \
	  "$ROOT/rmsnorm-materialize.out" \
	  "$ROOT/rmsnorm-descriptor.out" \
	  "$ROOT/rmsnorm-family.out" \
	  "$ROOT/rmsnorm-family-auto.out" \
	  "$ROOT/rmsnorm-attention.out" \
	  "$ROOT/rmsnorm-attention-auto.out" \
	  "$ROOT/rmsnorm-kv.out" \
	  "$ROOT/rmsnorm-kv-auto.out" \
	  "$ROOT/rmsnorm-context.out" \
	  "$ROOT/rmsnorm-context-auto.out" \
	  "$ROOT/rmsnorm-context-chunk.out" \
	  "$ROOT/rmsnorm-context-overflow.out" \
	  "$ROOT/fullish-plan.out" \
	  "$ROOT/fullish-plan-cuda.out" \
	  "$ROOT/fullish-descriptor.out" \
	  "$ROOT/fullish-descriptor-cuda.out" \
	  "$ROOT/fullish-family.out" \
	  "$ROOT/fullish-family-cuda.out" \
	  "$ROOT/fullish-attention.out" \
	  "$ROOT/fullish-attention-cuda.out" \
	  "$ROOT/fullish-kv.out" \
	  "$ROOT/fullish-kv-cuda.out" \
	  "$ROOT/fullish-context.out" \
	  "$ROOT/fullish-context-cuda.out" \
	  "$ROOT/fullish-materialize.out" \
	  "$ROOT/fullish-materialize-dry-run.out" \
	  "$ROOT/fullish-materialize-plan-only.out" \
  "$ROOT/fullish-materialize-limit.out" \
  "$ROOT/fullish-materialize-injected.out" \
	  "$ROOT/resident.out" \
	  "$ROOT/host-staged.out" \
	  "$ROOT/ssd-staged.out" \
	  "$ROOT/hybrid.out" \
	  "$ROOT/glm-attention.out" \
	  "$ROOT/glm-kv.out" \
	  "$ROOT/glm-context.out" \
	  "$ROOT/bad-attention-family.out" \
	  "$ROOT/bad-kv-family.out" \
	  "$ROOT/bad-context-family.out"
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
	  pattern='runtime_execution_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='attention_runtime_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='attention_backend_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='full_transformer_attention: tr''ue'
	  ! grep "$pattern" "$file"
	  pattern='real_attention_kv: tr''ue'
	  ! grep "$pattern" "$file"
	  pattern='kv_write_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='decode_kv_consumer_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='full_transformer_prefill_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='decode_context_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='full_generation_context_rea''dy: true'
	  ! grep "$pattern" "$file"
	  pattern='benchmark_status: mea''sured'
	  ! grep "$pattern" "$file"
  pattern='tok/''s'
  ! grep "$pattern" "$file"
  pattern='tokens/''sec'
  ! grep "$pattern" "$file"
	  pattern='DeepSeek generation imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='DeepSeek attention imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='DeepSeek KV imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='paged KV imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='full KV allocation imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='full transformer attention imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='long context imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='context extension imple''mented'
	  ! grep "$pattern" "$file"
	  pattern='DeepSeek context sup''port'
	  ! grep "$pattern" "$file"
  pattern='inference rea''dy'
  ! grep "$pattern" "$file"
  pattern='full model sup''port'
  ! grep "$pattern" "$file"
done
