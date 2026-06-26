#!/bin/sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-artifact-integrity}
MODEL="$OUT_DIR/integrity-controlled-F16.gguf"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

not_contains() {
    file=$1
    text=$2
    if grep -F "$text" "$file" >/dev/null; then
        fail "$file unexpectedly contained: $text"
    fi
}

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" gguf-emit controlled \
  --out "$MODEL" \
  --model-name integrity-controlled-f16 \
  --arch deepseek \
  --target-qtype F16 \
  --overwrite >"$OUT_DIR/emit.out" 2>"$OUT_DIR/emit.err"

"$YVEX_BIN" integrity check --model "$MODEL" --require-token-embedding --partial-token 0 \
  >"$OUT_DIR/integrity-pass.out" 2>"$OUT_DIR/integrity-pass.err"
contains "$OUT_DIR/integrity-pass.out" "artifact_integrity: check"
contains "$OUT_DIR/integrity-pass.out" "format: gguf"
contains "$OUT_DIR/integrity-pass.out" "version: 3"
contains "$OUT_DIR/integrity-pass.out" "architecture: deepseek"
contains "$OUT_DIR/integrity-pass.out" "tensor_count: 1"
contains "$OUT_DIR/integrity-pass.out" "known_tensor_bytes: 64"
contains "$OUT_DIR/integrity-pass.out" "integrity_status: pass"
contains "$OUT_DIR/integrity-pass.out" "status: artifact-integrity-pass"

"$YVEX_BIN" help integrity >"$OUT_DIR/help.out" 2>"$OUT_DIR/help.err"
contains "$OUT_DIR/help.out" "usage: yvex integrity check --model FILE_OR_ALIAS"

"$YVEX_BIN" integrity check --model tests/fixtures/gguf/bad-magic.gguf \
  >"$OUT_DIR/bad-magic.out" 2>"$OUT_DIR/bad-magic.err" && fail "bad magic passed" || true
contains "$OUT_DIR/bad-magic.out" "integrity_status: fail"
contains "$OUT_DIR/bad-magic.out" "error_0_code: bad-magic"
contains "$OUT_DIR/bad-magic.out" "status: artifact-integrity-fail"

"$YVEX_BIN" integrity check --model tests/fixtures/gguf/short-header.gguf \
  >"$OUT_DIR/truncated.out" 2>"$OUT_DIR/truncated.err" && fail "truncated file passed" || true
contains "$OUT_DIR/truncated.out" "integrity_status: fail"
contains "$OUT_DIR/truncated.out" "error_0_code: file-too-small"

"$YVEX_BIN" integrity check --model tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf \
  >"$OUT_DIR/range.out" 2>"$OUT_DIR/range.err" && fail "range-bad file passed" || true
contains "$OUT_DIR/range.out" "integrity_status: fail"
contains "$OUT_DIR/range.out" "error_0_code: tensor-range-out-of-file"

"$YVEX_BIN" integrity check --model tests/fixtures/gguf/tensor-dim-zero.gguf \
  >"$OUT_DIR/zero-dim.out" 2>"$OUT_DIR/zero-dim.err" && fail "zero-dim file passed" || true
contains "$OUT_DIR/zero-dim.out" "integrity_status: fail"
contains "$OUT_DIR/zero-dim.out" "error_0_code: zero-dimension"

"$YVEX_BIN" integrity check --model tests/fixtures/gguf/valid-minimal.gguf --require-token-embedding \
  >"$OUT_DIR/missing-required.out" 2>"$OUT_DIR/missing-required.err" && fail "missing required passed" || true
contains "$OUT_DIR/missing-required.out" "integrity_status: fail"
contains "$OUT_DIR/missing-required.out" "error_0_code: required-tensor-missing"

"$YVEX_BIN" integrity check --model "$MODEL" --partial-token 99 \
  >"$OUT_DIR/token-range.out" 2>"$OUT_DIR/token-range.err" && fail "token out of range passed" || true
contains "$OUT_DIR/token-range.out" "integrity_status: fail"
contains "$OUT_DIR/token-range.out" "error_0_code: token-out-of-range"

"$YVEX_BIN" materialize --model tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf --backend cpu \
  >"$OUT_DIR/materialize-range.out" 2>"$OUT_DIR/materialize-range.err" && fail "materialize corrupt range passed" || true
contains "$OUT_DIR/materialize-range.err" "absolute offset"
not_contains "$OUT_DIR/materialize-range.out" "status: weights-materialized"

"$YVEX_BIN" graph --model tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf --backend cpu --execute-partial --partial-token 0 \
  >"$OUT_DIR/graph-range.out" 2>"$OUT_DIR/graph-range.err" && fail "graph corrupt range passed" || true
contains "$OUT_DIR/graph-range.err" "absolute offset"
not_contains "$OUT_DIR/graph-range.out" "status: real-partial-graph-executed"

echo "cli artifact integrity: ok"
