#!/bin/sh
#
# YVEX - CLI metrics smoke tests
#
# File: tests/cli/metrics.sh
# Layer: test
#
# Purpose:
#   Proves observability layer run/chat metrics, trace, and profile artifacts are written for
#   implemented accepted-token runtime paths only.

set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-metrics}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf

mkdir -p "$OUT_DIR"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    file=$1
    text=$2
    grep -F "$text" "$file" >/dev/null || fail "$file missing: $text"
}

"$YVEX_BIN" run --model "$FIXTURE" --backend cpu --prompt "hello world" \
    --metrics-out "$OUT_DIR/run-metrics.json" \
    --trace-out "$OUT_DIR/run-trace.jsonl" \
    --profile-out "$OUT_DIR/run-profile.json" >"$OUT_DIR/run.out" 2>"$OUT_DIR/run.err"

test -f "$OUT_DIR/run-metrics.json" || fail "run metrics missing"
test -f "$OUT_DIR/run-trace.jsonl" || fail "run trace missing"
test -f "$OUT_DIR/run-profile.json" || fail "run profile missing"
contains "$OUT_DIR/run.out" "metrics_out: $OUT_DIR/run-metrics.json"
contains "$OUT_DIR/run-metrics.json" "\"schema\": \"yvex.metrics.v1\""
contains "$OUT_DIR/run-metrics.json" "\"accepted_tokens\": 3"
contains "$OUT_DIR/run-trace.jsonl" "\"schema\": \"yvex.trace.v1\""
contains "$OUT_DIR/run-profile.json" "\"schema\": \"yvex.profile.v1\""

printf "hello world\n/quit\n" | "$YVEX_BIN" chat --model "$FIXTURE" --backend cpu \
    --metrics-out "$OUT_DIR/chat-metrics.json" \
    --trace-out "$OUT_DIR/chat-trace.jsonl" \
    --profile-out "$OUT_DIR/chat-profile.json" >"$OUT_DIR/chat.out" 2>"$OUT_DIR/chat.err"

test -f "$OUT_DIR/chat-metrics.json" || fail "chat metrics missing"
test -f "$OUT_DIR/chat-trace.jsonl" || fail "chat trace missing"
test -f "$OUT_DIR/chat-profile.json" || fail "chat profile missing"
contains "$OUT_DIR/chat-metrics.json" "\"chat_turns\": 1"
contains "$OUT_DIR/chat-metrics.json" "\"accepted_tokens\": 3"
contains "$OUT_DIR/chat-trace.jsonl" "\"event\": \"chat_turn\""

if grep -E "d[e]code_tps|t[t]ft|g[e]nerated_tokens|tokens_per_s[e]cond" "$OUT_DIR/run-metrics.json" "$OUT_DIR/chat-metrics.json" >/dev/null; then
    fail "fake benchmark field found"
fi

printf 'cli metrics smoke: ok\n'
