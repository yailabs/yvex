#!/bin/sh
#
# YVEX - CLI smoke tests
#
# File: tests/test_cli.sh
# Layer: test
#
# Purpose:
#   Proves that the CLI command table exposes only implemented commands and
#   returns stable exit codes for common bootstrap and B0 filesystem behavior.
#
# Covers:
#   - yvex
#   - yvex --help
#   - yvex -h
#   - yvex --version
#   - yvex version
#   - yvex info
#   - yvex inspect
#   - yvex paths
#   - yvex commands
#   - yvex help info
#   - yvex unknown
#
# Commands:
#   - make test-cli
#   - make smoke
#
# Expected:
#   - exits 0 on success
#   - prints concise failure to stderr

set -eu

YVEX_BIN=${YVEX_BIN:-build/bin/yvex}
OUT_DIR=${YVEX_TEST_OUT_DIR:-build/tests/cli}

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

run_ok no_args "$YVEX_BIN"
contains "$OUT_DIR/no_args.out" "usage: yvex <command> [options]"

run_ok help_long "$YVEX_BIN" --help
contains "$OUT_DIR/help_long.out" "Implemented commands:"

run_ok help_short "$YVEX_BIN" -h
contains "$OUT_DIR/help_short.out" "Implemented commands:"

run_ok version_option "$YVEX_BIN" --version
contains "$OUT_DIR/version_option.out" "yvex 0.1.0"

run_ok version_command "$YVEX_BIN" version
contains "$OUT_DIR/version_command.out" "yvex 0.1.0"

run_ok info "$YVEX_BIN" info
contains "$OUT_DIR/info.out" "name: YVEX"
contains "$OUT_DIR/info.out" "status: C0 artifact/GGUF header skeleton"
contains "$OUT_DIR/info.out" "library: libyvex.a"
contains "$OUT_DIR/info.out" "filesystem: implemented"
contains "$OUT_DIR/info.out" "artifact: open/read implemented"
contains "$OUT_DIR/info.out" "gguf: header/probe only"

run_ok commands "$YVEX_BIN" commands
contains "$OUT_DIR/commands.out" "Implemented commands:"
contains "$OUT_DIR/commands.out" "  commands"
contains "$OUT_DIR/commands.out" "  help"
contains "$OUT_DIR/commands.out" "  info"
contains "$OUT_DIR/commands.out" "  inspect"
contains "$OUT_DIR/commands.out" "  paths"
contains "$OUT_DIR/commands.out" "  version"

run_ok help_info "$YVEX_BIN" help info
contains "$OUT_DIR/help_info.out" "usage: yvex info"

run_ok help_inspect "$YVEX_BIN" help inspect
contains "$OUT_DIR/help_inspect.out" "usage: yvex inspect <path>"

run_ok inspect_valid "$YVEX_BIN" inspect tests/fixtures/gguf/valid-minimal.gguf
contains "$OUT_DIR/inspect_valid.out" "format: gguf"
contains "$OUT_DIR/inspect_valid.out" "version: 3"
contains "$OUT_DIR/inspect_valid.out" "metadata_count: 0"
contains "$OUT_DIR/inspect_valid.out" "tensor_count: 0"
contains "$OUT_DIR/inspect_valid.out" "status: header-only"

set +e
"$YVEX_BIN" inspect tests/fixtures/gguf/bad-magic.gguf >"$OUT_DIR/inspect_bad_magic.out" 2>"$OUT_DIR/inspect_bad_magic.err"
rc=$?
set -e
if [ "$rc" -ne 5 ]; then
    fail "bad magic inspect exit code was $rc, expected 5"
fi
contains "$OUT_DIR/inspect_bad_magic.out" "format: unknown"
contains "$OUT_DIR/inspect_bad_magic.out" "status: unsupported"

run_ok help_paths "$YVEX_BIN" help paths
contains "$OUT_DIR/help_paths.out" "usage: yvex paths"

run_ok paths "$YVEX_BIN" paths
contains "$OUT_DIR/paths.out" "config:"
contains "$OUT_DIR/paths.out" "cache:"
contains "$OUT_DIR/paths.out" "state:"
contains "$OUT_DIR/paths.out" "data:"
contains "$OUT_DIR/paths.out" "project:"

run_ok paths_project "$YVEX_BIN" paths --project .
contains "$OUT_DIR/paths_project.out" "project: ./.yvex"

run_ok paths_run "$YVEX_BIN" paths --run
contains "$OUT_DIR/paths_run.out" "run_id: run_"
contains "$OUT_DIR/paths_run.out" "command:"

(
    export YVEX_RUN_DIR="$OUT_DIR/runs"
    run_ok paths_run_create "$YVEX_BIN" paths --run --create
)
contains "$OUT_DIR/paths_run_create.out" "root: $OUT_DIR/runs/run_"
test -d "$OUT_DIR/runs" || fail "paths --run --create did not create run root"

set +e
"$YVEX_BIN" unknown >"$OUT_DIR/unknown.out" 2>"$OUT_DIR/unknown.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "unknown command exit code was $rc, expected 2"
fi
contains "$OUT_DIR/unknown.err" "yvex: unknown command: unknown"
contains "$OUT_DIR/unknown.err" "Try 'yvex help' for usage."

set +e
"$YVEX_BIN" help unknown >"$OUT_DIR/help_unknown.out" 2>"$OUT_DIR/help_unknown.err"
rc=$?
set -e
if [ "$rc" -ne 2 ]; then
    fail "unknown help topic exit code was $rc, expected 2"
fi
contains "$OUT_DIR/help_unknown.err" "yvex: unknown help topic: unknown"

printf 'cli smoke: ok\n'
