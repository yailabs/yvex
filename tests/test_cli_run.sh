#!/bin/sh
#
# YVEX - CLI run smoke tests
#
# File: tests/test_cli_run.sh
# Layer: test
#
# Purpose:
#   Proves that yvex run is an diagnostic runtime accepted-only runtime shell: it opens the
#   engine/backend/session path, accepts prompt tokens, and reports generation
#   unsupported without emitting fake model output.
#
# Covers:
#   - yvex run --output plain
#   - yvex run --output json
#   - yvex run backend unsupported path
#   - yvex help run
#
# Commands:
#   - make test-cli
#   - tests/test_cli_run.sh
#
# Expected:
#   - exits 0 on success
#   - prints concise failure to stderr

set -eu

YVEX_BIN=${YVEX_BIN:-build/bin/yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-run}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

run_ok() {
    name=$1
    shift
    "$@" >"$OUT_DIR/$name.out" 2>"$OUT_DIR/$name.err" || fail "$name exited non-zero"
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

run_ok run_plain "$YVEX_BIN" run --model "$FIXTURE" --backend cpu --prompt "hello world"
contains "$OUT_DIR/run_plain.out" "run status: accepted-only"
contains "$OUT_DIR/run_plain.out" "model: yvex-tokenizer-test"
contains "$OUT_DIR/run_plain.out" "backend: cpu"
contains "$OUT_DIR/run_plain.out" "session_state: partial"
contains "$OUT_DIR/run_plain.out" "prompt_tokens: 3"
contains "$OUT_DIR/run_plain.out" "accepted_tokens: 3"
contains "$OUT_DIR/run_plain.out" "position: 3"
contains "$OUT_DIR/run_plain.out" "execution_ready: false"
contains "$OUT_DIR/run_plain.out" "generation: unsupported"
contains "$OUT_DIR/run_plain.out" "reason: decode runtime is not implemented in diagnostic runtime"

run_ok run_system "$YVEX_BIN" run --model "$FIXTURE" --backend cpu --system "You are helpful" --prompt "hello world"
contains "$OUT_DIR/run_system.out" "run status: accepted-only"
contains "$OUT_DIR/run_system.out" "accepted_tokens: 3"

run_ok run_json "$YVEX_BIN" run --model "$FIXTURE" --backend cpu --prompt "hello world" --output json
contains "$OUT_DIR/run_json.out" "\"schema\": \"yvex.cli.result.v1\""
contains "$OUT_DIR/run_json.out" "\"command\": \"run\""
contains "$OUT_DIR/run_json.out" "\"status\": \"accepted-only\""
contains "$OUT_DIR/run_json.out" "\"prompt_tokens\": 3"
contains "$OUT_DIR/run_json.out" "\"generation\": \"unsupported\""
contains "$OUT_DIR/run_json.out" "\"error\": null"

set +e
"$YVEX_BIN" run --model "$FIXTURE" --backend cuda --prompt "hello world" >"$OUT_DIR/run_cuda.out" 2>"$OUT_DIR/run_cuda.err"
rc=$?
set -e
contains "$OUT_DIR/run_cuda.out" "backend: cuda"
if [ "$rc" -eq 0 ]; then
    contains "$OUT_DIR/run_cuda.out" "run status: accepted-only"
    contains "$OUT_DIR/run_cuda.out" "execution_ready: false"
    contains "$OUT_DIR/run_cuda.out" "generation: unsupported"
elif [ "$rc" -eq 5 ]; then
    contains "$OUT_DIR/run_cuda.out" "run status: backend-unsupported"
    contains "$OUT_DIR/run_cuda.out" "execution_ready: false"
else
    fail "run cuda exit code was $rc"
fi

run_ok help_run "$YVEX_BIN" help run
contains "$OUT_DIR/help_run.out" "usage: yvex run --model FILE"

printf 'cli run smoke: ok\n'
