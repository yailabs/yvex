#!/bin/sh
#
# YVEX - Imatrix CLI smoke test
#
# File: tests/cli/imatrix.sh
# Layer: test

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/imatrix-cli}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR"

printf 'fake-imatrix' > "$OUT_DIR/fake.dat"

"$YVEX_BIN" compile quant imatrix create \
  --name test-imatrix \
  --arch deepseek4 \
  --imatrix "$OUT_DIR/fake.dat" \
  --format routed_moe_dat \
  --status present \
  --dataset test-dataset \
  --producer test \
  --out "$OUT_DIR/imatrix.json" > "$OUT_DIR/create.out" 2> "$OUT_DIR/create.err" || fail "create failed"

test -f "$OUT_DIR/imatrix.json" || fail "manifest missing"
grep -E 'imatrix manifest: written|IMATRIX  create' "$OUT_DIR/create.out" >/dev/null || fail "missing create heading"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'file_exists: yes' || fail "missing file_exists"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'status: imatrix-manifest-written' || fail "missing create status"

"$YVEX_BIN" compile quant imatrix inspect --manifest "$OUT_DIR/imatrix.json" > "$OUT_DIR/inspect.out" 2> "$OUT_DIR/inspect.err" || fail "inspect failed"
grep -E 'imatrix: inspect|IMATRIX  inspect' "$OUT_DIR/inspect.out" >/dev/null || fail "missing inspect heading"
python3 tests/support/human_field.py "$OUT_DIR/inspect.out" 'status: imatrix-manifest' || fail "missing inspect status"

"$YVEX_BIN" compile quant imatrix validate --manifest "$OUT_DIR/imatrix.json" > "$OUT_DIR/validate.out" 2> "$OUT_DIR/validate.err" || fail "validate failed"
grep -E 'imatrix: validate|IMATRIX  validate' "$OUT_DIR/validate.out" >/dev/null || fail "missing validate heading"
python3 tests/support/human_field.py "$OUT_DIR/validate.out" 'requires_imatrix_rules: 0' || fail "missing requires count"
python3 tests/support/human_field.py "$OUT_DIR/validate.out" 'covered_rules: 0' || fail "missing covered count"
python3 tests/support/human_field.py "$OUT_DIR/validate.out" 'status: imatrix-valid' || fail "missing validate status"

"$YVEX_BIN" compile quant imatrix --help > "$OUT_DIR/help.out" 2> "$OUT_DIR/help.err" || fail "help failed"
grep 'yvex compile quant imatrix' "$OUT_DIR/help.out" >/dev/null || fail "missing help"
