#!/bin/sh
#
# YVEX - controlled GGUF emitter CLI smoke test

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/gguf-emit}
OUT="$OUT_DIR/yvex-owned.gguf"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    python3 tests/support/human_field.py "$1" "$2" || fail "$1 missing: $2"
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" compile artifact emit \
    --out "$OUT" \
    --model-name yvex-owned-gguf-test \
    --arch llama \
    --overwrite > "$OUT_DIR/emit.out" 2> "$OUT_DIR/emit.err" || fail "emit failed"

test -f "$OUT" || fail "emitted file missing"
grep -E 'gguf emit: controlled|ARTIFACT EMIT  controlled' "$OUT_DIR/emit.out" >/dev/null || fail "missing emit heading"
contains "$OUT_DIR/emit.out" 'status: gguf-written'
contains "$OUT_DIR/emit.out" 'roundtrip_validated: yes'

"$YVEX_BIN" artifact show "$OUT" > "$OUT_DIR/inspect.out" 2> "$OUT_DIR/inspect.err" || fail "inspect failed"
contains "$OUT_DIR/inspect.out" 'format: gguf'
contains "$OUT_DIR/inspect.out" 'version: 3'
contains "$OUT_DIR/inspect.out" 'architecture: llama'
contains "$OUT_DIR/inspect.out" 'model_name: yvex-owned-gguf-test'
contains "$OUT_DIR/inspect.out" 'tensor_count: 1'
contains "$OUT_DIR/inspect.out" 'known_tensor_bytes: 128'
contains "$OUT_DIR/inspect.out" 'status: descriptor-only'

"$YVEX_BIN" inspect artifact metadata "$OUT" > "$OUT_DIR/metadata.out" 2> "$OUT_DIR/metadata.err" || fail "metadata failed"
contains "$OUT_DIR/metadata.out" 'metadata_count: 12'

"$YVEX_BIN" inspect artifact tensors "$OUT" > "$OUT_DIR/tensors.out" 2> "$OUT_DIR/tensors.err" || fail "tensors failed"
grep 'token_embd.weight' "$OUT_DIR/tensors.out" >/dev/null || fail "missing tensor name"
if ! grep 'dims=\[4,8\]' "$OUT_DIR/tensors.out" >/dev/null; then
    contains "$OUT_DIR/tensors.out" 'dims: [4, 8]'
fi
if ! grep 'dtype=F32' "$OUT_DIR/tensors.out" >/dev/null; then
    contains "$OUT_DIR/tensors.out" 'dtype: F32'
fi

"$YVEX_BIN" artifact materialize --model "$OUT" --backend cpu > "$OUT_DIR/materialize-cpu.out" 2> "$OUT_DIR/materialize-cpu.err" || fail "cpu materialize failed"
python3 tests/support/human_field.py --regex "$OUT_DIR/materialize-cpu.out" \
    'materialization[ _]status: materialized' || fail "missing materialized status"
python3 tests/support/human_field.py "$OUT_DIR/materialize-cpu.out" 'tensors_materialized: 1' || fail "missing materialized tensor count"
python3 tests/support/human_field.py "$OUT_DIR/materialize-cpu.out" 'bytes_materialized: 128' || fail "missing materialized bytes"
python3 tests/support/human_field.py "$OUT_DIR/materialize-cpu.out" 'execution_ready: false' || fail "missing execution false"
python3 tests/support/human_field.py "$OUT_DIR/materialize-cpu.out" 'status: weights-materialized' || fail "missing weights status"

"$YVEX_BIN" compile artifact emit --help > "$OUT_DIR/help.out" 2> "$OUT_DIR/help.err" || fail "help failed"
grep -E '^(usage: )?yvex compile artifact emit' "$OUT_DIR/help.out" >/dev/null || fail "missing help usage"
