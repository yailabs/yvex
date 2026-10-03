#!/bin/sh
#
# YVEX - Physical-variant CLI admission smoke test
#

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/physical-variant-cli}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

expect_rc() {
    expected=$1
    shift
    set +e
    "$@"
    actual=$?
    set -e
    test "$actual" -eq "$expected" || fail "expected rc $expected, got $actual: $*"
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR"

"$YVEX_BIN" compile quant preset list > "$OUT_DIR/list.out" 2> "$OUT_DIR/list.err" ||
    fail "preset list failed"
grep '^source-faithful$' "$OUT_DIR/list.out" >/dev/null || fail "source-faithful missing"
grep '^deepseek-v4-flash-dspark-q8_0-q2_k-v1$' "$OUT_DIR/list.out" >/dev/null ||
    fail "release preset missing"
grep '^deepseek-v4-flash-dspark-bootstrap-q2-v1$' "$OUT_DIR/list.out" >/dev/null ||
    fail "DS4-like preset missing"

"$YVEX_BIN" compile quant preset show deepseek-v4-flash-dspark-bootstrap-q2-v1 \
    > "$OUT_DIR/show.out" 2> "$OUT_DIR/show.err" || fail "preset show failed"
python3 tests/support/human_field.py "$OUT_DIR/show.out" 'schema_version: 2' ||
    fail "schema v2 missing"
if ! grep '^imatrix_rules: 3$' "$OUT_DIR/show.out" >/dev/null; then
    python3 tests/support/human_field.py "$OUT_DIR/show.out" 'requires_imatrix: 3' ||
        fail "imatrix rules missing"
fi

"$YVEX_BIN" compile quant --help > "$OUT_DIR/help.out" 2> "$OUT_DIR/help.err" ||
    fail "quant help failed"
python3 tests/support/human_field.py "$OUT_DIR/help.out" 'yvex compile quant plan' ||
    fail "plan grammar missing"
python3 tests/support/human_field.py "$OUT_DIR/help.out" 'yvex compile quant probe' ||
    fail "probe grammar missing"
grep 'Plan an identity-bound physical variant' "$OUT_DIR/help.out" >/dev/null ||
    fail "plan summary missing"

expect_rc 1 "$YVEX_BIN" compile quant preset show no-such-preset \
    > "$OUT_DIR/unknown.out" 2> "$OUT_DIR/unknown.err"
expect_rc 2 "$YVEX_BIN" compile quant plan --target deepseek4-v4-flash-dspark \
    > "$OUT_DIR/incomplete.out" 2> "$OUT_DIR/incomplete.err"
expect_rc 1 "$YVEX_BIN" compile quant plan --target minimax-h3-fl2va \
    --source /does/not/exist --component audio_vae --out-plan "$OUT_DIR/minimax.plan" \
    > "$OUT_DIR/minimax-missing.out" 2> "$OUT_DIR/minimax-missing.err"
python3 tests/support/human_field.py "$OUT_DIR/minimax-missing.err" \
    'immutable source acquisition admission failed' ||
    fail "MiniMax source refusal missing"
expect_rc 1 "$YVEX_BIN" compile quant plan --target minimax-h3-fl2va \
    --source /does/not/exist --component transformer \
    --preset minimax-h3-transformer-q8_0-v1 --backend cuda \
    --out-plan "$OUT_DIR/minimax-q8.plan" \
    > "$OUT_DIR/minimax-q8-missing.out" 2> "$OUT_DIR/minimax-q8-missing.err"
python3 tests/support/human_field.py "$OUT_DIR/minimax-q8-missing.err" \
    'immutable source acquisition admission failed' || fail "MiniMax Q8 source refusal missing"
expect_rc 1 "$YVEX_BIN" compile quant plan --target minimax-h3-fl2va \
    --source /does/not/exist --component audio_vae \
    --preset minimax-h3-transformer-q8_0-v1 --out-plan "$OUT_DIR/minimax-q8.plan" \
    > "$OUT_DIR/minimax-q8-component.out" 2> "$OUT_DIR/minimax-q8-component.err"
python3 tests/support/human_field.py "$OUT_DIR/minimax-q8-component.err" \
    'component alternate profile targets another component' || fail "MiniMax Q8 component refusal missing"
expect_rc 1 "$YVEX_BIN" compile quant plan --target minimax-h3-fl2va \
    --source /does/not/exist --component pipeline --out-plan "$OUT_DIR/minimax.plan" \
    > "$OUT_DIR/minimax-component.out" 2> "$OUT_DIR/minimax-component.err"
python3 tests/support/human_field.py "$OUT_DIR/minimax-component.err" \
    'component must be text_encoder, transformer, video_vae, or audio_vae' ||
    fail "MiniMax component refusal missing"
expect_rc 1 "$YVEX_BIN" compile quant plan --target unknown-physical-target \
    --source /does/not/exist --component transformer --out-plan "$OUT_DIR/unknown.plan" \
    > "$OUT_DIR/unknown-target.out" 2> "$OUT_DIR/unknown-target.err"
python3 tests/support/human_field.py "$OUT_DIR/unknown-target.err" \
    'target has no physical-variant compiler adapter' || fail "unknown target adapter refusal missing"
expect_rc 2 "$YVEX_BIN" compile quant nope > "$OUT_DIR/bad-action.out" 2> "$OUT_DIR/bad-action.err"

printf 'physical variant cli: ok\n'
