#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./build/bin/yvex}"
ROOT="build/tests/models-cli"
REG="$ROOT/models.local.json"
GGUF="$ROOT/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"

rm -rf "$ROOT"
mkdir -p "$ROOT"

"$YVEX_BIN" gguf-emit controlled \
  --out "$GGUF" \
  --model-name model-registry-test \
  --arch llama \
  --overwrite >/dev/null

"$YVEX_BIN" models scan --root "$ROOT" --registry "$REG" > "$ROOT/scan.out"
grep 'candidate: deepseek4-v4-flash-selected-embed' "$ROOT/scan.out"
grep 'status: models-scan' "$ROOT/scan.out"

"$YVEX_BIN" models add --path "$GGUF" --registry "$REG" > "$ROOT/add.out"
grep 'alias: deepseek4-v4-flash-selected-embed' "$ROOT/add.out"
grep 'status: models-added' "$ROOT/add.out"
test -f "$REG"

"$YVEX_BIN" models list --registry "$REG" > "$ROOT/list.out"
grep 'deepseek4-v4-flash-selected-embed' "$ROOT/list.out"
grep 'status: models-list' "$ROOT/list.out"

"$YVEX_BIN" models use deepseek4-v4-flash-selected-embed --registry "$REG" > "$ROOT/use.out"
grep 'selected: deepseek4-v4-flash-selected-embed' "$ROOT/use.out"
grep 'status: models-selected' "$ROOT/use.out"

"$YVEX_BIN" models current --registry "$REG" > "$ROOT/current.out"
grep 'selected: deepseek4-v4-flash-selected-embed' "$ROOT/current.out"
grep 'execution_ready: false' "$ROOT/current.out"
grep 'status: models-current' "$ROOT/current.out"

"$YVEX_BIN" models inspect deepseek4-v4-flash-selected-embed --registry "$REG" > "$ROOT/inspect.out"
grep 'alias: deepseek4-v4-flash-selected-embed' "$ROOT/inspect.out"
grep 'gguf:' "$ROOT/inspect.out"
grep 'tensor_count: 1' "$ROOT/inspect.out"
grep 'status: models-inspect' "$ROOT/inspect.out"

"$YVEX_BIN" models remove deepseek4-v4-flash-selected-embed --registry "$REG" > "$ROOT/remove.out"
grep 'removed: deepseek4-v4-flash-selected-embed' "$ROOT/remove.out"
grep 'status: models-removed' "$ROOT/remove.out"

"$YVEX_BIN" models current --registry "$REG" > "$ROOT/current-none.out"
grep 'selected: none' "$ROOT/current-none.out"
grep 'status: models-none' "$ROOT/current-none.out"

"$YVEX_BIN" models use missing --registry "$REG" > "$ROOT/use-missing.out" 2> "$ROOT/use-missing.err" && exit 1 || true
grep 'alias not found: missing' "$ROOT/use-missing.err"

"$YVEX_BIN" help models > "$ROOT/help.out"
grep 'yvex models' "$ROOT/help.out"
