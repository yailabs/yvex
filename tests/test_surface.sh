#!/bin/sh
set -eu

cd "$(dirname "$0")/.."

test -f include/yvex/api.h
test -f src/cli/main.c
test -f src/core/operator.h
test -f src/cli/render/private.h

for surface in backend generate graph kv model_artifacts model_target sampling source; do
  test -f "src/cli/commands/$surface.c" || {
    echo "surface: missing command adapter: $surface" >&2
    exit 1
  }
  test -f "src/cli/input/$surface.c" || {
    echo "surface: missing typed input owner: $surface" >&2
    exit 1
  }
done

for surface in backend generate graph kv model_artifacts model_target sampling source; do
  test -f "src/cli/render/$surface.c" || {
    echo "surface: missing typed render owner: $surface" >&2
    exit 1
  }
done

for writer in out json table; do
  test -f "src/cli/io/$writer.c" || {
    echo "surface: missing CLI byte writer: $writer" >&2
    exit 1
  }
done

if grep -RInE '(^|[^a-zA-Z_])(printf|fprintf|vprintf|vfprintf|puts|fputs|putchar|perror)[[:space:]]*\(' \
    src/cli/commands src/cli/input; then
  echo "surface: command/input owner writes operator output" >&2
  exit 1
fi

if grep -RInE '\b(argc|argv)\b|usage:[[:space:]]*yvex' \
    src/model src/graph src/runtime src/generation src/backend src/source src/artifact src/gguf; then
  echo "surface: domain owner contains CLI grammar" >&2
  exit 1
fi

if find . -maxdepth 1 -type f \( -name 'yvex_*.c' -o -name 'yvex_*_private.h' \) -print | grep .; then
  echo "surface: forbidden root compatibility owner" >&2
  exit 1
fi

echo "surface topology: ok commands=8 inputs=8 renderers=8 writers=3"
