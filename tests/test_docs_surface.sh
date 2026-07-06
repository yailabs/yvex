#!/usr/bin/env sh
set -eu

test -f README.md
test -f AGENTS.md
test -f MODEL_ARTIFACTS.md
test -f docs/api.md
test -f docs/contract.md
test -f docs/model-families.md
test -f docs/operator-runbook.md
test -f docs/cli-output-architecture.md
test -d docs/runbooks
test -f docs/runbooks/README.md
test -f docs/runbooks/deepseek.md
test -f docs/runbooks/glm.md
test -f docs/runbooks/common.md
test -f docs/spine.md

test ! -e docs/README.md
test ! -e docs/backend-contract.md
test ! -e docs/cli-commands.md
test ! -e docs/cli-interface-spine.md
test ! -e docs/cli-runtime.md
test ! -e docs/runtime-filesystem.md

grep -nF "native C inference engine" README.md >/dev/null || {
  echo "README must open with public inference-engine identity" >&2
  exit 1
}

grep -nF "local open-weight models" README.md >/dev/null || {
  echo "README must identify the local open-weight model target" >&2
  exit 1
}

grep -nF 'local inference engine cannot treat `GGUF` as a file extension only.' README.md >/dev/null || {
  echo "README must frame GGUF/artifact work as engine infrastructure" >&2
  exit 1
}

if grep -nE '^## Current state$|^## Runtime roadmap$|Current YVEX status|Runtime stage[[:space:]]*\|.*Status|^\|[[:space:]]*Area[[:space:]]*\|[[:space:]]*Current state|^\|[[:space:]]*Measurement[[:space:]]*\|[[:space:]]*Current YVEX status|^\|[[:space:]]*Future benchmark row[[:space:]]*\|.*Status|^\|[[:space:]]*Eval group[[:space:]]*\|.*Status|^\|.*[[:space:]](implemented|planned|next|unsupported)[[:space:]].*\|' README.md; then
  echo "README must not expose internal implementation roadmap/status tables" >&2
  exit 1
fi

if grep -nF "not a full transformer run" README.md ||
   grep -nF "not a complete inference system" README.md ||
   grep -nF "not transformer prefill yet" README.md ||
   grep -nF "not a real prefill" README.md ||
   grep -nF "only foundations" README.md ||
   grep -nF "benchmark results: none" README.md; then
  echo "README must frame staged runtime progress as inference-engine vision, not boundary apology" >&2
  exit 1
fi

count="$(find docs -maxdepth 1 -type f | wc -l | tr -d ' ')"
if [ "$count" -ne 6 ]; then
  echo "unexpected docs file count: $count"
  find docs -maxdepth 1 -type f | sort
  exit 1
fi

grep -nF "table = renderer layout, not a long-term user-selected mode" docs/spine.md >/dev/null || {
  echo "spine must keep table as layout doctrine" >&2
  exit 1
}

grep -nF "| CLI.ARCH.AUDIT.0 | complete | docs/operator |" docs/spine.md >/dev/null || {
  echo "CLI architecture audit must be complete as docs/operator doctrine" >&2
  exit 1
}

grep -nF "V010.CLI.25         renderer ownership foundation" docs/spine.md >/dev/null || {
  echo "spine must preserve the renderer foundation migration row" >&2
  exit 1
}

grep -nF "| V010.CLI.25 | complete | operator | Renderer ownership foundation |" docs/spine.md >/dev/null || {
  echo "spine must mark V010.CLI.25 complete after renderer foundation implementation" >&2
  exit 1
}

grep -nF "V010.CLI.26         base CLI grammar and command catalog" docs/spine.md >/dev/null || {
  echo "spine must preserve the base CLI grammar row" >&2
  exit 1
}

grep -nF "| V010.CLI.26 | complete | operator | Base CLI grammar and command catalog |" docs/spine.md >/dev/null || {
  echo "spine must mark V010.CLI.26 complete after command grammar implementation" >&2
  exit 1
}

grep -nF "V010.CLI.27 - model artifact porcelain migration" docs/spine.md >/dev/null || {
  echo "spine must move model artifact porcelain migration to V010.CLI.27" >&2
  exit 1
}

grep -nF "V010.CLI.29         graph/runtime diagnostic renderer migration" docs/spine.md >/dev/null || {
  echo "spine must preserve the shifted graph/runtime migration row" >&2
  exit 1
}

grep -nF "## CLI Output Architecture Doctrine" docs/spine.md >/dev/null || {
  echo "spine must use the porcelain/plumbing architecture doctrine heading" >&2
  exit 1
}

if grep -nF "## Output Mode Taxonomy" docs/spine.md ||
   grep -nF "## CLI Output UX Doctrine" docs/spine.md; then
  echo "spine must not restore the old output-mode taxonomy as future doctrine" >&2
  exit 1
fi

grep -nF "JSON output is not implemented uniformly in this audit" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must not claim uniform JSON output" >&2
  exit 1
}

grep -nF "## Renderer Foundation" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must record the implemented renderer foundation" >&2
  exit 1
}

grep -nF "## Base CLI Grammar" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must record the implemented base CLI grammar" >&2
  exit 1
}

grep -nF "This audit does not remove all print sites" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must preserve the audit-only boundary" >&2
  exit 1
}

runbook_count="$(find docs/runbooks -maxdepth 1 -type f | wc -l | tr -d ' ')"
if [ "$runbook_count" -ne 4 ]; then
  echo "unexpected runbook file count: $runbook_count"
  find docs/runbooks -maxdepth 1 -type f | sort
  exit 1
fi

unexpected_runbooks="$(
  find docs/runbooks -maxdepth 1 -type f \
    ! -name README.md \
    ! -name deepseek.md \
    ! -name glm.md \
    ! -name common.md \
    -print
)"
if [ -n "$unexpected_runbooks" ]; then
  echo "$unexpected_runbooks"
  echo "unexpected runbook files"
  exit 1
fi

if grep -nE 'OPERATOR\.PATHS\.0|MODEL\.TARGET\.PATHS\.0|MODEL\.PREPARE\.0|MODEL\.CHECK\.0|SPINE\.GENERATION\.TARGET\.0|DOCS\.RUNBOOKS\.MODEL\.0' docs/operator-runbook.md docs/model-families.md docs/runbooks/*.md; then
  echo "public runbooks must not expose internal delivery IDs" >&2
  exit 1
fi

echo "docs surface: ok"
