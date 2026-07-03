#!/usr/bin/env sh
set -eu

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/models-cli}
REG="$ROOT/models.local.json"
GGUF="$ROOT/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"

matches() {
  file=$1
  pattern=$2
  grep -E -- "$pattern" "$file" >/dev/null
}

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
grep 'MODELS  count=1' "$ROOT/list.out"
matches "$ROOT/list.out" '^SEL[[:space:]]{2,}ALIAS[[:space:]]{2,}FAMILY[[:space:]]{2,}CLASS[[:space:]]{2,}TENSORS[[:space:]]{2,}SIZE[[:space:]]{2,}READY$'
matches "$ROOT/list.out" '^[*-][[:space:]]{2,}deepseek4-v4-flash-selected-embed[[:space:]]{2,}deepseek4[[:space:]]{2,}embed[[:space:]]{2,}[0-9]+[[:space:]]{2,}[0-9]+[[:space:]]{2,}no$'
! grep '^[*-] deepseek4-v4-flash-selected-embed deepseek4 embed [0-9]' "$ROOT/list.out"
grep 'status: models-list' "$ROOT/list.out"

"$YVEX_BIN" models list --registry "$REG" --output table > "$ROOT/list-table.out"
grep 'MODELS  count=1' "$ROOT/list-table.out"
matches "$ROOT/list-table.out" '^SEL[[:space:]]{2,}ALIAS[[:space:]]{2,}FAMILY[[:space:]]{2,}CLASS[[:space:]]{2,}TENSORS[[:space:]]{2,}SIZE[[:space:]]{2,}READY$'
grep 'deepseek4-v4-flash-selected-embed' "$ROOT/list-table.out"
grep 'status: models-list' "$ROOT/list-table.out"

"$YVEX_BIN" models list --registry "$REG" --audit > "$ROOT/list-audit.out"
grep 'registered_sha256:' "$ROOT/list-audit.out"
grep 'registered_selected_embedding_ready:' "$ROOT/list-audit.out"
grep 'status: models-list' "$ROOT/list-audit.out"

"$YVEX_BIN" models use deepseek4-v4-flash-selected-embed --registry "$REG" > "$ROOT/use.out"
grep 'selected: deepseek4-v4-flash-selected-embed' "$ROOT/use.out"
grep 'status: models-selected' "$ROOT/use.out"

"$YVEX_BIN" models current --registry "$REG" > "$ROOT/current.out"
grep 'selected: deepseek4-v4-flash-selected-embed' "$ROOT/current.out"
grep 'execution_ready: false' "$ROOT/current.out"
grep 'status: models-current' "$ROOT/current.out"

"$YVEX_BIN" models current --registry "$REG" --audit > "$ROOT/current-audit.out"
grep 'registered_sha256:' "$ROOT/current-audit.out"
grep 'registered_tensor_count:' "$ROOT/current-audit.out"
grep 'status: models-current' "$ROOT/current-audit.out"

"$YVEX_BIN" models list --registry "$REG" --output nope > "$ROOT/list-bad-output.out" 2> "$ROOT/list-bad-output.err" && exit 1 || true
grep 'unsupported output mode: nope' "$ROOT/list-bad-output.err"

"$YVEX_BIN" models inspect deepseek4-v4-flash-selected-embed --registry "$REG" > "$ROOT/inspect.out"
grep 'model: deepseek4-v4-flash-selected-embed' "$ROOT/inspect.out"
grep 'family: deepseek class=selected-slice' "$ROOT/inspect.out"
grep 'boundary: selected-slice only, full-runtime generation unsupported' "$ROOT/inspect.out"
grep 'status: models-inspect' "$ROOT/inspect.out"
test "$(wc -l < "$ROOT/inspect.out")" -le 8

"$YVEX_BIN" models inspect deepseek4-v4-flash-selected-embed --registry "$REG" --audit > "$ROOT/inspect-audit.out"
grep 'alias: deepseek4-v4-flash-selected-embed' "$ROOT/inspect-audit.out"
grep 'gguf:' "$ROOT/inspect-audit.out"
grep 'tensor_count: 1' "$ROOT/inspect-audit.out"
grep 'status: models-inspect' "$ROOT/inspect-audit.out"

"$YVEX_BIN" models inspect deepseek4-v4-flash-selected-embed --registry "$REG" --output nope > "$ROOT/inspect-bad-output.out" 2> "$ROOT/inspect-bad-output.err" && exit 1 || true
grep 'unsupported output mode: nope' "$ROOT/inspect-bad-output.err"

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
grep 'verify: pass alias=deepseek4-v4-flash-selected-embed' "$ROOT/prepare-verify.out"

"$YVEX_BIN" models verify deepseek4-v4-flash-selected-embed --registry "$PREP_REG" --audit > "$ROOT/prepare-verify-audit.out"
grep 'current_sha256:' "$ROOT/prepare-verify-audit.out"
grep 'digest_status: pass' "$ROOT/prepare-verify-audit.out"
grep 'status: models-identity-pass' "$ROOT/prepare-verify-audit.out"

"$YVEX_BIN" models verify deepseek4-v4-flash-selected-embed --registry "$PREP_REG" --output nope > "$ROOT/verify-bad-output.out" 2> "$ROOT/verify-bad-output.err" && exit 1 || true
grep 'unsupported output mode: nope' "$ROOT/verify-bad-output.err"

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

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level quick --registry "$CHECK_REG" > "$ROOT/check-quick-normal.out"
grep 'model-check: pass target=deepseek4-v4-flash-selected-embed level=quick' "$ROOT/check-quick-normal.out"
grep 'boundary: selected-slice check only, generation unsupported' "$ROOT/check-quick-normal.out"
test "$(wc -l < "$ROOT/check-quick-normal.out")" -le 8

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level quick --registry "$CHECK_REG" --audit > "$ROOT/check-quick.out"
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

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --level quick --models-root "$CHECK_ROOT" --audit > "$ROOT/check-models-root.out"
grep 'model_input_kind: target' "$ROOT/check-models-root.out"
grep 'build/tests/model-check/root/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf' "$ROOT/check-models-root.out"
grep 'stage: registry-identity unregistered' "$ROOT/check-models-root.out"
grep 'stage: integrity-check pass' "$ROOT/check-models-root.out"
grep 'status: model-check-pass' "$ROOT/check-models-root.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime --registry "$CHECK_REG" --no-graph --audit > "$ROOT/check-runtime-no-graph.out"
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

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime --registry "$CHECK_REG" --no-materialize --audit > "$ROOT/check-runtime-no-materialize.out"
grep 'stage: integrity-report pass' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: materialize skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: engine skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: session skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: plan skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'stage: graph-partial skipped' "$ROOT/check-runtime-no-materialize.out"
grep 'runtime_execution: not-performed' "$ROOT/check-runtime-no-materialize.out"
grep 'status: model-check-pass' "$ROOT/check-runtime-no-materialize.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --backend cpu --level full --registry "$CHECK_REG" --no-graph --report-dir "$CHECK/reports" --audit > "$ROOT/check-full-report.out"
grep 'level: full' "$ROOT/check-full-report.out"
grep 'stage: model-gate skipped' "$ROOT/check-full-report.out"
grep 'stage: materialize-gate skipped' "$ROOT/check-full-report.out"
grep 'report_path: build/tests/model-check/reports/model-check-deepseek4-v4-flash-selected-embed-cpu-full.txt' "$ROOT/check-full-report.out"
grep 'status: model-check-pass' "$ROOT/check-full-report.out"
test -f "$CHECK/reports/model-check-deepseek4-v4-flash-selected-embed-cpu-full.txt"

"$YVEX_BIN" models check glm-5.2-official-safetensors --dry-run > "$ROOT/check-invalid-dry-run.out" 2> "$ROOT/check-invalid-dry-run.err" && exit 1 || true
grep 'unknown models check option' "$ROOT/check-invalid-dry-run.err"

"$YVEX_BIN" models check glm-5.2-official-safetensors --level quick --audit > "$ROOT/check-glm-unsupported.out" 2> "$ROOT/check-glm-unsupported.err" && exit 1 || true
grep 'status: model-check-unsupported' "$ROOT/check-glm-unsupported.out"
grep 'source-only target cannot be checked as a YVEX-produced runtime artifact yet' "$ROOT/check-glm-unsupported.out"
grep 'generation: unsupported' "$ROOT/check-glm-unsupported.out"

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed-rmsnorm --level quick --audit > "$ROOT/check-segment-unsupported.out" 2> "$ROOT/check-segment-unsupported.err" && exit 1 || true
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

"$YVEX_BIN" models check deepseek4-v4-flash-selected-embed --output nope > "$ROOT/check-invalid-output.out" 2> "$ROOT/check-invalid-output.err" && exit 1 || true
grep 'unsupported output mode: nope' "$ROOT/check-invalid-output.err"

"$YVEX_BIN" help model-target > "$ROOT/model-target-help.out"
grep 'model-target decision --release v0.1.0' "$ROOT/model-target-help.out"
grep 'This command records the v0.1.0 target decision' "$ROOT/model-target-help.out"

"$YVEX_BIN" model-target decision --help > "$ROOT/model-target-decision-help.out"
grep 'usage: yvex model-target decision --release v0.1.0' "$ROOT/model-target-decision-help.out"
grep 'does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks' "$ROOT/model-target-decision-help.out"

"$YVEX_BIN" model-target decision --release v0.1.0 > "$ROOT/model-target-decision-normal.out"
grep 'report: target-decision' "$ROOT/model-target-decision-normal.out"
grep 'status: target-decision-blocked' "$ROOT/model-target-decision-normal.out"
grep 'selected: none' "$ROOT/model-target-decision-normal.out"
grep 'top_blocker: no eligible full-runtime candidate' "$ROOT/model-target-decision-normal.out"
grep 'next: V010.SOURCE.2' "$ROOT/model-target-decision-normal.out"
! grep 'next: V010\.CLI\.18' "$ROOT/model-target-decision-normal.out"
grep 'boundary: report-only; generation unsupported; benchmark not measured' "$ROOT/model-target-decision-normal.out"

"$YVEX_BIN" model-target decision --release v0.1.0 --output table > "$ROOT/model-target-decision-table.out"
matches "$ROOT/model-target-decision-table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$ROOT/model-target-decision-table.out" '^target-decision[[:space:]]{2,}blocked[[:space:]]{2,}none[[:space:]]{2,}0[[:space:]]{2,}V010\.SOURCE\.2$'

"$YVEX_BIN" model-target decision --release v0.1.0 --output nope > "$ROOT/model-target-decision-bad-output.out" 2> "$ROOT/model-target-decision-bad-output.err" && exit 1 || true
grep 'model-target decision: unsupported output mode: nope' "$ROOT/model-target-decision-bad-output.err"

"$YVEX_BIN" model-target decision --release v0.1.0 --audit --include-candidates --include-pressure-targets --include-blockers --include-critical-path --include-next > "$ROOT/model-target-decision.out"
grep 'target_decision: v0.1.0' "$ROOT/model-target-decision.out"
grep 'status: target-decision-blocked' "$ROOT/model-target-decision.out"
grep 'decision_state: blocked-no-candidate' "$ROOT/model-target-decision.out"
grep 'full_runtime_candidate_status: missing' "$ROOT/model-target-decision.out"
grep 'selected_runtime_slice_eligible: false' "$ROOT/model-target-decision.out"
grep 'source_only_eligible: false' "$ROOT/model-target-decision.out"
grep 'external_reference_eligible: false' "$ROOT/model-target-decision.out"
grep 'runtime_claim: unsupported' "$ROOT/model-target-decision.out"
grep 'generation: unsupported-full-model' "$ROOT/model-target-decision.out"
grep 'benchmark_status: not-measured' "$ROOT/model-target-decision.out"
grep 'release_ready: false' "$ROOT/model-target-decision.out"
grep 'candidate.0.id: deepseek4-v4-flash-selected-embed' "$ROOT/model-target-decision.out"
grep 'candidate.0.status: ineligible-selected-slice' "$ROOT/model-target-decision.out"
grep 'candidate.1.id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/model-target-decision.out"
grep 'candidate.1.reason: selected-runtime-slice missing MoE router/expert tensor coverage' "$ROOT/model-target-decision.out"
grep 'candidate.2.id: glm-5.2-official-safetensors' "$ROOT/model-target-decision.out"
grep 'candidate.2.class: huge-source-pressure' "$ROOT/model-target-decision.out"
grep 'candidate.2.status: ineligible-source-only' "$ROOT/model-target-decision.out"
grep 'candidate.3.id: qwen-metal-portability' "$ROOT/model-target-decision.out"
grep 'candidate.3.class: metal-reduced-full-runtime-pressure' "$ROOT/model-target-decision.out"
grep 'candidate.3.status: ineligible-pressure-target' "$ROOT/model-target-decision.out"
grep 'candidate.3.next: source artifact class fields' "$ROOT/model-target-decision.out"
grep 'candidate.4.id: gemma-dense-portability' "$ROOT/model-target-decision.out"
grep 'candidate.4.class: reduced-dense-full-runtime-pressure' "$ROOT/model-target-decision.out"
grep 'candidate.4.status: ineligible-pressure-target' "$ROOT/model-target-decision.out"
grep 'candidate.4.next: source artifact class fields' "$ROOT/model-target-decision.out"
grep 'deepseek_pressure_status: selected-slice-pressure-only' "$ROOT/model-target-decision.out"
grep 'glm_pressure_status: source-storage-pressure-only' "$ROOT/model-target-decision.out"
grep 'qwen_metal_pressure_status: planned-portability-pressure-only' "$ROOT/model-target-decision.out"
grep 'next_required_rows: V010.TARGET.2' "$ROOT/model-target-decision.out"

"$YVEX_BIN" model-target decision --release v0.1.0 --audit --candidate deepseek4-v4-flash-selected-embed-rmsnorm --include-blockers --include-next > "$ROOT/model-target-decision-rmsnorm.out"
grep 'candidate_count: 1' "$ROOT/model-target-decision-rmsnorm.out"
grep 'candidate.0.id: deepseek4-v4-flash-selected-embed-rmsnorm' "$ROOT/model-target-decision-rmsnorm.out"
grep 'candidate.0.status: ineligible-selected-slice' "$ROOT/model-target-decision-rmsnorm.out"
grep 'generation: unsupported-full-model' "$ROOT/model-target-decision-rmsnorm.out"

"$YVEX_BIN" model-target decision --release v0.1.0 --audit --candidate glm-5.2-official-safetensors --include-blockers --include-next > "$ROOT/model-target-decision-glm.out"
grep 'candidate_count: 1' "$ROOT/model-target-decision-glm.out"
grep 'candidate.0.id: glm-5.2-official-safetensors' "$ROOT/model-target-decision-glm.out"
grep 'candidate.0.class: huge-source-pressure' "$ROOT/model-target-decision-glm.out"
grep 'candidate.0.status: ineligible-source-only' "$ROOT/model-target-decision-glm.out"
grep 'generation: unsupported-full-model' "$ROOT/model-target-decision-glm.out"

"$YVEX_BIN" model-target decision --release v0.1.0 --candidate missing-target --include-blockers > "$ROOT/model-target-decision-missing.out" 2> "$ROOT/model-target-decision-missing.err" && exit 1 || true
grep 'status: missing-candidate' "$ROOT/model-target-decision-missing.out"
grep 'candidate_requested: missing-target' "$ROOT/model-target-decision-missing.out"
grep 'runtime_claim: unsupported' "$ROOT/model-target-decision-missing.out"

"$YVEX_BIN" model-target decision --release v9.9.9 > "$ROOT/model-target-decision-unsupported-release.out" 2> "$ROOT/model-target-decision-unsupported-release.err" && exit 1 || true
grep 'target_decision: v9.9.9' "$ROOT/model-target-decision-unsupported-release.out"
grep 'status: unsupported-release' "$ROOT/model-target-decision-unsupported-release.out"
grep 'runtime_claim: unsupported' "$ROOT/model-target-decision-unsupported-release.out"
grep 'generation: unsupported-full-model' "$ROOT/model-target-decision-unsupported-release.out"
grep 'benchmark_status: not-measured' "$ROOT/model-target-decision-unsupported-release.out"
grep 'release_ready: false' "$ROOT/model-target-decision-unsupported-release.out"
