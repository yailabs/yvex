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

test -d src
test -d src/app
test -d src/cli
test -d src/cli/commands
test -d src/cli/render
test -d src/cli/io
test -d src/cli/catalog
test -d src/cli/schema
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
test -f src/runtime/yvex_chat.c
test -f src/cli/commands/yvex_model_artifacts_cli.c
test -f src/source/yvex_source.c
test -f src/cli/commands/yvex_source_cli.c
test -f src/generation/yvex_prefill.c
test -f src/generation/yvex_kv.c
test -f src/cli/commands/yvex_kv_cli.c
test -f src/generation/yvex_decode.c
test -f src/cli/commands/yvex_decode_cli.c
test -f src/generation/yvex_logits.c
test -f src/cli/commands/yvex_logits_cli.c
test -f src/generation/yvex_sampling.c
test -f src/cli/commands/yvex_sampling_cli.c
test -f src/generation/yvex_generation.c
test -f src/cli/commands/yvex_generate_cli.c
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
test -f src/gguf/gguf.c
test -f src/gguf/naming.c
test -f src/gguf/tools.c
test -f src/cli/commands/yvex_gguf_cli.c
test -f src/gguf/conversion.c
test -f src/cli/commands/yvex_conversion_cli.c
test -f src/gguf/families.h
test -f src/gguf/quant.c
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
grep -nF 'model-target command grammar' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'usage/help' src/cli/commands/yvex_model_target_cli.c >/dev/null
grep -nF 'report rendering' src/cli/commands/yvex_model_target_cli.c >/dev/null
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

grep -nF 'usage: yvex model-target' src/cli/commands/yvex_model_target_cli.c >/dev/null

DOMAIN_FILES='src/accounts src/artifact src/backend src/core src/daemon src/gguf src/graph src/metrics src/model src/runtime src/source src/tokenizer src/generation'

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
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/backend/yvex_backend.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/graph/yvex_graph.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/source/yvex_source.c' >/dev/null
printf '%s\n' "$CORE_BLOCK" | grep -F 'src/generation/yvex_generation.c' >/dev/null
if printf '%s\n' "$CORE_BLOCK" | grep -F 'src/cli/'; then
  echo "CORE_SRCS must not include CLI sources"
  exit 1
fi
grep -nF 'CLI_SRCS :=' Makefile >/dev/null
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
       -o -name 'yvex_graph_guard.c' \
       -o -name 'yvex_graph_primitives.c' \
       -o -name 'yvex_graph_private.h' \
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
