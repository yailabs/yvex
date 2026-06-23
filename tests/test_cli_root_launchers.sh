#!/usr/bin/env sh
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$ROOT_DIR"

OUT_DIR="build/tests/cli-root-launchers"
mkdir -p "$OUT_DIR"

test -x ./yvex
test -x ./yvexd
test -x ./build/bin/yvex
test -x ./build/bin/yvexd

./yvex version > "$OUT_DIR/yvex-version.out"
./yvex commands > "$OUT_DIR/yvex-commands.out"
./yvex help > "$OUT_DIR/yvex-help.out"

grep 'commands' "$OUT_DIR/yvex-commands.out" >/dev/null
grep 'version' "$OUT_DIR/yvex-commands.out" >/dev/null

./yvexd --version > "$OUT_DIR/yvexd-version.out"
./yvexd --help > "$OUT_DIR/yvexd-help.out"

grep 'yvexd' "$OUT_DIR/yvexd-help.out" >/dev/null

restore_yvex() {
  if [ -f build/bin/yvex.tmp ]; then
    mv build/bin/yvex.tmp build/bin/yvex
  fi
}

mv build/bin/yvex build/bin/yvex.tmp
trap restore_yvex EXIT
set +e
./yvex commands > "$OUT_DIR/yvex-missing.out" 2> "$OUT_DIR/yvex-missing.err"
rc=$?
set -e
restore_yvex
trap - EXIT
test "$rc" -eq 127
grep "build/bin/yvex not found or not executable" "$OUT_DIR/yvex-missing.err" >/dev/null
