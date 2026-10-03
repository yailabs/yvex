#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-build/no-nvcc/yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cuda-failclosed}

mkdir -p "$OUT_DIR"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

contains() {
    grep -F "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

human_matches() {
    python3 tests/support/human_field.py --regex "$1" "$2" || fail "$1 missing pattern: $2"
}

human_contains() {
    python3 tests/support/human_field.py "$1" "$2" || fail "$1 missing: $2"
}

make -pn YVEX_CUDA_ARCH=auto CUDA_AUTO_ARCH=sm_121 >"$OUT_DIR/make-auto.out"
contains "$OUT_DIR/make-auto.out" "CUDA_EFFECTIVE_ARCH := sm_121"
make -pn YVEX_CUDA_ARCH=sm_90 CUDA_AUTO_ARCH=sm_121 >"$OUT_DIR/make-explicit.out"
contains "$OUT_DIR/make-explicit.out" "CUDA_EFFECTIVE_ARCH := sm_90"

if grep -RIn -E 'Fallback embedded PTX|\.visible[[:space:]]+\.entry' \
    src/backend/cuda --include='*.c' >/dev/null; then
    fail "production C source contains embedded CUDA entry points"
fi

for contract in \
    'max_host_bytes' \
    'peak_host_bytes' \
    'cuda.attention.validate.geometry' \
    'cuda.attention.validate.alias' \
    'cuda.attention.validate.host_budget' \
    'cuda.attention.context'; do
    grep -R -F "$contract" include/yvex/backend.h \
        src/backend/cuda/attention.c >/dev/null ||
        fail "encoded-attention admission missing: $contract"
done
grep -F 'atomicCAS(status, 0, 2)' src/backend/cuda/kernels.cu >/dev/null ||
    fail "encoded-attention kernels do not publish contract failures"
if grep -E 'yvex_(attention|graph)|cpu_(chunk|probe|reference)' \
    src/backend/cuda/attention.c >/dev/null; then
    fail "encoded-attention CUDA owner contains a CPU numerical fallback"
fi

set +e
"$YVEX_BIN" inspect backend cuda >"$OUT_DIR/backend.out" 2>"$OUT_DIR/backend.err"
rc=$?
set -e

if [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/backend.out" "BACKEND  cuda · unavailable"
    human_matches "$OUT_DIR/backend.out" '(?m)^\s*reason\s+\S+'
    echo "cuda no-nvcc fail-closed: driver unavailable"
    exit 0
fi
[ "$rc" -eq 0 ] || fail "backend cuda returned $rc"

contains "$OUT_DIR/backend.out" "BACKEND  cuda · context-ready"
human_contains "$OUT_DIR/backend.out" "tensor_alloc: supported"
human_contains "$OUT_DIR/backend.out" "tensor_read_write: supported"
for primitive in op_embed op_rms_norm op_rope op_attention op_matmul op_mlp; do
    human_contains "$OUT_DIR/backend.out" "$primitive: unsupported"
done
contains "$OUT_DIR/backend.out" "CUDA  context available · bundle absent · unavailable · no executable image"
contains "$OUT_DIR/backend.out" "BUNDLE  unavailable · kernel-bundle-absent"
for variant in embed-f32-to-f32 attention-noncausal-f32 qtype-row-dot encoded-attention; do
    human_matches "$OUT_DIR/backend.out" "(?m)^$variant\s+unsupported\s+kernel-bundle-absent$"
done

set +e
"$YVEX_BIN" bench attention execute --target deepseek4-v4-flash-dspark --backend cuda \
    --runtime-binding "$OUT_DIR/missing.yvex-runtime-binding" \
    --artifact "$OUT_DIR/missing.gguf" --output json \
    >"$OUT_DIR/graph.out" 2>"$OUT_DIR/graph.err"
rc=$?
set -e
[ "$rc" -ne 0 ] || fail "bundle-less CUDA attention unexpectedly succeeded"
python3 - "$OUT_DIR/graph.out" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    result = json.load(stream)
assert result["status"] == "refused"
assert result["backend"] == "cuda"
assert result["persistent_kv_ready"] is False
assert result["transformer_ready"] is False
assert result["runtime_generation_ready"] is False
assert result["failure_where"] == "runtime.model"
assert "runtime binding" in result["reason"]
PY
contains "$OUT_DIR/graph.err" "runtime binding"

echo "cuda no-nvcc fail-closed: ok"
