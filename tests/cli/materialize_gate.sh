#!/bin/sh
set -eu

YVEX_BIN="${YVEX_BIN:-./yvex}"
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/materialize-gate-cli}
mkdir -p "$OUT_DIR"

fail() {
  echo "cli materialize-gate: $*" >&2
  exit 1
}

"$YVEX_BIN" gguf-emit controlled \
  --out "$OUT_DIR/selected.gguf" \
  --model-name materialize-gate-test \
  --arch llama \
  --overwrite > "$OUT_DIR/emit.out"

"$YVEX_BIN" materialize-gate check \
  --model "$OUT_DIR/selected.gguf" \
  --label fixture-selected \
  --family llama \
  --scope selected-tensor \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4,8 \
  --expect-dtype F32 \
  --expect-bytes 128 \
  --backend cpu \
  --require-cpu \
  --repeat 2 \
  --check-cleanup \
  --report-out "$OUT_DIR/report.txt" > "$OUT_DIR/check.out"

grep 'status: materialize-gate-pass' "$OUT_DIR/report.txt" >/dev/null || fail "missing pass"
grep 'scope: selected-tensor' "$OUT_DIR/report.txt" >/dev/null || fail "missing scope"
grep 'execution_ready: false' "$OUT_DIR/report.txt" >/dev/null || fail "execution ready changed"

if "$YVEX_BIN" materialize-gate check \
  --model "$OUT_DIR/missing.gguf" \
  --label missing \
  --family llama \
  --scope selected-tensor \
  --backend cpu \
  --require-cpu \
  --report-out "$OUT_DIR/missing-report.txt" > "$OUT_DIR/missing.out"; then
  fail "missing file unexpectedly passed"
fi
grep 'failure_class: missing_file' "$OUT_DIR/missing-report.txt" >/dev/null || fail "missing failure class"

if "$YVEX_BIN" materialize-gate check \
  --model "$OUT_DIR/selected.gguf" \
  --label bad-shape \
  --family llama \
  --scope selected-tensor \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 8,4 \
  --expect-dtype F32 \
  --expect-bytes 128 \
  --backend cpu \
  --require-cpu \
  --report-out "$OUT_DIR/bad-report.txt" > "$OUT_DIR/bad.out"; then
  fail "bad shape unexpectedly passed"
fi
grep 'failure_class: tensor_spec_mismatch' "$OUT_DIR/bad-report.txt" >/dev/null || fail "missing mismatch class"

"$YVEX_BIN" help materialize-gate > "$OUT_DIR/help.out"
grep 'usage: yvex materialize-gate check' "$OUT_DIR/help.out" >/dev/null || fail "missing help"
