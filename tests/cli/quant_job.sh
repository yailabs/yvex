#!/bin/sh
#
# YVEX - quant-job CLI smoke test
#
# File: tests/cli/quant_job.sh
# Layer: test

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/quant-job-cli}

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

yvex_test_cleanup "$OUT_DIR"
mkdir -p "$OUT_DIR/native"

printf 'fake-template\n' > "$OUT_DIR/template.gguf"
printf '#!/bin/sh\nexit 0\n' > "$OUT_DIR/tool"
chmod +x "$OUT_DIR/tool"

"$YVEX_BIN" compile quant job create \
  --name test-job \
  --arch deepseek4 \
  --tool external \
  --tool-path "$OUT_DIR/tool" \
  --native-source "$OUT_DIR/native" \
  --template "$OUT_DIR/template.gguf" \
  --out-gguf "$OUT_DIR/out.gguf" \
  --log "$OUT_DIR/job.log" \
  --status ready \
  --command "test command" \
  --out "$OUT_DIR/job.json" > "$OUT_DIR/create.out" 2> "$OUT_DIR/create.err" || fail "create failed"

test -f "$OUT_DIR/job.json" || fail "manifest missing"
grep -E 'compile quant job: written|QUANT JOB  create' "$OUT_DIR/create.out" >/dev/null || fail "missing create heading"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'tool_exists: yes' || fail "missing tool exists"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'source_exists: yes' || fail "missing source exists"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'template_exists: yes' || fail "missing template exists"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'output_exists: no' || fail "missing output exists"
python3 tests/support/human_field.py "$OUT_DIR/create.out" 'status: quant-job-written' || fail "missing create status"

"$YVEX_BIN" compile quant job inspect --manifest "$OUT_DIR/job.json" > "$OUT_DIR/inspect.out" 2> "$OUT_DIR/inspect.err" || fail "inspect failed"
grep -E 'compile quant job: inspect|QUANT JOB  inspect' "$OUT_DIR/inspect.out" >/dev/null || fail "missing inspect heading"
python3 tests/support/human_field.py "$OUT_DIR/inspect.out" 'status: quant-job-manifest' || fail "missing inspect status"

"$YVEX_BIN" compile quant job validate --manifest "$OUT_DIR/job.json" > "$OUT_DIR/validate.out" 2> "$OUT_DIR/validate.err" || fail "validate failed"
grep -E 'compile quant job: validate|QUANT JOB  validate' "$OUT_DIR/validate.out" >/dev/null || fail "missing validate heading"
python3 tests/support/human_field.py "$OUT_DIR/validate.out" 'status: quant-job-valid' || fail "missing validate status"

"$YVEX_BIN" compile quant job --help > "$OUT_DIR/help.out" 2> "$OUT_DIR/help.err" || fail "help failed"
grep 'yvex compile quant job' "$OUT_DIR/help.out" >/dev/null || fail "missing help"
