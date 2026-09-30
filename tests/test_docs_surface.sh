#!/usr/bin/env sh
set -eu

YVEX_BIN=${YVEX_BIN:-./yvex}

fail() {
  printf 'docs surface: %s\n' "$1" >&2
  exit 1
}

require_text() {
  grep -nF -- "$2" "$1" >/dev/null || fail "$1 missing required text: $2"
}

reject_text() {
  if grep -nF -- "$2" "$1" >/dev/null; then
    fail "$1 retains forbidden text: $2"
  fi
}

require_human() {
  printf '%s\n' "$1" | python3 tests/support/human_field.py /dev/stdin "$2" ||
    fail "$3"
}

# Joining a parent/leaf is a presentation oracle, not relaxed command identity.
for invalid in 'load-extra [PROFILE]' 'load [WRONG]'; do
  if printf '  yvex engine\n    %s  description\n' "$invalid" |
      python3 tests/support/human_field.py /dev/stdin 'yvex engine load [PROFILE]'; then
    fail 'hierarchical help oracle accepted the wrong command/argument'
  fi
done

# Inventory, links, protocol identity, assets and retired paths have one owner.
"${DOCS_PYTHON:-python3}" tests/documentation_architecture.py
require_text README.md '## Why YVEX'
require_text README.md '## Quick start'
require_text README.md '## Product boundary'
require_text README.md '## Documentation'
require_text README.md '## Evidence and current limits'
require_text README.md './yvex model list'
require_text README.md './yvex serve'
require_text README.md './yvex model load'
require_text README.md './yvex chat'
reject_text README.md 'yvex run'
reject_text README.md 'yvex server'
reject_text README.md 'Active Next:'
reject_text README.md 'export YVEX_MODEL_ARTIFACT'

readme_lines=$(wc -l < README.md | tr -d ' ')
test "$readme_lines" -le 500 || fail "README exceeds bounded public entry surface: $readme_lines"

require_text docs/architecture/README.md '# YVEX System Architecture'
require_text docs/architecture/deployment-specialization.md '## Runtime binding'
require_text docs/architecture/computational-state.md '## Sessions and transactional state'
require_text docs/reference/commands.md 'yvex.operator.registry.v1'
require_text docs/model-families/integration.md '# Model-Family Integration Contract'
require_text docs/contracts/artifacts.md '# Artifact and Admission Contract'
require_text docs/contracts/runtime.md 'A client connection is not a session.'
require_text docs/contracts/runtime.md 'no explicit exact request silently changes'
require_text docs/contracts/events-telemetry.md 'No consumer scrapes another renderer'
require_text docs/contracts/c-api.md '## Compiled Operator Registry Boundary'
require_text docs/contracts/openai-compatibility.md 'YVEX never executes application tools.'
require_text docs/guides/operator-runbook.md '## First verified startup'
test ! -e ./yvexd || fail 'retired hidden server executable remains'
if test -x "$YVEX_BIN"; then
  help=$("$YVEX_BIN")
  for command in 'chat' 'serve' 'host' 'model' 'inspect' 'help' 'version'
  do
    printf '%s\n' "$help" | grep -F "$command" >/dev/null ||
      fail "built yvex help lacks canonical command: $command"
  done
  for plumbing in 'engine' 'session' 'source' 'artifact' 'profile' 'compile' 'bench'
  do
    printf '%s\n' "$help" | grep -F "  $plumbing " >/dev/null &&
      fail "built yvex help exposes advanced root: $plumbing"
  done
  for retired in 'yvex run' 'yvex server'; do
    printf '%s\n' "$help" | grep -F "$retired" >/dev/null &&
      fail "built yvex help exposes retired command: $retired"
  done
  advanced=$("$YVEX_BIN" help --advanced)
  for command in 'yvex bench attention execute' 'yvex engine load [PROFILE]' \
                 'yvex source list' 'yvex artifact list' 'yvex profile list'
  do
    require_human "$advanced" "$command" "advanced help lacks canonical command: $command"
  done
  model_help=$("$YVEX_BIN" help model)
  for command in 'yvex model search' 'yvex model pull' 'yvex model prepare' \
                 'yvex model load' 'yvex model unload' 'yvex model push'
  do
    require_human "$model_help" "$command" "model help lacks porcelain command: $command"
  done
  session_help=$("$YVEX_BIN" help session)
  require_human "$session_help" 'yvex session cancel' 'session help lacks canonical cancel command'
  compile_help=$("$YVEX_BIN" help compile)
  require_human "$compile_help" 'yvex compile quant plan' 'compile help lacks canonical quant plan'
  printf '%s\n' "$help" | grep -F 'yvex graph' >/dev/null &&
    fail 'built yvex help exposes retired graph namespace'
fi

serve_help=$("$YVEX_BIN" serve --help)
require_human "$serve_help" 'operation: host.serve' 'serve help lacks foreground host operation'
printf '%s\n' "$serve_help" | grep -F -- 'engine load' >/dev/null &&
  fail 'serve help embeds engine administration'
engine_help=$("$YVEX_BIN" help engine)
require_human "$engine_help" 'yvex engine load [PROFILE]' 'engine help lacks explicit profile load'
require_human "$engine_help" 'yvex engine unload ENGINE' 'engine help lacks independent unload'
