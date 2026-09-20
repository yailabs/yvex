#!/usr/bin/env bash
set -euo pipefail

. tests/support/cleanup.sh

YVEX_BIN=${YVEX_BIN:-"$PWD/yvex"}
FAKE_HF="$PWD/tests/fixtures/bin/fake-hf"
ROOT=$(mktemp -d /tmp/yvex-source-lifecycle-XXXXXX)
CLIENT_PIDS=()

cleanup() {
  local root pid
  for root in "$ROOT"/models-*; do
    test -d "$root" || continue
    "$YVEX_BIN" source stop gemma-4-12b-it --models-root "$root" \
      --output json >/dev/null 2>&1 || true
  done
  for pid in "${CLIENT_PIDS[@]:-}"; do
    kill "$pid" >/dev/null 2>&1 || true
    wait "$pid" >/dev/null 2>&1 || true
  done
  yvex_test_cleanup "$ROOT"
}
trap cleanup EXIT HUP INT TERM

status_json() {
  "$YVEX_BIN" source status gemma-4-12b-it --models-root "$1" --output json
}

json_field() {
  python3 -c 'import json,sys; value=json.load(sys.stdin); print(value[sys.argv[1]])' "$1"
}

wait_field() {
  local root=$1 field=$2 expected=$3 attempt value
  for attempt in $(seq 1 120); do
    value=$(status_json "$root" 2>/dev/null | json_field "$field" 2>/dev/null || true)
    test "$value" = "$expected" && return 0
    sleep 0.1
  done
  printf 'timed out waiting for %s=%s under %s (last=%s)\n' \
    "$field" "$expected" "$root" "${value:-missing}" >&2
  return 1
}

wait_nonzero_field() {
  local root=$1 field=$2 attempt value
  for attempt in $(seq 1 120); do
    value=$(status_json "$root" 2>/dev/null | json_field "$field" 2>/dev/null || true)
    case "$value" in
      ''|0|None) ;;
      *) return 0 ;;
    esac
    sleep 0.1
  done
  printf 'timed out waiting for nonzero %s under %s (last=%s)\n' \
    "$field" "$root" "${value:-missing}" >&2
  return 1
}

start_fake() {
  local root=$1 delay=$2 steps=$3 stall=${4:-5}
  shift 4 || true
  env YVEX_CONFIG_DIR="$ROOT/config" YVEX_FAKE_HF_AUTH=1 \
    YVEX_FAKE_HF_STEP_DELAY="$delay" YVEX_FAKE_HF_STEPS="$steps" \
    YVEX_HF_CLI="$FAKE_HF" "$@" \
    "$YVEX_BIN" source acquire gemma-4-12b-it --models-root "$root" \
    --auth required --progress log --tick-seconds 1 --stall-seconds "$stall" \
    --audit >"$root.client.out" 2>"$root.client.err" &
  START_PID=$!
  CLIENT_PIDS+=("$START_PID")
}

# Normal completion proves detached supervision, exact terminal truth, and source finalization.
NORMAL="$ROOT/models-normal"
start_fake "$NORMAL" 0 3 5 env
wait "$START_PID"
normal=$(status_json "$NORMAL")
test "$(printf '%s' "$normal" | json_field schema)" = yvex.model.acquisition.status.v2
test "$(printf '%s' "$normal" | json_field lifecycle)" = complete
test "$(printf '%s' "$normal" | json_field health)" = not-applicable
test "$(printf '%s' "$normal" | json_field active)" = False
test "$(printf '%s' "$normal" | json_field inflight_selected_bytes)" = None
test "$(printf '%s' "$normal" | json_field current_rate_bytes_per_second)" = None
test -f "$NORMAL/evidence/build/gemma/gemma-4-12b-it.source-manifest.json"

# A real PTY gets an in-place, width-bounded projection.  NO_COLOR keeps the
# same operation facts but selects append-only plain output even on a PTY.
PTY_ROOT="$ROOT/models-pty"
pty_command="stty cols 40; env YVEX_CONFIG_DIR=$ROOT/config YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=0 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI=$FAKE_HF $YVEX_BIN source acquire gemma-4-12b-it --models-root $PTY_ROOT --auth required --progress live --tick-seconds 1 --stall-seconds 5"
env -u NO_COLOR TERM=xterm-256color script -q -e -c "$pty_command" \
  "$ROOT/pty.typescript" </dev/null >/dev/null
LC_ALL=C grep -q $'\033\[2K' "$ROOT/pty.typescript"
if grep -q 'files' "$ROOT/pty.typescript"; then
  printf 'narrow PTY projection exceeded its reduced fact surface\n' >&2
  exit 1
fi

PLAIN_PTY_ROOT="$ROOT/models-plain-pty"
plain_pty_command="env YVEX_CONFIG_DIR=$ROOT/config YVEX_FAKE_HF_AUTH=1 YVEX_FAKE_HF_STEP_DELAY=0 YVEX_FAKE_HF_STEPS=3 YVEX_HF_CLI=$FAKE_HF $YVEX_BIN source acquire gemma-4-12b-it --models-root $PLAIN_PTY_ROOT --auth required --progress live --tick-seconds 1 --stall-seconds 5"
NO_COLOR=1 TERM=xterm-256color script -q -e -c "$plain_pty_command" \
  "$ROOT/plain-pty.typescript" </dev/null >/dev/null
if LC_ALL=C grep -q $'\033' "$ROOT/plain-pty.typescript"; then
  printf 'NO_COLOR PTY projection contains terminal escapes\n' >&2
  exit 1
fi

# Disconnecting the first client must not own or stop the authenticated operation.
DETACH="$ROOT/models-detach"
start_fake "$DETACH" 1 6 5 env
wait_field "$DETACH" active True
before=$(status_json "$DETACH")
operation=$(printf '%s' "$before" | json_field operation_id)
kill -TERM "$START_PID"
wait "$START_PID" >/dev/null 2>&1 || true
after=$(status_json "$DETACH")
test "$(printf '%s' "$after" | json_field operation_id)" = "$operation"
test "$(printf '%s' "$after" | json_field active)" = True
wait_field "$DETACH" lifecycle complete

# A slow but advancing provider remains healthy; committed and provider activity stay distinct.
SLOW="$ROOT/models-slow"
start_fake "$SLOW" 1 5 3 env
wait_field "$SLOW" health healthy
slow=$(status_json "$SLOW")
test "$(printf '%s' "$slow" | json_field provider_activity_bytes)" != None
test "$(printf '%s' "$slow" | json_field inflight_selected_bytes)" = None
wait "$START_PID"

# Process I/O is useful progress evidence, but is not misreported as a stable
# provider event when the adapter emits no structured event contract.
NO_EVENT="$ROOT/models-no-event"
start_fake "$NO_EVENT" 1 4 5 env YVEX_FAKE_HF_DISABLE_EVENTS=1
wait_nonzero_field "$NO_EVENT" provider_pid
no_event_initial=$(status_json "$NO_EVENT")
initial_event=$(printf '%s' "$no_event_initial" | json_field last_provider_event_unix)
test "$initial_event" != None
test "$(printf '%s' "$no_event_initial" | json_field provider_event_sequence)" = None
wait_field "$NO_EVENT" health healthy
no_event=$(status_json "$NO_EVENT")
test "$(printf '%s' "$no_event" | json_field provider_activity_bytes)" != None
test "$(printf '%s' "$no_event" | json_field last_progress_unix)" != None
test "$(printf '%s' "$no_event" | json_field last_provider_event_unix)" = "$initial_event"
wait "$START_PID"

# Cooperative machine events expose provider retry separately from a stall;
# human provider logs are never parsed into operation truth.
RETRY="$ROOT/models-retry"
start_fake "$RETRY" 1 4 5 env YVEX_FAKE_HF_RETRY_BEFORE=2
wait_field "$RETRY" lifecycle retrying
retry=$(status_json "$RETRY")
test "$(printf '%s' "$retry" | json_field health)" = degraded
test "$(printf '%s' "$retry" | json_field retry_count)" = 1
test "$(printf '%s' "$retry" | json_field current_object)" = model-00001-of-00002.safetensors
wait_field "$RETRY" lifecycle complete
wait "$START_PID"

# Provider-internal partial objects and cache activity remain separate from committed
# selected files and selected-domain in-flight bytes.  Default cache roots are YVEX-owned.
PARTIAL="$ROOT/models-partial"
start_fake "$PARTIAL" 1 4 5 env YVEX_FAKE_HF_PARTIAL_OBJECT=1 \
  YVEX_FAKE_HF_EXPECT_CACHE_ROOT="$PARTIAL" HF_XET_HIGH_PERFORMANCE=1 \
  YVEX_FAKE_HF_EXPECT_HIGH_PERFORMANCE=1
wait_field "$PARTIAL" provider_partial_objects 1
partial=$(status_json "$PARTIAL")
test "$(printf '%s' "$partial" | json_field completed_files)" = 4
test "$(printf '%s' "$partial" | json_field incomplete_files)" = None
test "$(printf '%s' "$partial" | json_field inflight_selected_bytes)" = None
test "$(printf '%s' "$partial" | json_field partial_files)" = 1
wait "$START_PID"
partial_done=$(status_json "$PARTIAL")
test "$(printf '%s' "$partial_done" | json_field provider_partial_objects)" = 0

# Advanced provider cache overrides remain explicit and are not replaced by
# the models-root defaults.
OVERRIDE="$ROOT/models-override"
override_hub="$ROOT/cache-override/hub"
override_xet="$ROOT/cache-override/xet"
start_fake "$OVERRIDE" 0 3 5 env HF_HUB_CACHE="$override_hub" \
  HF_XET_CACHE="$override_xet" YVEX_FAKE_HF_EXPECT_HUB_CACHE="$override_hub" \
  YVEX_FAKE_HF_EXPECT_XET_CACHE="$override_xet"
wait "$START_PID"
wait_field "$OVERRIDE" lifecycle complete

# An alive provider with no useful progress beyond the bounded window becomes stalled.
STALL="$ROOT/models-stall"
start_fake "$STALL" 4 4 1 env
wait_field "$STALL" health stalled
stall=$(status_json "$STALL")
test "$(printf '%s' "$stall" | json_field active)" = True
"$YVEX_BIN" source stop gemma-4-12b-it --models-root "$STALL" \
  --output json >/dev/null
wait_field "$STALL" lifecycle stopped
wait "$START_PID" >/dev/null 2>&1 || true

# Explicit stop/resume preserves the exact target and increments operation generation.
RESUME="$ROOT/models-resume"
start_fake "$RESUME" 2 5 5 env
wait_field "$RESUME" active True
resume_before=$(status_json "$RESUME")
generation=$(printf '%s' "$resume_before" | json_field generation)
"$YVEX_BIN" source stop gemma-4-12b-it --models-root "$RESUME" \
  --output json >/dev/null
wait_field "$RESUME" lifecycle stopped
wait "$START_PID" >/dev/null 2>&1 || true
env YVEX_CONFIG_DIR="$ROOT/config" YVEX_FAKE_HF_AUTH=1 \
  YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source resume gemma-4-12b-it \
  --models-root "$RESUME" --auth required --progress off --tick-seconds 1 \
  --stall-seconds 5 --audit >/dev/null
resume_after=$(status_json "$RESUME")
test "$(printf '%s' "$resume_after" | json_field lifecycle)" = complete
test "$(printf '%s' "$resume_after" | json_field generation)" = "$((generation + 1))"

# A crashed supervisor reconciles as interrupted/resumable and cannot be resumed over an
# authenticated orphan provider. Explicit stop is the only admitted takeover.
CRASH="$ROOT/models-crash"
start_fake "$CRASH" 10 4 5 env YVEX_FAKE_HF_ORPHAN_SURVIVE=1
wait_field "$CRASH" active True
wait_nonzero_field "$CRASH" provider_pid
crash=$(status_json "$CRASH")
supervisor=$(printf '%s' "$crash" | json_field supervisor_pid)
kill -KILL "$supervisor"
wait_field "$CRASH" lifecycle stopped
crash=$(status_json "$CRASH")
test "$(printf '%s' "$crash" | json_field reason)" = supervisor-identity-lost
if env YVEX_CONFIG_DIR="$ROOT/config" YVEX_FAKE_HF_AUTH=1 YVEX_HF_CLI="$FAKE_HF" \
    "$YVEX_BIN" source resume gemma-4-12b-it --models-root "$CRASH" \
    --auth required --progress off >/dev/null 2>"$ROOT/crash-resume.err"; then
  printf 'resume unexpectedly admitted an orphan provider\n' >&2
  exit 1
fi
grep -q 'orphan provider is still active' "$ROOT/crash-resume.err"
"$YVEX_BIN" source stop gemma-4-12b-it --models-root "$CRASH" \
  --output json >/dev/null
wait "$START_PID" >/dev/null 2>&1 || true

# Failed provider work is explicit and a later operator resume uses a new generation.
FAILED="$ROOT/models-failed"
start_fake "$FAILED" 0 3 5 env YVEX_FAKE_HF_FAIL_AT_STEP=2
wait "$START_PID" >/dev/null 2>&1 || true
wait_field "$FAILED" lifecycle failed
failed=$(status_json "$FAILED")
failed_generation=$(printf '%s' "$failed" | json_field generation)
env YVEX_CONFIG_DIR="$ROOT/config" YVEX_FAKE_HF_AUTH=1 \
  YVEX_HF_CLI="$FAKE_HF" "$YVEX_BIN" source resume gemma-4-12b-it \
  --models-root "$FAILED" --auth required --progress off --tick-seconds 1 \
  --stall-seconds 5 --audit >/dev/null
failed_after=$(status_json "$FAILED")
test "$(printf '%s' "$failed_after" | json_field lifecycle)" = complete
test "$(printf '%s' "$failed_after" | json_field generation)" = "$((failed_generation + 1))"

# JSON and NO_COLOR redirected projections are stable and ANSI-free.
printf '%s' "$normal" | python3 -m json.tool >/dev/null
if printf '%s' "$normal" | LC_ALL=C grep -q $'\033'; then
  printf 'JSON projection contains terminal escapes\n' >&2
  exit 1
fi
NO_COLOR=1 "$YVEX_BIN" source status gemma-4-12b-it --models-root "$NORMAL" \
  >"$ROOT/no-color.out"
if LC_ALL=C grep -q $'\033' "$ROOT/no-color.out"; then
  printf 'NO_COLOR projection contains terminal escapes\n' >&2
  exit 1
fi

printf 'source acquisition lifecycle: pass\n'
