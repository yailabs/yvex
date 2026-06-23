#!/bin/sh
#
# YVEX - Source manifest CLI smoke test
#
# File: tests/test_cli_source_manifest.sh
# Layer: test
#
# Purpose:
#   Proves the open-weight intake source-manifest create command over a tiny fake local
#   source tree. The script does not download or commit model files.

set -eu

YVEX_BIN=${YVEX_BIN:-build/bin/yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/source-manifest-cli}
MODEL_DIR="$OUT_DIR/model"
MANIFEST="$OUT_DIR/manifest.json"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

rm -rf "$OUT_DIR"
mkdir -p "$MODEL_DIR"
printf '{}\n' > "$MODEL_DIR/config.json"
printf '{}\n' > "$MODEL_DIR/tokenizer.json"
printf 'x' > "$MODEL_DIR/model-00001.safetensors"

"$YVEX_BIN" source-manifest create \
  --hf-repo test-org/test-model \
  --revision test-rev \
  --license test-license \
  --model-card https://example.invalid/test-model \
  --local-path "$MODEL_DIR" \
  --node test-node \
  --status in-progress \
  --out "$MANIFEST" > "$OUT_DIR/create.out" 2> "$OUT_DIR/create.err" || fail "source-manifest create failed"

test -f "$MANIFEST" || fail "manifest not written"
grep '"schema": "yvex.source_manifest.v1"' "$MANIFEST" >/dev/null || fail "missing schema"
grep '"repo": "test-org/test-model"' "$MANIFEST" >/dev/null || fail "missing repo"
grep '"status": "in-progress"' "$MANIFEST" >/dev/null || fail "missing status"
grep 'model-00001.safetensors' "$MANIFEST" >/dev/null || fail "missing safetensors file"
grep 'status: source-manifest-written' "$OUT_DIR/create.out" >/dev/null || fail "missing CLI status"
