#!/bin/sh
#
# YVEX - Source manifest CLI smoke test
#
# File: tests/cli/source_manifest.sh
# Layer: test
#
# Purpose:
#   Proves the open-weight intake source-manifest create command over a tiny fake local
#   source tree. The script does not download or commit model files.

set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/source-manifest-cli}
MODEL_DIR="$OUT_DIR/model"
MANIFEST="$OUT_DIR/manifest.json"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

rm -rf "$OUT_DIR"
mkdir -p "$MODEL_DIR"
printf '{}\n' > "$MODEL_DIR/config.json"
printf '{}\n' > "$MODEL_DIR/tokenizer.json"
printf 'x' > "$MODEL_DIR/model-00001.safetensors"

"$YVEX_BIN" source-manifest create \
  --hf-repo test-org/test-model \
  --revision test-rev \
  --license test-license \
  --model-card https://example.invalid/test-model \
  --local-path "$MODEL_DIR" \
  --node test-node \
  --status in-progress \
  --out "$MANIFEST" > "$OUT_DIR/create.out" 2> "$OUT_DIR/create.err" || fail "source-manifest create failed"

test -f "$MANIFEST" || fail "manifest not written"
grep '"schema": "yvex.source_manifest.v1"' "$MANIFEST" >/dev/null || fail "missing schema"
grep '"repo": "test-org/test-model"' "$MANIFEST" >/dev/null || fail "missing repo"
grep '"status": "in-progress"' "$MANIFEST" >/dev/null || fail "missing status"
grep 'model-00001.safetensors' "$MANIFEST" >/dev/null || fail "missing safetensors file"
grep 'status: source-manifest-written' "$OUT_DIR/create.out" >/dev/null || fail "missing CLI status"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$OUT_DIR/models" > "$OUT_DIR/deepseek.out"
grep 'target: deepseek4-v4-flash' "$OUT_DIR/deepseek.out" >/dev/null || fail "missing canonical DeepSeek target"
grep 'status: exact-source-blocked' "$OUT_DIR/deepseek.out" >/dev/null || fail "missing blocked source status"
grep 'top_blocker: missing-source-path' "$OUT_DIR/deepseek.out" >/dev/null || fail "missing source refusal"
grep 'next: V010.REBASE.DEEPSEEK.0' "$OUT_DIR/deepseek.out" >/dev/null || fail "wrong blocked handoff"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$OUT_DIR/models" --output table > "$OUT_DIR/deepseek-table.out"
grep 'deepseek4-v4-flash' "$OUT_DIR/deepseek-table.out" >/dev/null || fail "table lost canonical target"
grep 'blocked' "$OUT_DIR/deepseek-table.out" >/dev/null || fail "table lost verification state"
grep 'top_blocker: missing-source-path' "$OUT_DIR/deepseek-table.out" >/dev/null || fail "table lost typed refusal"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$OUT_DIR/models" --audit > "$OUT_DIR/deepseek-audit.out"
grep 'canonical_repository: deepseek-ai/DeepSeek-V4-Flash' "$OUT_DIR/deepseek-audit.out" >/dev/null || fail "audit lost repository identity"
grep 'source_verification_status: blocked' "$OUT_DIR/deepseek-audit.out" >/dev/null || fail "audit lost verification state"
grep 'blocker_0: missing-source-path' "$OUT_DIR/deepseek-audit.out" >/dev/null || fail "audit lost typed refusal"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$OUT_DIR/models" --output json > "$OUT_DIR/deepseek.json"
jq -e '.target_id == "deepseek4-v4-flash" and .repository == "deepseek-ai/DeepSeek-V4-Flash" and .verification == "blocked" and .top_blocker == "missing-source-path" and .source_verification_blocker_count == 1 and .blocker_0 == "missing-source-path" and .next == "V010.REBASE.DEEPSEEK.0" and .artifact_status == "not-produced" and .runtime == "unsupported" and .generation == "unsupported" and .tensor_payload_loaded == false' "$OUT_DIR/deepseek.json" >/dev/null || fail "JSON source facts disagree"

set +e
"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$OUT_DIR/models" --strict > "$OUT_DIR/deepseek-strict.out"
strict_rc=$?
set -e
test "$strict_rc" -eq 5 || fail "strict verification returned $strict_rc instead of 5"

VERIFIED_MODELS="$OUT_DIR/verified-models"
VERIFIED_SOURCE="$VERIFIED_MODELS/hf/deepseek/DeepSeek-V4-Flash"
python3 - "$VERIFIED_SOURCE" <<'PY'
import json
import struct
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)
revision = "0123456789abcdef0123456789abcdef01234567"
config = {
    "architectures": ["DeepseekV4ForCausalLM"],
    "model_type": "deepseek_v4",
    "hidden_size": 4096,
    "num_hidden_layers": 43,
    "num_attention_heads": 64,
    "num_key_value_heads": 1,
    "head_dim": 512,
    "qk_rope_head_dim": 64,
    "max_position_embeddings": 1048576,
    "moe_intermediate_size": 2048,
    "n_routed_experts": 256,
    "n_shared_experts": 1,
    "num_experts_per_tok": 6,
    "num_hash_layers": 3,
    "q_lora_rank": 1024,
    "o_lora_rank": 1024,
    "vocab_size": 129280,
    "sliding_window": 128,
    "tie_word_embeddings": False,
    "torch_dtype": "bfloat16",
    "expert_dtype": "fp4",
    "hidden_act": "silu",
    "attention_bias": False,
    "attention_dropout": 0.0,
    "bos_token_id": 0,
    "eos_token_id": 1,
    "compress_ratios": [0, 0] + [value for _ in range(20) for value in (4, 128)] + [4, 0],
    "compress_rope_theta": 160000,
    "hc_eps": 0.000001,
    "hc_mult": 4,
    "hc_sinkhorn_iters": 20,
    "index_head_dim": 128,
    "index_n_heads": 64,
    "index_topk": 512,
    "num_nextn_predict_layers": 1,
    "o_groups": 8,
    "rms_norm_eps": 0.000001,
    "rope_theta": 10000,
    "routed_scaling_factor": 1.5,
    "scoring_func": "sqrtsoftplus",
    "topk_method": "noaux_tc",
    "norm_topk_prob": True,
    "swiglu_limit": 10.0,
    "use_cache": True,
    "rope_scaling": {
        "type": "yarn",
        "factor": 16,
        "original_max_position_embeddings": 65536,
    },
    "quantization_config": {
        "quant_method": "fp8",
        "fmt": "e4m3",
        "weight_block_size": [128, 128],
    },
}
tokenizer = {
    "version": "1.0",
    "added_tokens": [],
    "normalizer": None,
    "pre_tokenizer": {},
    "post_processor": {},
    "decoder": {},
    "model": {"type": "BPE", "vocab": {}},
}
tokenizer_config = {
    "tokenizer_class": "PreTrainedTokenizerFast",
    "model_max_length": 1048576,
    "bos_token": {"content": "<bos>"},
    "eos_token": {"content": "<eos>"},
}
generation_config = {
    "_from_model_config": True,
    "bos_token_id": 0,
    "eos_token_id": 1,
    "do_sample": True,
    "temperature": 1.0,
    "top_p": 1.0,
    "transformers_version": "4.46.3",
}
index = {
    "metadata": {"total_size": 8},
    "weight_map": {
        "model.embed_tokens.weight": "model-00001-of-00001.safetensors"
    },
}
manifest = {
    "schema": "yvex.source_manifest.v1",
    "status": "complete",
    "source": {
        "kind": "huggingface",
        "repo": "deepseek-ai/DeepSeek-V4-Flash",
        "revision": revision,
    },
    "local": {"path": str(root)},
}
files = {
    "config.json": config,
    "tokenizer.json": tokenizer,
    "tokenizer_config.json": tokenizer_config,
    "generation_config.json": generation_config,
    "model.safetensors.index.json": index,
    "source-manifest.json": manifest,
}
for name, value in files.items():
    (root / name).write_text(json.dumps(value), encoding="utf-8")
header = json.dumps({
    "model.embed_tokens.weight": {
        "dtype": "BF16",
        "shape": [2, 2],
        "data_offsets": [0, 8],
    }
}, separators=(",", ":")).encode()
shard_name = "model-00001-of-00001.safetensors"
(root / shard_name).write_bytes(struct.pack("<Q", len(header)) + header + b"12345678")
metadata_root = root / ".cache/huggingface/download"
metadata_root.mkdir(parents=True, exist_ok=True)
for name in ("config.json", "tokenizer.json", "tokenizer_config.json",
             "generation_config.json",
             "model.safetensors.index.json", shard_name):
    (metadata_root / f"{name}.metadata").write_text(
        f"{revision}\nblob-id\n0\n", encoding="utf-8")
PY

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$VERIFIED_MODELS" --strict > "$OUT_DIR/deepseek-verified.out"
grep 'status: exact-source-verified' "$OUT_DIR/deepseek-verified.out" >/dev/null || fail "valid source did not verify"
grep 'verification: verified' "$OUT_DIR/deepseek-verified.out" >/dev/null || fail "normal output lost verified state"
grep 'next: V010.GGUF.QTYPE.ABI.1' "$OUT_DIR/deepseek-verified.out" >/dev/null || fail "verified handoff is wrong"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$VERIFIED_MODELS" --output table --strict > "$OUT_DIR/deepseek-verified-table.out"
grep 'verified' "$OUT_DIR/deepseek-verified-table.out" >/dev/null || fail "table output lost verified state"
grep 'top_blocker: none' "$OUT_DIR/deepseek-verified-table.out" >/dev/null || fail "verified table output invented a blocker"
grep 'V010.GGUF.QTYPE.ABI.1' "$OUT_DIR/deepseek-verified-table.out" >/dev/null || fail "table handoff is wrong"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$VERIFIED_MODELS" --audit --strict > "$OUT_DIR/deepseek-verified-audit.out"
grep 'source_verification_status: verified' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "audit output lost verified state"
grep 'config_compress_ratio_count: 44' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "audit output lost raw architecture facts"
grep 'tokenizer_model_type: BPE' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "audit output lost tokenizer structure"
grep 'generation_config_status: verified' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "audit output lost generation sidecar structure"
grep 'release_qtype: unselected' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "source verification selected a qtype"
grep 'generation: unsupported-full-model' "$OUT_DIR/deepseek-verified-audit.out" >/dev/null || fail "source verification promoted generation"

"$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --models-root "$VERIFIED_MODELS" --output json --strict > "$OUT_DIR/deepseek-verified.json"
jq -e '.verification == "verified" and .source_kind == "huggingface" and .source_verification_blocker_count == 0 and .blocker_count == 6 and .blocker_0 == "missing-deepseek-architecture-ir" and .next == "V010.GGUF.QTYPE.ABI.1" and .shard_count == 1 and .header_shard_count == 1 and .header_tensor_count == 1 and .config_valid == true and .tokenizer_valid == true and .generation_config_valid == true and .shard_index_valid == true and .artifact_status == "not-produced" and .runtime == "unsupported" and .generation == "unsupported"' "$OUT_DIR/deepseek-verified.json" >/dev/null || fail "verified JSON source facts disagree"

if "$YVEX_BIN" source-manifest report --family deepseek --release v0.1.0 \
  --target deepseek4-v4-flash-selected-embed --models-root "$VERIFIED_MODELS" \
  > "$OUT_DIR/deepseek-selected-target.out" 2> "$OUT_DIR/deepseek-selected-target.err"; then
  fail "selected proof alias remained eligible for exact source verification"
fi
grep 'unsupported target' "$OUT_DIR/deepseek-selected-target.err" >/dev/null || fail "selected alias refusal is not explicit"

if "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 \
  --strict > "$OUT_DIR/qwen-strict.out" 2> "$OUT_DIR/qwen-strict.err"; then
  fail "strict exact-source mode was accepted for a non-release family"
fi
grep -- '--strict is available only for the canonical DeepSeek target' \
  "$OUT_DIR/qwen-strict.err" >/dev/null || fail "non-release strict refusal is not explicit"
