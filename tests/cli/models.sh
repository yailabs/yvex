#!/usr/bin/env sh
set -eu

. tests/support/cleanup.sh

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/models-cli}
REG="$ROOT/models.local.json"
CATALOG_ROOT="$ROOT/catalog-empty"
GGUF="$ROOT/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf"

matches() {
  file=$1
  pattern=$2
  grep -E -- "$pattern" "$file" >/dev/null
}

make_missing_role_source() {
  dir=$1
  variant=${2:-complete}
  yvex_test_cleanup "$dir"
  mkdir -p "$dir"
  if [ "$variant" != "missing-metadata" ]; then
    cat > "$dir/config.json" <<'JSON'
{
  "model_type": "qwen",
  "vocab_size": 16,
  "hidden_size": 8,
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0,
  "unk_token_id": 3
}
JSON
    cat > "$dir/tokenizer_config.json" <<'JSON'
{
  "tokenizer_class": "PreTrainedTokenizerFast",
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0,
  "unk_token_id": 3
}
JSON
    cat > "$dir/special_tokens_map.json" <<'JSON'
{
  "bos_token": "<s>",
  "eos_token": "</s>",
  "unk_token": "<unk>",
  "pad_token": "<pad>",
  "additional_special_tokens": []
}
JSON
    cat > "$dir/generation_config.json" <<'JSON'
{
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0
}
JSON
    printf '{"version":"1.0","model":{"type":"BPE"}}\n' > "$dir/tokenizer.json"
  fi
  python3 - "$dir/model.safetensors" "$variant" <<'PY'
import json
import struct
import sys

variant = sys.argv[2]
items = [
    ("model.embed_tokens.weight", [16, 8]),
    ("model.layers.0.self_attn.q_proj.weight", [8, 8]),
    ("model.layers.0.self_attn.k_proj.weight", [8, 8]),
    ("model.layers.0.self_attn.v_proj.weight", [8, 8]),
    ("model.layers.0.self_attn.o_proj.weight", [8, 8]),
    ("model.layers.0.mlp.gate_proj.weight", [16, 8]),
    ("model.layers.0.mlp.up_proj.weight", [16, 8]),
    ("model.layers.0.mlp.down_proj.weight", [8, 16]),
    ("model.layers.0.input_layernorm.weight", [8]),
    ("model.layers.0.post_attention_layernorm.weight", [8]),
    ("model.norm.weight", [8]),
    ("lm_head.weight", [16, 8]),
]
if variant == "missing-attention-k":
    items = [item for item in items if item[0] != "model.layers.0.self_attn.k_proj.weight"]
if variant == "missing-output-head":
    items = [item for item in items if item[0] != "lm_head.weight"]
if variant == "ambiguous-output-head":
    items.append(("output.weight", [16, 8]))
offset = 0
header = {}
for name, shape in items:
    size = 1
    for dim in shape:
        size *= dim
    nbytes = size * 4
    header[name] = {
        "dtype": "F32",
        "shape": shape,
        "data_offsets": [offset, offset + nbytes],
    }
    offset += nbytes
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY
}

expect_rc() {
  expected=$1
  shift
  set +e
  "$@"
  rc=$?
  set -e
  test "$rc" -eq "$expected"
}

assert_output_contract_pass() {
  file=$1
  python3 tests/support/human_field.py "$file" 'status: pass'
  python3 tests/support/human_field.py "$file" 'runtime_claim: unsupported'
  python3 tests/support/human_field.py "$file" 'generation: unsupported-full-model'
  python3 tests/support/human_field.py "$file" 'benchmark_status: not-measured'
  python3 tests/support/human_field.py "$file" 'release_ready: false'
  python3 tests/support/human_field.py "$file" 'boundary: output-contract check only; no runtime/generation claim'
  ! grep 'status: fail-' "$file"
  ! grep 'generation_ready: tr''ue' "$file"
  ! grep 'release_ready: tr''ue' "$file"
  ! grep 'benchmark_status: mea''sured' "$file"
}

write_fake_transformer_safetensors() {
  out=$1
  variant=${2:-complete}
  dtype=${3:-F32}
  mkdir -p "$(dirname "$out")"
  python3 - "$out" "$variant" "$dtype" <<'PY'
import json
import struct
import sys

variant = sys.argv[2]
dtype = sys.argv[3]
element_bytes = {
    "F32": 4,
    "F16": 2,
    "BF16": 2,
}.get(dtype, 4)
tensor_bytes = element_bytes * 4
names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
]
if variant == "qwen-incomplete":
    names = [
        "model.layers.0.self_attn.q_proj.weight",
        "model.layers.0.self_attn.k_proj.weight",
        "model.layers.0.self_attn.v_proj.weight",
        "model.layers.0.self_attn.o_proj.weight",
        "model.layers.0.input_layernorm.weight",
        "model.layers.0.post_attention_layernorm.weight",
        "model.layers.0.mlp.experts.0.gate_proj.weight",
        "model.norm.weight",
        "lm_head.weight",
        "model.unmapped.qwen_probe.weight",
    ]
elif variant == "qwen-coverage":
    names = [
        "model.language_model.embed_tokens.weight",
        "model.language_model.layers.0.self_attn.q_proj.weight",
        "model.language_model.layers.0.self_attn.k_proj.weight",
        "model.language_model.layers.0.self_attn.v_proj.weight",
        "model.language_model.layers.0.self_attn.o_proj.weight",
        "model.language_model.layers.0.self_attn.q_norm.weight",
        "model.language_model.layers.0.self_attn.k_norm.weight",
        "model.language_model.layers.0.input_layernorm.weight",
        "model.language_model.layers.0.post_attention_layernorm.weight",
        "model.language_model.layers.0.linear_attn.A_log",
        "model.language_model.layers.0.linear_attn.dt_bias",
        "model.language_model.layers.0.linear_attn.conv1d.weight",
        "model.language_model.layers.0.linear_attn.in_proj_a.weight",
        "model.language_model.layers.0.linear_attn.in_proj_b.weight",
        "model.language_model.layers.0.linear_attn.norm.weight",
        "model.language_model.layers.0.mlp.gate.weight",
        "model.language_model.layers.0.mlp.experts.gate_up_proj",
        "model.language_model.layers.0.mlp.experts.down_proj",
        "model.language_model.layers.0.mlp.shared_expert.gate_proj.weight",
        "model.language_model.layers.0.mlp.shared_expert.up_proj.weight",
        "model.language_model.layers.0.mlp.shared_expert.down_proj.weight",
        "model.language_model.layers.0.mlp.shared_expert_gate.weight",
        "model.language_model.norm.weight",
        "lm_head.weight",
        "model.language_model.layers.0.optional_probe.weight",
    ]
elif variant == "gemma-incomplete-no-head":
    names = [
        "model.layers.0.self_attn.q_proj.weight",
        "model.layers.0.self_attn.k_proj.weight",
        "model.layers.0.self_attn.v_proj.weight",
        "model.layers.0.self_attn.o_proj.weight",
        "model.layers.0.mlp.gate_proj.weight",
        "model.layers.0.mlp.up_proj.weight",
        "model.layers.0.mlp.down_proj.weight",
        "model.layers.0.input_layernorm.weight",
        "model.layers.0.post_attention_layernorm.weight",
        "model.norm.weight",
        "model.unmapped.gemma_probe.weight",
    ]
elif variant == "gemma-language-head":
    names = [
        "model.language_model.embed_tokens.weight",
        "model.language_model.layers.0.self_attn.q_proj.weight",
        "model.language_model.layers.0.self_attn.k_proj.weight",
        "model.language_model.layers.0.self_attn.v_proj.weight",
        "model.language_model.layers.0.self_attn.o_proj.weight",
        "model.language_model.layers.0.self_attn.q_norm.weight",
        "model.language_model.layers.0.self_attn.k_norm.weight",
        "model.language_model.layers.0.mlp.gate_proj.weight",
        "model.language_model.layers.0.mlp.up_proj.weight",
        "model.language_model.layers.0.mlp.down_proj.weight",
        "model.language_model.layers.0.input_layernorm.weight",
        "model.language_model.layers.0.post_attention_layernorm.weight",
        "model.language_model.layers.0.pre_feedforward_layernorm.weight",
        "model.language_model.layers.0.post_feedforward_layernorm.weight",
        "model.language_model.layers.0.layer_scalar",
        "model.language_model.norm.weight",
        "model.language_model.lm_head.weight",
    ]
elif variant == "gemma-language-tied":
    names = [
        "model.language_model.embed_tokens.weight",
        "model.language_model.layers.0.self_attn.q_proj.weight",
        "model.language_model.layers.0.self_attn.k_proj.weight",
        "model.language_model.layers.0.self_attn.v_proj.weight",
        "model.language_model.layers.0.self_attn.o_proj.weight",
        "model.language_model.layers.0.self_attn.q_norm.weight",
        "model.language_model.layers.0.self_attn.k_norm.weight",
        "model.language_model.layers.0.mlp.gate_proj.weight",
        "model.language_model.layers.0.mlp.up_proj.weight",
        "model.language_model.layers.0.mlp.down_proj.weight",
        "model.language_model.layers.0.input_layernorm.weight",
        "model.language_model.layers.0.post_attention_layernorm.weight",
        "model.language_model.layers.0.pre_feedforward_layernorm.weight",
        "model.language_model.layers.0.post_feedforward_layernorm.weight",
        "model.language_model.layers.0.layer_scalar",
        "model.language_model.norm.weight",
    ]
elif variant == "gemma-language-no-head":
    names = [
        "model.language_model.embed_tokens.weight",
        "model.language_model.layers.0.self_attn.q_proj.weight",
        "model.language_model.layers.0.self_attn.k_proj.weight",
        "model.language_model.layers.0.self_attn.v_proj.weight",
        "model.language_model.layers.0.self_attn.o_proj.weight",
        "model.language_model.layers.0.mlp.gate_proj.weight",
        "model.language_model.layers.0.mlp.up_proj.weight",
        "model.language_model.layers.0.mlp.down_proj.weight",
        "model.language_model.layers.0.input_layernorm.weight",
        "model.language_model.layers.0.post_attention_layernorm.weight",
        "model.language_model.layers.0.pre_feedforward_layernorm.weight",
        "model.language_model.layers.0.post_feedforward_layernorm.weight",
        "model.language_model.layers.0.layer_scalar",
        "model.language_model.norm.weight",
        "model.language_model.layers.0.unmapped_probe.weight",
    ]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": dtype,
        "shape": [2, 2],
        "data_offsets": [offset, offset + tensor_bytes],
    }
    offset += tensor_bytes
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY
}

write_fake_tokenizer_sidecars() {
  dir=$1
  family=$2
  mkdir -p "$dir"
  cat > "$dir/config.json" <<JSON
{
  "model_type": "$family",
  "vocab_size": 2,
  "hidden_size": 2,
  "bos_token_id": 1,
  "eos_token_id": 1,
  "pad_token_id": 0,
  "unk_token_id": 0,
  "tie_word_embeddings": false
}
JSON
  cat > "$dir/tokenizer_config.json" <<'JSON'
{
  "tokenizer_class": "PreTrainedTokenizerFast",
  "bos_token": "<s>",
  "eos_token": "</s>",
  "unk_token": "<unk>",
  "pad_token": "<pad>",
  "bos_token_id": 1,
  "eos_token_id": 1,
  "pad_token_id": 0,
  "unk_token_id": 0,
  "chat_template": "{{ bos_token }}{{ messages }}"
}
JSON
  cat > "$dir/special_tokens_map.json" <<'JSON'
{
  "bos_token": "<s>",
  "eos_token": "</s>",
  "unk_token": "<unk>",
  "pad_token": "<pad>",
  "additional_special_tokens": ["<start_of_turn>", "<end_of_turn>"]
}
JSON
  cat > "$dir/generation_config.json" <<'JSON'
{
  "bos_token_id": 1,
  "eos_token_id": 1,
  "pad_token_id": 0
}
JSON
  cat > "$dir/tokenizer.json" <<'JSON'
{
  "version": "1.0",
  "model": {"type": "BPE"},
  "vocab_size": 2,
  "added_tokens": [{"id": 1, "content": "</s>"}]
}
JSON
  if [ "$family" = "qwen" ]; then
    printf '{"<pad>":0,"</s>":1}\n' > "$dir/vocab.json"
    printf '#version: 0.2\n< p\n' > "$dir/merges.txt"
    printf '{{ bos_token }}{{ messages }}\n' > "$dir/chat_template.jinja"
  else
    rm -f "$dir/vocab.json" "$dir/merges.txt" "$dir/chat_template.jinja"
  fi
}

yvex_test_cleanup "$ROOT"
mkdir -p "$ROOT"
case "$ROOT" in
  /*)
    XDG_CONFIG_HOME="$ROOT/xdg"
    XDG_DATA_HOME="$ROOT/xdg-data"
    XDG_RUNTIME_DIR="$ROOT/runtime"
    YVEX_DATA_DIR="$ROOT/data"
    ;;
  *)
    XDG_CONFIG_HOME="$PWD/$ROOT/xdg"
    XDG_DATA_HOME="$PWD/$ROOT/xdg-data"
    XDG_RUNTIME_DIR="$PWD/$ROOT/runtime"
    YVEX_DATA_DIR="$PWD/$ROOT/data"
    ;;
esac
export XDG_CONFIG_HOME XDG_DATA_HOME XDG_RUNTIME_DIR YVEX_DATA_DIR
mkdir -p "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_RUNTIME_DIR" "$YVEX_DATA_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
mkdir -p "$CATALOG_ROOT"

FAKE_HF="$PWD/tests/fixtures/bin/fake-hf"
FAKE_HF_LOG="$ROOT/fake-hf-discovery.log"
export YVEX_FAKE_HF_LOG="$FAKE_HF_LOG"

RECON_ROOT="$ROOT/catalog-reconcile"
RECON_REV=b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08
RECON_SOURCE="$RECON_ROOT/source/hf/MiniMaxAI/MiniMax-H3/$RECON_REV"
mkdir -p "$RECON_SOURCE"
cat > "$RECON_SOURCE/yvex-source-acquisition.json" <<JSON
{"schema":"yvex.source-acquisition.v1","repository":"MiniMaxAI/MiniMax-H3",\
"revision":"$RECON_REV","acquisition_complete":true,"source_bytes":144016000740}
JSON
mkdir -p "$RECON_ROOT/registry/minimax-h3"
cat > "$RECON_ROOT/registry/minimax-h3/MiniMax-H3.download.json" <<JSON
{"schema":"yvex.model_download.registry.v1","target_id":"MiniMax-H3",
"family":"minimax-h3","provider":"huggingface","repo_id":"MiniMaxAI/MiniMax-H3",
"revision":"$RECON_REV","local_source_dir":"$RECON_SOURCE",
"status":"model-download-pass","total_regular_file_bytes":144016000740,
"safetensors_count":1,"upstream_identity_verified":true,"payload_hash_verified":true}
JSON
"$YVEX_BIN" source list --models-root "$RECON_ROOT" --registry "$REG" --json \
  > "$ROOT/reconciled-source.json"
python3 - "$ROOT/reconciled-source.json" <<'PY'
import json
import sys

sources = json.load(open(sys.argv[1], encoding="utf-8"))["sources"]
source = next(item for item in sources if item["name"] == "MiniMax-H3")
assert source["representation"] == "safetensors-source"
assert source["acquisition_state"] == "source-acquired"
assert source["verification_state"] == "payload-verification-recorded"
assert source["size_bytes"] == 144016000740
PY

COLUMNS=240 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search "MiniMax H3" --limit 2 \
  --models-root "$RECON_ROOT" \
  > "$ROOT/search.out"
grep 'REMOTE MODELS · "MiniMax H3"' "$ROOT/search.out"
grep 'MODEL.*PROVIDER.*REPOSITORY.*ARCH.*FORMATS.*SIZE.*STATE.*YVEX.*LOCATION' "$ROOT/search.out"
grep 'MiniMax-H3.*Hugging Face.*MiniMaxAI/MiniMax-H3.*safetensors.*source.*supported.*hf://MiniMaxAI/MiniMax-H3@' "$ROOT/search.out"
grep 'MiniMax-H3-GGUF.*Hugging Face.*unsloth/MiniMax-H3-GGUF.*gguf.*remote.*inspect.*hf://unsloth/MiniMax-H3-GGUF@' "$ROOT/search.out"
! grep 'package-preparation\|source-ingest\|physical-inspection' "$ROOT/search.out"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search "MiniMax H3" --all --json \
  --models-root "$RECON_ROOT" > "$ROOT/search-all.json"
python3 - "$ROOT/search-all.json" <<'PY'
import json
import sys

catalog = json.load(open(sys.argv[1], encoding="utf-8"))
assert catalog["schema"] == "yvex.model-catalog-projection.v1"
assert catalog["authorities"] == ["remote-provider", "local-catalog"]
models = catalog["models"]
assert models[0]["repository"] == "MiniMaxAI/MiniMax-H3"
assert models[0]["kind"] == "full model"
assert models[0]["model_identity"] == "MiniMaxAI/MiniMax-H3"
assert models[0]["family_affinity"] == "minimax-h3"
assert models[0]["local_source"] is True
by_repo = {model["repository"]: model for model in models}
assert by_repo["fal/MiniMax-H3-Realism-People-LoRA"]["kind"] == "adapter / LoRA"
assert by_repo["fal/MiniMax-H3-Realism-People-LoRA"]["model_identity"] == ""
assert by_repo["LBH-123-AI/Minimax_h3_latent_Upscaler"]["kind"] == "component"
assert by_repo["unsloth/MiniMax-H3-GGUF"]["kind"] == "conversion"
assert by_repo["community/MiniMax-H3-finetune"]["kind"] == "derivative / fork"
assert by_repo["community/unknown-model"]["kind"] == "unknown"
PY

YVEX_FAKE_HF_RESOLVED_SHA=42ed227ee7df40d41602854ae760620d6eb651fe \
  YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search "MiniMax H3" --limit 1 --json \
  --models-root "$RECON_ROOT" > "$ROOT/search-other-revision.json"
python3 - "$ROOT/search-other-revision.json" "$RECON_REV" <<'PY'
import json
import sys

model = json.load(open(sys.argv[1], encoding="utf-8"))["models"][0]
assert model["repository"] == "MiniMaxAI/MiniMax-H3"
assert model["kind"] == "full model"
assert model["canonical"] is True
assert model["local_source"] is False
assert model["local_related_revision"] is True
assert model["local_source_revision"] == sys.argv[2]
assert model["product_status"] == "acquirable"
PY

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search "MiniMax H3" --page 2 --limit 2 \
  --models-root "$RECON_ROOT" --json \
  > "$ROOT/search-page.json"
python3 - "$ROOT/search-page.json" <<'PY'
import json
import sys

catalog = json.load(open(sys.argv[1], encoding="utf-8"))
assert catalog["schema"] == "yvex.model-catalog-projection.v1"
assert [model["repository"] for model in catalog["models"]] == [
    "community/MiniMax-H3-finetune",
    "fal/MiniMax-H3-Realism-People-LoRA",
]
PY

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect MiniMaxAI/MiniMax-H3 \
  --models-root "$RECON_ROOT" --audit \
  > "$ROOT/remote-inspect.out"
python3 tests/support/human_field.py "$ROOT/remote-inspect.out" 'revision_reference: default'
grep 'resolved_revision: b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08' \
  "$ROOT/remote-inspect.out"
python3 tests/support/human_field.py "$ROOT/remote-inspect.out" 'family: minimax-h3'
python3 tests/support/human_field.py "$ROOT/remote-inspect.out" 'kind: full model'
python3 tests/support/human_field.py "$ROOT/remote-inspect.out" 'local_source: true'
python3 tests/support/human_field.py "$ROOT/remote-inspect.out" 'base_model: MiniMaxAI/MiniMax-H3-Base'
grep 'identity=safetensors-source format=safetensors precision=BF16+F16' \
  "$ROOT/remote-inspect.out"
grep 'identity=gguf-Q4_K_M format=gguf precision=Q4_K_M evidence=filename-hint' \
  "$ROOT/remote-inspect.out"
grep 'selector=model-Q4_K_M.gguf' "$ROOT/remote-inspect.out"
grep 'identity=gguf-IQ2_XXS format=gguf precision=IQ2_XXS evidence=filename-hint' \
  "$ROOT/remote-inspect.out"
grep 'file\[0\]: path=config.json kind=configuration representation=configuration' \
  "$ROOT/remote-inspect.out"
grep 'path=tokenizer.json kind=tokenizer representation=tokenizer' "$ROOT/remote-inspect.out"
grep 'path=README.md kind=sidecar representation=sidecar' "$ROOT/remote-inspect.out"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect MiniMaxAI/MiniMax-H3 \
  --models-root "$RECON_ROOT" --json \
  > "$ROOT/remote-inspect.json"
python3 - "$ROOT/remote-inspect.json" <<'PY'
import json
import sys

model = json.load(open(sys.argv[1], encoding="utf-8"))["models"][0]
assert model["requested_revision"] == "default"
assert model["resolved_revision"] == "b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
assert model["local_source"] is True
assert {file["kind"] for file in model["files"]} >= {
    "configuration",
    "tokenizer",
    "safetensors",
    "gguf",
    "sidecar",
}
assert next(file for file in model["files"] if file["path"] == "model-Q4_K_M.gguf")[
    "representation"
] == "gguf-Q4_K_M"
PY

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect unsloth/MiniMax-H3-GGUF \
  --output table > "$ROOT/inspect-gguf.out"
python3 tests/support/human_field.py "$ROOT/inspect-gguf.out" 'kind: conversion'
grep 'Q4_K_M (filename)' "$ROOT/inspect-gguf.out"
grep 'acquire-and-inspect-required' "$ROOT/inspect-gguf.out"

expect_rc 1 env YVEX_FAKE_HF_DISCOVERY_MODE=model-not-found YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect missing/model \
  > "$ROOT/inspect-model-missing.out" 2> "$ROOT/inspect-model-missing.err"
grep 'remote model was not found' "$ROOT/inspect-model-missing.err"
expect_rc 1 env YVEX_FAKE_HF_DISCOVERY_MODE=revision-not-found YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect MiniMaxAI/MiniMax-H3 --revision DOES_NOT_EXIST \
  > "$ROOT/inspect-revision-missing.out" 2> "$ROOT/inspect-revision-missing.err"
grep 'remote revision or reference was not found' "$ROOT/inspect-revision-missing.err"

! grep -A2 '^  download$' "$FAKE_HF_LOG"

expect_rc 2 "$YVEX_BIN" model search MiniMax --interactive \
    > "$ROOT/search-interactive.out" 2> "$ROOT/search-interactive.err"
python3 tests/support/human_field.py "$ROOT/search-interactive.err" 'model search: unknown flag: --interactive'

expect_rc 1 env YVEX_FAKE_HF_RESOLVED_SHA=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
  YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect MiniMaxAI/MiniMax-H3 \
  --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 \
  > "$ROOT/inspect-revision-mismatch.out" 2> "$ROOT/inspect-revision-mismatch.err"
grep 'provider resolved revision does not match the requested identity' \
  "$ROOT/inspect-revision-mismatch.err"

YVEX_FAKE_HF_RESOLVED_SHA=62af8fffb2f7030cac4de2f0169f5b8d1101b646 \
  YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect \
  deepseek-ai/DeepSeek-V4-Flash-DSpark --revision \
  62af8fffb2f7030cac4de2f0169f5b8d1101b646 --audit > "$ROOT/inspect-deepseek.out"
python3 tests/support/human_field.py "$ROOT/inspect-deepseek.out" 'family: deepseek'
python3 tests/support/human_field.py "$ROOT/inspect-deepseek.out" 'support_stage: package-preparation'

expect_rc 4 env YVEX_FAKE_HF_DISCOVERY_MODE=unsafe-file YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect MiniMaxAI/MiniMax-H3 \
  > "$ROOT/inspect-unsafe-file.out" 2> "$ROOT/inspect-unsafe-file.err"
grep 'provider file listing is malformed or oversized' "$ROOT/inspect-unsafe-file.err"

YVEX_FAKE_HF_DISCOVERY_MODE=empty YVEX_HF_CLI="$FAKE_HF" \
  "$YVEX_BIN" model search none > "$ROOT/search-empty.out"
grep '^REMOTE MODELS · "none"' "$ROOT/search-empty.out"

expect_rc 4 env YVEX_FAKE_HF_DISCOVERY_MODE=malformed YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search malformed > "$ROOT/search-malformed.out" \
  2> "$ROOT/search-malformed.err"
grep 'provider search did not return a JSON array' "$ROOT/search-malformed.err"

expect_rc 1 env YVEX_FAKE_HF_DISCOVERY_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search private > "$ROOT/search-auth.out" \
  2> "$ROOT/search-auth.err"
grep 'authentication is required' "$ROOT/search-auth.err"

expect_rc 1 env YVEX_FAKE_HF_FAIL=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" model search failed > "$ROOT/search-fail.out" \
  2> "$ROOT/search-fail.err"
grep 'provider operation failed' "$ROOT/search-fail.err"

HF_TOKEN='discovery-secret-must-not-leak' YVEX_HF_CLI="$FAKE_HF" \
  "$YVEX_BIN" model search redaction --limit 1 --json > "$ROOT/search-redaction.json"
! grep 'discovery-secret-must-not-leak' "$FAKE_HF_LOG"
! grep 'discovery-secret-must-not-leak' "$ROOT/search-redaction.json"

"$YVEX_BIN" compile artifact emit \
  --out "$GGUF" \
  --model-name model-registry-test \
  --arch llama \
  --overwrite >/dev/null

BINDING="$PWD/$ROOT/runtime.binding"
ARTIFACT="$PWD/$GGUF"
printf 'binding fixture\n' > "$BINDING"

"$YVEX_BIN" profile scan --root "$ROOT" --registry "$REG" > "$ROOT/scan.out"
python3 tests/support/human_field.py "$ROOT/scan.out" 'candidate: deepseek4-v4-flash-dspark-selected-embed'
python3 tests/support/human_field.py "$ROOT/scan.out" 'status: models-scan'

"$YVEX_BIN" profile create --path "$ARTIFACT" --registry "$REG" \
  --support-level selected-tensor-materialized \
  --runtime-binding "$BINDING" --target deepseek4-v4-flash-dspark \
  --backend cpu --execution-strategy speculative --ctx 4096 > "$ROOT/add.out"
python3 tests/support/human_field.py "$ROOT/add.out" 'alias: deepseek4-v4-flash-dspark-selected-embed'
python3 tests/support/human_field.py "$ROOT/add.out" 'status: models-added'
test -f "$REG"

"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" > "$ROOT/list.out"
grep 'deepseek4-v4-flash-dspark-selected-embed' "$ROOT/list.out"
grep 'DEPLOYMENT PROFILES' "$ROOT/list.out"
grep 'cpu/text/speculative' "$ROOT/list.out"
grep 'context=4096' "$ROOT/list.out"
grep 'runnable' "$ROOT/list.out"

"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" --output table \
  > "$ROOT/list-table.out"
grep 'deepseek4-v4-flash-dspark-selected-embed' "$ROOT/list-table.out"
grep 'runnable' "$ROOT/list-table.out"

"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" --json \
  > "$ROOT/list-audit.out"
python3 - "$ROOT/list-audit.out" <<'PY'
import json
import sys

profiles = json.load(open(sys.argv[1], encoding="utf-8"))["profiles"]
profile, = profiles
assert profile["identity"] == "deepseek4-v4-flash-dspark-selected-embed"
assert profile["launchable"] is False
assert profile["blocker"].startswith("malformed-binding:")
assert profile["backend"] == "cpu"
assert profile["execution_strategy"] == "speculative"
PY

"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" --output nope \
  > "$ROOT/list-bad-output.out" 2> "$ROOT/list-bad-output.err" && exit 1 || true
grep 'model plumbing --output requires table|audit|json' "$ROOT/list-bad-output.err"

"$YVEX_BIN" profile show deepseek4-v4-flash-dspark-selected-embed --registry "$REG" > "$ROOT/inspect.out"
python3 tests/support/human_field.py "$ROOT/inspect.out" 'model: deepseek4-v4-flash-dspark-selected-embed'
python3 tests/support/human_field.py "$ROOT/inspect.out" 'family: deepseek4 class=embed'
python3 tests/support/human_field.py "$ROOT/inspect.out" 'artifact: support=selected-tensor-materialized execution=not-established-by-inspection'
grep 'runtime profile: unavailable (malformed-binding:' \
  "$ROOT/inspect.out"
python3 tests/support/human_field.py "$ROOT/inspect.out" 'status: models-inspect'
test "$(wc -l < "$ROOT/inspect.out")" -le 8

"$YVEX_BIN" profile show deepseek4-v4-flash-dspark-selected-embed --registry "$REG" --audit > "$ROOT/inspect-audit.out"
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'alias: deepseek4-v4-flash-dspark-selected-embed'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'artifact_support_level: selected-tensor-materialized'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'artifact_execution_ready: false'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'startup_profile_status: unavailable'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'deployment_compatibility: malformed-binding'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'gguf: '
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'tensor_count: 1'
python3 tests/support/human_field.py "$ROOT/inspect-audit.out" 'status: models-inspect'

COMPOSITE_ROOT=$(realpath "$ROOT")
cat > "$COMPOSITE_ROOT/repository.json" <<JSON
{"repository":"MiniMaxAI/MiniMax-H3","requested_revision":"$RECON_REV",\
"resolved_revision":"$RECON_REV"}
JSON
"$YVEX_BIN" profile create --path "$ARTIFACT" --registry "$REG" \
  --alias minimax-h3-fl2va-runtime-media --family minimax-h3 \
  --startup-profile composite --installation-root "$COMPOSITE_ROOT" \
  --target minimax-h3-fl2va --backend cuda \
  > "$ROOT/add-composite.out"
"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" \
  > "$ROOT/list-composite.out"
grep 'minimax-h3-fl2va-runtime-media' "$ROOT/list-composite.out"
grep 'minimax-h3-fl2va · 0/1 runnable' "$ROOT/list-composite.out"
grep 'cuda/media/not-applicable' "$ROOT/list-composite.out"
grep 'required composite deployment component is unavailable' \
  "$ROOT/list-composite.out"

# Presence alone is not a composite deployment binding. Four readable GGUF
# files with the wrong family/component identities remain non-launchable.
mkdir -p "$COMPOSITE_ROOT/physical-v3" "$COMPOSITE_ROOT/physical-v4" \
  "$COMPOSITE_ROOT/physical"
cp "$ARTIFACT" "$COMPOSITE_ROOT/physical-v3/text_encoder.gguf"
cp "$ARTIFACT" "$COMPOSITE_ROOT/physical-v4/transformer.gguf"
cp "$ARTIFACT" "$COMPOSITE_ROOT/physical/video_vae.gguf"
cp "$ARTIFACT" "$COMPOSITE_ROOT/physical/audio_vae.gguf"
"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" \
  > "$ROOT/list-composite-mismatch.out"
grep 'minimax-h3-fl2va · 0/1 runnable' "$ROOT/list-composite-mismatch.out"
grep 'component does not match the current family execution contract' \
  "$ROOT/list-composite-mismatch.out"

"$YVEX_BIN" model list --models-root "$CATALOG_ROOT" --registry "$REG" \
  > "$ROOT/library-friendly.out"
grep '^MODELS$' "$ROOT/library-friendly.out"
python3 - "$ROOT/library-friendly.out" <<'PY'
from pathlib import Path
import re, sys
text = Path(sys.argv[1]).read_text()
for name in ('v4-flash', 'minimax-h3-fl2va'):
    record = text.split(name + '\n', 1)[1].split('\n\n', 1)[0]
    assert re.search(r'^\s*state\s+BLOCKED$', record, re.M)
    assert re.search(r'^\s*execution\s+not current$', record, re.M)
PY
! grep 'provider:huggingface' "$ROOT/library-friendly.out"
"$YVEX_BIN" source list --models-root "$RECON_ROOT" --registry "$REG" \
  > "$ROOT/sources-friendly.out"
grep '^SOURCES$' "$ROOT/sources-friendly.out"
python3 tests/support/human_field.py "$ROOT/sources-friendly.out" 'model binding: '
"$YVEX_BIN" artifact list --models-root "$CATALOG_ROOT" --registry "$REG" \
  > "$ROOT/artifacts-friendly.out"
grep '^ARTIFACTS$' "$ROOT/artifacts-friendly.out"
grep 'runnable profiles' "$ROOT/artifacts-friendly.out"
! grep 'ready=' "$ROOT/artifacts-friendly.out"
"$YVEX_BIN" model list --models-root "$CATALOG_ROOT" --registry "$REG" --json \
  > "$ROOT/library-friendly.json"
"$YVEX_BIN" artifact list --models-root "$CATALOG_ROOT" --registry "$REG" --json \
  > "$ROOT/artifacts-friendly.json"
"$YVEX_BIN" profile list --models-root "$CATALOG_ROOT" --registry "$REG" --json \
  > "$ROOT/profiles-friendly.json"
python3 - "$ROOT/library-friendly.json" "$ROOT/artifacts-friendly.json" \
  "$ROOT/profiles-friendly.json" <<'PY'
import json
import sys

models = json.load(open(sys.argv[1], encoding="utf-8"))["models"]
artifacts = json.load(open(sys.argv[2], encoding="utf-8"))["artifacts"]
profiles = json.load(open(sys.argv[3], encoding="utf-8"))["profiles"]
assert {model["name"] for model in models} == {
    "DeepSeek-V4-Flash",
    "minimax-h3-fl2va",
}
assert all(artifact["profile_count"] == 1 for artifact in artifacts)
assert all(artifact["launchable_profile_count"] == 0 for artifact in artifacts)
assert all(profile["deployment_class"] for profile in profiles)
assert all("runtime_binding" in profile for profile in profiles)
PY
"$YVEX_BIN" profile show minimax-h3-fl2va-runtime-media --registry "$REG" --audit \
  > "$ROOT/show-composite.out"
python3 tests/support/human_field.py "$ROOT/show-composite.out" 'runtime_profile: composite'
python3 tests/support/human_field.py "$ROOT/show-composite.out" "runtime_installation: $COMPOSITE_ROOT"
grep -E '^[[:space:]]*runtime_binding[[:space:]]*$' "$ROOT/show-composite.out"
python3 tests/support/human_field.py "$ROOT/show-composite.out" 'runtime_context: 0'
python3 tests/support/human_field.py "$ROOT/show-composite.out" 'startup_profile_status: unavailable'
python3 tests/support/human_field.py "$ROOT/show-composite.out" 'deployment_compatibility: artifact-mismatch'
YVEX_MODELS_REGISTRY="$REG" YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source inspect \
  MiniMaxAI/MiniMax-H3 --models-root "$RECON_ROOT" --json \
  > "$ROOT/reconciled-package.json"
python3 - "$ROOT/reconciled-package.json" <<'PY'
import json
import sys

model = json.load(open(sys.argv[1], encoding="utf-8"))["models"][0]
assert model["local_source"] is True
assert model["local_package"] is True
assert model["product_status"] == "supported"
PY
set +e
"$YVEX_BIN" profile create --path "$ARTIFACT" --registry "$REG" \
  --alias minimax-h3-fl2va-runtime-incomplete --family minimax-h3 \
  --startup-profile composite --target minimax-h3-fl2va --backend cuda \
  > "$ROOT/add-composite-bad.out" \
  2> "$ROOT/add-composite-bad.err"
composite_bad_status=$?
set -e
test "$composite_bad_status" -eq 2
grep 'composite startup profile requires --installation-root' \
  "$ROOT/add-composite-bad.err"

"$YVEX_BIN" profile show deepseek4-v4-flash-dspark-selected-embed --registry "$REG" --output nope > "$ROOT/inspect-bad-output.out" 2> "$ROOT/inspect-bad-output.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/inspect-bad-output.err" 'unsupported output mode: nope'

"$YVEX_BIN" profile create --path "$ARTIFACT" --registry "$REG" \
  --alias deepseek4-v4-flash-dspark-runtime-incomplete > "$ROOT/add-incomplete.out"
SERVER_RUNTIME="$ROOT/server-runtime"
yvex_test_cleanup "$SERVER_RUNTIME"
mkdir -m 700 "$SERVER_RUNTIME"
(
  server_pid=
  finish_model_host()
  {
    status=$?
    trap - EXIT HUP INT TERM
    if test -n "$server_pid" && kill -0 "$server_pid" 2>/dev/null; then
      XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" host stop >/dev/null 2>&1 || true
      wait "$server_pid" 2>/dev/null || true
    fi
    exit "$status"
  }
  trap finish_model_host EXIT HUP INT TERM

  YVEX_MODELS_REGISTRY="$REG" XDG_RUNTIME_DIR="$SERVER_RUNTIME" \
    "$YVEX_BIN" serve --logs off --openai off \
    > "$ROOT/lifecycle-host.out" 2> "$ROOT/lifecycle-host.err" &
  server_pid=$!
  ready=0
  attempt=0
  while test "$attempt" -lt 100; do
    if XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" host status --json \
        > "$ROOT/lifecycle-status.json" 2> "$ROOT/lifecycle-status.err"; then
      ready=1
      break
    fi
    kill -0 "$server_pid" 2>/dev/null || break
    attempt=$((attempt + 1))
    sleep 0.02
  done
  test "$ready" -eq 1

  XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" profile list \
    --models-root "$CATALOG_ROOT" --registry "$REG" --json \
    > "$ROOT/list-host-observed.json"
  python3 - "$ROOT/list-host-observed.json" <<'PY'
import json
import sys

catalog = json.load(open(sys.argv[1], encoding="utf-8"))
profiles = {profile["identity"]: profile for profile in catalog["profiles"]}
assert "deepseek4-v4-flash-dspark-selected-embed" in profiles
assert "minimax-h3-fl2va-runtime-media" in profiles
assert all("engine_state" not in profile for profile in profiles.values())
PY

  expect_rc 1 env XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" engine load \
    deepseek4-v4-flash-dspark-runtime-incomplete \
    > "$ROOT/use-incomplete.out" 2> "$ROOT/use-incomplete.err"
  grep 'model has no complete startup profile' "$ROOT/use-incomplete.err"
  "$YVEX_BIN" profile remove deepseek4-v4-flash-dspark-runtime-incomplete \
    --registry "$REG" > "$ROOT/remove-incomplete.out"

  "$YVEX_BIN" profile remove deepseek4-v4-flash-dspark-selected-embed \
    --registry "$REG" > "$ROOT/remove.out"
  python3 tests/support/human_field.py "$ROOT/remove.out" 'removed: deepseek4-v4-flash-dspark-selected-embed'
  python3 tests/support/human_field.py "$ROOT/remove.out" 'status: models-removed'
  "$YVEX_BIN" profile remove minimax-h3-fl2va-runtime-media --registry "$REG" \
    > "$ROOT/remove-composite.out"

  expect_rc 1 env XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" engine load missing \
    > "$ROOT/use-missing.out" 2> "$ROOT/use-missing.err"
  python3 tests/support/human_field.py "$ROOT/use-missing.err" 'profile is not registered: missing'

  XDG_RUNTIME_DIR="$SERVER_RUNTIME" "$YVEX_BIN" host stop \
    > "$ROOT/lifecycle-stop.out" 2> "$ROOT/lifecycle-stop.err"
  wait "$server_pid"
  server_pid=
)

"$YVEX_BIN" help --advanced > "$ROOT/help.out"
python3 tests/support/human_field.py "$ROOT/help.out" 'yvex source acquire'
python3 tests/support/human_field.py "$ROOT/help.out" 'yvex source status'
python3 tests/support/human_field.py "$ROOT/help.out" 'yvex compile'
python3 tests/support/human_field.py "$ROOT/help.out" 'yvex artifact status'
"$YVEX_BIN" source acquire --help > "$ROOT/help-acquire.out"
grep -- '--auth' "$ROOT/help-acquire.out"
grep -- '--progress' "$ROOT/help-acquire.out"
grep -- '--revision' "$ROOT/help-acquire.out"
grep -- '--include' "$ROOT/help-acquire.out"
"$YVEX_BIN" compile --help > "$ROOT/help-prepare.out"
grep -- '--audit' "$ROOT/help-prepare.out"

FAKE_GH="$PWD/tests/fixtures/bin/fake-gh"
DOWNLOAD_ROOT="$ROOT/download"
export YVEX_CONFIG_DIR="$ROOT/accounts-config"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --dry-run --models-root "$DOWNLOAD_ROOT" --auth never --audit > "$ROOT/download-dry-run.out"
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'status: model-download-dry-run'
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'stage: account-provider skipped'
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'payload_loaded: false'
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'gguf_created: false'
python3 tests/support/human_field.py "$ROOT/download-dry-run.out" 'generation: unsupported'
! grep 'tick: elapsed=' "$ROOT/download-dry-run.out"
test ! -e "$DOWNLOAD_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
test ! -e "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.download.receipt"
test ! -e "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.download.active.json"
test ! -e "$DOWNLOAD_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log"
test ! -e "$DOWNLOAD_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$DOWNLOAD_ROOT" --auth auto --audit > "$ROOT/download-gemma.out"
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'status: model-download-pass'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'provider: huggingface'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'stage: account-provider pass'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'stage: provider-cli pass'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'stage: source-manifest pass'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'stage: native-inventory pass'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'stage: progress-stream pass'
grep 'source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08' "$ROOT/download-gemma.out"
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'gguf_created: false'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'payload_loaded: false'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'generation: unsupported'
python3 tests/support/human_field.py "$ROOT/download-gemma.out" 'benchmark_status: not-measured'
test -f "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.source-manifest.json"
grep '"status": "in-progress"' "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.source-manifest.json"
test -f "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.native-inventory.json"
test -f "$DOWNLOAD_ROOT/evidence/build/gemma/gemma-4-12b-it.download-report.json"
test -f "$DOWNLOAD_ROOT/registry/gemma/gemma-4-12b-it.download.json"
! find "$DOWNLOAD_ROOT/gguf" -type f -name '*.gguf' 2>/dev/null | grep .

"$YVEX_BIN" source list --models-root "$DOWNLOAD_ROOT" --registry "$REG" --json \
  > "$ROOT/local-catalog.json"
python3 - "$ROOT/local-catalog.json" <<'PY'
import json
import sys

catalog = json.load(open(sys.argv[1], encoding="utf-8"))
source = next(item for item in catalog["sources"] if item["name"] == "gemma-4-12b-it")
assert source["representation"] == "safetensors-source"
assert source["acquisition_state"] == "source-acquired"
assert source["verification_state"] == "revision-verified"
assert source["blocker"] == ""
PY

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire \
  --repo community/minimax-h3-gguf --family minimax-h3 --name minimax-h3-community-gguf \
  --revision 2222222222222222222222222222222222222222 \
  --include model-Q4_K_M.gguf --models-root "$DOWNLOAD_ROOT" --auth auto \
  --no-native-inventory --audit > "$ROOT/download-gguf.out"
python3 tests/support/human_field.py "$ROOT/download-gguf.out" 'status: model-download-pass'
python3 tests/support/human_field.py "$ROOT/download-gguf.out" 'safetensors_count: 0'
python3 tests/support/human_field.py "$ROOT/download-gguf.out" 'gguf_count: 1'
grep 'boundary: acquired GGUF, structural inspection and package admission required' \
  "$ROOT/download-gguf.out"
"$YVEX_BIN" source list --models-root "$DOWNLOAD_ROOT" --registry "$REG" --json \
  > "$ROOT/local-catalog-gguf.json"
python3 - "$ROOT/local-catalog-gguf.json" <<'PY'
import json
import sys

catalog = json.load(open(sys.argv[1], encoding="utf-8"))
source = next(
    item for item in catalog["sources"] if item["name"] == "minimax-h3-community-gguf"
)
assert source["representation"] == "gguf"
assert source["acquisition_state"] == "source-acquired"
assert source["verification_state"] == "revision-verified"
assert source["blocker"] == ""
PY

LIVE_ROOT="$ROOT/download-live"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=1 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$LIVE_ROOT" --auth required --progress plain --tick-seconds 1 --audit > "$ROOT/download-live.out" 2>&1 &
LIVE_PID=$!
sleep 1
python3 tests/support/human_field.py "$ROOT/download-live.out" 'acquisition: state='
wait "$LIVE_PID"
python3 tests/support/human_field.py "$ROOT/download-live.out" 'acquisition: state=complete'
grep 'files=' "$ROOT/download-live.out"
grep 'committed=' "$ROOT/download-live.out"
grep 'provider_write_activity=' "$ROOT/download-live.out"
grep 'fake-hf: resolving repo' \
  "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.acquisition.supervisor.log"
grep 'fake-hf: downloading shard' \
  "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.acquisition.supervisor.log"
python3 tests/support/human_field.py "$ROOT/download-live.out" 'progress_mode: plain'
python3 tests/support/human_field.py "$ROOT/download-live.out" 'tick_seconds: 1'
python3 tests/support/human_field.py "$ROOT/download-live.out" 'stdout_streamed: true'
python3 tests/support/human_field.py "$ROOT/download-live.out" 'stderr_streamed: true'
python3 tests/support/human_field.py "$ROOT/download-live.out" 'provider_exit_code: 0'
test -f "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log"
test -f "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log"
python3 tests/support/human_field.py "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log" 'fake-hf: resolving repo'
python3 tests/support/human_field.py "$LIVE_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log" 'fake-hf: stderr resolving repo'

LIVE_FAIL_ROOT="$ROOT/download-live-fail"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_FAIL_AT_STEP=2 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$LIVE_FAIL_ROOT" --progress plain --tick-seconds 1 --audit > "$ROOT/download-live-fail.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-live-fail.out" 'status: model-download-fail'
python3 tests/support/human_field.py "$ROOT/download-live-fail.out" 'provider_exit_code: 43'
python3 tests/support/human_field.py "$ROOT/download-live-fail.out" 'stdout_log: '
python3 tests/support/human_field.py "$ROOT/download-live-fail.out" 'stderr_log: '
python3 tests/support/human_field.py "$ROOT/download-live-fail.out" 'top_blocker: provider-download-failed'
test -f "$LIVE_FAIL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log"
test -f "$LIVE_FAIL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log"
python3 tests/support/human_field.py "$LIVE_FAIL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log" 'fake-hf: downloading shard 1'
python3 tests/support/human_field.py "$LIVE_FAIL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log" 'fake-hf: failing at step 2'

SIGNAL_ROOT="$ROOT/download-signal"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=5 YVEX_FAKE_HF_STEPS=8 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$SIGNAL_ROOT" --progress plain --tick-seconds 1 --audit > "$ROOT/download-signal.out" 2>&1 &
SIGNAL_PID=$!
sleep 1
kill -TERM "$SIGNAL_PID"
set +e
wait "$SIGNAL_PID"
SIGNAL_RC=$?
set -e
test "$SIGNAL_RC" -ne 0
"$YVEX_BIN" source status gemma-4-12b-it --models-root "$SIGNAL_ROOT" \
  --output json > "$ROOT/download-signal-detached.json"
grep '"active":true' "$ROOT/download-signal-detached.json"
grep '"lifecycle":"downloading"' "$ROOT/download-signal-detached.json"
"$YVEX_BIN" source stop gemma-4-12b-it --models-root "$SIGNAL_ROOT" \
  --output json > "$ROOT/download-signal-stop.json"
grep '"lifecycle":"stopped"' "$ROOT/download-signal-stop.json"
grep '"reason":"operator-stop"' "$ROOT/download-signal-stop.json"
python3 tests/support/human_field.py \
  "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.acquisition.supervisor.log" 'status: model-download-interrupted'
python3 tests/support/human_field.py \
  "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.acquisition.supervisor.log" 'signal: SIGTERM'
test -f "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log"
test -f "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log"
test -f "$SIGNAL_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/config.json"
python3 tests/support/human_field.py "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stdout.log" 'fake-hf: resolving repo'
python3 tests/support/human_field.py "$SIGNAL_ROOT/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log" 'fake-hf: stderr resolving repo'

CONTROL_ROOT="$ROOT/download-control"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=5 YVEX_FAKE_HF_STEPS=8 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$CONTROL_ROOT" --auth required --progress log --tick-seconds 1 --audit > "$ROOT/download-control-run.out" 2>&1 &
CONTROL_PID=$!
i=0
while [ ! -f "$CONTROL_ROOT/evidence/build/gemma/gemma-4-12b-it.download.active.json" ] && [ "$i" -lt 10 ]; do
  sleep 1
  i=$((i + 1))
done
test -f "$CONTROL_ROOT/evidence/build/gemma/gemma-4-12b-it.download.active.json"

"$YVEX_BIN" source status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-running.out"
grep 'state       downloading' "$ROOT/download-control-status-running.out"
grep 'health      ' "$ROOT/download-control-status-running.out"

"$YVEX_BIN" source stop gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-stop.out"
grep 'state       stopped' "$ROOT/download-control-stop.out"
grep 'reason      operator-stop' "$ROOT/download-control-stop.out"
set +e
wait "$CONTROL_PID"
CONTROL_RC=$?
set -e
test "$CONTROL_RC" -ne 0
test -f "$CONTROL_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/config.json"

"$YVEX_BIN" source status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-stopped.out"
grep 'state       stopped' "$ROOT/download-control-status-stopped.out"
grep 'reason      operator-stop' "$ROOT/download-control-status-stopped.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source resume gemma-4-12b-it --models-root "$CONTROL_ROOT" --auth required --progress log --tick-seconds 1 --audit > "$ROOT/download-control-resume.out" 2>&1
python3 tests/support/human_field.py "$ROOT/download-control-resume.out" 'status: model-download-resume-pass'
python3 tests/support/human_field.py "$ROOT/download-control-resume.out" 'stage: download pass'
test -f "$CONTROL_ROOT/evidence/build/gemma/gemma-4-12b-it.download.last.json"
"$YVEX_BIN" source status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-resumed.out"
grep 'state       complete' "$ROOT/download-control-status-resumed.out"
grep 'reason      selected-source-complete' "$ROOT/download-control-status-resumed.out"

STALE_ROOT="$ROOT/download-control-stale"
mkdir -p "$STALE_ROOT/evidence/build/gemma" "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
cat > "$STALE_ROOT/evidence/build/gemma/gemma-4-12b-it.download.active.json" <<EOF
{
  "schema": "yvex.model_download.active.v1",
  "target_id": "gemma-4-12b-it",
  "family": "gemma",
  "provider": "huggingface",
  "repo_id": "google/gemma-4-12B-it",
  "revision": "b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08",
  "local_source_dir": "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08",
  "provider_pid": 99999999,
  "provider_pgid": 99999999,
  "status": "running"
}
EOF
"$YVEX_BIN" source status gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$STALE_ROOT" --audit > "$ROOT/download-control-stale-status.out"
python3 tests/support/human_field.py "$ROOT/download-control-stale-status.out" 'receipt_status: stale-active-receipt'

mkdir -p "$STALE_ROOT/cache/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/huggingface/download"
ln -s "$PWD/$STALE_ROOT/cache/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache"
printf 'lock\n' > "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache/huggingface/download/model.safetensors.lock"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source resume gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$STALE_ROOT" --auth required --audit > "$ROOT/download-control-lock-blocked.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-control-lock-blocked.out" 'status: model-download-resume-blocked'
python3 tests/support/human_field.py "$ROOT/download-control-lock-blocked.out" 'top_blocker: stale-lock-candidates'

"$YVEX_BIN" source cleanup gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$STALE_ROOT" --stale-locks --dry-run --audit > "$ROOT/download-control-cleanup-dry-run.out"
python3 tests/support/human_field.py "$ROOT/download-control-cleanup-dry-run.out" 'status: model-download-cleanup-dry-run'
python3 tests/support/human_field.py "$ROOT/download-control-cleanup-dry-run.out" 'stale_locks: 1'
test -f "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache/huggingface/download/model.safetensors.lock"

"$YVEX_BIN" source cleanup gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$STALE_ROOT" --stale-locks --yes --audit > "$ROOT/download-control-cleanup.out"
python3 tests/support/human_field.py "$ROOT/download-control-cleanup.out" 'status: model-download-cleanup'
python3 tests/support/human_field.py "$ROOT/download-control-cleanup.out" 'deleted: 1'
test ! -f "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache/huggingface/download/model.safetensors.lock"

printf 'lock\n' > "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache/huggingface/download/model.safetensors.lock"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source resume gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$STALE_ROOT" --auth required --clear-stale-locks --audit > "$ROOT/download-control-lock-clear-resume.out" 2>&1
python3 tests/support/human_field.py "$ROOT/download-control-lock-clear-resume.out" 'status: model-download-resume-pass'
test ! -f "$STALE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/.cache/huggingface/download/model.safetensors.lock"

SAFE_ROOT="$ROOT/download-control-safe"
SAFE_SRC="$SAFE_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
mkdir -p "$SAFE_SRC"
python3 - "$SAFE_SRC/model-ok.safetensors" "$SAFE_SRC/model-truncated.safetensors" <<'PY'
import json
import struct
import sys

def write(path, payload_len, actual_len):
    header = {
        "__metadata__": {"format": "pt"},
        "embed.weight": {
            "dtype": "F16",
            "shape": [1, max(1, payload_len // 2)],
            "data_offsets": [0, payload_len],
        },
    }
    blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(blob)))
        f.write(blob)
        f.write(bytes(actual_len))

write(sys.argv[1], 16, 16)
write(sys.argv[2], 32, 8)
PY
"$YVEX_BIN" source status gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$SAFE_ROOT" --audit > "$ROOT/download-control-safe-truncated.out"
python3 tests/support/human_field.py "$ROOT/download-control-safe-truncated.out" 'safetensors_header_checked: true'
python3 tests/support/human_field.py "$ROOT/download-control-safe-truncated.out" 'safetensors_size_status: truncated'
rm -f "$SAFE_SRC/model-truncated.safetensors"
"$YVEX_BIN" source status gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$SAFE_ROOT" --audit > "$ROOT/download-control-safe-ok.out"
python3 tests/support/human_field.py "$ROOT/download-control-safe-ok.out" 'safetensors_size_status: ok'

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-e2b --models-root "$ROOT/download-off" --no-progress --audit > "$ROOT/download-off.out" 2>&1
! grep 'model-download: start' "$ROOT/download-off.out"
! grep 'tick: elapsed=' "$ROOT/download-off.out"
! grep 'fake-hf: resolving repo' "$ROOT/download-off.out"
python3 tests/support/human_field.py "$ROOT/download-off.out" 'status: model-download-pass'

LOG_PROGRESS_ROOT="$ROOT/download-log-progress"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=1 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-e2b-it --models-root "$LOG_PROGRESS_ROOT" --progress log --tick-seconds 1 --audit > "$ROOT/download-log-progress.out" 2>&1
python3 tests/support/human_field.py "$ROOT/download-log-progress.out" 'tick: elapsed='
! grep 'fake-hf: resolving repo' "$ROOT/download-log-progress.out"
python3 tests/support/human_field.py "$ROOT/download-log-progress.out" 'progress_mode: log'
test -f "$LOG_PROGRESS_ROOT/evidence/build/acquisition/gemma-4-e2b-it.download.stdout.log"
python3 tests/support/human_field.py "$LOG_PROGRESS_ROOT/evidence/build/acquisition/gemma-4-e2b-it.download.stdout.log" 'fake-hf: resolving repo'

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-e2b --models-root "$DOWNLOAD_ROOT/noauth" --auth never --audit > "$ROOT/download-auth-never.out"
python3 tests/support/human_field.py "$ROOT/download-auth-never.out" 'stage: account-provider skipped'
python3 tests/support/human_field.py "$ROOT/download-auth-never.out" 'status: model-download-pass'

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire qwen3-8b --models-root "$DOWNLOAD_ROOT" --auth auto --audit > "$ROOT/download-qwen.out"
python3 tests/support/human_field.py "$ROOT/download-qwen.out" 'family: qwen'
grep 'source/hf/Qwen/Qwen3-8B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08' "$ROOT/download-qwen.out"
python3 tests/support/human_field.py "$ROOT/download-qwen.out" 'status: model-download-pass'

DYNAMIC_ROOT="$ROOT/download-dynamic-targets"
mkdir -p "$DYNAMIC_ROOT/evidence/fixtures/deepseek" "$DYNAMIC_ROOT/evidence/fixtures/qwen" "$DYNAMIC_ROOT/evidence/fixtures/gemma"
printf 'selected deepseek fixture\n' > "$DYNAMIC_ROOT/evidence/fixtures/deepseek/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf"
printf 'selected deepseek rmsnorm fixture\n' > "$DYNAMIC_ROOT/evidence/fixtures/deepseek/deepseek4-v4-flash-dspark-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
printf 'selected qwen fixture\n' > "$DYNAMIC_ROOT/evidence/fixtures/qwen/qwen3-8b-selected-embed-F16-noimatrix-yvex-v1.gguf"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire --repo Qwen/Qwen3.6-35B-A3B --family qwen --name qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --auth auto --progress off --audit > "$ROOT/download-dynamic-qwen.out"
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen.out" 'repo_id: Qwen/Qwen3.6-35B-A3B'
test -f "$DYNAMIC_ROOT/registry/qwen/qwen3-6-35b-a3b.download.json"
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.download-report.json"
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.source-manifest.json"
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.native-inventory.json"
"$YVEX_BIN" source status qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/download-dynamic-qwen-status.out"
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'repo_id: Qwen/Qwen3.6-35B-A3B'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'safetensors_count: 2'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'safetensors_size_status: ok'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-status.out" 'status: model-download-status'
"$YVEX_BIN" source cleanup qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --stale-locks --dry-run --audit > "$ROOT/download-dynamic-qwen-cleanup.out"
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-cleanup.out" 'model-download-cleanup: target=qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/download-dynamic-qwen-cleanup.out" 'status: model-download-cleanup-dry-run'
rm -f "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/"*.safetensors
write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" qwen-coverage BF16
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_status: naming-map-candidate'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_target_id: qwen3-6-35b-a3b'
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.embed_tokens.weight -> model.embedding.token.weight' "$ROOT/tensor-map-dynamic-qwen-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight' "$ROOT/tensor-map-dynamic-qwen-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.linear_attn.A_log -> model.layers.0.qwen_linear_attn.A_log' "$ROOT/tensor-map-dynamic-qwen-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.mlp.gate.weight -> model.layers.0.moe.router.weight' "$ROOT/tensor-map-dynamic-qwen-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.mlp.experts.gate_up_proj -> model.layers.0.moe.experts.all.gate_up_proj.weight' "$ROOT/tensor-map-dynamic-qwen-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.mlp.shared_expert.down_proj.weight -> model.layers.0.moe.shared_expert.down_proj.weight' "$ROOT/tensor-map-dynamic-qwen-audit.out"
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_qwen_linear_attn_count: [1-9]'
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_moe_router_count: [1-9]'
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_moe_expert_count: [1-9]'
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_moe_shared_count: [1-9]'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'tensor_map_required_role_coverage_status: required-groups-present'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-audit.out" 'generation: unsupported-full-model'
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tensor-map.json"
grep '"row": "MODELS.SOURCE.MAP.HANDOFF.0"' "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tensor-map.json"
grep '"required_role_coverage_status": "required-groups-present"' "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tensor-map.json"
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/tensor-map-dynamic-qwen-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-dynamic-qwen-table.out"
grep -F 'FAMILY  TARGET                STATUS                      TOTAL   EMBED    ATTN     MLP    NORM    HEAD     MOE   UNKNOWN   LAYERS  NEXT' "$ROOT/tensor-map-dynamic-qwen-table.out"
grep -F 'qwen    qwen3-6-35b-a3b       naming-map-candidate' "$ROOT/tensor-map-dynamic-qwen-table.out"
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role output-head --audit > "$ROOT/output-head-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'output_head_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'output_head_map_target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'output_head_native_name: lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-audit.out" 'generation: unsupported-full-model'
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.output-head-map.json"
grep '"row": "MODELS.SOURCE.MAP.HANDOFF.0"' "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.output-head-map.json"
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role output-head --output table > "$ROOT/output-head-dynamic-qwen-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-dynamic-qwen-table.out"
grep -F 'FAMILY  TARGET                STATUS                           HEAD  FINAL_NORM  EMBED  TIE_POLICY                          SHAPE_RELATION            NEXT' "$ROOT/output-head-dynamic-qwen-table.out"
grep -F 'qwen    qwen3-6-35b-a3b       output-head-profiled             yes' "$ROOT/output-head-dynamic-qwen-table.out"
write_fake_tokenizer_sidecars "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" qwen
"$YVEX_BIN" inspect target tokenizer-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" > "$ROOT/tokenizer-map-dynamic-qwen.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'tokenizer-map: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'tokenizer: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'vocab: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'merges: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'chat_template: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'specials: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'runtime: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen.out" 'next: V010.QUANT.1'
"$YVEX_BIN" inspect target tokenizer-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/tokenizer-map-dynamic-qwen-table.out"
grep 'TOKENIZER METADATA MAP' "$ROOT/tokenizer-map-dynamic-qwen-table.out"
grep -F 'TARGET                FAMILY  STATUS               TOKENIZER  VOCAB                         MERGES                  CHAT_TEMPLATE  SPECIALS  NEXT' "$ROOT/tokenizer-map-dynamic-qwen-table.out"
matches "$ROOT/tokenizer-map-dynamic-qwen-table.out" '^qwen3-6-35b-a3b[[:space:]]{2,}qwen[[:space:]]{2,}present-report-only[[:space:]]{2,}yes[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}V010\.QUANT\.1$'
"$YVEX_BIN" inspect target tokenizer-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tokenizer-map-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'tokenizer_map_target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'evidence_basis: sidecar-json-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'vocab_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'merges_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'tokenizer_backend_type: BPE'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'added_tokens_count: 1'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'special_tokens_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'stop_token_candidate.0.id: 1'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'chat_template_hash_status: not-computed'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'prompt_template_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'detokenization_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'gguf_tokenizer_contract_status: planned'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-audit.out" 'next_required_rows: V010.QUANT.1'
"$YVEX_BIN" inspect target tokenizer-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output json > "$ROOT/tokenizer-map-dynamic-qwen-json.out"
grep '"status":"present-report-only"' "$ROOT/tokenizer-map-dynamic-qwen-json.out"
grep '"target_id":"qwen3-6-35b-a3b"' "$ROOT/tokenizer-map-dynamic-qwen-json.out"
grep '"next":"V010.QUANT.1"' "$ROOT/tokenizer-map-dynamic-qwen-json.out"
test -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tokenizer-map.json"
grep '"schema_version": "yvex.source.tokenizer_map.v1"' "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tokenizer-map.json"
grep '"tokenizer_map_status": "present-report-only"' "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tokenizer-map.json"
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role tokenizer --audit > "$ROOT/tokenizer-map-dynamic-qwen-compat-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-qwen-compat-audit.out" 'tokenizer_map_status: present-report-only'
"$YVEX_BIN" compile source manifest report --family qwen --release v0.1.0 --source "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/source-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'model: Qwen3.6-35B-A3B'
! grep 'target_id: qwen3-8b' "$ROOT/source-dynamic-qwen-audit.out"
python3 tests/support/human_field.py --regex "$ROOT/source-dynamic-qwen-audit.out" 'source_manifest_path: .*qwen3-6-35b-a3b.source-manifest.json'
python3 tests/support/human_field.py --regex "$ROOT/source-dynamic-qwen-audit.out" 'native_inventory_path: .*qwen3-6-35b-a3b.native-inventory.json'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'source_manifest_status: present'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'native_inventory_report_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'tensor_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'tensor_role_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'output_head_map_status: available-report-only'
python3 tests/support/human_field.py --regex "$ROOT/source-dynamic-qwen-audit.out" 'tokenizer_map_path: .*qwen3-6-35b-a3b.tokenizer-map.json'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'tokenizer_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-qwen-audit.out" 'next_required_rows: V010.QUANT.1'
! grep 'missing-qwen-tensor-role-map' "$ROOT/source-dynamic-qwen-audit.out"
! grep 'missing-qwen-tensor-map' "$ROOT/source-dynamic-qwen-audit.out"
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" > "$ROOT/missing-roles-dynamic-qwen-coverage.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage.out" 'missing-roles: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage.out" 'top_blocker: quant-policy-or-artifact-emitter'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage.out" 'next: V010.QUANT.1'
grep 'qwen-linear-attn.*present' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
grep 'moe-router.*present' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
grep 'moe-experts.*present' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
grep 'shared-expert.*present' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
grep 'unknown-tensors.*unclassified-header-name' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
grep 'tokenizer.*present-report-only' "$ROOT/missing-roles-dynamic-qwen-coverage.out"
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'role_group.qwen_linear_attn.status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'role_group.moe_router.status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'role_group.moe_experts.status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'role_group.shared_expert.status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'role_group.unknown_tensors.status: unclassified-header-name'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'top_blocker: quant-policy-or-artifact-emitter'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-coverage-audit.out" 'next: V010.QUANT.1'
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output json > "$ROOT/missing-roles-dynamic-qwen-coverage-json.out"
grep '"top_blocker":"quant-policy-or-artifact-emitter"' "$ROOT/missing-roles-dynamic-qwen-coverage-json.out"
grep '"qwen_linear_attn":"present"' "$ROOT/missing-roles-dynamic-qwen-coverage-json.out"
grep '"shared_expert":"present"' "$ROOT/missing-roles-dynamic-qwen-coverage-json.out"
grep '"tokenizer":"present-report-only"' "$ROOT/missing-roles-dynamic-qwen-coverage-json.out"
"$YVEX_BIN" compile qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --dry-run --audit > "$ROOT/prepare-dynamic-qwen-coverage.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'reason: family quantization plan unimplemented'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-coverage.out" 'next: not-scheduled'
"$YVEX_BIN" inspect target quant-policy qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role-support > "$ROOT/qtype-role-support-dynamic-qwen.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'qtype-role-support: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'status: blocked'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'source_dtype: BF16'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'preferred_artifact_qtype: unresolved'
python3 tests/support/human_field.py --regex "$ROOT/qtype-role-support-dynamic-qwen.out" 'supported_roles: [1-9][0-9]*'
python3 tests/support/human_field.py --regex "$ROOT/qtype-role-support-dynamic-qwen.out" 'blocked_roles: [1-9][0-9]*'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'next: not-scheduled'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen.out" 'boundary: qtype role report only; no quantization/GGUF/runtime/generation'
! grep 'runtime_claim:' "$ROOT/qtype-role-support-dynamic-qwen.out"
"$YVEX_BIN" inspect target quant-policy qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role-support --output table > "$ROOT/qtype-role-support-dynamic-qwen-table.out"
grep 'QTYPE ROLE SUPPORT' "$ROOT/qtype-role-support-dynamic-qwen-table.out"
grep 'ROLE[[:space:]][[:space:]]*SRC_DTYPE[[:space:]][[:space:]]*ARTIFACT_QTYPE[[:space:]][[:space:]]*STORAGE[[:space:]][[:space:]]*COMPUTE[[:space:]][[:space:]]*CALIBRATION[[:space:]][[:space:]]*STATUS' "$ROOT/qtype-role-support-dynamic-qwen-table.out"
grep 'qwen_linear_attn_A_log[[:space:]][[:space:]]*BF16[[:space:]][[:space:]]*unresolved[[:space:]][[:space:]]*header-storage-profiled[[:space:]][[:space:]]*cpu-cuda-available[[:space:]][[:space:]]*deferred[[:space:]][[:space:]]*present' "$ROOT/qtype-role-support-dynamic-qwen-table.out"
grep 'moe_expert_gate_up[[:space:]][[:space:]]*BF16' "$ROOT/qtype-role-support-dynamic-qwen-table.out"
"$YVEX_BIN" inspect target quant-policy qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role-support --audit > "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'report: qtype-role-support'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'source_dtype: BF16'
grep 'role\.[0-9][0-9]*\.role_name: qwen_linear_attn_A_log' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
grep 'role\.[0-9][0-9]*\.role_name: tokenizer_metadata' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
grep 'role\.[0-9][0-9]*\.source_dtype: BF16' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
grep 'role\.[0-9][0-9]*\.compute_support_status: cpu-cuda-available' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
grep 'role\.[0-9][0-9]*\.artifact_emission_allowed: false' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
grep 'role\.[0-9][0-9]*\.artifact_emission_blocker: family-quantization-plan-unimplemented' "$ROOT/qtype-role-support-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'payload_bytes_read: false'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'quantization_performed: false'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'gguf_emitted: false'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-qwen-audit.out" 'benchmark_status: not-measured'
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role-support --output json > "$ROOT/qtype-role-support-json.out" 2> "$ROOT/qtype-role-support-json.err"
grep 'JSON output is unsupported' "$ROOT/qtype-role-support-json.err"
rm -f "$DYNAMIC_ROOT/evidence/build/qwen/qwen3-6-35b-a3b.tokenizer-map.json"
write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" qwen-incomplete
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-qwen-incomplete-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-incomplete-audit.out" 'tensor_map_status: naming-map-incomplete'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-qwen-incomplete-audit.out" 'tensor_map_target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-qwen-incomplete-audit.out" 'unmapped_unknown_count: [1-9]'
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --role output-head --audit > "$ROOT/output-head-dynamic-qwen-incomplete-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-incomplete-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-qwen-incomplete-audit.out" 'output_head_map_target_id: qwen3-6-35b-a3b'
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" > "$ROOT/missing-roles-dynamic-qwen.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'missing-roles: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'status: blocked'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'top_blocker: incomplete-tensor-map'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'next: V010.MAP.8'
grep 'qwen-linear-attn.*missing' "$ROOT/missing-roles-dynamic-qwen.out"
grep 'output-head.*present' "$ROOT/missing-roles-dynamic-qwen.out"
grep 'tokenizer.*missing' "$ROOT/missing-roles-dynamic-qwen.out"
grep 'artifact.*missing' "$ROOT/missing-roles-dynamic-qwen.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen.out" 'boundary: missing-role report only; no GGUF/runtime/generation'
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/missing-roles-dynamic-qwen-table.out"
grep 'qwen3-6-35b-a3b.*qwen.*blocked.*incomplete-tensor-map' "$ROOT/missing-roles-dynamic-qwen-table.out"
grep 'qwen3-6-35b-a3b.*missing.*missing.*V010.MAP.8' "$ROOT/missing-roles-dynamic-qwen-table.out"
grep 'qwen3-6-35b-a3b.*[[:space:]][1-9][0-9]*[[:space:]]*missing[[:space:]]*missing' "$ROOT/missing-roles-dynamic-qwen-table.out"
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/missing-roles-dynamic-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'source_status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'tensor_map_status: incomplete-report-only'
python3 tests/support/human_field.py --regex "$ROOT/missing-roles-dynamic-qwen-audit.out" 'tensor_map_path: .*qwen3-6-35b-a3b.tensor-map.json'
python3 tests/support/human_field.py --regex "$ROOT/missing-roles-dynamic-qwen-audit.out" 'tensor_map_unmapped_unknown_count: [1-9]'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'tokenizer_map_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/missing-roles-dynamic-qwen-audit.out" 'expected_artifact_path: .*qwen3-6-35b-a3b.gguf'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'artifact_emission_status: not-performed'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'artifact_identity_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'prepare_blocker_count: '
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'top_blocker: incomplete-tensor-map'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'runtime_execution: not-performed'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'generation: unsupported'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-qwen-audit.out" 'benchmark_status: not-measured'
"$YVEX_BIN" inspect target missing-roles qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --output json > "$ROOT/missing-roles-dynamic-qwen-json.out"
grep '"target_id":"qwen3-6-35b-a3b"' "$ROOT/missing-roles-dynamic-qwen-json.out"
grep '"top_blocker":"incomplete-tensor-map"' "$ROOT/missing-roles-dynamic-qwen-json.out"
"$YVEX_BIN" compile qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --dry-run --audit > "$ROOT/prepare-dynamic-qwen.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'target_id: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'source_status: present'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'model_class_status: present'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'tensor_map_status: incomplete-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'tokenizer_map_status: missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/prepare-dynamic-qwen.out" 'expected_artifact_path: .*qwen3-6-35b-a3b.gguf'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'artifact_plan_status: planned-full-gguf'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'artifact_emission_status: not-performed'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'artifact_identity_status: missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'prepare_blocker_count: '
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'top_blocker: incomplete-tensor-map'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'reason: incomplete tensor map / tokenizer metadata mapping / artifact path missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen.out" 'status: model-prepare-unsupported'
! grep 'status: model-prepare-unknown-target' "$ROOT/prepare-dynamic-qwen.out"
! grep 'reason: missing compile map / model class / artifact path' "$ROOT/prepare-dynamic-qwen.out"
"$YVEX_BIN" compile qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --dry-run > "$ROOT/prepare-dynamic-qwen-normal.out" 2>&1 && exit 1 || true
grep 'models prepare: qwen3-6-35b-a3b \[blocked\]' "$ROOT/prepare-dynamic-qwen-normal.out"
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-normal.out" 'family: qwen  source: present  artifact: missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-normal.out" 'plan: full-gguf planned  emission: not-performed'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-normal.out" 'top_blocker: incomplete-tensor-map'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-normal.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-qwen-normal.out" 'boundary: prepare dry-run only; no artifact emission/runtime/generation'
! grep 'source_manifest_path:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'native_inventory_path:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'tensor_map_path:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'output_head_map_path:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'expected_artifact_path:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'artifact_identity_status:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'prepare_blocker_count:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'runtime_execution:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'generation:' "$ROOT/prepare-dynamic-qwen-normal.out"
! grep 'reason:' "$ROOT/prepare-dynamic-qwen-normal.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire --repo google/Gemma-4-31B-it --family gemma --name gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --auth auto --progress off --audit > "$ROOT/download-dynamic-gemma.out"
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma.out" 'target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma.out" 'repo_id: google/Gemma-4-31B-it'
test -f "$DYNAMIC_ROOT/registry/gemma/gemma-4-31b-it.download.json"
test -f "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.source-manifest.json"
"$YVEX_BIN" source status gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/download-dynamic-gemma-status.out"
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma-status.out" 'target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma-status.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma-status.out" 'repo_id: google/Gemma-4-31B-it'
python3 tests/support/human_field.py "$ROOT/download-dynamic-gemma-status.out" 'safetensors_size_status: ok'
rm -f "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/"*.safetensors
write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" gemma-language-head BF16
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-audit.out" 'tensor_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-audit.out" 'tensor_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-audit.out" 'tensor_map_target_id: gemma-4-31b-it'
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.embed_tokens.weight -> model.embedding.token.weight' "$ROOT/tensor-map-dynamic-gemma-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight' "$ROOT/tensor-map-dynamic-gemma-audit.out"
grep 'tensor_map.entry.[0-9][0-9]*.mapping: model.language_model.layers.0.layer_scalar -> model.layers.0.layer_scalar' "$ROOT/tensor-map-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-audit.out" 'generation: unsupported-full-model'
test -f "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.tensor-map.json"
grep '"row": "MODELS.SOURCE.MAP.HANDOFF.0"' "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.tensor-map.json"
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/tensor-map-dynamic-gemma-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-dynamic-gemma-table.out"
grep -F 'FAMILY  TARGET                STATUS                      TOTAL   EMBED    ATTN     MLP    NORM    HEAD     MOE   UNKNOWN   LAYERS  NEXT' "$ROOT/tensor-map-dynamic-gemma-table.out"
grep -F 'gemma   gemma-4-31b-it        naming-map-profiled' "$ROOT/tensor-map-dynamic-gemma-table.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role output-head --audit > "$ROOT/output-head-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'output_head_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'output_head_map_target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'output_head_native_name: model.language_model.lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'tie_policy_status: separate-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-audit.out" 'generation: unsupported-full-model'
test -f "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.output-head-map.json"
grep '"row": "MODELS.SOURCE.MAP.HANDOFF.0"' "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.output-head-map.json"
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role output-head --output table > "$ROOT/output-head-dynamic-gemma-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-dynamic-gemma-table.out"
grep -F 'FAMILY  TARGET                STATUS                           HEAD  FINAL_NORM  EMBED  TIE_POLICY                          SHAPE_RELATION            NEXT' "$ROOT/output-head-dynamic-gemma-table.out"
grep -F 'gemma   gemma-4-31b-it        output-head-profiled             yes' "$ROOT/output-head-dynamic-gemma-table.out"
write_fake_tokenizer_sidecars "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" gemma
"$YVEX_BIN" inspect target tokenizer-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" > "$ROOT/tokenizer-map-dynamic-gemma.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'tokenizer-map: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'tokenizer: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'vocab: embedded-or-tokenizer-json'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'merges: not-required-or-absent'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'chat_template: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'specials: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'runtime: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma.out" 'next: V010.QUANT.1'
"$YVEX_BIN" inspect target tokenizer-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/tokenizer-map-dynamic-gemma-table.out"
grep 'TOKENIZER METADATA MAP' "$ROOT/tokenizer-map-dynamic-gemma-table.out"
matches "$ROOT/tokenizer-map-dynamic-gemma-table.out" '^gemma-4-31b-it[[:space:]]{2,}gemma[[:space:]]{2,}present-report-only[[:space:]]{2,}yes[[:space:]]{2,}embedded-or-tokenizer-json[[:space:]]{2,}not-required-or-absent[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}V010\.QUANT\.1$'
"$YVEX_BIN" inspect target tokenizer-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tokenizer-map-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'tokenizer_map_target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'vocab_status: embedded-or-tokenizer-json'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'merges_status: not-required-or-absent'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'special_tokens_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-dynamic-gemma-audit.out" 'next_required_rows: V010.QUANT.1'
"$YVEX_BIN" inspect target tokenizer-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --output json > "$ROOT/tokenizer-map-dynamic-gemma-json.out"
grep '"target_id":"gemma-4-31b-it"' "$ROOT/tokenizer-map-dynamic-gemma-json.out"
grep '"vocab_status":"embedded-or-tokenizer-json"' "$ROOT/tokenizer-map-dynamic-gemma-json.out"
grep '"next":"V010.QUANT.1"' "$ROOT/tokenizer-map-dynamic-gemma-json.out"
test -f "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.tokenizer-map.json"
grep '"tokenizer_map_status": "present-report-only"' "$DYNAMIC_ROOT/evidence/build/gemma/gemma-4-31b-it.tokenizer-map.json"
"$YVEX_BIN" compile source manifest report --family gemma --release v0.1.0 --source "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/source-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'model: Gemma-4-31B-it'
! grep 'target_id: gemma-4-12b-it' "$ROOT/source-dynamic-gemma-audit.out"
python3 tests/support/human_field.py --regex "$ROOT/source-dynamic-gemma-audit.out" 'source_manifest_path: .*gemma-4-31b-it.source-manifest.json'
python3 tests/support/human_field.py --regex "$ROOT/source-dynamic-gemma-audit.out" 'native_inventory_path: .*gemma-4-31b-it.native-inventory.json'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'tensor_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'tensor_role_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'output_head_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'tokenizer_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-audit.out" 'next_required_rows: V010.QUANT.1'
! grep 'missing-gemma-tensor-role-map' "$ROOT/source-dynamic-gemma-audit.out"
! grep 'missing-gemma-tensor-map' "$ROOT/source-dynamic-gemma-audit.out"
"$YVEX_BIN" compile gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --dry-run --audit > "$ROOT/prepare-dynamic-gemma.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'source_status: present'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'model_class_status: present'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/prepare-dynamic-gemma.out" 'expected_artifact_path: .*gemma-4-31b-it.gguf'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'artifact_plan_status: planned-full-gguf'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'artifact_emission_status: not-performed'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'artifact_identity_status: missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'prepare_blocker_count: '
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'reason: family quantization plan unimplemented'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'next: not-scheduled'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma.out" 'status: model-prepare-unsupported'
! grep 'status: model-prepare-unknown-target' "$ROOT/prepare-dynamic-gemma.out"
! grep 'reason: missing compile map / model class / artifact path' "$ROOT/prepare-dynamic-gemma.out"
"$YVEX_BIN" inspect target quant-policy gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role-support > "$ROOT/qtype-role-support-dynamic-gemma.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'qtype-role-support: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'status: blocked'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'source_dtype: BF16'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma.out" 'next: not-scheduled'
"$YVEX_BIN" inspect target quant-policy gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role-support --output table > "$ROOT/qtype-role-support-dynamic-gemma-table.out"
grep 'attention_q_norm[[:space:]][[:space:]]*BF16' "$ROOT/qtype-role-support-dynamic-gemma-table.out"
grep 'output_head_tied_embedding[[:space:]][[:space:]]*BF16' "$ROOT/qtype-role-support-dynamic-gemma-table.out"
"$YVEX_BIN" inspect target quant-policy gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role-support --audit > "$ROOT/qtype-role-support-dynamic-gemma-audit.out"
grep 'role\.[0-9][0-9]*\.role_name: pre_feedforward_layernorm' "$ROOT/qtype-role-support-dynamic-gemma-audit.out"
grep 'role\.[0-9][0-9]*\.role_name: layer_scalar' "$ROOT/qtype-role-support-dynamic-gemma-audit.out"
grep 'role\.[0-9][0-9]*\.role_name: tokenizer_metadata' "$ROOT/qtype-role-support-dynamic-gemma-audit.out"
grep 'role\.[0-9][0-9]*\.compute_support_status: cpu-cuda-available' "$ROOT/qtype-role-support-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-dynamic-gemma-audit.out" 'generation: unsupported-full-model'
write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" qwen-coverage BF16
write_fake_tokenizer_sidecars "$DYNAMIC_ROOT/source/hf/Qwen/Qwen3.6-35B-A3B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" qwen
"$YVEX_BIN" inspect target tensor-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-qwen-restored-audit.out"
"$YVEX_BIN" inspect target tokenizer-map qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" > "$ROOT/tokenizer-map-dynamic-qwen-restored.out"
"$YVEX_BIN" inspect target quant-policy --gate v0.1.0 --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/qtype-role-support-gate-table.out"
grep 'FAMILY[[:space:]][[:space:]]*TARGET[[:space:]][[:space:]]*STATUS[[:space:]][[:space:]]*ROLES[[:space:]][[:space:]]*BLOCKED[[:space:]][[:space:]]*TOP_BLOCKER[[:space:]][[:space:]]*NEXT' "$ROOT/qtype-role-support-gate-table.out"
grep 'deepseek[[:space:]][[:space:]]*deepseek4-v4-flash-dspark[[:space:]][[:space:]]*blocked[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*artifact-materialization-unimplemented[[:space:]][[:space:]]*V010.ARTIFACT.MATERIALIZE.0' "$ROOT/qtype-role-support-gate-table.out"
grep 'qwen[[:space:]][[:space:]]*qwen3-6-35b-a3b[[:space:]][[:space:]]*blocked[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*family-quantization-plan-unimplemented[[:space:]][[:space:]]*not-scheduled' "$ROOT/qtype-role-support-gate-table.out"
grep 'gemma[[:space:]][[:space:]]*gemma-4-31b-it[[:space:]][[:space:]]*blocked[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*[1-9][0-9]*[[:space:]][[:space:]]*family-quantization-plan-unimplemented[[:space:]][[:space:]]*not-scheduled' "$ROOT/qtype-role-support-gate-table.out"
"$YVEX_BIN" inspect target quant-policy --gate v0.1.0 --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/qtype-role-support-gate-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-gate-audit.out" 'report: qtype-role-support-gate'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-gate-audit.out" 'family.0.top_blocker: artifact-materialization-unimplemented'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-gate-audit.out" 'family.1.top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-gate-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-gate-audit.out" 'generation: unsupported-full-model'

write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" gemma-language-tied
printf '{"tie_word_embeddings":true,"vocab_size":2,"bos_token_id":1,"eos_token_id":1,"pad_token_id":0,"unk_token_id":0}\n' > "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/config.json"
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-gemma-tied-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-tied-audit.out" 'tensor_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-tied-audit.out" 'tensor_map_required_role_coverage_status: required-groups-present'
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role output-head --audit > "$ROOT/output-head-dynamic-gemma-tied-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'output_head_map_status: tied-output-head-report-only'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'output_head_native_name: model.language_model.embed_tokens.weight'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'output_head_canonical_role: model.output_head.tied_embedding'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'output_head_mapping_status: tied-to-token-embedding-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'tie_policy_status: tied-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-tied-audit.out" 'config_tie_word_embeddings_status: true'
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role output-head --output table > "$ROOT/output-head-dynamic-gemma-tied-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-dynamic-gemma-tied-table.out"
grep -F 'FAMILY  TARGET                STATUS                           HEAD  FINAL_NORM  EMBED  TIE_POLICY                          SHAPE_RELATION            NEXT' "$ROOT/output-head-dynamic-gemma-tied-table.out"
grep -F 'gemma   gemma-4-31b-it        tied-output-head-report-only     yes' "$ROOT/output-head-dynamic-gemma-tied-table.out"
"$YVEX_BIN" inspect target missing-roles gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/missing-roles-dynamic-gemma-tied-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'role_group.output_head.status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'role_group.tied_head_policy.status: tied-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'top_blocker: quant-policy-or-artifact-emitter'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-tied-audit.out" 'next: V010.QUANT.1'
"$YVEX_BIN" compile gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --dry-run --audit > "$ROOT/prepare-dynamic-gemma-tied.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-tied.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-tied.out" 'output_head_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-tied.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-tied.out" 'top_blocker: family-quantization-plan-unimplemented'

write_fake_transformer_safetensors "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/model.safetensors" gemma-language-no-head
printf '{"tie_word_embeddings":false}\n' > "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/config.json"
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/tensor-map-dynamic-gemma-incomplete-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-incomplete-audit.out" 'tensor_map_status: naming-map-candidate'
python3 tests/support/human_field.py "$ROOT/tensor-map-dynamic-gemma-incomplete-audit.out" 'tensor_map_target_id: gemma-4-31b-it'
python3 tests/support/human_field.py --regex "$ROOT/tensor-map-dynamic-gemma-incomplete-audit.out" 'unmapped_unknown_count: [1-9]'
"$YVEX_BIN" inspect target tensor-map gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --role output-head --audit > "$ROOT/output-head-dynamic-gemma-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-missing-audit.out" 'output_head_map_status: output-head-missing'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-missing-audit.out" 'output_head_map_target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-missing-audit.out" 'output_head_missing_status: missing'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-missing-audit.out" 'tie_policy_status: not-proven'
python3 tests/support/human_field.py "$ROOT/output-head-dynamic-gemma-missing-audit.out" 'config_tie_word_embeddings_status: false'
"$YVEX_BIN" inspect target missing-roles gemma-4-31b-it --models-root "$DYNAMIC_ROOT" > "$ROOT/missing-roles-dynamic-gemma.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma.out" 'missing-roles: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma.out" 'status: blocked'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma.out" 'top_blocker: missing-output-head-map'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma.out" 'next: V010.MAP.8'
grep 'output-head.*missing' "$ROOT/missing-roles-dynamic-gemma.out"
grep 'tied-head-policy.*not-proven' "$ROOT/missing-roles-dynamic-gemma.out"
grep 'tokenizer.*present-report-only' "$ROOT/missing-roles-dynamic-gemma.out"
grep 'unknown-tensors.*unclassified-header-name' "$ROOT/missing-roles-dynamic-gemma.out"
grep 'artifact.*missing' "$ROOT/missing-roles-dynamic-gemma.out"
"$YVEX_BIN" inspect target missing-roles gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --output table > "$ROOT/missing-roles-dynamic-gemma-table.out"
grep 'gemma-4-31b-it.*gemma.*blocked.*missing-output-head-map' "$ROOT/missing-roles-dynamic-gemma-table.out"
grep 'gemma-4-31b-it.*present-report-only.*missing.*V010.MAP.8' "$ROOT/missing-roles-dynamic-gemma-table.out"
grep 'gemma-4-31b-it.*[[:space:]][1-9][0-9]*[[:space:]]*present-report-only[[:space:]]*missing' "$ROOT/missing-roles-dynamic-gemma-table.out"
"$YVEX_BIN" inspect target missing-roles gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/missing-roles-dynamic-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'target_id: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'source_status: present'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'output_head_map_status: missing-in-report'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/missing-roles-dynamic-gemma-audit.out" 'expected_artifact_path: .*gemma-4-31b-it.gguf'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'top_blocker: missing-output-head-map'
python3 tests/support/human_field.py "$ROOT/missing-roles-dynamic-gemma-audit.out" 'next: V010.MAP.8'
"$YVEX_BIN" compile source manifest report --family gemma --release v0.1.0 --source "$DYNAMIC_ROOT/source/hf/google/Gemma-4-31B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/source-dynamic-gemma-incomplete-map.out"
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-incomplete-map.out" 'tensor_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-incomplete-map.out" 'tensor_role_map_status: available-report-only'
python3 tests/support/human_field.py "$ROOT/source-dynamic-gemma-incomplete-map.out" 'output_head_map_status: missing-in-report'
! grep 'missing-gemma-tensor-role-map' "$ROOT/source-dynamic-gemma-incomplete-map.out"
! grep 'missing-gemma-tensor-map' "$ROOT/source-dynamic-gemma-incomplete-map.out"
"$YVEX_BIN" compile gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --dry-run --audit > "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 2>&1 && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'output_head_map_status: missing-in-report'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'top_blocker: missing-output-head-map'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'reason: output head mapping missing / artifact path missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-incomplete-map.out" 'status: model-prepare-unsupported'
"$YVEX_BIN" compile gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --dry-run > "$ROOT/prepare-dynamic-gemma-normal.out" 2>&1 && exit 1 || true
grep 'models prepare: gemma-4-31b-it \[blocked\]' "$ROOT/prepare-dynamic-gemma-normal.out"
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-normal.out" 'family: gemma  source: present  artifact: missing'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-normal.out" 'plan: full-gguf planned  emission: not-performed'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-normal.out" 'top_blocker: missing-output-head-map'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-normal.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/prepare-dynamic-gemma-normal.out" 'boundary: prepare dry-run only; no artifact emission/runtime/generation'
! grep 'source_manifest_path:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'native_inventory_path:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'tensor_map_path:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'output_head_map_path:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'expected_artifact_path:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'artifact_identity_status:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'prepare_blocker_count:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'runtime_execution:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'generation:' "$ROOT/prepare-dynamic-gemma-normal.out"
! grep 'reason:' "$ROOT/prepare-dynamic-gemma-normal.out"

"$YVEX_BIN" inspect artifact registry --models-root "$DYNAMIC_ROOT" > "$ROOT/artifacts-list.out"
grep 'deepseek4-v4-flash-dspark-selected-embed.*deepseek.*yvex-selected-gguf.*present.*ready' "$ROOT/artifacts-list.out"
grep 'deepseek4-v4-flash-dspark-selected-embed-rmsnorm.*deepseek.*yvex-selected-gguf.*present.*ready' "$ROOT/artifacts-list.out"
grep 'qwen3-8b-selected-embed.*qwen.*yvex-selected-gguf.*present.*ready' "$ROOT/artifacts-list.out"
grep 'qwen3-6-35b-a3b.*qwen.*planned-full-gguf.*missing.*blocked' "$ROOT/artifacts-list.out"
grep 'gemma-4-31b-it.*gemma.*planned-full-gguf.*missing.*blocked' "$ROOT/artifacts-list.out"
python3 tests/support/human_field.py "$ROOT/artifacts-list.out" 'status: artifacts-list'
! grep 'runtime_ready' "$ROOT/artifacts-list.out"
"$YVEX_BIN" inspect artifact registry --models-root "$DYNAMIC_ROOT" --family qwen --output table > "$ROOT/artifacts-list-qwen-table.out"
grep 'qwen3-8b-selected-embed.*qwen.*present' "$ROOT/artifacts-list-qwen-table.out"
grep 'qwen3-6-35b-a3b.*qwen.*missing.*blocked' "$ROOT/artifacts-list-qwen-table.out"
! grep 'gemma-4-31b-it' "$ROOT/artifacts-list-qwen-table.out"
"$YVEX_BIN" inspect artifact registry --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/artifacts-list-audit.out"
grep 'artifact\.[0-9][0-9]*\.expected_artifact_path: .*qwen3-6-35b-a3b.gguf' "$ROOT/artifacts-list-audit.out"
grep 'artifact\.[0-9][0-9]*\.expected_artifact_path: .*gemma-4-31b-it.gguf' "$ROOT/artifacts-list-audit.out"
grep 'artifact\.[0-9][0-9]*\.tensor_map_path: .*gemma-4-31b-it.tensor-map.json' "$ROOT/artifacts-list-audit.out"
python3 tests/support/human_field.py "$ROOT/artifacts-list-audit.out" 'source_payload_loaded: false'
python3 tests/support/human_field.py "$ROOT/artifacts-list-audit.out" 'hash_performed: false'
"$YVEX_BIN" inspect artifact registry --models-root "$DYNAMIC_ROOT" --output json > "$ROOT/artifacts-list-json.out"
grep '"status":"artifacts-list"' "$ROOT/artifacts-list-json.out"
grep '"target_id":"qwen3-6-35b-a3b"' "$ROOT/artifacts-list-json.out"
"$YVEX_BIN" inspect artifact status qwen3-6-35b-a3b --models-root "$DYNAMIC_ROOT" > "$ROOT/artifacts-status-qwen.out"
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'artifact: qwen3-6-35b-a3b'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'class: planned-full-gguf'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'source: present'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/artifacts-status-qwen.out" 'expected: .*qwen3-6-35b-a3b.gguf'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'prepare: blocked'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'next: not-scheduled'
python3 tests/support/human_field.py "$ROOT/artifacts-status-qwen.out" 'boundary: artifact discovery only; no runtime/generation'
"$YVEX_BIN" inspect artifact status gemma-4-31b-it --models-root "$DYNAMIC_ROOT" --audit > "$ROOT/artifacts-status-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'artifact: gemma-4-31b-it'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'class: planned-full-gguf'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'artifact_status: missing'
python3 tests/support/human_field.py --regex "$ROOT/artifacts-status-gemma-audit.out" 'expected: .*gemma-4-31b-it.gguf'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'prepare: blocked'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'top_blocker: missing-output-head-map'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'tensor_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'output_head_map_status: missing-in-report'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'source_payload_loaded: false'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'hash_performed: false'
python3 tests/support/human_field.py "$ROOT/artifacts-status-gemma-audit.out" 'status: artifacts-status'

QWEN32_STATUS_ROOT="$ROOT/download-qwen32-status"
"$YVEX_BIN" source status qwen3-32b --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$QWEN32_STATUS_ROOT" --audit > "$ROOT/download-qwen32-status.out"
python3 tests/support/human_field.py "$ROOT/download-qwen32-status.out" 'target_id: qwen3-32b'
python3 tests/support/human_field.py "$ROOT/download-qwen32-status.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/download-qwen32-status.out" 'repo_id: Qwen/Qwen3-32B'
grep 'source/hf/Qwen/Qwen3-32B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08' "$ROOT/download-qwen32-status.out"
python3 tests/support/human_field.py "$ROOT/download-qwen32-status.out" 'status: model-download-status'

QWEN32_CLEANUP_ROOT="$ROOT/download-qwen32-cleanup"
QWEN32_CLEANUP_SRC="$QWEN32_CLEANUP_ROOT/source/hf/Qwen/Qwen3-32B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
mkdir -p "$QWEN32_CLEANUP_SRC/.cache/huggingface/download"
printf 'partial\n' > "$QWEN32_CLEANUP_SRC/.cache/huggingface/download/model.safetensors.incomplete"
for path in \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.download.receipt" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.download.active.json" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.download.last.json" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.download-report.json" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.source-manifest.json" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.native-inventory.json" \
  "$QWEN32_CLEANUP_ROOT/registry/qwen/qwen3-32b.download.json" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/acquisition/qwen3-32b.download.stdout.log" \
  "$QWEN32_CLEANUP_ROOT/evidence/build/acquisition/qwen3-32b.download.stderr.log"
do
  mkdir -p "$(dirname "$path")"
  printf 'sidecar\n' > "$path"
done
"$YVEX_BIN" source cleanup qwen3-32b --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$QWEN32_CLEANUP_ROOT" --failed-partials --dry-run --audit > "$ROOT/download-qwen32-cleanup-dry-run.out"
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup-dry-run.out" 'cleanup_failed_partials: true'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup-dry-run.out" 'cleanup_sidecars: true'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup-dry-run.out" 'cleanup_logs: true'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup-dry-run.out" 'status: model-download-cleanup-dry-run'
test -d "$QWEN32_CLEANUP_SRC"
test -f "$QWEN32_CLEANUP_ROOT/evidence/build/acquisition/qwen3-32b.download.stderr.log"
"$YVEX_BIN" source cleanup qwen3-32b --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$QWEN32_CLEANUP_ROOT" --failed-partials --yes --audit > "$ROOT/download-qwen32-cleanup.out"
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup.out" 'cleanup_failed_partials: true'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup.out" 'deleted_source_entries: '
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup.out" 'deleted_sidecars: 7'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup.out" 'deleted_logs: 2'
python3 tests/support/human_field.py "$ROOT/download-qwen32-cleanup.out" 'status: model-download-cleanup'
test ! -e "$QWEN32_CLEANUP_SRC"
test ! -e "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.download-report.json"
test ! -e "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.source-manifest.json"
test ! -e "$QWEN32_CLEANUP_ROOT/evidence/build/qwen/qwen3-32b.native-inventory.json"
test ! -e "$QWEN32_CLEANUP_ROOT/registry/qwen/qwen3-32b.download.json"
test ! -e "$QWEN32_CLEANUP_ROOT/evidence/build/acquisition/qwen3-32b.download.stdout.log"
test ! -e "$QWEN32_CLEANUP_ROOT/evidence/build/acquisition/qwen3-32b.download.stderr.log"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$DOWNLOAD_ROOT/required" --auth required --audit > "$ROOT/download-auth-required.out" 2> "$ROOT/download-auth-required.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-auth-required.out" 'stage: account-provider blocked'
python3 tests/support/human_field.py "$ROOT/download-auth-required.out" 'top_blocker: provider-login-required'

YVEX_HF_CLI=/missing/hf "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$DOWNLOAD_ROOT/missing" --auth auto --audit > "$ROOT/download-missing-hf.out" 2> "$ROOT/download-missing-hf.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-missing-hf.out" 'status: model-download-blocked'
python3 tests/support/human_field.py "$ROOT/download-missing-hf.out" 'top_blocker: missing-huggingface-cli'
python3 tests/support/human_field.py "$ROOT/download-missing-hf.out" 'stage: account-provider blocked'

YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_FAIL=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --revision b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08 --models-root "$DOWNLOAD_ROOT/fail" --auth auto --audit > "$ROOT/download-fail.out" 2> "$ROOT/download-fail.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-fail.out" 'status: model-download-fail'
test -f "$DOWNLOAD_ROOT/fail/evidence/build/acquisition/gemma-4-12b-it.download.stderr.log"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire --repo test-org/test-model --family gemma --name test-model --models-root "$DOWNLOAD_ROOT/direct" --auth auto --audit > "$ROOT/download-direct.out"
python3 tests/support/human_field.py "$ROOT/download-direct.out" 'repo_id: test-org/test-model'
grep 'source/hf/test-org/test-model/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08' "$ROOT/download-direct.out"
python3 tests/support/human_field.py "$ROOT/download-direct.out" 'status: model-download-pass'

YVEX_FAKE_GH_AUTH=1 YVEX_GH_CLI="$FAKE_GH" "$YVEX_BIN" source acquire --provider github --repo test-org/test-model --release v1 --asset '*.gguf' --models-root "$DOWNLOAD_ROOT/github" --auth auto --audit > "$ROOT/download-github.out"
python3 tests/support/human_field.py "$ROOT/download-github.out" 'provider: github'
python3 tests/support/human_field.py "$ROOT/download-github.out" 'stage: account-provider pass'
python3 tests/support/human_field.py "$ROOT/download-github.out" 'stage: download pass'
python3 tests/support/human_field.py "$ROOT/download-github.out" 'github/test-org/test-model/v1'
python3 tests/support/human_field.py "$ROOT/download-github.out" 'gguf_created: false'
python3 tests/support/human_field.py "$ROOT/download-github.out" 'generation: unsupported'
test -f "$DOWNLOAD_ROOT/github/source/github/test-org/test-model/v1/fake-model.gguf"

"$YVEX_BIN" source acquire --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-missing-target.out" 2> "$ROOT/download-missing-target.err" && exit 1 || true
grep 'requires TARGET or --repo' "$ROOT/download-missing-target.err"
"$YVEX_BIN" source acquire --repo test-org/test-model --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-repo-no-family.out" 2> "$ROOT/download-repo-no-family.err" && exit 1 || true
grep 'requires a safe lower-case --family key' "$ROOT/download-repo-no-family.err"
"$YVEX_BIN" source acquire --repo test-org/test-model --family 'llama/unsafe' --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-bad-family.out" 2> "$ROOT/download-bad-family.err" && exit 1 || true
grep 'requires a safe lower-case --family key' "$ROOT/download-bad-family.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --models-root "" > "$ROOT/download-empty-root.out" 2> "$ROOT/download-empty-root.err" && exit 1 || true
grep 'models download --models-root value is empty or invalid' "$ROOT/download-empty-root.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --max-workers 0 > "$ROOT/download-bad-workers.out" 2> "$ROOT/download-bad-workers.err" && exit 1 || true
grep 'requires a positive integer' "$ROOT/download-bad-workers.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --auth maybe > "$ROOT/download-bad-auth.out" 2> "$ROOT/download-bad-auth.err" && exit 1 || true
grep 'auth requires auto|required|never' "$ROOT/download-bad-auth.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --progress nope > "$ROOT/download-bad-progress.out" 2> "$ROOT/download-bad-progress.err" && exit 1 || true
grep 'requires auto|live|plain|log|off' "$ROOT/download-bad-progress.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --tick-seconds 0 > "$ROOT/download-bad-tick.out" 2> "$ROOT/download-bad-tick.err" && exit 1 || true
grep 'tick-seconds requires a positive integer' "$ROOT/download-bad-tick.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --source s3 > "$ROOT/download-bad-source.out" 2> "$ROOT/download-bad-source.err" && exit 1 || true
grep 'supports hf only' "$ROOT/download-bad-source.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --auth nope > "$ROOT/download-bad-auth.out" 2> "$ROOT/download-bad-auth.err" && exit 1 || true
grep 'requires auto|required|never' "$ROOT/download-bad-auth.err"
"$YVEX_BIN" source acquire --provider github --repo test-org/test-model > "$ROOT/download-github-no-asset.out" 2> "$ROOT/download-github-no-asset.err" && exit 1 || true
grep 'requires --asset GLOB' "$ROOT/download-github-no-asset.err"
"$YVEX_BIN" source acquire gemma-4-12b-it --surprise > "$ROOT/download-unknown-flag.out" 2> "$ROOT/download-unknown-flag.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/download-unknown-flag.err" 'unknown flag: --surprise'
"$YVEX_BIN" source acquire gemma-4-12b-it extra > "$ROOT/download-extra-positional.out" 2> "$ROOT/download-extra-positional.err" && exit 1 || true
grep 'expected 0 to 1 positional arguments, received 2' "$ROOT/download-extra-positional.err"

HF_TOKEN=secret-value YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-e2b --models-root "$DOWNLOAD_ROOT/token" --auth auto --audit > "$ROOT/download-token.out"
python3 tests/support/human_field.py "$ROOT/download-token.out" 'auth_state: env-token-present'
python3 tests/support/human_field.py "$ROOT/download-token.out" 'token_value_redacted: true'
! grep -R 'secret-value' "$ROOT/download-token.out" "$DOWNLOAD_ROOT/token" "$ROOT"
! git ls-files '*.safetensors' '*.bin' '*.dat' | grep .

PREP="$ROOT/prepare"
PREP_SOURCE="$PREP/source/hf/deepseek-ai/DeepSeek-V4-Flash-DSpark/62af8fffb2f7030cac4de2f0169f5b8d1101b646"
PREP_REG="$PREP/registry/models.local.json"
PREP_GGUF="$PREP/evidence/fixtures/deepseek/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf"
mkdir -p "$PREP_SOURCE"

python3 - "$PREP_SOURCE/model-00001.safetensors" <<'PY'
import json
import struct
import sys

path = sys.argv[1]
header = {
    "__metadata__": {"format": "pt"},
    "embed.weight": {"dtype": "F16", "shape": [8, 4], "data_offsets": [0, 64]},
}
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(path, "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(bytes(range(64)))
PY

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --dry-run --models-root "$PREP" --registry "$PREP_REG" > "$ROOT/prepare-dry-run.out"
python3 tests/support/human_field.py "$ROOT/prepare-dry-run.out" 'status: model-prepare-dry-run'
python3 tests/support/human_field.py "$ROOT/prepare-dry-run.out" 'stage: convert-emit planned'
python3 tests/support/human_field.py "$ROOT/prepare-dry-run.out" 'generation: unsupported'

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --models-root "$ROOT/missing-prepare" --registry "$ROOT/missing-prepare/registry/models.local.json" > "$ROOT/prepare-missing.out" 2> "$ROOT/prepare-missing.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-missing.out" 'stage: source-path fail'
python3 tests/support/human_field.py "$ROOT/prepare-missing.out" 'status: model-prepare-fail'

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed-rmsnorm --dry-run > "$ROOT/prepare-segment-unsupported.out" 2> "$ROOT/prepare-segment-unsupported.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-segment-unsupported.out" 'status: model-prepare-unsupported'
grep 'segment prepare is planned' "$ROOT/prepare-segment-unsupported.out"

"$YVEX_BIN" compile glm-5.2-official-safetensors --dry-run > "$ROOT/prepare-glm-unsupported.out" 2> "$ROOT/prepare-glm-unsupported.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-glm-unsupported.out" 'status: model-prepare-unsupported'
grep 'YVEX-produced GGUF emission for this target is planned' "$ROOT/prepare-glm-unsupported.out"

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --models-root "$PREP" --registry "$PREP_REG" --overwrite --no-register > "$ROOT/prepare-no-register.out"
python3 tests/support/human_field.py "$ROOT/prepare-no-register.out" 'stage: source-manifest pass'
python3 tests/support/human_field.py "$ROOT/prepare-no-register.out" 'stage: convert-emit pass'
python3 tests/support/human_field.py "$ROOT/prepare-no-register.out" 'stage: registry-add skipped'
python3 tests/support/human_field.py "$ROOT/prepare-no-register.out" 'status: model-prepare'
test -f "$PREP_GGUF"
test ! -f "$PREP_REG"

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed \
  --models-root "$PREP" --registry "$PREP_REG" --overwrite --no-use \
  > "$ROOT/prepare-no-use.out" 2> "$ROOT/prepare-no-use.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-no-use.err" 'unknown flag: --no-use'

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --models-root "$PREP" --registry "$PREP_REG" --overwrite > "$ROOT/prepare-register-use.out"
python3 tests/support/human_field.py "$ROOT/prepare-register-use.out" 'stage: registry-remove-existing not-found'
python3 tests/support/human_field.py "$ROOT/prepare-register-use.out" 'stage: registry-add pass'
python3 tests/support/human_field.py "$ROOT/prepare-register-use.out" 'stage: registry-verify pass'
python3 tests/support/human_field.py "$ROOT/prepare-register-use.out" 'status: model-prepare'

"$YVEX_BIN" profile verify deepseek4-v4-flash-dspark-selected-embed --registry "$PREP_REG" > "$ROOT/prepare-verify.out"
python3 tests/support/human_field.py "$ROOT/prepare-verify.out" 'status: models-identity-pass'
python3 tests/support/human_field.py "$ROOT/prepare-verify.out" 'verify: pass alias=deepseek4-v4-flash-dspark-selected-embed'

"$YVEX_BIN" profile verify deepseek4-v4-flash-dspark-selected-embed --registry "$PREP_REG" --audit > "$ROOT/prepare-verify-audit.out"
python3 tests/support/human_field.py "$ROOT/prepare-verify-audit.out" 'current_sha256: '
python3 tests/support/human_field.py "$ROOT/prepare-verify-audit.out" 'digest_status: pass'
python3 tests/support/human_field.py "$ROOT/prepare-verify-audit.out" 'status: models-identity-pass'

"$YVEX_BIN" profile verify deepseek4-v4-flash-dspark-selected-embed --registry "$PREP_REG" --output nope > "$ROOT/verify-bad-output.out" 2> "$ROOT/verify-bad-output.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/verify-bad-output.err" 'unsupported output mode: nope'

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --models-root "$PREP" --registry "$PREP_REG" > "$ROOT/prepare-overwrite-refused.out" 2> "$ROOT/prepare-overwrite-refused.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/prepare-overwrite-refused.out" 'stage: convert-emit refused'
python3 tests/support/human_field.py "$ROOT/prepare-overwrite-refused.out" 'status: model-prepare-refused'

"$YVEX_BIN" compile deepseek4-v4-flash-dspark-selected-embed --out "$PREP_GGUF" --out-dir "$PREP/evidence/fixtures/deepseek" > "$ROOT/prepare-invalid.out" 2> "$ROOT/prepare-invalid.err" && exit 1 || true
grep 'conflicts with --out-dir' "$ROOT/prepare-invalid.err"

CHECK="build/tests/model-check"
CHECK_REG="$CHECK/registry/models.local.json"
CHECK_GGUF="$CHECK/models/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf"
CHECK_ROOT="$CHECK/root"
CHECK_ROOT_GGUF="$CHECK_ROOT/evidence/fixtures/deepseek/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf"
yvex_test_cleanup "$CHECK"
mkdir -p "$CHECK/models" "$CHECK/registry" "$CHECK_ROOT/evidence/fixtures/deepseek"

"$YVEX_BIN" compile artifact emit --out "$CHECK_GGUF" --model-name model-check-test --arch llama --overwrite >/dev/null
"$YVEX_BIN" compile artifact emit --out "$CHECK_ROOT_GGUF" --model-name model-check-target-root-test --arch llama --overwrite >/dev/null
"$YVEX_BIN" profile create --path "$CHECK_GGUF" --alias deepseek4-v4-flash-dspark-selected-embed --support-level selected-tensor-materialized --registry "$CHECK_REG" > "$ROOT/check-add.out"
python3 tests/support/human_field.py "$ROOT/check-add.out" 'status: models-added'

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --level quick --registry "$CHECK_REG" > "$ROOT/check-quick-normal.out"
python3 tests/support/human_field.py "$ROOT/check-quick-normal.out" 'model-check: pass target=deepseek4-v4-flash-dspark-selected-embed level=quick'
python3 tests/support/human_field.py "$ROOT/check-quick-normal.out" 'boundary: selected-slice check only, generation unsupported'
test "$(wc -l < "$ROOT/check-quick-normal.out")" -le 8

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --level quick --registry "$CHECK_REG" --audit > "$ROOT/check-quick.out"
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'status: model-check'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'target_id: deepseek4-v4-flash-dspark-selected-embed'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'backend: cpu'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'level: quick'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: inspect pass'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: tensors pass'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: metadata pass'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: registry-identity pass'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: integrity-check pass'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: materialize skipped'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'stage: graph-partial skipped'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'execution_ready: false'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'generation: unsupported'
python3 tests/support/human_field.py "$ROOT/check-quick.out" 'status: model-check-pass'

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --level quick --models-root "$CHECK_ROOT" --audit > "$ROOT/check-models-root.out"
python3 tests/support/human_field.py "$ROOT/check-models-root.out" 'model_input_kind: target'
grep 'build/tests/model-check/root/evidence/fixtures/deepseek/deepseek4-v4-flash-dspark-selected-embed-F16-noimatrix-yvex-v1.gguf' "$ROOT/check-models-root.out"
python3 tests/support/human_field.py "$ROOT/check-models-root.out" 'stage: registry-identity unregistered'
python3 tests/support/human_field.py "$ROOT/check-models-root.out" 'stage: integrity-check pass'
python3 tests/support/human_field.py "$ROOT/check-models-root.out" 'status: model-check-pass'

"$YVEX_BIN" artifact status glm-5.2-official-safetensors --dry-run > "$ROOT/check-invalid-dry-run.out" 2> "$ROOT/check-invalid-dry-run.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/check-invalid-dry-run.err" 'unknown flag: --dry-run'

"$YVEX_BIN" artifact status glm-5.2-official-safetensors --level quick --audit > "$ROOT/check-glm-unsupported.out" 2> "$ROOT/check-glm-unsupported.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/check-glm-unsupported.out" 'status: model-check-unsupported'
grep 'source-only target cannot be checked as a YVEX-produced runtime artifact yet' "$ROOT/check-glm-unsupported.out"
python3 tests/support/human_field.py "$ROOT/check-glm-unsupported.out" 'generation: unsupported'

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed-rmsnorm --level quick --audit > "$ROOT/check-segment-unsupported.out" 2> "$ROOT/check-segment-unsupported.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/check-segment-unsupported.out" 'status: model-check-unsupported'
grep 'segment check is planned' "$ROOT/check-segment-unsupported.out"
python3 tests/support/human_field.py "$ROOT/check-segment-unsupported.out" 'generation: unsupported'

"$YVEX_BIN" artifact status > "$ROOT/check-invalid-missing-target.out" 2> "$ROOT/check-invalid-missing-target.err" && exit 1 || true
grep 'expected 1 positional argument, received 0' "$ROOT/check-invalid-missing-target.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --backend > "$ROOT/check-invalid-backend-missing.out" 2> "$ROOT/check-invalid-backend-missing.err" && exit 1 || true
grep 'requires a value' "$ROOT/check-invalid-backend-missing.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --backend missing > "$ROOT/check-invalid-backend.out" 2> "$ROOT/check-invalid-backend.err" && exit 1 || true
grep 'invalid value for --backend: missing' "$ROOT/check-invalid-backend.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --level > "$ROOT/check-invalid-level-missing.out" 2> "$ROOT/check-invalid-level-missing.err" && exit 1 || true
grep 'requires a value' "$ROOT/check-invalid-level-missing.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --level impossible > "$ROOT/check-invalid-level.out" 2> "$ROOT/check-invalid-level.err" && exit 1 || true
grep 'unknown models check level' "$ROOT/check-invalid-level.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --registry "" > "$ROOT/check-invalid-registry.out" 2> "$ROOT/check-invalid-registry.err" && exit 1 || true
grep 'empty or invalid' "$ROOT/check-invalid-registry.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --report-dir "" > "$ROOT/check-invalid-report-dir.out" 2> "$ROOT/check-invalid-report-dir.err" && exit 1 || true
grep 'empty or invalid' "$ROOT/check-invalid-report-dir.err"

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --unknown-flag > "$ROOT/check-invalid-unknown.out" 2> "$ROOT/check-invalid-unknown.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/check-invalid-unknown.err" 'unknown flag: --unknown-flag'

"$YVEX_BIN" artifact status deepseek4-v4-flash-dspark-selected-embed --output nope > "$ROOT/check-invalid-output.out" 2> "$ROOT/check-invalid-output.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/check-invalid-output.err" 'unsupported output mode: nope'

"$YVEX_BIN" inspect target --help > "$ROOT/model-target-help.out"
python3 tests/support/human_field.py "$ROOT/model-target-help.out" 'operation: evidence.target'
python3 tests/support/human_field.py "$ROOT/model-target-help.out" 'plane: Inspect'
python3 tests/support/human_field.py "$ROOT/model-target-help.out" 'lane: offline-engine'
grep -- '--candidate' "$ROOT/model-target-help.out"
grep -- '--output' "$ROOT/model-target-help.out"
grep -- '--strict' "$ROOT/model-target-help.out"

CLASS_MISSING_ROOT="$ROOT/qwen-class-missing-root"
expect_rc 5 "$YVEX_BIN" inspect target class-profile deepseek4-v4-flash-dspark \
  --models-root "$CLASS_MISSING_ROOT" > "$ROOT/model-class-deepseek-blocked.out"
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked.out" 'model-class: deepseek'
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked.out" 'status: architecture-ir-blocked'
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked.out" 'reason: missing-source-path'
grep 'runtime/generation unsupported' "$ROOT/model-class-deepseek-blocked.out"

expect_rc 5 "$YVEX_BIN" inspect target class-profile deepseek4-v4-flash-dspark \
  --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/model-class-deepseek-blocked-table.out"
grep 'TARGET  SOURCE  IR  REASON' "$ROOT/model-class-deepseek-blocked-table.out"
grep 'deepseek4-v4-flash-dspark  blocked  not-built  missing-source-path' "$ROOT/model-class-deepseek-blocked-table.out"

expect_rc 5 "$YVEX_BIN" inspect target class-profile deepseek4-v4-flash-dspark \
  --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/model-class-deepseek-blocked-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked-audit.out" 'architecture_ir_status: blocked'
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked-audit.out" 'source_verification_status: blocked'
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked-audit.out" 'runtime_execution: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-deepseek-blocked-audit.out" 'generation: unsupported'

expect_rc 5 "$YVEX_BIN" inspect target class-profile deepseek4-v4-flash-dspark \
  --models-root "$CLASS_MISSING_ROOT" --output json > "$ROOT/model-class-deepseek-blocked.json"
jq -e '.status == "architecture-ir-blocked" and .target_id == "deepseek4-v4-flash-dspark" and .reason == "missing-source-path" and .runtime == "unsupported" and .generation == "unsupported"' \
  "$ROOT/model-class-deepseek-blocked.json" >/dev/null

"$YVEX_BIN" inspect target class-profile qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/model-class-qwen-missing.out"
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'model-class: qwen'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'target: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'class: qwen-source-model-class-profile'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'patterns: tensors=0 attn=0 mlp=0 norm=0 head=0 moe=0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing.out" 'next: V010.MAP.8'
grep 'no tensor role mapping/runtime/generation' "$ROOT/model-class-qwen-missing.out"

"$YVEX_BIN" inspect target class-profile qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/model-class-qwen-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'model_class_profile_status: source-missing'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'model_class_source_metadata_status: missing'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'model_class_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'model_class_pattern_status: lexical-only'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'model_class_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'backend_selection: deferred'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'backend_pressure: metal-planned'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-collection-qwen-missing.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'tensor-collection: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'target: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'collections: embedding=0 attention_qkvo=0 mlp_gud=0 norm=0 head=0 moe=0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing.out" 'boundary: tensor collection inventory only; no role mapping/runtime/generation'

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-collection-qwen-missing-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-qwen-missing-table.out"
matches "$ROOT/tensor-collection-qwen-missing-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-collection-qwen-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_source_status: missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_embedding_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_attention_complete_qkvo_layer_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_mlp_complete_gud_layer_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'tensor_collection_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-map-qwen-missing.out"
grep 'tensor-map: qwen3-8b \[blocked\]' "$ROOT/tensor-map-qwen-missing.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing.out" 'family: qwen  stage: header-naming-map  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing.out" 'roles: total=0 embedding=0 attention=0 mlp=0 norm=0 head=0 moe=0 unknown=0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing.out" 'boundary: report-only; use --audit for tensor entries'
! grep 'tensor_map.entry.' "$ROOT/tensor-map-qwen-missing.out"
! grep 'runtime_claim:' "$ROOT/tensor-map-qwen-missing.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-map-qwen-missing-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-qwen-missing-table.out"
matches "$ROOT/tensor-map-qwen-missing-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-map-qwen-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_stage: header-naming-map'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_source_status: missing'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_mapped_total_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_unmapped_unknown_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_runtime_role_coverage_status: report-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_artifact_contract_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'tensor_map_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-map-gemma-missing.out"
grep 'tensor-map: gemma-4-12b-it \[blocked\]' "$ROOT/tensor-map-gemma-missing.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing.out" 'family: gemma  stage: header-naming-map  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing.out" 'roles: total=0 embedding=0 attention=0 mlp=0 norm=0 head=0 moe=0 unknown=0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing.out" 'boundary: report-only; use --audit for tensor entries'
! grep 'tensor_map.entry.' "$ROOT/tensor-map-gemma-missing.out"
! grep 'runtime_claim:' "$ROOT/tensor-map-gemma-missing.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-map-gemma-missing-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-gemma-missing-table.out"
matches "$ROOT/tensor-map-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-map-gemma-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_stage: header-naming-map'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_source_status: missing'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_mapped_total_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_unmapped_unknown_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_runtime_role_coverage_status: report-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_artifact_contract_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'tensor_map_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --models-root "$CLASS_MISSING_ROOT" > "$ROOT/output-head-qwen-missing.out"
grep 'output-head-map: qwen3-8b \[blocked\]' "$ROOT/output-head-qwen-missing.out"
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'family: qwen  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'head: missing  final_norm: missing  embedding: missing  tie: unknown'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'shape: unknown'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing.out" 'boundary: mapping only; no logits/runtime/generation'
! grep 'output_head_map_' "$ROOT/output-head-qwen-missing.out"
! grep 'runtime_claim:' "$ROOT/output-head-qwen-missing.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --models-root "$CLASS_MISSING_ROOT" > "$ROOT/output-head-gemma-missing.out"
grep 'output-head-map: gemma-4-12b-it \[blocked\]' "$ROOT/output-head-gemma-missing.out"
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing.out" 'family: gemma  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing.out" 'head: missing  final_norm: missing  embedding: missing  tie: unknown'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing.out" 'next: V010.MAP.8'
! grep 'output_head_map_' "$ROOT/output-head-gemma-missing.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/output-head-gemma-missing-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-gemma-missing-table.out"
matches "$ROOT/output-head-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}no[[:space:]]{2,}no[[:space:]]{2,}no[[:space:]]{2,}unknown[[:space:]]{2,}unknown[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/output-head-gemma-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_status: source-missing'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_stage: header-output-head-map'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_map_source_status: missing'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_candidate_count: 0'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'output_head_missing_status: missing'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tokenizer-map-qwen-missing.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'tokenizer-map: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'tokenizer: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'vocab: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'merges: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'chat_template: unknown'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'specials: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'runtime: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'next: V010.MAP.7'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing.out" 'boundary: tokenizer metadata mapping only; no tokenization/detokenization/runtime/generation'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tokenizer-map-qwen-missing-table.out"
grep 'TOKENIZER METADATA MAP' "$ROOT/tokenizer-map-qwen-missing-table.out"
matches "$ROOT/tokenizer-map-qwen-missing-table.out" '^qwen3-8b[[:space:]]{2,}qwen[[:space:]]{2,}source-missing[[:space:]]{2,}no[[:space:]]{2,}missing[[:space:]]{2,}missing[[:space:]]{2,}unknown[[:space:]]{2,}missing[[:space:]]{2,}V010\.MAP\.7$'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tokenizer-map-qwen-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_status: source-missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_stage: metadata-tokenizer-map'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_evidence_basis: sidecar-json-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_map_source_status: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'tokenization_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'detokenization_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-missing-audit.out" 'next_required_rows: V010.MAP.7'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role tokenizer --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tokenizer-map-gemma-missing.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-missing.out" 'tokenizer-map: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-missing.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-missing.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-missing.out" 'next: V010.MAP.7'

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" > "$ROOT/model-class-gemma-missing.out"
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'model-class: gemma'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'target: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'class: gemma-source-model-class-profile'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'patterns: tensors=0 attn=0 mlp=0 norm=0 head=0 moe=0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing.out" 'next: V010.MAP.8'
grep 'no tensor role mapping/runtime/generation' "$ROOT/model-class-gemma-missing.out"

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/model-class-gemma-missing-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-gemma-missing-table.out"
matches "$ROOT/model-class-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/model-class-gemma-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_profile_status: source-missing'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_family: gemma'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_name: gemma-source-model-class-profile'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_runtime_shape: dense-causal-decoder-candidate-pending-config'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_source_metadata_status: missing'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_pattern_status: lexical-only'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'model_class_runtime_status: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'backend_selection: deferred'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'backend_pressure: cpu-cuda-baseline-planned'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-missing-audit.out" 'next_required_rows: V010.MAP.8'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-collection-gemma-missing.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'tensor-collection: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'target: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'collections: embedding=0 attention_qkvo=0 mlp_gud=0 norm=0 head=0 moe=0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing.out" 'boundary: tensor collection inventory only; no role mapping/runtime/generation'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-collection-gemma-missing-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-gemma-missing-table.out"
matches "$ROOT/tensor-collection-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-collection-gemma-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_status: source-missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_source_status: missing'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_embedding_tensor_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_attention_complete_qkvo_layer_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_mlp_complete_gud_layer_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'tensor_collection_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-missing-audit.out" 'next_required_rows: V010.MAP.8'

QWEN_CLASS_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-class-profile-test-$$"
yvex_test_cleanup "$QWEN_CLASS_SOURCE"
mkdir -p "$QWEN_CLASS_SOURCE"
printf '{}\n' > "$QWEN_CLASS_SOURCE/config.json"
printf '{}\n' > "$QWEN_CLASS_SOURCE/tokenizer.json"
python3 - "$QWEN_CLASS_SOURCE/model-00001-of-00001.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" > "$ROOT/model-class-qwen.out"
python3 tests/support/human_field.py "$ROOT/model-class-qwen.out" 'status: metadata-profiled'
python3 tests/support/human_field.py "$ROOT/model-class-qwen.out" 'patterns: tensors=10 attn=4 mlp=3 norm=2 head=1 moe=0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen.out" 'top_blocker: missing-qwen-tensor-role-map'
python3 tests/support/human_field.py "$ROOT/model-class-qwen.out" 'next: V010.MAP.8'

"$YVEX_BIN" inspect target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" --output table > "$ROOT/model-class-qwen-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-qwen-table.out"
matches "$ROOT/model-class-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}metadata-profiled[[:space:]]{2,}10[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}2[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" --audit > "$ROOT/model-class-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_profile_status: metadata-profiled'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_config_status: present'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_source_metadata_status: header-only'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_tensor_count: 10'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_embedding_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_attention_q_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_attention_k_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_attention_v_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_attention_o_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_mlp_gate_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_mlp_up_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_mlp_down_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_norm_pattern_count: 2'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_output_head_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_moe_router_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_moe_expert_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_other_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_pattern_status: lexical-only'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'model_class_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'backend_selection: deferred'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'backend_pressure: metal-planned'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-audit.out" 'next_required_rows: V010.MAP.8'

QWEN_CLASS_MODELS_ROOT="$ROOT/qwen-class-models-root"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire qwen3-8b --models-root "$QWEN_CLASS_MODELS_ROOT" --auth auto --progress off > "$ROOT/qwen-class-acquire.out"
cp -R "$QWEN_CLASS_SOURCE/." "$QWEN_CLASS_MODELS_ROOT/source/hf/Qwen/Qwen3-8B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/"
"$YVEX_BIN" inspect target class-profile qwen3-8b --models-root "$QWEN_CLASS_MODELS_ROOT" --audit > "$ROOT/model-class-qwen-models-root-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-qwen-models-root-audit.out" 'model_class_profile_status: metadata-profiled'
matches "$ROOT/model-class-qwen-models-root-audit.out" 'source_path: .*/qwen-class-models-root/source/hf/Qwen/Qwen3-8B/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08$'
python3 tests/support/human_field.py "$ROOT/model-class-qwen-models-root-audit.out" 'model_class_source_metadata_status: header-only'

QWEN_COLLECTION_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-tensor-collection-test-$$"
yvex_test_cleanup "$QWEN_COLLECTION_SOURCE"
mkdir -p "$QWEN_COLLECTION_SOURCE"
printf '{}\n' > "$QWEN_COLLECTION_SOURCE/config.json"
printf '{}\n' > "$QWEN_COLLECTION_SOURCE/tokenizer.json"
python3 - "$QWEN_COLLECTION_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" > "$ROOT/tensor-collection-qwen.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'tensor-collection: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'target: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'status: collection-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'collections: embedding=1 attention_qkvo=1 mlp_gud=1 norm=3 head=1 moe=0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'layers_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'top_blocker: missing-qwen-tensor-role-map'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen.out" 'boundary: tensor collection inventory only; no role mapping/runtime/generation'

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --output table > "$ROOT/tensor-collection-qwen-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-qwen-table.out"
matches "$ROOT/tensor-collection-qwen-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN_QKVO[[:space:]]{2,}MLP_GUD[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-collection-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}collection-profiled[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --audit > "$ROOT/tensor-collection-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_status: collection-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_source_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_manifest_status: not-checked'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_config_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_layer_count_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_embedding_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_embedding_tensor_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_q_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_k_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_v_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_o_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_attention_complete_qkvo_layer_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_mlp_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_mlp_gate_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_mlp_up_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_mlp_down_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_mlp_complete_gud_layer_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_norm_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_norm_tensor_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_output_head_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_output_head_tensor_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_moe_status: not-observed'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_moe_router_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_moe_expert_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_tokenizer_collection_status: sidecar-observed'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_validation_status: lexical-and-header-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'tensor_collection_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-collection-qwen-audit.out" 'next_required_rows: V010.MAP.8'
! grep 'generation_ready: tr''ue' "$ROOT/tensor-collection-qwen-audit.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" > "$ROOT/tensor-map-qwen.out"
grep 'tensor-map: qwen3-8b \[reported\]' "$ROOT/tensor-map-qwen.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'family: qwen  stage: header-naming-map  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'roles: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'layers: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'top_blocker: missing-qwen-runtime-role-validation'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen.out" 'boundary: report-only; use --audit for tensor entries'
! grep 'tensor_map.entry.' "$ROOT/tensor-map-qwen.out"
! grep 'runtime_claim:' "$ROOT/tensor-map-qwen.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --output table > "$ROOT/tensor-map-qwen-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-qwen-table.out"
matches "$ROOT/tensor-map-qwen-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}TOTAL[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN[[:space:]]{2,}MLP[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}UNKNOWN[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-map-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}naming-map-profiled[[:space:]]{2,}12[[:space:]]{2,}1[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --audit > "$ROOT/tensor-map-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_stage: header-naming-map'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_source_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_config_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_mapped_total_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_unmapped_unknown_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_ambiguous_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_layer_count_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_embedding_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_attention_count: 4'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_attention_q_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_attention_k_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_attention_v_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_attention_o_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_mlp_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_mlp_gate_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_mlp_up_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_mlp_down_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_norm_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_output_head_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_moe_router_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_moe_expert_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_validation_status: lexical-and-header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_canonical_role_status: mapped-candidates'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_runtime_role_coverage_status: report-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_artifact_contract_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'tensor_map_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-audit.out" 'next_required_rows: V010.MAP.8'
grep 'tensor_map.entry.' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.embed_tokens.weight -> model.embedding.token.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.layers.0.input_layernorm.weight -> model.layers.0.attention.norm.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'lm_head.weight -> model.output_head.weight' "$ROOT/tensor-map-qwen-audit.out"
! grep 'generation_ready: tr''ue' "$ROOT/tensor-map-qwen-audit.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --check-output-contract normal > "$ROOT/output-contract-qwen-tensor-map-normal.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --check-output-contract table > "$ROOT/output-contract-qwen-tensor-map-table.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --check-output-contract audit > "$ROOT/output-contract-qwen-tensor-map-audit.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" > "$ROOT/output-head-qwen.out"
grep 'output-head-map: qwen3-8b \[reported\]' "$ROOT/output-head-qwen.out"
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'family: qwen  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'head: model.output_head.weight  final_norm: model.final_norm.weight  embedding: model.embedding.token.weight  tie: separate-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'shape: compatible-same-shape'
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'top_blocker: missing-output-head-runtime-consumer'
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/output-head-qwen.out" 'boundary: mapping only; no logits/runtime/generation'
! grep 'output_head_map_' "$ROOT/output-head-qwen.out"
! grep 'native_output_head:' "$ROOT/output-head-qwen.out"
! grep 'runtime_claim:' "$ROOT/output-head-qwen.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" --output table > "$ROOT/output-head-qwen-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-qwen-table.out"
matches "$ROOT/output-head-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}output-head-profiled[[:space:]]{2,}yes[[:space:]]{2,}yes[[:space:]]{2,}yes[[:space:]]{2,}separate-output-head-candidate[[:space:]]{2,}compatible-same-shape[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" --audit > "$ROOT/output-head-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_map_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_map_stage: header-output-head-map'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_native_name: lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_canonical_role: model.output_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_mapping_status: mapped-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_candidate_count: 1'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_ambiguous_count: 0'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_missing_status: present'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'embedding_canonical_role: model.embedding.token.weight'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'final_norm_canonical_role: model.final_norm.weight'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'tie_policy_status: separate-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'config_tie_word_embeddings_status: missing'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'shape_relation_status: compatible-same-shape'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_runtime_consumer_status: target-runtime-owned'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_logits_status: target-capability-dependent'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_artifact_contract_status: artifact-owner'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_runtime_descriptor_status: runtime-owner'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head_graph_consumer_status: runtime-logits-owner'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'next_required_rows: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head.entry.output.native_name: lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-audit.out" 'output_head.entry.output.canonical_role: model.output_head.weight'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" --check-output-contract normal > "$ROOT/output-contract-qwen-output-head-normal.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" --check-output-contract table > "$ROOT/output-contract-qwen-output-head-table.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_COLLECTION_SOURCE" --check-output-contract audit > "$ROOT/output-contract-qwen-output-head-audit.out"

TOKENIZER_COMPLETE_SOURCE="${TMPDIR:-/tmp}/yvex-tokenizer-map-complete-test-$$"
yvex_test_cleanup "$TOKENIZER_COMPLETE_SOURCE"
mkdir -p "$TOKENIZER_COMPLETE_SOURCE"
cat > "$TOKENIZER_COMPLETE_SOURCE/config.json" <<'JSON'
{
  "model_type": "qwen",
  "vocab_size": 16,
  "hidden_size": 8,
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0,
  "unk_token_id": 3,
  "tie_word_embeddings": false
}
JSON
cat > "$TOKENIZER_COMPLETE_SOURCE/tokenizer_config.json" <<'JSON'
{
  "tokenizer_class": "PreTrainedTokenizerFast",
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0,
  "unk_token_id": 3,
  "chat_template": "{{ bos_token }}{{ messages }}"
}
JSON
cat > "$TOKENIZER_COMPLETE_SOURCE/special_tokens_map.json" <<'JSON'
{
  "bos_token": "<s>",
  "eos_token": "</s>",
  "unk_token": "<unk>",
  "pad_token": "<pad>",
  "additional_special_tokens": ["<extra_0>", "<extra_1>"]
}
JSON
cat > "$TOKENIZER_COMPLETE_SOURCE/generation_config.json" <<'JSON'
{
  "bos_token_id": 1,
  "eos_token_id": 2,
  "pad_token_id": 0
}
JSON
cat > "$TOKENIZER_COMPLETE_SOURCE/tokenizer.json" <<'JSON'
{
  "version": "1.0",
  "model": {"type": "BPE"},
  "added_tokens": []
}
JSON
python3 - "$TOKENIZER_COMPLETE_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

items = [
    ("model.embed_tokens.weight", [16, 8]),
    ("model.norm.weight", [8]),
    ("lm_head.weight", [16, 8]),
]
offset = 0
header = {}
for name, shape in items:
    size = 1
    for dim in shape:
        size *= dim
    nbytes = size * 4
    header[name] = {
        "dtype": "F32",
        "shape": shape,
        "data_offsets": [offset, offset + nbytes],
    }
    offset += nbytes
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tokenizer-map qwen3-8b --source "$TOKENIZER_COMPLETE_SOURCE" > "$ROOT/tokenizer-map-qwen.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'tokenizer-map: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'family: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'tokenizer: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'vocab: embedded-or-tokenizer-json'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'chat_template: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'specials: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'runtime: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'top_blocker: quant-policy-or-artifact-emitter'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'next: V010.QUANT.1'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen.out" 'boundary: tokenizer metadata mapping only; no tokenization/detokenization/runtime/generation'

"$YVEX_BIN" inspect target tokenizer-map qwen3-8b --source "$TOKENIZER_COMPLETE_SOURCE" --output table > "$ROOT/tokenizer-map-qwen-table.out"
grep 'TOKENIZER METADATA MAP' "$ROOT/tokenizer-map-qwen-table.out"
matches "$ROOT/tokenizer-map-qwen-table.out" '^TARGET[[:space:]]{2,}FAMILY[[:space:]]{2,}STATUS[[:space:]]{2,}TOKENIZER[[:space:]]{2,}VOCAB[[:space:]]{2,}MERGES[[:space:]]{2,}CHAT_TEMPLATE[[:space:]]{2,}SPECIALS[[:space:]]{2,}NEXT$'
matches "$ROOT/tokenizer-map-qwen-table.out" '^qwen3-8b[[:space:]]{2,}qwen[[:space:]]{2,}present-report-only[[:space:]]{2,}yes[[:space:]]{2,}embedded-or-tokenizer-json[[:space:]]{2,}missing[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}V010\.QUANT\.1$'

"$YVEX_BIN" inspect target tokenizer-map qwen3-8b --source "$TOKENIZER_COMPLETE_SOURCE" --audit > "$ROOT/tokenizer-map-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'schema_version: yvex.source.tokenizer_map.v1'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_map_family: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_map_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_map_stage: metadata-tokenizer-map'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_map_evidence_basis: sidecar-json-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_json_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_config_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'special_tokens_map_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'generation_config_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'config_json_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_class: PreTrainedTokenizerFast'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'model_type: qwen'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'vocab_size_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'vocab_size: 16'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'config_vocab_size: 16'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'output_head_vocab_dim_candidate: 16'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'output_head_vocab_relation_status: vocab-size-matches-output-head'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'bos_token_id_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'bos_token_id: 1'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'eos_token_id_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'eos_token_id: 2'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'pad_token_id_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'pad_token_id: 0'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'unk_token_id_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'unk_token_id: 3'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'additional_special_tokens_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'additional_special_tokens_count: 2'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'chat_template_status: present'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'chat_template_present: true'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'chat_template_hash_status: not-computed'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'gguf_tokenizer_contract_status: planned'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'tokenization_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'detokenization_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'eos_stop_policy_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-qwen-audit.out" 'next_required_rows: V010.QUANT.1'

"$YVEX_BIN" inspect target tokenizer-map gemma-4-12b-it --source "$TOKENIZER_COMPLETE_SOURCE" > "$ROOT/tokenizer-map-gemma.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma.out" 'tokenizer-map: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma.out" 'family: gemma'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma.out" 'status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma.out" 'next: V010.QUANT.1'

"$YVEX_BIN" inspect target tokenizer-map gemma-4-12b-it --source "$TOKENIZER_COMPLETE_SOURCE" --output table > "$ROOT/tokenizer-map-gemma-table.out"
grep 'TOKENIZER METADATA MAP' "$ROOT/tokenizer-map-gemma-table.out"
matches "$ROOT/tokenizer-map-gemma-table.out" '^gemma-4-12b-it[[:space:]]{2,}gemma[[:space:]]{2,}present-report-only[[:space:]]{2,}yes[[:space:]]{2,}embedded-or-tokenizer-json[[:space:]]{2,}not-required-or-absent[[:space:]]{2,}present[[:space:]]{2,}present[[:space:]]{2,}V010\.QUANT\.1$'

"$YVEX_BIN" inspect target tokenizer-map gemma-4-12b-it --source "$TOKENIZER_COMPLETE_SOURCE" --audit > "$ROOT/tokenizer-map-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'tokenizer_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'tokenizer_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'tokenizer_map_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'output_head_vocab_relation_status: vocab-size-matches-output-head'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-gemma-audit.out" 'generation: unsupported-full-model'

TOKENIZER_MISSING_SOURCE="${TMPDIR:-/tmp}/yvex-tokenizer-map-missing-test-$$"
yvex_test_cleanup "$TOKENIZER_MISSING_SOURCE"
mkdir -p "$TOKENIZER_MISSING_SOURCE"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --source "$TOKENIZER_MISSING_SOURCE" --audit > "$ROOT/tokenizer-map-metadata-missing-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-metadata-missing-audit.out" 'tokenizer_map_status: metadata-missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-metadata-missing-audit.out" 'top_blocker: missing-tokenizer-sidecars'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-metadata-missing-audit.out" 'next_required_rows: V010.MAP.7'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-metadata-missing-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-metadata-missing-audit.out" 'generation: unsupported-full-model'
yvex_test_cleanup "$TOKENIZER_MISSING_SOURCE"

TOKENIZER_INCOMPLETE_SOURCE="${TMPDIR:-/tmp}/yvex-tokenizer-map-incomplete-test-$$"
yvex_test_cleanup "$TOKENIZER_INCOMPLETE_SOURCE"
mkdir -p "$TOKENIZER_INCOMPLETE_SOURCE"
printf '{"vocab_size":16}\n' > "$TOKENIZER_INCOMPLETE_SOURCE/config.json"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --source "$TOKENIZER_INCOMPLETE_SOURCE" --audit > "$ROOT/tokenizer-map-incomplete-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-incomplete-audit.out" 'tokenizer_map_status: tokenizer-metadata-incomplete'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-incomplete-audit.out" 'vocab_size: 16'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-incomplete-audit.out" 'tokenizer_json_status: missing'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-incomplete-audit.out" 'tokenizer_runtime_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-incomplete-audit.out" 'next_required_rows: V010.MAP.7'
yvex_test_cleanup "$TOKENIZER_INCOMPLETE_SOURCE"

TOKENIZER_MISMATCH_SOURCE="${TMPDIR:-/tmp}/yvex-tokenizer-map-mismatch-test-$$"
yvex_test_cleanup "$TOKENIZER_MISMATCH_SOURCE"
cp -R "$TOKENIZER_COMPLETE_SOURCE" "$TOKENIZER_MISMATCH_SOURCE"
perl -0pi -e 's/"vocab_size": 16/"vocab_size": 17/' "$TOKENIZER_MISMATCH_SOURCE/config.json"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --source "$TOKENIZER_MISMATCH_SOURCE" --audit > "$ROOT/tokenizer-map-mismatch-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-mismatch-audit.out" 'tokenizer_map_status: tokenizer-metadata-ambiguous'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-mismatch-audit.out" 'vocab_size: 17'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-mismatch-audit.out" 'output_head_vocab_relation_status: vocab-size-mismatch-output-head'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-mismatch-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-mismatch-audit.out" 'generation: unsupported-full-model'
yvex_test_cleanup "$TOKENIZER_MISMATCH_SOURCE"

TOKENIZER_MALFORMED_SOURCE="${TMPDIR:-/tmp}/yvex-tokenizer-map-malformed-test-$$"
yvex_test_cleanup "$TOKENIZER_MALFORMED_SOURCE"
cp -R "$TOKENIZER_COMPLETE_SOURCE" "$TOKENIZER_MALFORMED_SOURCE"
printf '{"tokenizer_class":' > "$TOKENIZER_MALFORMED_SOURCE/tokenizer_config.json"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --source "$TOKENIZER_MALFORMED_SOURCE" --audit > "$ROOT/tokenizer-map-malformed-audit.out"
python3 tests/support/human_field.py "$ROOT/tokenizer-map-malformed-audit.out" 'tokenizer_map_status: tokenizer-metadata-malformed'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-malformed-audit.out" 'tokenizer_config_status: malformed'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-malformed-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tokenizer-map-malformed-audit.out" 'generation: unsupported-full-model'
yvex_test_cleanup "$TOKENIZER_MALFORMED_SOURCE"
yvex_test_cleanup "$TOKENIZER_COMPLETE_SOURCE"

MISSING_ROLE_COMPLETE_SOURCE="${TMPDIR:-/tmp}/yvex-missing-role-complete-test-$$"
make_missing_role_source "$MISSING_ROLE_COMPLETE_SOURCE" complete

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" > "$ROOT/missing-role-qwen.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'family: qwen  evidence: header+sidecar-only'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'source_roles: 12/12 present, 0 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'metadata_roles: 4/4 present, 0 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'top_blocker: missing-artifact-contract'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'next: V010.MAP.9'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen.out" 'boundary: report-only; use --audit for role details'
! grep 'missing_role.entry.' "$ROOT/missing-role-qwen.out"
! grep 'downstream_blockers:' "$ROOT/missing-role-qwen.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --output table > "$ROOT/missing-role-qwen-table.out"
grep 'MISSING ROLE BLOCKER REPORT' "$ROOT/missing-role-qwen-table.out"
matches "$ROOT/missing-role-qwen-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}OBS_SRC[[:space:]]{2,}MISS_SRC[[:space:]]{2,}AMBIG_SRC[[:space:]]{2,}OBS_META[[:space:]]{2,}MISS_META[[:space:]]{2,}TOP_BLOCKER[[:space:]]{2,}NEXT$'
matches "$ROOT/missing-role-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}missing-role-report-blocked[[:space:]]{2,}12[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}4[[:space:]]{2,}0[[:space:]]{2,}missing-artifact-contract[[:space:]]{2,}V010\.MAP\.9$'

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --audit > "$ROOT/missing-role-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_report_status: missing-role-report-blocked'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_report_family: qwen'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_report_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_report_stage: missing-role-blocker-report'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_report_evidence_basis: header-and-sidecar-metadata-only'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_source_role_required_count: 12'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_source_role_observed_count: 12'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_source_role_missing_count: 0'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_metadata_required_count: 4'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_metadata_observed_count: 4'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_metadata_missing_count: 0'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_embedding_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_attention_norm_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_attention_q_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_attention_k_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_attention_v_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_attention_o_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_mlp_norm_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_mlp_gate_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_mlp_up_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_mlp_down_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_final_norm_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_output_head_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_tokenizer_metadata_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_config_metadata_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_generation_metadata_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_special_tokens_status: present'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_artifact_contract_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_runtime_descriptor_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_graph_consumer_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_logits_runtime_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_tokenizer_runtime_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_top_blocker: missing-artifact-contract'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'missing_role_next_required_row: V010.MAP.9'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-audit.out" 'release_ready: false'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-qwen-missing-roles-normal.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract table > "$ROOT/output-contract-qwen-missing-roles-table.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-qwen-missing-roles-audit.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" > "$ROOT/missing-role-gemma.out"
grep 'missing-roles: gemma-4-12b-it \[blocked\]' "$ROOT/missing-role-gemma.out"
python3 tests/support/human_field.py "$ROOT/missing-role-gemma.out" 'family: gemma  evidence: header+sidecar-only'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma.out" 'source_roles: 12/12 present, 0 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma.out" 'metadata_roles: 4/4 present, 0 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma.out" 'top_blocker: missing-artifact-contract'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma.out" 'next: V010.MAP.9'
! grep 'missing_role.entry.' "$ROOT/missing-role-gemma.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --output table > "$ROOT/missing-role-gemma-table.out"
matches "$ROOT/missing-role-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}missing-role-report-blocked[[:space:]]{2,}12[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}4[[:space:]]{2,}0[[:space:]]{2,}missing-artifact-contract[[:space:]]{2,}V010\.MAP\.9$'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --audit > "$ROOT/missing-role-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'missing_role_report_status: missing-role-report-blocked'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'missing_role_report_family: gemma'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'missing_role_source_role_observed_count: 12'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'missing_role_metadata_observed_count: 4'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-audit.out" 'generation: unsupported-full-model'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-gemma-missing-roles-normal.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract table > "$ROOT/output-contract-gemma-missing-roles-table.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-gemma-missing-roles-audit.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" > "$ROOT/tensor-mapping-gate-qwen.out"
grep 'tensor-mapping-gate: qwen3-8b \[reported\]' "$ROOT/tensor-mapping-gate-qwen.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'gate: v0.1.0  family: qwen'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'roles: source 12/12, metadata 4/4, missing 0, ambiguous 0'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'result: pass'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'top_blocker: missing-qtype-policy-report'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'next: V010.QUANT.0'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen.out" 'boundary: report-only; no artifact/runtime/generation'
! grep 'tensor_naming_map:' "$ROOT/tensor-mapping-gate-qwen.out"
! grep 'runtime_claim:' "$ROOT/tensor-mapping-gate-qwen.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --output table > "$ROOT/tensor-mapping-gate-qwen-table.out"
grep 'TENSOR MAPPING GATE' "$ROOT/tensor-mapping-gate-qwen-table.out"
matches "$ROOT/tensor-mapping-gate-qwen-table.out" '^TARGET[[:space:]]{2,}FAMILY[[:space:]]{2,}GATE[[:space:]]{2,}SOURCE_ROLES[[:space:]]{2,}META_ROLES[[:space:]]{2,}MISSING[[:space:]]{2,}AMBIG[[:space:]]{2,}TOP_BLOCKER[[:space:]]{2,}STATUS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-mapping-gate-qwen-table.out" '^qwen3-8b[[:space:]]{2,}qwen[[:space:]]{2,}v0\.1\.0[[:space:]]{2,}12/12[[:space:]]{2,}4/4[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}missing-qtype-policy-report[[:space:]]{2,}passed-for-artifact-planning[[:space:]]{2,}V010\.QUANT\.0$'
! grep 'runtime_claim:' "$ROOT/tensor-mapping-gate-qwen-table.out"
! grep 'release_ready:' "$ROOT/tensor-mapping-gate-qwen-table.out"

"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --audit > "$ROOT/tensor-mapping-gate-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'tensor_mapping_gate_status: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'tensor_mapping_gate_result: pass'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'tensor_mapping_gate_target_id: qwen3-8b'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'tensor_naming_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'tokenizer_metadata_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'missing_role_report_status: missing-role-report-blocked'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'expected_source_role_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'observed_source_role_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'expected_metadata_role_count: 4'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'observed_metadata_role_count: 4'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'missing_roles: none'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'ambiguous_roles: none'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'downstream_blockers: artifact_contract=missing qtype_policy=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing logits_runtime=missing tokenizer_runtime=missing generation_runtime=missing eval_benchmark=missing'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'next_required_rows: V010.QUANT.0'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'payload_bytes_read: false'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'artifact_emitted: false'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'runtime_descriptor_constructed: false'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'graph_consumer_fed: false'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-audit.out" 'release_ready: false'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-qwen-gate-normal.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract table > "$ROOT/output-contract-qwen-gate-table.out"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-qwen-gate-audit.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" > "$ROOT/tensor-mapping-gate-gemma.out"
grep 'tensor-mapping-gate: gemma-4-12b-it \[reported\]' "$ROOT/tensor-mapping-gate-gemma.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma.out" 'gate: v0.1.0  family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma.out" 'roles: source 12/12, metadata 4/4, missing 0, ambiguous 0'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma.out" 'next: V010.QUANT.0'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --output table > "$ROOT/tensor-mapping-gate-gemma-table.out"
matches "$ROOT/tensor-mapping-gate-gemma-table.out" '^gemma-4-12b-it[[:space:]]{2,}gemma[[:space:]]{2,}v0\.1\.0[[:space:]]{2,}12/12[[:space:]]{2,}4/4[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}missing-qtype-policy-report[[:space:]]{2,}passed-for-artifact-planning[[:space:]]{2,}V010\.QUANT\.0$'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --audit > "$ROOT/tensor-mapping-gate-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma-audit.out" 'tensor_mapping_gate_status: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma-audit.out" 'tensor_mapping_gate_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma-audit.out" 'next_required_rows: V010.QUANT.0'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-gemma-gate-normal.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract table > "$ROOT/output-contract-gemma-gate-table.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --source "$MISSING_ROLE_COMPLETE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-gemma-gate-audit.out"

MISSING_ROLE_NO_K_SOURCE="${TMPDIR:-/tmp}/yvex-missing-role-no-k-test-$$"
make_missing_role_source "$MISSING_ROLE_NO_K_SOURCE" missing-attention-k
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_K_SOURCE" > "$ROOT/missing-role-qwen-no-k.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen-no-k.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k.out" 'source_roles: 11/12 present, 1 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k.out" 'missing_source: attention_k'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k.out" 'top_blocker: missing-source-role-attention-k'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_K_SOURCE" --audit > "$ROOT/missing-role-qwen-no-k-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k-audit.out" 'missing_role_attention_k_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k-audit.out" 'missing_role_top_blocker: missing-source-role-attention-k'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k-audit.out" 'missing_role.entry.0.role: attention_k'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-k-audit.out" 'missing_role.entry.0.blocker_class: source-role-missing'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_NO_K_SOURCE" > "$ROOT/tensor-mapping-gate-qwen-no-k.out"
grep 'tensor-mapping-gate: qwen3-8b \[blocked\]' "$ROOT/tensor-mapping-gate-qwen-no-k.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-k.out" 'roles: source 11/12, metadata 4/4, missing 1, ambiguous 0'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-k.out" 'missing: attention_k'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-k.out" 'top_blocker: missing-source-role-attention-k'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-k.out" 'next: V010.MAP.9'

MISSING_ROLE_NO_HEAD_SOURCE="${TMPDIR:-/tmp}/yvex-missing-role-no-head-test-$$"
make_missing_role_source "$MISSING_ROLE_NO_HEAD_SOURCE" missing-output-head
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_HEAD_SOURCE" > "$ROOT/missing-role-qwen-no-head.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen-no-head.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-head.out" 'missing_source: output_head'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-head.out" 'top_blocker: missing-source-role-output-head'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_HEAD_SOURCE" --audit > "$ROOT/missing-role-qwen-no-head-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-head-audit.out" 'missing_role_output_head_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-head-audit.out" 'missing_role_top_blocker: missing-source-role-output-head'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_NO_HEAD_SOURCE" > "$ROOT/tensor-mapping-gate-qwen-no-head.out"
grep 'tensor-mapping-gate: qwen3-8b \[blocked\]' "$ROOT/tensor-mapping-gate-qwen-no-head.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-head.out" 'missing: output_head'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-head.out" 'top_blocker: missing-output-head-tensor'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-head.out" 'next: V010.MAP.9'

MISSING_ROLE_NO_METADATA_SOURCE="${TMPDIR:-/tmp}/yvex-missing-role-no-metadata-test-$$"
make_missing_role_source "$MISSING_ROLE_NO_METADATA_SOURCE" missing-metadata
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_METADATA_SOURCE" > "$ROOT/missing-role-qwen-no-metadata.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen-no-metadata.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata.out" 'source_roles: 12/12 present, 0 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata.out" 'metadata_roles: 0/4 present, 4 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata.out" 'missing_metadata: tokenizer_metadata,config_metadata,generation_metadata,special_tokens'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata.out" 'top_blocker: missing-tokenizer-metadata'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_NO_METADATA_SOURCE" --audit > "$ROOT/missing-role-qwen-no-metadata-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata-audit.out" 'missing_role_tokenizer_metadata_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata-audit.out" 'missing_role_config_metadata_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata-audit.out" 'missing_role_generation_metadata_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata-audit.out" 'missing_role_special_tokens_status: missing'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-no-metadata-audit.out" 'missing_role_top_blocker: missing-tokenizer-metadata'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_NO_METADATA_SOURCE" > "$ROOT/tensor-mapping-gate-qwen-no-metadata.out"
grep 'tensor-mapping-gate: qwen3-8b \[blocked\]' "$ROOT/tensor-mapping-gate-qwen-no-metadata.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-metadata.out" 'missing: tokenizer_metadata,config_metadata,generation_metadata,special_tokens'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-metadata.out" 'top_blocker: missing-tokenizer-sidecars'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-no-metadata.out" 'next: V010.MAP.9'

MISSING_ROLE_AMBIG_SOURCE="${TMPDIR:-/tmp}/yvex-missing-role-ambig-test-$$"
make_missing_role_source "$MISSING_ROLE_AMBIG_SOURCE" ambiguous-output-head
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_AMBIG_SOURCE" > "$ROOT/missing-role-qwen-ambiguous.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen-ambiguous.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous.out" 'source_roles: 11/12 present, 0 missing, 1 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous.out" 'top_blocker: ambiguous-source-role-output-head'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source "$MISSING_ROLE_AMBIG_SOURCE" --audit > "$ROOT/missing-role-qwen-ambiguous-audit.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous-audit.out" 'missing_role_output_head_status: ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous-audit.out" 'missing_role_top_blocker: ambiguous-source-role-output-head'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous-audit.out" 'missing_role.entry.0.role: output_head'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-ambiguous-audit.out" 'missing_role.entry.0.blocker_class: source-role-ambiguous'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --source "$MISSING_ROLE_AMBIG_SOURCE" > "$ROOT/tensor-mapping-gate-qwen-ambiguous.out"
grep 'tensor-mapping-gate: qwen3-8b \[blocked\]' "$ROOT/tensor-mapping-gate-qwen-ambiguous.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-ambiguous.out" 'ambiguous: output_head'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-ambiguous.out" 'top_blocker: ambiguous-output-head-tensor'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-ambiguous.out" 'next: V010.MAP.9'

MISSING_ROLE_MISSING_ROOT="$ROOT/missing-role-missing-root"
yvex_test_cleanup "$MISSING_ROLE_MISSING_ROOT"
"$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --models-root "$MISSING_ROLE_MISSING_ROOT" > "$ROOT/missing-role-qwen-missing-source.out"
grep 'missing-roles: qwen3-8b \[blocked\]' "$ROOT/missing-role-qwen-missing-source.out"
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-missing-source.out" 'source_roles: 0/12 present, 12 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-missing-source.out" 'metadata_roles: 0/4 present, 4 missing, 0 ambiguous'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-missing-source.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/missing-role-qwen-missing-source.out" 'next: V010.MAP.9'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --models-root "$MISSING_ROLE_MISSING_ROOT" > "$ROOT/tensor-mapping-gate-qwen-missing-source.out"
grep 'tensor-mapping-gate: qwen3-8b \[blocked\]' "$ROOT/tensor-mapping-gate-qwen-missing-source.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-missing-source.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-qwen-missing-source.out" 'next: V010.MAP.9'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role missing-roles --models-root "$MISSING_ROLE_MISSING_ROOT" > "$ROOT/missing-role-gemma-missing-source.out"
grep 'missing-roles: gemma-4-12b-it \[blocked\]' "$ROOT/missing-role-gemma-missing-source.out"
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-missing-source.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/missing-role-gemma-missing-source.out" 'next: V010.MAP.9'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --gate v0.1.0 --models-root "$MISSING_ROLE_MISSING_ROOT" > "$ROOT/tensor-mapping-gate-gemma-missing-source.out"
grep 'tensor-mapping-gate: gemma-4-12b-it \[blocked\]' "$ROOT/tensor-mapping-gate-gemma-missing-source.out"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma-missing-source.out" 'top_blocker: missing-gemma-source-path'
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-gemma-missing-source.out" 'next: V010.MAP.9'

MODEL_TARGET_QTYPE_SOURCE="$MISSING_ROLE_COMPLETE_SOURCE"

"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" > "$ROOT/qtype-policy-qwen.out"
grep 'qtype-policy: qwen3-8b \[reported\]' "$ROOT/qtype-policy-qwen.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'family: qwen  mapping_gate: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'source_dtype: F32=12 F16=0 BF16=0 other=0'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'policy: artifact-planning-storage-policy'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'preferred: F16'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'candidates: F16,BF16,F32,Q8_0,Q2_K,IQ2_XXS'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'refused: Q4_K'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'top_blocker: family-quantization-plan-unimplemented'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'next: not-scheduled'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen.out" 'boundary: report-only; no quantization/artifact/runtime'
! grep 'qtype_policy_status:' "$ROOT/qtype-policy-qwen.out"
! grep 'calibration_status:' "$ROOT/qtype-policy-qwen.out"
! grep 'runtime_claim:' "$ROOT/qtype-policy-qwen.out"

"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" --output table > "$ROOT/qtype-policy-qwen-table.out"
grep 'QTYPE POLICY' "$ROOT/qtype-policy-qwen-table.out"
matches "$ROOT/qtype-policy-qwen-table.out" '^TARGET[[:space:]]{2,}FAMILY[[:space:]]{2,}SOURCE_DTYPE[[:space:]]{2,}POLICY[[:space:]]{2,}PREFERRED[[:space:]]{2,}CANDIDATES[[:space:]]{2,}REFUSED[[:space:]]{2,}STATUS[[:space:]]{2,}NEXT$'
matches "$ROOT/qtype-policy-qwen-table.out" '^qwen3-8b[[:space:]]{2,}qwen[[:space:]]{2,}F32=12 F16=0 BF16=0 other=0[[:space:]]{2,}artifact-planning-storage-policy[[:space:]]{2,}F16[[:space:]]{2,}F16,BF16,F32,Q8_0,Q2_K,IQ2_XXS[[:space:]]{2,}Q4_K[[:space:]]{2,}policy-reported[[:space:]]{2,}not-scheduled$'
! grep 'CALIBRATION' "$ROOT/qtype-policy-qwen-table.out"
! grep 'calibration_status:' "$ROOT/qtype-policy-qwen-table.out"
! grep 'runtime_claim:' "$ROOT/qtype-policy-qwen-table.out"

"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" --audit > "$ROOT/qtype-policy-qwen-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'source_dtype_profile_status: profiled'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'source_dtype_counts: F32=12,F16=0,BF16=0'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'source_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'mapping_gate_status: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'tensor_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'tokenizer_metadata_map_status: present-report-only'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'missing_role_report_status: missing-role-report-blocked'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'qtype_policy_basis: header-only-source-metadata+canonical-numeric-registry'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'qtype_policy_status: reported'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'numeric_capability.Q8_0: encoder=available decoder=available cpu=available cuda=available calibration=none'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'numeric_capability.Q2_K: encoder=available decoder=available cpu=available cuda=available calibration=optional'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'refusal_reasons: Q4_K:encoder-unavailable IQ2_XXS:calibration-required'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'artifact_identity_status: missing'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'runtime_descriptor_status: missing'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'graph_consumer_status: missing'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'backend_residency_status: missing'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'downstream_blockers: family_quantization_plan=missing artifact_emit=missing artifact_identity=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing generation_runtime=missing eval_benchmark=missing'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'next_required_rows: not-scheduled'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-audit.out" 'release_ready: false'
"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-qwen-qtype-normal.out"
"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract table > "$ROOT/output-contract-qwen-qtype-table.out"
"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-qwen-qtype-audit.out"

"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" > "$ROOT/qtype-policy-gemma.out"
grep 'qtype-policy: gemma-4-12b-it \[reported\]' "$ROOT/qtype-policy-gemma.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma.out" 'family: gemma  mapping_gate: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma.out" 'source_dtype: F32=12 F16=0 BF16=0 other=0'
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma.out" 'preferred: F16'
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma.out" 'next: not-scheduled'
"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" --output table > "$ROOT/qtype-policy-gemma-table.out"
matches "$ROOT/qtype-policy-gemma-table.out" '^gemma-4-12b-it[[:space:]]{2,}gemma[[:space:]]{2,}F32=12 F16=0 BF16=0 other=0[[:space:]]{2,}artifact-planning-storage-policy[[:space:]]{2,}F16[[:space:]]{2,}F16,BF16,F32,Q8_0,Q2_K,IQ2_XXS[[:space:]]{2,}Q4_K[[:space:]]{2,}policy-reported[[:space:]]{2,}not-scheduled$'
"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" --audit > "$ROOT/qtype-policy-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma-audit.out" 'mapping_gate_status: passed-for-artifact-planning'
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma-audit.out" 'qtype_policy_status: reported'
python3 tests/support/human_field.py "$ROOT/qtype-policy-gemma-audit.out" 'next_required_rows: not-scheduled'
"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract normal > "$ROOT/output-contract-gemma-qtype-normal.out"
"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract table > "$ROOT/output-contract-gemma-qtype-table.out"
"$YVEX_BIN" inspect target quant-policy gemma-4-12b-it --source "$MODEL_TARGET_QTYPE_SOURCE" --check-output-contract audit > "$ROOT/output-contract-gemma-qtype-audit.out"

"$YVEX_BIN" inspect target quant-policy qwen3-8b --models-root "$MISSING_ROLE_MISSING_ROOT" > "$ROOT/qtype-policy-qwen-missing-source.out"
grep 'qtype-policy: qwen3-8b \[blocked\]' "$ROOT/qtype-policy-qwen-missing-source.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-source.out" 'family: qwen  mapping_gate: blocked-missing-source'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-source.out" 'top_blocker: missing-qwen-source-path'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-source.out" 'next: V010.MAP.9'

QTYPE_MISSING_DTYPE_SOURCE="${TMPDIR:-/tmp}/yvex-qtype-policy-missing-dtype-test-$$"
yvex_test_cleanup "$QTYPE_MISSING_DTYPE_SOURCE"
cp -R "$MODEL_TARGET_QTYPE_SOURCE" "$QTYPE_MISSING_DTYPE_SOURCE"
rm -f "$QTYPE_MISSING_DTYPE_SOURCE"/*.safetensors
"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$QTYPE_MISSING_DTYPE_SOURCE" > "$ROOT/qtype-policy-qwen-missing-dtype.out"
grep 'qtype-policy: qwen3-8b \[blocked\]' "$ROOT/qtype-policy-qwen-missing-dtype.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-dtype.out" 'source_dtype: F32=0 F16=0 BF16=0 other=0'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-dtype.out" 'top_blocker: missing-source-dtype-profile'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-missing-dtype.out" 'next: V010.MAP.9'
yvex_test_cleanup "$QTYPE_MISSING_DTYPE_SOURCE"

"$YVEX_BIN" inspect target quant-policy qwen3-8b --source "$MISSING_ROLE_NO_K_SOURCE" > "$ROOT/qtype-policy-qwen-blocked-gate.out"
grep 'qtype-policy: qwen3-8b \[blocked\]' "$ROOT/qtype-policy-qwen-blocked-gate.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-blocked-gate.out" 'family: qwen  mapping_gate: blocked-missing-runtime-roles'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-blocked-gate.out" 'top_blocker: missing-source-role-attention-k'
python3 tests/support/human_field.py "$ROOT/qtype-policy-qwen-blocked-gate.out" 'next: V010.MAP.9'

expect_rc 2 "$YVEX_BIN" inspect target quant-policy nope > "$ROOT/qtype-policy-unknown-target.out" 2> "$ROOT/qtype-policy-unknown-target.err"
grep 'qtype-policy: nope \[unsupported\]' "$ROOT/qtype-policy-unknown-target.out"
"$YVEX_BIN" inspect target quant-policy deepseek4-v4-flash-dspark-selected-embed > "$ROOT/qtype-policy-unsupported-class.out"
grep 'qtype-policy: deepseek4-v4-flash-dspark-selected-embed \[blocked\]' "$ROOT/qtype-policy-unsupported-class.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-unsupported-class.out" 'top_blocker: unsupported-target-class'
"$YVEX_BIN" inspect target quant-policy deepseek4-v4-flash-dspark-selected-embed-rmsnorm --role-support > "$ROOT/qtype-role-support-deepseek-selected.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'qtype-role-support: deepseek4-v4-flash-dspark-selected-embed-rmsnorm'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'family: deepseek'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'status: blocked'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'source_dtype: selected-slice'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'top_blocker: complete-artifact-admission-required'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected.out" 'next: V010.ARTIFACT.MATERIALIZE.0'
"$YVEX_BIN" inspect target quant-policy deepseek4-v4-flash-dspark-selected-embed-rmsnorm --role-support --audit > "$ROOT/qtype-role-support-deepseek-selected-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected-audit.out" 'selected_slice_evidence_only: true'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected-audit.out" 'full_family_artifact_status: missing'
grep 'role\.[0-9][0-9]*\.role_status: selected-slice-evidence-only' "$ROOT/qtype-role-support-deepseek-selected-audit.out"
grep 'role\.[0-9][0-9]*\.artifact_emission_blocker: complete-artifact-admission-required' "$ROOT/qtype-role-support-deepseek-selected-audit.out"
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/qtype-role-support-deepseek-selected-audit.out" 'generation: unsupported-full-model'
"$YVEX_BIN" inspect target quant-policy glm-5.2-official-safetensors > "$ROOT/qtype-policy-unsupported-family.out"
grep 'qtype-policy: glm-5.2-official-safetensors \[unsupported\]' "$ROOT/qtype-policy-unsupported-family.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-unsupported-family.out" 'top_blocker: unsupported-family'
expect_rc 2 "$YVEX_BIN" inspect target quant-policy > "$ROOT/qtype-policy-missing-target.out" 2> "$ROOT/qtype-policy-missing-target.err"
grep 'requires TARGET' "$ROOT/qtype-policy-missing-target.err"
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-8b --output nope > "$ROOT/qtype-policy-bad-output.out" 2> "$ROOT/qtype-policy-bad-output.err"
python3 tests/support/human_field.py "$ROOT/qtype-policy-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target quant-policy nope --check-output-contract normal > "$ROOT/qtype-policy-contract-unknown-target.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-contract-unknown-target.out" 'status: unsupported-target'
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-8b --check-output-contract nope > "$ROOT/qtype-policy-contract-bad-mode.out"
python3 tests/support/human_field.py "$ROOT/qtype-policy-contract-bad-mode.out" 'status: unsupported-mode'
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-8b --check-output-contract \
  > "$ROOT/qtype-policy-contract-missing-mode.out" \
  2> "$ROOT/qtype-policy-contract-missing-mode.err"
grep -- '--check-output-contract requires a value' \
  "$ROOT/qtype-policy-contract-missing-mode.err"
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-8b --source > "$ROOT/qtype-policy-missing-source-arg.out" 2> "$ROOT/qtype-policy-missing-source-arg.err"
grep -- '--source requires a value' "$ROOT/qtype-policy-missing-source-arg.err"
expect_rc 2 "$YVEX_BIN" inspect target quant-policy qwen3-8b --models-root > "$ROOT/qtype-policy-missing-models-root.out" 2> "$ROOT/qtype-policy-missing-models-root.err"
grep -- '--models-root requires a value' "$ROOT/qtype-policy-missing-models-root.err"

yvex_test_cleanup "$MISSING_ROLE_COMPLETE_SOURCE" "$MISSING_ROLE_NO_K_SOURCE" \
  "$MISSING_ROLE_NO_HEAD_SOURCE" "$MISSING_ROLE_NO_METADATA_SOURCE" \
  "$MISSING_ROLE_AMBIG_SOURCE"

QWEN_OUTPUT_HEAD_MISSING_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-output-head-missing-test-$$"
yvex_test_cleanup "$QWEN_OUTPUT_HEAD_MISSING_SOURCE"
mkdir -p "$QWEN_OUTPUT_HEAD_MISSING_SOURCE"
python3 - "$QWEN_OUTPUT_HEAD_MISSING_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

items = [
    ("model.embed_tokens.weight", [16, 8]),
    ("model.norm.weight", [8]),
]
offset = 0
header = {}
for name, shape in items:
    size = 1
    for dim in shape:
        size *= dim
    nbytes = size * 4
    header[name] = {
        "dtype": "F32",
        "shape": shape,
        "data_offsets": [offset, offset + nbytes],
    }
    offset += nbytes
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_OUTPUT_HEAD_MISSING_SOURCE" --audit > "$ROOT/output-head-qwen-missing-head-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing-head-audit.out" 'output_head_map_status: output-head-missing'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing-head-audit.out" 'output_head_missing_status: missing'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing-head-audit.out" 'top_blocker: missing-output-head-tensor'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing-head-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-missing-head-audit.out" 'generation: unsupported-full-model'
yvex_test_cleanup "$QWEN_OUTPUT_HEAD_MISSING_SOURCE"

QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-output-head-ambiguous-test-$$"
yvex_test_cleanup "$QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE"
mkdir -p "$QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE"
python3 - "$QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

items = [
    ("model.embed_tokens.weight", [16, 8]),
    ("model.norm.weight", [8]),
    ("lm_head.weight", [16, 8]),
    ("output.weight", [16, 8]),
]
offset = 0
header = {}
for name, shape in items:
    size = 1
    for dim in shape:
        size *= dim
    nbytes = size * 4
    header[name] = {
        "dtype": "F32",
        "shape": shape,
        "data_offsets": [offset, offset + nbytes],
    }
    offset += nbytes
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source "$QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE" --audit > "$ROOT/output-head-qwen-ambiguous-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'output_head_map_status: output-head-ambiguous'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'output_head_candidate_count: 2'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'output_head_ambiguous_count: 1'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'output_head_mapping_status: ambiguous'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-qwen-ambiguous-audit.out" 'generation: unsupported-full-model'
yvex_test_cleanup "$QWEN_OUTPUT_HEAD_AMBIGUOUS_SOURCE"

QWEN_TENSOR_MAP_UNKNOWN_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-tensor-map-unknown-test-$$"
yvex_test_cleanup "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
mkdir -p "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
printf '{}\n' > "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/config.json"
printf '{}\n' > "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/tokenizer.json"
python3 - "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
    "model.layers.0.weird_unknown.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE" > "$ROOT/tensor-map-qwen-unknown.out"
grep 'tensor-map: qwen3-8b \[naming-map-candidate\]' "$ROOT/tensor-map-qwen-unknown.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown.out" 'roles: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=1'
"$YVEX_BIN" inspect target tensor-map qwen3-8b --source "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE" --audit > "$ROOT/tensor-map-qwen-unknown-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown-audit.out" 'tensor_map_status: naming-map-candidate'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown-audit.out" 'tensor_map_tensor_count: 13'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown-audit.out" 'tensor_map_mapped_total_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown-audit.out" 'tensor_map_unmapped_unknown_count: 1'
grep 'tensor_map.entry.' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'model.layers.0.weird_unknown.weight' "$ROOT/tensor-map-qwen-unknown-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-qwen-unknown-audit.out" 'mapping_status: unmapped-unknown'
yvex_test_cleanup "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
yvex_test_cleanup "$QWEN_COLLECTION_SOURCE"

BAD_RUNTIME_CLAIM='runtime_claim: support''ed'
BAD_GENERATION_READY='generation_ready: tr''ue'
BAD_BENCHMARK_MEASURED='benchmark_status: measur''ed'
BAD_RELEASE_READY='release_ready: tr''ue'
! grep "$BAD_RUNTIME_CLAIM" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_GENERATION_READY" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_BENCHMARK_MEASURED" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_RELEASE_READY" "$ROOT/model-class-qwen-audit.out"
yvex_test_cleanup "$QWEN_CLASS_SOURCE"

GEMMA_CLASS_SOURCE="${TMPDIR:-/tmp}/yvex-gemma-class-profile-test-$$"
yvex_test_cleanup "$GEMMA_CLASS_SOURCE"
mkdir -p "$GEMMA_CLASS_SOURCE"
printf '{}\n' > "$GEMMA_CLASS_SOURCE/config.json"
printf '{}\n' > "$GEMMA_CLASS_SOURCE/tokenizer.json"
python3 - "$GEMMA_CLASS_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" > "$ROOT/model-class-gemma.out"
python3 tests/support/human_field.py "$ROOT/model-class-gemma.out" 'status: metadata-profiled'
python3 tests/support/human_field.py "$ROOT/model-class-gemma.out" 'class: gemma-source-model-class-profile'
python3 tests/support/human_field.py "$ROOT/model-class-gemma.out" 'patterns: tensors=12 attn=4 mlp=3 norm=3 head=1 moe=0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma.out" 'top_blocker: missing-gemma-tensor-role-map'
python3 tests/support/human_field.py "$ROOT/model-class-gemma.out" 'next: V010.MAP.8'

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/model-class-gemma-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-gemma-table.out"
matches "$ROOT/model-class-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}metadata-profiled[[:space:]]{2,}12[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/model-class-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_profile_status: metadata-profiled'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_family: gemma'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_name: gemma-source-model-class-profile'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_runtime_shape: dense-causal-decoder-candidate-pending-config'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_config_status: present'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_source_metadata_status: header-only'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_embedding_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_attention_q_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_attention_k_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_attention_v_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_attention_o_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_mlp_gate_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_mlp_up_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_mlp_down_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_norm_pattern_count: 3'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_output_head_pattern_count: 1'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_moe_router_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_moe_expert_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_other_pattern_count: 0'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_pattern_status: lexical-only'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'model_class_runtime_status: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'backend_selection: deferred'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'backend_pressure: cpu-cuda-baseline-planned'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-audit.out" 'next_required_rows: V010.MAP.8'

GEMMA_CLASS_MODELS_ROOT="$ROOT/gemma-class-models-root"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$GEMMA_CLASS_MODELS_ROOT" --auth auto --progress off > "$ROOT/gemma-class-acquire.out"
cp -R "$GEMMA_CLASS_SOURCE/." "$GEMMA_CLASS_MODELS_ROOT/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08/"
"$YVEX_BIN" inspect target class-profile gemma-4-12b-it --models-root "$GEMMA_CLASS_MODELS_ROOT" --audit > "$ROOT/model-class-gemma-models-root-audit.out"
python3 tests/support/human_field.py "$ROOT/model-class-gemma-models-root-audit.out" 'model_class_profile_status: metadata-profiled'
matches "$ROOT/model-class-gemma-models-root-audit.out" 'source_path: .*/gemma-class-models-root/source/hf/google/gemma-4-12B-it/b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08$'
python3 tests/support/human_field.py "$ROOT/model-class-gemma-models-root-audit.out" 'model_class_source_metadata_status: header-only'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" > "$ROOT/tensor-collection-gemma.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'tensor-collection: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'target: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'status: collection-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'evidence: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'collections: embedding=1 attention_qkvo=1 mlp_gud=1 norm=3 head=1 moe=0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'layers_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'top_blocker: missing-gemma-tensor-role-map'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma.out" 'boundary: tensor collection inventory only; no role mapping/runtime/generation'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/tensor-collection-gemma-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-gemma-table.out"
matches "$ROOT/tensor-collection-gemma-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN_QKVO[[:space:]]{2,}MLP_GUD[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-collection-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}collection-profiled[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010\.MAP\.8$'

"$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/tensor-collection-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_status: collection-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_stage: header-collection-inventory'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_source_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_manifest_status: not-checked'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_config_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_layer_count_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_embedding_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_embedding_tensor_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_q_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_k_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_v_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_o_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_attention_complete_qkvo_layer_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_mlp_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_mlp_gate_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_mlp_up_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_mlp_down_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_mlp_complete_gud_layer_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_norm_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_norm_tensor_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_output_head_status: candidate'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_output_head_tensor_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_moe_status: not-observed'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_moe_router_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_moe_expert_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_tokenizer_collection_status: sidecar-observed'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_validation_status: lexical-and-header-only'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_role_mapping_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'tensor_collection_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-audit.out" 'next_required_rows: V010.MAP.8'
! grep 'generation_ready: tr''ue' "$ROOT/tensor-collection-gemma-audit.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" > "$ROOT/tensor-map-gemma.out"
grep 'tensor-map: gemma-4-12b-it \[reported\]' "$ROOT/tensor-map-gemma.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'family: gemma  stage: header-naming-map  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'roles: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'layers: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'top_blocker: missing-dense-runtime-role-validation'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma.out" 'boundary: report-only; use --audit for tensor entries'
! grep 'tensor_map.entry.' "$ROOT/tensor-map-gemma.out"
! grep 'runtime_claim:' "$ROOT/tensor-map-gemma.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/tensor-map-gemma-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-gemma-table.out"
matches "$ROOT/tensor-map-gemma-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}TOTAL[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN[[:space:]]{2,}MLP[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}UNKNOWN[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-map-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}naming-map-profiled[[:space:]]{2,}12[[:space:]]{2,}1[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/tensor-map-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_status: naming-map-profiled'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_stage: header-naming-map'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_evidence_basis: header-metadata-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_source_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_config_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_tokenizer_status: present'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_tensor_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_mapped_total_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_unmapped_unknown_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_ambiguous_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_layer_count_observed: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_embedding_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_attention_count: 4'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_attention_q_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_attention_k_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_attention_v_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_attention_o_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_mlp_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_mlp_gate_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_mlp_up_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_mlp_down_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_norm_count: 3'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_output_head_count: 1'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_moe_router_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_moe_expert_count: 0'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_validation_status: lexical-and-header-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_canonical_role_status: mapped-candidates'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_runtime_role_coverage_status: report-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_artifact_contract_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_runtime_descriptor_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'tensor_map_graph_consumer_status: not-implemented'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-audit.out" 'next_required_rows: V010.MAP.8'
grep 'tensor_map.entry.' "$ROOT/tensor-map-gemma-audit.out"
grep 'model.embed_tokens.weight -> model.embedding.token.weight' "$ROOT/tensor-map-gemma-audit.out"
grep 'model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight' "$ROOT/tensor-map-gemma-audit.out"
grep 'model.layers.0.input_layernorm.weight -> model.layers.0.attention.norm.weight' "$ROOT/tensor-map-gemma-audit.out"
grep 'lm_head.weight -> model.output_head.weight' "$ROOT/tensor-map-gemma-audit.out"
! grep 'generation_ready: tr''ue' "$ROOT/tensor-map-gemma-audit.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --check-output-contract normal > "$ROOT/output-contract-gemma-tensor-map-normal.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --check-output-contract table > "$ROOT/output-contract-gemma-tensor-map-table.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --check-output-contract audit > "$ROOT/output-contract-gemma-tensor-map-audit.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" > "$ROOT/output-head-gemma.out"
grep 'output-head-map: gemma-4-12b-it \[reported\]' "$ROOT/output-head-gemma.out"
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'family: gemma  evidence: header-only'
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'head: model.output_head.weight  final_norm: model.final_norm.weight  embedding: model.embedding.token.weight  tie: separate-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'shape: compatible-same-shape'
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'top_blocker: missing-output-head-runtime-consumer'
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'next: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/output-head-gemma.out" 'boundary: mapping only; no logits/runtime/generation'
! grep 'output_head_map_' "$ROOT/output-head-gemma.out"
! grep 'native_output_head:' "$ROOT/output-head-gemma.out"
! grep 'runtime_claim:' "$ROOT/output-head-gemma.out"

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/output-head-gemma-table.out"
grep 'OUTPUT HEAD TENSOR MAP' "$ROOT/output-head-gemma-table.out"
matches "$ROOT/output-head-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}output-head-profiled[[:space:]]{2,}yes[[:space:]]{2,}yes[[:space:]]{2,}yes[[:space:]]{2,}separate-output-head-candidate[[:space:]]{2,}compatible-same-shape[[:space:]]{2,}V010.MAP.8$'

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/output-head-gemma-audit.out"
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_map_status: output-head-profiled'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_map_family: gemma'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_map_target_id: gemma-4-12b-it'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_native_name: lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_canonical_role: model.output_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_mapping_status: mapped-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_candidate_count: 1'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'embedding_canonical_role: model.embedding.token.weight'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'final_norm_canonical_role: model.final_norm.weight'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'tie_policy_status: separate-output-head-candidate'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'shape_relation_status: compatible-same-shape'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_runtime_consumer_status: target-runtime-owned'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head_logits_status: target-capability-dependent'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'next_required_rows: V010.MAP.8'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head.entry.output.native_name: lm_head.weight'
python3 tests/support/human_field.py "$ROOT/output-head-gemma-audit.out" 'output_head.entry.output.canonical_role: model.output_head.weight'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" --check-output-contract normal > "$ROOT/output-contract-gemma-output-head-normal.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" --check-output-contract table > "$ROOT/output-contract-gemma-output-head-table.out"
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role output-head --source "$GEMMA_CLASS_SOURCE" --check-output-contract audit > "$ROOT/output-contract-gemma-output-head-audit.out"

GEMMA_TENSOR_MAP_UNKNOWN_SOURCE="${TMPDIR:-/tmp}/yvex-gemma-tensor-map-unknown-test-$$"
yvex_test_cleanup "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE"
mkdir -p "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE"
printf '{}\n' > "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE/config.json"
printf '{}\n' > "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE/tokenizer.json"
python3 - "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
    "model.layers.0.weird_unknown.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE" > "$ROOT/tensor-map-gemma-unknown.out"
grep 'tensor-map: gemma-4-12b-it \[naming-map-candidate\]' "$ROOT/tensor-map-gemma-unknown.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown.out" 'roles: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=1'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE" --audit > "$ROOT/tensor-map-gemma-unknown-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown-audit.out" 'tensor_map_status: naming-map-candidate'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown-audit.out" 'tensor_map_tensor_count: 13'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown-audit.out" 'tensor_map_mapped_total_count: 12'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown-audit.out" 'tensor_map_unmapped_unknown_count: 1'
grep 'model.layers.0.weird_unknown.weight' "$ROOT/tensor-map-gemma-unknown-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-unknown-audit.out" 'mapping_status: unmapped-unknown'
yvex_test_cleanup "$GEMMA_TENSOR_MAP_UNKNOWN_SOURCE"

GEMMA_TENSOR_MAP_NORM_SOURCE="${TMPDIR:-/tmp}/yvex-gemma-tensor-map-norm-test-$$"
yvex_test_cleanup "$GEMMA_TENSOR_MAP_NORM_SOURCE"
mkdir -p "$GEMMA_TENSOR_MAP_NORM_SOURCE"
python3 - "$GEMMA_TENSOR_MAP_NORM_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.layers.0.pre_feedforward_layernorm.weight",
    "model.layers.0.post_feedforward_layernorm.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_TENSOR_MAP_NORM_SOURCE" > "$ROOT/tensor-map-gemma-norm.out"
grep 'tensor-map: gemma-4-12b-it \[blocked\]' "$ROOT/tensor-map-gemma-norm.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm.out" 'roles: total=2 embedding=0 attention=0 mlp=0 norm=2 head=0 moe=0 unknown=0'
"$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source "$GEMMA_TENSOR_MAP_NORM_SOURCE" --audit > "$ROOT/tensor-map-gemma-norm-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm-audit.out" 'tensor_map_norm_count: 2'
grep 'model.layers.0.pre_feedforward_layernorm.weight -> model.layers.0.mlp.norm.weight' "$ROOT/tensor-map-gemma-norm-audit.out"
grep 'model.layers.0.post_feedforward_layernorm.weight -> model.layers.0.mlp.norm.weight' "$ROOT/tensor-map-gemma-norm-audit.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm-audit.out" 'tensor_map_runtime_role_coverage_status: report-only'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm-audit.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm-audit.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-norm-audit.out" 'release_ready: false'
yvex_test_cleanup "$GEMMA_TENSOR_MAP_NORM_SOURCE"

! grep "$BAD_RUNTIME_CLAIM" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_GENERATION_READY" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_BENCHMARK_MEASURED" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_RELEASE_READY" "$ROOT/model-class-gemma-audit.out"
yvex_test_cleanup "$GEMMA_CLASS_SOURCE"

expect_rc 2 "$YVEX_BIN" inspect target class-profile > "$ROOT/model-class-missing-target.out" 2> "$ROOT/model-class-missing-target.err"
grep 'requires TARGET' "$ROOT/model-class-missing-target.err"
expect_rc 2 "$YVEX_BIN" inspect target class-profile nope > "$ROOT/model-class-bad-target.out" 2> "$ROOT/model-class-bad-target.err"
python3 tests/support/human_field.py "$ROOT/model-class-bad-target.err" 'unsupported target: nope'
expect_rc 2 "$YVEX_BIN" inspect target class-profile deepseek4-v4-flash > "$ROOT/model-class-retired-deepseek.out" 2> "$ROOT/model-class-retired-deepseek.err"
python3 tests/support/human_field.py "$ROOT/model-class-retired-deepseek.err" 'unsupported target: deepseek4-v4-flash; use deepseek4-v4-flash-dspark'
expect_rc 2 "$YVEX_BIN" inspect target class-profile qwen-metal-portability > "$ROOT/model-class-old-target.out" 2> "$ROOT/model-class-old-target.err"
python3 tests/support/human_field.py "$ROOT/model-class-old-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target class-profile gemma-dense-portability > "$ROOT/model-class-old-gemma-target.out" 2> "$ROOT/model-class-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/model-class-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target class-profile qwen3-8b --output nope > "$ROOT/model-class-bad-output.out" 2> "$ROOT/model-class-bad-output.err"
python3 tests/support/human_field.py "$ROOT/model-class-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target class-profile qwen3-8b --source > "$ROOT/model-class-missing-source.out" 2> "$ROOT/model-class-missing-source.err"
grep -- '--source requires a value' "$ROOT/model-class-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target class-profile gemma-4-12b-it --output nope > "$ROOT/model-class-gemma-bad-output.out" 2> "$ROOT/model-class-gemma-bad-output.err"
python3 tests/support/human_field.py "$ROOT/model-class-gemma-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target class-profile gemma-4-12b-it --source > "$ROOT/model-class-gemma-missing-source.out" 2> "$ROOT/model-class-gemma-missing-source.err"
grep -- '--source requires a value' "$ROOT/model-class-gemma-missing-source.err"

expect_rc 2 "$YVEX_BIN" inspect target tensor-collection > "$ROOT/tensor-collection-missing-target.out" 2> "$ROOT/tensor-collection-missing-target.err"
grep 'requires TARGET' "$ROOT/tensor-collection-missing-target.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection nope > "$ROOT/tensor-collection-bad-target.out" 2> "$ROOT/tensor-collection-bad-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-collection-bad-target.err" 'unsupported target: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection qwen-metal-portability > "$ROOT/tensor-collection-old-target.out" 2> "$ROOT/tensor-collection-old-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-collection-old-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection qwen3-8b --output nope > "$ROOT/tensor-collection-bad-output.out" 2> "$ROOT/tensor-collection-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tensor-collection-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection qwen3-8b --source > "$ROOT/tensor-collection-missing-source.out" 2> "$ROOT/tensor-collection-missing-source.err"
grep -- '--source requires a value' "$ROOT/tensor-collection-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection qwen3-8b --models-root > "$ROOT/tensor-collection-missing-models-root.out" 2> "$ROOT/tensor-collection-missing-models-root.err"
grep -- '--models-root requires a value' "$ROOT/tensor-collection-missing-models-root.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection gemma-dense-portability > "$ROOT/tensor-collection-old-gemma-target.out" 2> "$ROOT/tensor-collection-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-collection-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --output nope > "$ROOT/tensor-collection-gemma-bad-output.out" 2> "$ROOT/tensor-collection-gemma-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tensor-collection-gemma-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-collection gemma-4-12b-it --source > "$ROOT/tensor-collection-gemma-missing-source.out" 2> "$ROOT/tensor-collection-gemma-missing-source.err"
grep -- '--source requires a value' "$ROOT/tensor-collection-gemma-missing-source.err"

expect_rc 2 "$YVEX_BIN" inspect target tensor-map > "$ROOT/tensor-map-missing-target.out" 2> "$ROOT/tensor-map-missing-target.err"
grep 'requires TARGET' "$ROOT/tensor-map-missing-target.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map nope > "$ROOT/tensor-map-bad-target.out" 2> "$ROOT/tensor-map-bad-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-map-bad-target.err" 'unsupported target: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen-metal-portability > "$ROOT/tensor-map-old-target.out" 2> "$ROOT/tensor-map-old-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-map-old-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --output nope > "$ROOT/tensor-map-bad-output.out" 2> "$ROOT/tensor-map-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tensor-map-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map nope --check-output-contract normal > "$ROOT/tensor-map-contract-unknown-target.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-contract-unknown-target.out" 'status: unsupported-target'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --check-output-contract nope > "$ROOT/tensor-map-contract-bad-mode.out"
python3 tests/support/human_field.py "$ROOT/tensor-map-contract-bad-mode.out" 'status: unsupported-mode'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --check-output-contract \
  > "$ROOT/tensor-map-contract-missing-mode.out" \
  2> "$ROOT/tensor-map-contract-missing-mode.err"
grep -- '--check-output-contract requires a value' \
  "$ROOT/tensor-map-contract-missing-mode.err"
for output_contract_report in "$ROOT"/output-contract-*.out; do
  test -f "$output_contract_report"
  assert_output_contract_pass "$output_contract_report"
done
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --source > "$ROOT/tensor-map-missing-source.out" 2> "$ROOT/tensor-map-missing-source.err"
grep -- '--source requires a value' "$ROOT/tensor-map-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --models-root > "$ROOT/tensor-map-missing-models-root.out" 2> "$ROOT/tensor-map-missing-models-root.err"
grep -- '--models-root requires a value' "$ROOT/tensor-map-missing-models-root.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --output nope > "$ROOT/tensor-map-gemma-bad-output.out" 2> "$ROOT/tensor-map-gemma-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tensor-map-gemma-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --source > "$ROOT/tensor-map-gemma-missing-source.out" 2> "$ROOT/tensor-map-gemma-missing-source.err"
grep -- '--source requires a value' "$ROOT/tensor-map-gemma-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-dense-portability > "$ROOT/tensor-map-old-gemma-target.out" 2> "$ROOT/tensor-map-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-map-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role > "$ROOT/output-head-missing-role.out" 2> "$ROOT/output-head-missing-role.err"
grep -- '--role requires a value' "$ROOT/output-head-missing-role.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role nope > "$ROOT/output-head-qwen-bad-role.out" 2> "$ROOT/output-head-qwen-bad-role.err"
python3 tests/support/human_field.py "$ROOT/output-head-qwen-bad-role.err" 'unsupported role: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-4-12b-it --role nope > "$ROOT/output-head-gemma-bad-role.out" 2> "$ROOT/output-head-gemma-bad-role.err"
python3 tests/support/human_field.py "$ROOT/output-head-gemma-bad-role.err" 'unsupported role: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen-metal-portability --role output-head > "$ROOT/output-head-old-qwen-target.out" 2> "$ROOT/output-head-old-qwen-target.err"
python3 tests/support/human_field.py "$ROOT/output-head-old-qwen-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-dense-portability --role output-head > "$ROOT/output-head-old-gemma-target.out" 2> "$ROOT/output-head-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/output-head-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --output nope > "$ROOT/output-head-bad-output.out" 2> "$ROOT/output-head-bad-output.err"
python3 tests/support/human_field.py "$ROOT/output-head-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role output-head --source > "$ROOT/output-head-missing-source.out" 2> "$ROOT/output-head-missing-source.err"
grep -- '--source requires a value' "$ROOT/output-head-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen-metal-portability --role tokenizer > "$ROOT/tokenizer-old-qwen-target.out" 2> "$ROOT/tokenizer-old-qwen-target.err"
python3 tests/support/human_field.py "$ROOT/tokenizer-old-qwen-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-dense-portability --role tokenizer > "$ROOT/tokenizer-old-gemma-target.out" 2> "$ROOT/tokenizer-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/tokenizer-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --output nope > "$ROOT/tokenizer-bad-output.out" 2> "$ROOT/tokenizer-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tokenizer-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role tokenizer --source > "$ROOT/tokenizer-missing-source.out" 2> "$ROOT/tokenizer-missing-source.err"
grep -- '--source requires a value' "$ROOT/tokenizer-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --output nope > "$ROOT/missing-role-bad-output.out" 2> "$ROOT/missing-role-bad-output.err"
python3 tests/support/human_field.py "$ROOT/missing-role-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --source > "$ROOT/missing-role-missing-source.out" 2> "$ROOT/missing-role-missing-source.err"
grep -- '--source requires a value' "$ROOT/missing-role-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen-metal-portability --role missing-roles > "$ROOT/missing-role-old-qwen-target.out" 2> "$ROOT/missing-role-old-qwen-target.err"
python3 tests/support/human_field.py "$ROOT/missing-role-old-qwen-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map gemma-dense-portability --role missing-roles > "$ROOT/missing-role-old-gemma-target.out" 2> "$ROOT/missing-role-old-gemma-target.err"
python3 tests/support/human_field.py "$ROOT/missing-role-old-gemma-target.err" 'unsupported target: gemma-dense-portability'
expect_rc 2 "$YVEX_BIN" inspect target missing-roles > "$ROOT/missing-roles-direct-missing-target.out" 2> "$ROOT/missing-roles-direct-missing-target.err"
grep 'requires TARGET' "$ROOT/missing-roles-direct-missing-target.err"
expect_rc 2 "$YVEX_BIN" inspect target missing-roles qwen3-8b --output nope > "$ROOT/missing-roles-direct-bad-output.out" 2> "$ROOT/missing-roles-direct-bad-output.err"
python3 tests/support/human_field.py "$ROOT/missing-roles-direct-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target missing-roles qwen3-8b --models-root > "$ROOT/missing-roles-direct-missing-root.out" 2> "$ROOT/missing-roles-direct-missing-root.err"
grep -- '--models-root requires a value' "$ROOT/missing-roles-direct-missing-root.err"
expect_rc 2 "$YVEX_BIN" inspect target missing-roles qwen3-8b --source > "$ROOT/missing-roles-direct-missing-source.out" 2> "$ROOT/missing-roles-direct-missing-source.err"
grep -- '--source requires a value' "$ROOT/missing-roles-direct-missing-source.err"
expect_rc 2 "$YVEX_BIN" inspect target missing-roles qwen-metal-portability > "$ROOT/missing-roles-direct-old-qwen-target.out" 2> "$ROOT/missing-roles-direct-old-qwen-target.err"
python3 tests/support/human_field.py "$ROOT/missing-roles-direct-old-qwen-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --gate > "$ROOT/tensor-mapping-gate-missing-value.out" 2> "$ROOT/tensor-mapping-gate-missing-value.err"
grep -- '--gate requires a value' "$ROOT/tensor-mapping-gate-missing-value.err"
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v9.9.9 > "$ROOT/tensor-mapping-gate-bad-release.out" 2> "$ROOT/tensor-mapping-gate-bad-release.err"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-bad-release.err" 'unsupported release: v9.9.9'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --gate v0.1.0 --output nope > "$ROOT/tensor-mapping-gate-bad-output.out" 2> "$ROOT/tensor-mapping-gate-bad-output.err"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-bad-output.err" 'unsupported output mode: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen-metal-portability --gate v0.1.0 > "$ROOT/tensor-mapping-gate-old-qwen-target.out" 2> "$ROOT/tensor-mapping-gate-old-qwen-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-old-qwen-target.err" 'unsupported target: qwen-metal-portability'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map nope --gate v0.1.0 > "$ROOT/tensor-mapping-gate-unknown-target.out" 2> "$ROOT/tensor-mapping-gate-unknown-target.err"
python3 tests/support/human_field.py "$ROOT/tensor-mapping-gate-unknown-target.err" 'unsupported target: nope'
expect_rc 2 "$YVEX_BIN" inspect target tensor-map qwen3-8b --role missing-roles --gate v0.1.0 > "$ROOT/tensor-mapping-gate-role-conflict.out" 2> "$ROOT/tensor-mapping-gate-role-conflict.err"
grep 'gate cannot be combined with --role' "$ROOT/tensor-mapping-gate-role-conflict.err"

"$YVEX_BIN" inspect target decision --help > "$ROOT/model-target-decision-help.out"
grep -F 'usage: yvex inspect target [action] [target] [options]' \
  "$ROOT/model-target-decision-help.out"
grep -- '--release' "$ROOT/model-target-decision-help.out"

"$YVEX_BIN" inspect target decision --release v0.1.0 > "$ROOT/model-target-decision-normal.out"
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'report: target-decision'
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'status: target-selected-mapping-specified'
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'selected: deepseek4-v4-flash-dspark'
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'top_blocker: source payload trust'
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'next: V010.SOURCE.PAYLOAD.STREAM.0'
python3 tests/support/human_field.py "$ROOT/model-target-decision-normal.out" 'boundary: release target selected; artifact/runtime/generation unsupported; benchmark not measured'
! grep 'deepseek4-v4-flash-dspark-selected' "$ROOT/model-target-decision-normal.out"

"$YVEX_BIN" inspect target decision --release v0.1.0 --output table > "$ROOT/model-target-decision-table.out"
matches "$ROOT/model-target-decision-table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$ROOT/model-target-decision-table.out" '^target-decision[[:space:]]{2,}selected-mapping-specified[[:space:]]{2,}deepseek4-v4-flash-dspark[[:space:]]{2,}0[[:space:]]{2,}V010\.SOURCE\.PAYLOAD\.STREAM\.0$'

"$YVEX_BIN" inspect target decision --release v0.1.0 --output json > "$ROOT/model-target-decision-json.out"
jq -e '.selected_target_id == "deepseek4-v4-flash-dspark" and .upstream_repository == "deepseek-ai/DeepSeek-V4-Flash-DSpark" and .source_verification == "complete" and .architecture_ir == "complete" and .tensor_coverage == "complete" and .gguf_mapping == "complete" and .artifact_status == "not-produced" and .runtime == "unsupported" and .generation == "unsupported" and .next == "V010.SOURCE.PAYLOAD.STREAM.0"' "$ROOT/model-target-decision-json.out" >/dev/null

"$YVEX_BIN" inspect target decision --release v0.1.0 --output nope > "$ROOT/model-target-decision-bad-output.out" 2> "$ROOT/model-target-decision-bad-output.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/model-target-decision-bad-output.err" 'model-target decision: unsupported output mode: nope'

"$YVEX_BIN" inspect target decision --release v0.1.0 --audit --include-candidates --include-pressure-targets --include-blockers --include-critical-path --include-next > "$ROOT/model-target-decision.out"
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'target_decision: v0.1.0'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'status: target-selected-mapping-specified'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'decision_state: selected'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'selected_target_id: deepseek4-v4-flash-dspark'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'upstream_repository: deepseek-ai/DeepSeek-V4-Flash-DSpark'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'source_verification_status: complete'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'architecture_ir_status: complete'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'tensor_coverage_status: complete'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'gguf_mapping_status: complete'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'full_runtime_candidate_status: unsupported'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'selected_runtime_slice_eligible: false'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'source_only_eligible: false'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'external_reference_eligible: false'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'release_ready: false'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'candidate.0.id: deepseek4-v4-flash-dspark'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'candidate.0.class: release-source-target'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'candidate.0.status: selected-mapping-specified'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'qwen_engineering_scope: preserved-non-release'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'gemma_engineering_scope: preserved-non-release'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'selected_slice_scope: bounded-evidence-only'
python3 tests/support/human_field.py "$ROOT/model-target-decision.out" 'next_required_rows: V010.SOURCE.PAYLOAD.STREAM.0'
! grep 'candidate.*deepseek4-v4-flash-dspark-selected' "$ROOT/model-target-decision.out"

"$YVEX_BIN" inspect target decision --release v0.1.0 --audit --candidate deepseek4-v4-flash-dspark --include-blockers --include-next > "$ROOT/model-target-decision-deepseek.out"
python3 tests/support/human_field.py "$ROOT/model-target-decision-deepseek.out" 'candidate_count: 1'
python3 tests/support/human_field.py "$ROOT/model-target-decision-deepseek.out" 'candidate.0.id: deepseek4-v4-flash-dspark'
python3 tests/support/human_field.py "$ROOT/model-target-decision-deepseek.out" 'candidate.0.status: selected-mapping-specified'
python3 tests/support/human_field.py "$ROOT/model-target-decision-deepseek.out" 'generation: unsupported-full-model'

expect_rc 2 "$YVEX_BIN" inspect target decision --release v0.1.0 --audit --candidate deepseek4-v4-flash-dspark-selected-embed-rmsnorm --include-blockers --include-next > "$ROOT/model-target-decision-rmsnorm.out" 2> "$ROOT/model-target-decision-rmsnorm.err"
python3 tests/support/human_field.py "$ROOT/model-target-decision-rmsnorm.out" 'status: missing-candidate'

expect_rc 2 "$YVEX_BIN" inspect target decision --release v0.1.0 --audit --candidate glm-5.2-official-safetensors --include-blockers --include-next > "$ROOT/model-target-decision-glm.out" 2> "$ROOT/model-target-decision-glm.err"
python3 tests/support/human_field.py "$ROOT/model-target-decision-glm.out" 'status: missing-candidate'

"$YVEX_BIN" inspect target decision --release v0.1.0 --candidate missing-target --include-blockers > "$ROOT/model-target-decision-missing.out" 2> "$ROOT/model-target-decision-missing.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/model-target-decision-missing.out" 'status: missing-candidate'
python3 tests/support/human_field.py "$ROOT/model-target-decision-missing.out" 'candidate_requested: missing-target'
python3 tests/support/human_field.py "$ROOT/model-target-decision-missing.out" 'runtime_claim: unsupported'

"$YVEX_BIN" inspect target decision --release v9.9.9 > "$ROOT/model-target-decision-unsupported-release.out" 2> "$ROOT/model-target-decision-unsupported-release.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'target_decision: v9.9.9'
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'status: unsupported-release'
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'runtime_claim: unsupported'
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'generation: unsupported-full-model'
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'benchmark_status: not-measured'
python3 tests/support/human_field.py "$ROOT/model-target-decision-unsupported-release.out" 'release_ready: false'
