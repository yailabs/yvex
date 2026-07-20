#!/bin/sh
#
# YVEX - CLI chat smoke tests
#
# File: tests/cli/chat.sh
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
#   - tests/cli.sh
#
# Expected:
#   - exits 0 on success
#   - prints concise failure to stderr

set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli-chat}
FIXTURE=tests/fixtures/gguf/valid-tokenizer-simple.gguf
SELECTED_MODEL="$OUT_DIR/chat-selected-model-fixture-F32-noimatrix-yvex-v1.gguf"
SELECTED_REGISTRY="$OUT_DIR/models.local.json"
SELECTED_ALIAS="chat-selected-model-fixture"
EMPTY_REGISTRY="$OUT_DIR/empty/models.local.json"

yvex_test_cleanup "$OUT_DIR"
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

mkdir -p "$OUT_DIR/empty"
if YVEX_MODELS_REGISTRY="$EMPTY_REGISTRY" "$YVEX_BIN" chat --backend cpu >"$OUT_DIR/chat_no_model.out" 2>"$OUT_DIR/chat_no_model.err"; then
    fail "chat without model/current selection should fail"
fi
contains "$OUT_DIR/chat_no_model.err" "no model selected"
contains "$OUT_DIR/chat_no_model.err" "pass --model FILE_OR_ALIAS"
contains "$OUT_DIR/chat_no_model.err" "models use ALIAS"

"$YVEX_BIN" gguf-emit controlled \
    --out "$SELECTED_MODEL" \
    --model-name chat-selected-fixture \
    --arch llama \
    --overwrite >"$OUT_DIR/emit_selected.out" 2>"$OUT_DIR/emit_selected.err"

"$YVEX_BIN" models add \
    --path "$SELECTED_MODEL" \
    --alias "$SELECTED_ALIAS" \
    --registry "$SELECTED_REGISTRY" >"$OUT_DIR/models_add.out" 2>"$OUT_DIR/models_add.err"

"$YVEX_BIN" models use "$SELECTED_ALIAS" \
    --registry "$SELECTED_REGISTRY" >"$OUT_DIR/models_use.out" 2>"$OUT_DIR/models_use.err"

printf "/status\n/quit\n" | \
    YVEX_MODELS_REGISTRY="$SELECTED_REGISTRY" "$YVEX_BIN" chat --backend cpu >"$OUT_DIR/chat_current.out" 2>"$OUT_DIR/chat_current.err"
contains "$OUT_DIR/chat_current.out" "YVEX chat runtime"
contains "$OUT_DIR/chat_current.out" "model: chat-selected-fixture"
contains "$OUT_DIR/chat_current.out" "session_state: partial"
contains "$OUT_DIR/chat_current.out" "generation: unsupported"
contains "$OUT_DIR/chat_current.out" "bye"

printf "/quit\n" | \
    YVEX_MODELS_REGISTRY="$SELECTED_REGISTRY" "$YVEX_BIN" chat --model "$SELECTED_ALIAS" --backend cpu >"$OUT_DIR/chat_alias.out" 2>"$OUT_DIR/chat_alias.err"
contains "$OUT_DIR/chat_alias.out" "YVEX chat runtime"
contains "$OUT_DIR/chat_alias.out" "model: chat-selected-fixture"
contains "$OUT_DIR/chat_alias.out" "bye"

"$YVEX_BIN" help chat >"$OUT_DIR/help_chat.out" 2>"$OUT_DIR/help_chat.err"
contains "$OUT_DIR/help_chat.out" "usage: yvex chat [--model FILE_OR_ALIAS]"
contains "$OUT_DIR/help_chat.out" "current model selected with yvex models use ALIAS"

printf 'cli chat smoke: ok\n'
