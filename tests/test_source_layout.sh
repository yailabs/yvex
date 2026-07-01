#!/usr/bin/env sh
set -eu

test ! -d src
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

test -f yvex_cli.c
test ! -f yvex_cli_runtime.c
test -f yvexd.c
test -f yvex_core.c
test -f yvex_fs.c
test -f yvex_model_artifacts.c
test -f yvex_source.c
test -f yvex_prefill.c
test -f yvex_kv.c
test -f yvex_decode.c
test -f yvex_logits.c
test -f yvex_sampling.c
test -f yvex_generation.c
test -f yvex_eval.c
test -f yvex_bench.c
test -f yvex_profile.c
test -f cuda/cuda_backend.c
test -f cuda/cuda_errors.c
test -f cuda/cuda_info.c
test -f cuda/cuda_internal.h
test -f cuda/cuda_kernels.cu
test -f cuda/cuda_kernels.h
test -f cuda/cuda_ops.c
test -f cuda/cuda_tensor.c
test -f gguf/gguf.c
test -f gguf/naming.c
test -f gguf/tools.c
test -f gguf/conversion.c
test -f gguf/families.h
test -f gguf/quant.c

test -d include/yvex
test -d cuda
test -d gguf
test ! -d models
test -d docs
test -d tests
test -d tests/vectors

bad_command_files="$(
  find . -maxdepth 3 \
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
       -o -name 'yvex_prefill_chunks.c' \
       -o -name 'yvex_prefill_layers.c' \
       -o -name 'yvex_prefill_runtime.c' \
       -o -name 'yvex_graph_prefill.c' \
       -o -name 'yvex_runtime_prefill.c' \
       -o -name 'prefill_chunks.c' \
       -o -name 'prefill_layers.c' \
       -o -name 'graph_prefill.c' \) \
    -print
)"
if [ -n "$bad_command_files" ]; then
  echo "$bad_command_files"
  echo "unexpected command/CLI split files"
  exit 1
fi

if grep -nE 'FILE_OR_ALIAS|--execute-op|--execute-fixture|--execute-partial|--execute-segment|--attach-kv|--expect-sha256|--native-source|standalone F32|standalone RoPE|DeepSeek|GGUF descriptor' yvex_cli.c; then
  echo "long command catalog text must not live in yvex_cli.c"
  exit 1
fi

if grep -nE 'open_artifact_for_gguf|open_model_context|open_tokenizer_context|close_model_context|close_tokenizer_context|print_quoted_bytes|print_tensor_dims|print_native_dims|parse_id_list|parse_positive_ull|parse_ull_allow_zero|parse_uint_allow_zero|parse_dims_csv' yvex_cli.c; then
  echo "shared command helpers must not live in yvex_cli.c"
  exit 1
fi

if grep -E 'cli_rope_reference|cli_attention_reference|cli_matmul_reference|cli_mlp_reference|command_graph_execute_rope_op|command_graph_execute_attention_op|command_graph_execute_matmul_op|command_graph_execute_mlp_op' yvex_cli.c >/dev/null; then
  echo "graph reference/proof implementations must not live in yvex_cli.c"
  exit 1
fi

cli_lines="$(wc -l < yvex_cli.c)"
if [ "$cli_lines" -gt 260 ]; then
  echo "yvex_cli.c is too large: $cli_lines lines"
  exit 1
fi

root_c_count="$(find . -maxdepth 1 -type f -name 'yvex*.c' | wc -l | tr -d ' ')"

if [ "$root_c_count" -gt 27 ]; then
  echo "too many root C files: $root_c_count"
  exit 1
fi

echo "source layout: ok"
