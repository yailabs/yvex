#!/usr/bin/env sh
set -eu

test ! -d cli
test ! -d server
test ! -d backends
test ! -d graph
test ! -d runtime
test ! -d commands
test ! -d operator
test ! -d scheduler
test ! -d executor

test -x ./yvex
test -x ./yvexd

test -f docs/topology-closure-audit.md
test -x tests/test_topology_closure_audit.sh

test -d src
test -d src/app
test -d src/cli
test -d src/cli/commands
test -d src/cli/input
test -d src/cli/render
test -d src/cli/io
test -d src/cli/catalog
test -d src/cli/schema
test -d src/io
test -d src/accounts
test -d src/core
test -d src/artifact
test -d src/backend
test -d src/backend/cuda
test -d src/daemon
test -d src/graph
test -d src/metrics
test -d src/source
test -d src/server
test -d src/gguf
test -d src/model
test -d src/tokenizer
test -d src/runtime
test -d src/generation
test -d src/eval
test -d src/bench

test -f src/cli/yvex_cli.c
test ! -f src/cli/yvex_cli_runtime.c
test -f src/cli/commands/yvex_model_target_cli.c
test ! -f src/cli/commands/yvexd_cli.c
test -f src/daemon/yvexd.c
test -f src/core/yvex_core.c
test -f src/core/yvex_fs.c
test -f src/core/yvex_operator_private.h
test -f src/core/yvex_operator_render_private.h
test -f src/cli/yvex_render_private.h
test -f src/accounts/yvex_accounts.c
test -f src/cli/commands/yvex_accounts_cli.c
test -f src/cli/commands/yvex_paths_cli.c
test -f src/model/yvex_model.c
test -f src/model/yvex_model_artifacts.c
test -d src/model/artifacts
test -f src/model/artifacts/yvex_model_artifact_registry.c
test -f src/model/artifacts/yvex_model_artifact_registry.h
test -f src/model/artifacts/yvex_model_artifact_ref.c
test -f src/model/artifacts/yvex_model_artifact_ref.h
test -f src/model/artifacts/yvex_model_artifact_gate.c
test -f src/model/artifacts/yvex_model_artifact_gate.h
test -f src/model/artifacts/yvex_model_artifact_report.c
test -f src/model/artifacts/yvex_model_artifact_report.h
test -f src/model/artifacts/yvex_model_artifact_status_report.c
test -f src/model/artifacts/yvex_model_artifact_status_report.h
test -f src/model/artifacts/yvex_model_artifact_list_report.c
test -f src/model/artifacts/yvex_model_artifact_list_report.h
test -f src/model/artifacts/yvex_model_artifact_check_report.c
test -f src/model/artifacts/yvex_model_artifact_check_report.h
test -f src/model/artifacts/yvex_model_artifact_write.c
test -f src/model/artifacts/yvex_model_artifact_write.h
test -f src/model/artifacts/yvex_model_artifact_private.h
test -f src/artifact/yvex_artifact_descriptor.c
test -f src/artifact/yvex_artifact_descriptor.h
test -f src/artifact/yvex_artifact_materialize.c
test -f src/artifact/yvex_artifact_materialize.h
test -f src/artifact/yvex_artifact_roundtrip_gate.c
test -f src/artifact/yvex_artifact_roundtrip_gate.h
test -f src/artifact/yvex_artifact_report.c
test -f src/artifact/yvex_artifact_report.h
test -f src/model/yvex_runtime_descriptor.c
test -f src/model/yvex_runtime_descriptor.h
test -f src/model/yvex_runtime_descriptor_report.c
test -f src/model/yvex_runtime_descriptor_report.h
test -f src/model/target/yvex_model_target_catalog.c
test -f src/model/target/yvex_model_target_catalog.h
test -f src/model/target/yvex_model_target_decision.c
test -f src/model/target/yvex_model_target_decision.h
test -f src/model/target/yvex_model_target_candidates.c
test -f src/model/target/yvex_model_target_candidates.h
test -f src/model/target/yvex_model_class_profile.c
test -f src/model/target/yvex_model_class_profile.h
test -f src/model/target/yvex_tensor_collection_report.c
test -f src/model/target/yvex_tensor_collection_report.h
test -f src/model/target/yvex_tensor_naming_report.c
test -f src/model/target/yvex_tensor_naming_report.h
test -f src/model/target/yvex_output_head_map_report.c
test -f src/model/target/yvex_output_head_map_report.h
test -f src/model/target/yvex_tokenizer_map_report.c
test -f src/model/target/yvex_tokenizer_map_report.h
test -f src/model/target/yvex_missing_role_report.c
test -f src/model/target/yvex_missing_role_report.h
test -f src/model/target/yvex_mapping_gate_report.c
test -f src/model/target/yvex_mapping_gate_report.h
test -f src/model/target/yvex_qtype_policy_report.c
test -f src/model/target/yvex_qtype_policy_report.h
test -f src/model/target/yvex_qtype_role_support_report.c
test -f src/model/target/yvex_qtype_role_support_report.h
test -f src/model/target/yvex_model_target_sidecar_write.c
test -f src/model/target/yvex_model_target_sidecar_write.h
test -f src/model/target/yvex_model_target_private.h
test -f src/model/target/yvex_model_target_report.c
test -f src/model/target/yvex_model_target_report.h
test -f src/cli/input/yvex_model_target_args.c
test -f src/cli/input/yvex_model_target_args.h
test -f src/cli/render/yvex_model_target_render.c
test -f src/cli/render/yvex_model_target_render.h
test -f src/cli/catalog/model_target_fields.def
test -f src/runtime/yvex_chat.c
test -f src/cli/commands/yvex_model_artifacts_cli.c
test -f src/cli/model_artifacts/yvex_model_artifacts_surface.h
test -f src/cli/model_artifacts/yvex_model_artifacts_surface_common.c
test -f src/cli/model_artifacts/yvex_model_artifacts_surface_common.h
test -f src/cli/model_artifacts/yvex_models_surface.c
test -f src/cli/model_artifacts/yvex_models_surface.h
test -f src/cli/model_artifacts/yvex_models_download_surface.c
test -f src/cli/model_artifacts/yvex_models_download_surface.h
test -f src/cli/model_artifacts/yvex_models_prepare_surface.c
test -f src/cli/model_artifacts/yvex_models_prepare_surface.h
test -f src/cli/model_artifacts/yvex_models_artifacts_surface.c
test -f src/cli/model_artifacts/yvex_models_artifacts_surface.h
test -f src/cli/model_artifacts/yvex_fullmodel_surface.c
test -f src/cli/model_artifacts/yvex_fullmodel_surface.h
test -f src/cli/model_artifacts/yvex_attention_surface.c
test -f src/cli/model_artifacts/yvex_attention_surface.h
test -f src/cli/model_artifacts/yvex_context_surface.c
test -f src/cli/model_artifacts/yvex_context_surface.h
test -f src/cli/model_artifacts/yvex_moe_surface.c
test -f src/cli/model_artifacts/yvex_moe_surface.h
test -f src/cli/model_artifacts/yvex_tensor_collection_surface.c
test -f src/cli/model_artifacts/yvex_tensor_collection_surface.h
test -f src/cli/input/yvex_model_artifacts_args.c
test -f src/cli/input/yvex_model_artifacts_args.h
test -f src/cli/render/yvex_model_artifacts_render.c
test -f src/cli/render/yvex_model_artifacts_render.h
test -f src/cli/render/yvex_model_artifacts_render_common.c
test -f src/cli/render/yvex_model_artifacts_render_common.h
test -f src/cli/render/yvex_models_render.c
test -f src/cli/render/yvex_models_render.h
test -f src/cli/render/yvex_models_artifacts_render.c
test -f src/cli/render/yvex_models_artifacts_render.h
test -f src/cli/render/yvex_models_download_render.c
test -f src/cli/render/yvex_models_download_render.h
test -f src/cli/render/yvex_models_download_write_render.c
test -f src/cli/render/yvex_models_download_process_render.c
test -f src/cli/render/yvex_models_download_control_render.c
test -f src/cli/render/yvex_models_prepare_render.c
test -f src/cli/render/yvex_models_prepare_render.h
test -f src/cli/render/yvex_fullmodel_render.c
test -f src/cli/render/yvex_fullmodel_render.h
test -f src/cli/render/yvex_fullmodel_report_render.c
test -f src/cli/render/yvex_fullmodel_materialize_render.c
test -f src/cli/render/yvex_attention_render.c
test -f src/cli/render/yvex_attention_render.h
test -f src/cli/render/yvex_context_render.c
test -f src/cli/render/yvex_context_render.h
test -f src/cli/render/yvex_moe_render.c
test -f src/cli/render/yvex_moe_render.h
test -f src/cli/render/yvex_tensor_collection_render.c
test -f src/cli/render/yvex_tensor_collection_render.h
test -f src/cli/catalog/model_artifacts_fields.def
test -f src/cli/catalog/model_artifacts_boundaries.def
test -f src/source/yvex_source.c
test -f src/source/yvex_source_private.h
test -f src/source/yvex_source_manifest.c
test -f src/source/yvex_source_manifest.h
test -f src/source/yvex_source_scan.c
test -f src/source/yvex_source_scan.h
test -f src/source/yvex_native_weights.c
test -f src/source/yvex_native_weights.h
test -f src/source/yvex_safetensors_header.c
test -f src/source/yvex_safetensors_header.h
test -f src/source/yvex_source_report.c
test -f src/source/yvex_source_report.h
test -f src/source/yvex_source_write.c
test -f src/source/yvex_source_write.h
test -f src/cli/commands/yvex_source_cli.c
test -f src/cli/input/yvex_source_args.c
test -f src/cli/input/yvex_source_args.h
test -f src/cli/render/yvex_source_render.c
test -f src/cli/render/yvex_source_render.h
test -f src/io/yvex_json_writer.c
test -f src/io/yvex_json_writer.h
test -f src/generation/yvex_prefill.c
test -f src/generation/yvex_kv.c
test -f src/generation/yvex_kv_report.c
test -f src/generation/yvex_kv_report.h
test -f src/generation/yvex_kv_private.h
test -f src/cli/input/yvex_kv_args.c
test -f src/cli/input/yvex_kv_args.h
test -f src/cli/commands/yvex_kv_cli.c
test -f src/cli/render/yvex_kv_render.c
test -f src/cli/render/yvex_kv_render.h
test -f src/cli/catalog/kv_options.def
test -f src/cli/catalog/kv_fields.def
test -f src/cli/catalog/kv_boundaries.def
test -f src/generation/yvex_decode.c
test -f src/cli/commands/yvex_decode_cli.c
test -f src/generation/yvex_logits.c
test -f src/cli/commands/yvex_logits_cli.c
test -f src/generation/yvex_sampling.c
test -f src/generation/yvex_sampling_report.c
test -f src/generation/yvex_sampling_report.h
test -f src/generation/yvex_sampling_private.h
test -f src/cli/input/yvex_sampling_args.c
test -f src/cli/input/yvex_sampling_args.h
test -f src/cli/commands/yvex_sampling_cli.c
test -f src/cli/render/yvex_sampling_render.c
test -f src/cli/render/yvex_sampling_render.h
test -f src/cli/catalog/sampling_options.def
test -f src/cli/catalog/sampling_fields.def
test -f src/cli/catalog/sampling_boundaries.def
test -f src/graph/yvex_graph_report.c
test -f src/graph/yvex_graph_report.h
test -f src/graph/yvex_graph_private.h
test -f src/graph/yvex_memory_plan.c
test -f src/graph/yvex_memory_plan.h
test -f src/graph/yvex_graph_plan.c
test -f src/graph/yvex_graph_plan.h
test -f src/graph/yvex_graph_guard.c
test -f src/graph/yvex_graph_guard.h
test -f src/graph/yvex_graph_primitive.c
test -f src/graph/yvex_graph_primitive.h
test -f src/graph/yvex_graph_bind.c
test -f src/graph/yvex_graph_bind.h
test -f src/graph/yvex_graph_execute.c
test -f src/graph/yvex_graph_execute.h
test -f src/cli/input/yvex_graph_args.c
test -f src/cli/input/yvex_graph_args.h
test -f src/cli/commands/yvex_graph_cli.c
test -f src/cli/render/yvex_graph_render.c
test -f src/cli/render/yvex_graph_render.h
test -f src/cli/catalog/graph_options.def
test -f src/cli/catalog/graph_fields.def
test -f src/cli/catalog/graph_boundaries.def
test -f src/generation/yvex_generation.c
test -f src/generation/yvex_generation_private.h
test -f src/generation/yvex_generation_report.c
test -f src/generation/yvex_generation_report.h
test -f src/generation/yvex_generation_trace.c
test -f src/generation/yvex_generation_trace.h
test -f src/cli/commands/yvex_generate_cli.c
test -f src/cli/input/yvex_generate_args.c
test -f src/cli/input/yvex_generate_args.h
test -f src/cli/render/yvex_generate_render.c
test -f src/cli/render/yvex_generate_render.h
test -f src/cli/render/yvex_generate_trace_render.c
test -f src/cli/render/yvex_generate_trace_render.h
test -f src/cli/catalog/generate_fields.def
test -f src/cli/catalog/generate_trace_fields.def
test -f src/eval/yvex_eval.c
test -f src/bench/yvex_bench.c
test -f src/metrics/yvex_metrics.c
test -f src/metrics/yvex_profile.c
test -f src/cli/commands/yvex_profile_cli.c
test -f src/backend/yvex_backend.c
test -f src/backend/cuda/cuda_backend.c
test -f src/backend/cuda/cuda_errors.c
test -f src/backend/cuda/cuda_info.c
test -f src/backend/cuda/cuda_internal.h
test -f src/backend/cuda/cuda_kernels.cu
test -f src/backend/cuda/cuda_kernels.h
test -f src/backend/cuda/cuda_ops.c
test -f src/backend/cuda/cuda_tensor.c
test -f src/backend/cuda/cuda_qtype.c
test -f src/backend/cuda/cuda_qtype.h
test -f src/backend/yvex_backend_tensor.c
test -f src/backend/yvex_backend_tensor.h
test -f src/backend/yvex_backend_qtype.c
test -f src/backend/yvex_backend_qtype.h
test -f src/backend/yvex_backend_report.c
test -f src/backend/yvex_backend_report.h
test -f src/gguf/gguf.c
test -f src/gguf/naming.c
test -f src/gguf/tools.c
test -f src/cli/commands/yvex_gguf_cli.c
test -f src/gguf/conversion.c
test -f src/cli/commands/yvex_conversion_cli.c
test -f src/gguf/families.h
test -f src/gguf/quant.c
test -f src/gguf/yvex_gguf_private.h
test -f src/gguf/yvex_gguf_container.c
test -f src/gguf/yvex_gguf_metadata.c
test -f src/gguf/yvex_gguf_tensor_info.c
test -f src/gguf/yvex_gguf_qtype.c
test -f src/gguf/yvex_gguf_range_map.c
test -f src/gguf/yvex_gguf_reader.c
test -f src/gguf/yvex_gguf_writer.c
test -f src/gguf/yvex_gguf_roundtrip.c
test -f src/gguf/yvex_gguf_name_map.c
test -f src/gguf/yvex_gguf_layout_map.c
test -f src/gguf/yvex_gguf_descriptor.c
test -f src/gguf/yvex_gguf_report.c
test -f src/cli/commands/yvex_quant_cli.c
test -f src/cli/io/yvex_cli_out.c
test -f src/cli/io/yvex_cli_error.c
test -f src/cli/io/yvex_cli_json.c
test -f src/cli/io/yvex_cli_table.c
test -f src/cli/io/yvex_cli_log.c

test -d include/yvex
test ! -d cuda
test ! -d gguf
test ! -d models
test -d docs
test -d tests
test -d tests/vectors

test -z "$(git ls-files 'yvex_*.c')"
test -z "$(git ls-files 'yvex_*_private.h')"

for f in \
  src/model/yvex_model.c \
  src/cli/commands/yvex_model_target_cli.c \
  src/cli/yvex_cli.c \
  src/cli/commands/yvex_runtime_cli.c \
  src/cli/commands/yvex_generate_cli.c \
  src/cli/commands/yvex_artifact_cli.c \
  src/cli/commands/yvex_source_cli.c
do
  grep -nF 'Owner:' "$f" >/dev/null
  grep -nF 'Owns:' "$f" >/dev/null
  grep -nF 'Does not own:' "$f" >/dev/null
  grep -nF 'Invariants:' "$f" >/dev/null
  grep -nF 'Boundary:' "$f" >/dev/null
  grep -nF 'Purpose:' "$f" >/dev/null
  grep -nF 'Inputs:' "$f" >/dev/null
  grep -nF 'Effects:' "$f" >/dev/null
  grep -nF 'Failure:' "$f" >/dev/null
done

grep -nF 'Does not own:' src/model/yvex_model.c >/dev/null
grep -nF 'CLI grammar' src/model/yvex_model.c >/dev/null
grep -nF 'usage text' src/model/yvex_model.c >/dev/null
grep -nF 'model-target command dispatch only' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'parse argv, build typed report, render typed report' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'does not create capability' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'top-level command lookup' src/cli/yvex_cli.c >/dev/null
grep -nF 'model metadata/materialization facts are not model support' src/model/yvex_model.c >/dev/null
grep -nF 'tensor payload bytes' src/model/yvex_model.c >/dev/null
grep -nF 'materialization' src/model/yvex_model.c >/dev/null

if grep -nF 'usage: yvex model-target' src/model/yvex_model.c; then
  echo "model-target usage text must not live in src/model/yvex_model.c"
  exit 1
fi

if grep -nF 'yvex model-target' src/model/yvex_model.c; then
  echo "model-target command text must not live in src/model/yvex_model.c"
  exit 1
fi

grep -nF 'yvex_model_target_render_help' src/cli/render/yvex_model_target_render.c >/dev/null

grep -nF 'const yvex_source_report *report' src/cli/render/yvex_source_render.c >/dev/null
grep -nF 'yvex_source_render_normal' src/cli/render/yvex_source_render.c >/dev/null
grep -nF 'yvex_cli_out_writef' src/cli/render/yvex_source_render.c >/dev/null

source_adapter_lines="$(wc -l < src/cli/commands/yvex_source_cli.c | tr -d ' ')"
if test "$source_adapter_lines" -gt 350; then
  echo "source command adapter too large: $source_adapter_lines"
  exit 1
fi

if grep -nE 'command_source_manifest|yvex_source_manifest_(command|help|report_command)|source-manifest requires|unknown source-manifest|source manifest: written' src/gguf/tools.c; then
  echo "source-manifest command/help/report dispatch must not live in src/gguf/tools.c"
  exit 1
fi

SOURCE_OUTPUT_HITS="$(
  grep -RInE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/source |
  grep -vE '^src/source/yvex_source_write\.c:' || true
)"
if test -n "$SOURCE_OUTPUT_HITS"; then
  echo "$SOURCE_OUTPUT_HITS"
  echo "source domain output must be isolated to src/source/yvex_source_write.c"
  exit 1
fi

if grep -nE '\bstdout\b|\bstderr\b' src/source/yvex_source_write.c src/source/yvex_source_write.h; then
  echo "source manifest writer must not choose stdout/stderr"
  exit 1
fi

if grep -nE '\b(stdout|stderr)\b' src/io/yvex_json_writer.c src/io/yvex_json_writer.h; then
  echo "generic JSON writer must not choose stdout/stderr"
  exit 1
fi

SOURCE_CLI_RESIDUE="$(
  grep -RInE '\bargc\b|\bargv\b|strcmp\(argv\[|usage: yvex|--family|--release|--output|--audit|--json|--include-|_command\(|_help\(|_usage\(' src/source || true
)"
if test -n "$SOURCE_CLI_RESIDUE"; then
  echo "$SOURCE_CLI_RESIDUE"
  echo "source cell must not own CLI parsing, help, usage, or command adapters"
  exit 1
fi

if grep -RInE '#include .*src/cli|#include "yvex_(cli|operator|console|render)' src/source; then
  echo "source cell must not include CLI/operator render headers"
  exit 1
fi

if grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_source_render.c; then
  echo "source renderer must use src/cli/io writers"
  exit 1
fi

GENERATION_DOMAIN_FILES='src/generation/yvex_generation.c src/generation/yvex_generation_report.c src/generation/yvex_generation_trace.c src/generation/yvex_generation_private.h src/generation/yvex_generation_report.h src/generation/yvex_generation_trace.h'

GENERATION_OUTPUT_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $GENERATION_DOMAIN_FILES || true
)"
if test -n "$GENERATION_OUTPUT_HITS"; then
  echo "$GENERATION_OUTPUT_HITS"
  echo "generation domain must not print or render"
  exit 1
fi

GENERATION_CLI_RESIDUE="$(
  grep -nE '\bargc\b|\bargv\b|usage: yvex|--output|--audit|--json|yvex_generate_command|yvex_generate_help|generate_parse_|generate_print_|generate_emit_' $GENERATION_DOMAIN_FILES || true
)"
if test -n "$GENERATION_CLI_RESIDUE"; then
  echo "$GENERATION_CLI_RESIDUE"
  echo "generation domain must not own CLI parsing, help, command, or render functions"
  exit 1
fi

if grep -nE '#include .*src/cli|#include "yvex_(cli|operator|console|render)' $GENERATION_DOMAIN_FILES; then
  echo "generation domain must not include CLI/operator render headers"
  exit 1
fi

grep -nF 'const yvex_generation_report *report' src/cli/render/yvex_generate_render.c >/dev/null
grep -nF 'const yvex_generation_report *report' src/cli/render/yvex_generate_trace_render.c >/dev/null
grep -nF 'yvex_generate_render_normal' src/cli/render/yvex_generate_render.c >/dev/null
grep -nF 'yvex_generate_render_audit' src/cli/render/yvex_generate_render.c >/dev/null
grep -nF 'yvex_generate_render_trace' src/cli/render/yvex_generate_trace_render.c >/dev/null
grep -nF 'yvex_cli_out_writef' src/cli/render/yvex_generate_render.c >/dev/null
grep -nF 'yvex_cli_out_writef' src/cli/render/yvex_generate_trace_render.c >/dev/null
if grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_generate_render.c src/cli/render/yvex_generate_trace_render.c; then
  echo "generate renderers must use src/cli/io writers"
  exit 1
fi

if grep -nE '\b(yvex_engine_open|yvex_backend_open|yvex_engine_sample_token|yvex_token_input_parse_explicit)\b' src/cli/commands/yvex_generate_cli.c; then
  echo "generate command adapter must not open runtime/domain state directly"
  exit 1
fi

generate_adapter_lines="$(wc -l < src/cli/commands/yvex_generate_cli.c | tr -d ' ')"
if test "$generate_adapter_lines" -gt 350; then
  echo "generate command adapter too large: $generate_adapter_lines"
  exit 1
fi

KV_DOMAIN_FILES='src/generation/yvex_kv.c src/generation/yvex_kv_report.c src/generation/yvex_kv_report.h src/generation/yvex_kv_private.h'

BAD_KV_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $KV_DOMAIN_FILES || true
)"
if test -n "$BAD_KV_OUTPUT"; then
  echo "$BAD_KV_OUTPUT"
  echo "KV domain/report must not write operator output"
  exit 1
fi

BAD_KV_COMMAND="$(
  grep -nE 'yvex_kv_command|yvex_kv_help|command_kv|command_kv_report|argc|argv|arg_count|args|usage: yvex|--model|--family|--backend|--output|--audit|--layers|--heads|--capacity' $KV_DOMAIN_FILES || true
)"
if test -n "$BAD_KV_COMMAND"; then
  echo "$BAD_KV_COMMAND"
  echo "KV command/help/parser code must not live in generation domain"
  exit 1
fi

BAD_KV_CLI_INCLUDE="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $KV_DOMAIN_FILES || true
)"
if test -n "$BAD_KV_CLI_INCLUDE"; then
  echo "$BAD_KV_CLI_INCLUDE"
  echo "KV domain/report must not include CLI/operator headers"
  exit 1
fi

grep -nF 'const yvex_kv_report *report' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_normal' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_audit' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_help' src/cli/render/yvex_kv_render.c >/dev/null

BAD_KV_RENDER_STDIO="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_kv_render.c src/cli/render/yvex_kv_render.h || true
)"
if test -n "$BAD_KV_RENDER_STDIO"; then
  echo "$BAD_KV_RENDER_STDIO"
  echo "KV renderer must use src/cli/io writers"
  exit 1
fi

BAD_KV_RENDER_PLACEHOLDER="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_kv_render.c src/cli/render/yvex_kv_render.h || true
)"
if test -n "$BAD_KV_RENDER_PLACEHOLDER"; then
  echo "$BAD_KV_RENDER_PLACEHOLDER"
  echo "KV renderer must be real and typed"
  exit 1
fi

kv_adapter_lines="$(wc -l < src/cli/commands/yvex_kv_cli.c | tr -d ' ')"
if test "$kv_adapter_lines" -gt 350; then
  echo "src/cli/commands/yvex_kv_cli.c has $kv_adapter_lines lines; adapter must stay <= 350"
  exit 1
fi

BAD_KV_INPUT_EXEC="$(
  grep -nE 'yvex_model_ref_resolve|open_model_context|yvex_kv_cache_create|yvex_kv_cache_append|yvex_kv_cache_read|yvex_kv_report_build|yvex_kv_render|fprintf|printf' src/cli/input/yvex_kv_args.c src/cli/input/yvex_kv_args.h || true
)"
if test -n "$BAD_KV_INPUT_EXEC"; then
  echo "$BAD_KV_INPUT_EXEC"
  echo "KV input parser must parse only"
  exit 1
fi

BAD_KV_COMMAND_DOMAIN="$(
  grep -nE 'open_model_context|yvex_kv_cache_create_shape|yvex_kv_cache_append_position_f32|yvex_kv_cache_read_position_f32|kv_scan_roles|fprintf|printf' src/cli/commands/yvex_kv_cli.c || true
)"
if test -n "$BAD_KV_COMMAND_DOMAIN"; then
  echo "$BAD_KV_COMMAND_DOMAIN"
  echo "KV command adapter must dispatch only"
  exit 1
fi

SAMPLING_DOMAIN_FILES='src/generation/yvex_sampling.c src/generation/yvex_sampling_report.c src/generation/yvex_sampling_report.h src/generation/yvex_sampling_private.h'

BAD_SAMPLING_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$BAD_SAMPLING_OUTPUT"; then
  echo "$BAD_SAMPLING_OUTPUT"
  echo "sampling domain/report must not write operator output"
  exit 1
fi

BAD_SAMPLING_COMMAND="$(
  grep -nE 'yvex_sample_command|yvex_sample_help|argc|argv|arg_count|args|usage: yvex|--model|--backend|--segment|--tokens|--strategy|--logits-count|--attach-kv|--layers|--context-length' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$BAD_SAMPLING_COMMAND"; then
  echo "$BAD_SAMPLING_COMMAND"
  echo "sample command/help/parser code must not live in generation domain"
  exit 1
fi

BAD_SAMPLING_CLI_INCLUDE="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$BAD_SAMPLING_CLI_INCLUDE"; then
  echo "$BAD_SAMPLING_CLI_INCLUDE"
  echo "sampling domain/report must not include CLI/operator headers"
  exit 1
fi

grep -nF 'const yvex_sampling_report *report' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_normal' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_audit' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_help' src/cli/render/yvex_sampling_render.c >/dev/null

BAD_SAMPLING_RENDER_STDIO="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_sampling_render.c src/cli/render/yvex_sampling_render.h || true
)"
if test -n "$BAD_SAMPLING_RENDER_STDIO"; then
  echo "$BAD_SAMPLING_RENDER_STDIO"
  echo "sampling renderer must use src/cli/io writers"
  exit 1
fi

BAD_SAMPLING_RENDER_PLACEHOLDER="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_sampling_render.c src/cli/render/yvex_sampling_render.h || true
)"
if test -n "$BAD_SAMPLING_RENDER_PLACEHOLDER"; then
  echo "$BAD_SAMPLING_RENDER_PLACEHOLDER"
  echo "sampling renderer must be real and typed"
  exit 1
fi

sampling_adapter_lines="$(wc -l < src/cli/commands/yvex_sampling_cli.c | tr -d ' ')"
if test "$sampling_adapter_lines" -gt 350; then
  echo "src/cli/commands/yvex_sampling_cli.c has $sampling_adapter_lines lines; adapter must stay <= 350"
  exit 1
fi

grep -nF 'yvex_sampling_args_parse' src/cli/commands/yvex_sampling_cli.c >/dev/null
grep -nF 'yvex_sampling_report_build' src/cli/commands/yvex_sampling_cli.c >/dev/null
grep -nF 'yvex_sampling_render' src/cli/commands/yvex_sampling_cli.c >/dev/null

BAD_SAMPLING_INPUT_EXEC="$(
  grep -nE 'yvex_model_ref_resolve|enforce_registered_identity|open_model_context|preflight_graph_guard|yvex_engine_open|yvex_engine_sample_token|yvex_sampling_report_build|yvex_sampling_render|fprintf|printf' src/cli/input/yvex_sampling_args.c src/cli/input/yvex_sampling_args.h || true
)"
if test -n "$BAD_SAMPLING_INPUT_EXEC"; then
  echo "$BAD_SAMPLING_INPUT_EXEC"
  echo "sampling input parser must parse only"
  exit 1
fi

BAD_SAMPLING_COMMAND_DOMAIN="$(
  grep -nE 'yvex_model_ref_resolve|enforce_registered_identity|preflight_graph_guard|yvex_engine_open|yvex_engine_sample_token|yvex_token_input_parse|fprintf|printf' src/cli/commands/yvex_sampling_cli.c || true
)"
if test -n "$BAD_SAMPLING_COMMAND_DOMAIN"; then
  echo "$BAD_SAMPLING_COMMAND_DOMAIN"
  echo "sampling command adapter must dispatch only"
  exit 1
fi

GRAPH_DOMAIN_FILES='src/graph/*.c src/graph/*.h'

BAD_GRAPH_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$BAD_GRAPH_OUTPUT"; then
  echo "$BAD_GRAPH_OUTPUT"
  echo "graph domain/report must not write operator output"
  exit 1
fi

BAD_GRAPH_COMMAND="$(
  grep -nE 'yvex_graph_command|yvex_graph_help|argc|argv|arg_count|args|usage: yvex|--model|--backend|--output|--audit|--graph|--plan|--primitive' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$BAD_GRAPH_COMMAND"; then
  echo "$BAD_GRAPH_COMMAND"
  echo "graph command/help/parser code must not live in graph domain"
  exit 1
fi

BAD_GRAPH_CLI_INCLUDE="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$BAD_GRAPH_CLI_INCLUDE"; then
  echo "$BAD_GRAPH_CLI_INCLUDE"
  echo "graph domain/report must not include CLI/operator headers"
  exit 1
fi

grep -nF 'const yvex_graph_report *report' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_normal' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_audit' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_help' src/cli/render/yvex_graph_render.c >/dev/null

BAD_GRAPH_RENDER_STDIO="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_graph_render.c src/cli/render/yvex_graph_render.h || true
)"
if test -n "$BAD_GRAPH_RENDER_STDIO"; then
  echo "$BAD_GRAPH_RENDER_STDIO"
  echo "graph renderer must use src/cli/io writers"
  exit 1
fi

BAD_GRAPH_RENDER_PLACEHOLDER="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_graph_render.c src/cli/render/yvex_graph_render.h || true
)"
if test -n "$BAD_GRAPH_RENDER_PLACEHOLDER"; then
  echo "$BAD_GRAPH_RENDER_PLACEHOLDER"
  echo "graph renderer must be real and typed"
  exit 1
fi

graph_adapter_lines="$(wc -l < src/cli/commands/yvex_graph_cli.c | tr -d ' ')"
if test "$graph_adapter_lines" -gt 350; then
  echo "src/cli/commands/yvex_graph_cli.c has $graph_adapter_lines lines; adapter must stay <= 350"
  exit 1
fi

grep -nF 'yvex_graph_args_parse' src/cli/commands/yvex_graph_cli.c >/dev/null
grep -nF 'yvex_graph_render' src/cli/commands/yvex_graph_cli.c >/dev/null
grep -nE 'yvex_graph_.*report_build|yvex_graph_report_build' src/cli/commands/yvex_graph_cli.c >/dev/null

BAD_GRAPH_INPUT_EXEC="$(
  grep -nE 'yvex_model_ref_resolve|yvex_plan_create|yvex_graph_build_for_model|yvex_memory_plan_from_graph|yvex_backend_open|yvex_graph_report_build|yvex_graph_render|fprintf|printf' src/cli/input/yvex_graph_args.c src/cli/input/yvex_graph_args.h || true
)"
if test -n "$BAD_GRAPH_INPUT_EXEC"; then
  echo "$BAD_GRAPH_INPUT_EXEC"
  echo "graph input parser must parse only"
  exit 1
fi

BAD_GRAPH_COMMAND_DOMAIN="$(
  grep -nE 'yvex_model_ref_resolve|yvex_plan_create|yvex_graph_build_for_model|yvex_memory_plan_from_graph|yvex_backend_open|fprintf|printf' src/cli/commands/yvex_graph_cli.c || true
)"
if test -n "$BAD_GRAPH_COMMAND_DOMAIN"; then
  echo "$BAD_GRAPH_COMMAND_DOMAIN"
  echo "graph command adapter must dispatch only"
  exit 1
fi

BAD_MODEL_TARGET_CLI_DOMAIN="$(
  grep -nE 'model_target_classes|model_targets|full_runtime_candidate|dense_candidate|qwen_metal|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|output_head_map_profile|tokenizer_map|qtype_policy|role_support|native_weight|safetensors|source_path' src/cli/commands/yvex_model_target_cli.c || true
)"
if test -n "$BAD_MODEL_TARGET_CLI_DOMAIN"; then
  echo "$BAD_MODEL_TARGET_CLI_DOMAIN"
  echo "model-target domain/report facts must not live in CLI command adapter"
  exit 1
fi

model_target_report_lines="$(wc -l < src/model/target/yvex_model_target_report.c | tr -d ' ')"
if test "$model_target_report_lines" -gt 800; then
  echo "src/model/target/yvex_model_target_report.c has $model_target_report_lines lines; coordinator must stay <= 800"
  exit 1
fi

BAD_MODEL_TARGET_COORDINATOR="$(
  grep -nE 'model_target_classes|model_targets|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|output_head_map_profile|native_weight|safetensors|source_path|model_target_capture_out|model_target_capture_err|model_target_out_writef|model_target_file_char|captured report text' src/model/target/yvex_model_target_report.c || true
)"
if test -n "$BAD_MODEL_TARGET_COORDINATOR"; then
  echo "$BAD_MODEL_TARGET_COORDINATOR"
  echo "model-target report coordinator must not own report-specific internals"
  exit 1
fi

BAD_MODEL_TARGET_BOUNDARY_SHELL="$(
  grep -nE 'typedef int yvex_.*_file_boundary' src/model/target/*.c || true
)"
if test -n "$BAD_MODEL_TARGET_BOUNDARY_SHELL"; then
  echo "$BAD_MODEL_TARGET_BOUNDARY_SHELL"
  echo "model-target report modules must not be boundary typedef shells"
  exit 1
fi

BAD_MODEL_TARGET_TEXT_COMPAT="$(
  grep -nE 'primary_text|diagnostic_text|primary_len|diagnostic_len|model_target_capture_out|model_target_capture_err|captured report text|raw_output|report_text|captured_text|line_buffer|model_target_sink_out|model_target_sink_err' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_TEXT_COMPAT"; then
  echo "$BAD_MODEL_TARGET_TEXT_COMPAT"
  echo "model-target report layer must not use captured-output compatibility names"
  exit 1
fi

BAD_MODEL_TARGET_RUNNER_FILES="$(
  find src/model/target -maxdepth 1 -type f \( -name '*runner*' -o -name '*internal*' -o -name '*compat*' -o -name '*backend*' -o -name '*bridge*' -o -name '*shim*' \) -print
)"
if test -n "$BAD_MODEL_TARGET_RUNNER_FILES"; then
  echo "$BAD_MODEL_TARGET_RUNNER_FILES"
  echo "model-target must not keep shared execution compatibility files"
  exit 1
fi

BAD_MODEL_TARGET_RUNNER_SYMBOLS="$(
  grep -nE 'yvex_model_target_runner|runner_report_build|_runner|\brunner\b|compatibility backend|internal backend|shared report execution|report execution support' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_RUNNER_SYMBOLS"; then
  echo "$BAD_MODEL_TARGET_RUNNER_SYMBOLS"
  echo "model-target must not keep shared execution runner symbols"
  exit 1
fi

BAD_MODEL_TARGET_CLI_SHAPED_REQUEST="$(
  grep -nE '\b(argc|argv)\b' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_CLI_SHAPED_REQUEST"; then
  echo "$BAD_MODEL_TARGET_CLI_SHAPED_REQUEST"
  echo "model-target domain and render layers must not carry CLI-shaped request fields"
  exit 1
fi

BAD_MODEL_TARGET_FAKE_FILE="$(
  grep -nE '\(FILE \*\)\(uintptr_t\)|model_target_text_write|model_target_segment_append|model_target_out_writef|model_target_out|model_target_err|model_target_file_char|model_target_sink' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_FAKE_FILE"; then
  echo "$BAD_MODEL_TARGET_FAKE_FILE"
  echo "model-target must not use fake FILE or sink-based report output"
  exit 1
fi

if test -f src/model/target/yvex_model_target_internal.c; then
  model_target_internal_lines="$(wc -l < src/model/target/yvex_model_target_internal.c | tr -d ' ')"
  if test "$model_target_internal_lines" -gt 300; then
    echo "src/model/target/yvex_model_target_internal.c has $model_target_internal_lines lines; internal backend must be dissolved"
    exit 1
  fi
  BAD_MODEL_TARGET_INTERNAL="$(
    grep -nE 'model_target_sink|model_target_out_writef|model_target_stream_write|fwrite|fprintf|printf|model_target_classes|model_targets|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|qtype_policy|role_support|safetensors|native_weight' src/model/target/yvex_model_target_internal.c || true
  )"
  if test -n "$BAD_MODEL_TARGET_INTERNAL"; then
    echo "$BAD_MODEL_TARGET_INTERNAL"
    echo "model-target internal backend must not own report facts or output helpers"
    exit 1
  fi
fi

BAD_MODEL_TARGET_INTERNAL_DELEGATION="$(
  grep -nE 'yvex_model_target_internal_report|yvex_model_target_internal_.*build|return yvex_model_target_internal_|internal backend|compatibility backend' src/model/target/*.c src/model/target/*.h || true
)"
if test -n "$BAD_MODEL_TARGET_INTERNAL_DELEGATION"; then
  echo "$BAD_MODEL_TARGET_INTERNAL_DELEGATION"
  echo "model-target modules must not delegate to an internal backend"
  exit 1
fi

for module in \
  src/model/target/yvex_model_class_profile.c \
  src/model/target/yvex_tensor_collection_report.c \
  src/model/target/yvex_tensor_naming_report.c \
  src/model/target/yvex_output_head_map_report.c \
  src/model/target/yvex_tokenizer_map_report.c \
  src/model/target/yvex_missing_role_report.c \
  src/model/target/yvex_mapping_gate_report.c \
  src/model/target/yvex_qtype_policy_report.c \
  src/model/target/yvex_qtype_role_support_report.c \
  src/model/target/yvex_model_target_candidates.c \
  src/model/target/yvex_model_target_decision.c
do
  module_lines="$(wc -l < "$module" | tr -d ' ')"
  if test "$module_lines" -lt 80; then
    echo "$module has $module_lines lines; model-target report module must be a real ownership file"
    exit 1
  fi
  grep -nE '[_a-zA-Z0-9]+_build[[:space:]]*\(' "$module" >/dev/null
done

for source_file in src/model/target/*.c
do
  source_lines="$(wc -l < "$source_file" | tr -d ' ')"
  if test "$source_lines" -gt 2500; then
    echo "$source_file has $source_lines lines; model-target files must not become monoliths"
    exit 1
  fi
done

grep -nF 'yvex_model_target_decision_report_build' src/model/target/yvex_model_target_decision.c >/dev/null
grep -nF 'yvex_model_target_candidate_report_build' src/model/target/yvex_model_target_candidates.c >/dev/null
grep -nF 'yvex_model_class_profile_report_build' src/model/target/yvex_model_class_profile.c >/dev/null
grep -nF 'yvex_tensor_collection_report_build' src/model/target/yvex_tensor_collection_report.c >/dev/null
grep -nF 'yvex_tensor_naming_report_build' src/model/target/yvex_tensor_naming_report.c >/dev/null
grep -nF 'yvex_output_head_map_report_build' src/model/target/yvex_output_head_map_report.c >/dev/null
grep -nF 'yvex_tokenizer_map_report_build' src/model/target/yvex_tokenizer_map_report.c >/dev/null
grep -nF 'yvex_missing_role_report_build' src/model/target/yvex_missing_role_report.c >/dev/null
grep -nF 'yvex_mapping_gate_report_build' src/model/target/yvex_mapping_gate_report.c >/dev/null
grep -nF 'yvex_qtype_policy_report_build' src/model/target/yvex_qtype_policy_report.c >/dev/null
grep -nF 'yvex_qtype_role_support_report_build' src/model/target/yvex_qtype_role_support_report.c >/dev/null

model_target_adapter_lines="$(wc -l < src/cli/commands/yvex_model_target_cli.c | tr -d ' ')"
if test "$model_target_adapter_lines" -gt 350; then
  echo "src/cli/commands/yvex_model_target_cli.c has $model_target_adapter_lines lines; adapter must stay <= 350"
  exit 1
fi

grep -nF 'yvex_model_target_args_parse' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'yvex_model_target_render' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nE 'yvex_model_target_.*build|yvex_.*_report_build' src/cli/commands/yvex_model_target_cli.c >/dev/null

BAD_MODEL_TARGET_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/target/*.c src/model/target/*.h |
    grep -vE '^src/model/target/yvex_model_target_sidecar_write\.c:' || true
)"
if test -n "$BAD_MODEL_TARGET_OUTPUT"; then
  echo "$BAD_MODEL_TARGET_OUTPUT"
  echo "model-target domain/report must not write operator output"
  exit 1
fi

BAD_MODEL_TARGET_WRITER_STDIO="$(
  grep -nE 'stdout|stderr' src/model/target/yvex_model_target_sidecar_write.c src/model/target/yvex_model_target_sidecar_write.h || true
)"
if test -n "$BAD_MODEL_TARGET_WRITER_STDIO"; then
  echo "$BAD_MODEL_TARGET_WRITER_STDIO"
  echo "model-target sidecar writer must not write stdout/stderr"
  exit 1
fi

BAD_MODEL_TARGET_CLI_INCLUDE="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' src/model/target/*.c src/model/target/*.h || true
)"
if test -n "$BAD_MODEL_TARGET_CLI_INCLUDE"; then
  echo "$BAD_MODEL_TARGET_CLI_INCLUDE"
  echo "model-target domain/report must not include CLI/operator headers"
  exit 1
fi

model_artifacts_root_lines="$(wc -l < src/model/yvex_model_artifacts.c | tr -d ' ')"
if test "$model_artifacts_root_lines" -gt 500; then
  echo "src/model/yvex_model_artifacts.c has $model_artifacts_root_lines lines; model-artifacts root must stay <= 500"
  exit 1
fi

if grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/yvex_model_artifacts.c; then
  echo "model-artifacts root must not write operator output"
  exit 1
fi

if grep -nE '#include "yvex_(cli|operator|console)|yvex_operator_render|#include <.*cli.*>|src/cli' src/model/yvex_model_artifacts.c src/model/artifacts/*.c src/model/artifacts/*.h; then
  echo "model-artifacts domain must not include CLI/operator ownership"
  exit 1
fi

if grep -nE '\b(argc|argv)\b|usage:|--help|--output|--audit' src/model/yvex_model_artifacts.c src/model/artifacts/*.c src/model/artifacts/*.h; then
  echo "model-artifacts domain must not carry CLI-shaped input"
  exit 1
fi

if grep -nE '\b(argc|argv)\b' src/cli/render/yvex_model_artifacts_render.c src/cli/render/yvex_model_artifacts_render.h; then
  echo "model-artifacts domain/render must not carry CLI-shaped input"
  exit 1
fi

BAD_MODEL_ARTIFACTS_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/artifacts/*.c src/model/artifacts/*.h |
    grep -vE '^src/model/artifacts/yvex_model_artifact_write\.c:' || true
)"
if test -n "$BAD_MODEL_ARTIFACTS_OUTPUT"; then
  echo "$BAD_MODEL_ARTIFACTS_OUTPUT"
  echo "model-artifacts domain/report must not write operator output"
  exit 1
fi

if grep -nE 'stdout|stderr' src/model/artifacts/yvex_model_artifact_write.c src/model/artifacts/yvex_model_artifact_write.h; then
  echo "model-artifacts writer must not write stdout/stderr"
  exit 1
fi

if grep -nE 'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer' src/model/artifacts/*.c src/model/artifacts/*.h src/cli/render/yvex_model_artifacts_render.c src/cli/render/yvex_model_artifacts_render.h; then
  echo "model-artifacts reports/renderers must not use text-buffer report debt"
  exit 1
fi

model_artifacts_command_lines="$(wc -l < src/cli/commands/yvex_model_artifacts_cli.c | tr -d ' ')"
if test "$model_artifacts_command_lines" -gt 350; then
  echo "src/cli/commands/yvex_model_artifacts_cli.c has $model_artifacts_command_lines lines; adapter must stay <= 350"
  exit 1
fi

grep -nF 'yvex_model_artifacts_args_parse' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifact_report_build' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifacts_render' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null

BAD_MODEL_ARTIFACTS_COMMAND="$(
  grep -nE 'yvex_operator_private|yvex_operator_render_private|write_escaped|write_field|write_u64_field|model_artifacts_cli_strdup|path_exists|is_path_like_reference|set_path_ref|yvex_model_registry|yvex_model_ref|yvex_model_gate|yvex_artifact|yvex_backend|yvex_weight_table|native_weight|safetensors|mkdir|access\(|opendir|readdir|poll|waitpid|signal|printf|fprintf|fwrite' src/cli/commands/yvex_model_artifacts_cli.c || true
)"
if test -n "$BAD_MODEL_ARTIFACTS_COMMAND"; then
  echo "$BAD_MODEL_ARTIFACTS_COMMAND"
  echo "model-artifacts command adapter must stay thin"
  exit 1
fi

if test -f src/cli/model_artifacts/yvex_model_artifacts_surface.c; then
  model_artifacts_surface_lines="$(wc -l < src/cli/model_artifacts/yvex_model_artifacts_surface.c | tr -d ' ')"
  if test "$model_artifacts_surface_lines" -gt 350; then
    echo "src/cli/model_artifacts/yvex_model_artifacts_surface.c has $model_artifacts_surface_lines lines; root surface must stay <= 350"
    exit 1
  fi
fi

model_artifacts_common_lines="$(wc -l < src/cli/model_artifacts/yvex_model_artifacts_surface_common.c | tr -d ' ')"
if test "$model_artifacts_common_lines" -gt 500; then
  echo "src/cli/model_artifacts/yvex_model_artifacts_surface_common.c has $model_artifacts_common_lines lines; common helper must stay <= 500"
  exit 1
fi

BAD_MODEL_ARTIFACTS_SURFACE_SIZE="$(
  find src/cli/model_artifacts -name '*.c' -print0 |
    xargs -0 wc -l |
    awk '$1 ~ /^[0-9]+$/ && $2 != "total" && $1 > 2500 { print }' || true
)"
if test -n "$BAD_MODEL_ARTIFACTS_SURFACE_SIZE"; then
  echo "$BAD_MODEL_ARTIFACTS_SURFACE_SIZE"
  echo "model-artifacts CLI command-family surfaces must stay <= 2500 lines"
  exit 1
fi

if git grep -n 'src/cli/model_artifacts' -- Makefile | grep CORE_SRCS; then
  echo "src/cli/model_artifacts must stay out of CORE_SRCS"
  exit 1
fi

BAD_MODEL_TARGET_INPUT_EXEC="$(
  grep -nE 'native_weight|safetensors|yvex_model_ref_resolve|yvex_model_target_.*build|yvex_.*_report_build|yvex_model_target_render[[:space:]]*\(|fopen|\b(fprintf|printf)\s*\(' src/cli/input/yvex_model_target_args.c src/cli/input/yvex_model_target_args.h || true
)"
if test -n "$BAD_MODEL_TARGET_INPUT_EXEC"; then
  echo "$BAD_MODEL_TARGET_INPUT_EXEC"
  echo "model-target input parser must parse only"
  exit 1
fi

grep -nF 'yvex_model_target_render' src/cli/render/yvex_model_target_render.c >/dev/null
grep -nE 'const yvex_.*report \*report|const yvex_model_target_.* \*report' src/cli/render/yvex_model_target_render.c >/dev/null

BAD_MODEL_TARGET_RENDER_STDIO="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_RENDER_STDIO"; then
  echo "$BAD_MODEL_TARGET_RENDER_STDIO"
  echo "model-target renderer must use src/cli/io writers"
  exit 1
fi

BAD_MODEL_TARGET_RENDER_PLACEHOLDER="$(
  grep -nE 'captured_text|report_text|raw_output|const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$BAD_MODEL_TARGET_RENDER_PLACEHOLDER"; then
  echo "$BAD_MODEL_TARGET_RENDER_PLACEHOLDER"
  echo "model-target renderer must be real and typed"
  exit 1
fi

DOMAIN_FILES='src/accounts src/artifact src/backend src/core src/daemon src/gguf src/graph src/metrics src/model/yvex_model.c src/model/yvex_model_artifacts.c src/runtime src/source src/tokenizer src/generation'

PRINT_HITS="$(
  grep -RInE '\b(printf|vprintf|puts|putchar|perror)\s*\(' $DOMAIN_FILES || true
)"

if test -n "$PRINT_HITS"; then
  echo "$PRINT_HITS"
  echo "domain files must not use obvious direct operator print helpers"
  exit 1
fi

USAGE_HITS="$(
  grep -RIn 'usage: yvex' $DOMAIN_FILES || true
)" || true

if test -n "$USAGE_HITS"; then
  echo "$USAGE_HITS"
  echo "usage text outside src/cli"
  exit 1
fi

OPTION_HITS="$(
  grep -RInE '\bargc\b|\bargv\b|strcmp\(argv\[|--output|--audit|--json|--include-' $DOMAIN_FILES || true
)" || true

if test -n "$OPTION_HITS"; then
  echo "$OPTION_HITS"
  echo "domain files must not expose CLI parsing tokens"
  exit 1
fi

COMMAND_STRUCT_HITS="$(
  grep -RInE '^(struct yvex_graph|struct yvex_plan|struct yvex_memory_plan|struct yvex_native_weight_table|struct yvex_backend|struct yvex_engine|struct yvex_kv|struct yvex_logits|struct yvex_sampling)' src/cli/commands || true
)"
if test -n "$COMMAND_STRUCT_HITS"; then
  echo "$COMMAND_STRUCT_HITS"
  echo "domain structs must not live in src/cli/commands"
  exit 1
fi

COMMAND_EXPORT_HITS="$(
  grep -RInE '^(int|void|const char \*) yvex_(account|artifact|backend|engine|graph|kv|logits|sampling|source|native|gguf|conversion|metrics|trace|model_registry|model_ref|tokenizer|prompt|tokens|decode|generation)' src/cli/commands |
  grep -vE '_(command|help)\(' || true
)"
if test -n "$COMMAND_EXPORT_HITS"; then
  echo "$COMMAND_EXPORT_HITS"
  echo "domain exported symbols must not live in src/cli/commands"
  exit 1
fi

if grep -RIn '\byvex_backend_open\b' src/cli/commands; then
  echo "CLI command adapters must not open backends directly"
  exit 1
fi

if grep -RInE '#include .*src/cli|#include "yvex_(cli|console|render)' $DOMAIN_FILES; then
  echo "domain files must not include CLI headers"
  exit 1
fi

PLACEHOLDER_RENDER_HITS="$(
  git grep -nE 'renderer-only|const void \*report|present/not-bound|not-bound|_render_boundary' -- \
    'src/cli/render/**' || true
)"
if test -n "$PLACEHOLDER_RENDER_HITS"; then
  echo "$PLACEHOLDER_RENDER_HITS"
  echo "placeholder renderers are forbidden"
  exit 1
fi

CORE_BLOCK="$(sed -n '/^CORE_SRCS :=/,/^$/p' Makefile)"
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/accounts/yvex_accounts.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/artifact/yvex_artifact.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/artifact/yvex_artifact_descriptor.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/artifact/yvex_artifact_materialize.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/artifact/yvex_artifact_roundtrip_gate.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/backend/yvex_backend.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/backend/yvex_backend_qtype.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/backend/yvex_backend_tensor.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/graph/yvex_graph.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/graph/yvex_graph_bind.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/graph/yvex_graph_execute.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/gguf/yvex_gguf_container.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/gguf/yvex_gguf_qtype.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/source/yvex_source.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/generation/yvex_generation.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/model/yvex_runtime_descriptor.c' >/dev/null
if printf '%s\n' "$CORE_BLOCK" | grep -F 'src/cli/'; then
  echo "CORE_SRCS must not include CLI sources"
  exit 1
fi
grep -nF 'src/backend/cuda/cuda_qtype.c' Makefile >/dev/null
grep -nF 'CLI_SRCS :=' Makefile >/dev/null
grep -nF 'CLI_INPUT_SRCS :=' Makefile >/dev/null
grep -nF 'src/daemon/yvexd.c' Makefile >/dev/null

CATALOG_HITS="$(
  git grep -n 'src/cli/catalog' -- 'src/**/*.c' 'src/**/*.h' |
  grep -vE '^src/cli/'
)" || true

if test -n "$CATALOG_HITS"; then
  echo "$CATALOG_HITS"
  echo "CLI catalog included outside src/cli"
  exit 1
fi

CATALOG_LOGIC_HITS="$(
  grep -RInE '\b(if|for|while|switch|return)\b|[{}();]|printf|fprintf' src/cli/catalog || true
)"

if test -n "$CATALOG_LOGIC_HITS"; then
  echo "$CATALOG_LOGIC_HITS"
  echo "CLI catalog files must contain lists only"
  exit 1
fi

if git grep -nF 'This file owns everything' -- 'src/*.c' 'src/*.cu'; then
  echo "vague source ownership contract found"
  exit 1
fi

if grep -nF 'helper function' src/model/yvex_model.c; then
  echo "vague function comment found in src/model/yvex_model.c"
  exit 1
fi

bad_command_files="$(
  find . -maxdepth 5 \
    \( -name 'yvex_cli_*.c' \
       -o -name 'yvex_cli_*.h' \
       -o -name 'yvex_*_commands.c' \
       -o -name 'commands.c' \
       -o -name 'yvex_command_private.h' \
       -o -name 'yvex_graph_block.c' \
       -o -name 'yvex_graph_commands.c' \
       -o -name 'yvex_graph_primitives.c' \
       -o -name 'yvex_graph_reference.c' \
       -o -name 'yvex_graph_layers.c' \
       -o -name 'yvex_graph_scheduler.c' \
       -o -name 'yvex_graph_check.c' \
       -o -name 'graph_layers.c' \
       -o -name 'graph_check.c' \
       -o -name 'scheduler.c' \
       -o -name 'yvex_model_artifact_support.c' \
       -o -name 'yvex_model_artifacts_private.h' \
       -o -name 'yvex_model_artifact_commands.c' \
       -o -name 'yvex_model_target.c' \
       -o -name 'yvex_model_targets.c' \
       -o -name 'yvex_targets.c' \
       -o -name 'yvex_target_registry.c' \
       -o -name 'model_target.c' \
       -o -name 'model_targets.c' \
       -o -name 'target_registry.c' \
       -o -name 'source_target.c' \
       -o -name 'owintake_targets.c' \
       -o -name 'yvex_model_target.h' \
       -o -name 'yvex_target_private.h' \
       -o -name 'yvex_owi_private.h' \
       -o -name 'yvex_block.c' \
       -o -name 'yvex_transformer_block.c' \
       -o -name 'yvex_layer.c' \
       -o -name 'yvex_layers.c' \
       -o -name 'yvex_executor.c' \
       -o -name 'yvex_graph_executor.c' \
       -o -name 'block.c' \
       -o -name 'layer.c' \
       -o -name 'layers.c' \
       -o -name 'executor.c' \
       -o -name 'block.h' \
       -o -name 'layer.h' \
       -o -name 'layers.h' \
       -o -name 'executor.h' \
       -o -name 'yvex_paths.c' \
       -o -name 'yvex_operator_paths.c' \
       -o -name 'yvex_path_config.c' \
       -o -name 'operator_paths.c' \
       -o -name 'paths_config.c' \
       -o -name 'path_config.c' \
       -o -name 'yvex_paths_private.h' \
       -o -name 'yvex_operator_paths.h' \
       -o -name 'operator_paths.h' \
       -o -path './include/yvex/operator_paths.h' \
       -o -path './include/yvex/paths.h' \
       -o -name 'yvex_model_target_paths.c' \
       -o -name 'yvex_target_paths.c' \
       -o -name 'model_target_paths.c' \
       -o -name 'target_paths.c' \
       -o -name 'yvex_operator_targets.c' \
       -o -name 'model_paths.c' \
       -o -path './include/yvex/model_target_paths.h' \
       -o -path './include/yvex/target_paths.h' \
       -o -name 'yvex_model_target_paths_private.h' \
       -o -name 'target_paths_private.h' \
       -o -name 'yvex_model_prepare.c' \
       -o -name 'yvex_prepare.c' \
       -o -name 'model_prepare.c' \
       -o -name 'prepare.c' \
       -o -name 'models_prepare.c' \
       -o -name 'yvex_prepare_private.h' \
       -o -name 'yvex_model_prepare_private.h' \
       -o -path './include/yvex/prepare.h' \
       -o -path './include/yvex/model_prepare.h' \
       -o -name 'yvex_model_check.c' \
       -o -name 'models_check.c' \
       -o -name 'model_check.c' \
       -o -name 'check.c' \
       -o -name 'yvex_check.c' \
       -o -name 'yvex_model_check_private.h' \
       -o -path './include/yvex/model_check.h' \
       -o -name 'yvex_runtime_cli_backend.c' \
       -o -name 'yvex_runtime_cli_tokenizer.c' \
       -o -name 'yvex_runtime_cli_paths.c' \
       -o -name 'yvex_runtime_cli_graph.c' \
       -o -name 'yvex_runtime_engine_commands.c' \
       -o -name 'yvex_runtime_session_commands.c' \
       -o -name 'yvex_runtime_commands.c' \
       -o -name 'yvex_runtime_graph.c' \
       -o -name 'yvex_runtime_support.c' \
       -o -name 'runtime_commands.c' \
       -o -name 'runtime_graph.c' \
       -o -name 'yvex_decode_command.c' \
       -o -name 'yvex_decode_runtime.c' \
       -o -name 'decode_command.c' \
       -o -name 'decode_runtime.c' \
       -o -name 'yvex_logits_command.c' \
       -o -name 'yvex_logits_runtime.c' \
       -o -name 'logits_command.c' \
       -o -name 'logits_runtime.c' \
       -o -name 'yvex_sampling_command.c' \
       -o -name 'yvex_sampling_runtime.c' \
       -o -name 'sampling_command.c' \
       -o -name 'sampling_runtime.c' \
       -o -name 'sample_command.c' \
       -o -name 'sample_runtime.c' \
       -o -name 'yvex_generation_command.c' \
       -o -name 'yvex_generation_runtime.c' \
       -o -name 'generation_command.c' \
       -o -name 'generation_runtime.c' \
       -o -name 'generate_command.c' \
       -o -name 'generate_runtime.c' \
       -o -name 'yvex_prefill_chunks.c' \
       -o -name 'yvex_prefill_layers.c' \
       -o -name 'yvex_prefill_runtime.c' \
       -o -name 'yvex_graph_prefill.c' \
       -o -name 'yvex_runtime_prefill.c' \
       -o -name 'prefill_chunks.c' \
       -o -name 'prefill_layers.c' \
       -o -name 'graph_prefill.c' \) \
    -print |
  grep -vE '^\./src/cli/io/yvex_cli_(out|error|json|table|log)\.(c|h)$' || true
)"
if [ -n "$bad_command_files" ]; then
  echo "$bad_command_files"
  echo "unexpected command/CLI split files"
  exit 1
fi

if grep -nE 'FILE_OR_ALIAS|--execute-op|--execute-fixture|--execute-partial|--execute-segment|--attach-kv|--expect-sha256|--native-source|standalone F32|standalone RoPE|DeepSeek|GGUF descriptor' src/cli/yvex_cli.c; then
  echo "long command catalog text must not live in yvex_cli.c"
  exit 1
fi

if grep -nE 'open_artifact_for_gguf|open_model_context|open_tokenizer_context|close_model_context|close_tokenizer_context|print_quoted_bytes|print_tensor_dims|print_native_dims|parse_id_list|parse_positive_ull|parse_ull_allow_zero|parse_uint_allow_zero|parse_dims_csv' src/cli/yvex_cli.c; then
  echo "shared command helpers must not live in yvex_cli.c"
  exit 1
fi

if grep -E 'cli_rope_reference|cli_attention_reference|cli_matmul_reference|cli_mlp_reference|command_graph_execute_rope_op|command_graph_execute_attention_op|command_graph_execute_matmul_op|command_graph_execute_mlp_op' src/cli/yvex_cli.c >/dev/null; then
  echo "graph reference/proof implementations must not live in yvex_cli.c"
  exit 1
fi

cli_lines="$(wc -l < src/cli/yvex_cli.c)"
if [ "$cli_lines" -gt 360 ]; then
  echo "yvex_cli.c is too large: $cli_lines lines"
  exit 1
fi

root_source_files="$(git ls-files 'yvex_*.c' 'yvex_*_private.h')"
if [ -n "$root_source_files" ]; then
  echo "$root_source_files"
  echo "root yvex source/private header files are forbidden after TOPOLOGY.FS.0"
  exit 1
fi

echo "source layout: ok"
