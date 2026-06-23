#!/usr/bin/env sh
set -eu

test -f README.md
test -f AGENTS.md
test -f MODEL_ARTIFACTS.md
test -f docs/api.md
test -f docs/contract.md
test -f docs/spine.md

test ! -e docs/README.md
test ! -e docs/backend-contract.md
test ! -e docs/cli-commands.md
test ! -e docs/cli-interface-spine.md
test ! -e docs/cli-runtime.md
test ! -e docs/runtime-filesystem.md

count="$(find docs -maxdepth 1 -type f | wc -l | tr -d ' ')"
if [ "$count" -ne 3 ]; then
  echo "unexpected docs file count: $count"
  find docs -maxdepth 1 -type f | sort
  exit 1
fi

echo "docs surface: ok"
