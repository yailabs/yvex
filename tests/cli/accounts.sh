#!/usr/bin/env sh
set -eu

. tests/support/cleanup.sh

YVEX_BIN="${YVEX_BIN:-./yvex}"
ROOT=${YVEX_TEST_OUT_DIR:-build/tests/accounts-cli}
FAKE_HF="$PWD/tests/fixtures/bin/fake-hf"
FAKE_GH="$PWD/tests/fixtures/bin/fake-gh"

yvex_test_cleanup "$ROOT"
mkdir -p "$ROOT"

"$YVEX_BIN" source accounts --help > "$ROOT/help.out"
grep 'yvex source accounts providers' "$ROOT/help.out"
grep 'yvex source accounts status' "$ROOT/help.out"
grep 'yvex source accounts whoami' "$ROOT/help.out"
grep 'yvex source accounts login' "$ROOT/help.out"
grep 'yvex source accounts logout' "$ROOT/help.out"
grep 'yvex source accounts ensure' "$ROOT/help.out"
"$YVEX_BIN" source accounts login --help > "$ROOT/login-help.out"
python3 tests/support/human_field.py "$ROOT/login-help.out" "operation: provider.account.login"
grep -- '--force' "$ROOT/login-help.out"
grep '<PROVIDER>' "$ROOT/login-help.out"
grep -- '--add-to-git-credential' "$ROOT/login-help.out"
! grep -- '--token-stdin' "$ROOT/login-help.out"

YVEX_CONFIG_DIR="$ROOT/missing-config" \
YVEX_HF_CLI=/missing/hf \
YVEX_GH_CLI=/missing/gh \
  "$YVEX_BIN" source accounts status --output audit > "$ROOT/status-missing.out"
python3 tests/support/human_field.py "$ROOT/status-missing.out" 'blocker: missing-huggingface-cli'
python3 tests/support/human_field.py "$ROOT/status-missing.out" 'blocker: missing-github-cli'
python3 tests/support/human_field.py "$ROOT/status-missing.out" 'raw_token_stored_by_yvex: false'

YVEX_CONFIG_DIR="$ROOT/providers-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_GH_CLI="$FAKE_GH" \
  "$YVEX_BIN" source accounts providers --output table > "$ROOT/providers-table.out"
grep 'huggingface' "$ROOT/providers-table.out"
grep 'github' "$ROOT/providers-table.out"
grep 'PROVIDER' "$ROOT/providers-table.out"

YVEX_CONFIG_DIR="$ROOT/providers-plain-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_GH_CLI="$FAKE_GH" \
  "$YVEX_BIN" source accounts providers > "$ROOT/providers-plain.out"
grep 'Authentication is delegated to provider tooling' "$ROOT/providers-plain.out"
! grep 'no login' "$ROOT/providers-plain.out"

YVEX_CONFIG_DIR="$ROOT/hf-whoami-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_FAKE_HF_AUTH=1 \
  "$YVEX_BIN" source accounts whoami huggingface --output audit > "$ROOT/hf-whoami.out"
python3 tests/support/human_field.py "$ROOT/hf-whoami.out" 'provider: huggingface'
python3 tests/support/human_field.py "$ROOT/hf-whoami.out" 'auth_state: logged-in'
python3 tests/support/human_field.py "$ROOT/hf-whoami.out" 'status: account-whoami-pass'

YVEX_CONFIG_DIR="$ROOT/hf-login-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_FAKE_HF_STATE="$ROOT/hf-login.state" \
YVEX_FAKE_HF_LOGIN_OK=1 \
  "$YVEX_BIN" source accounts login huggingface --output audit > "$ROOT/hf-login.out"
python3 tests/support/human_field.py "$ROOT/hf-login.out" 'status: account-login-pass'
test -f "$ROOT/hf-login-config/accounts.local.json"
grep 'token_value_redacted' "$ROOT/hf-login-config/accounts.local.json"
grep 'raw_token_stored_by_yvex": false' "$ROOT/hf-login-config/accounts.local.json"

YVEX_CONFIG_DIR="$ROOT/hf-ensure-config" \
YVEX_HF_CLI="$FAKE_HF" \
  "$YVEX_BIN" source accounts ensure huggingface --interactive never --output audit > "$ROOT/hf-ensure.out" 2> "$ROOT/hf-ensure.err" && exit 1 || true
python3 tests/support/human_field.py "$ROOT/hf-ensure.out" 'status: account-ensure-blocked'
python3 tests/support/human_field.py "$ROOT/hf-ensure.out" 'blocker: provider-login-required'

YVEX_CONFIG_DIR="$ROOT/gh-login-config" \
YVEX_GH_CLI="$FAKE_GH" \
YVEX_FAKE_GH_STATE="$ROOT/gh-login.state" \
YVEX_FAKE_GH_LOGIN_OK=1 \
  "$YVEX_BIN" source accounts login github --output audit > "$ROOT/gh-login.out"
python3 tests/support/human_field.py "$ROOT/gh-login.out" 'provider: github'
python3 tests/support/human_field.py "$ROOT/gh-login.out" 'status: account-login-pass'
test -f "$ROOT/gh-login-config/accounts.local.json"

HF_TOKEN=super-secret \
GH_TOKEN=other-secret \
YVEX_CONFIG_DIR="$ROOT/token-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_GH_CLI="$FAKE_GH" \
  "$YVEX_BIN" source accounts status --output audit > "$ROOT/token-status.out"
python3 tests/support/human_field.py "$ROOT/token-status.out" 'token_value_redacted: true'
! grep 'super-secret' "$ROOT/token-status.out"
! grep 'other-secret' "$ROOT/token-status.out"
! grep -R 'super-secret' "$ROOT/token-config"
! grep -R 'other-secret' "$ROOT/token-config"

YVEX_CONFIG_DIR="$ROOT/json-config" \
YVEX_HF_CLI="$FAKE_HF" \
YVEX_GH_CLI="$FAKE_GH" \
  "$YVEX_BIN" source accounts providers --json > "$ROOT/providers.json"
python3 - "$ROOT/providers.json" <<'PY'
import json, pathlib, sys
payload = json.loads(pathlib.Path(sys.argv[1]).read_text())
assert payload["schema"] == "yvex.provider.list.v1"
assert {row["provider"] for row in payload["providers"]} == {"huggingface", "github"}
assert all(row["raw_token_stored_by_yvex"] is False for row in payload["providers"])
PY

if "$YVEX_BIN" source accounts login huggingface --token-stdin \
    > "$ROOT/dead-flag.out" 2> "$ROOT/dead-flag.err"; then
  exit 1
fi
grep 'unknown flag: --token-stdin' "$ROOT/dead-flag.err"

"$YVEX_BIN" source accounts status --output nope > "$ROOT/bad-output.out" 2> "$ROOT/bad-output.err" && exit 1 || true
grep 'invalid value for --output: nope' "$ROOT/bad-output.err"
