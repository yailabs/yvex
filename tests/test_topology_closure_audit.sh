#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

sh tests/test_source_ownership.sh
sh tests/test_repository_layout.sh
sh tests/test_architecture_boundaries.sh
sh tests/test_surface.sh

test -f src/graph/attention.c
test -f src/graph/numeric.c
test -f src/graph/families/deepseek_v4.c
test -f src/backend/cuda/families/deepseek_v4.c
test -f tests/reference/deepseek_attention.c

if grep -RIn 'yvex_test_attention_reference_' src include; then
  echo "topology closure: production attention calls the test reference" >&2
  exit 1
fi

if find src -type f -path '*/families/*' | grep -E '/deepseek[^/]*_(plan|execute|sink|numeric|internal|reference|report)\.(c|h|cu)$'; then
  echo "topology closure: family phase fragmentation remains" >&2
  exit 1
fi

family_files=$(find src -type f -path '*/families/deepseek_v4.*' | wc -l | tr -d ' ')
test "$family_files" -eq 3 || {
  echo "topology closure: DeepSeek family budget is $family_files/3" >&2
  exit 1
}

for fact in \
  'attention_execution_supported=0' \
  'attention_cuda_execution_ready=0' \
  'runtime_generation_ready=0'; do
  grep -F "$fact" tests/live/attention_deepseek.c >/dev/null || {
    echo "topology closure: missing preserved refusal fact: $fact" >&2
    exit 1
  }
done

echo "topology closure: ok hard=1 advisory=0 family_files=$family_files"
