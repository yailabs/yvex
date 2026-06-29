#!/usr/bin/env sh
set -eu

test ! -d src
test ! -d cli
test ! -d server
test ! -d backends

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
  find . -maxdepth 2 \
    \( -name 'yvex_cli_*.c' \
       -o -name 'yvex_cli_*.h' \
       -o -name 'yvex_*_commands.c' \
       -o -name 'commands.c' \
       -o -name 'yvex_command_private.h' \
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
       -o -name 'yvex_owi_private.h' \) \
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
