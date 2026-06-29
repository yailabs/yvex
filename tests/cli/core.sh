#!/bin/sh
#
# YVEX - CLI smoke tests
#
# File: tests/cli/core.sh
# Layer: test
#
# Purpose:
#   Proves that the CLI command table exposes only implemented commands and
#   returns stable exit codes for common bootstrap, filesystem, artifact, GGUF
#   directory, tensor table, descriptor, tokenizer, prompt, graph, plan,
#   backend, engine, session, run, and chat behavior.
#
# Covers:
#   - yvex
#   - yvex --help
#   - yvex -h
#   - yvex --version
#   - yvex version
#   - yvex info
#   - yvex convert
#   - yvex qtype-support
#   - yvex inspect
#   - yvex input
#   - yvex kv
#   - yvex materialize
#   - yvex materialize-gate
#   - yvex metadata
#   - yvex model-gate
#   - yvex model-target
#   - yvex models
#   - yvex native-weights
#   - yvex paths
#   - yvex prompt
#   - yvex graph
#   - yvex gguf-template
#   - yvex gguf-emit
#   - yvex tensor-map
#   - yvex quant-job
#   - yvex quant-policy
#   - yvex imatrix
#   - yvex plan
#   - yvex tokenize
#   - yvex tokenizer
#   - yvex tensors
#   - yvex detokenize
#   - yvex engine
#   - yvex session
#   - yvex source-manifest
#   - yvex run
#   - yvex chat
#   - yvex commands
#   - yvex help info
#   - yvex unknown
#
# Commands:
#   - make test-cli
#   - make smoke
#
# Expected:
#   - exits 0 on success
#   - prints concise failure to stderr

set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli}

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

run_ok() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" || fail "$name exited non-zero"
}

contains() {
    file=$1
    text=$2
    grep -F -- "$text" "$file" >/dev/null || fail "$file missing: $text"
}

run_fail() {
    name=$1
    shift
    set +e
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err"
    rc=$?
    set -e
    if [ "$rc" -eq 0 ]; then
        fail "$name unexpectedly succeeded"
    fi
}

run_ok no_args "$YVEX_BIN"
contains "$OUT_DIR/no_args.out" "usage: yvex <command> [options]"

run_ok help_long "$YVEX_BIN" --help
contains "$OUT_DIR/help_long.out" "Implemented commands:"

run_ok help_short "$YVEX_BIN" -h
contains "$OUT_DIR/help_short.out" "Implemented commands:"

run_ok version_option "$YVEX_BIN" --version
contains "$OUT_DIR/version_option.out" "yvex 0.1.0"

run_ok version_command "$YVEX_BIN" version
contains "$OUT_DIR/version_command.out" "yvex 0.1.0"

run_ok info "$YVEX_BIN" info
contains "$OUT_DIR/info.out" "name: YVEX"
contains "$OUT_DIR/info.out" "status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token input boundary, prefill state foundation, minimal KV binding, and minimal KV ownership"
contains "$OUT_DIR/info.out" "library: libyvex.a"
contains "$OUT_DIR/info.out" "filesystem: implemented"
contains "$OUT_DIR/info.out" "artifact: open/read implemented"
contains "$OUT_DIR/info.out" "gguf: metadata/tensor directory parsing implemented"
contains "$OUT_DIR/info.out" "model: descriptor-only implemented"
contains "$OUT_DIR/info.out" "tokenizer: fixture encode/decode implemented"
contains "$OUT_DIR/info.out" "token_input: explicit token boundary implemented"
contains "$OUT_DIR/info.out" "prefill_state: segment-summary foundation and minimal KV binding implemented"
contains "$OUT_DIR/info.out" "prompt: default renderer implemented"
contains "$OUT_DIR/info.out" "graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward primitive implemented"
contains "$OUT_DIR/info.out" "planner: estimate-only implemented"
contains "$OUT_DIR/info.out" "backend: CPU reference implemented"
contains "$OUT_DIR/info.out" "backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is available"
contains "$OUT_DIR/info.out" "weights: selected tensor materialization implemented"
contains "$OUT_DIR/info.out" "engine: descriptor open and selected-weight attachment implemented"
contains "$OUT_DIR/info.out" "session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented"
contains "$OUT_DIR/info.out" "run: accepted-only runtime shell implemented"
contains "$OUT_DIR/info.out" "chat: accepted-only REPL shell implemented"
contains "$OUT_DIR/info.out" "metrics: runtime collector implemented"
contains "$OUT_DIR/info.out" "trace: JSONL writer implemented"
contains "$OUT_DIR/info.out" "profile: JSON writer implemented"
contains "$OUT_DIR/info.out" "run_artifacts: metrics/trace/profile files implemented"
contains "$OUT_DIR/info.out" "source_manifest: provenance JSON writer implemented"
contains "$OUT_DIR/info.out" "native_weights: safetensors header inventory implemented"
contains "$OUT_DIR/info.out" "gguf_template: contract validator implemented"
contains "$OUT_DIR/info.out" "gguf_emit: controlled GGUF writer implemented"
contains "$OUT_DIR/info.out" "conversion: open-weight selected tensor bridge implemented"
contains "$OUT_DIR/info.out" "model_ref: alias-or-path resolver implemented"
contains "$OUT_DIR/info.out" "model_registry: local model alias registry implemented"
contains "$OUT_DIR/info.out" "quant_job: external quantization job manifest implemented"
contains "$OUT_DIR/info.out" "qtype_support: conversion support matrix implemented"
contains "$OUT_DIR/info.out" "weight_mapping: tensor adapter contract implemented"
contains "$OUT_DIR/info.out" "quant_policy: manifest validator implemented"
contains "$OUT_DIR/info.out" "imatrix: calibration artifact manifest implemented"
contains "$OUT_DIR/info.out" "server_binary: yvexd shell implemented"
contains "$OUT_DIR/info.out" "server_endpoints: health/metrics/models status implemented"
contains "$OUT_DIR/info.out" "server_generation: not implemented"
contains "$OUT_DIR/info.out" "kv: minimal session-owned append/read boundary implemented"
contains "$OUT_DIR/info.out" "logits: unavailable skeleton implemented"
contains "$OUT_DIR/info.out" "generation: unsupported"
contains "$OUT_DIR/info.out" "cuda: available when local driver/device probe succeeds"
contains "$OUT_DIR/info.out" "server: yvexd status shell implemented"

run_ok commands "$YVEX_BIN" commands
contains "$OUT_DIR/commands.out" "Implemented commands:"
contains "$OUT_DIR/commands.out" "  backend"
contains "$OUT_DIR/commands.out" "  chat"
contains "$OUT_DIR/commands.out" "  commands"
contains "$OUT_DIR/commands.out" "  convert"
contains "$OUT_DIR/commands.out" "  cuda-info"
contains "$OUT_DIR/commands.out" "  detokenize"
contains "$OUT_DIR/commands.out" "  engine"
contains "$OUT_DIR/commands.out" "  graph"
contains "$OUT_DIR/commands.out" "  gguf-emit"
contains "$OUT_DIR/commands.out" "  gguf-template"
contains "$OUT_DIR/commands.out" "  help"
contains "$OUT_DIR/commands.out" "  imatrix"
contains "$OUT_DIR/commands.out" "  info"
contains "$OUT_DIR/commands.out" "  inspect"
contains "$OUT_DIR/commands.out" "  input"
contains "$OUT_DIR/commands.out" "  kv"
contains "$OUT_DIR/commands.out" "  materialize"
contains "$OUT_DIR/commands.out" "  materialize-gate"
contains "$OUT_DIR/commands.out" "  metadata"
contains "$OUT_DIR/commands.out" "  model-gate"
contains "$OUT_DIR/commands.out" "  model-target"
contains "$OUT_DIR/commands.out" "  models"
contains "$OUT_DIR/commands.out" "  native-weights"
contains "$OUT_DIR/commands.out" "  paths"
contains "$OUT_DIR/commands.out" "  plan"
contains "$OUT_DIR/commands.out" "  prompt"
contains "$OUT_DIR/commands.out" "  quant-job"
contains "$OUT_DIR/commands.out" "  quant-policy"
contains "$OUT_DIR/commands.out" "  qtype-support"
contains "$OUT_DIR/commands.out" "  run"
contains "$OUT_DIR/commands.out" "  session"
contains "$OUT_DIR/commands.out" "  source-manifest"
contains "$OUT_DIR/commands.out" "  tensor-map"
contains "$OUT_DIR/commands.out" "  tokenize"
contains "$OUT_DIR/commands.out" "  tokenizer"
contains "$OUT_DIR/commands.out" "  tensors"
contains "$OUT_DIR/commands.out" "  version"

run_ok help_info "$YVEX_BIN" help info
contains "$OUT_DIR/help_info.out" "usage: yvex info"

run_ok help_backend "$YVEX_BIN" help backend
contains "$OUT_DIR/help_backend.out" "usage: yvex backend cpu|cuda"

run_ok help_convert "$YVEX_BIN" help convert
contains "$OUT_DIR/help_convert.out" "usage: yvex convert"

run_ok help_qtype_support "$YVEX_BIN" help qtype-support
contains "$OUT_DIR/help_qtype_support.out" "usage: yvex qtype-support"

run_ok help_cuda_info "$YVEX_BIN" help cuda-info
contains "$OUT_DIR/help_cuda_info.out" "usage: yvex cuda-info"

run_ok help_chat "$YVEX_BIN" help chat
contains "$OUT_DIR/help_chat.out" "usage: yvex chat [--model FILE_OR_ALIAS]"

run_ok help_inspect "$YVEX_BIN" help inspect
contains "$OUT_DIR/help_inspect.out" "usage: yvex inspect FILE_OR_ALIAS"

run_ok help_input "$YVEX_BIN" help input
contains "$OUT_DIR/help_input.out" "usage: yvex input tokens"

run_ok help_kv "$YVEX_BIN" help kv
contains "$OUT_DIR/help_kv.out" "usage: yvex kv --layers N --heads N --head-dim N --capacity N"

run_ok help_materialize "$YVEX_BIN" help materialize
contains "$OUT_DIR/help_materialize.out" "usage: yvex materialize --model FILE_OR_ALIAS"

run_ok help_materialize_gate "$YVEX_BIN" help materialize-gate
contains "$OUT_DIR/help_materialize_gate.out" "usage: yvex materialize-gate check"

run_ok help_metadata "$YVEX_BIN" help metadata
contains "$OUT_DIR/help_metadata.out" "usage: yvex metadata FILE_OR_ALIAS"

run_ok help_model_gate "$YVEX_BIN" help model-gate
contains "$OUT_DIR/help_model_gate.out" "usage: yvex model-gate check"

run_ok help_model_target "$YVEX_BIN" help model-target
contains "$OUT_DIR/help_model_target.out" "usage: yvex model-target classes"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target list"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target inspect TARGET [--paths] [--models-root DIR]"
contains "$OUT_DIR/help_model_target.out" "--paths           show expected operator-local source, artifact, report, reference, and registry paths"
contains "$OUT_DIR/help_model_target.out" "--models-root DIR override configured operator model root for this command only"
contains "$OUT_DIR/help_model_target.out" "Model targets are pressure objects, not capability claims."
contains "$OUT_DIR/help_model_target.out" "External GGUFs and external runners are reference evidence only."
contains "$OUT_DIR/help_model_target.out" "Model-target path reporting does not read model payloads, create artifacts, register aliases, or claim runtime support."

run_ok help_models "$YVEX_BIN" help models
contains "$OUT_DIR/help_models.out" "usage: yvex models"
contains "$OUT_DIR/help_models.out" "support-level LEVEL"

run_ok help_native_weights "$YVEX_BIN" help native-weights
contains "$OUT_DIR/help_native_weights.out" "usage: yvex native-weights --source DIR"

run_ok help_tensors "$YVEX_BIN" help tensors
contains "$OUT_DIR/help_tensors.out" "usage: yvex tensors FILE_OR_ALIAS"

run_ok help_tokenizer "$YVEX_BIN" help tokenizer
contains "$OUT_DIR/help_tokenizer.out" "usage: yvex tokenizer <path>"

run_ok help_tokenize "$YVEX_BIN" help tokenize
contains "$OUT_DIR/help_tokenize.out" "usage: yvex tokenize <path> --text TEXT"

run_ok help_detokenize "$YVEX_BIN" help detokenize
contains "$OUT_DIR/help_detokenize.out" "usage: yvex detokenize <path> --ids IDS"

run_ok help_engine "$YVEX_BIN" help engine
contains "$OUT_DIR/help_engine.out" "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]"

run_ok help_graph "$YVEX_BIN" help graph
contains "$OUT_DIR/help_graph.out" "usage: yvex graph [--model] FILE_OR_ALIAS"

run_ok help_gguf_template "$YVEX_BIN" help gguf-template
contains "$OUT_DIR/help_gguf_template.out" "usage: yvex gguf-template"

run_ok help_gguf_emit "$YVEX_BIN" help gguf-emit
contains "$OUT_DIR/help_gguf_emit.out" "usage: yvex gguf-emit"

run_ok help_tensor_map "$YVEX_BIN" help tensor-map
contains "$OUT_DIR/help_tensor_map.out" "usage: yvex tensor-map"

run_ok help_quant_job "$YVEX_BIN" help quant-job
contains "$OUT_DIR/help_quant_job.out" "usage: yvex quant-job"

run_ok help_quant_policy "$YVEX_BIN" help quant-policy
contains "$OUT_DIR/help_quant_policy.out" "usage: yvex quant-policy"

run_ok help_imatrix "$YVEX_BIN" help imatrix
contains "$OUT_DIR/help_imatrix.out" "usage: yvex imatrix"

run_ok help_plan "$YVEX_BIN" help plan
contains "$OUT_DIR/help_plan.out" "usage: yvex plan <path>"

run_ok help_prompt "$YVEX_BIN" help prompt
contains "$OUT_DIR/help_prompt.out" "usage: yvex prompt <path>"

run_ok help_run "$YVEX_BIN" help run
contains "$OUT_DIR/help_run.out" "usage: yvex run --model FILE"

run_ok help_session "$YVEX_BIN" help session
contains "$OUT_DIR/help_session.out" "usage: yvex session FILE_OR_ALIAS [--backend cpu|cuda]"

run_ok help_source_manifest "$YVEX_BIN" help source-manifest
contains "$OUT_DIR/help_source_manifest.out" "usage: yvex source-manifest create"

run_ok model_target_classes "$YVEX_BIN" model-target classes
contains "$OUT_DIR/model_target_classes.out" "status: model-target-classes"
contains "$OUT_DIR/model_target_classes.out" "class: selected-runtime-slice"
contains "$OUT_DIR/model_target_classes.out" "class: official-source-huge-model"
contains "$OUT_DIR/model_target_classes.out" "class: external-GGUF-reference"
contains "$OUT_DIR/model_target_classes.out" "capability_claim: false"
contains "$OUT_DIR/model_target_classes.out" "runtime_execution: partial-boundary-only"
contains "$OUT_DIR/model_target_classes.out" "generation: unsupported"

run_ok model_target_list "$YVEX_BIN" model-target list
contains "$OUT_DIR/model_target_list.out" "status: model-target-list"
contains "$OUT_DIR/model_target_list.out" "target: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_list.out" "target: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_list.out" "target: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_list.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_list.out" "generation: unsupported"

run_ok model_target_deepseek_embed "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed
contains "$OUT_DIR/model_target_deepseek_embed.out" "status: model-target"
contains "$OUT_DIR/model_target_deepseek_embed.out" "target_id: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_deepseek_embed.out" "target_artifact_class: YVEX-produced selected GGUF"
contains "$OUT_DIR/model_target_deepseek_embed.out" "tensor_set: token_embd.weight"
contains "$OUT_DIR/model_target_deepseek_embed.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_deepseek_embed.out" "generation: unsupported"
contains "$OUT_DIR/model_target_deepseek_embed.out" "external_reference: false"

run_ok model_target_deepseek_rmsnorm "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "target_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "pressure_purpose: selected-embedding-plus-rmsnorm-segment"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "tensor_set: token_embd.weight,blk.0.attn_norm.weight"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "generation: unsupported"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "external_reference: false"

run_ok model_target_glm "$YVEX_BIN" model-target inspect glm-5.2-official-safetensors
contains "$OUT_DIR/model_target_glm.out" "target_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_glm.out" "target_class: official-source-huge-model"
contains "$OUT_DIR/model_target_glm.out" "target_artifact_class: future YVEX-produced GGUF"
contains "$OUT_DIR/model_target_glm.out" "local_path_class: hf/glm/GLM-5.2"
contains "$OUT_DIR/model_target_glm.out" "source_footprint_class: 282 safetensors,1.5T-class"
contains "$OUT_DIR/model_target_glm.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_glm.out" "generation: unsupported"
contains "$OUT_DIR/model_target_glm.out" "external_reference: false"

run_ok model_target_help_subcommand "$YVEX_BIN" model-target help
contains "$OUT_DIR/model_target_help_subcommand.out" "Model targets are pressure objects, not capability claims."

MODEL_TARGET_PATHS_DIR="$OUT_DIR/model-target-paths"
MODEL_TARGET_MODELS_ROOT="$(pwd)/$MODEL_TARGET_PATHS_DIR/models"
rm -rf "$MODEL_TARGET_PATHS_DIR"
mkdir -p "$MODEL_TARGET_PATHS_DIR/models"

run_ok model_target_paths_config "$YVEX_BIN" paths --project "$MODEL_TARGET_PATHS_DIR" configure --models-root "$MODEL_TARGET_PATHS_DIR/models" --create
contains "$OUT_DIR/model_target_paths_config.out" "status: paths-configured"
contains "$OUT_DIR/model_target_paths_config.out" "models_root_source: explicit"

run_ok model_target_paths_embed "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root "$MODEL_TARGET_PATHS_DIR/models"
contains "$OUT_DIR/model_target_paths_embed.out" "target_id: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_paths_embed.out" "models_root_source: explicit"
contains "$OUT_DIR/model_target_paths_embed.out" "models_root: $MODEL_TARGET_MODELS_ROOT"
contains "$OUT_DIR/model_target_paths_embed.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/deepseek/DeepSeek-V4-Flash"
contains "$OUT_DIR/model_target_paths_embed.out" "source_exists:"
contains "$OUT_DIR/model_target_paths_embed.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
contains "$OUT_DIR/model_target_paths_embed.out" "artifact_exists:"
contains "$OUT_DIR/model_target_paths_embed.out" "report_dir: $MODEL_TARGET_MODELS_ROOT/reports/deepseek"
contains "$OUT_DIR/model_target_paths_embed.out" "report_dir_exists:"
contains "$OUT_DIR/model_target_paths_embed.out" "reference_dir: $MODEL_TARGET_MODELS_ROOT/reference/deepseek"
contains "$OUT_DIR/model_target_paths_embed.out" "reference_dir_exists:"
contains "$OUT_DIR/model_target_paths_embed.out" "registry_dir: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_embed.out" "registry_dir_exists:"
contains "$OUT_DIR/model_target_paths_embed.out" "registry_alias: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_paths_embed.out" "source_artifact_class: official safetensors"
contains "$OUT_DIR/model_target_paths_embed.out" "target_artifact_class: YVEX-produced selected GGUF"
contains "$OUT_DIR/model_target_paths_embed.out" "runtime_execution: selected-boundary-only"
contains "$OUT_DIR/model_target_paths_embed.out" "generation: unsupported"

run_ok model_target_paths_rmsnorm "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths --models-root "$MODEL_TARGET_PATHS_DIR/models"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "target_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/deepseek/DeepSeek-V4-Flash"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "registry_alias: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "source_artifact_class: official safetensors"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "target_artifact_class: YVEX-produced selected GGUF"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "runtime_execution: selected-segment-boundary-only"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "generation: unsupported"

run_ok model_target_paths_glm "$YVEX_BIN" model-target inspect glm-5.2-official-safetensors --paths --models-root "$MODEL_TARGET_PATHS_DIR/models"
contains "$OUT_DIR/model_target_paths_glm.out" "target_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_paths_glm.out" "models_root_source: explicit"
contains "$OUT_DIR/model_target_paths_glm.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/glm/GLM-5.2"
contains "$OUT_DIR/model_target_paths_glm.out" "artifact_path: planned"
contains "$OUT_DIR/model_target_paths_glm.out" "artifact_exists: false"
contains "$OUT_DIR/model_target_paths_glm.out" "report_dir: $MODEL_TARGET_MODELS_ROOT/reports/glm"
contains "$OUT_DIR/model_target_paths_glm.out" "reference_dir: $MODEL_TARGET_MODELS_ROOT/reference/glm"
contains "$OUT_DIR/model_target_paths_glm.out" "registry_dir: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_glm.out" "registry_alias: none"
contains "$OUT_DIR/model_target_paths_glm.out" "source_artifact_class: official safetensors"
contains "$OUT_DIR/model_target_paths_glm.out" "target_artifact_class: future YVEX-produced GGUF"
contains "$OUT_DIR/model_target_paths_glm.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_paths_glm.out" "generation: unsupported"

(
    MODEL_TARGET_ENV_PROJECT="$MODEL_TARGET_PATHS_DIR/env-project"
    OUT_DIR_ABS="$(pwd)/$OUT_DIR"
    case "$YVEX_BIN" in
        /*) YVEX_BIN_ABS="$YVEX_BIN" ;;
        *) YVEX_BIN_ABS="$(pwd)/$YVEX_BIN" ;;
    esac
    mkdir -p "$MODEL_TARGET_ENV_PROJECT"
    cd "$MODEL_TARGET_ENV_PROJECT"
    YVEX_MODELS_ROOT="$MODEL_TARGET_MODELS_ROOT" "$YVEX_BIN_ABS" model-target inspect deepseek4-v4-flash-selected-embed --paths >"$OUT_DIR_ABS/model_target_paths_env.out" 2>"$OUT_DIR_ABS/model_target_paths_env.err"
)
contains "$OUT_DIR/model_target_paths_env.out" "models_root_source: environment"
contains "$OUT_DIR/model_target_paths_env.out" "models_root: $MODEL_TARGET_MODELS_ROOT"
contains "$OUT_DIR/model_target_paths_env.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"

run_fail model_target_paths_empty_root "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root ""
run_fail model_target_paths_missing_root "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root
run_fail model_target_paths_root_without_paths "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --models-root "$MODEL_TARGET_PATHS_DIR/models"

set +e
"$YVEX_BIN" model-target inspect missing-target >"$OUT_DIR/model_target_missing.out" 2>"$OUT_DIR/model_target_missing.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "missing model target exit code was $rc, expected 2"
fi
contains "$OUT_DIR/model_target_missing.err" "model-target: unknown target: missing-target"

set +e
"$YVEX_BIN" model-target missing-subcommand >"$OUT_DIR/model_target_bad_subcommand.out" 2>"$OUT_DIR/model_target_bad_subcommand.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "bad model-target subcommand exit code was $rc, expected 2"
fi
contains "$OUT_DIR/model_target_bad_subcommand.err" "model-target: unknown subcommand: missing-subcommand"

run_ok inspect_valid "$YVEX_BIN" inspect tests/fixtures/gguf/valid-minimal.gguf
contains "$OUT_DIR/inspect_valid.out" "format: gguf"
contains "$OUT_DIR/inspect_valid.out" "version: 3"
contains "$OUT_DIR/inspect_valid.out" "metadata_count: 0"
contains "$OUT_DIR/inspect_valid.out" "tensor_count: 0"
contains "$OUT_DIR/inspect_valid.out" "architecture: unknown"
contains "$OUT_DIR/inspect_valid.out" "status: descriptor-only"

run_ok inspect_directory "$YVEX_BIN" inspect tests/fixtures/gguf/valid-metadata-tensors.gguf
contains "$OUT_DIR/inspect_directory.out" "metadata_count: 5"
contains "$OUT_DIR/inspect_directory.out" "tensor_count: 1"
contains "$OUT_DIR/inspect_directory.out" "alignment: 32"
contains "$OUT_DIR/inspect_directory.out" "architecture: llama"
contains "$OUT_DIR/inspect_directory.out" "model_name: yvex-test"
contains "$OUT_DIR/inspect_directory.out" "known_tensor_bytes: 128"
contains "$OUT_DIR/inspect_directory.out" "unsupported_tensor_accounting: 0"
contains "$OUT_DIR/inspect_directory.out" "status: descriptor-only"

run_ok metadata "$YVEX_BIN" metadata tests/fixtures/gguf/valid-metadata-tensors.gguf
contains "$OUT_DIR/metadata.out" "format: gguf"
contains "$OUT_DIR/metadata.out" "metadata_count: 5"
contains "$OUT_DIR/metadata.out" "general.architecture = \"llama\""
contains "$OUT_DIR/metadata.out" "general.name = \"yvex-test\""
contains "$OUT_DIR/metadata.out" "llama.context_length = 4096"
contains "$OUT_DIR/metadata.out" "general.file_type = 0"
contains "$OUT_DIR/metadata.out" "general.alignment = 32"

run_ok tensors "$YVEX_BIN" tensors tests/fixtures/gguf/valid-metadata-tensors.gguf
contains "$OUT_DIR/tensors.out" "format: gguf"
contains "$OUT_DIR/tensors.out" "tensor_count: 1"
contains "$OUT_DIR/tensors.out" "alignment: 32"
contains "$OUT_DIR/tensors.out" "0 token_embd.weight role=token_embedding rank=2 dims=[4,8] dtype=F32 bytes=128 offset=0 absolute="

run_ok materialize "$YVEX_BIN" materialize --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
contains "$OUT_DIR/materialize.out" "materialization status: materialized"
contains "$OUT_DIR/materialize.out" "bytes_materialized: 128"
contains "$OUT_DIR/materialize.out" "execution_ready: false"
contains "$OUT_DIR/materialize.out" "status: weights-materialized"

run_ok tokenizer "$YVEX_BIN" tokenizer tests/fixtures/gguf/valid-tokenizer-simple.gguf
contains "$OUT_DIR/tokenizer.out" "tokenizer_model: yvex-fixture-simple"
contains "$OUT_DIR/tokenizer.out" "support: fixture-encode-decode"
contains "$OUT_DIR/tokenizer.out" "vocab_size: 8"
contains "$OUT_DIR/tokenizer.out" "bos_token_id: 1"
contains "$OUT_DIR/tokenizer.out" "eos_token_id: 2"
contains "$OUT_DIR/tokenizer.out" "unk_token_id: 0"
contains "$OUT_DIR/tokenizer.out" "status: tokenizer-descriptor"

run_ok tokenize "$YVEX_BIN" tokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello world"
contains "$OUT_DIR/tokenize.out" "tokens: 3"
contains "$OUT_DIR/tokenize.out" "ids: 3 4 5"
contains "$OUT_DIR/tokenize.out" "  3 \"hello\""
contains "$OUT_DIR/tokenize.out" "status: tokenized"

run_ok detokenize "$YVEX_BIN" detokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --ids 3,4,5
contains "$OUT_DIR/detokenize.out" "text: \"hello world\""
contains "$OUT_DIR/detokenize.out" "status: detokenized"

run_ok prompt "$YVEX_BIN" prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --system "You are helpful" --user "hello world"
contains "$OUT_DIR/prompt.out" "template: yvex-default"
contains "$OUT_DIR/prompt.out" "chat_template_metadata: absent"
contains "$OUT_DIR/prompt.out" "<system>"
contains "$OUT_DIR/prompt.out" "You are helpful"
contains "$OUT_DIR/prompt.out" "<assistant>"
contains "$OUT_DIR/prompt.out" "status: rendered"

run_ok prompt_tokens "$YVEX_BIN" prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --user "hello world" --tokens
contains "$OUT_DIR/prompt_tokens.out" "tokens:"
contains "$OUT_DIR/prompt_tokens.out" "ids:"
contains "$OUT_DIR/prompt_tokens.out" "status: rendered"

run_ok graph "$YVEX_BIN" graph tests/fixtures/gguf/valid-tokenizer-simple.gguf
contains "$OUT_DIR/graph.out" "graph status: partial"
contains "$OUT_DIR/graph.out" "model_name: yvex-tokenizer-test"
contains "$OUT_DIR/graph.out" "value 2 hidden kind=activation shape=[1,4] dtype=F32 residency=host"
contains "$OUT_DIR/graph.out" "op 0 embed status=planned inputs=[0,1] outputs=[2]"
contains "$OUT_DIR/graph.out" "missing output_norm reason=\"required for final normalization\""
contains "$OUT_DIR/graph.out" "status: graph-partial"

run_ok plan_cpu "$YVEX_BIN" plan tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
contains "$OUT_DIR/plan_cpu.out" "plan status: partial"
contains "$OUT_DIR/plan_cpu.out" "backend: cpu"
contains "$OUT_DIR/plan_cpu.out" "backend_status: available"
contains "$OUT_DIR/plan_cpu.out" "tensor_alloc: yes"
contains "$OUT_DIR/plan_cpu.out" "op_embed: yes"
contains "$OUT_DIR/plan_cpu.out" "op_matmul: yes"
contains "$OUT_DIR/plan_cpu.out" "op_mlp: yes"
contains "$OUT_DIR/plan_cpu.out" "op_rope: yes"
contains "$OUT_DIR/plan_cpu.out" "op_attention: yes"
contains "$OUT_DIR/plan_cpu.out" "model_tensor_bytes_known: 128"
contains "$OUT_DIR/plan_cpu.out" "activation_peak_bytes: 16"
contains "$OUT_DIR/plan_cpu.out" "execution_ready: false"
contains "$OUT_DIR/plan_cpu.out" "graph partial; missing output_norm, output_head; backend lacks full graph ops"
contains "$OUT_DIR/plan_cpu.out" "status: plan-only"

run_ok plan_cuda "$YVEX_BIN" plan tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cuda
contains "$OUT_DIR/plan_cuda.out" "backend: cuda"
contains "$OUT_DIR/plan_cuda.out" "backend_status:"
contains "$OUT_DIR/plan_cuda.out" "execution_ready: false"
contains "$OUT_DIR/plan_cuda.out" "status: plan-only"

run_ok backend_cpu "$YVEX_BIN" backend cpu
contains "$OUT_DIR/backend_cpu.out" "backend: cpu"
contains "$OUT_DIR/backend_cpu.out" "status: ready"
contains "$OUT_DIR/backend_cpu.out" "allocated_bytes: 0"
contains "$OUT_DIR/backend_cpu.out" "tensor_alloc: yes"
contains "$OUT_DIR/backend_cpu.out" "tensor_read_write: yes"
contains "$OUT_DIR/backend_cpu.out" "op_embed: yes"
contains "$OUT_DIR/backend_cpu.out" "op_matmul: yes"
contains "$OUT_DIR/backend_cpu.out" "op_mlp: yes"
contains "$OUT_DIR/backend_cpu.out" "op_rope: yes"
contains "$OUT_DIR/backend_cpu.out" "op_attention: yes"
contains "$OUT_DIR/backend_cpu.out" "status: backend-ready"

set +e
"$YVEX_BIN" backend cuda >"$OUT_DIR/backend_cuda.out" 2>"$OUT_DIR/backend_cuda.err"
rc=$?
set -e
contains "$OUT_DIR/backend_cuda.out" "backend: cuda"
if [ "$rc" -eq 0 ]; then
    contains "$OUT_DIR/backend_cuda.out" "status: ready"
    contains "$OUT_DIR/backend_cuda.out" "tensor_alloc: yes"
    contains "$OUT_DIR/backend_cuda.out" "tensor_read_write: yes"
    contains "$OUT_DIR/backend_cuda.out" "op_embed: yes"
    contains "$OUT_DIR/backend_cuda.out" "op_matmul: yes"
    contains "$OUT_DIR/backend_cuda.out" "op_mlp: yes"
    contains "$OUT_DIR/backend_cuda.out" "op_rope: yes"
    contains "$OUT_DIR/backend_cuda.out" "op_attention: yes"
    contains "$OUT_DIR/backend_cuda.out" "status: backend-ready"
elif [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/backend_cuda.out" "status: unsupported"
    contains "$OUT_DIR/backend_cuda.out" "status: backend-unsupported"
else
    fail "backend cuda exit code was $rc"
fi

run_ok engine "$YVEX_BIN" engine tests/fixtures/gguf/valid-tokenizer-simple.gguf
contains "$OUT_DIR/engine.out" "engine status: partial"
contains "$OUT_DIR/engine.out" "architecture: llama"
contains "$OUT_DIR/engine.out" "model_name: yvex-tokenizer-test"
contains "$OUT_DIR/engine.out" "known_tensor_bytes: 128"
contains "$OUT_DIR/engine.out" "tokenizer_model: yvex-fixture-simple"
contains "$OUT_DIR/engine.out" "tokenizer_support: fixture-encode-decode"
contains "$OUT_DIR/engine.out" "graph_status: partial"
contains "$OUT_DIR/engine.out" "execution_ready: false"
contains "$OUT_DIR/engine.out" "reason: graph partial; missing output_norm, output_head"
contains "$OUT_DIR/engine.out" "status: engine-descriptor"

run_ok session_cpu "$YVEX_BIN" session tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
contains "$OUT_DIR/session_cpu.out" "engine_status: partial"
contains "$OUT_DIR/session_cpu.out" "backend: cpu"
contains "$OUT_DIR/session_cpu.out" "backend_status: ready"
contains "$OUT_DIR/session_cpu.out" "session_state: partial"
contains "$OUT_DIR/session_cpu.out" "context_length: 4096"
contains "$OUT_DIR/session_cpu.out" "position: 0"
contains "$OUT_DIR/session_cpu.out" "accepted_tokens: 0"
contains "$OUT_DIR/session_cpu.out" "kv_status: unavailable"
contains "$OUT_DIR/session_cpu.out" "kv_bytes: 0"
contains "$OUT_DIR/session_cpu.out" "logits_status: unavailable"
contains "$OUT_DIR/session_cpu.out" "execution_ready: false"
contains "$OUT_DIR/session_cpu.out" "reason: graph partial; missing output_norm, output_head"
contains "$OUT_DIR/session_cpu.out" "status: session-created"

run_ok session_accept "$YVEX_BIN" session tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --text "hello world" --accept-tokens
contains "$OUT_DIR/session_accept.out" "tokens: 3"
contains "$OUT_DIR/session_accept.out" "accepted_tokens: 3"
contains "$OUT_DIR/session_accept.out" "position: 3"
contains "$OUT_DIR/session_accept.out" "execution_ready: false"
contains "$OUT_DIR/session_accept.out" "status: session-token-accepted"

set +e
"$YVEX_BIN" session tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cuda >"$OUT_DIR/session_cuda.out" 2>"$OUT_DIR/session_cuda.err"
rc=$?
set -e
contains "$OUT_DIR/session_cuda.out" "backend: cuda"
if [ "$rc" -eq 0 ]; then
    contains "$OUT_DIR/session_cuda.out" "backend_status: ready"
    contains "$OUT_DIR/session_cuda.out" "execution_ready: false"
    contains "$OUT_DIR/session_cuda.out" "status: session-created"
elif [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/session_cuda.out" "backend_status: unsupported"
    contains "$OUT_DIR/session_cuda.out" "status: session-backend-unsupported"
else
    fail "session cuda exit code was $rc"
fi

set +e
"$YVEX_BIN" inspect tests/fixtures/gguf/bad-magic.gguf >"$OUT_DIR/inspect_bad_magic.out" 2>"$OUT_DIR/inspect_bad_magic.err"
rc=$?
set -e
if [ "$rc" -ne 5 ]; then
    fail "bad magic inspect exit code was $rc, expected 5"
fi
contains "$OUT_DIR/inspect_bad_magic.out" "format: unknown"
contains "$OUT_DIR/inspect_bad_magic.out" "status: unsupported"

run_ok help_paths "$YVEX_BIN" help paths
contains "$OUT_DIR/help_paths.out" "usage: yvex paths"
contains "$OUT_DIR/help_paths.out" "yvex paths [--project DIR] configure --models-root DIR [--create]"
contains "$OUT_DIR/help_paths.out" "yvex paths [--project DIR] resolve --family deepseek|glm --kind source|gguf|reports|reference|registry"

run_ok paths "$YVEX_BIN" paths
contains "$OUT_DIR/paths.out" "config:"
contains "$OUT_DIR/paths.out" "cache:"
contains "$OUT_DIR/paths.out" "state:"
contains "$OUT_DIR/paths.out" "data:"
contains "$OUT_DIR/paths.out" "project:"
contains "$OUT_DIR/paths.out" "status: paths"
contains "$OUT_DIR/paths.out" "models_root_source:"
contains "$OUT_DIR/paths.out" "models_root:"
contains "$OUT_DIR/paths.out" "hf_root:"
contains "$OUT_DIR/paths.out" "gguf_root:"
contains "$OUT_DIR/paths.out" "reports_root:"
contains "$OUT_DIR/paths.out" "reference_root:"
contains "$OUT_DIR/paths.out" "registry_root:"
contains "$OUT_DIR/paths.out" "operator_config_path:"

run_ok paths_project "$YVEX_BIN" paths --project .
contains "$OUT_DIR/paths_project.out" "project: ./.yvex"

run_ok paths_run "$YVEX_BIN" paths --run
contains "$OUT_DIR/paths_run.out" "run_id: run_"
contains "$OUT_DIR/paths_run.out" "command:"

(
    export YVEX_RUN_DIR="$OUT_DIR/runs"
    run_ok paths_run_create "$YVEX_BIN" paths --run --create
)
contains "$OUT_DIR/paths_run_create.out" "root: $OUT_DIR/runs/run_"
test -d "$OUT_DIR/runs" || fail "paths --run --create did not create run root"

OPERATOR_PATHS_DIR="$OUT_DIR/operator-paths"
rm -rf "$OPERATOR_PATHS_DIR"
mkdir -p "$OPERATOR_PATHS_DIR"

run_ok operator_paths_initial "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR"
contains "$OUT_DIR/operator_paths_initial.out" "status: paths"
contains "$OUT_DIR/operator_paths_initial.out" "models_root_source:"
contains "$OUT_DIR/operator_paths_initial.out" "operator_config_path: $OPERATOR_PATHS_DIR/.yvex/operator-paths.conf"

run_ok operator_paths_configure "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" configure --models-root "$OPERATOR_PATHS_DIR/models"
contains "$OUT_DIR/operator_paths_configure.out" "status: paths-configured"
contains "$OUT_DIR/operator_paths_configure.out" "models_root_source: explicit"
contains "$OUT_DIR/operator_paths_configure.out" "models_root:"
contains "$OUT_DIR/operator_paths_configure.out" "hf_root:"
contains "$OUT_DIR/operator_paths_configure.out" "gguf_root:"
contains "$OUT_DIR/operator_paths_configure.out" "reports_root:"
contains "$OUT_DIR/operator_paths_configure.out" "reference_root:"
contains "$OUT_DIR/operator_paths_configure.out" "registry_root:"
contains "$OUT_DIR/operator_paths_configure.out" "config_path: $OPERATOR_PATHS_DIR/.yvex/operator-paths.conf"
contains "$OUT_DIR/operator_paths_configure.out" "created: false"

run_ok operator_paths_after_config "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR"
contains "$OUT_DIR/operator_paths_after_config.out" "status: paths"
contains "$OUT_DIR/operator_paths_after_config.out" "models_root_source: configured"

for family in deepseek glm; do
    for kind in source gguf reports reference registry; do
        name="operator_paths_resolve_${family}_${kind}"
        run_ok "$name" "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" resolve --family "$family" --kind "$kind"
        contains "$OUT_DIR/$name.out" "status: paths-resolve"
        contains "$OUT_DIR/$name.out" "models_root_source: configured"
        contains "$OUT_DIR/$name.out" "family: $family"
        contains "$OUT_DIR/$name.out" "kind: $kind"
        contains "$OUT_DIR/$name.out" "path:"
        contains "$OUT_DIR/$name.out" "exists:"
    done
done

run_ok operator_paths_configure_create "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" configure --models-root "$OPERATOR_PATHS_DIR/models" --create
contains "$OUT_DIR/operator_paths_configure_create.out" "status: paths-configured"
contains "$OUT_DIR/operator_paths_configure_create.out" "created: true"

for dir in \
    "$OPERATOR_PATHS_DIR/models/hf" \
    "$OPERATOR_PATHS_DIR/models/hf/deepseek" \
    "$OPERATOR_PATHS_DIR/models/hf/glm" \
    "$OPERATOR_PATHS_DIR/models/gguf" \
    "$OPERATOR_PATHS_DIR/models/gguf/deepseek" \
    "$OPERATOR_PATHS_DIR/models/gguf/glm" \
    "$OPERATOR_PATHS_DIR/models/reports" \
    "$OPERATOR_PATHS_DIR/models/reports/deepseek" \
    "$OPERATOR_PATHS_DIR/models/reports/glm" \
    "$OPERATOR_PATHS_DIR/models/reference" \
    "$OPERATOR_PATHS_DIR/models/reference/deepseek" \
    "$OPERATOR_PATHS_DIR/models/reference/glm" \
    "$OPERATOR_PATHS_DIR/models/registry"; do
    test -d "$dir" || fail "operator path directory was not created: $dir"
done

run_ok operator_paths_create "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" --create
contains "$OUT_DIR/operator_paths_create.out" "status: paths-created"
contains "$OUT_DIR/operator_paths_create.out" "created: true"

run_ok operator_paths_reset "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" configure --reset
contains "$OUT_DIR/operator_paths_reset.out" "status: paths-reset"
contains "$OUT_DIR/operator_paths_reset.out" "removed: true"
contains "$OUT_DIR/operator_paths_reset.out" "models_root_source:"
contains "$OUT_DIR/operator_paths_reset.out" "models_root:"

run_fail operator_paths_configure_missing "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" configure
run_fail operator_paths_configure_empty "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" configure --models-root ""
run_fail operator_paths_bad_family "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" resolve --family missing --kind source
run_fail operator_paths_bad_kind "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" resolve --family deepseek --kind missing
run_fail operator_paths_missing_kind "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" resolve --family deepseek
run_fail operator_paths_missing_family "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" resolve --kind source

set +e
"$YVEX_BIN" unknown >"$OUT_DIR/unknown.out" 2>"$OUT_DIR/unknown.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "unknown command exit code was $rc, expected 2"
fi
contains "$OUT_DIR/unknown.err" "yvex: unknown command: unknown"
contains "$OUT_DIR/unknown.err" "Try 'yvex help' for usage."

set +e
"$YVEX_BIN" help unknown >"$OUT_DIR/help_unknown.out" 2>"$OUT_DIR/help_unknown.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "unknown help topic exit code was $rc, expected 2"
fi
contains "$OUT_DIR/help_unknown.err" "yvex: unknown help topic: unknown"

printf 'cli smoke: ok\n'
#   - yvex backend
