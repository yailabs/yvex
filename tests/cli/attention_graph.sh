#!/bin/sh
set -eu

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-./yvex}
OUT_PARENT=${YVEX_TEST_OUT_DIR:-build/tests/cli/attention-graph}
case "$OUT_PARENT" in
    /*) ;;
    *) OUT_PARENT="$PWD/$OUT_PARENT" ;;
esac
yvex_test_cleanup_validate "$OUT_PARENT"
mkdir -p "$OUT_PARENT"
test -d "$OUT_PARENT" && test ! -L "$OUT_PARENT" || {
    printf 'FAIL: unsafe attention test root: %s\n' "$OUT_PARENT" >&2
    exit 1
}
OUT_PARENT=$(cd "$OUT_PARENT" && pwd -P)
OUT_DIR=$(mktemp -d "$OUT_PARENT/run.XXXXXX")
yvex_test_cleanup_validate "$OUT_DIR"

cleanup() {
    status=$?
    trap - 0 HUP INT TERM
    set +e
    yvex_test_cleanup_preserving_status "$status" "$OUT_DIR"
    final_status=$?
    exit "$final_status"
}

trap cleanup 0
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

contains() {
    grep -F -- "$2" "$1" >/dev/null || fail "$1 missing: $2"
}

expect_status() {
    expected=$1
    shift
    set +e
    "$@"
    actual=$?
    set -e
    [ "$actual" -eq "$expected" ] || fail "expected status $expected, got $actual: $*"
}

"$YVEX_BIN" help graph >"$OUT_DIR/help.out" 2>"$OUT_DIR/help.err"
"$YVEX_BIN" graph attention --help >"$OUT_DIR/attention-help.out" \
    2>"$OUT_DIR/attention-help.err"
"$YVEX_BIN" graph attention execute --help >"$OUT_DIR/execute-help.out" \
    2>"$OUT_DIR/execute-help.err"
"$YVEX_BIN" commands >"$OUT_DIR/catalog.out" 2>"$OUT_DIR/catalog.err"
contains "$OUT_DIR/catalog.out" "Graph diagnostics and production attention probes."
contains "$OUT_DIR/help.out" "yvex graph attention execute --target deepseek4-v4-flash"
contains "$OUT_DIR/help.out" "--backend cpu|cuda"
contains "$OUT_DIR/help.out" "--compare-backends"
contains "$OUT_DIR/help.out" "--scope quick|full"
contains "$OUT_DIR/help.out" "--output normal|table|audit|json"
contains "$OUT_DIR/help.out" "not prompt execution"
contains "$OUT_DIR/attention-help.out" "yvex graph attention execute"
contains "$OUT_DIR/execute-help.out" "yvex graph attention execute"

expect_status 2 "$YVEX_BIN" graph attention \
    >"$OUT_DIR/missing-action.out" 2>"$OUT_DIR/missing-action.err"
contains "$OUT_DIR/missing-action.err" "graph attention requires the execute action"

expect_status 2 "$YVEX_BIN" graph attention inspect \
    >"$OUT_DIR/unknown-action.out" 2>"$OUT_DIR/unknown-action.err"
contains "$OUT_DIR/unknown-action.err" "graph attention requires the execute action"

expect_status 2 "$YVEX_BIN" graph attention execute --backend cpu \
    >"$OUT_DIR/missing-target.out" 2>"$OUT_DIR/missing-target.err"
contains "$OUT_DIR/missing-target.err" "requires --target TARGET"

expect_status 2 "$YVEX_BIN" graph attention execute \
    --target deepseek4-v4-flash --backend cpu --compare-backends \
    >"$OUT_DIR/backend-conflict.out" 2>"$OUT_DIR/backend-conflict.err"
contains "$OUT_DIR/backend-conflict.err" "cannot be combined with --backend"

expect_status 2 "$YVEX_BIN" graph attention execute \
    --target deepseek4-v4-flash --backend cpu --probe fixture \
    >"$OUT_DIR/probe-refusal.out" 2>"$OUT_DIR/probe-refusal.err"
contains "$OUT_DIR/probe-refusal.err" "unsupported attention probe: fixture"

expect_status 2 "$YVEX_BIN" graph attention execute \
    --target deepseek4-v4-flash --backend cpu --scope reduced \
    >"$OUT_DIR/scope-refusal.out" 2>"$OUT_DIR/scope-refusal.err"
contains "$OUT_DIR/scope-refusal.err" "unsupported attention scope: reduced"

expect_status 5 "$YVEX_BIN" graph attention execute \
    --target qwen3-8b --backend cpu --scope quick --output json \
    >"$OUT_DIR/target-refusal.json" 2>"$OUT_DIR/target-refusal.err"
contains "$OUT_DIR/target-refusal.err" "unsupported attention target: qwen3-8b"
python3 - "$OUT_DIR/target-refusal.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    result = json.load(stream)
assert result["status"] == "refused"
assert result["target"] == "qwen3-8b"
assert result["failure_code"] == "YVEX_ERR_UNSUPPORTED"
assert result["failure_where"] == "runtime.attention"
assert result["operator_command_available"] is True
assert result["runtime_generation_ready"] is False
assert "family" not in result
assert "execution_class" not in result
assert "weights_class" not in result
assert "layers_executed" not in result
PY

if "$YVEX_BIN" graph --backend cpu --execute-op --op attention \
    --seq-len 4 --position 3 --head-dim 8 --causal \
    >"$OUT_DIR/attention.out" 2>"$OUT_DIR/attention.err"; then
    fail "retired production attention proof unexpectedly executed"
fi
contains "$OUT_DIR/attention.out" "graph_integrity_guard: refused"
contains "$OUT_DIR/attention.out" "graph_execution_phase: admission"
contains "$OUT_DIR/attention.out" "execution_ready: false"
contains "$OUT_DIR/attention.out" "attention_execution_supported: true"
contains "$OUT_DIR/attention.out" "generation_ready: false"
contains "$OUT_DIR/attention.out" "reason: production-fixtures-are-test-owned"
contains "$OUT_DIR/attention.out" "status: graph-proof-retired"
contains "$OUT_DIR/attention.err" "production graph fixtures were retired to focused tests"

if [ "${YVEX_ATTENTION_LIVE:-0}" = 1 ]; then
    MODELS_ROOT=${YVEX_ATTENTION_MODELS_ROOT:?attention live models root is required}
    ARTIFACT=${YVEX_ATTENTION_ARTIFACT:?attention live artifact is required}

    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cpu --probe canonical --scope quick --output audit \
        >"$OUT_DIR/quick-cpu.out" 2>"$OUT_DIR/quick-cpu.err"
    contains "$OUT_DIR/quick-cpu.out" "status: complete"
    contains "$OUT_DIR/quick-cpu.out" "backend: cpu"
    contains "$OUT_DIR/quick-cpu.out" "layers_executed: 3"
    contains "$OUT_DIR/quick-cpu.out" "bindings_executed: 38"

    expect_status 3 "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cpu --probe canonical --scope quick --output json \
        >/dev/full 2>"$OUT_DIR/render-failure.err"
    contains "$OUT_DIR/render-failure.err" "attention result rendering failed"

    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cpu --probe canonical --scope full --output audit \
        >"$OUT_DIR/full-cpu-first.out" 2>"$OUT_DIR/full-cpu-first.err"
    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cpu --probe canonical --scope full --output audit \
        >"$OUT_DIR/full-cpu-second.out" 2>"$OUT_DIR/full-cpu-second.err"
    cmp "$OUT_DIR/full-cpu-first.out" "$OUT_DIR/full-cpu-second.out" >/dev/null ||
        fail "full CPU operator output is not deterministic"
    contains "$OUT_DIR/full-cpu-first.out" "layers_executed: 43"
    contains "$OUT_DIR/full-cpu-first.out" "bindings_executed: 634"
    contains "$OUT_DIR/full-cpu-first.out" "swa_layers_executed: 2"
    contains "$OUT_DIR/full-cpu-first.out" "csa_layers_executed: 21"
    contains "$OUT_DIR/full-cpu-first.out" "hca_layers_executed: 20"

    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cuda --probe canonical --scope quick --output audit \
        >"$OUT_DIR/quick-cuda.out" 2>"$OUT_DIR/quick-cuda.err"
    contains "$OUT_DIR/quick-cuda.out" "status: complete"
    contains "$OUT_DIR/quick-cuda.out" "backend: cuda"
    contains "$OUT_DIR/quick-cuda.out" "layers_executed: 3"
    contains "$OUT_DIR/quick-cuda.out" "attention_cuda_execution_ready: true"

    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --backend cuda --probe canonical --scope full --output audit \
        >"$OUT_DIR/full-cuda.out" 2>"$OUT_DIR/full-cuda.err"
    contains "$OUT_DIR/full-cuda.out" "layers_executed: 43"
    contains "$OUT_DIR/full-cuda.out" "bindings_executed: 634"
    contains "$OUT_DIR/full-cuda.out" "kernel_launches:"

    "$YVEX_BIN" graph attention execute --target deepseek4-v4-flash \
        --models-root "$MODELS_ROOT" --artifact "$ARTIFACT" \
        --compare-backends --probe canonical --scope full --output json \
        >"$OUT_DIR/full-compare.json" 2>"$OUT_DIR/full-compare.err"
    python3 - "$OUT_DIR/full-compare.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    result = json.load(stream)
assert result["status"] == "complete"
assert result["backend"] == "compare"
assert result["layers_executed"] == 43
assert result["bindings_executed"] == 634
assert result["swa_layers_executed"] == 2
assert result["csa_layers_executed"] == 21
assert result["hca_layers_executed"] == 20
assert result["comparison_passed"] is True
assert result["comparison_values"] == 176128
assert result["comparison_finite_values"] == result["comparison_values"]
assert result["comparison_nonfinite_values"] == 0
assert result["comparison_maximum_absolute_error"] == 0
assert result["comparison_maximum_relative_error"] == 0
assert result["comparison_rmse"] == 0
assert len(result["comparison_contract_identity"]) == 64
assert result["bitwise_equality_observed"] is True
assert result["artifact_identity_verified"] is True
assert result["artifact_bytes_hashed"] == 102408545440
assert result["physical_payload_compatible"] is True
assert result["artifact_rebuild_required"] is False
assert result["materialization_rebuild_required"] is False
assert result["operator_command_available"] is True
assert result["end_user_generation_available"] is False
assert result["runtime_generation_ready"] is False
PY

    CONTROL_ARTIFACT=$(printf '%s\001%s' "$OUT_DIR/missing-control" ".gguf")
    expect_status 3 "$YVEX_BIN" graph attention execute \
        --target deepseek4-v4-flash --models-root "$MODELS_ROOT" \
        --artifact "$CONTROL_ARTIFACT" --backend cpu --scope quick --output json \
        >"$OUT_DIR/control-refusal.json" 2>"$OUT_DIR/control-refusal.err"
    python3 - "$OUT_DIR/control-refusal.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    result = json.load(stream)
assert result["status"] == "refused"
assert "\x01" in result["artifact_path"]
assert result["failure_code"] == "YVEX_ERR_IO"
assert result["failure_where"]
assert "layers_executed" not in result
PY
    printf 'cli attention graph live: quick/full CPU/CUDA and full comparison ok\n'
fi

printf 'cli attention graph: production grammar and retired proof refusal ok\n'
