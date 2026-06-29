#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/models-cli}
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
grep 'models prepare TARGET' "$ROOT/help.out"
grep 'models check TARGET' "$ROOT/help.out"

PREP="$ROOT/prepare"
PREP_SOURCE="$PREP/hf/deepseek/DeepSeek-V4-Flash"
PREP_REG="$PREP/registry/models.local.json"
PREP_GGUF="$PREP/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
mkdir -p "$PREP_SOURCE"

python3 - "$PREP_SOURCE/model-00001.safetensors" <<'PY'
import json
import struct
import sys

path = sys.argv[1]
header = {
    "__metadata__": {"format": "pt"},
    "embed.weight": {"dtype": "F16", "shape": [8, 4], "data_offsets": [0, 64]},
}
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(path, "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(bytes(range(64)))
PY

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --dry-run --models-root "$PREP" --registry "$PREP_REG" > "$ROOT/prepare-dry-run.out"
grep 'status: model-prepare-dry-run' "$ROOT/prepare-dry-run.out"
grep 'stage: convert-emit planned' "$ROOT/prepare-dry-run.out"
grep 'generation: unsupported' "$ROOT/prepare-dry-run.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --models-root "$ROOT/missing-prepare" --registry "$ROOT/missing-prepare/registry/models.local.json" > "$ROOT/prepare-missing.out" 2> "$ROOT/prepare-missing.err" && exit 1 || true
grep 'stage: source-path fail' "$ROOT/prepare-missing.out"
grep 'status: model-prepare-fail' "$ROOT/prepare-missing.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed-rmsnorm --dry-run > "$ROOT/prepare-segment-unsupported.out" 2> "$ROOT/prepare-segment-unsupported.err" && exit 1 || true
grep 'status: model-prepare-unsupported' "$ROOT/prepare-segment-unsupported.out"
grep 'segment prepare is planned' "$ROOT/prepare-segment-unsupported.out"

"$YVEX_BIN" models prepare glm-5.2-official-safetensors --dry-run > "$ROOT/prepare-glm-unsupported.out" 2> "$ROOT/prepare-glm-unsupported.err" && exit 1 || true
grep 'status: model-prepare-unsupported' "$ROOT/prepare-glm-unsupported.out"
grep 'YVEX-produced GGUF emission for this target is planned' "$ROOT/prepare-glm-unsupported.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --models-root "$PREP" --registry "$PREP_REG" --overwrite --no-register > "$ROOT/prepare-no-register.out"
grep 'stage: source-manifest pass' "$ROOT/prepare-no-register.out"
grep 'stage: convert-emit pass' "$ROOT/prepare-no-register.out"
grep 'stage: registry-add skipped' "$ROOT/prepare-no-register.out"
grep 'status: model-prepare' "$ROOT/prepare-no-register.out"
test -f "$PREP_GGUF"
test ! -f "$PREP_REG"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --models-root "$PREP" --registry "$PREP_REG" --overwrite --no-use > "$ROOT/prepare-no-use.out"
grep 'stage: registry-add pass' "$ROOT/prepare-no-use.out"
grep 'stage: registry-use skipped' "$ROOT/prepare-no-use.out"
grep 'stage: registry-verify pass' "$ROOT/prepare-no-use.out"
grep 'status: model-prepare' "$ROOT/prepare-no-use.out"

"$YVEX_BIN" models current --registry "$PREP_REG" > "$ROOT/prepare-current-none.out"
grep 'selected: none' "$ROOT/prepare-current-none.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --models-root "$PREP" --registry "$PREP_REG" --overwrite > "$ROOT/prepare-register-use.out"
grep 'stage: registry-remove-existing pass' "$ROOT/prepare-register-use.out"
grep 'stage: registry-use pass' "$ROOT/prepare-register-use.out"
grep 'status: model-prepare' "$ROOT/prepare-register-use.out"

"$YVEX_BIN" models verify deepseek4-v4-flash-selected-embed --registry "$PREP_REG" > "$ROOT/prepare-verify.out"
grep 'status: models-identity-pass' "$ROOT/prepare-verify.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --models-root "$PREP" --registry "$PREP_REG" > "$ROOT/prepare-overwrite-refused.out" 2> "$ROOT/prepare-overwrite-refused.err" && exit 1 || true
grep 'stage: convert-emit refused' "$ROOT/prepare-overwrite-refused.out"
grep 'status: model-prepare-refused' "$ROOT/prepare-overwrite-refused.out"

"$YVEX_BIN" models prepare deepseek4-v4-flash-selected-embed --out "$PREP_GGUF" --out-dir "$PREP/gguf/deepseek" > "$ROOT/prepare-invalid.out" 2> "$ROOT/prepare-invalid.err" && exit 1 || true
grep 'mutually exclusive' "$ROOT/prepare-invalid.err"

CHECK="build/tests/model-check"
CHECK_REG="$CHECK/registry/models.local.json"
CHECK_GGUF="$CHECK/models/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
CHECK_ROOT="$CHECK/root"
CHECK_ROOT_GGUF="$CHECK_ROOT/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
rm -rf "$CHECK"
mkdir -p "$CHECK/models" "$CHECK/registry" "$CHECK_ROOT/gguf/deepseek"

"$YVEX_BIN" gguf-emit controlled --out "$CHECK_GGUF" --model-name model-check-test --arch llama --overwrite >/dev/null
"$YVEX_BIN" gguf-emit controlled --out "$CHECK_ROOT_GGUF" --model-name model-check-target-root-test --arch llama --overwrite >/dev/null
"$YVEX_BIN" models add --path "$CHECK_GGUF" --alias deepseek4-v4-flash-selected-embed --support-level selected-tensor-materialized --registry "$CHECK_REG" > "$ROOT/check-add.out"
grep 'status: models-added' "$ROOT/check-add.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level quick --registry "$CHECK_REG" > "$ROOT/check-quick.out"
grep 'status: model-check' "$ROOT/check-quick.out"
grep 'target_id: deepseek4-v4-flash-selected-embed' "$ROOT/check-quick.out"
grep 'backend: cpu' "$ROOT/check-quick.out"
grep 'level: quick' "$ROOT/check-quick.out"
grep 'stage: inspect pass' "$ROOT/check-quick.out"
grep 'stage: tensors pass' "$ROOT/check-quick.out"
grep 'stage: metadata pass' "$ROOT/check-quick.out"
grep 'stage: registry-identity pass' "$ROOT/check-quick.out"
grep 'stage: integrity-check pass' "$ROOT/check-quick.out"
grep 'stage: materialize skipped' "$ROOT/check-quick.out"
grep 'stage: graph-partial skipped' "$ROOT/check-quick.out"
grep 'execution_ready: false' "$ROOT/check-quick.out"
grep 'generation: unsupported' "$ROOT/check-quick.out"
grep 'status: model-check-pass' "$ROOT/check-quick.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level quick --models-root "$CHECK_ROOT" > "$ROOT/check-models-root.out"
grep 'model_input_kind: target' "$ROOT/check-models-root.out"
grep 'build/tests/model-check/root/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf' "$ROOT/check-models-root.out"
grep 'stage: registry-identity unregistered' "$ROOT/check-models-root.out"
grep 'stage: integrity-check pass' "$ROOT/check-models-root.out"
grep 'status: model-check-pass' "$ROOT/check-models-root.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime --registry "$CHECK_REG" --no-graph > "$ROOT/check-runtime-no-graph.out"
grep 'status: model-check' "$ROOT/check-runtime-no-graph.out"
grep 'level: runtime' "$ROOT/check-runtime-no-graph.out"
grep 'stage: integrity-report pass' "$ROOT/check-runtime-no-graph.out"
grep 'stage: materialize pass' "$ROOT/check-runtime-no-graph.out"
grep 'stage: engine pass' "$ROOT/check-runtime-no-graph.out"
grep 'stage: session pass' "$ROOT/check-runtime-no-graph.out"
grep 'stage: plan pass' "$ROOT/check-runtime-no-graph.out"
grep 'stage: graph-partial skipped' "$ROOT/check-runtime-no-graph.out"
grep 'runtime_execution: selected-boundary-only' "$ROOT/check-runtime-no-graph.out"
grep 'generation: unsupported' "$ROOT/check-runtime-no-graph.out"
grep 'status: model-check-pass' "$ROOT/check-runtime-no-graph.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime --registry "$CHECK_REG" --no-materialize > "$ROOT/check-runtime-no-materialize.out"
grep 'stage: integrity-report pass' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: materialize skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: engine skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: session skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: plan skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: graph-partial skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'runtime_execution: not-performed' "$ROOT/check-runtime-no-materialize.out"
grep 'status: model-check-pass' "$ROOT/check-runtime-no-materialize.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level full --registry "$CHECK_REG" --no-graph --report-dir "$CHECK/reports" > "$ROOT/check-full-report.out"
grep 'level: full' "$ROOT/check-full-report.out"
grep 'stage: model-gate skipped' "$ROOT/check-full-report.out"
grep 'stage: materialize-gate skipped' "$ROOT/check-full-report.out"
grep 'report_path: build/tests/model-check/reports/model-check-deepseek4-v4-flash-selected-embed-cpu-full.txt' "$ROOT/check-full-report.out"
grep 'status: model-check-pass' "$ROOT/check-full-report.out"
test -f "$CHECK/reports/model-check-deepseek4-v4-flash-selected-embed-cpu-full.txt"

"$YVEX_BIN" models check glm-5.2-official-safetensors --dry-run > "$ROOT/check-invalid-dry-run.out" 2> "$ROOT/check-invalid-dry-run.err" && exit 1 || true
grep 'unknown models check option' "$ROOT/check-invalid-dry-run.err"

"$YVEX_BIN" models check glm-5.2-official-safetensors --level quick > "$ROOT/check-glm-unsupported.out" 2> "$ROOT/check-glm-unsupported.err" && exit 1 || true
grep 'status: model-check-unsupported' "$ROOT/check-glm-unsupported.out"
grep 'source-only target cannot be checked as a YVEX-produced runtime artifact yet' "$ROOT/check-glm-unsupported.out"
grep 'generation: unsupported' "$ROOT/check-glm-unsupported.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed-rmsnorm --level quick > "$ROOT/check-segment-unsupported.out" 2> "$ROOT/check-segment-unsupported.err" && exit 1 || true
grep 'status: model-check-unsupported' "$ROOT/check-segment-unsupported.out"
grep 'segment check is planned' "$ROOT/check-segment-unsupported.out"
grep 'generation: unsupported' "$ROOT/check-segment-unsupported.out"

"$YVEX_BIN" models check > "$ROOT/check-invalid-missing-target.out" 2> "$ROOT/check-invalid-missing-target.err" && exit 1 || true
grep 'requires TARGET' "$ROOT/check-invalid-missing-target.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend > "$ROOT/check-invalid-backend-missing.out" 2> "$ROOT/check-invalid-backend-missing.err" && exit 1 || true
grep 'requires a value' "$ROOT/check-invalid-backend-missing.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend missing > "$ROOT/check-invalid-backend.out" 2> "$ROOT/check-invalid-backend.err" && exit 1 || true
grep 'unknown backend kind' "$ROOT/check-invalid-backend.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level > "$ROOT/check-invalid-level-missing.out" 2> "$ROOT/check-invalid-level-missing.err" && exit 1 || true
grep 'requires a value' "$ROOT/check-invalid-level-missing.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level impossible > "$ROOT/check-invalid-level.out" 2> "$ROOT/check-invalid-level.err" && exit 1 || true
grep 'unknown models check level' "$ROOT/check-invalid-level.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --registry "" > "$ROOT/check-invalid-registry.out" 2> "$ROOT/check-invalid-registry.err" && exit 1 || true
grep 'empty or invalid' "$ROOT/check-invalid-registry.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --report-dir "" > "$ROOT/check-invalid-report-dir.out" 2> "$ROOT/check-invalid-report-dir.err" && exit 1 || true
grep 'empty or invalid' "$ROOT/check-invalid-report-dir.err"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --unknown-flag > "$ROOT/check-invalid-unknown.out" 2> "$ROOT/check-invalid-unknown.err" && exit 1 || true
grep 'unknown models check option' "$ROOT/check-invalid-unknown.err"
