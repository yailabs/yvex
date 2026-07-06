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

grep -nF "SPINE.CLI.REBASE.1 - full Operator CLI track rebase after executed V010.CLI.26" docs/spine.md >/dev/null || {
  echo "spine must record the full CLI track rebase after V010.CLI.26" >&2
  exit 1
}

grep -nF "| SPINE.CLI.REBASE.1 | complete | docs/operator | Full Operator CLI track rebase after executed V010.CLI.26 |" docs/spine.md >/dev/null || {
  echo "spine ledger must mark SPINE.CLI.REBASE.1 complete" >&2
  exit 1
}

grep -nF "v0.1.0 - multi-family supported generation over YVEX-produced quantized artifacts" docs/spine.md >/dev/null || {
  echo "spine must lock the v0.1.0 release target to multi-family generation artifacts" >&2
  exit 1
}

grep -nF "The supported v0.1.0 generation family set is DeepSeek, Qwen, and Gemma." docs/spine.md >/dev/null || {
  echo "spine must name DeepSeek, Qwen, and Gemma as the supported v0.1.0 family set" >&2
  exit 1
}

grep -nF "V010.QUANT.1 - multi-family dtype/qtype support by role" docs/spine.md >/dev/null || {
  echo "spine must set Active Next to multi-family qtype support by role" >&2
  exit 1
}

grep -nF "V010.QUANT.1        multi-family dtype/qtype support by runtime role" docs/spine.md >/dev/null || {
  echo "spine must preserve the multi-family quant row title" >&2
  exit 1
}

grep -nF "generation-capable artifact is not runtime generation" docs/spine.md >/dev/null || {
  echo "spine must keep generation-capable artifact separate from runtime generation" >&2
  exit 1
}

grep -nF "DeepSeek cannot close v0.1.0 alone" docs/spine.md >/dev/null || {
  echo "spine must prevent DeepSeek-only v0.1.0 closure" >&2
  exit 1
}

grep -nF "Qwen is a required v0.1.0 supported generation family" docs/spine.md >/dev/null || {
  echo "spine must make Qwen a required v0.1.0 family, not only a Metal future lane" >&2
  exit 1
}

grep -nF "Gemma is the first required dense-family generation target" docs/spine.md >/dev/null || {
  echo "spine must make Gemma the required dense-family generation target" >&2
  exit 1
}

grep -nF "GLM remains huge source/storage pressure" docs/spine.md >/dev/null || {
  echo "spine must keep GLM as source/storage pressure unless promoted" >&2
  exit 1
}

grep -nF "| SPINE.RETARGET.MULTIFAMILY.0 | complete | docs/artifact | v0.1.0 multi-family generation target lock |" docs/spine.md >/dev/null || {
  echo "spine ledger must mark the multi-family retarget complete" >&2
  exit 1
}

grep -nF "V010.CLI.27 - base status and refusal grammar" docs/spine.md >/dev/null || {
  echo "spine must keep the planned CLI substrate row to base status/refusal grammar" >&2
  exit 1
}

grep -nF "V010.CLI.MODELS.4   models artifacts porcelain" docs/spine.md >/dev/null || {
  echo "spine must remap model artifact porcelain to the models command-family layer" >&2
  exit 1
}

grep -nF "V010.CLI.RUNTIME.0  runtime diagnostic command grammar" docs/spine.md >/dev/null || {
  echo "spine must remap runtime diagnostic migration to the command-family layer" >&2
  exit 1
}

if grep -nF "Active implementation next:" -A1 docs/spine.md | grep -nF "V010.CLI.26 - model artifact porcelain migration"; then
  echo "spine must not restore stale V010.CLI.26 artifact migration as Active Next" >&2
  exit 1
fi

if grep -nF "V010.CLI.27 - model artifact porcelain migration" docs/spine.md; then
  echo "spine must not restore V010.CLI.27 as the model artifact porcelain migration" >&2
  exit 1
fi

if grep -nF "V010.CLI.27         model artifact porcelain migration" docs/spine.md; then
  echo "spine must not keep the old flat V010.CLI.27 artifact migration row" >&2
  exit 1
fi

grep -nF "## Full CLI Track Rebase" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must document the full CLI track rebase" >&2
  exit 1
}

grep -nF "V010.CLI.27 - base status and refusal grammar" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must preserve status/refusal grammar as planned" >&2
  exit 1
}

grep -nF "V010.QUANT.1 - multi-family dtype/qtype support by role" docs/cli-output-architecture.md >/dev/null || {
  echo "CLI output architecture must hand immediate Active Next back to multi-family qtype support" >&2
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

if grep -nF "v0.1.0 may close on a feasible dense" docs/spine.md ||
   grep -nF "close one real full-runtime path first" docs/spine.md ||
   grep -nF "DeepSeek V4 Flash full generation is not automatically the minimum v0.1.0 release blocker" docs/spine.md ||
   grep -nF "Qwen/Metal future portability" docs/spine.md ||
   grep -nF "Gemma source/model-class/tensor-collection profile" docs/spine.md; then
  echo "spine must not restore stale single-target or Metal-only retarget language" >&2
  exit 1
fi

echo "docs surface: ok"
