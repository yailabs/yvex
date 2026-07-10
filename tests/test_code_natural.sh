#!/usr/bin/env sh
set -eu

impl_files="$(
  git ls-files '*.c' '*.cu' |
    grep -v '^tests/' |
    grep -v '^build/' |
    while IFS= read -r path; do
      test -f "$path" && printf '%s\n' "$path"
    done || true
)"

git grep -n \
  -e 'compressed implementation unit' \
  -e 'inlined yvex_' \
  -e '===== yvex_' \
  -e '===== src/gguf/' \
  -e '===== models/' \
  -- '*.c' '*.h' src && {
    echo "mechanical source markers found"
    exit 1
  } || true

git grep -n '^#ifndef .*_H$' -- $impl_files && {
    echo "header guard found inside C implementation"
    exit 1
  } || true

awk '
function reset_file() {
    seen_code = 0
    in_block_comment = 0
}

FNR == 1 {
    reset_file()
}

{
    line = $0

    if (in_block_comment) {
        if (line ~ /\*\//) {
            in_block_comment = 0
        }
        next
    }

    if (line ~ /^[[:space:]]*$/) {
        next
    }
    if (line ~ /^[[:space:]]*\/\*/) {
        if (line !~ /\*\//) {
            in_block_comment = 1
        }
        next
    }
    if (line ~ /^[[:space:]]*\/\//) {
        next
    }
    if (line ~ /^#include/) {
        if (seen_code) {
            print FILENAME ":" FNR ": include after code"
            bad = 1
        }
        next
    }
    if (line ~ /^#/) {
        next
    }

    seen_code = 1
}

END {
    exit bad ? 1 : 0
}
' \
  $impl_files

test ! -f yvex_internal.h
test -z "$(git ls-files 'yvex_*.c')"
test -z "$(git ls-files 'yvex_*_private.h')"

if grep -nF 'usage: yvex model-target' src/model/yvex_model.c; then
  echo "model-target usage text must not live in src/model/yvex_model.c"
  exit 1
fi

if grep -nF 'yvex model-target' src/model/yvex_model.c; then
  echo "model-target command text must not live in src/model/yvex_model.c"
  exit 1
fi

grep -nF 'yvex_model_target_render_help' src/cli/render/yvex_model_target_render.c >/dev/null

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

PLACEHOLDER_RENDER_HITS="$(
  git grep -nE 'renderer-only|const void \*report|present/not-bound|not-bound|_render_boundary' -- \
    'src/cli/render/**' || true
)"
if test -n "$PLACEHOLDER_RENDER_HITS"; then
  echo "$PLACEHOLDER_RENDER_HITS"
  echo "placeholder renderers are forbidden"
  exit 1
fi

grep -nF 'const yvex_source_report *report' src/cli/render/yvex_source_render.c >/dev/null
grep -nF 'yvex_cli_out_writef' src/cli/render/yvex_source_render.c >/dev/null
if grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_source_render.c; then
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

KV_DOMAIN_FILES='src/generation/yvex_kv.c src/generation/yvex_kv_report.c src/generation/yvex_kv_report.h src/generation/yvex_kv_private.h'

KV_DOMAIN_OUTPUT_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $KV_DOMAIN_FILES || true
)"
if test -n "$KV_DOMAIN_OUTPUT_HITS"; then
  echo "$KV_DOMAIN_OUTPUT_HITS"
  echo "no KV direct output in generation domain"
  exit 1
fi

KV_DOMAIN_COMMAND_HITS="$(
  grep -nE 'yvex_kv_command|yvex_kv_help|command_kv|command_kv_report|argc|argv|arg_count|args|usage: yvex|--model|--family|--backend|--output|--audit|--layers|--heads|--capacity' $KV_DOMAIN_FILES || true
)"
if test -n "$KV_DOMAIN_COMMAND_HITS"; then
  echo "$KV_DOMAIN_COMMAND_HITS"
  echo "no KV command/help/argv parsing in generation domain"
  exit 1
fi

KV_DOMAIN_INCLUDE_HITS="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $KV_DOMAIN_FILES || true
)"
if test -n "$KV_DOMAIN_INCLUDE_HITS"; then
  echo "$KV_DOMAIN_INCLUDE_HITS"
  echo "no KV CLI/operator include in generation domain"
  exit 1
fi

grep -nF 'yvex_kv_args_parse' src/cli/input/yvex_kv_args.c >/dev/null
grep -nF 'yvex_kv_report_build' src/generation/yvex_kv_report.c >/dev/null
grep -nF 'yvex_kv_ownership_report_build' src/generation/yvex_kv_report.c >/dev/null
grep -nF 'const yvex_kv_report *report' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_normal' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_audit' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_render_help' src/cli/render/yvex_kv_render.c >/dev/null
grep -nF 'yvex_kv_command' src/cli/commands/yvex_kv_cli.c >/dev/null
grep -nF 'yvex_kv_args_parse' src/cli/commands/yvex_kv_cli.c >/dev/null
grep -nF 'yvex_kv_report_build' src/cli/commands/yvex_kv_cli.c >/dev/null
grep -nF 'yvex_kv_render' src/cli/commands/yvex_kv_cli.c >/dev/null

KV_RENDER_STDIO_HITS="$(
  grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_kv_render.c src/cli/render/yvex_kv_render.h || true
)"
if test -n "$KV_RENDER_STDIO_HITS"; then
  echo "$KV_RENDER_STDIO_HITS"
  echo "no KV renderer direct stdio"
  exit 1
fi

KV_RENDER_PLACEHOLDER_HITS="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_kv_render.c src/cli/render/yvex_kv_render.h || true
)"
if test -n "$KV_RENDER_PLACEHOLDER_HITS"; then
  echo "$KV_RENDER_PLACEHOLDER_HITS"
  echo "no KV placeholder renderer"
  exit 1
fi

if grep -nF 'void kv_cli_adapter_file(void)' src/cli/commands/yvex_kv_cli.c; then
  echo "no empty KV command adapter"
  exit 1
fi

SAMPLING_DOMAIN_FILES='src/generation/yvex_sampling.c src/generation/yvex_sampling_report.c src/generation/yvex_sampling_report.h src/generation/yvex_sampling_private.h'

SAMPLING_OUTPUT_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$SAMPLING_OUTPUT_HITS"; then
  echo "$SAMPLING_OUTPUT_HITS"
  echo "no sample direct output in generation domain"
  exit 1
fi

SAMPLING_COMMAND_HITS="$(
  grep -nE 'yvex_sample_command|yvex_sample_help|argc|argv|arg_count|args|usage: yvex|--model|--backend|--segment|--tokens|--strategy|--logits-count|--attach-kv|--layers|--context-length' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$SAMPLING_COMMAND_HITS"; then
  echo "$SAMPLING_COMMAND_HITS"
  echo "no sample command/help/argv parsing in generation domain"
  exit 1
fi

SAMPLING_INCLUDE_HITS="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $SAMPLING_DOMAIN_FILES || true
)"
if test -n "$SAMPLING_INCLUDE_HITS"; then
  echo "$SAMPLING_INCLUDE_HITS"
  echo "no sample CLI/operator include in generation domain"
  exit 1
fi

grep -nF 'const yvex_sampling_report *report' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_normal' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_audit' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_render_help' src/cli/render/yvex_sampling_render.c >/dev/null
grep -nF 'yvex_sampling_args_parse' src/cli/commands/yvex_sampling_cli.c >/dev/null
grep -nF 'yvex_sampling_report_build' src/cli/commands/yvex_sampling_cli.c >/dev/null
grep -nF 'yvex_sampling_render' src/cli/commands/yvex_sampling_cli.c >/dev/null

SAMPLING_RENDER_STDIO_HITS="$(
  grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_sampling_render.c src/cli/render/yvex_sampling_render.h || true
)"
if test -n "$SAMPLING_RENDER_STDIO_HITS"; then
  echo "$SAMPLING_RENDER_STDIO_HITS"
  echo "no sample renderer direct stdio"
  exit 1
fi

SAMPLING_RENDER_PLACEHOLDER_HITS="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_sampling_render.c src/cli/render/yvex_sampling_render.h || true
)"
if test -n "$SAMPLING_RENDER_PLACEHOLDER_HITS"; then
  echo "$SAMPLING_RENDER_PLACEHOLDER_HITS"
  echo "no sample placeholder renderer"
  exit 1
fi

if grep -nF 'void sampling_cli_adapter_file(void)' src/cli/commands/yvex_sampling_cli.c; then
  echo "no empty sample command adapter"
  exit 1
fi

GRAPH_DOMAIN_FILES='src/graph/*.c src/graph/*.h'

GRAPH_OUTPUT_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$GRAPH_OUTPUT_HITS"; then
  echo "$GRAPH_OUTPUT_HITS"
  echo "no graph direct output in graph domain"
  exit 1
fi

GRAPH_COMMAND_HITS="$(
  grep -nE 'yvex_graph_command|yvex_graph_help|argc|argv|arg_count|args|usage: yvex|--model|--backend|--output|--audit|--graph|--plan|--primitive' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$GRAPH_COMMAND_HITS"; then
  echo "$GRAPH_COMMAND_HITS"
  echo "no graph command/help/argv parsing in graph domain"
  exit 1
fi

GRAPH_INCLUDE_HITS="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' $GRAPH_DOMAIN_FILES || true
)"
if test -n "$GRAPH_INCLUDE_HITS"; then
  echo "$GRAPH_INCLUDE_HITS"
  echo "no graph CLI/operator include in graph domain"
  exit 1
fi

grep -nF 'const yvex_graph_report *report' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_normal' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_audit' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_render_help' src/cli/render/yvex_graph_render.c >/dev/null
grep -nF 'yvex_graph_args_parse' src/cli/commands/yvex_graph_cli.c >/dev/null
grep -nE 'yvex_graph_.*report_build|yvex_graph_report_build' src/cli/commands/yvex_graph_cli.c >/dev/null
grep -nF 'yvex_graph_render' src/cli/commands/yvex_graph_cli.c >/dev/null

GRAPH_RENDER_STDIO_HITS="$(
  grep -nE '\b(printf|fprintf|vfprintf|fputs|fputc|puts|putchar|perror)\s*\(' src/cli/render/yvex_graph_render.c src/cli/render/yvex_graph_render.h || true
)"
if test -n "$GRAPH_RENDER_STDIO_HITS"; then
  echo "$GRAPH_RENDER_STDIO_HITS"
  echo "no graph renderer direct stdio"
  exit 1
fi

GRAPH_RENDER_PLACEHOLDER_HITS="$(
  grep -nE 'const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_graph_render.c src/cli/render/yvex_graph_render.h || true
)"
if test -n "$GRAPH_RENDER_PLACEHOLDER_HITS"; then
  echo "$GRAPH_RENDER_PLACEHOLDER_HITS"
  echo "no graph placeholder renderer"
  exit 1
fi

if grep -nF 'void graph_cli_adapter_file(void)' src/cli/commands/yvex_graph_cli.c; then
  echo "no empty graph command adapter"
  exit 1
fi

MODEL_TARGET_CLI_DOMAIN_HITS="$(
  grep -nE 'model_target_classes|model_targets|full_runtime_candidate|dense_candidate|qwen_metal|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|output_head_map_profile|tokenizer_map|qtype_policy|role_support|native_weight|safetensors|source_path' src/cli/commands/yvex_model_target_cli.c || true
)"
if test -n "$MODEL_TARGET_CLI_DOMAIN_HITS"; then
  echo "$MODEL_TARGET_CLI_DOMAIN_HITS"
  echo "no model-target domain facts in CLI command adapter"
  exit 1
fi

model_target_report_lines="$(wc -l < src/model/target/yvex_model_target_report.c | tr -d ' ')"
if test "$model_target_report_lines" -gt 800; then
  echo "no model-target monolith report coordinator"
  exit 1
fi

MODEL_TARGET_COORDINATOR_HITS="$(
  grep -nE 'model_target_classes|model_targets|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|output_head_map_profile|native_weight|safetensors|source_path|model_target_capture_out|model_target_capture_err|model_target_out_writef|captured report text' src/model/target/yvex_model_target_report.c || true
)"
if test -n "$MODEL_TARGET_COORDINATOR_HITS"; then
  echo "$MODEL_TARGET_COORDINATOR_HITS"
  echo "no model-target monolith report coordinator"
  exit 1
fi

MODEL_TARGET_TEXT_COMPAT_HITS="$(
  grep -nE 'primary_text|diagnostic_text|primary_len|diagnostic_len|model_target_capture_out|model_target_capture_err|captured report text|raw_output|report_text|captured_text|line_buffer|model_target_sink_out|model_target_sink_err' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_TEXT_COMPAT_HITS"; then
  echo "$MODEL_TARGET_TEXT_COMPAT_HITS"
  echo "no captured output buffers in model-target report layer"
  exit 1
fi

MODEL_TARGET_RUNNER_FILES="$(
  find src/model/target -maxdepth 1 -type f \( -name '*runner*' -o -name '*internal*' -o -name '*compat*' -o -name '*backend*' -o -name '*bridge*' -o -name '*shim*' \) -print
)"
if test -n "$MODEL_TARGET_RUNNER_FILES"; then
  echo "$MODEL_TARGET_RUNNER_FILES"
  echo "no model-target shared execution compatibility files"
  exit 1
fi

MODEL_TARGET_RUNNER_SYMBOL_HITS="$(
  grep -nE 'yvex_model_target_runner|runner_report_build|_runner|\brunner\b|compatibility backend|internal backend|shared report execution|report execution support' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_RUNNER_SYMBOL_HITS"; then
  echo "$MODEL_TARGET_RUNNER_SYMBOL_HITS"
  echo "no model-target shared execution runner symbols"
  exit 1
fi

MODEL_TARGET_CLI_SHAPED_REQUEST_HITS="$(
  grep -nE '\b(argc|argv)\b' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_CLI_SHAPED_REQUEST_HITS"; then
  echo "$MODEL_TARGET_CLI_SHAPED_REQUEST_HITS"
  echo "no argc/argv in model-target domain or renderer"
  exit 1
fi

MODEL_TARGET_FAKE_FILE_HITS="$(
  grep -nE '\(FILE \*\)\(uintptr_t\)|model_target_text_write|model_target_segment_append|model_target_out_writef|model_target_out|model_target_err|model_target_file_char|model_target_sink' src/model/target/*.c src/model/target/*.h src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_FAKE_FILE_HITS"; then
  echo "$MODEL_TARGET_FAKE_FILE_HITS"
  echo "no model-target fake FILE or sink output"
  exit 1
fi

if test -f src/model/target/yvex_model_target_internal.c; then
  model_target_internal_lines="$(wc -l < src/model/target/yvex_model_target_internal.c | tr -d ' ')"
  if test "$model_target_internal_lines" -gt 300; then
    echo "no model-target internal backend over 300 lines"
    exit 1
  fi
  MODEL_TARGET_INTERNAL_HITS="$(
    grep -nE 'model_target_sink|model_target_out_writef|model_target_stream_write|fwrite|fprintf|printf|model_target_classes|model_targets|model_class_profile_specs|tensor_collection_profile|tensor_naming_profile|qtype_policy|role_support|safetensors|native_weight' src/model/target/yvex_model_target_internal.c || true
  )"
  if test -n "$MODEL_TARGET_INTERNAL_HITS"; then
    echo "$MODEL_TARGET_INTERNAL_HITS"
    echo "no model-target internal backend report facts or output helpers"
    exit 1
  fi
fi

MODEL_TARGET_INTERNAL_DELEGATION_HITS="$(
  grep -nE 'yvex_model_target_internal_report|yvex_model_target_internal_.*build|return yvex_model_target_internal_|internal backend|compatibility backend' src/model/target/*.c src/model/target/*.h || true
)"
if test -n "$MODEL_TARGET_INTERNAL_DELEGATION_HITS"; then
  echo "$MODEL_TARGET_INTERNAL_DELEGATION_HITS"
  echo "no model-target internal backend delegation"
  exit 1
fi

MODEL_TARGET_BOUNDARY_SHELL_HITS="$(
  grep -nE 'typedef int yvex_.*_file_boundary' src/model/target/*.c || true
)"
if test -n "$MODEL_TARGET_BOUNDARY_SHELL_HITS"; then
  echo "$MODEL_TARGET_BOUNDARY_SHELL_HITS"
  echo "no boundary typedef shells in model-target report modules"
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
    echo "no model-target report module under 80 lines unless explicitly a header"
    exit 1
  fi
  grep -nE '[_a-zA-Z0-9]+_build[[:space:]]*\(' "$module" >/dev/null
done

for source_file in src/model/target/*.c
do
  source_lines="$(wc -l < "$source_file" | tr -d ' ')"
  if test "$source_lines" -gt 2500; then
    echo "no model-target source file over 2500 lines"
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
  echo "no empty model-target command adapter"
  exit 1
fi

grep -nF 'yvex_model_target_args_parse' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'yvex_model_target_report_build' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'yvex_model_target_render' src/cli/commands/yvex_model_target_cli.c >/dev/null

MODEL_TARGET_DOMAIN_OUTPUT_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/target/*.c src/model/target/*.h |
    grep -vE '^src/model/target/yvex_model_target_sidecar_write\.c:' || true
)"
if test -n "$MODEL_TARGET_DOMAIN_OUTPUT_HITS"; then
  echo "$MODEL_TARGET_DOMAIN_OUTPUT_HITS"
  echo "no model-target direct output in domain/report"
  exit 1
fi

MODEL_TARGET_DOMAIN_INCLUDE_HITS="$(
  grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' src/model/target/*.c src/model/target/*.h || true
)"
if test -n "$MODEL_TARGET_DOMAIN_INCLUDE_HITS"; then
  echo "$MODEL_TARGET_DOMAIN_INCLUDE_HITS"
  echo "no model-target CLI/operator include in domain/report"
  exit 1
fi

MODEL_TARGET_RENDER_STDIO_HITS="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror)\s*\(' src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_RENDER_STDIO_HITS"; then
  echo "$MODEL_TARGET_RENDER_STDIO_HITS"
  echo "no model-target renderer direct stdio"
  exit 1
fi

MODEL_TARGET_RENDER_PLACEHOLDER_HITS="$(
  grep -nE 'captured_text|report_text|raw_output|const void \*report|not-bound|renderer-only|_render_boundary|boundary anchor' src/cli/render/yvex_model_target_render.c src/cli/render/yvex_model_target_render.h || true
)"
if test -n "$MODEL_TARGET_RENDER_PLACEHOLDER_HITS"; then
  echo "$MODEL_TARGET_RENDER_PLACEHOLDER_HITS"
  echo "no model-target placeholder renderer"
  exit 1
fi

if grep -RInE '#include .*src/cli|#include "yvex_(cli|console|render)' src/source; then
  echo "source cell must not include CLI headers"
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

awk '
FNR == 1 {
    in_header = 0
    checked = 0
}
FNR <= 24 && checked == 0 {
    if ($0 ~ /^[[:space:]]*\/\*/) {
        in_header = 1
    }
    if (in_header && $0 ~ /command implementations|helper functions|miscellaneous|glue|stuff|TODO|future implementation goes here|placeholder/) {
        print FILENAME ":" FNR ": vague file header text"
        bad = 1
    }
    if (in_header && $0 ~ /\*\//) {
        checked = 1
        in_header = 0
    }
}
END {
    exit bad ? 1 : 0
}
' \
  $impl_files

awk '
FNR == 1 {
    in_comment = 0
}
{
    line = $0
    comment_line = 0

    if (in_comment) {
        comment_line = 1
        if (line ~ /\*\//) {
            in_comment = 0
        }
    } else if (line ~ /^[[:space:]]*\/\*/) {
        comment_line = 1
        if (line !~ /\*\//) {
            in_comment = 1
        }
    } else if (line ~ /^[[:space:]]*\/\//) {
        comment_line = 1
    }

    if (!comment_line) {
        next
    }

    if (line ~ /handles things|does stuff|helper function|process data|business logic|simple wrapper around everything/) {
        print FILENAME ":" FNR ": vague source comment text"
        bad = 1
    }
    if (line ~ /magic/ && line !~ /GGUF magic|YVEX_GGUF_MAGIC|bad GGUF magic/) {
        print FILENAME ":" FNR ": vague source comment text"
        bad = 1
    }
}
END {
    exit bad ? 1 : 0
}
' \
  $impl_files

for f in \
  src/model/yvex_model.c \
  src/cli/commands/yvex_model_target_cli.c \
  src/cli/yvex_cli.c \
  src/cli/commands/yvex_runtime_cli.c \
  src/cli/commands/yvex_generate_cli.c \
  src/cli/commands/yvex_artifact_cli.c \
  src/cli/commands/yvex_source_cli.c
do
  grep -nF 'Purpose:' "$f" >/dev/null
  grep -nF 'Inputs:' "$f" >/dev/null
  grep -nF 'Effects:' "$f" >/dev/null
  grep -nF 'Failure:' "$f" >/dev/null
  grep -nF 'Boundary:' "$f" >/dev/null
done

grep -nF 'command dispatch only' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'typed report' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'typed renderer' src/cli/render/yvex_model_target_render.c >/dev/null
grep -nF 'does not create capability' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'model metadata/materialization facts are not model support' src/model/yvex_model.c >/dev/null
grep -nF 'tensor payload bytes' src/model/yvex_model.c >/dev/null
grep -nF 'materialization' src/model/yvex_model.c >/dev/null

if grep -nE 'implemented|ready|supports generation|benchmark result|token/sec|evaluation suite implemented|generation implemented' \
    src/cli/commands/yvex_generate_cli.c src/eval/yvex_eval.c src/bench/yvex_bench.c; then
  echo "future boundary files must not claim runtime readiness"
  exit 1
fi

scan_forbidden_claim() {
  pattern=$1
  if git grep -nF "$pattern" -- README.md MODEL_ARTIFACTS.md docs '*.c' include tests; then
    echo "forbidden capability claim found: $pattern"
    exit 1
  fi
}

pattern='GLM sup''port'
scan_forbidden_claim "$pattern"
pattern='GLM generation rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM execution rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM runtime rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM inference imple''mented'
scan_forbidden_claim "$pattern"
pattern='YVEX supports GLM gener''ation'
scan_forbidden_claim "$pattern"
pattern='Qwen source down''loaded'
scan_forbidden_claim "$pattern"
pattern='Qwen source veri''fied'
scan_forbidden_claim "$pattern"
pattern='Qwen suppor''ted'
scan_forbidden_claim "$pattern"
pattern='Qwen runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen CUDA runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma source down''loaded'
scan_forbidden_claim "$pattern"
pattern='Gemma source veri''fied'
scan_forbidden_claim "$pattern"
pattern='native inventory implemented for Qw''en'
scan_forbidden_claim "$pattern"
pattern='native inventory implemented for Gem''ma'
scan_forbidden_claim "$pattern"
pattern='native inventory proves source rea''dy'
scan_forbidden_claim "$pattern"
pattern='native_inventory_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source tensor roles map''ped'
scan_forbidden_claim "$pattern"
pattern='tensor roles map''ped'
scan_forbidden_claim "$pattern"
pattern='Qwen model class imple''mented'
scan_forbidden_claim "$pattern"
pattern='Qwen model class rea''dy'
scan_forbidden_claim "$pattern"
pattern='model_class_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='Gemma model class imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Gemma CUDA runtime imple''mented'
scan_forbidden_claim "$pattern"
pattern='Metal support imple''mented'
scan_forbidden_claim "$pattern"
pattern='CUDA inference rea''dy'
scan_forbidden_claim "$pattern"
pattern='CUDA generation rea''dy'
scan_forbidden_claim "$pattern"
pattern='CUDA runtime rea''dy'
scan_forbidden_claim "$pattern"
pattern='FlashAttention imple''mented'
scan_forbidden_claim "$pattern"
pattern='tensor core GEMM imple''mented'
scan_forbidden_claim "$pattern"
pattern='source_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source_manifest_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='metadata_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='source_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_identity_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_hash_veri''fied: true'
scan_forbidden_claim "$pattern"
pattern='source_remote_chec''ked: true'
scan_forbidden_claim "$pattern"
pattern='payload_loa''ded: true'
scan_forbidden_claim "$pattern"
pattern='native_safetensors_payload_loa''ded: true'
scan_forbidden_claim "$pattern"
pattern='generation_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: measu''red'
scan_forbidden_claim "$pattern"
pattern='benchmark mea''sured'
scan_forbidden_claim "$pattern"
pattern='release_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='20 tok/s achie''ved'
scan_forbidden_claim "$pattern"
pattern='external GGUF satisfies OW''I'
scan_forbidden_claim "$pattern"
pattern='external runner proves YV''EX'
scan_forbidden_claim "$pattern"
pattern='model target is capab''ility'
scan_forbidden_claim "$pattern"
pattern='full inference imple''mented'
scan_forbidden_claim "$pattern"
pattern='full transformer prefill imple''mented'
scan_forbidden_claim "$pattern"
pattern='transformer inference rea''dy'
scan_forbidden_claim "$pattern"
pattern='DeepSeek block rea''dy'
scan_forbidden_claim "$pattern"
pattern='GLM block rea''dy'
scan_forbidden_claim "$pattern"
pattern='generation rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='release_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='runtime_claim: sup''ported'
scan_forbidden_claim "$pattern"
pattern='generation: sup''ported'
scan_forbidden_claim "$pattern"
pattern='full_model_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='real_deepseek_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: meas''ured'
scan_forbidden_claim "$pattern"
pattern='decode rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='logits rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='first token gener''ated'
scan_forbidden_claim "$pattern"
pattern='token/sec'':'
scan_forbidden_claim "$pattern"
pattern='benchmark pa''ss'
scan_forbidden_claim "$pattern"
pattern='models check imple''mented'
scan_forbidden_claim "$pattern"
pattern='graph check imple''mented'
scan_forbidden_claim "$pattern"
pattern='DeepSeek generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='DeepSeek CUDA generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='prepare material''izes'
scan_forbidden_claim "$pattern"
pattern='prepare runs gr''aph'
scan_forbidden_claim "$pattern"
pattern='prepare starts ser''ver'
scan_forbidden_claim "$pattern"
pattern='prepare runs ch''at'
scan_forbidden_claim "$pattern"
pattern='prepare dec''odes'
scan_forbidden_claim "$pattern"
pattern='prepare produces log''its'
scan_forbidden_claim "$pattern"
pattern='prepare sam''ples'
scan_forbidden_claim "$pattern"
pattern='prepare gener''ates'
scan_forbidden_claim "$pattern"
pattern='prepare bench''marks'
scan_forbidden_claim "$pattern"
pattern='check material''izes as generation'
scan_forbidden_claim "$pattern"
pattern='check runs gr''aph as generation'
scan_forbidden_claim "$pattern"
pattern='check produces log''its'
scan_forbidden_claim "$pattern"
pattern='check gener''ates'
scan_forbidden_claim "$pattern"
pattern='20 tok''/s achie''ved'
scan_forbidden_claim "$pattern"
pattern='DSpark par''ity'
scan_forbidden_claim "$pattern"
pattern='decode_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='logits_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='sampling_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='generation_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='full_model_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='real_deepseek_generation: tr''ue'
scan_forbidden_claim "$pattern"
pattern='benchmark_status: mea''sured'
scan_forbidden_claim "$pattern"
pattern='qwen_runtime_status: sup''ported'
scan_forbidden_claim "$pattern"
pattern='metal_backend_status: sup''ported'
scan_forbidden_claim "$pattern"
pattern='Qwen generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='Metal support imple''mented'
scan_forbidden_claim "$pattern"
pattern='provider generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='raw_token_stored_by_yvex: tr''ue'
scan_forbidden_claim "$pattern"
pattern='streaming generation imple''mented'
scan_forbidden_claim "$pattern"
pattern='execution_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='graph_execution_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='inference_rea''dy: true'
scan_forbidden_claim "$pattern"
pattern='full_transformer_prefill_ready: tr''ue'
scan_forbidden_claim "$pattern"
pattern='operator path gener''ates'
scan_forbidden_claim "$pattern"
pattern='paths configure down''loads'
scan_forbidden_claim "$pattern"
pattern='paths configure creates art''ifacts'
scan_forbidden_claim "$pattern"
pattern='target paths prepare mod''els'
scan_forbidden_claim "$pattern"
pattern='target paths check mod''els'
scan_forbidden_claim "$pattern"
pattern='target paths create art''ifacts'
scan_forbidden_claim "$pattern"
pattern='target paths register ali''ases'
scan_forbidden_claim "$pattern"
pattern='target paths inspect safeten''sors'
scan_forbidden_claim "$pattern"
pattern='target paths inspect GG''UF'
scan_forbidden_claim "$pattern"

if awk '
  /\b(system|popen|execl|execv|fork)[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
    bad = 1
  }
  END {
    exit bad ? 1 : 0
  }
' src/cli/commands/yvex_model_artifacts_cli.c; then
  :
else
  echo "models command owners must not shell out; provider execution belongs to accounts"
  exit 1
fi

model_artifacts_command_lines="$(wc -l < src/cli/commands/yvex_model_artifacts_cli.c | tr -d ' ')"
if test "$model_artifacts_command_lines" -gt 350; then
  echo "model-artifacts command adapter exceeds 350 lines"
  exit 1
fi

grep -nF 'yvex_model_artifacts_args_parse' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifact_report_build' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null
grep -nF 'yvex_model_artifacts_render' src/cli/commands/yvex_model_artifacts_cli.c >/dev/null

if grep -nE 'yvex_operator_private|yvex_operator_render_private|write_escaped|write_field|write_u64_field|model_artifacts_cli_strdup|path_exists|is_path_like_reference|set_path_ref|yvex_model_registry|yvex_model_ref|yvex_model_gate|yvex_artifact|yvex_backend|yvex_weight_table|native_weight|safetensors|mkdir|access\(|opendir|readdir|poll|waitpid|signal|printf|fprintf|fwrite' src/cli/commands/yvex_model_artifacts_cli.c; then
  echo "no model-artifacts domain/path/render helpers in command adapter"
  exit 1
fi

model_artifacts_root_lines="$(wc -l < src/model/yvex_model_artifacts.c | tr -d ' ')"
if test "$model_artifacts_root_lines" -gt 500; then
  echo "no model-artifacts root monolith"
  exit 1
fi

if grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/yvex_model_artifacts.c; then
  echo "no direct output in model-artifacts root"
  exit 1
fi

if grep -nE '#include "yvex_(cli|operator|console)|yvex_operator_render|#include <.*cli.*>|src/cli' src/model/yvex_model_artifacts.c src/model/artifacts/*.c src/model/artifacts/*.h; then
  echo "no CLI/operator include in model-artifacts domain"
  exit 1
fi

if grep -nE '\b(argc|argv)\b|usage:|--help|--output|--audit' src/model/yvex_model_artifacts.c src/model/artifacts/*.c src/model/artifacts/*.h; then
  echo "no CLI-shaped input in model-artifacts domain"
  exit 1
fi

if grep -nE '\b(argc|argv)\b' src/cli/render/yvex_model_artifacts_render.c src/cli/render/yvex_model_artifacts_render.h; then
  echo "no CLI-shaped input in model-artifacts domain/render"
  exit 1
fi

BAD_MODEL_ARTIFACTS_OUTPUT="$(
  grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' src/model/artifacts/*.c src/model/artifacts/*.h |
    grep -vE '^src/model/artifacts/yvex_model_artifact_write\.c:' || true
)"
if test -n "$BAD_MODEL_ARTIFACTS_OUTPUT"; then
  echo "$BAD_MODEL_ARTIFACTS_OUTPUT"
  echo "no direct output in model-artifacts domain/report"
  exit 1
fi

if grep -nE 'stdout|stderr' src/model/artifacts/yvex_model_artifact_write.c src/model/artifacts/yvex_model_artifact_write.h; then
  echo "model-artifacts writer must not write stdout/stderr"
  exit 1
fi

if grep -nE 'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer' src/model/artifacts/*.c src/model/artifacts/*.h src/cli/render/yvex_model_artifacts_render.c src/cli/render/yvex_model_artifacts_render.h; then
  echo "no model-artifacts text-buffer report debt"
  exit 1
fi

if test -f src/cli/model_artifacts/yvex_model_artifacts_surface.c; then
  model_artifacts_surface_lines="$(wc -l < src/cli/model_artifacts/yvex_model_artifacts_surface.c | tr -d ' ')"
  if test "$model_artifacts_surface_lines" -gt 350; then
    echo "no model-artifacts root CLI surface monolith"
    exit 1
  fi
fi

model_artifacts_common_lines="$(wc -l < src/cli/model_artifacts/yvex_model_artifacts_surface_common.c | tr -d ' ')"
if test "$model_artifacts_common_lines" -gt 500; then
  echo "no hidden model-artifacts common monolith"
  exit 1
fi

if find src/cli/model_artifacts -name '*.c' -print0 |
    xargs -0 wc -l |
    awk '$1 ~ /^[0-9]+$/ && $2 != "total" && $1 > 2500 { print; bad=1 } END { exit bad ? 1 : 0 }'; then
  :
else
  echo "no giant model-artifacts CLI surface file"
  exit 1
fi

if grep -nE 'yvex_cli_out_writef|yvex_cli_out_fputs|yvex_cli_out_line|yvex_cli_out_char|yvex_cli_out_kv_|yvex_cli_table_|yvex_cli_json_|printf|fprintf|fwrite|write_escaped|write_field|write_u64_field|print_' src/cli/model_artifacts/*.c; then
  echo "model-artifacts surface files must not format operator output"
  exit 1
fi

if grep -nE 'parse_models_prepare_options|parse_models_output_mode|parse_.*_options|strcmp\(args\[|strcmp\(argv\[|for .*arg_count|for .*argc' src/cli/model_artifacts/*.c; then
  echo "model-artifacts surface files must not own parser-heavy argv scans"
  exit 1
fi

if find src/cli/model_artifacts -name '*.c' -print0 |
    xargs -0 wc -l |
    awk '$1 ~ /^[0-9]+$/ && $2 != "total" && $1 > 1500 { print; bad=1 } END { exit bad ? 1 : 0 }'; then
  :
else
  echo "model-artifacts surface file exceeds 1500 lines"
  exit 1
fi

test -f src/cli/render/yvex_models_render.c
test -f src/cli/render/yvex_models_download_render.c
test -f src/cli/render/yvex_models_prepare_render.c
test -f src/cli/render/yvex_fullmodel_render.c
test -f src/cli/render/yvex_attention_render.c
test -f src/cli/render/yvex_context_render.c
test -f src/cli/render/yvex_moe_render.c
test -f src/cli/render/yvex_tensor_collection_render.c

if git grep -n 'src/cli/model_artifacts' -- Makefile | grep CORE_SRCS; then
  echo "model-artifacts CLI surfaces must not enter CORE_SRCS"
  exit 1
fi

if grep -nE '\b(system|popen|execl)[[:space:]]*\(' src/cli/commands/yvex_accounts_cli.c; then
  echo "accounts must use bounded exec helpers only"
  exit 1
fi

runtime_files="$(git ls-files 'src/cli/commands/yvex_runtime_cli.c' 'src/cli/commands/yvex_backend_cli.c' 'src/cli/commands/yvex_tokenizer_cli.c' 'src/tokenizer/yvex_token_input.c' 'src/cli/commands/yvex_kv_cli.c' 'src/generation/yvex_prefill.c' 'src/cli/commands/yvex_paths_cli.c' 2>/dev/null || true)"

if [ -n "$runtime_files" ] && grep -nE '\b(system|popen|execl|execv|fork)[[:space:]]*\(' $runtime_files; then
  echo "runtime ownership files must not shell out"
  exit 1
fi

scan_runtime_claim() {
  pattern=$1
  if [ -n "$runtime_files" ] && git grep -nF "$pattern" -- $runtime_files; then
    echo "forbidden runtime claim found: $pattern"
    exit 1
  fi
}

pattern='execution_ready: tr''ue'
scan_runtime_claim "$pattern"
pattern='generation_ready: tr''ue'
scan_runtime_claim "$pattern"
pattern='generation rea''dy'
scan_runtime_claim "$pattern"
pattern='inference rea''dy'
scan_runtime_claim "$pattern"
pattern='DeepSeek generation imple''mented'
scan_runtime_claim "$pattern"

system_target_files="
src/gguf/yvex_gguf_private.h
src/gguf/yvex_gguf_container.c
src/gguf/yvex_gguf_metadata.c
src/gguf/yvex_gguf_tensor_info.c
src/gguf/yvex_gguf_qtype.c
src/gguf/yvex_gguf_range_map.c
src/gguf/yvex_gguf_reader.c
src/gguf/yvex_gguf_writer.c
src/gguf/yvex_gguf_roundtrip.c
src/gguf/yvex_gguf_name_map.c
src/gguf/yvex_gguf_layout_map.c
src/gguf/yvex_gguf_descriptor.c
src/gguf/yvex_gguf_report.c
src/artifact/yvex_artifact_descriptor.c
src/artifact/yvex_artifact_descriptor.h
src/artifact/yvex_artifact_materialize.c
src/artifact/yvex_artifact_materialize.h
src/artifact/yvex_artifact_roundtrip_gate.c
src/artifact/yvex_artifact_roundtrip_gate.h
src/artifact/yvex_artifact_report.c
src/artifact/yvex_artifact_report.h
src/model/yvex_runtime_descriptor.c
src/model/yvex_runtime_descriptor.h
src/model/yvex_runtime_descriptor_report.c
src/model/yvex_runtime_descriptor_report.h
src/graph/yvex_graph_bind.c
src/graph/yvex_graph_bind.h
src/graph/yvex_graph_execute.c
src/graph/yvex_graph_execute.h
src/backend/yvex_backend_tensor.c
src/backend/yvex_backend_tensor.h
src/backend/yvex_backend_qtype.c
src/backend/yvex_backend_qtype.h
src/backend/yvex_backend_report.c
src/backend/yvex_backend_report.h
src/backend/cuda/cuda_qtype.c
src/backend/cuda/cuda_qtype.h
"

for f in $system_target_files; do
  test -f "$f"
  for token in Owner Owns "Does not own" Invariants Boundary; do
    grep -nF "$token" "$f" >/dev/null || {
      echo "system target owner contract missing $token in $f"
      exit 1
    }
  done
done

if git grep -nE 'placeholder|stub|not-bound|renderer-only|adapter slot|boundary anchor|typedef int' -- \
    src/gguf \
    src/artifact \
    src/model/yvex_runtime_descriptor* \
    src/graph/yvex_graph_bind* \
    src/graph/yvex_graph_execute* \
    src/backend/yvex_backend_tensor* \
    src/backend/yvex_backend_qtype* \
    src/backend/yvex_backend_report* \
    src/backend/cuda/cuda_qtype*; then
  echo "system target files must not be placeholders or compile-only anchors"
  exit 1
fi

if git grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fwrite)\s*\(|stdout|stderr' -- \
    src/gguf/yvex_gguf_*.c \
    src/artifact/yvex_artifact_descriptor.c \
    src/artifact/yvex_artifact_materialize.c \
    src/artifact/yvex_artifact_roundtrip_gate.c \
    src/artifact/yvex_artifact_report.c \
    src/model/yvex_runtime_descriptor*.c \
    src/graph/yvex_graph_bind.c \
    src/graph/yvex_graph_execute.c \
    src/backend/yvex_backend_tensor.c \
    src/backend/yvex_backend_qtype.c \
    src/backend/yvex_backend_report.c \
    src/backend/cuda/cuda_qtype.c; then
  echo "system target owner files must not write operator output"
  exit 1
fi

system_target_claim_pattern='writer com''plete|roundtrip com''plete|generation-capable artifact com''plete|materialization com''plete|runtime descriptor rea''dy|graph execution com''plete|qtype compute com''plete|backend runtime generation rea''dy'
if git grep -nE "$system_target_claim_pattern" -- $system_target_files; then
  echo "system target owner files must not introduce capability completion claims"
  exit 1
fi

grep -nF 'yvex_gguf_container_abi_from_header' src/gguf/yvex_gguf_container.c >/dev/null
grep -nF 'yvex_gguf_metadata_abi_from_gguf' src/gguf/yvex_gguf_metadata.c >/dev/null
grep -nF 'yvex_gguf_tensor_info_abi_from_gguf' src/gguf/yvex_gguf_tensor_info.c >/dev/null
grep -nF 'yvex_gguf_range_fact_from_gguf' src/gguf/yvex_gguf_range_map.c >/dev/null
grep -nF 'yvex_gguf_reader_classify_error' src/gguf/yvex_gguf_reader.c >/dev/null
grep -nF 'yvex_gguf_descriptor_abi_from_sections' src/gguf/yvex_gguf_descriptor.c >/dev/null
grep -nF 'yvex_gguf_artifact_abi_report_build' src/gguf/yvex_gguf_report.c >/dev/null
grep -nF 'yvex_gguf_qtype_geometry_find' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_qtype_storage_bytes' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_qtype_validate_tensor_storage' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_qtype_refusal_reason' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_qtype_abi_from_gguf' src/gguf/yvex_gguf_qtype.c >/dev/null
grep -nF 'yvex_gguf_writer_supported' src/gguf/yvex_gguf_writer.c >/dev/null
grep -nF 'yvex_gguf_roundtrip_supported' src/gguf/yvex_gguf_roundtrip.c >/dev/null

if git grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fwrite)\s*\(|stdout|stderr' -- \
    src/gguf/yvex_gguf_*.c \
    src/gguf/yvex_gguf_private.h; then
  echo "GGUF ABI owner files must not write operator output"
  exit 1
fi

gguf_claim_pattern='generation-rea''dy|inference-rea''dy|release-rea''dy|writer com''plete|roundtrip com''plete|generation-capable artifact emit''ted|runtime descriptor rea''dy|benchmark meas''ured|through''put'
if git grep -nE "$gguf_claim_pattern" -- src/gguf docs/system-target.md; then
  echo "GGUF ABI row must not introduce runtime, writer, generation, benchmark, or release claims"
  exit 1
fi
if grep -nE 'V010\.GGUF\.(QTYPE|ARTIFACT)\.ABI\.[01]' PROJECT.md |
    grep -E "$gguf_claim_pattern"; then
  echo "PROJECT.md GGUF ABI rows must not introduce higher capability claims"
  exit 1
fi

gguf_qtype_claim_pattern='compute supp''ort|backend supp''ort|CUDA supp''ort|matmul supp''ort|qtype policy selection com''plete|quantization implementation|writer com''plete|roundtrip com''plete|materialization com''plete'
if git grep -nE "$gguf_qtype_claim_pattern" -- src/gguf/yvex_gguf_qtype.c src/gguf/yvex_gguf_private.h; then
  echo "GGUF qtype ABI owner must stay byte-geometry-only"
  exit 1
fi

echo "code naturalness: ok"
