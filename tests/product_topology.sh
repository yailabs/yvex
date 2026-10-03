#!/bin/sh
# Verifies that one executable owns commands, clients, and the foreground server mode.
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}
BUILD_DIR=${BUILD_DIR:-build}
make_inputs=$(make --no-print-directory -s print-build-inputs)

test -x "$YVEX_BIN"
test ! -e ./yvexd
test ! -e ./yvex-dev
test ! -e ./yvex-openai
test ! -e "$BUILD_DIR/package/product/bin/yvex-dev"
test ! -e "$BUILD_DIR/package/product/bin/yvex-openai"
test ! -e "$BUILD_DIR/package/developer"
cmp LICENSE "$BUILD_DIR/package/product/share/yvex/LICENSE"
cmp NOTICE.md "$BUILD_DIR/package/product/share/yvex/NOTICE.md"
test "$(find "$BUILD_DIR/package/product/bin" -type f \
    \( -perm -100 -o -perm -010 -o -perm -001 \) -exec basename {} \; \
    | LC_ALL=C sort | tr '\n' ' ')" = 'yvex '

test "$(nm "$YVEX_BIN" | awk '$NF == "main" || $NF == "_main" { count++ } END { print count + 0 }')" = 1
test "$(rg -l '^fn main\(' src/cli src/server \
    | LC_ALL=C sort | tr '\n' ' ')" = 'src/cli/rust/build.rs src/cli/rust/main.rs '
test "$(nm "$YVEX_BIN" | awk '$NF == "yvex_server_serve" || $NF == "_yvex_server_serve" { count++ } END { print count + 0 }')" = 1
if nm "$YVEX_BIN" | awk '{print $NF}' | grep '^_*yvex_cli_'; then
    echo 'product topology: legacy C CLI symbol in Rust product' >&2
    exit 1
fi
! rg -n '^gateway:|^dev-tools:|^package-dev:|YVEX_OPENAI_BIN|YVEX_DEV_BIN' $make_inputs \
    >/dev/null
printf 'test: product_topology single yvex command/server binary\n'
