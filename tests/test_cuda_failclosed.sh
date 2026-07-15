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

if grep -RIn -E 'Fallback embedded PTX|\.visible[[:space:]]+\.entry' \
    src/backend/cuda --include='*.c' >/dev/null; then
    fail "production C source contains embedded CUDA entry points"
fi

set +e
"$YVEX_BIN" backend cuda >"$OUT_DIR/backend.out" 2>"$OUT_DIR/backend.err"
rc=$?
set -e

if [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/backend.out" "backend: cuda"
    contains "$OUT_DIR/backend.out" "status: unsupported"
    contains "$OUT_DIR/backend.out" "status: backend-unsupported"
    echo "cuda no-nvcc fail-closed: driver unavailable"
    exit 0
fi
[ "$rc" -eq 0 ] || fail "backend cuda returned $rc"

contains "$OUT_DIR/backend.out" "status: context-ready"
contains "$OUT_DIR/backend.out" "tensor_alloc: yes"
contains "$OUT_DIR/backend.out" "tensor_read_write: yes"
contains "$OUT_DIR/backend.out" "op_embed: no"
contains "$OUT_DIR/backend.out" "op_rms_norm: no"
contains "$OUT_DIR/backend.out" "op_rope: no"
contains "$OUT_DIR/backend.out" "op_attention: no"
contains "$OUT_DIR/backend.out" "op_matmul: no"
contains "$OUT_DIR/backend.out" "op_mlp: no"
contains "$OUT_DIR/backend.out" "context_available: yes"
contains "$OUT_DIR/backend.out" "kernel_bundle: absent"
contains "$OUT_DIR/backend.out" "kernel_bundle_reason: kernel-bundle-absent"
contains "$OUT_DIR/backend.out" "embed-f32-to-f32: unsupported (kernel-bundle-absent)"
contains "$OUT_DIR/backend.out" "attention-noncausal-f32: unsupported (kernel-bundle-absent)"
contains "$OUT_DIR/backend.out" "qtype-row-dot: unsupported (kernel-bundle-absent)"
contains "$OUT_DIR/backend.out" "status: backend-capabilities"

set +e
"$YVEX_BIN" graph --backend cuda --execute-op --op rope \
    --position 7 --head-dim 8 >"$OUT_DIR/graph.out" 2>"$OUT_DIR/graph.err"
rc=$?
set -e
[ "$rc" -eq 5 ] || fail "bundle-less CUDA graph returned $rc"
contains "$OUT_DIR/graph.out" "backend_status: context-ready"
contains "$OUT_DIR/graph.out" "backend_op_status: unsupported"
contains "$OUT_DIR/graph.out" "dispatch_attempted: false"
contains "$OUT_DIR/graph.out" "reason: backend-op-rope-unsupported"
contains "$OUT_DIR/graph.out" "status: graph-op-fail"

echo "cuda no-nvcc fail-closed: ok"
