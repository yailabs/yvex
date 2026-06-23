#!/usr/bin/env sh
set -eu

test ! -d src
test ! -d cli
test ! -d server

test -f yvex_cli.c
test -f yvexd.c
test -f yvex_gguf.c
test -f yvex_model_registry.c
test -f yvex_chat_repl.c
test -f yvex_server.c

test -d include/yvex
test -d backends/cuda
test -d docs
test -d tests

echo "source layout: ok"
