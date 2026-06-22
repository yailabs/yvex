#!/bin/sh
set -u

YVEX_BIN=${YVEX_BIN:-./build/bin/yvex}
OUT_DIR=${OUT_DIR:-build/tests}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

mkdir -p "$OUT_DIR"

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

"$YVEX_BIN" cuda-info >"$OUT_DIR/cuda_info.out" 2>"$OUT_DIR/cuda_info.err"
rc=$?
if [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/cuda_info.out" "cuda: unavailable"
    contains "$OUT_DIR/cuda_info.out" "status: cuda-unavailable"
    echo "cli cuda smoke: skip"
    exit 77
fi
if [ "$rc" -ne 0 ]; then
    fail "cuda-info exit code was $rc"
fi
contains "$OUT_DIR/cuda_info.out" "cuda: available"
contains "$OUT_DIR/cuda_info.out" "status: cuda-info"

"$YVEX_BIN" backend cuda >"$OUT_DIR/backend_cuda_ready.out" 2>"$OUT_DIR/backend_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "backend cuda exit code was $rc"
fi
contains "$OUT_DIR/backend_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/backend_cuda_ready.out" "status: ready"
contains "$OUT_DIR/backend_cuda_ready.out" "tensor_alloc: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "tensor_read_write: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_embed: yes"
contains "$OUT_DIR/backend_cuda_ready.out" "op_matmul: no"
contains "$OUT_DIR/backend_cuda_ready.out" "status: backend-ready"

"$YVEX_BIN" plan "$FIXTURE" --backend cuda >"$OUT_DIR/plan_cuda_ready.out" 2>"$OUT_DIR/plan_cuda_ready.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "plan cuda exit code was $rc"
fi
contains "$OUT_DIR/plan_cuda_ready.out" "backend: cuda"
contains "$OUT_DIR/plan_cuda_ready.out" "backend_status: available"
contains "$OUT_DIR/plan_cuda_ready.out" "op_embed: yes"
contains "$OUT_DIR/plan_cuda_ready.out" "execution_ready: false"
contains "$OUT_DIR/plan_cuda_ready.out" "status: plan-only"

"$YVEX_BIN" help cuda-info >"$OUT_DIR/help_cuda_info.out" 2>"$OUT_DIR/help_cuda_info.err"
rc=$?
if [ "$rc" -ne 0 ]; then
    fail "help cuda-info exit code was $rc"
fi
contains "$OUT_DIR/help_cuda_info.out" "usage: yvex cuda-info"

echo "cli cuda smoke: ok"
