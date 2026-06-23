#!/bin/sh
#
# YVEX - CLI chat smoke tests
#
# File: tests/test_cli_chat.sh
# Layer: test
#
# Purpose:
#   Proves that yvex chat is an diagnostic runtime REPL shell that accepts user prompt tokens,
#   handles slash commands, and reports generation unsupported.
#
# Covers:
#   - piped chat session
#   - slash command parser through CLI
#   - chat help
#
# Commands:
#   - make test-cli
#   - tests/test_cli_chat.sh
#
# Expected:
#   - exits 0 on success
#   - prints concise failure to stderr

set -eu

YVEX_BIN=${YVEX_BIN:-build/bin/yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-chat}
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

printf "/status\nhello world\n/tokens\n/reset\n/quit\n" | \
    "$YVEX_BIN" chat --model "$FIXTURE" --backend cpu >"$OUT_DIR/chat_pipe.out" 2>"$OUT_DIR/chat_pipe.err"
contains "$OUT_DIR/chat_pipe.out" "YVEX chat runtime"
contains "$OUT_DIR/chat_pipe.out" "session_state: partial"
contains "$OUT_DIR/chat_pipe.out" "accepted tokens: 3"
contains "$OUT_DIR/chat_pipe.out" "position: 3"
contains "$OUT_DIR/chat_pipe.out" "assistant: [generation unsupported in diagnostic runtime]"
contains "$OUT_DIR/chat_pipe.out" "accepted_tokens: 3"
contains "$OUT_DIR/chat_pipe.out" "session reset"
contains "$OUT_DIR/chat_pipe.out" "bye"

printf "/help\n/quit\n" | "$YVEX_BIN" chat --model "$FIXTURE" --backend cpu >"$OUT_DIR/chat_help.out" 2>"$OUT_DIR/chat_help.err"
contains "$OUT_DIR/chat_help.out" "commands:"
contains "$OUT_DIR/chat_help.out" "/status"
contains "$OUT_DIR/chat_help.out" "/quit"

printf "/bad\n/quit\n" | "$YVEX_BIN" chat --model "$FIXTURE" --backend cpu >"$OUT_DIR/chat_bad.out" 2>"$OUT_DIR/chat_bad.err"
contains "$OUT_DIR/chat_bad.out" "unknown slash command: /bad"
contains "$OUT_DIR/chat_bad.out" "type /help"

"$YVEX_BIN" help chat >"$OUT_DIR/help_chat.out" 2>"$OUT_DIR/help_chat.err"
contains "$OUT_DIR/help_chat.out" "usage: yvex chat --model FILE"

printf 'cli chat smoke: ok\n'
