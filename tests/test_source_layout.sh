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

if find . -maxdepth 1 \( -name 'yvex_cli_*.c' -o -name 'yvex_cli_*.h' \) -print | grep .; then
  echo "private CLI-prefixed source files are not allowed"
  exit 1
fi

if grep -E 'cli_rope_reference|cli_attention_reference|cli_matmul_reference|cli_mlp_reference|command_graph_execute_rope_op|command_graph_execute_attention_op|command_graph_execute_matmul_op|command_graph_execute_mlp_op' yvex_cli.c >/dev/null; then
  echo "graph reference/proof implementations must not live in yvex_cli.c"
  exit 1
fi

root_c_count="$(find . -maxdepth 1 -type f -name 'yvex*.c' | wc -l | tr -d ' ')"

if [ "$root_c_count" -gt 31 ]; then
  echo "too many root C files: $root_c_count"
  exit 1
fi

echo "source layout: ok"
