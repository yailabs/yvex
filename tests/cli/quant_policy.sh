#!/bin/sh
#
# YVEX - Quant policy CLI smoke test
#
# File: tests/cli/quant_policy.sh
# Layer: test

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/quant-policy-cli}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR"

cat > "$OUT_DIR/policy.json" <<'JSON'
{
  "schema": "yvex.quant_policy.v1",
  "name": "test-policy",
  "architecture": "deepseek4",
  "rules": [
    {"selector_kind": "role", "selector": "token_embedding", "qtype": "Q8_0", "requires_imatrix": false},
    {"selector_kind": "pattern", "selector": "blk.*.ffn.experts.*", "qtype": "Q2_K", "requires_imatrix": true}
  ]
}
JSON

"$YVEX_BIN" compile quant policy inspect --policy "$OUT_DIR/policy.json" > "$OUT_DIR/inspect.out" 2> "$OUT_DIR/inspect.err" || fail "inspect failed"
grep -E 'compile quant policy: inspect|QUANT POLICY  inspect' "$OUT_DIR/inspect.out" >/dev/null || fail "missing inspect heading"
grep -E 'selector=role:token_embedding qtype=Q8_0|role:token_embedding +Q8_0' "$OUT_DIR/inspect.out" >/dev/null || fail "missing role rule"
grep -E 'requires_imatrix=yes|Q2_K +yes' "$OUT_DIR/inspect.out" >/dev/null || fail "missing imatrix flag"

"$YVEX_BIN" compile quant policy validate --policy "$OUT_DIR/policy.json" > "$OUT_DIR/validate.out" 2> "$OUT_DIR/validate.err" || fail "validate failed"
grep -E 'compile quant policy: validate|QUANT POLICY  validate' "$OUT_DIR/validate.out" >/dev/null || fail "missing validate heading"
python3 tests/support/human_field.py --regex "$OUT_DIR/validate.out" 'status: quant-policy-.*' || fail "missing validate status"

"$YVEX_BIN" compile quant policy derive \
  --template tests/fixtures/gguf/valid-tokenizer-simple.gguf \
  --arch llama \
  --out "$OUT_DIR/derived.json" > "$OUT_DIR/derive.out" 2> "$OUT_DIR/derive.err" || fail "derive failed"
test -f "$OUT_DIR/derived.json" || fail "derived policy missing"
grep -E 'compile quant policy: derived|QUANT POLICY  derive' "$OUT_DIR/derive.out" >/dev/null || fail "missing derive heading"
python3 tests/support/human_field.py "$OUT_DIR/derive.out" 'status: quant-policy-written' || fail "missing derive status"

"$YVEX_BIN" compile quant policy validate --policy "$OUT_DIR/derived.json" --template tests/fixtures/gguf/valid-tokenizer-simple.gguf > "$OUT_DIR/derived-validate.out" 2> "$OUT_DIR/derived-validate.err" || fail "derived validate failed"
python3 tests/support/human_field.py --regex "$OUT_DIR/derived-validate.out" 'status: quant-policy-.*' || fail "missing derived validate status"

"$YVEX_BIN" compile quant policy --help > "$OUT_DIR/help.out" 2> "$OUT_DIR/help.err" || fail "help failed"
grep 'yvex compile quant policy' "$OUT_DIR/help.out" >/dev/null || fail "missing help"
