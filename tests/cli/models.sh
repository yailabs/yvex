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

expect_rc() {
  expected=$1
  shift
  set +e
  "$@"
  rc=$?
  set -e
  test "$rc" -eq "$expected"
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
grep 'models download TARGET' "$ROOT/help.out"
grep -- '--auth auto|required|never' "$ROOT/help.out"
grep -- '--progress auto|live|plain|log|off' "$ROOT/help.out"
grep 'models download status qwen3-32b' "$ROOT/help.out"
grep 'models prepare TARGET' "$ROOT/help.out"
grep 'models check TARGET' "$ROOT/help.out"

FAKE_HF="$PWD/tests/fixtures/bin/fake-hf"
FAKE_GH="$PWD/tests/fixtures/bin/fake-gh"
DOWNLOAD_ROOT="$ROOT/download"
export YVEX_CONFIG_DIR="$ROOT/accounts-config"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --dry-run --models-root "$DOWNLOAD_ROOT" --auth never --audit > "$ROOT/download-dry-run.out"
grep 'status: model-download-dry-run' "$ROOT/download-dry-run.out"
grep 'family: gemma' "$ROOT/download-dry-run.out"
grep 'stage: account-provider skipped' "$ROOT/download-dry-run.out"
grep 'payload_loaded: false' "$ROOT/download-dry-run.out"
grep 'gguf_created: false' "$ROOT/download-dry-run.out"
grep 'generation: unsupported' "$ROOT/download-dry-run.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$DOWNLOAD_ROOT" --auth auto --audit > "$ROOT/download-gemma.out"
grep 'status: model-download-pass' "$ROOT/download-gemma.out"
grep 'provider: huggingface' "$ROOT/download-gemma.out"
grep 'stage: account-provider pass' "$ROOT/download-gemma.out"
grep 'stage: provider-cli pass' "$ROOT/download-gemma.out"
grep 'stage: source-manifest pass' "$ROOT/download-gemma.out"
grep 'stage: native-inventory pass' "$ROOT/download-gemma.out"
grep 'stage: progress-stream pass' "$ROOT/download-gemma.out"
grep 'hf/gemma/gemma-4-12b-it' "$ROOT/download-gemma.out"
grep 'gguf_created: false' "$ROOT/download-gemma.out"
grep 'payload_loaded: false' "$ROOT/download-gemma.out"
grep 'generation: unsupported' "$ROOT/download-gemma.out"
grep 'benchmark_status: not-measured' "$ROOT/download-gemma.out"
test -f "$DOWNLOAD_ROOT/reports/gemma/gemma-4-12b-it.source-manifest.json"
test -f "$DOWNLOAD_ROOT/reports/gemma/gemma-4-12b-it.native-inventory.json"
test -f "$DOWNLOAD_ROOT/reports/gemma/gemma-4-12b-it.download-report.json"
test -f "$DOWNLOAD_ROOT/registry/gemma/gemma-4-12b-it.download.json"
! find "$DOWNLOAD_ROOT/gguf" -type f -name '*.gguf' 2>/dev/null | grep .

LIVE_ROOT="$ROOT/download-live"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=1 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$LIVE_ROOT" --auth required --progress plain --tick-seconds 1 --audit > "$ROOT/download-live.out" 2>&1 &
LIVE_PID=$!
sleep 1
grep 'model-download: start target=gemma-4-12b-it' "$ROOT/download-live.out"
grep 'stage: download running' "$ROOT/download-live.out"
wait "$LIVE_PID"
grep 'tick: elapsed=' "$ROOT/download-live.out"
grep 'files=' "$ROOT/download-live.out"
grep 'bytes=' "$ROOT/download-live.out"
grep 'fake-hf: resolving repo' "$ROOT/download-live.out"
grep 'fake-hf: downloading shard' "$ROOT/download-live.out"
grep 'progress_mode: plain' "$ROOT/download-live.out"
grep 'tick_seconds: 1' "$ROOT/download-live.out"
grep 'stdout_streamed: true' "$ROOT/download-live.out"
grep 'stderr_streamed: true' "$ROOT/download-live.out"
grep 'provider_exit_code: 0' "$ROOT/download-live.out"
test -f "$LIVE_ROOT/logs/gemma-4-12b-it.download.stdout.log"
test -f "$LIVE_ROOT/logs/gemma-4-12b-it.download.stderr.log"
grep 'fake-hf: resolving repo' "$LIVE_ROOT/logs/gemma-4-12b-it.download.stdout.log"
grep 'fake-hf: stderr resolving repo' "$LIVE_ROOT/logs/gemma-4-12b-it.download.stderr.log"

LIVE_FAIL_ROOT="$ROOT/download-live-fail"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_FAIL_AT_STEP=2 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$LIVE_FAIL_ROOT" --progress plain --tick-seconds 1 --audit > "$ROOT/download-live-fail.out" 2>&1 && exit 1 || true
grep 'status: model-download-fail' "$ROOT/download-live-fail.out"
grep 'provider_exit_code: 43' "$ROOT/download-live-fail.out"
grep 'stdout_log:' "$ROOT/download-live-fail.out"
grep 'stderr_log:' "$ROOT/download-live-fail.out"
grep 'top_blocker: provider-download-failed' "$ROOT/download-live-fail.out"
test -f "$LIVE_FAIL_ROOT/logs/gemma-4-12b-it.download.stdout.log"
test -f "$LIVE_FAIL_ROOT/logs/gemma-4-12b-it.download.stderr.log"
grep 'fake-hf: downloading shard 1' "$LIVE_FAIL_ROOT/logs/gemma-4-12b-it.download.stdout.log"
grep 'fake-hf: failing at step 2' "$LIVE_FAIL_ROOT/logs/gemma-4-12b-it.download.stderr.log"

SIGNAL_ROOT="$ROOT/download-signal"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=5 YVEX_FAKE_HF_STEPS=8 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$SIGNAL_ROOT" --progress plain --tick-seconds 1 --audit > "$ROOT/download-signal.out" 2>&1 &
SIGNAL_PID=$!
sleep 1
kill -INT "$SIGNAL_PID"
set +e
wait "$SIGNAL_PID"
SIGNAL_RC=$?
set -e
test "$SIGNAL_RC" -ne 0
grep 'status: model-download-interrupted' "$ROOT/download-signal.out"
grep 'stage: download interrupted' "$ROOT/download-signal.out"
grep 'signal: SIGINT' "$ROOT/download-signal.out"
grep 'child_signal_forwarded: true' "$ROOT/download-signal.out"
grep 'child_exit_status: interrupted' "$ROOT/download-signal.out"
grep 'orphan_check_performed: true' "$ROOT/download-signal.out"
grep 'orphan_check_status: pass' "$ROOT/download-signal.out"
grep 'partial_source_preserved: true' "$ROOT/download-signal.out"
grep 'lock_cleanup: not-attempted' "$ROOT/download-signal.out"
grep 'lock_files_deleted: false' "$ROOT/download-signal.out"
grep 'provider_pid:' "$ROOT/download-signal.out"
grep 'provider_process_group:' "$ROOT/download-signal.out"
PROVIDER_PID=$(awk '/provider_pid:/ { print $2; exit }' "$ROOT/download-signal.out")
PROVIDER_PGID=$(awk '/provider_process_group:/ { print $2; exit }' "$ROOT/download-signal.out")
test -n "$PROVIDER_PID"
test -n "$PROVIDER_PGID"
! kill -0 "$PROVIDER_PID" 2>/dev/null
! ps -o pid= -g "$PROVIDER_PGID" | grep .
test -f "$SIGNAL_ROOT/logs/gemma-4-12b-it.download.stdout.log"
test -f "$SIGNAL_ROOT/logs/gemma-4-12b-it.download.stderr.log"
test -f "$SIGNAL_ROOT/hf/gemma/gemma-4-12b-it/config.json"
grep 'fake-hf: resolving repo' "$SIGNAL_ROOT/logs/gemma-4-12b-it.download.stdout.log"
grep 'fake-hf: stderr resolving repo' "$SIGNAL_ROOT/logs/gemma-4-12b-it.download.stderr.log"

CONTROL_ROOT="$ROOT/download-control"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=5 YVEX_FAKE_HF_STEPS=8 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$CONTROL_ROOT" --auth required --progress log --tick-seconds 1 --audit > "$ROOT/download-control-run.out" 2>&1 &
CONTROL_PID=$!
i=0
while [ ! -f "$CONTROL_ROOT/reports/gemma/gemma-4-12b-it.download.active.json" ] && [ "$i" -lt 10 ]; do
  sleep 1
  i=$((i + 1))
done
test -f "$CONTROL_ROOT/reports/gemma/gemma-4-12b-it.download.active.json"

"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-running.out"
grep 'status: model-download-status' "$ROOT/download-control-status-running.out"
grep 'receipt_status: active' "$ROOT/download-control-status-running.out"
grep 'provider_process_alive: true' "$ROOT/download-control-status-running.out"
grep 'stop_available: true' "$ROOT/download-control-status-running.out"
grep 'resume_available: false' "$ROOT/download-control-status-running.out"

"$YVEX_BIN" models download stop gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-stop.out"
grep 'status: model-download-stopped' "$ROOT/download-control-stop.out"
grep 'child_signal_forwarded: true' "$ROOT/download-control-stop.out"
grep 'cleanup: preserved-partial-source' "$ROOT/download-control-stop.out"
set +e
wait "$CONTROL_PID"
CONTROL_RC=$?
set -e
test "$CONTROL_RC" -ne 0
test -f "$CONTROL_ROOT/hf/gemma/gemma-4-12b-it/config.json"

"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-stopped.out"
grep 'provider_process_alive: false' "$ROOT/download-control-status-stopped.out"
grep 'last_receipt_status: stopped' "$ROOT/download-control-status-stopped.out"
grep 'resume_available: true' "$ROOT/download-control-status-stopped.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download resume gemma-4-12b-it --models-root "$CONTROL_ROOT" --auth required --progress log --tick-seconds 1 --audit > "$ROOT/download-control-resume.out" 2>&1
grep 'status: model-download-resume-pass' "$ROOT/download-control-resume.out"
grep 'stage: download pass' "$ROOT/download-control-resume.out"
test -f "$CONTROL_ROOT/reports/gemma/gemma-4-12b-it.download.last.json"
"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$CONTROL_ROOT" --audit > "$ROOT/download-control-status-resumed.out"
grep 'last_receipt_status: pass' "$ROOT/download-control-status-resumed.out"

STALE_ROOT="$ROOT/download-control-stale"
mkdir -p "$STALE_ROOT/reports/gemma" "$STALE_ROOT/hf/gemma/gemma-4-12b-it"
cat > "$STALE_ROOT/reports/gemma/gemma-4-12b-it.download.active.json" <<EOF
{
  "schema": "yvex.model_download.active.v1",
  "target_id": "gemma-4-12b-it",
  "family": "gemma",
  "provider": "huggingface",
  "repo_id": "google/gemma-4-12B-it",
  "revision": "main",
  "local_source_dir": "$STALE_ROOT/hf/gemma/gemma-4-12b-it",
  "provider_pid": 99999999,
  "provider_pgid": 99999999,
  "status": "running"
}
EOF
"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$STALE_ROOT" --audit > "$ROOT/download-control-stale-status.out"
grep 'receipt_status: stale-active-receipt' "$ROOT/download-control-stale-status.out"

mkdir -p "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download"
printf 'lock\n' > "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download/model.safetensors.lock"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download resume gemma-4-12b-it --models-root "$STALE_ROOT" --auth required --audit > "$ROOT/download-control-lock-blocked.out" 2>&1 && exit 1 || true
grep 'status: model-download-resume-blocked' "$ROOT/download-control-lock-blocked.out"
grep 'top_blocker: stale-lock-candidates' "$ROOT/download-control-lock-blocked.out"

"$YVEX_BIN" models download cleanup gemma-4-12b-it --models-root "$STALE_ROOT" --stale-locks --dry-run --audit > "$ROOT/download-control-cleanup-dry-run.out"
grep 'status: model-download-cleanup-dry-run' "$ROOT/download-control-cleanup-dry-run.out"
grep 'stale_locks: 1' "$ROOT/download-control-cleanup-dry-run.out"
test -f "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download/model.safetensors.lock"

"$YVEX_BIN" models download cleanup gemma-4-12b-it --models-root "$STALE_ROOT" --stale-locks --yes --audit > "$ROOT/download-control-cleanup.out"
grep 'status: model-download-cleanup' "$ROOT/download-control-cleanup.out"
grep 'deleted: 1' "$ROOT/download-control-cleanup.out"
test ! -f "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download/model.safetensors.lock"

printf 'lock\n' > "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download/model.safetensors.lock"
YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download resume gemma-4-12b-it --models-root "$STALE_ROOT" --auth required --clear-stale-locks --audit > "$ROOT/download-control-lock-clear-resume.out" 2>&1
grep 'status: model-download-resume-pass' "$ROOT/download-control-lock-clear-resume.out"
test ! -f "$STALE_ROOT/hf/gemma/gemma-4-12b-it/.cache/huggingface/download/model.safetensors.lock"

SAFE_ROOT="$ROOT/download-control-safe"
SAFE_SRC="$SAFE_ROOT/hf/gemma/gemma-4-12b-it"
mkdir -p "$SAFE_SRC"
python3 - "$SAFE_SRC/model-ok.safetensors" "$SAFE_SRC/model-truncated.safetensors" <<'PY'
import json
import struct
import sys

def write(path, payload_len, actual_len):
    header = {
        "__metadata__": {"format": "pt"},
        "embed.weight": {
            "dtype": "F16",
            "shape": [1, max(1, payload_len // 2)],
            "data_offsets": [0, payload_len],
        },
    }
    blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
    with open(path, "wb") as f:
        f.write(struct.pack("<Q", len(blob)))
        f.write(blob)
        f.write(bytes(actual_len))

write(sys.argv[1], 16, 16)
write(sys.argv[2], 32, 8)
PY
"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$SAFE_ROOT" --audit > "$ROOT/download-control-safe-truncated.out"
grep 'safetensors_header_checked: true' "$ROOT/download-control-safe-truncated.out"
grep 'safetensors_size_status: truncated' "$ROOT/download-control-safe-truncated.out"
rm -f "$SAFE_SRC/model-truncated.safetensors"
"$YVEX_BIN" models download status gemma-4-12b-it --models-root "$SAFE_ROOT" --audit > "$ROOT/download-control-safe-ok.out"
grep 'safetensors_size_status: ok' "$ROOT/download-control-safe-ok.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-e2b --models-root "$ROOT/download-off" --no-progress --audit > "$ROOT/download-off.out" 2>&1
! grep 'model-download: start' "$ROOT/download-off.out"
! grep 'tick: elapsed=' "$ROOT/download-off.out"
! grep 'fake-hf: resolving repo' "$ROOT/download-off.out"
grep 'status: model-download-pass' "$ROOT/download-off.out"

LOG_PROGRESS_ROOT="$ROOT/download-log-progress"
YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=1 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-e2b-it --models-root "$LOG_PROGRESS_ROOT" --progress log --tick-seconds 1 --audit > "$ROOT/download-log-progress.out" 2>&1
grep 'tick: elapsed=' "$ROOT/download-log-progress.out"
! grep 'fake-hf: resolving repo' "$ROOT/download-log-progress.out"
grep 'progress_mode: log' "$ROOT/download-log-progress.out"
test -f "$LOG_PROGRESS_ROOT/logs/gemma-4-e2b-it.download.stdout.log"
grep 'fake-hf: resolving repo' "$LOG_PROGRESS_ROOT/logs/gemma-4-e2b-it.download.stdout.log"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-e2b --models-root "$DOWNLOAD_ROOT/noauth" --auth never --audit > "$ROOT/download-auth-never.out"
grep 'stage: account-provider skipped' "$ROOT/download-auth-never.out"
grep 'status: model-download-pass' "$ROOT/download-auth-never.out"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download qwen3-8b --models-root "$DOWNLOAD_ROOT" --auth auto --audit > "$ROOT/download-qwen.out"
grep 'family: qwen' "$ROOT/download-qwen.out"
grep 'hf/qwen/qwen3-8b' "$ROOT/download-qwen.out"
grep 'status: model-download-pass' "$ROOT/download-qwen.out"

QWEN32_STATUS_ROOT="$ROOT/download-qwen32-status"
"$YVEX_BIN" models download status qwen3-32b --models-root "$QWEN32_STATUS_ROOT" --audit > "$ROOT/download-qwen32-status.out"
grep 'target_id: qwen3-32b' "$ROOT/download-qwen32-status.out"
grep 'family: qwen' "$ROOT/download-qwen32-status.out"
grep 'repo_id: Qwen/Qwen3-32B' "$ROOT/download-qwen32-status.out"
grep 'hf/qwen/qwen3-32b' "$ROOT/download-qwen32-status.out"
grep 'status: model-download-status' "$ROOT/download-qwen32-status.out"

YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$DOWNLOAD_ROOT/required" --auth required --audit > "$ROOT/download-auth-required.out" 2> "$ROOT/download-auth-required.err" && exit 1 || true
grep 'stage: account-provider blocked' "$ROOT/download-auth-required.out"
grep 'top_blocker: provider-login-required' "$ROOT/download-auth-required.out"

YVEX_HF_CLI=/missing/hf "$YVEX_BIN" models download gemma-4-12b-it --models-root "$DOWNLOAD_ROOT/missing" --auth auto --audit > "$ROOT/download-missing-hf.out" 2> "$ROOT/download-missing-hf.err" && exit 1 || true
grep 'status: model-download-blocked' "$ROOT/download-missing-hf.out"
grep 'top_blocker: missing-huggingface-cli' "$ROOT/download-missing-hf.out"
grep 'stage: account-provider blocked' "$ROOT/download-missing-hf.out"

YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_FAIL=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-12b-it --models-root "$DOWNLOAD_ROOT/fail" --auth auto --audit > "$ROOT/download-fail.out" 2> "$ROOT/download-fail.err" && exit 1 || true
grep 'status: model-download-fail' "$ROOT/download-fail.out"
test -f "$DOWNLOAD_ROOT/fail/logs/gemma-4-12b-it.download.stderr.log"

YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download --repo test-org/test-model --family gemma --name test-model --models-root "$DOWNLOAD_ROOT/direct" --auth auto --audit > "$ROOT/download-direct.out"
grep 'repo_id: test-org/test-model' "$ROOT/download-direct.out"
grep 'hf/gemma/test-model' "$ROOT/download-direct.out"
grep 'status: model-download-pass' "$ROOT/download-direct.out"

YVEX_FAKE_GH_AUTH=1 YVEX_GH_CLI="$FAKE_GH" "$YVEX_BIN" models download --provider github --repo test-org/test-model --release v1 --asset '*.gguf' --models-root "$DOWNLOAD_ROOT/github" --auth auto --audit > "$ROOT/download-github.out"
grep 'provider: github' "$ROOT/download-github.out"
grep 'stage: account-provider pass' "$ROOT/download-github.out"
grep 'stage: download pass' "$ROOT/download-github.out"
grep 'github/test-org/test-model/v1' "$ROOT/download-github.out"
grep 'gguf_created: false' "$ROOT/download-github.out"
grep 'generation: unsupported' "$ROOT/download-github.out"
test -f "$DOWNLOAD_ROOT/github/github/test-org/test-model/v1/fake-model.gguf"

"$YVEX_BIN" models download --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-missing-target.out" 2> "$ROOT/download-missing-target.err" && exit 1 || true
grep 'requires TARGET or --repo' "$ROOT/download-missing-target.err"
"$YVEX_BIN" models download --repo test-org/test-model --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-repo-no-family.out" 2> "$ROOT/download-repo-no-family.err" && exit 1 || true
grep 'requires --family' "$ROOT/download-repo-no-family.err"
"$YVEX_BIN" models download --repo test-org/test-model --family llama --models-root "$DOWNLOAD_ROOT/parser" > "$ROOT/download-bad-family.out" 2> "$ROOT/download-bad-family.err" && exit 1 || true
grep 'requires --family deepseek|glm|qwen|gemma' "$ROOT/download-bad-family.err"
"$YVEX_BIN" models download gemma-4-12b-it --models-root "" > "$ROOT/download-empty-root.out" 2> "$ROOT/download-empty-root.err" && exit 1 || true
grep 'models download --models-root value is empty or invalid' "$ROOT/download-empty-root.err"
"$YVEX_BIN" models download gemma-4-12b-it --max-workers 0 > "$ROOT/download-bad-workers.out" 2> "$ROOT/download-bad-workers.err" && exit 1 || true
grep 'requires a positive integer' "$ROOT/download-bad-workers.err"
"$YVEX_BIN" models download gemma-4-12b-it --auth maybe > "$ROOT/download-bad-auth.out" 2> "$ROOT/download-bad-auth.err" && exit 1 || true
grep 'auth requires auto|required|never' "$ROOT/download-bad-auth.err"
"$YVEX_BIN" models download gemma-4-12b-it --progress nope > "$ROOT/download-bad-progress.out" 2> "$ROOT/download-bad-progress.err" && exit 1 || true
grep 'requires auto|live|plain|log|off' "$ROOT/download-bad-progress.err"
"$YVEX_BIN" models download gemma-4-12b-it --tick-seconds 0 > "$ROOT/download-bad-tick.out" 2> "$ROOT/download-bad-tick.err" && exit 1 || true
grep 'tick-seconds requires a positive integer' "$ROOT/download-bad-tick.err"
"$YVEX_BIN" models download gemma-4-12b-it --source s3 > "$ROOT/download-bad-source.out" 2> "$ROOT/download-bad-source.err" && exit 1 || true
grep 'supports hf only' "$ROOT/download-bad-source.err"
"$YVEX_BIN" models download gemma-4-12b-it --auth nope > "$ROOT/download-bad-auth.out" 2> "$ROOT/download-bad-auth.err" && exit 1 || true
grep 'requires auto|required|never' "$ROOT/download-bad-auth.err"
"$YVEX_BIN" models download --provider github --repo test-org/test-model > "$ROOT/download-github-no-asset.out" 2> "$ROOT/download-github-no-asset.err" && exit 1 || true
grep 'requires --asset GLOB' "$ROOT/download-github-no-asset.err"
"$YVEX_BIN" models download gemma-4-12b-it --surprise > "$ROOT/download-unknown-flag.out" 2> "$ROOT/download-unknown-flag.err" && exit 1 || true
grep 'unknown models download option' "$ROOT/download-unknown-flag.err"
"$YVEX_BIN" models download gemma-4-12b-it extra > "$ROOT/download-extra-positional.out" 2> "$ROOT/download-extra-positional.err" && exit 1 || true
grep 'extra positional argument' "$ROOT/download-extra-positional.err"

HF_TOKEN=secret-value YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" models download gemma-4-e2b --models-root "$DOWNLOAD_ROOT/token" --auth auto --audit > "$ROOT/download-token.out"
grep 'auth_state: env-token-present' "$ROOT/download-token.out"
grep 'token_value_redacted: true' "$ROOT/download-token.out"
! grep -R 'secret-value' "$ROOT/download-token.out" "$DOWNLOAD_ROOT/token" "$ROOT"
! git ls-files '*.safetensors' '*.bin' '*.dat' | grep .

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
grep 'model-target class-profile TARGET' "$ROOT/model-target-help.out"
grep 'model-target tensor-collection TARGET' "$ROOT/model-target-help.out"
grep 'model-target tensor-map TARGET' "$ROOT/model-target-help.out"
grep 'This command records the v0.1.0 target decision' "$ROOT/model-target-help.out"

CLASS_MISSING_ROOT="$ROOT/qwen-class-missing-root"
"$YVEX_BIN" model-target class-profile qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/model-class-qwen-missing.out"
grep 'model-class: qwen' "$ROOT/model-class-qwen-missing.out"
grep 'target: qwen3-8b' "$ROOT/model-class-qwen-missing.out"
grep 'status: source-missing' "$ROOT/model-class-qwen-missing.out"
grep 'class: qwen-source-model-class-profile' "$ROOT/model-class-qwen-missing.out"
grep 'evidence: header-metadata-only' "$ROOT/model-class-qwen-missing.out"
grep 'patterns: tensors=0 attn=0 mlp=0 norm=0 head=0 moe=0' "$ROOT/model-class-qwen-missing.out"
grep 'top_blocker: missing-qwen-source-path' "$ROOT/model-class-qwen-missing.out"
grep 'next: V010.MAP.1' "$ROOT/model-class-qwen-missing.out"
grep 'no tensor role mapping/runtime/generation' "$ROOT/model-class-qwen-missing.out"

"$YVEX_BIN" model-target class-profile qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/model-class-qwen-missing-audit.out"
grep 'model_class_profile_status: source-missing' "$ROOT/model-class-qwen-missing-audit.out"
grep 'model_class_source_metadata_status: missing' "$ROOT/model-class-qwen-missing-audit.out"
grep 'model_class_tensor_count: 0' "$ROOT/model-class-qwen-missing-audit.out"
grep 'model_class_pattern_status: lexical-only' "$ROOT/model-class-qwen-missing-audit.out"
grep 'model_class_role_mapping_status: not-implemented' "$ROOT/model-class-qwen-missing-audit.out"
grep 'backend_selection: deferred' "$ROOT/model-class-qwen-missing-audit.out"
grep 'backend_pressure: metal-planned' "$ROOT/model-class-qwen-missing-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/model-class-qwen-missing-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/model-class-qwen-missing-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/model-class-qwen-missing-audit.out"
grep 'release_ready: false' "$ROOT/model-class-qwen-missing-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/model-class-qwen-missing-audit.out"

"$YVEX_BIN" model-target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-collection-qwen-missing.out"
grep 'tensor-collection: qwen' "$ROOT/tensor-collection-qwen-missing.out"
grep 'target: qwen3-8b' "$ROOT/tensor-collection-qwen-missing.out"
grep 'status: source-missing' "$ROOT/tensor-collection-qwen-missing.out"
grep 'stage: header-collection-inventory' "$ROOT/tensor-collection-qwen-missing.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-collection-qwen-missing.out"
grep 'collections: embedding=0 attention_qkvo=0 mlp_gud=0 norm=0 head=0 moe=0' "$ROOT/tensor-collection-qwen-missing.out"
grep 'top_blocker: missing-qwen-source-path' "$ROOT/tensor-collection-qwen-missing.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-collection-qwen-missing.out"
grep 'boundary: tensor collection inventory only; no role mapping/runtime/generation' "$ROOT/tensor-collection-qwen-missing.out"

"$YVEX_BIN" model-target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-collection-qwen-missing-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-qwen-missing-table.out"
matches "$ROOT/tensor-collection-qwen-missing-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target tensor-collection qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_status: source-missing' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_family: qwen' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_target_id: qwen3-8b' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_stage: header-collection-inventory' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_evidence_basis: header-metadata-only' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_source_status: missing' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_tensor_count: 0' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_embedding_tensor_count: 0' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_attention_complete_qkvo_layer_count: 0' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_mlp_complete_gud_layer_count: 0' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'tensor_collection_role_mapping_status: not-implemented' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'release_ready: false' "$ROOT/tensor-collection-qwen-missing-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-collection-qwen-missing-audit.out"

"$YVEX_BIN" model-target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-map-qwen-missing.out"
grep 'tensor-map: qwen' "$ROOT/tensor-map-qwen-missing.out"
grep 'target: qwen3-8b' "$ROOT/tensor-map-qwen-missing.out"
grep 'status: source-missing' "$ROOT/tensor-map-qwen-missing.out"
grep 'stage: header-naming-map' "$ROOT/tensor-map-qwen-missing.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-map-qwen-missing.out"
grep 'mapped: total=0 embedding=0 attention=0 mlp=0 norm=0 head=0 moe=0 unknown=0' "$ROOT/tensor-map-qwen-missing.out"
grep 'top_blocker: missing-qwen-source-path' "$ROOT/tensor-map-qwen-missing.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-map-qwen-missing.out"
grep 'boundary: tensor naming map only; no runtime descriptor/graph/runtime/generation' "$ROOT/tensor-map-qwen-missing.out"

"$YVEX_BIN" model-target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-map-qwen-missing-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-qwen-missing-table.out"
matches "$ROOT/tensor-map-qwen-missing-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010.MAP.1$'

"$YVEX_BIN" model-target tensor-map qwen3-8b --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_status: source-missing' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_family: qwen' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_target_id: qwen3-8b' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_stage: header-naming-map' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_evidence_basis: header-metadata-only' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_source_status: missing' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_mapped_total_count: 0' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_unmapped_unknown_count: 0' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_runtime_role_coverage_status: not-complete' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_artifact_contract_status: not-implemented' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_runtime_descriptor_status: not-implemented' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'tensor_map_graph_consumer_status: not-implemented' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'release_ready: false' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'top_blocker: missing-qwen-source-path' "$ROOT/tensor-map-qwen-missing-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-map-qwen-missing-audit.out"

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" > "$ROOT/model-class-gemma-missing.out"
grep 'model-class: gemma' "$ROOT/model-class-gemma-missing.out"
grep 'target: gemma-4-12b-it' "$ROOT/model-class-gemma-missing.out"
grep 'status: source-missing' "$ROOT/model-class-gemma-missing.out"
grep 'class: gemma-source-model-class-profile' "$ROOT/model-class-gemma-missing.out"
grep 'evidence: header-metadata-only' "$ROOT/model-class-gemma-missing.out"
grep 'patterns: tensors=0 attn=0 mlp=0 norm=0 head=0 moe=0' "$ROOT/model-class-gemma-missing.out"
grep 'top_blocker: missing-gemma-source-path' "$ROOT/model-class-gemma-missing.out"
grep 'next: V010.MAP.1' "$ROOT/model-class-gemma-missing.out"
grep 'no tensor role mapping/runtime/generation' "$ROOT/model-class-gemma-missing.out"

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/model-class-gemma-missing-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-gemma-missing-table.out"
matches "$ROOT/model-class-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_profile_status: source-missing' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_family: gemma' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_target_id: gemma-4-12b-it' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_name: gemma-source-model-class-profile' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_runtime_shape: dense-causal-decoder-candidate-pending-config' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_source_metadata_status: missing' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_tensor_count: 0' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_pattern_status: lexical-only' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_role_mapping_status: not-implemented' "$ROOT/model-class-gemma-missing-audit.out"
grep 'model_class_runtime_status: unsupported' "$ROOT/model-class-gemma-missing-audit.out"
grep 'backend_selection: deferred' "$ROOT/model-class-gemma-missing-audit.out"
grep 'backend_pressure: cpu-cuda-baseline-planned' "$ROOT/model-class-gemma-missing-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/model-class-gemma-missing-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/model-class-gemma-missing-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/model-class-gemma-missing-audit.out"
grep 'release_ready: false' "$ROOT/model-class-gemma-missing-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/model-class-gemma-missing-audit.out"

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" > "$ROOT/tensor-collection-gemma-missing.out"
grep 'tensor-collection: gemma' "$ROOT/tensor-collection-gemma-missing.out"
grep 'target: gemma-4-12b-it' "$ROOT/tensor-collection-gemma-missing.out"
grep 'status: source-missing' "$ROOT/tensor-collection-gemma-missing.out"
grep 'stage: header-collection-inventory' "$ROOT/tensor-collection-gemma-missing.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-collection-gemma-missing.out"
grep 'collections: embedding=0 attention_qkvo=0 mlp_gud=0 norm=0 head=0 moe=0' "$ROOT/tensor-collection-gemma-missing.out"
grep 'top_blocker: missing-gemma-source-path' "$ROOT/tensor-collection-gemma-missing.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-collection-gemma-missing.out"
grep 'boundary: tensor collection inventory only; no role mapping/runtime/generation' "$ROOT/tensor-collection-gemma-missing.out"

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --output table > "$ROOT/tensor-collection-gemma-missing-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-gemma-missing-table.out"
matches "$ROOT/tensor-collection-gemma-missing-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}source-missing[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --models-root "$CLASS_MISSING_ROOT" --audit > "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_status: source-missing' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_family: gemma' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_target_id: gemma-4-12b-it' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_stage: header-collection-inventory' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_evidence_basis: header-metadata-only' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_source_status: missing' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_tensor_count: 0' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_embedding_tensor_count: 0' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_attention_complete_qkvo_layer_count: 0' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_mlp_complete_gud_layer_count: 0' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'tensor_collection_role_mapping_status: not-implemented' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'release_ready: false' "$ROOT/tensor-collection-gemma-missing-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-collection-gemma-missing-audit.out"

QWEN_CLASS_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-class-profile-test-$$"
rm -rf "$QWEN_CLASS_SOURCE"
mkdir -p "$QWEN_CLASS_SOURCE"
printf '{}\n' > "$QWEN_CLASS_SOURCE/config.json"
printf '{}\n' > "$QWEN_CLASS_SOURCE/tokenizer.json"
python3 - "$QWEN_CLASS_SOURCE/model-00001-of-00001.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" model-target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" > "$ROOT/model-class-qwen.out"
grep 'status: metadata-profiled' "$ROOT/model-class-qwen.out"
grep 'patterns: tensors=10 attn=4 mlp=3 norm=2 head=1 moe=0' "$ROOT/model-class-qwen.out"
grep 'top_blocker: missing-qwen-tensor-role-map' "$ROOT/model-class-qwen.out"
grep 'next: V010.MAP.1' "$ROOT/model-class-qwen.out"

"$YVEX_BIN" model-target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" --output table > "$ROOT/model-class-qwen-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-qwen-table.out"
matches "$ROOT/model-class-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}metadata-profiled[[:space:]]{2,}10[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}2[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target class-profile qwen3-8b --source "$QWEN_CLASS_SOURCE" --audit > "$ROOT/model-class-qwen-audit.out"
grep 'model_class_profile_status: metadata-profiled' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_config_status: present' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_tokenizer_status: present' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_source_metadata_status: header-only' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_tensor_count: 10' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_embedding_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_attention_q_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_attention_k_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_attention_v_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_attention_o_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_mlp_gate_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_mlp_up_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_mlp_down_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_norm_pattern_count: 2' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_output_head_pattern_count: 1' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_moe_router_pattern_count: 0' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_moe_expert_pattern_count: 0' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_other_pattern_count: 0' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_pattern_status: lexical-only' "$ROOT/model-class-qwen-audit.out"
grep 'model_class_role_mapping_status: not-implemented' "$ROOT/model-class-qwen-audit.out"
grep 'backend_selection: deferred' "$ROOT/model-class-qwen-audit.out"
grep 'backend_pressure: metal-planned' "$ROOT/model-class-qwen-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/model-class-qwen-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/model-class-qwen-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/model-class-qwen-audit.out"
grep 'release_ready: false' "$ROOT/model-class-qwen-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/model-class-qwen-audit.out"

QWEN_CLASS_MODELS_ROOT="$ROOT/qwen-class-models-root"
mkdir -p "$QWEN_CLASS_MODELS_ROOT/hf/qwen"
cp -R "$QWEN_CLASS_SOURCE" "$QWEN_CLASS_MODELS_ROOT/hf/qwen/qwen3-8b"
"$YVEX_BIN" model-target class-profile qwen3-8b --models-root "$QWEN_CLASS_MODELS_ROOT" --audit > "$ROOT/model-class-qwen-models-root-audit.out"
grep 'model_class_profile_status: metadata-profiled' "$ROOT/model-class-qwen-models-root-audit.out"
matches "$ROOT/model-class-qwen-models-root-audit.out" 'source_path: .*/qwen-class-models-root/hf/qwen/qwen3-8b$'
grep 'model_class_source_metadata_status: header-only' "$ROOT/model-class-qwen-models-root-audit.out"

QWEN_COLLECTION_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-tensor-collection-test-$$"
rm -rf "$QWEN_COLLECTION_SOURCE"
mkdir -p "$QWEN_COLLECTION_SOURCE"
printf '{}\n' > "$QWEN_COLLECTION_SOURCE/config.json"
printf '{}\n' > "$QWEN_COLLECTION_SOURCE/tokenizer.json"
python3 - "$QWEN_COLLECTION_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" model-target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" > "$ROOT/tensor-collection-qwen.out"
grep 'tensor-collection: qwen' "$ROOT/tensor-collection-qwen.out"
grep 'target: qwen3-8b' "$ROOT/tensor-collection-qwen.out"
grep 'status: collection-profiled' "$ROOT/tensor-collection-qwen.out"
grep 'stage: header-collection-inventory' "$ROOT/tensor-collection-qwen.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-collection-qwen.out"
grep 'collections: embedding=1 attention_qkvo=1 mlp_gud=1 norm=3 head=1 moe=0' "$ROOT/tensor-collection-qwen.out"
grep 'layers_observed: 1' "$ROOT/tensor-collection-qwen.out"
grep 'top_blocker: missing-qwen-tensor-role-map' "$ROOT/tensor-collection-qwen.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-collection-qwen.out"
grep 'boundary: tensor collection inventory only; no role mapping/runtime/generation' "$ROOT/tensor-collection-qwen.out"

"$YVEX_BIN" model-target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --output table > "$ROOT/tensor-collection-qwen-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-qwen-table.out"
matches "$ROOT/tensor-collection-qwen-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN_QKVO[[:space:]]{2,}MLP_GUD[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-collection-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}collection-profiled[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target tensor-collection qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --audit > "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_status: collection-profiled' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_family: qwen' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_target_id: qwen3-8b' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_stage: header-collection-inventory' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_evidence_basis: header-metadata-only' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_source_status: present' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_manifest_status: not-checked' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_config_status: present' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_tokenizer_status: present' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_tensor_count: 12' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_layer_count_observed: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_embedding_status: candidate' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_embedding_tensor_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_status: candidate' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_q_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_k_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_v_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_o_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_attention_complete_qkvo_layer_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_mlp_status: candidate' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_mlp_gate_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_mlp_up_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_mlp_down_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_mlp_complete_gud_layer_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_norm_status: candidate' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_norm_tensor_count: 3' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_output_head_status: candidate' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_output_head_tensor_count: 1' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_moe_status: not-observed' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_moe_router_count: 0' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_moe_expert_count: 0' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_tokenizer_collection_status: sidecar-observed' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_validation_status: lexical-and-header-only' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_role_mapping_status: not-implemented' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_runtime_descriptor_status: not-implemented' "$ROOT/tensor-collection-qwen-audit.out"
grep 'tensor_collection_graph_consumer_status: not-implemented' "$ROOT/tensor-collection-qwen-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-collection-qwen-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-collection-qwen-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-collection-qwen-audit.out"
grep 'release_ready: false' "$ROOT/tensor-collection-qwen-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-collection-qwen-audit.out"
! grep 'generation_ready: tr''ue' "$ROOT/tensor-collection-qwen-audit.out"

"$YVEX_BIN" model-target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" > "$ROOT/tensor-map-qwen.out"
grep 'tensor-map: qwen' "$ROOT/tensor-map-qwen.out"
grep 'target: qwen3-8b' "$ROOT/tensor-map-qwen.out"
grep 'status: naming-map-profiled' "$ROOT/tensor-map-qwen.out"
grep 'stage: header-naming-map' "$ROOT/tensor-map-qwen.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-map-qwen.out"
grep 'mapped: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=0' "$ROOT/tensor-map-qwen.out"
grep 'layers_observed: 1' "$ROOT/tensor-map-qwen.out"
grep 'top_blocker: missing-qwen-runtime-role-validation' "$ROOT/tensor-map-qwen.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-map-qwen.out"
grep 'boundary: tensor naming map only; no runtime descriptor/graph/runtime/generation' "$ROOT/tensor-map-qwen.out"

"$YVEX_BIN" model-target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --output table > "$ROOT/tensor-map-qwen-table.out"
grep 'TENSOR NAMING MAP' "$ROOT/tensor-map-qwen-table.out"
matches "$ROOT/tensor-map-qwen-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}TOTAL[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN[[:space:]]{2,}MLP[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}UNKNOWN[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-map-qwen-table.out" '^qwen[[:space:]]{2,}qwen3-8b[[:space:]]{2,}naming-map-profiled[[:space:]]{2,}12[[:space:]]{2,}1[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010.MAP.1$'

"$YVEX_BIN" model-target tensor-map qwen3-8b --source "$QWEN_COLLECTION_SOURCE" --audit > "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_status: naming-map-profiled' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_family: qwen' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_target_id: qwen3-8b' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_stage: header-naming-map' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_evidence_basis: header-metadata-only' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_source_status: present' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_config_status: present' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_tokenizer_status: present' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_tensor_count: 12' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_mapped_total_count: 12' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_unmapped_unknown_count: 0' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_ambiguous_count: 0' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_layer_count_observed: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_embedding_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_attention_count: 4' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_attention_q_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_attention_k_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_attention_v_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_attention_o_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_mlp_count: 3' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_mlp_gate_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_mlp_up_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_mlp_down_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_norm_count: 3' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_output_head_count: 1' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_moe_router_count: 0' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_moe_expert_count: 0' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_validation_status: lexical-and-header-only' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_canonical_role_status: mapped-candidates' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_runtime_role_coverage_status: not-complete' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_artifact_contract_status: not-implemented' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_runtime_descriptor_status: not-implemented' "$ROOT/tensor-map-qwen-audit.out"
grep 'tensor_map_graph_consumer_status: not-implemented' "$ROOT/tensor-map-qwen-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-map-qwen-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-map-qwen-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-map-qwen-audit.out"
grep 'release_ready: false' "$ROOT/tensor-map-qwen-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.embed_tokens.weight -> model.embedding.token.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'model.layers.0.input_layernorm.weight -> model.layers.0.attention.norm.weight' "$ROOT/tensor-map-qwen-audit.out"
grep 'lm_head.weight -> model.output_head.weight' "$ROOT/tensor-map-qwen-audit.out"
! grep 'generation_ready: tr''ue' "$ROOT/tensor-map-qwen-audit.out"

QWEN_TENSOR_MAP_UNKNOWN_SOURCE="${TMPDIR:-/tmp}/yvex-qwen-tensor-map-unknown-test-$$"
rm -rf "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
mkdir -p "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
printf '{}\n' > "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/config.json"
printf '{}\n' > "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/tokenizer.json"
python3 - "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
    "model.layers.0.weird_unknown.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" model-target tensor-map qwen3-8b --source "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE" > "$ROOT/tensor-map-qwen-unknown.out"
grep 'status: naming-map-candidate' "$ROOT/tensor-map-qwen-unknown.out"
grep 'mapped: total=12 embedding=1 attention=4 mlp=3 norm=3 head=1 moe=0 unknown=1' "$ROOT/tensor-map-qwen-unknown.out"
"$YVEX_BIN" model-target tensor-map qwen3-8b --source "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE" --audit > "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'tensor_map_status: naming-map-candidate' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'tensor_map_tensor_count: 13' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'tensor_map_mapped_total_count: 12' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'tensor_map_unmapped_unknown_count: 1' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'tensor_map.entry.' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'model.layers.0.weird_unknown.weight' "$ROOT/tensor-map-qwen-unknown-audit.out"
grep 'mapping_status: unmapped-unknown' "$ROOT/tensor-map-qwen-unknown-audit.out"
rm -rf "$QWEN_TENSOR_MAP_UNKNOWN_SOURCE"
rm -rf "$QWEN_COLLECTION_SOURCE"

BAD_RUNTIME_CLAIM='runtime_claim: support''ed'
BAD_GENERATION_READY='generation_ready: tr''ue'
BAD_BENCHMARK_MEASURED='benchmark_status: measur''ed'
BAD_RELEASE_READY='release_ready: tr''ue'
! grep "$BAD_RUNTIME_CLAIM" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_GENERATION_READY" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_BENCHMARK_MEASURED" "$ROOT/model-class-qwen-audit.out"
! grep "$BAD_RELEASE_READY" "$ROOT/model-class-qwen-audit.out"
rm -rf "$QWEN_CLASS_SOURCE"

GEMMA_CLASS_SOURCE="${TMPDIR:-/tmp}/yvex-gemma-class-profile-test-$$"
rm -rf "$GEMMA_CLASS_SOURCE"
mkdir -p "$GEMMA_CLASS_SOURCE"
printf '{}\n' > "$GEMMA_CLASS_SOURCE/config.json"
printf '{}\n' > "$GEMMA_CLASS_SOURCE/tokenizer.json"
python3 - "$GEMMA_CLASS_SOURCE/model.safetensors" <<'PY'
import json
import struct
import sys

names = [
    "model.embed_tokens.weight",
    "model.layers.0.self_attn.q_proj.weight",
    "model.layers.0.self_attn.k_proj.weight",
    "model.layers.0.self_attn.v_proj.weight",
    "model.layers.0.self_attn.o_proj.weight",
    "model.layers.0.mlp.gate_proj.weight",
    "model.layers.0.mlp.up_proj.weight",
    "model.layers.0.mlp.down_proj.weight",
    "model.layers.0.input_layernorm.weight",
    "model.layers.0.post_attention_layernorm.weight",
    "model.norm.weight",
    "lm_head.weight",
]
offset = 0
header = {}
for name in names:
    header[name] = {
        "dtype": "F32",
        "shape": [2, 2],
        "data_offsets": [offset, offset + 16],
    }
    offset += 16
blob = json.dumps(header, separators=(",", ":")).encode("utf-8")
with open(sys.argv[1], "wb") as f:
    f.write(struct.pack("<Q", len(blob)))
    f.write(blob)
    f.write(b"x" * offset)
PY

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" > "$ROOT/model-class-gemma.out"
grep 'status: metadata-profiled' "$ROOT/model-class-gemma.out"
grep 'class: gemma-source-model-class-profile' "$ROOT/model-class-gemma.out"
grep 'patterns: tensors=12 attn=4 mlp=3 norm=3 head=1 moe=0' "$ROOT/model-class-gemma.out"
grep 'top_blocker: missing-gemma-tensor-role-map' "$ROOT/model-class-gemma.out"
grep 'next: V010.MAP.1' "$ROOT/model-class-gemma.out"

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/model-class-gemma-table.out"
grep 'MODEL CLASS PROFILE' "$ROOT/model-class-gemma-table.out"
matches "$ROOT/model-class-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}metadata-profiled[[:space:]]{2,}12[[:space:]]{2,}4[[:space:]]{2,}3[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target class-profile gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/model-class-gemma-audit.out"
grep 'model_class_profile_status: metadata-profiled' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_family: gemma' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_target_id: gemma-4-12b-it' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_name: gemma-source-model-class-profile' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_runtime_shape: dense-causal-decoder-candidate-pending-config' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_evidence_basis: header-metadata-only' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_config_status: present' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_tokenizer_status: present' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_source_metadata_status: header-only' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_tensor_count: 12' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_embedding_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_attention_q_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_attention_k_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_attention_v_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_attention_o_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_mlp_gate_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_mlp_up_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_mlp_down_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_norm_pattern_count: 3' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_output_head_pattern_count: 1' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_moe_router_pattern_count: 0' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_moe_expert_pattern_count: 0' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_other_pattern_count: 0' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_pattern_status: lexical-only' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_role_mapping_status: not-implemented' "$ROOT/model-class-gemma-audit.out"
grep 'model_class_runtime_status: unsupported' "$ROOT/model-class-gemma-audit.out"
grep 'backend_selection: deferred' "$ROOT/model-class-gemma-audit.out"
grep 'backend_pressure: cpu-cuda-baseline-planned' "$ROOT/model-class-gemma-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/model-class-gemma-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/model-class-gemma-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/model-class-gemma-audit.out"
grep 'release_ready: false' "$ROOT/model-class-gemma-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/model-class-gemma-audit.out"

GEMMA_CLASS_MODELS_ROOT="$ROOT/gemma-class-models-root"
mkdir -p "$GEMMA_CLASS_MODELS_ROOT/hf/gemma"
cp -R "$GEMMA_CLASS_SOURCE" "$GEMMA_CLASS_MODELS_ROOT/hf/gemma/gemma-4-12b-it"
"$YVEX_BIN" model-target class-profile gemma-4-12b-it --models-root "$GEMMA_CLASS_MODELS_ROOT" --audit > "$ROOT/model-class-gemma-models-root-audit.out"
grep 'model_class_profile_status: metadata-profiled' "$ROOT/model-class-gemma-models-root-audit.out"
matches "$ROOT/model-class-gemma-models-root-audit.out" 'source_path: .*/gemma-class-models-root/hf/gemma/gemma-4-12b-it$'
grep 'model_class_source_metadata_status: header-only' "$ROOT/model-class-gemma-models-root-audit.out"

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" > "$ROOT/tensor-collection-gemma.out"
grep 'tensor-collection: gemma' "$ROOT/tensor-collection-gemma.out"
grep 'target: gemma-4-12b-it' "$ROOT/tensor-collection-gemma.out"
grep 'status: collection-profiled' "$ROOT/tensor-collection-gemma.out"
grep 'stage: header-collection-inventory' "$ROOT/tensor-collection-gemma.out"
grep 'evidence: header-metadata-only' "$ROOT/tensor-collection-gemma.out"
grep 'collections: embedding=1 attention_qkvo=1 mlp_gud=1 norm=3 head=1 moe=0' "$ROOT/tensor-collection-gemma.out"
grep 'layers_observed: 1' "$ROOT/tensor-collection-gemma.out"
grep 'top_blocker: missing-gemma-tensor-role-map' "$ROOT/tensor-collection-gemma.out"
grep 'next: V010.MAP.1' "$ROOT/tensor-collection-gemma.out"
grep 'boundary: tensor collection inventory only; no role mapping/runtime/generation' "$ROOT/tensor-collection-gemma.out"

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --output table > "$ROOT/tensor-collection-gemma-table.out"
grep 'TENSOR COLLECTION INVENTORY' "$ROOT/tensor-collection-gemma-table.out"
matches "$ROOT/tensor-collection-gemma-table.out" '^FAMILY[[:space:]]{2,}TARGET[[:space:]]{2,}STATUS[[:space:]]{2,}EMBED[[:space:]]{2,}ATTN_QKVO[[:space:]]{2,}MLP_GUD[[:space:]]{2,}NORM[[:space:]]{2,}HEAD[[:space:]]{2,}MOE[[:space:]]{2,}LAYERS[[:space:]]{2,}NEXT$'
matches "$ROOT/tensor-collection-gemma-table.out" '^gemma[[:space:]]{2,}gemma-4-12b-it[[:space:]]{2,}collection-profiled[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}1[[:space:]]{2,}3[[:space:]]{2,}1[[:space:]]{2,}0[[:space:]]{2,}1[[:space:]]{2,}V010\.MAP\.1$'

"$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --source "$GEMMA_CLASS_SOURCE" --audit > "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_status: collection-profiled' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_family: gemma' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_target_id: gemma-4-12b-it' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_stage: header-collection-inventory' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_evidence_basis: header-metadata-only' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_source_status: present' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_manifest_status: not-checked' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_config_status: present' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_tokenizer_status: present' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_tensor_count: 12' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_layer_count_observed: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_embedding_status: candidate' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_embedding_tensor_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_status: candidate' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_q_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_k_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_v_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_o_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_attention_complete_qkvo_layer_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_mlp_status: candidate' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_mlp_gate_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_mlp_up_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_mlp_down_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_mlp_complete_gud_layer_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_norm_status: candidate' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_norm_tensor_count: 3' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_output_head_status: candidate' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_output_head_tensor_count: 1' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_moe_status: not-observed' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_moe_router_count: 0' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_moe_expert_count: 0' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_tokenizer_collection_status: sidecar-observed' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_validation_status: lexical-and-header-only' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_role_mapping_status: not-implemented' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_runtime_descriptor_status: not-implemented' "$ROOT/tensor-collection-gemma-audit.out"
grep 'tensor_collection_graph_consumer_status: not-implemented' "$ROOT/tensor-collection-gemma-audit.out"
grep 'runtime_claim: unsupported' "$ROOT/tensor-collection-gemma-audit.out"
grep 'generation: unsupported-full-model' "$ROOT/tensor-collection-gemma-audit.out"
grep 'benchmark_status: not-measured' "$ROOT/tensor-collection-gemma-audit.out"
grep 'release_ready: false' "$ROOT/tensor-collection-gemma-audit.out"
grep 'next_required_rows: V010.MAP.1' "$ROOT/tensor-collection-gemma-audit.out"
! grep 'generation_ready: tr''ue' "$ROOT/tensor-collection-gemma-audit.out"

! grep "$BAD_RUNTIME_CLAIM" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_GENERATION_READY" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_BENCHMARK_MEASURED" "$ROOT/model-class-gemma-audit.out"
! grep "$BAD_RELEASE_READY" "$ROOT/model-class-gemma-audit.out"
rm -rf "$GEMMA_CLASS_SOURCE"

expect_rc 2 "$YVEX_BIN" model-target class-profile > "$ROOT/model-class-missing-target.out" 2> "$ROOT/model-class-missing-target.err"
grep 'requires TARGET' "$ROOT/model-class-missing-target.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile nope > "$ROOT/model-class-bad-target.out" 2> "$ROOT/model-class-bad-target.err"
grep 'unsupported target: nope' "$ROOT/model-class-bad-target.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile qwen-metal-portability > "$ROOT/model-class-old-target.out" 2> "$ROOT/model-class-old-target.err"
grep 'unsupported target: qwen-metal-portability' "$ROOT/model-class-old-target.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile gemma-dense-portability > "$ROOT/model-class-old-gemma-target.out" 2> "$ROOT/model-class-old-gemma-target.err"
grep 'unsupported target: gemma-dense-portability' "$ROOT/model-class-old-gemma-target.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile qwen3-8b --output nope > "$ROOT/model-class-bad-output.out" 2> "$ROOT/model-class-bad-output.err"
grep 'unsupported output mode: nope' "$ROOT/model-class-bad-output.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile qwen3-8b --source > "$ROOT/model-class-missing-source.out" 2> "$ROOT/model-class-missing-source.err"
grep 'source requires DIR' "$ROOT/model-class-missing-source.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile gemma-4-12b-it --output nope > "$ROOT/model-class-gemma-bad-output.out" 2> "$ROOT/model-class-gemma-bad-output.err"
grep 'unsupported output mode: nope' "$ROOT/model-class-gemma-bad-output.err"
expect_rc 2 "$YVEX_BIN" model-target class-profile gemma-4-12b-it --source > "$ROOT/model-class-gemma-missing-source.out" 2> "$ROOT/model-class-gemma-missing-source.err"
grep 'source requires DIR' "$ROOT/model-class-gemma-missing-source.err"

expect_rc 2 "$YVEX_BIN" model-target tensor-collection > "$ROOT/tensor-collection-missing-target.out" 2> "$ROOT/tensor-collection-missing-target.err"
grep 'requires TARGET' "$ROOT/tensor-collection-missing-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection nope > "$ROOT/tensor-collection-bad-target.out" 2> "$ROOT/tensor-collection-bad-target.err"
grep 'unsupported target: nope' "$ROOT/tensor-collection-bad-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection qwen-metal-portability > "$ROOT/tensor-collection-old-target.out" 2> "$ROOT/tensor-collection-old-target.err"
grep 'unsupported target: qwen-metal-portability' "$ROOT/tensor-collection-old-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection qwen3-8b --output nope > "$ROOT/tensor-collection-bad-output.out" 2> "$ROOT/tensor-collection-bad-output.err"
grep 'unsupported output mode: nope' "$ROOT/tensor-collection-bad-output.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection qwen3-8b --source > "$ROOT/tensor-collection-missing-source.out" 2> "$ROOT/tensor-collection-missing-source.err"
grep 'source requires DIR' "$ROOT/tensor-collection-missing-source.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection qwen3-8b --models-root > "$ROOT/tensor-collection-missing-models-root.out" 2> "$ROOT/tensor-collection-missing-models-root.err"
grep 'models-root requires DIR' "$ROOT/tensor-collection-missing-models-root.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection gemma-dense-portability > "$ROOT/tensor-collection-old-gemma-target.out" 2> "$ROOT/tensor-collection-old-gemma-target.err"
grep 'unsupported target: gemma-dense-portability' "$ROOT/tensor-collection-old-gemma-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --output nope > "$ROOT/tensor-collection-gemma-bad-output.out" 2> "$ROOT/tensor-collection-gemma-bad-output.err"
grep 'unsupported output mode: nope' "$ROOT/tensor-collection-gemma-bad-output.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-collection gemma-4-12b-it --source > "$ROOT/tensor-collection-gemma-missing-source.out" 2> "$ROOT/tensor-collection-gemma-missing-source.err"
grep 'source requires DIR' "$ROOT/tensor-collection-gemma-missing-source.err"

expect_rc 2 "$YVEX_BIN" model-target tensor-map > "$ROOT/tensor-map-missing-target.out" 2> "$ROOT/tensor-map-missing-target.err"
grep 'requires TARGET' "$ROOT/tensor-map-missing-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map nope > "$ROOT/tensor-map-bad-target.out" 2> "$ROOT/tensor-map-bad-target.err"
grep 'unsupported target: nope' "$ROOT/tensor-map-bad-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map qwen-metal-portability > "$ROOT/tensor-map-old-target.out" 2> "$ROOT/tensor-map-old-target.err"
grep 'unsupported target: qwen-metal-portability' "$ROOT/tensor-map-old-target.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map qwen3-8b --output nope > "$ROOT/tensor-map-bad-output.out" 2> "$ROOT/tensor-map-bad-output.err"
grep 'unsupported output mode: nope' "$ROOT/tensor-map-bad-output.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map qwen3-8b --source > "$ROOT/tensor-map-missing-source.out" 2> "$ROOT/tensor-map-missing-source.err"
grep 'source requires DIR' "$ROOT/tensor-map-missing-source.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map qwen3-8b --models-root > "$ROOT/tensor-map-missing-models-root.out" 2> "$ROOT/tensor-map-missing-models-root.err"
grep 'models-root requires DIR' "$ROOT/tensor-map-missing-models-root.err"
expect_rc 2 "$YVEX_BIN" model-target tensor-map gemma-4-12b-it > "$ROOT/tensor-map-gemma-target.out" 2> "$ROOT/tensor-map-gemma-target.err"
grep 'unsupported target: gemma-4-12b-it' "$ROOT/tensor-map-gemma-target.err"

"$YVEX_BIN" model-target decision --help > "$ROOT/model-target-decision-help.out"
grep 'usage: yvex model-target decision --release v0.1.0' "$ROOT/model-target-decision-help.out"
grep 'does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks' "$ROOT/model-target-decision-help.out"

"$YVEX_BIN" model-target decision --release v0.1.0 > "$ROOT/model-target-decision-normal.out"
grep 'report: target-decision' "$ROOT/model-target-decision-normal.out"
grep 'status: target-decision-blocked' "$ROOT/model-target-decision-normal.out"
grep 'selected: none' "$ROOT/model-target-decision-normal.out"
grep 'top_blocker: no eligible full-runtime candidate' "$ROOT/model-target-decision-normal.out"
grep 'next: V010.MAP.1' "$ROOT/model-target-decision-normal.out"
! grep 'next: V010\.CLI\.18' "$ROOT/model-target-decision-normal.out"
! grep 'next: V010\.SOURCE\.7' "$ROOT/model-target-decision-normal.out"
! grep 'next: MODEL\.CLASS\.QWEN\.0' "$ROOT/model-target-decision-normal.out"
grep 'boundary: report-only; generation unsupported; benchmark not measured' "$ROOT/model-target-decision-normal.out"

"$YVEX_BIN" model-target decision --release v0.1.0 --output table > "$ROOT/model-target-decision-table.out"
matches "$ROOT/model-target-decision-table.out" '^REPORT[[:space:]]{2,}STATUS[[:space:]]{2,}SELECTED[[:space:]]{2,}ELIGIBLE[[:space:]]{2,}NEXT$'
matches "$ROOT/model-target-decision-table.out" '^target-decision[[:space:]]{2,}blocked[[:space:]]{2,}none[[:space:]]{2,}0[[:space:]]{2,}V010\.MAP\.1$'

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
grep 'candidate.3.id: qwen3-8b' "$ROOT/model-target-decision.out"
grep 'candidate.3.class: source-model-candidate' "$ROOT/model-target-decision.out"
grep 'candidate.3.status: ineligible-source-model-candidate' "$ROOT/model-target-decision.out"
grep 'candidate.3.next: tensor role mapping' "$ROOT/model-target-decision.out"
grep 'candidate.4.id: gemma-4-12b-it' "$ROOT/model-target-decision.out"
grep 'candidate.4.class: source-model-candidate' "$ROOT/model-target-decision.out"
grep 'candidate.4.status: ineligible-source-model-candidate' "$ROOT/model-target-decision.out"
grep 'candidate.4.next: tensor role mapping' "$ROOT/model-target-decision.out"
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
