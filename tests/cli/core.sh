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
#   - yvex decode
#   - yvex logits
#   - yvex sample
#   - yvex generate
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

matches() {
    file=$1
    pattern=$2
    grep -E -- "$pattern" "$file" >/dev/null || fail "$file missing pattern: $pattern"
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

run_fail_code() {
    name=$1
    expected=$2
    shift 2
    set +e
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err"
    rc=$?
    set -e
    if [ "$rc" -ne "$expected" ]; then
        fail "$name exit code was $rc, expected $expected"
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
contains "$OUT_DIR/info.out" "info: YVEX"
contains "$OUT_DIR/info.out" "runtime: bounded diagnostic generation"
contains "$OUT_DIR/info.out" "cli_output: normal"
contains "$OUT_DIR/info.out" "full_model_generation: unsupported"
contains "$OUT_DIR/info.out" "hint: use --audit for full diagnostic fields"

run_ok info_audit "$YVEX_BIN" info --audit
contains "$OUT_DIR/info_audit.out" "name: YVEX"
contains "$OUT_DIR/info_audit.out" "status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, matmul, and MLP ops, explicit token input boundary, prefill state foundation, minimal KV binding, minimal KV ownership, bounded decode/logits/sampling diagnostics, and bounded diagnostic generation loop with explicit append accounting"
contains "$OUT_DIR/info_audit.out" "library: libyvex.a"
contains "$OUT_DIR/info_audit.out" "filesystem: implemented"
contains "$OUT_DIR/info_audit.out" "artifact: open/read implemented"
contains "$OUT_DIR/info_audit.out" "gguf: metadata/tensor directory parsing implemented"
contains "$OUT_DIR/info_audit.out" "model: descriptor-only implemented"
contains "$OUT_DIR/info_audit.out" "tokenizer: fixture encode/decode implemented"
contains "$OUT_DIR/info_audit.out" "token_input: explicit token boundary implemented"
contains "$OUT_DIR/info_audit.out" "prefill_state: segment-summary foundation, bounded layer-backed prefill state, chunked prefill lifecycle, and minimal KV binding implemented"
contains "$OUT_DIR/info_audit.out" "prompt: default renderer implemented"
contains "$OUT_DIR/info_audit.out" "graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, and standalone F32 MLP/feed-forward primitive implemented"
contains "$OUT_DIR/info_audit.out" "planner: estimate-only implemented"
contains "$OUT_DIR/info_audit.out" "backend: CPU reference implemented"
contains "$OUT_DIR/info_audit.out" "backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, F32 matmul/projection primitive, and F32 MLP/feed-forward primitive implemented when CUDA is available"
contains "$OUT_DIR/info_audit.out" "weights: selected tensor materialization implemented"
contains "$OUT_DIR/info_audit.out" "engine: descriptor open and selected-weight attachment implemented"
contains "$OUT_DIR/info_audit.out" "session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented"
contains "$OUT_DIR/info_audit.out" "run: accepted-only runtime shell implemented"
contains "$OUT_DIR/info_audit.out" "chat: accepted-only REPL shell implemented"
contains "$OUT_DIR/info_audit.out" "metrics: runtime collector implemented"
contains "$OUT_DIR/info_audit.out" "trace: JSONL writer implemented"
contains "$OUT_DIR/info_audit.out" "profile: JSON writer implemented"
contains "$OUT_DIR/info_audit.out" "run_artifacts: metrics/trace/profile files implemented"
contains "$OUT_DIR/info_audit.out" "source_manifest: provenance JSON writer implemented"
contains "$OUT_DIR/info_audit.out" "native_weights: safetensors header inventory implemented"
contains "$OUT_DIR/info_audit.out" "gguf_template: contract validator implemented"
contains "$OUT_DIR/info_audit.out" "gguf_emit: controlled GGUF writer implemented"
contains "$OUT_DIR/info_audit.out" "conversion: open-weight selected tensor bridge implemented"
contains "$OUT_DIR/info_audit.out" "model_ref: alias-or-path resolver implemented"
contains "$OUT_DIR/info_audit.out" "model_registry: local model alias registry implemented"
contains "$OUT_DIR/info_audit.out" "quant_job: external quantization job manifest implemented"
contains "$OUT_DIR/info_audit.out" "qtype_support: conversion support matrix implemented"
contains "$OUT_DIR/info_audit.out" "weight_mapping: tensor adapter contract implemented"
contains "$OUT_DIR/info_audit.out" "quant_policy: manifest validator implemented"
contains "$OUT_DIR/info_audit.out" "imatrix: calibration artifact manifest implemented"
contains "$OUT_DIR/info_audit.out" "server_binary: yvexd shell implemented"
contains "$OUT_DIR/info_audit.out" "server_endpoints: health/metrics/models status implemented"
contains "$OUT_DIR/info_audit.out" "server_generation: not implemented"
contains "$OUT_DIR/info_audit.out" "kv: minimal session-owned append/read boundary implemented"
contains "$OUT_DIR/info_audit.out" "decode: bounded diagnostic state step implemented"
contains "$OUT_DIR/info_audit.out" "logits: bounded diagnostic buffer implemented"
contains "$OUT_DIR/info_audit.out" "sampling: bounded greedy sampler implemented"
contains "$OUT_DIR/info_audit.out" "generation_loop: bounded diagnostic loop implemented"
contains "$OUT_DIR/info_audit.out" "generation: unsupported-full-model"
contains "$OUT_DIR/info_audit.out" "cuda: available when local driver/device probe succeeds"
contains "$OUT_DIR/info_audit.out" "server: yvexd status shell implemented"
run_fail_code info_bad_output 2 "$YVEX_BIN" info --output nope
contains "$OUT_DIR/info_bad_output.err" "yvex info: unsupported output mode: nope"

run_ok commands "$YVEX_BIN" commands
contains "$OUT_DIR/commands.out" "Implemented commands:"
contains "$OUT_DIR/commands.out" "  attention"
contains "$OUT_DIR/commands.out" "attention class and KV requirement reports"
contains "$OUT_DIR/commands.out" "  backend"
contains "$OUT_DIR/commands.out" "  chat"
contains "$OUT_DIR/commands.out" "  commands"
contains "$OUT_DIR/commands.out" "  context"
contains "$OUT_DIR/commands.out" "context class and runtime boundary reports"
contains "$OUT_DIR/commands.out" "  convert"
contains "$OUT_DIR/commands.out" "  cuda-info"
contains "$OUT_DIR/commands.out" "  decode"
contains "$OUT_DIR/commands.out" "  detokenize"
contains "$OUT_DIR/commands.out" "  engine"
contains "$OUT_DIR/commands.out" "  graph"
contains "$OUT_DIR/commands.out" "  generate"
contains "$OUT_DIR/commands.out" "bounded diagnostic generation loop"
contains "$OUT_DIR/commands.out" "  gguf-emit"
contains "$OUT_DIR/commands.out" "  gguf-template"
contains "$OUT_DIR/commands.out" "  help"
contains "$OUT_DIR/commands.out" "  imatrix"
contains "$OUT_DIR/commands.out" "  info"
contains "$OUT_DIR/commands.out" "  inspect"
contains "$OUT_DIR/commands.out" "  input"
contains "$OUT_DIR/commands.out" "  kv"
contains "$OUT_DIR/commands.out" "KV diagnostics and KV cache class reports"
contains "$OUT_DIR/commands.out" "  logits"
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
contains "$OUT_DIR/commands.out" "  sample"
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

run_ok help_attention "$YVEX_BIN" help attention
contains "$OUT_DIR/help_attention.out" "usage: yvex attention report --model FILE_OR_ALIAS"
contains "$OUT_DIR/help_attention.out" "classifies attention requirements"
contains "$OUT_DIR/help_attention.out" "report-only boundary"
contains "$OUT_DIR/help_attention.out" "does not run full attention"
contains "$OUT_DIR/help_attention.out" "does not run transformer prefill"
contains "$OUT_DIR/help_attention.out" "does not write real attention-backed KV"
contains "$OUT_DIR/help_attention.out" "does not generate"
contains "$OUT_DIR/help_attention.out" "does not benchmark"
contains "$OUT_DIR/help_attention.out" "not full transformer attention"

run_ok help_context "$YVEX_BIN" help context
contains "$OUT_DIR/help_context.out" "usage: yvex context report --model FILE_OR_ALIAS"
contains "$OUT_DIR/help_context.out" "report-only boundary"
contains "$OUT_DIR/help_context.out" "model/requested/active context"
contains "$OUT_DIR/help_context.out" "chunking policy"
contains "$OUT_DIR/help_context.out" "overflow behavior"
contains "$OUT_DIR/help_context.out" "does not run full transformer prefill"
contains "$OUT_DIR/help_context.out" "does not execute real decode"
contains "$OUT_DIR/help_context.out" "does not generate"
contains "$OUT_DIR/help_context.out" "does not benchmark"
contains "$OUT_DIR/help_context.out" "no long-context runtime support"
contains "$OUT_DIR/help_context.out" "no context extension support"

run_ok help_convert "$YVEX_BIN" help convert
contains "$OUT_DIR/help_convert.out" "usage: yvex convert"

run_ok help_qtype_support "$YVEX_BIN" help qtype-support
contains "$OUT_DIR/help_qtype_support.out" "usage: yvex qtype-support"

run_ok help_cuda_info "$YVEX_BIN" help cuda-info
contains "$OUT_DIR/help_cuda_info.out" "usage: yvex cuda-info"

run_ok help_decode "$YVEX_BIN" help decode
contains "$OUT_DIR/help_decode.out" "usage: yvex decode"
contains "$OUT_DIR/help_decode.out" "bounded diagnostic decode-state step"
contains "$OUT_DIR/help_decode.out" "does not produce logits, sample, generate"

run_ok help_logits "$YVEX_BIN" help logits
contains "$OUT_DIR/help_logits.out" "usage: yvex logits"
contains "$OUT_DIR/help_logits.out" "bounded diagnostic logits buffer"
contains "$OUT_DIR/help_logits.out" "does not run the real model output head, sample, generate"

run_ok help_sample "$YVEX_BIN" help sample
contains "$OUT_DIR/help_sample.out" "usage: yvex sample"
contains "$OUT_DIR/help_sample.out" "bounded diagnostic token"
contains "$OUT_DIR/help_sample.out" "does not run stochastic sampling, append tokens, generate"

run_ok help_generate "$YVEX_BIN" help generate
contains "$OUT_DIR/help_generate.out" "usage: yvex generate --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS --max-new-tokens N [options]"
contains "$OUT_DIR/help_generate.out" "Normal path:"
contains "$OUT_DIR/help_generate.out" "./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3"
contains "$OUT_DIR/help_generate.out" "--max-new-tokens 2 --trace-level full"
contains "$OUT_DIR/help_generate.out" "--cancel-after-steps 1"
contains "$OUT_DIR/help_generate.out" "--context-length 5"
contains "$OUT_DIR/help_generate.out" "Required arguments:"
contains "$OUT_DIR/help_generate.out" "Diagnostic options:"
contains "$OUT_DIR/help_generate.out" "Trace options:"
contains "$OUT_DIR/help_generate.out" "Cancellation options:"
contains "$OUT_DIR/help_generate.out" "Stop behavior:"
contains "$OUT_DIR/help_generate.out" "Output policy:"
contains "$OUT_DIR/help_generate.out" "Boundaries:"
contains "$OUT_DIR/help_generate.out" "--trace-level none|tokens|steps|kv|logits|sampling|full"
contains "$OUT_DIR/help_generate.out" "Bounded diagnostic generation loop"
contains "$OUT_DIR/help_generate.out" "EOS and stop-token text matching are unsupported"
contains "$OUT_DIR/help_generate.out" "Trace records are diagnostic text records only"
contains "$OUT_DIR/help_generate.out" "--cancel-after-steps N"
contains "$OUT_DIR/help_generate.out" "Partial diagnostic output is preserved"
contains "$OUT_DIR/help_generate.out" "The command emits no ANSI color by default"
contains "$OUT_DIR/help_generate.out" "full model generation: unsupported"
contains "$OUT_DIR/help_generate.out" "real DeepSeek generation: unsupported"
contains "$OUT_DIR/help_generate.out" "real output-head logits: unsupported"
contains "$OUT_DIR/help_generate.out" "real vocabulary sampling: unsupported"
contains "$OUT_DIR/help_generate.out" "provider/server/streaming generation: unsupported"
contains "$OUT_DIR/help_generate.out" "benchmark_status: not-measured"

run_ok help_chat "$YVEX_BIN" help chat
contains "$OUT_DIR/help_chat.out" "usage: yvex chat [--model FILE_OR_ALIAS]"

run_ok help_inspect "$YVEX_BIN" help inspect
contains "$OUT_DIR/help_inspect.out" "usage: yvex inspect FILE_OR_ALIAS"

run_ok help_input "$YVEX_BIN" help input
contains "$OUT_DIR/help_input.out" "usage: yvex input tokens"

run_ok help_kv "$YVEX_BIN" help kv
contains "$OUT_DIR/help_kv.out" "usage: yvex kv report --model FILE_OR_ALIAS"
contains "$OUT_DIR/help_kv.out" "usage: yvex kv --layers N --heads N --head-dim N --capacity N"
contains "$OUT_DIR/help_kv.out" "KV cache class and requirements report"
contains "$OUT_DIR/help_kv.out" "report-only boundary"
contains "$OUT_DIR/help_kv.out" "does not allocate full runtime KV"
contains "$OUT_DIR/help_kv.out" "write real attention-backed KV"
contains "$OUT_DIR/help_kv.out" "execute decode"
contains "$OUT_DIR/help_kv.out" "generate"
contains "$OUT_DIR/help_kv.out" "benchmark"
contains "$OUT_DIR/help_kv.out" "diagnostic/minimal"
contains "$OUT_DIR/help_kv.out" "not DeepSeek KV, real attention KV"

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
contains "$OUT_DIR/help_model_target.out" "       yvex model-target candidate --release v0.1.0 [options]"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target dense-candidate --release v0.1.0 [options]"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target qwen-metal --release v0.1.0 [options]"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target tensor-map TARGET"
contains "$OUT_DIR/help_model_target.out" "       yvex model-target inspect TARGET [--paths] [--models-root DIR]"
contains "$OUT_DIR/help_model_target.out" "--paths           show expected operator-local source, artifact, report, reference, and registry paths"
contains "$OUT_DIR/help_model_target.out" "--models-root DIR override configured operator model root for this command only"
contains "$OUT_DIR/help_model_target.out" "The candidate report evaluates full-runtime target eligibility for a release."
contains "$OUT_DIR/help_model_target.out" "The dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate."
contains "$OUT_DIR/help_model_target.out" "The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work."
contains "$OUT_DIR/help_model_target.out" "The tensor naming map reads safetensors headers only"
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

run_ok help_prefill "$YVEX_BIN" help prefill
contains "$OUT_DIR/help_prefill.out" "--layers N"
contains "$OUT_DIR/help_prefill.out" "--chunk-size N"
contains "$OUT_DIR/help_prefill.out" "--position-start N"
contains "$OUT_DIR/help_prefill.out" "--context-length N"
contains "$OUT_DIR/help_prefill.out" "Layer-backed prefill uses the selected embedding+RMSNorm segment plus a controlled layer fixture scheduler over a sampled row."
contains "$OUT_DIR/help_prefill.out" "Chunked prefill partitions validated token input into bounded diagnostic chunks"
contains "$OUT_DIR/help_prefill.out" "It is not full transformer prefill, decode, logits, sampling, or generation."

run_ok help_prompt "$YVEX_BIN" help prompt
contains "$OUT_DIR/help_prompt.out" "usage: yvex prompt <path>"

run_ok help_run "$YVEX_BIN" help run
contains "$OUT_DIR/help_run.out" "usage: yvex run --model FILE"

run_ok help_session "$YVEX_BIN" help session
contains "$OUT_DIR/help_session.out" "usage: yvex session FILE_OR_ALIAS [--backend cpu|cuda]"

run_ok help_source_manifest "$YVEX_BIN" help source-manifest
contains "$OUT_DIR/help_source_manifest.out" "usage: yvex source-manifest create"
contains "$OUT_DIR/help_source_manifest.out" "source-manifest report --family qwen --release v0.1.0 [options]"
contains "$OUT_DIR/help_source_manifest.out" "does not download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready"

run_ok source_manifest_report_help "$YVEX_BIN" source-manifest report --help
contains "$OUT_DIR/source_manifest_report_help.out" "usage: yvex source-manifest report --family qwen|gemma --release v0.1.0 [options]"
contains "$OUT_DIR/source_manifest_report_help.out" "--family qwen|gemma"
contains "$OUT_DIR/source_manifest_report_help.out" "--output normal|table|audit"
contains "$OUT_DIR/source_manifest_report_help.out" "--audit"
contains "$OUT_DIR/source_manifest_report_help.out" "Report fields include source artifact class, target artifact class, source footprint, and source provenance evidence."
contains "$OUT_DIR/source_manifest_report_help.out" "Source footprint reports count top-level regular files and bytes without loading tensor payloads."
contains "$OUT_DIR/source_manifest_report_help.out" "Source provenance fields classify local/planned state only; they do not verify upstream identity, hash files, or prove source readiness."
contains "$OUT_DIR/source_manifest_report_help.out" "Native safetensors inventory reads safetensors headers only and never loads tensor payload bytes."
contains "$OUT_DIR/source_manifest_report_help.out" "Source tensor metadata inventory is derived from safetensors headers only and does not map tensors to runtime roles."
contains "$OUT_DIR/source_manifest_report_help.out" "Source manifest hardening is shallow/report-only; it does not create manifests, check remotes, hash files, load payloads, or prove source readiness."
run_fail_code source_manifest_report_missing 2 "$YVEX_BIN" source-manifest report
contains "$OUT_DIR/source_manifest_report_missing.err" "source-manifest report: --family is required"
run_fail_code source_manifest_report_missing_release 2 "$YVEX_BIN" source-manifest report --family qwen
contains "$OUT_DIR/source_manifest_report_missing_release.err" "source-manifest report: --release is required"
run_fail_code source_manifest_report_missing_family 2 "$YVEX_BIN" source-manifest report --release v0.1.0
contains "$OUT_DIR/source_manifest_report_missing_family.err" "source-manifest report: --family is required"
run_fail_code source_manifest_report_bad_family 2 "$YVEX_BIN" source-manifest report --family nope --release v0.1.0
contains "$OUT_DIR/source_manifest_report_bad_family.err" "source-manifest report: unsupported family: nope"
run_fail_code source_manifest_report_bad_release 2 "$YVEX_BIN" source-manifest report --family qwen --release nope
contains "$OUT_DIR/source_manifest_report_bad_release.err" "source-manifest report: unsupported release: nope"
run_fail_code source_manifest_report_bad_output 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --output nope
contains "$OUT_DIR/source_manifest_report_bad_output.err" "source-manifest report: unsupported output mode: nope"
run_fail_code source_manifest_report_gemma_bad_output 2 "$YVEX_BIN" source-manifest report --family gemma --release v0.1.0 --output nope
contains "$OUT_DIR/source_manifest_report_gemma_bad_output.err" "source-manifest report: unsupported output mode: nope"
run_fail_code source_manifest_report_missing_source_value 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source
contains "$OUT_DIR/source_manifest_report_missing_source_value.err" "source-manifest report: --source requires DIR"
run_fail_code source_manifest_report_missing_target_value 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --target
contains "$OUT_DIR/source_manifest_report_missing_target_value.err" "source-manifest report: --target requires TARGET"
run_fail_code source_manifest_report_missing_models_root_value 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --models-root
contains "$OUT_DIR/source_manifest_report_missing_models_root_value.err" "source-manifest report: --models-root requires DIR"
run_fail_code source_manifest_report_missing_tensor_limit_value 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --tensor-limit
contains "$OUT_DIR/source_manifest_report_missing_tensor_limit_value.err" "source-manifest report: --tensor-limit requires N"
run_fail_code source_manifest_report_bad_tensor_limit 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --tensor-limit nope
contains "$OUT_DIR/source_manifest_report_bad_tensor_limit.err" "source-manifest report: --tensor-limit requires a positive integer"
run_fail_code source_manifest_report_unknown_flag 2 "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --unknown
contains "$OUT_DIR/source_manifest_report_unknown_flag.err" "source-manifest report: unknown option: --unknown"

QWEN_MISSING_ROOT="$OUT_DIR/qwen-missing-models-root"
rm -rf "$QWEN_MISSING_ROOT"
run_ok source_manifest_report_missing_source "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --models-root "$QWEN_MISSING_ROOT"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "report: qwen-source-pressure"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "status: source-target-profiled"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "family: qwen"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "target: qwen3-8b"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "source: official-source-tensors-planned  status=missing"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "files: 0  safetensors: 0  bytes: 0  footprint: missing"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "provenance: planned-official status=missing revision=unknown"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "native: missing  files=0  tensors=0  header_bytes=0"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "metadata: missing  tensors=0  dtypes=0  max_rank=0"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "manifest: missing  consistency=not-checked"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "top_blocker: missing-qwen-source-manifest"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "next: V010.MAP.7"
contains "$OUT_DIR/source_manifest_report_missing_source.out" "boundary: source report only; no artifact/runtime/generation/benchmark"
! grep -F 'missing-qwen-source-target' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: OWI.TARGETS.QWEN.0' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.1' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.2' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.3' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.4' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.5' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.6' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.7' "$OUT_DIR/source_manifest_report_missing_source.out" >/dev/null
run_ok source_manifest_report_table "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --models-root "$QWEN_MISSING_ROOT" --output table
contains "$OUT_DIR/source_manifest_report_table.out" "SOURCE PRESSURE  release=v0.1.0"
matches "$OUT_DIR/source_manifest_report_table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}SOURCE[[:space:]]{2,}TENSORS[[:space:]]{2,}MANIFEST[[:space:]]{2,}CONSISTENCY[[:space:]]{2,}NEXT$'
matches "$OUT_DIR/source_manifest_report_table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}missing[[:space:]]{2,}0[[:space:]]{2,}missing[[:space:]]{2,}not-checked[[:space:]]{2,}V010\.MAP\.7$'
run_ok source_manifest_report_audit "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --models-root "$QWEN_MISSING_ROOT" --audit
contains "$OUT_DIR/source_manifest_report_audit.out" "source-report: qwen"
contains "$OUT_DIR/source_manifest_report_audit.out" "family_key: qwen"
contains "$OUT_DIR/source_manifest_report_audit.out" "model: Qwen3-8B"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_id: qwen3-8b"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_target_status: profiled"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_family_profile_status: present"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_artifact_format: safetensors+config-tokenizer-sidecars"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_artifact_origin: official"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_artifact_authority: upstream-official"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_sidecar_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_container: safetensors"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_payload_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_artifact_origin: planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "target_artifact_required: true"
contains "$OUT_DIR/source_manifest_report_audit.out" "external_reference_status: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "yvex_produced_artifact_status: planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_provenance_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_authority_status: planned"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_path_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_exists: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_regular_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_safetensors_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_bin_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_dat_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_json_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tokenizer_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_config_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_total_size_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_safetensors_size_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_sidecar_size_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_other_size_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_footprint_class: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_footprint_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_count_scope: top-level-regular-files"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "largest_source_file_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "largest_source_file_name: none"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_expected: true"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_schema_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_schema_version: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_family: qwen"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_family_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_target_id: qwen3-8b"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_target_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_source_path_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_artifact_class_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_footprint_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_authority: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_provenance_status: manifest-missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_native_inventory_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_tensor_metadata_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_consistency_status: not-checked"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_hardening_status: report-only"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_creation_performed: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_manifest_hash_computed: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_revision: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_commit: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_commit_status: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tag: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tag_status: unknown"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_license_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_readme_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_identity_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_digest_status: not-computed"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_inventory_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_inventory_scope: top-level-safetensors-headers"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_inventory_source: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_opened: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_header_read_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_header_error_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_header_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_safetensors_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_tensor_metadata_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_tensor_payload_status: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_declared_data_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_declared_tensor_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_max_rank: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_max_tensor_elements: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_largest_tensor_name: none"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_largest_tensor_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_f16_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_bf16_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_f32_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_i8_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_i16_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_i32_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_i64_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_u8_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_dtype_other_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_invalid_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_inventory_error_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "native_inventory_report_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_status: missing"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_scope: safetensors-header"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_source: not-present"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_name_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_file_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_dtype_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_rank_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_shape_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_declared_data_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_declared_tensor_bytes: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_total_elements: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_max_rank: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_max_elements: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_largest_name: none"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_largest_file: none"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_largest_dtype: none"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_largest_shape: []"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_dtype_f16_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_dtype_f32_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_rank_2_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_name_pattern_status: lexical-only"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_name_embed_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_name_lm_head_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "source_tensor_metadata_error_count: 0"
contains "$OUT_DIR/source_manifest_report_audit.out" "runtime_claim: unsupported"
contains "$OUT_DIR/source_manifest_report_audit.out" "generation: unsupported-full-model"
contains "$OUT_DIR/source_manifest_report_audit.out" "benchmark_status: not-measured"
contains "$OUT_DIR/source_manifest_report_audit.out" "release_ready: false"
contains "$OUT_DIR/source_manifest_report_audit.out" "blocker_0: missing-qwen-source-path"
contains "$OUT_DIR/source_manifest_report_audit.out" "next_required_rows: V010.MAP.7"

GEMMA_MISSING_ROOT="$OUT_DIR/gemma-missing-models-root"
rm -rf "$GEMMA_MISSING_ROOT"
run_ok source_manifest_report_gemma_missing_source "$YVEX_BIN" source-manifest report --family gemma --release v0.1.0 --models-root "$GEMMA_MISSING_ROOT"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "report: gemma-source-pressure"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "status: source-target-profiled"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "family: gemma"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "target: gemma-4-12b-it"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "source: official-source-tensors-planned  status=missing"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "files: 0  safetensors: 0  bytes: 0  footprint: missing"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "provenance: planned-official status=missing revision=unknown"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "native: missing  files=0  tensors=0  header_bytes=0"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "metadata: missing  tensors=0  dtypes=0  max_rank=0"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "manifest: missing  consistency=not-checked"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "top_blocker: missing-gemma-source-manifest"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "next: V010.MAP.7"
contains "$OUT_DIR/source_manifest_report_gemma_missing_source.out" "boundary: source report only; no artifact/runtime/generation/benchmark"
! grep -F 'missing-gemma-source-target' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: OWI.TARGETS.GEMMA.0' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.1' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.2' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.3' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.4' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.5' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.6' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
! grep -F 'next: V010.SOURCE.7' "$OUT_DIR/source_manifest_report_gemma_missing_source.out" >/dev/null
run_ok source_manifest_report_gemma_table "$YVEX_BIN" source-manifest report --family gemma --release v0.1.0 --models-root "$GEMMA_MISSING_ROOT" --output table
contains "$OUT_DIR/source_manifest_report_gemma_table.out" "SOURCE PRESSURE  release=v0.1.0"
matches "$OUT_DIR/source_manifest_report_gemma_table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}missing[[:space:]]{2,}0[[:space:]]{2,}missing[[:space:]]{2,}not-checked[[:space:]]{2,}V010\.MAP\.7$'
run_ok source_manifest_report_gemma_audit "$YVEX_BIN" source-manifest report --family gemma --release v0.1.0 --models-root "$GEMMA_MISSING_ROOT" --audit
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source-report: gemma"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "family: Gemma"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "family_key: gemma"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "model: Gemma-4-12B-it"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "target_id: gemma-4-12b-it"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_family_profile_status: present"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_artifact_origin: official"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_provenance_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_authority_status: planned"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_file_count: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_regular_file_count: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_safetensors_count: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_total_size_bytes: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_footprint_class: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_count_scope: top-level-regular-files"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_expected: true"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_schema_status: not-checked"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_family: gemma"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_family_status: not-checked"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_target_id: gemma-4-12b-it"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_target_status: not-checked"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_consistency_status: not-checked"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_hardening_status: report-only"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_creation_performed: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_manifest_hash_computed: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_commit_status: unknown"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "native_inventory_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "native_safetensors_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_tensor_metadata_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "largest_source_file_name: none"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "runtime_shape: dense-candidate-pending-source-config"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "source_path_status: missing"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "blocker_0: missing-gemma-source-path"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "blocker_16: missing-gemma-benchmark-path"
contains "$OUT_DIR/source_manifest_report_gemma_audit.out" "next_required_rows: V010.MAP.7"

QWEN_FAKE_SOURCE="${TMPDIR:-/tmp}/yvex-native-safetensors-inventory-test-$$"
QWEN_FAKE_MODELS="${TMPDIR:-/tmp}/yvex-native-safetensors-inventory-models-$$"
rm -rf "$QWEN_FAKE_SOURCE" "$QWEN_FAKE_MODELS"
mkdir -p "$QWEN_FAKE_SOURCE"
printf 'readme\n' > "$QWEN_FAKE_SOURCE/README.md"
printf 'license\n' > "$QWEN_FAKE_SOURCE/LICENSE"
printf '{}\n' > "$QWEN_FAKE_SOURCE/config.json"
printf '{}\n' > "$QWEN_FAKE_SOURCE/tokenizer.json"
printf 'note\n' > "$QWEN_FAKE_SOURCE/notes.txt"
python3 - "$QWEN_FAKE_SOURCE/model-00001-of-00002.safetensors" "$QWEN_FAKE_SOURCE/model-00002-of-00002.safetensors" <<'PY'
import json
import struct
import sys

items = [
    (sys.argv[1], {"token_embd.weight": {"dtype": "F32", "shape": [2, 3], "data_offsets": [0, 24]}}, b"1" * 24),
    (sys.argv[2], {"output.weight": {"dtype": "F16", "shape": [3, 2], "data_offsets": [0, 12]}}, b"2" * 12),
]
for path, header, payload in items:
    blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(blob)))
        f.write(blob)
        f.write(payload)
PY
run_ok source_manifest_report_fake_source "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_SOURCE" --models-root "$QWEN_FAKE_MODELS"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "status: source-present-report-only"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "source: official-source-tensors-planned  status=present"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "files: 7  safetensors: 2"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "footprint: tiny"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "provenance: local-path status=local-unverified revision=unknown"
matches "$OUT_DIR/source_manifest_report_fake_source.out" '^native: header-only[[:space:]]{2,}files=2[[:space:]]{2,}tensors=2[[:space:]]{2,}header_bytes=[1-9][0-9]*$'
contains "$OUT_DIR/source_manifest_report_fake_source.out" "metadata: header-only  tensors=2  dtypes=2  max_rank=2"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "manifest: missing  consistency=not-checked"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "top_blocker: missing-qwen-source-manifest"
contains "$OUT_DIR/source_manifest_report_fake_source.out" "next: V010.MAP.7"
run_ok source_manifest_report_fake_source_table "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_SOURCE" --models-root "$QWEN_FAKE_MODELS" --output table
matches "$OUT_DIR/source_manifest_report_fake_source_table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}present[[:space:]]{2,}2[[:space:]]{2,}missing[[:space:]]{2,}not-checked[[:space:]]{2,}V010\.MAP\.7$'
run_ok source_manifest_report_fake_source_tensors "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_SOURCE" --models-root "$QWEN_FAKE_MODELS" --include-tensors --tensor-limit 2
contains "$OUT_DIR/source_manifest_report_fake_source_tensors.out" "TENSORS  limit=2"
matches "$OUT_DIR/source_manifest_report_fake_source_tensors.out" '^NAME[[:space:]]{2,}FILE[[:space:]]{2,}DTYPE[[:space:]]{2,}RANK[[:space:]]{2,}SHAPE[[:space:]]{2,}ELEMENTS[[:space:]]{2,}BYTES$'
matches "$OUT_DIR/source_manifest_report_fake_source_tensors.out" '^token_embd\.weight[[:space:]]{2,}model-00001-of-00002\.safetensors[[:space:]]{2,}F32[[:space:]]{2,}2[[:space:]]{2,}\[2,3\][[:space:]]{2,}6[[:space:]]{2,}24$'
matches "$OUT_DIR/source_manifest_report_fake_source_tensors.out" '^output\.weight[[:space:]]{2,}model-00002-of-00002\.safetensors[[:space:]]{2,}F16[[:space:]]{2,}2[[:space:]]{2,}\[3,2\][[:space:]]{2,}6[[:space:]]{2,}12$'
run_ok source_manifest_report_fake_source_audit "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_SOURCE" --models-root "$QWEN_FAKE_MODELS" --audit
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_exists: true"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_provenance_status: local-unverified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_origin: explicit-source-path"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_authority: local-unverified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_authority_status: local-unverified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "config_status: present"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "tokenizer_status: present"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_sidecar_status: present"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "safetensors_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_file_count: 7"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_regular_file_count: 7"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_safetensors_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_json_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tokenizer_file_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_config_file_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_footprint_class: tiny"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_footprint_status: report-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_count_scope: top-level-regular-files"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_payload_status: present-not-loaded"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_expected: true"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_status: missing"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_schema_status: not-checked"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_consistency_status: not-checked"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_hardening_status: report-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_creation_performed: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_hash_computed: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_manifest_provenance_status: manifest-missing"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_commit_status: unknown"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tag_status: unknown"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_license_status: present-unverified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_readme_status: present-unverified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_identity_status: not-verified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_digest_status: not-computed"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_inventory_status: header-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_inventory_source: source-path"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_opened: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_header_read_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_header_error_count: 0"
matches "$OUT_DIR/source_manifest_report_fake_source_audit.out" '^native_safetensors_header_bytes: [1-9][0-9]*$'
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_safetensors_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_tensor_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_tensor_metadata_status: header-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_tensor_payload_status: not-loaded"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_declared_data_bytes: 36"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_declared_tensor_bytes: 36"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_max_rank: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_max_tensor_elements: 6"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_largest_tensor_name: token_embd.weight"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_largest_tensor_bytes: 24"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_dtype_f16_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_dtype_f32_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_invalid_file_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "native_inventory_error_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_status: header-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_scope: safetensors-header"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_source: source-path"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_file_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_dtype_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_rank_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_shape_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_declared_data_bytes: 36"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_declared_tensor_bytes: 36"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_total_elements: 12"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_max_rank: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_max_elements: 6"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_name: token_embd.weight"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_file: model-00001-of-00002.safetensors"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_dtype: F32"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_rank: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_shape: [2,3]"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_elements: 6"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_largest_declared_bytes: 24"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_dtype_f16_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_dtype_f32_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_rank_2_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_pattern_status: lexical-only"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_embed_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_attn_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_mlp_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_norm_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_lm_head_count: 1"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_name_other_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_metadata_error_count: 0"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_sample_count: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_name: output.weight"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_file: model-00002-of-00002.safetensors"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_dtype: F16"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_rank: 2"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_shape: [3,2]"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_elements: 6"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_0_declared_bytes: 12"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_1_name: token_embd.weight"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_1_file: model-00001-of-00002.safetensors"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_1_dtype: F32"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "source_tensor_1_shape: [2,3]"
contains "$OUT_DIR/source_manifest_report_fake_source_audit.out" "blocker_0: missing-qwen-source-manifest"
rm -rf "$QWEN_FAKE_SOURCE" "$QWEN_FAKE_MODELS"

QWEN_FAKE_MANIFEST_SOURCE="${TMPDIR:-/tmp}/yvex-source-manifest-hardening-test-$$"
QWEN_FAKE_MANIFEST_MODELS="${TMPDIR:-/tmp}/yvex-source-manifest-hardening-models-$$"
rm -rf "$QWEN_FAKE_MANIFEST_SOURCE" "$QWEN_FAKE_MANIFEST_MODELS"
mkdir -p "$QWEN_FAKE_MANIFEST_SOURCE"
printf '{}\n' > "$QWEN_FAKE_MANIFEST_SOURCE/config.json"
printf '{}\n' > "$QWEN_FAKE_MANIFEST_SOURCE/tokenizer.json"
printf '{"schema":"yvex.source_manifest.v1","family":"qwen","target":"qwen3-8b","source_artifact_class":"official-source-tensors-planned","footprint":{},"provenance":{},"native_inventory":{},"tensor_metadata":{}}\n' > "$QWEN_FAKE_MANIFEST_SOURCE/source_manifest.json"
python3 - "$QWEN_FAKE_MANIFEST_SOURCE/model-00001-of-00001.safetensors" <<'PY'
import json
import struct
import sys

header = {"token_embd.weight": {"dtype": "F32", "shape": [1, 2], "data_offsets": [0, 8]}}
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"1" * 8)
PY
run_ok source_manifest_report_fake_manifest "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_MANIFEST_SOURCE" --models-root "$QWEN_FAKE_MANIFEST_MODELS"
contains "$OUT_DIR/source_manifest_report_fake_manifest.out" "status: source-profile-incomplete"
contains "$OUT_DIR/source_manifest_report_fake_manifest.out" "manifest: present  consistency=partial"
contains "$OUT_DIR/source_manifest_report_fake_manifest.out" "top_blocker: missing-qwen-tensor-role-map"
contains "$OUT_DIR/source_manifest_report_fake_manifest.out" "next: V010.MAP.7"
run_ok source_manifest_report_fake_manifest_table "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_MANIFEST_SOURCE" --models-root "$QWEN_FAKE_MANIFEST_MODELS" --output table
matches "$OUT_DIR/source_manifest_report_fake_manifest_table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}present[[:space:]]{2,}1[[:space:]]{2,}present[[:space:]]{2,}partial[[:space:]]{2,}V010\.MAP\.7$'
run_ok source_manifest_report_fake_manifest_audit "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_FAKE_MANIFEST_SOURCE" --models-root "$QWEN_FAKE_MANIFEST_MODELS" --audit
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_expected: true"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_status: present"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_schema_status: matched"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_schema_version: yvex.source_manifest.v1"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_family: qwen"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_family_status: matched"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_target_id: qwen3-8b"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_target_status: matched"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_source_path_status: present"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_artifact_class_status: declared"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_footprint_status: declared"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_provenance_status: manifest-present"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_native_inventory_status: declared"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_tensor_metadata_status: declared"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_consistency_status: partial"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_hardening_status: report-only"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_creation_performed: false"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_remote_checked: false"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "source_manifest_hash_computed: false"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "runtime_claim: unsupported"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "generation: unsupported-full-model"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "benchmark_status: not-measured"
contains "$OUT_DIR/source_manifest_report_fake_manifest_audit.out" "release_ready: false"
rm -rf "$QWEN_FAKE_MANIFEST_SOURCE" "$QWEN_FAKE_MANIFEST_MODELS"

QWEN_BROKEN_SOURCE="${TMPDIR:-/tmp}/yvex-native-safetensors-malformed-test-$$"
QWEN_BROKEN_MODELS="${TMPDIR:-/tmp}/yvex-native-safetensors-malformed-models-$$"
rm -rf "$QWEN_BROKEN_SOURCE" "$QWEN_BROKEN_MODELS"
mkdir -p "$QWEN_BROKEN_SOURCE"
printf '{}\n' > "$QWEN_BROKEN_SOURCE/config.json"
printf '{}\n' > "$QWEN_BROKEN_SOURCE/tokenizer.json"
printf 'broken\n' > "$QWEN_BROKEN_SOURCE/broken.safetensors"
run_ok source_manifest_report_broken_safetensors "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_BROKEN_SOURCE" --models-root "$QWEN_BROKEN_MODELS"
contains "$OUT_DIR/source_manifest_report_broken_safetensors.out" "native: header-error  files=1  tensors=0  header_bytes=0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors.out" "metadata: header-error  tensors=0  dtypes=0  max_rank=0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors.out" "manifest: missing  consistency=not-checked"
contains "$OUT_DIR/source_manifest_report_broken_safetensors.out" "next: V010.MAP.7"
run_ok source_manifest_report_broken_safetensors_audit "$YVEX_BIN" source-manifest report --family qwen --release v0.1.0 --source "$QWEN_BROKEN_SOURCE" --models-root "$QWEN_BROKEN_MODELS" --audit
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_inventory_status: header-error"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_safetensors_count: 1"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_safetensors_opened: 1"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_safetensors_header_error_count: 1"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_safetensors_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_invalid_file_count: 1"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "native_inventory_error_count: 1"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "source_tensor_metadata_status: header-error"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "source_tensor_metadata_payload_bytes_read: 0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/source_manifest_report_broken_safetensors_audit.out" "source_tensor_metadata_error_count: 1"
rm -rf "$QWEN_BROKEN_SOURCE" "$QWEN_BROKEN_MODELS"

run_ok model_target_classes "$YVEX_BIN" model-target classes
contains "$OUT_DIR/model_target_classes.out" "status: model-target-classes"
contains "$OUT_DIR/model_target_classes.out" "class: selected-runtime-slice"
contains "$OUT_DIR/model_target_classes.out" "class: official-source-huge-model"
contains "$OUT_DIR/model_target_classes.out" "class: source-model-candidate"
contains "$OUT_DIR/model_target_classes.out" "class: source-model-candidate"
contains "$OUT_DIR/model_target_classes.out" "class: external-GGUF-reference"
contains "$OUT_DIR/model_target_classes.out" "capability_claim: false"
contains "$OUT_DIR/model_target_classes.out" "runtime_execution: partial-boundary-only"
contains "$OUT_DIR/model_target_classes.out" "generation: unsupported"

run_ok model_target_list "$YVEX_BIN" model-target list
contains "$OUT_DIR/model_target_list.out" "MODEL TARGETS  count=5"
matches "$OUT_DIR/model_target_list.out" '^TARGET[[:space:]]{2,}FAMILY[[:space:]]{2,}CLASS[[:space:]]{2,}RUNTIME[[:space:]]{2,}GENERATION$'
matches "$OUT_DIR/model_target_list.out" '^deepseek4-v4-flash-selected-embed[[:space:]]{2,}DeepSeek[[:space:]]{2,}selected-runtime-slice[[:space:]]{2,}unsupported[[:space:]]{2,}unsupported$'
matches "$OUT_DIR/model_target_list.out" '^qwen3-8b[[:space:]]{2,}Qwen[[:space:]]{2,}source-model-candidate[[:space:]]{2,}unsupported[[:space:]]{2,}unsupported$'
matches "$OUT_DIR/model_target_list.out" '^gemma-4-12b-it[[:space:]]{2,}Gemma[[:space:]]{2,}source-model-candidate[[:space:]]{2,}unsupported[[:space:]]{2,}unsupported$'
! grep -F 'qwen-metal-portability' "$OUT_DIR/model_target_list.out" >/dev/null
! grep -F 'gemma-dense-portability' "$OUT_DIR/model_target_list.out" >/dev/null
! grep -F 'deepseek4-v4-flash-selected-embed DeepSeek selected-runtime-slice unsupported unsupported' "$OUT_DIR/model_target_list.out" >/dev/null
contains "$OUT_DIR/model_target_list.out" "status: model-target-list"
run_ok model_target_list_table "$YVEX_BIN" model-target list --output table
contains "$OUT_DIR/model_target_list_table.out" "MODEL TARGETS  count=5"
matches "$OUT_DIR/model_target_list_table.out" '^TARGET[[:space:]]{2,}FAMILY[[:space:]]{2,}CLASS[[:space:]]{2,}RUNTIME[[:space:]]{2,}GENERATION$'
matches "$OUT_DIR/model_target_list_table.out" '^qwen3-8b[[:space:]]{2,}Qwen[[:space:]]{2,}source-model-candidate[[:space:]]{2,}unsupported[[:space:]]{2,}unsupported$'
matches "$OUT_DIR/model_target_list_table.out" '^gemma-4-12b-it[[:space:]]{2,}Gemma[[:space:]]{2,}source-model-candidate[[:space:]]{2,}unsupported[[:space:]]{2,}unsupported$'
! grep -F 'qwen-metal-portability' "$OUT_DIR/model_target_list_table.out" >/dev/null
! grep -F 'gemma-dense-portability' "$OUT_DIR/model_target_list_table.out" >/dev/null
contains "$OUT_DIR/model_target_list_table.out" "status: model-target-list"
run_ok model_target_list_audit "$YVEX_BIN" model-target list --audit
contains "$OUT_DIR/model_target_list_audit.out" "target: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_list_audit.out" "target_class: selected-runtime-slice"
contains "$OUT_DIR/model_target_list_audit.out" "target: qwen3-8b"
contains "$OUT_DIR/model_target_list_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/model_target_list_audit.out" "model_class_profile_status: command-visible"
contains "$OUT_DIR/model_target_list_audit.out" "model_class_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_list_audit.out" "model_class_evidence_basis: header-metadata-only"
contains "$OUT_DIR/model_target_list_audit.out" "model_class_pattern_status: lexical-only"
contains "$OUT_DIR/model_target_list_audit.out" "model_class_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_status: command-visible"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_family: qwen"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_stage: header-collection-inventory"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_status: not-run"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_family: qwen"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_next: V010.MAP.7"
contains "$OUT_DIR/model_target_list_audit.out" "target: gemma-4-12b-it"
contains "$OUT_DIR/model_target_list_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_family: gemma"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_list_audit.out" "tensor_collection_validation_status: lexical-and-header-only"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_family: gemma"
contains "$OUT_DIR/model_target_list_audit.out" "output_head_map_target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_list_audit.out" "source_provenance_status:"
contains "$OUT_DIR/model_target_list_audit.out" "source_origin:"
contains "$OUT_DIR/model_target_list_audit.out" "source_authority:"
contains "$OUT_DIR/model_target_list_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_list_audit.out" "source_identity_status:"
contains "$OUT_DIR/model_target_list_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_list_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_list_audit.out" "native_inventory_status:"
contains "$OUT_DIR/model_target_list_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_list_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_list_audit.out" "source_tensor_metadata_status:"
contains "$OUT_DIR/model_target_list_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_list_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/model_target_list_audit.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_list_audit.out" "generation: unsupported"
run_fail_code model_target_list_bad_output 2 "$YVEX_BIN" model-target list --output nope
contains "$OUT_DIR/model_target_list_bad_output.err" "model-target list: unsupported output mode: nope"
run_ok model_target_candidate_help "$YVEX_BIN" model-target candidate --help
contains "$OUT_DIR/model_target_candidate_help.out" "usage: yvex model-target candidate --release v0.1.0 [options]"
contains "$OUT_DIR/model_target_candidate_help.out" "The candidate report evaluates full-runtime target eligibility for a release."
contains "$OUT_DIR/model_target_candidate_help.out" "does not select a ready model"

run_ok model_target_candidate "$YVEX_BIN" model-target candidate --release v0.1.0
contains "$OUT_DIR/model_target_candidate.out" "report: model-target candidate"
contains "$OUT_DIR/model_target_candidate.out" "status: blocked-no-candidate"
contains "$OUT_DIR/model_target_candidate.out" "release: v0.1.0"
contains "$OUT_DIR/model_target_candidate.out" "selected: none"
contains "$OUT_DIR/model_target_candidate.out" "top_blocker: no eligible full-runtime candidate"
contains "$OUT_DIR/model_target_candidate.out" "next: V010.MAP.7"
contains "$OUT_DIR/model_target_candidate.out" "boundary: report-only; generation unsupported; benchmark not measured"
run_ok model_target_candidate_table "$YVEX_BIN" model-target candidate --release v0.1.0 --output table
matches "$OUT_DIR/model_target_candidate_table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$OUT_DIR/model_target_candidate_table.out" '^full-runtime-candidate[[:space:]]{2,}missing[[:space:]]{2,}none[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.7$'

run_ok model_target_candidate_full "$YVEX_BIN" model-target candidate --release v0.1.0 --audit --include-candidates --include-pressure-targets --include-blockers --include-next
contains "$OUT_DIR/model_target_candidate_full.out" "deepseek_pressure_status: selected-slice-pressure-only"
contains "$OUT_DIR/model_target_candidate_full.out" "glm_pressure_status: source-storage-pressure-only"
contains "$OUT_DIR/model_target_candidate_full.out" "qwen_metal_pressure_status: planned-portability-pressure-only"
contains "$OUT_DIR/model_target_candidate_full.out" "next_required_rows: V010.TARGET.3"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_0_id: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_0_stage: selected-slice"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_0_eligibility: selected-slice-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_0_blocker_0: selected-runtime-slice-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_1_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_1_stage: diagnostic-runtime"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_1_eligibility: selected-slice-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_2_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_2_eligibility: source-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_3_id: qwen3-8b"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_3_stage: source-target-profiled"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_3_eligibility: planned-portability-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_4_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_4_stage: source-target-profiled"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_4_eligibility: planned-dense-pressure-only"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_4_blocker_1: missing-gemma-source-path"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_5_id: tests/fixtures/gguf/valid-tokenizer-simple.gguf"
contains "$OUT_DIR/model_target_candidate_full.out" "candidate_5_eligibility: fixture-only"

run_ok model_target_candidate_rmsnorm "$YVEX_BIN" model-target candidate --release v0.1.0 --audit --target deepseek4-v4-flash-selected-embed-rmsnorm --include-blockers --include-next
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_count: 1"
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_0_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_0_stage: diagnostic-runtime"
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_0_eligibility: selected-slice-only"
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_0_blocker_0: selected-runtime-slice-only"
contains "$OUT_DIR/model_target_candidate_rmsnorm.out" "candidate_0_next_required_rows: V010.TARGET.3,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6"

run_fail_code model_target_candidate_missing_release 2 "$YVEX_BIN" model-target candidate
contains "$OUT_DIR/model_target_candidate_missing_release.err" "model-target candidate: --release is required"
run_fail_code model_target_candidate_bad_release 2 "$YVEX_BIN" model-target candidate --release nope
contains "$OUT_DIR/model_target_candidate_bad_release.out" "status: unsupported-release"
run_fail_code model_target_candidate_bad_output 2 "$YVEX_BIN" model-target candidate --release v0.1.0 --output nope
contains "$OUT_DIR/model_target_candidate_bad_output.err" "model-target candidate: unsupported output mode: nope"
run_fail_code model_target_candidate_unknown_flag 2 "$YVEX_BIN" model-target candidate --release v0.1.0 --unknown
contains "$OUT_DIR/model_target_candidate_unknown_flag.err" "model-target candidate: unknown option: --unknown"
run_fail_code model_target_candidate_missing_release_value 2 "$YVEX_BIN" model-target candidate --release
contains "$OUT_DIR/model_target_candidate_missing_release_value.err" "model-target candidate: --release requires VERSION"
run_fail_code model_target_candidate_missing_target_value 2 "$YVEX_BIN" model-target candidate --release v0.1.0 --target
contains "$OUT_DIR/model_target_candidate_missing_target_value.err" "model-target candidate: --target requires TARGET"
run_fail_code model_target_candidate_unknown_target 2 "$YVEX_BIN" model-target candidate --release v0.1.0 --target missing-target
contains "$OUT_DIR/model_target_candidate_unknown_target.out" "status: full-runtime-candidate-report-fail"
contains "$OUT_DIR/model_target_candidate_unknown_target.out" "target_requested: missing-target"

run_ok model_target_dense_candidate_help "$YVEX_BIN" model-target dense-candidate --help
contains "$OUT_DIR/model_target_dense_candidate_help.out" "usage: yvex model-target dense-candidate --release v0.1.0 [options]"
contains "$OUT_DIR/model_target_dense_candidate_help.out" "The dense-candidate report evaluates whether a dense model target can become the first v0.1.0 full-runtime candidate."
contains "$OUT_DIR/model_target_dense_candidate_help.out" "does not download weights, emit artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready"

run_ok model_target_dense_candidate "$YVEX_BIN" model-target dense-candidate --release v0.1.0
contains "$OUT_DIR/model_target_dense_candidate.out" "report: model-target dense-candidate"
contains "$OUT_DIR/model_target_dense_candidate.out" "status: dense-candidate-missing"
contains "$OUT_DIR/model_target_dense_candidate.out" "release: v0.1.0"
contains "$OUT_DIR/model_target_dense_candidate.out" "selected: none"
contains "$OUT_DIR/model_target_dense_candidate.out" "top_blocker: no selected dense full-runtime candidate"
contains "$OUT_DIR/model_target_dense_candidate.out" "next: V010.MAP.7"
contains "$OUT_DIR/model_target_dense_candidate.out" "boundary: report-only; generation unsupported; benchmark not measured"
run_ok model_target_dense_candidate_table "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --output table
matches "$OUT_DIR/model_target_dense_candidate_table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$OUT_DIR/model_target_dense_candidate_table.out" '^dense-candidate[[:space:]]{2,}missing[[:space:]]{2,}none[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.7$'

run_ok model_target_dense_candidate_full "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --audit --include-candidates --include-requirements --include-blockers --include-next
contains "$OUT_DIR/model_target_dense_candidate_full.out" "next_required_rows: V010.TARGET.7"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_0_id: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_0_stage: selected-slice"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_0_eligibility: not-dense-target"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_0_blocker_1: selected-runtime-slice-only"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_0_required_role_5: dense-mlp"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_1_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_1_stage: diagnostic-runtime"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_1_eligibility: not-dense-target"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_2_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_2_blocker_0: moe-target"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_2_blocker_1: source-only-target"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_3_id: qwen3-8b"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_3_stage: source-target-profiled"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_3_eligibility: dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_3_blocker_0: planned-portability-only"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_3_blocker_1: missing-qwen-source-path"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_4_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_4_stage: source-target-profiled"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_4_eligibility: dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_4_blocker_0: planned-dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_4_blocker_1: missing-gemma-source-path"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_5_id: tests/fixtures/gguf/valid-tokenizer-simple.gguf"
contains "$OUT_DIR/model_target_dense_candidate_full.out" "dense_candidate_5_eligibility: fixture-only"

run_ok model_target_dense_candidate_rmsnorm "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --audit --target deepseek4-v4-flash-selected-embed-rmsnorm --include-blockers --include-next
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_count: 1"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_stage: diagnostic-runtime"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_eligibility: not-dense-target"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_blocker_0: not-dense-target"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_blocker_1: selected-runtime-slice-only"
contains "$OUT_DIR/model_target_dense_candidate_rmsnorm.out" "dense_candidate_0_next_required_rows: V010.TARGET.7,V010.TARGET.4,V010.MAP.2,V010.FULLMODEL.6"

run_ok model_target_dense_candidate_qwen "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --audit --target qwen3-8b --include-blockers --include-next
contains "$OUT_DIR/model_target_dense_candidate_qwen.out" "dense_candidate_status: candidate-incomplete"
contains "$OUT_DIR/model_target_dense_candidate_qwen.out" "dense_candidate_0_stage: source-target-profiled"
contains "$OUT_DIR/model_target_dense_candidate_qwen.out" "dense_candidate_0_eligibility: dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_qwen.out" "dense_candidate_0_blocker_0: planned-portability-only"
contains "$OUT_DIR/model_target_dense_candidate_qwen.out" "dense_candidate_0_blocker_1: missing-qwen-source-path"
run_ok model_target_dense_candidate_gemma "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --audit --target gemma-4-12b-it --include-blockers --include-next
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_status: candidate-incomplete"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_class: source-model-candidate"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_stage: source-target-profiled"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_eligibility: dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_blocker_0: planned-dense-pressure-only"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_blocker_1: missing-gemma-source-path"
contains "$OUT_DIR/model_target_dense_candidate_gemma.out" "dense_candidate_0_next_required_rows: V010.TARGET.7,V010.MAP.7"

run_fail_code model_target_dense_candidate_missing_release 2 "$YVEX_BIN" model-target dense-candidate
contains "$OUT_DIR/model_target_dense_candidate_missing_release.err" "model-target dense-candidate: --release is required"
run_fail_code model_target_dense_candidate_bad_release 2 "$YVEX_BIN" model-target dense-candidate --release nope
contains "$OUT_DIR/model_target_dense_candidate_bad_release.out" "status: unsupported-release"
run_fail_code model_target_dense_candidate_unknown_flag 2 "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --unknown
contains "$OUT_DIR/model_target_dense_candidate_unknown_flag.err" "model-target dense-candidate: unknown option: --unknown"
run_fail_code model_target_dense_candidate_missing_release_value 2 "$YVEX_BIN" model-target dense-candidate --release
contains "$OUT_DIR/model_target_dense_candidate_missing_release_value.err" "model-target dense-candidate: --release requires VERSION"
run_fail_code model_target_dense_candidate_missing_target_value 2 "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --target
contains "$OUT_DIR/model_target_dense_candidate_missing_target_value.err" "model-target dense-candidate: --target requires TARGET"
run_fail_code model_target_dense_candidate_unknown_target 2 "$YVEX_BIN" model-target dense-candidate --release v0.1.0 --target missing-target
contains "$OUT_DIR/model_target_dense_candidate_unknown_target.out" "status: dense-candidate-report-fail"
contains "$OUT_DIR/model_target_dense_candidate_unknown_target.out" "target_requested: missing-target"

run_ok model_target_qwen_metal_help "$YVEX_BIN" model-target qwen-metal --help
contains "$OUT_DIR/model_target_qwen_metal_help.out" "usage: yvex model-target qwen-metal --release v0.1.0 [options]"
contains "$OUT_DIR/model_target_qwen_metal_help.out" "The Qwen/Metal pressure report records a planned reduced-scale Apple Silicon / Metal lane for future full-runtime work."
contains "$OUT_DIR/model_target_qwen_metal_help.out" "does not download weights, implement Metal, emit Qwen artifacts, materialize tensors, execute graph/runtime paths, generate, evaluate, benchmark, or mark a release ready"

run_ok model_target_qwen_metal "$YVEX_BIN" model-target qwen-metal --release v0.1.0
contains "$OUT_DIR/model_target_qwen_metal.out" "report: model-target qwen-metal"
contains "$OUT_DIR/model_target_qwen_metal.out" "status: pressure-target-only"
contains "$OUT_DIR/model_target_qwen_metal.out" "release: v0.1.0"
contains "$OUT_DIR/model_target_qwen_metal.out" "lane: qwen-metal / apple-silicon-metal"
contains "$OUT_DIR/model_target_qwen_metal.out" "target: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_metal.out" "candidate: source-target-profiled pressure-target-only"
contains "$OUT_DIR/model_target_qwen_metal.out" "source_target: profiled"
contains "$OUT_DIR/model_target_qwen_metal.out" "source: missing"
contains "$OUT_DIR/model_target_qwen_metal.out" "backend: metal unsupported"
contains "$OUT_DIR/model_target_qwen_metal.out" "next: V010.MAP.7"
contains "$OUT_DIR/model_target_qwen_metal.out" "boundary: report-only; generation unsupported; benchmark not measured"
run_ok model_target_qwen_metal_table "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --output table
matches "$OUT_DIR/model_target_qwen_metal_table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$OUT_DIR/model_target_qwen_metal_table.out" '^qwen-metal-pressure[[:space:]]{2,}pressure[[:space:]]{2,}none[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.7$'

run_ok model_target_qwen_metal_full "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --audit --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_count: 3"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_id: qwen-small"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_class: backend-compatibility-pressure"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_stage: report-only"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_eligibility: pressure-target-only"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_source_target_status: pending"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_backend_status: unsupported"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_runtime_status: unsupported"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_0_generation_status: unsupported-full-model"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_1_id: qwen-medium"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_2_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_2_stage: source-target-profiled"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "qwen_candidate_2_source_target_status: profiled"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "candidate_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "candidate_stage: source-target-profiled"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "source_target_status: profiled"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "hardware_profile_status: planned"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "machine_profile_required: true"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "unified_memory_report_required: true"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "metal_device_report_required: true"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "metal_feasibility_status: missing"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "metal_allocation_status: unsupported"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "metal_graph_primitive_status: unsupported"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "cuda_lane_independent: true"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "source_family: qwen"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "source_target_status: profiled"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "source_manifest_status: missing"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "native_tensor_inventory_status: missing"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "source_config_status: missing"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "model_class_profile_status: command-visible"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_0: missing-qwen-source-path"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_1: missing-qwen-source-manifest"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_10: missing-metal-backend-feasibility"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_17: missing-real-prefill"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_20: missing-real-output-head-logits"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "blocker_21: missing-real-vocabulary-sampling"
contains "$OUT_DIR/model_target_qwen_metal_full.out" "next_required_rows: V010.MAP.7"

run_ok model_target_qwen_metal_small "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --audit --target qwen-small --include-blockers --include-next
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_count: 1"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_0_id: qwen-small"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_0_source_target_status: pending"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_0_blocker_0: missing-qwen-source-path"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_0_blocker_6: missing-metal-backend-feasibility"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "qwen_candidate_0_blocker_7: missing-real-prefill"
contains "$OUT_DIR/model_target_qwen_metal_small.out" "next_required_rows: V010.MAP.7"

run_fail_code model_target_qwen_metal_missing_release 2 "$YVEX_BIN" model-target qwen-metal
contains "$OUT_DIR/model_target_qwen_metal_missing_release.err" "model-target qwen-metal: --release is required"
run_fail_code model_target_qwen_metal_bad_release 2 "$YVEX_BIN" model-target qwen-metal --release nope
contains "$OUT_DIR/model_target_qwen_metal_bad_release.out" "status: unsupported-release"
run_fail_code model_target_qwen_metal_unknown_flag 2 "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --unknown
contains "$OUT_DIR/model_target_qwen_metal_unknown_flag.err" "model-target qwen-metal: unknown option: --unknown"
run_fail_code model_target_qwen_metal_missing_release_value 2 "$YVEX_BIN" model-target qwen-metal --release
contains "$OUT_DIR/model_target_qwen_metal_missing_release_value.err" "model-target qwen-metal: --release requires VERSION"
run_fail_code model_target_qwen_metal_missing_target_value 2 "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --target
contains "$OUT_DIR/model_target_qwen_metal_missing_target_value.err" "model-target qwen-metal: --target requires TARGET"
run_fail_code model_target_qwen_metal_unknown_target 2 "$YVEX_BIN" model-target qwen-metal --release v0.1.0 --target missing-target
contains "$OUT_DIR/model_target_qwen_metal_unknown_target.out" "status: qwen-metal-pressure-report-fail"
contains "$OUT_DIR/model_target_qwen_metal_unknown_target.out" "target_requested: missing-target"

run_ok model_target_deepseek_embed "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed
contains "$OUT_DIR/model_target_deepseek_embed.out" "status: model-target"
contains "$OUT_DIR/model_target_deepseek_embed.out" "target: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_deepseek_embed.out" "family: DeepSeek  class=selected-runtime-slice"
contains "$OUT_DIR/model_target_deepseek_embed.out" "source: official-safetensors  status=unknown"
contains "$OUT_DIR/model_target_deepseek_embed.out" "artifact: YVEX-produced-selected-GGUF  status=present"
contains "$OUT_DIR/model_target_deepseek_embed.out" "runtime: selected-boundary-only"
contains "$OUT_DIR/model_target_deepseek_embed.out" "generation: unsupported"
contains "$OUT_DIR/model_target_deepseek_embed.out" "boundary: selected-slice only; no full-runtime generation"

run_ok model_target_deepseek_embed_audit "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --audit
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "target_id: deepseek4-v4-flash-selected-embed"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_provenance_status: unknown"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_identity_status: not-verified"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "native_inventory_status: unknown"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_tensor_metadata_status: unknown"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_deepseek_embed_audit.out" "source_tensor_metadata_payload_loaded: false"

run_ok model_target_deepseek_rmsnorm "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "target: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "family: DeepSeek  class=selected-runtime-slice"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "source: official-safetensors  status=unknown"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "artifact: YVEX-produced-selected-GGUF  status=present"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "runtime: selected-segment-boundary-only"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "generation: unsupported"
contains "$OUT_DIR/model_target_deepseek_rmsnorm.out" "status: model-target"

run_ok model_target_deepseek_rmsnorm_audit "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --audit
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "target_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_provenance_status: unknown"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_identity_status: not-verified"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "native_inventory_status: unknown"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_tensor_metadata_status: unknown"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_deepseek_rmsnorm_audit.out" "source_tensor_metadata_payload_loaded: false"

run_ok model_target_glm "$YVEX_BIN" model-target inspect glm-5.2-official-safetensors
contains "$OUT_DIR/model_target_glm.out" "target: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_glm.out" "family: GLM  class=official-source-huge-model"
contains "$OUT_DIR/model_target_glm.out" "source: official-safetensors-huge  status=planned"
contains "$OUT_DIR/model_target_glm.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/model_target_glm.out" "runtime: unsupported"
contains "$OUT_DIR/model_target_glm.out" "generation: unsupported"
contains "$OUT_DIR/model_target_glm.out" "next: V010.SOURCE.8"
contains "$OUT_DIR/model_target_glm.out" "boundary: source/storage pressure only; no GLM runtime/generation"

run_ok model_target_glm_audit "$YVEX_BIN" model-target inspect glm-5.2-official-safetensors --audit
contains "$OUT_DIR/model_target_glm_audit.out" "target_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_glm_audit.out" "source_provenance_status: planned"
contains "$OUT_DIR/model_target_glm_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/model_target_glm_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/model_target_glm_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_glm_audit.out" "source_identity_status: not-verified"
contains "$OUT_DIR/model_target_glm_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_glm_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_glm_audit.out" "native_inventory_status: planned"
contains "$OUT_DIR/model_target_glm_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_glm_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_glm_audit.out" "source_tensor_metadata_status: planned"
contains "$OUT_DIR/model_target_glm_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_glm_audit.out" "source_tensor_metadata_payload_loaded: false"

run_ok model_target_qwen "$YVEX_BIN" model-target inspect qwen3-8b
contains "$OUT_DIR/model_target_qwen.out" "target: qwen3-8b"
contains "$OUT_DIR/model_target_qwen.out" "family: Qwen  class=source-model-candidate"
contains "$OUT_DIR/model_target_qwen.out" "source: official-source-tensors-planned  status=missing"
contains "$OUT_DIR/model_target_qwen.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/model_target_qwen.out" "runtime: unsupported"
contains "$OUT_DIR/model_target_qwen.out" "generation: unsupported"
contains "$OUT_DIR/model_target_qwen.out" "next: V010.MAP.7"
contains "$OUT_DIR/model_target_qwen.out" "boundary: target/source profile only; no source download/runtime/generation"

run_fail_code model_target_old_qwen 2 "$YVEX_BIN" model-target inspect qwen-metal-portability
contains "$OUT_DIR/model_target_old_qwen.err" "unknown target: qwen-metal-portability"

run_ok model_target_qwen_audit "$YVEX_BIN" model-target inspect qwen3-8b --audit
contains "$OUT_DIR/model_target_qwen_audit.out" "target_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_audit.out" "family: Qwen"
contains "$OUT_DIR/model_target_qwen_audit.out" "model: Qwen3-8B"
contains "$OUT_DIR/model_target_qwen_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_provenance_status: missing"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_identity_status: not-present"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_qwen_audit.out" "native_inventory_status: missing"
contains "$OUT_DIR/model_target_qwen_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_qwen_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_tensor_metadata_status: missing"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_qwen_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_profile_status: command-visible"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_runtime_shape: causal-decoder-candidate-pending-config"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_evidence_basis: header-metadata-only"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_pattern_status: lexical-only"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_qwen_audit.out" "model_class_runtime_status: unsupported"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_status: command-visible"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_family: qwen"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_stage: header-collection-inventory"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_evidence_basis: header-metadata-only"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_validation_status: lexical-and-header-only"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_runtime_descriptor_status: not-implemented"
contains "$OUT_DIR/model_target_qwen_audit.out" "tensor_collection_graph_consumer_status: not-implemented"
contains "$OUT_DIR/model_target_qwen_audit.out" "output_head_map_status: not-run"
contains "$OUT_DIR/model_target_qwen_audit.out" "output_head_map_family: qwen"
contains "$OUT_DIR/model_target_qwen_audit.out" "output_head_map_target_id: qwen3-8b"
contains "$OUT_DIR/model_target_qwen_audit.out" "output_head_map_stage: header-output-head-map"
contains "$OUT_DIR/model_target_qwen_audit.out" "output_head_map_next: V010.MAP.7"
contains "$OUT_DIR/model_target_qwen_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_qwen_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/model_target_qwen_audit.out" "benchmark_status: not-measured"
contains "$OUT_DIR/model_target_qwen_audit.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_qwen_audit.out" "generation: unsupported"

run_ok model_target_gemma "$YVEX_BIN" model-target inspect gemma-4-12b-it
contains "$OUT_DIR/model_target_gemma.out" "target: gemma-4-12b-it"
contains "$OUT_DIR/model_target_gemma.out" "family: Gemma  class=source-model-candidate"
contains "$OUT_DIR/model_target_gemma.out" "source: official-source-tensors-planned  status=missing"
contains "$OUT_DIR/model_target_gemma.out" "artifact: future-YVEX-produced-GGUF  status=planned"
contains "$OUT_DIR/model_target_gemma.out" "runtime: unsupported"
contains "$OUT_DIR/model_target_gemma.out" "generation: unsupported"
contains "$OUT_DIR/model_target_gemma.out" "next: V010.MAP.7"
contains "$OUT_DIR/model_target_gemma.out" "boundary: target/source profile only; no source download/runtime/generation"

run_fail_code model_target_old_gemma 2 "$YVEX_BIN" model-target inspect gemma-dense-portability
contains "$OUT_DIR/model_target_old_gemma.err" "unknown target: gemma-dense-portability"

run_ok model_target_gemma_audit "$YVEX_BIN" model-target inspect gemma-4-12b-it --audit
contains "$OUT_DIR/model_target_gemma_audit.out" "target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_gemma_audit.out" "family: Gemma"
contains "$OUT_DIR/model_target_gemma_audit.out" "model: Gemma-4-12B-it"
contains "$OUT_DIR/model_target_gemma_audit.out" "target_class: source-model-candidate"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_provenance_status: missing"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_origin: planned-official"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_authority: upstream-official-planned"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_revision_status: unknown"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_identity_status: not-present"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_hash_status: not-computed"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_verification_status: not-verified"
contains "$OUT_DIR/model_target_gemma_audit.out" "native_inventory_status: missing"
contains "$OUT_DIR/model_target_gemma_audit.out" "native_tensor_count: 0"
contains "$OUT_DIR/model_target_gemma_audit.out" "native_safetensors_payload_loaded: false"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_tensor_metadata_status: missing"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_tensor_count: 0"
contains "$OUT_DIR/model_target_gemma_audit.out" "source_tensor_metadata_payload_loaded: false"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_profile_status: command-visible"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_runtime_shape: dense-causal-decoder-candidate-pending-config"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_evidence_basis: header-metadata-only"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_pattern_status: lexical-only"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_gemma_audit.out" "model_class_runtime_status: unsupported"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_status: command-visible"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_family: gemma"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_stage: header-collection-inventory"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_evidence_basis: header-metadata-only"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_validation_status: lexical-and-header-only"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_role_mapping_status: not-implemented"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_runtime_descriptor_status: not-implemented"
contains "$OUT_DIR/model_target_gemma_audit.out" "tensor_collection_graph_consumer_status: not-implemented"
contains "$OUT_DIR/model_target_gemma_audit.out" "output_head_map_status: not-run"
contains "$OUT_DIR/model_target_gemma_audit.out" "output_head_map_family: gemma"
contains "$OUT_DIR/model_target_gemma_audit.out" "output_head_map_target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_gemma_audit.out" "output_head_map_stage: header-output-head-map"
contains "$OUT_DIR/model_target_gemma_audit.out" "output_head_map_next: V010.MAP.7"
contains "$OUT_DIR/model_target_gemma_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_gemma_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/model_target_gemma_audit.out" "benchmark_status: not-measured"
contains "$OUT_DIR/model_target_gemma_audit.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_gemma_audit.out" "generation: unsupported"

run_ok model_target_help_subcommand "$YVEX_BIN" model-target help
contains "$OUT_DIR/model_target_help_subcommand.out" "Model targets are pressure objects, not capability claims."

MODEL_TARGET_PATHS_DIR="$OUT_DIR/model-target-paths"
MODEL_TARGET_MODELS_ROOT="$(pwd)/$MODEL_TARGET_PATHS_DIR/models"
rm -rf "$MODEL_TARGET_PATHS_DIR"
mkdir -p "$MODEL_TARGET_PATHS_DIR/models"

run_ok model_target_paths_config "$YVEX_BIN" paths --project "$MODEL_TARGET_PATHS_DIR" configure --models-root "$MODEL_TARGET_PATHS_DIR/models" --create
contains "$OUT_DIR/model_target_paths_config.out" "status: paths-configured"
contains "$OUT_DIR/model_target_paths_config.out" "models_root_source: explicit"

run_ok model_target_paths_embed "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root "$MODEL_TARGET_PATHS_DIR/models" --audit
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
contains "$OUT_DIR/model_target_paths_embed.out" "source_artifact_class: official-safetensors"
contains "$OUT_DIR/model_target_paths_embed.out" "target_artifact_class: YVEX-produced-selected-GGUF"
contains "$OUT_DIR/model_target_paths_embed.out" "runtime_execution: selected-boundary-only"
contains "$OUT_DIR/model_target_paths_embed.out" "generation: unsupported"

run_ok model_target_paths_rmsnorm "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths --models-root "$MODEL_TARGET_PATHS_DIR/models" --audit
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "target_id: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/deepseek/DeepSeek-V4-Flash"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "registry_alias: deepseek4-v4-flash-selected-embed-rmsnorm"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "source_artifact_class: official-safetensors"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "target_artifact_class: YVEX-produced-selected-GGUF"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "runtime_execution: selected-segment-boundary-only"
contains "$OUT_DIR/model_target_paths_rmsnorm.out" "generation: unsupported"

run_ok model_target_paths_glm "$YVEX_BIN" model-target inspect glm-5.2-official-safetensors --paths --models-root "$MODEL_TARGET_PATHS_DIR/models" --audit
contains "$OUT_DIR/model_target_paths_glm.out" "target_id: glm-5.2-official-safetensors"
contains "$OUT_DIR/model_target_paths_glm.out" "models_root_source: explicit"
contains "$OUT_DIR/model_target_paths_glm.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/glm/GLM-5.2"
contains "$OUT_DIR/model_target_paths_glm.out" "artifact_path: planned"
contains "$OUT_DIR/model_target_paths_glm.out" "artifact_exists: false"
contains "$OUT_DIR/model_target_paths_glm.out" "report_dir: $MODEL_TARGET_MODELS_ROOT/reports/glm"
contains "$OUT_DIR/model_target_paths_glm.out" "reference_dir: $MODEL_TARGET_MODELS_ROOT/reference/glm"
contains "$OUT_DIR/model_target_paths_glm.out" "registry_dir: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_glm.out" "registry_alias: none"
contains "$OUT_DIR/model_target_paths_glm.out" "source_artifact_class: official-safetensors-huge"
contains "$OUT_DIR/model_target_paths_glm.out" "source_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_glm.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_paths_glm.out" "target_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_glm.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_paths_glm.out" "generation: unsupported"

run_ok model_target_paths_qwen "$YVEX_BIN" model-target inspect qwen3-8b --paths --models-root "$MODEL_TARGET_PATHS_DIR/models"
contains "$OUT_DIR/model_target_paths_qwen.out" "target: qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen.out" "source: missing  $MODEL_TARGET_MODELS_ROOT/hf/qwen/qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen.out" "source_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_paths_qwen.out" "artifact: planned  $MODEL_TARGET_MODELS_ROOT/gguf/qwen/qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen.out" "artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_paths_qwen.out" "reports: $MODEL_TARGET_MODELS_ROOT/reports/qwen"
contains "$OUT_DIR/model_target_paths_qwen.out" "registry: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_qwen.out" "boundary: path report only, no runtime execution"

run_ok model_target_paths_qwen_audit "$YVEX_BIN" model-target inspect qwen3-8b --paths --models-root "$MODEL_TARGET_PATHS_DIR/models" --audit
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "target_id: qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "models_root_source: explicit"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/qwen/qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/qwen/qwen3-8b"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "report_dir: $MODEL_TARGET_MODELS_ROOT/reports/qwen"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "reference_dir: $MODEL_TARGET_MODELS_ROOT/reference/qwen"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "registry_dir: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "source_tensor_payload_status: not-present"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "yvex_produced_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_paths_qwen_audit.out" "generation: unsupported"

run_ok model_target_paths_gemma "$YVEX_BIN" model-target inspect gemma-4-12b-it --paths --models-root "$MODEL_TARGET_PATHS_DIR/models"
contains "$OUT_DIR/model_target_paths_gemma.out" "target: gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma.out" "source: missing  $MODEL_TARGET_MODELS_ROOT/hf/gemma/gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma.out" "source_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_paths_gemma.out" "artifact: planned  $MODEL_TARGET_MODELS_ROOT/gguf/gemma/gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma.out" "artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_paths_gemma.out" "reports: $MODEL_TARGET_MODELS_ROOT/reports/gemma"
contains "$OUT_DIR/model_target_paths_gemma.out" "registry: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_gemma.out" "boundary: path report only, no runtime execution"

run_ok model_target_paths_gemma_audit "$YVEX_BIN" model-target inspect gemma-4-12b-it --paths --models-root "$MODEL_TARGET_PATHS_DIR/models" --audit
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "target_id: gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "models_root_source: explicit"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "source_path: $MODEL_TARGET_MODELS_ROOT/hf/gemma/gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/gemma/gemma-4-12b-it"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "report_dir: $MODEL_TARGET_MODELS_ROOT/reports/gemma"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "reference_dir: $MODEL_TARGET_MODELS_ROOT/reference/gemma"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "registry_dir: $MODEL_TARGET_MODELS_ROOT/registry"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "source_artifact_class: official-source-tensors-planned"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "source_artifact_status: missing"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "source_tensor_payload_status: not-present"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "target_artifact_class: future-YVEX-produced-GGUF"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "target_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "yvex_produced_artifact_status: planned"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "runtime_execution: unsupported"
contains "$OUT_DIR/model_target_paths_gemma_audit.out" "generation: unsupported"

(
    MODEL_TARGET_ENV_PROJECT="$MODEL_TARGET_PATHS_DIR/env-project"
    OUT_DIR_ABS="$(pwd)/$OUT_DIR"
    case "$YVEX_BIN" in
        /*) YVEX_BIN_ABS="$YVEX_BIN" ;;
        *) YVEX_BIN_ABS="$(pwd)/$YVEX_BIN" ;;
    esac
    mkdir -p "$MODEL_TARGET_ENV_PROJECT"
    cd "$MODEL_TARGET_ENV_PROJECT"
    YVEX_MODELS_ROOT="$MODEL_TARGET_MODELS_ROOT" "$YVEX_BIN_ABS" model-target inspect deepseek4-v4-flash-selected-embed --paths --audit >"$OUT_DIR_ABS/model_target_paths_env.out" 2>"$OUT_DIR_ABS/model_target_paths_env.err"
)
contains "$OUT_DIR/model_target_paths_env.out" "models_root_source: environment"
contains "$OUT_DIR/model_target_paths_env.out" "models_root: $MODEL_TARGET_MODELS_ROOT"
contains "$OUT_DIR/model_target_paths_env.out" "artifact_path: $MODEL_TARGET_MODELS_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"

run_fail model_target_paths_empty_root "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root ""
run_fail model_target_paths_missing_root "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root
run_fail model_target_paths_root_without_paths "$YVEX_BIN" model-target inspect deepseek4-v4-flash-selected-embed --models-root "$MODEL_TARGET_PATHS_DIR/models"

run_fail_code prefill_layer_without_layers 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layer-hidden-dim 8
run_fail_code prefill_layers_zero 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 0
run_fail_code prefill_layers_too_many 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 17
run_fail_code prefill_layer_partial_dims 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 2 --layer-hidden-dim 8 --layer-head-dim 4
run_fail_code prefill_chunk_size_zero 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --chunk-size 0
run_fail_code prefill_position_start_invalid 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --position-start nope
run_fail_code prefill_context_length_zero 2 "$YVEX_BIN" prefill --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --context-length 0

run_fail_code decode_missing_model 2 "$YVEX_BIN" decode --backend cpu --segment embedding-rmsnorm --tokens 0,1
run_fail_code decode_wrong_segment 2 "$YVEX_BIN" decode --model missing --backend cpu --segment nope --tokens 0,1
run_fail_code decode_missing_tokens 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm
run_fail_code decode_context_length_zero 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --context-length 0
run_fail_code decode_position_start_invalid 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --position-start nope
run_fail_code decode_chunk_size_zero 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --chunk-size 0
run_fail_code decode_layers_zero 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 0
run_fail_code decode_layers_too_many 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 17
run_fail_code decode_layer_without_layers 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layer-hidden-dim 8
run_fail_code decode_layer_partial_dims 2 "$YVEX_BIN" decode --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 2 --layer-hidden-dim 8 --layer-head-dim 4

run_fail_code logits_missing_model 2 "$YVEX_BIN" logits --backend cpu --segment embedding-rmsnorm --tokens 0,1
run_fail_code logits_wrong_segment 2 "$YVEX_BIN" logits --model missing --backend cpu --segment nope --tokens 0,1
run_fail_code logits_missing_tokens 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm
run_fail_code logits_count_zero 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --logits-count 0
run_fail_code logits_count_too_many 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --logits-count 257
run_fail_code logits_context_length_zero 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --context-length 0
run_fail_code logits_position_start_invalid 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --position-start nope
run_fail_code logits_chunk_size_zero 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --chunk-size 0
run_fail_code logits_layers_zero 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 0
run_fail_code logits_layers_too_many 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 17
run_fail_code logits_layer_without_layers 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layer-hidden-dim 8
run_fail_code logits_layer_partial_dims 2 "$YVEX_BIN" logits --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 2 --layer-hidden-dim 8 --layer-head-dim 4

run_fail_code sample_missing_model 2 "$YVEX_BIN" sample --backend cpu --segment embedding-rmsnorm --tokens 0,1
run_fail_code sample_wrong_segment 2 "$YVEX_BIN" sample --model missing --backend cpu --segment nope --tokens 0,1
run_fail_code sample_missing_tokens 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm
run_fail_code sample_strategy_stochastic 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --strategy stochastic
run_fail_code sample_logits_count_zero 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --logits-count 0
run_fail_code sample_logits_count_too_many 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --logits-count 257
run_fail_code sample_context_length_zero 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --context-length 0
run_fail_code sample_position_start_invalid 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --position-start nope
run_fail_code sample_chunk_size_zero 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --chunk-size 0
run_fail_code sample_layers_zero 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 0
run_fail_code sample_layers_too_many 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 17
run_fail_code sample_layer_without_layers 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layer-hidden-dim 8
run_fail_code sample_layer_partial_dims 2 "$YVEX_BIN" sample --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 2 --layer-hidden-dim 8 --layer-head-dim 4

run_fail_code generate_no_args 2 "$YVEX_BIN" generate
contains "$OUT_DIR/generate_no_args.err" "error: generate requires --model FILE_OR_ALIAS"
contains "$OUT_DIR/generate_no_args.err" "usage: yvex generate --model FILE_OR_ALIAS"
run_fail_code generate_missing_model 2 "$YVEX_BIN" generate --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1
contains "$OUT_DIR/generate_missing_model.err" "error: generate requires --model FILE_OR_ALIAS"
run_fail_code generate_bad_backend 2 "$YVEX_BIN" generate --model missing --backend tpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1
contains "$OUT_DIR/generate_bad_backend.err" "error: --backend supports cpu|cuda for bounded diagnostics"
run_fail_code generate_wrong_segment 2 "$YVEX_BIN" generate --model missing --backend cpu --segment nope --tokens 0,1 --max-new-tokens 1
contains "$OUT_DIR/generate_wrong_segment.err" "error: generate segment currently supports embedding-rmsnorm for bounded diagnostics"
run_fail_code generate_missing_tokens 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --max-new-tokens 1
contains "$OUT_DIR/generate_missing_tokens.err" "error: generate requires --tokens IDS"
run_fail_code generate_missing_max_new_tokens 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1
contains "$OUT_DIR/generate_missing_max_new_tokens.err" "error: generate requires --max-new-tokens N"
run_fail_code generate_max_new_tokens_invalid 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens nope
contains "$OUT_DIR/generate_max_new_tokens_invalid.err" "error: --max-new-tokens must be an integer greater than 0"
run_fail_code generate_max_new_tokens_zero 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 0
contains "$OUT_DIR/generate_max_new_tokens_zero.err" "error: --max-new-tokens must be an integer greater than 0"
run_fail_code generate_max_new_tokens_negative 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens -1
contains "$OUT_DIR/generate_max_new_tokens_negative.err" "error: --max-new-tokens must be an integer greater than 0"
run_fail_code generate_strategy_stochastic 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --strategy stochastic
contains "$OUT_DIR/generate_strategy_stochastic.err" "error: --strategy currently supports greedy only"
run_fail_code generate_trace_level_invalid 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --trace-level nope
contains "$OUT_DIR/generate_trace_level_invalid.err" "error: --trace-level requires none|tokens|steps|kv|logits|sampling|full"
run_fail_code generate_output_invalid 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --output nope
contains "$OUT_DIR/generate_output_invalid.err" "error: unsupported output mode: nope"
run_fail_code generate_cancel_after_steps_missing 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --cancel-after-steps
contains "$OUT_DIR/generate_cancel_after_steps_missing.err" "error: --cancel-after-steps must be a non-negative integer"
run_fail_code generate_cancel_after_steps_invalid 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --cancel-after-steps nope
contains "$OUT_DIR/generate_cancel_after_steps_invalid.err" "error: --cancel-after-steps must be a non-negative integer"
run_fail_code generate_cancel_after_steps_negative 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --cancel-after-steps -1
contains "$OUT_DIR/generate_cancel_after_steps_negative.err" "error: --cancel-after-steps must be a non-negative integer"
run_fail_code generate_cancel_after_steps_overflow 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --cancel-after-steps 18446744073709551616
contains "$OUT_DIR/generate_cancel_after_steps_overflow.err" "error: --cancel-after-steps must be a non-negative integer"
run_fail_code generate_logits_count_zero 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --logits-count 0
contains "$OUT_DIR/generate_logits_count_zero.err" "error: --logits-count requires 1 <= N <= 256"
run_fail_code generate_logits_count_too_many 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --logits-count 257
contains "$OUT_DIR/generate_logits_count_too_many.err" "error: --logits-count requires 1 <= N <= 256"
run_fail_code generate_context_length_zero 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --context-length 0
contains "$OUT_DIR/generate_context_length_zero.err" "error: --context-length requires a positive integer"
run_fail_code generate_context_length_negative 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --context-length -1
contains "$OUT_DIR/generate_context_length_negative.err" "error: --context-length requires a positive integer"
run_fail_code generate_position_start_invalid 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --position-start nope
run_fail_code generate_chunk_size_zero 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --chunk-size 0
run_fail_code generate_layers_zero 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --layers 0
run_fail_code generate_layers_too_many 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --layers 17
run_fail_code generate_layer_without_layers 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --layer-hidden-dim 8
run_fail_code generate_layer_partial_dims 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --layers 2 --layer-hidden-dim 8 --layer-head-dim 4
run_fail_code generate_unknown_flag 2 "$YVEX_BIN" generate --model missing --backend cpu --segment embedding-rmsnorm --tokens 0,1 --max-new-tokens 1 --unknown
contains "$OUT_DIR/generate_unknown_flag.err" "error: unknown generate option: --unknown"

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
contains "$OUT_DIR/help_paths.out" "yvex paths [--project DIR] resolve --family deepseek|glm|qwen|gemma --kind source|gguf|reports|reference|registry"

run_ok paths "$YVEX_BIN" paths
contains "$OUT_DIR/paths.out" "paths: normal"
contains "$OUT_DIR/paths.out" "models_root_source:"
contains "$OUT_DIR/paths.out" "models_root:"
contains "$OUT_DIR/paths.out" "hf_root:"
contains "$OUT_DIR/paths.out" "gguf_root:"
contains "$OUT_DIR/paths.out" "reports_root:"
contains "$OUT_DIR/paths.out" "registry_root:"
contains "$OUT_DIR/paths.out" "hint: use --audit for project/cache/state paths"

run_ok paths_audit "$YVEX_BIN" paths --audit
contains "$OUT_DIR/paths_audit.out" "config:"
contains "$OUT_DIR/paths_audit.out" "cache:"
contains "$OUT_DIR/paths_audit.out" "state:"
contains "$OUT_DIR/paths_audit.out" "data:"
contains "$OUT_DIR/paths_audit.out" "project:"
contains "$OUT_DIR/paths_audit.out" "status: paths"
contains "$OUT_DIR/paths_audit.out" "reference_root:"
contains "$OUT_DIR/paths_audit.out" "operator_config_path:"
run_fail_code paths_bad_output 2 "$YVEX_BIN" paths --output nope
contains "$OUT_DIR/paths_bad_output.err" "yvex paths: unsupported output mode: nope"

run_ok paths_project "$YVEX_BIN" paths --project .
contains "$OUT_DIR/paths_project.out" "paths: normal"
contains "$OUT_DIR/paths_project.out" "models_root:"

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

run_ok operator_paths_initial "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" --audit
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
contains "$OUT_DIR/operator_paths_after_config.out" "paths: normal"
contains "$OUT_DIR/operator_paths_after_config.out" "models_root_source: configured"

for family in deepseek glm qwen gemma; do
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
    "$OPERATOR_PATHS_DIR/models/hf/qwen" \
    "$OPERATOR_PATHS_DIR/models/hf/gemma" \
    "$OPERATOR_PATHS_DIR/models/gguf" \
    "$OPERATOR_PATHS_DIR/models/gguf/deepseek" \
    "$OPERATOR_PATHS_DIR/models/gguf/glm" \
    "$OPERATOR_PATHS_DIR/models/gguf/qwen" \
    "$OPERATOR_PATHS_DIR/models/gguf/gemma" \
    "$OPERATOR_PATHS_DIR/models/reports" \
    "$OPERATOR_PATHS_DIR/models/reports/deepseek" \
    "$OPERATOR_PATHS_DIR/models/reports/glm" \
    "$OPERATOR_PATHS_DIR/models/reports/qwen" \
    "$OPERATOR_PATHS_DIR/models/reports/gemma" \
    "$OPERATOR_PATHS_DIR/models/reference" \
    "$OPERATOR_PATHS_DIR/models/reference/deepseek" \
    "$OPERATOR_PATHS_DIR/models/reference/glm" \
    "$OPERATOR_PATHS_DIR/models/reference/qwen" \
    "$OPERATOR_PATHS_DIR/models/reference/gemma" \
    "$OPERATOR_PATHS_DIR/models/registry"; do
    test -d "$dir" || fail "operator path directory was not created: $dir"
done

run_ok operator_paths_create "$YVEX_BIN" paths --project "$OPERATOR_PATHS_DIR" --create --audit
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
