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
test -f docs/v010-release-doctrine.md
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

for agents_heading in \
  "## 0. Repository Contract" \
  "## 1. Execution Order" \
  "## 2. Ownership" \
  "## 3. C Implementation Rules" \
  "## 4. CLI Rules" \
  "## 5. Runtime And Backend Rules" \
  "## 6. Source, Tensor, And Artifact Rules" \
  "## 7. Evidence Stages" \
  "## 8. Claims" \
  "## 9. Docs" \
  "## 10. Tests" \
  "## 11. Validation" \
  "## 12. Review Failure" \
  "## 13. Final Rule"; do
  grep -nF "$agents_heading" AGENTS.md >/dev/null || {
    echo "AGENTS.md missing canonical heading: $agents_heading" >&2
    exit 1
  }
done

if grep -nE '^## ' AGENTS.md |
   grep -vE '## (0\. Repository Contract|1\. Execution Order|2\. Ownership|3\. C Implementation Rules|4\. CLI Rules|5\. Runtime And Backend Rules|6\. Source, Tensor, And Artifact Rules|7\. Evidence Stages|8\. Claims|9\. Docs|10\. Tests|11\. Validation|12\. Review Failure|13\. Final Rule)$'; then
  echo "AGENTS.md must not add extra H2 sections" >&2
  exit 1
fi

for agents_term in \
  "# AGENTS.md" \
  "YVEX is a native C local inference engine" \
  "yvex_cli.c owns dispatch only" \
  "Header-only means header-only" \
  "Generation-capable artifact is not runtime generation" \
  "A claim requires implementation, tests, command proof, and documented boundary" \
  "Make YVEX more real"; do
  grep -nF "$agents_term" AGENTS.md >/dev/null || {
    echo "AGENTS.md missing canonical rule: $agents_term" >&2
    exit 1
  }
done

if grep -niE 'aims to|designed to empower|seamless|journey|future-proof|state-of-the-art|robust and scalable|comprehensive solution' AGENTS.md; then
  echo "AGENTS.md must stay terse and free of marketing/filler language" >&2
  exit 1
fi

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
   grep -nF "not runtime prefill" README.md ||
   grep -nF "only foundations" README.md ||
   grep -nF "benchmark results: none" README.md; then
  echo "README must frame staged runtime progress as inference-engine vision, not boundary apology" >&2
  exit 1
fi

count="$(find docs -maxdepth 1 -type f | wc -l | tr -d ' ')"
if [ "$count" -ne 7 ]; then
  echo "unexpected docs file count: $count"
  find docs -maxdepth 1 -type f | sort
  exit 1
fi

grep -nF "docs/v010-release-doctrine.md" docs/spine.md >/dev/null || {
  echo "spine must list v0.1.0 release doctrine in the active docs set" >&2
  exit 1
}

for doctrine_heading in \
  "## 0. Authority" \
  "## 1. Release Identity" \
  "## 2. Supported Family Set" \
  "## 3. Closure Rule" \
  "## 4. Supported Family Meaning" \
  "## 5. Artifact Versus Runtime Generation" \
  "## 6. Non-Closing Evidence" \
  "## 7. Target Classes" \
  "## 8. Release Gates" \
  "## 9. Readiness Vocabulary" \
  "## 10. Forbidden Claims" \
  "## 11. Governed Surfaces" \
  "## 12. Change Control"; do
  grep -nF "$doctrine_heading" docs/v010-release-doctrine.md >/dev/null || {
    echo "release doctrine missing canonical heading: $doctrine_heading" >&2
    exit 1
  }
done

if grep -nE '^## ' docs/v010-release-doctrine.md |
   grep -vE '## (0\. Authority|1\. Release Identity|2\. Supported Family Set|3\. Closure Rule|4\. Supported Family Meaning|5\. Artifact Versus Runtime Generation|6\. Non-Closing Evidence|7\. Target Classes|8\. Release Gates|9\. Readiness Vocabulary|10\. Forbidden Claims|11\. Governed Surfaces|12\. Change Control)$'; then
  echo "release doctrine must not add extra H2 sections" >&2
  exit 1
fi

for doctrine_term in \
  "# v0.1.0 Release Doctrine" \
  "v0.1.0 is not the first model that runs" \
  "every supported generation family in {DeepSeek, Qwen, Gemma}" \
  "generation-capable artifact is not runtime generation" \
  "DeepSeek cannot close v0.1.0 alone" \
  "Qwen cannot close v0.1.0 alone" \
  "Gemma cannot close v0.1.0 alone" \
  "Qwen/Metal is post-v0.1.0 backend portability" \
  "GLM is source/storage pressure unless a later TRACK.SCOPE row promotes it" \
  "release-ready"; do
  grep -nF "$doctrine_term" docs/v010-release-doctrine.md >/dev/null || {
    echo "release doctrine missing required term: $doctrine_term" >&2
    exit 1
  }
done

grep -nF "| V010.SCOPE.0 | complete | v0.1.0 release doctrine. |" docs/spine.md >/dev/null || {
  echo "spine must mark V010.SCOPE.0 complete" >&2
  exit 1
}

if awk '
  FILENAME == "docs/v010-release-doctrine.md" &&
    /^## 10\. Forbidden Claims/ { skip = 1 }
  FILENAME == "docs/v010-release-doctrine.md" &&
    /^## 11\. Governed Surfaces/ { skip = 0 }
  !skip { print FILENAME ":" FNR ":" $0 }
' docs/v010-release-doctrine.md docs/spine.md |
   grep -nE 'single model can close v0\.1\.0|DeepSeek alone closes v0\.1\.0|Qwen/Metal closes v0\.1\.0|GLM closes v0\.1\.0|generation-capable artifact is runtime generation'; then
  echo "release doctrine or spine contains stale positive closure language" >&2
  exit 1
fi

spine_lines="$(wc -l < docs/spine.md | tr -d ' ')"
if [ "$spine_lines" -gt 1800 ]; then
  echo "spine must stay under 1800 lines after restoring explicit row trackmap labels, got $spine_lines" >&2
  exit 1
fi

for heading in \
  "## 0. Dashboard" \
  "## 1. Capability Map" \
  "## 2. Unsupported Boundaries" \
  "## 3. v0.1.0 Target" \
  "## 4. Active Next" \
  "## 5. Tracks" \
  "## 6. Track Gates" \
  "## 7. Row Contract Families" \
  "## 8. Release Gates" \
  "## 9. Post-v0.1.0" \
  "## 10. Appendix: Canonical Vocabulary"; do
  grep -nF "$heading" docs/spine.md >/dev/null || {
    echo "spine missing canonical heading: $heading" >&2
    exit 1
  }
done

if grep -nE '^## [0-9]+\. ' docs/spine.md |
   grep -vE '## (0\. Dashboard|1\. Capability Map|2\. Unsupported Boundaries|3\. v0\.1\.0 Target|4\. Active Next|5\. Tracks|6\. Track Gates|7\. Row Contract Families|8\. Release Gates|9\. Post-v0\.1\.0|10\. Appendix: Canonical Vocabulary)$'; then
  echo "spine must not add extra numbered top-level sections" >&2
  exit 1
fi

if grep -nF "## 1. Current Capability" docs/spine.md; then
  echo "spine must replace Current Capability with Capability Map" >&2
  exit 1
fi

for capability_heading in \
  "### 1.1 Pipeline Capability Map" \
  "### 1.2 Supported-Family Capability Matrix" \
  "### 1.3 Implemented Capability Snapshot"; do
  grep -nF "$capability_heading" docs/spine.md >/dev/null || {
    echo "spine missing capability-map heading: $capability_heading" >&2
    exit 1
  }
done

for capability_term in \
  "source target identity" \
  "dtype/qtype support by runtime role" \
  "qtype compute/refusal matrix" \
  "generation-capable artifact emission" \
  "transformer prefill" \
  "KV writes" \
  "KV reads" \
  "runtime decode" \
  "output-head logits" \
  "vocabulary sampling" \
  "runtime generation loop" \
  "CLI generation command" \
  "eval smoke/regression" \
  "benchmark transcript" \
  "DeepSeek" \
  "Qwen" \
  "Gemma"; do
  grep -nF "$capability_term" docs/spine.md >/dev/null || {
    echo "spine missing capability-map term: $capability_term" >&2
    exit 1
  }
done

if grep -nE '\b([Rr]eal|[Aa]ctual)\b' docs/spine.md; then
  echo "spine must use transformer/runtime wording instead of vague capability adjectives" >&2
  exit 1
fi

grep -nF "Current highest implemented runtime stage | bounded diagnostic generation" docs/spine.md >/dev/null || {
  echo "spine must keep the current highest implemented runtime stage" >&2
  exit 1
}

grep -nF "Current full-runtime state | unsupported" docs/spine.md >/dev/null || {
  echo "spine must keep full-runtime unsupported" >&2
  exit 1
}

grep -nF "Generation state | DeepSeek/Qwen/Gemma unsupported" docs/spine.md >/dev/null || {
  echo "spine must keep family generation unsupported" >&2
  exit 1
}

grep -nF "Benchmark state | not measured" docs/spine.md >/dev/null || {
  echo "spine must keep benchmark state not measured" >&2
  exit 1
}

grep -nF "CUDA state | bounded primitive-hardening only" docs/spine.md >/dev/null || {
  echo "spine must keep CUDA primitive-hardening boundary" >&2
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

grep -nF "v0.1.0 closes only when every supported v0.1.0 generation family reaches the" docs/spine.md >/dev/null || {
  echo "spine must keep the multi-family closure rule" >&2
  exit 1
}

grep -nF "V010.QUANT.1 - multi-family dtype/qtype support by role" docs/spine.md >/dev/null || {
  echo "spine must set Active Next to multi-family qtype support by role" >&2
  exit 1
}

grep -nF "| V010.QUANT.1 | active | multi-family dtype/qtype support by runtime role. |" docs/spine.md >/dev/null || {
  echo "spine must preserve the multi-family quant row title" >&2
  exit 1
}

grep -nF "### Canonical Row Trackmap" docs/spine.md >/dev/null || {
  echo "spine must preserve the active canonical row trackmap" >&2
  exit 1
}

grep -nF "| Wave | Status | Description |" docs/spine.md >/dev/null || {
  echo "spine row trackmap must include wave/status/description columns" >&2
  exit 1
}

for row_label in \
  "SPINE.ROW.CATALOG.0" \
  "SPINE.ROW.CATALOG.1" \
  "SPINE.CAPABILITY.MAP.0" \
  "V010.SOURCE.0" \
  "V010.SOURCE.10" \
  "V010.CLI.0" \
  "V010.CLI.15" \
  "V010.CLI.28" \
  "V010.CLI.TARGET.0" \
  "V010.CLI.SOURCE.0" \
  "V010.CLI.GRAPH.0" \
  "V010.CLI.RUNTIME.0" \
  "V010.CLI.GENERATE.0" \
  "V010.CLI.CHAT.0" \
  "V010.DOCTOR.0" \
  "V010.SERVE.0" \
  "V010.EVAL.0" \
  "V010.BENCH.0" \
  "V010.RELEASE.0" \
  "V010.CI.0" \
  "POST010.QWEN.METAL.0" \
  "POST010.SPEC.0"; do
  grep -nF "$row_label" docs/spine.md >/dev/null || {
    echo "spine row trackmap missing: $row_label" >&2
    exit 1
  }
done

grep -nF "| V010.CLI.27 | planned, not Active Next | base status and refusal grammar. |" docs/spine.md >/dev/null || {
  echo "spine must keep V010.CLI.27 planned but not Active Next" >&2
  exit 1
}

if grep -nF "V010.CLI.29" docs/spine.md; then
  echo "spine must not contain undefined V010.CLI.29" >&2
  exit 1
fi

if grep -nE 'V010\.[A-Z.]+\.[0-9]+-[0-9]+|V010\.[A-Z.]+\.\*|POST010\.[A-Z.]+\.\*|[A-Z]+(\.[A-Z]+)+\.\*' docs/spine.md; then
  echo "spine row labels must be explicit, not compressed ranges or wildcards" >&2
  exit 1
fi

awk '
  /^### TRACK.SCOPE -/ { in_catalog = 0 }
  /^### Canonical Row Trackmap/ { in_catalog = 1 }
  in_catalog && /^\| [A-Z0-9]/ {
    split($0, fields, "|")
    if (fields[4] ~ /^[[:space:]]*$/) {
      print
      bad = 1
    }
  }
  END { exit bad }
' docs/spine.md || {
  echo "spine row trackmap entries must include descriptions" >&2
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

grep -nF "GLM remains source/storage pressure" docs/spine.md >/dev/null || {
  echo "spine must keep GLM as source/storage pressure" >&2
  exit 1
}

grep -nF "GLM remains huge source/storage pressure" docs/spine.md >/dev/null || {
  echo "spine must keep GLM as source/storage pressure unless promoted" >&2
  exit 1
}

grep -nF "Qwen/Metal remains post-v0.1.0 backend portability" docs/spine.md >/dev/null || {
  echo "spine must keep Qwen/Metal post-v0.1.0" >&2
  exit 1
}

grep -nF "| SPINE.RETARGET.MULTIFAMILY.0 | complete | Lock v0.1.0 to DeepSeek, Qwen, and Gemma as the supported generation-family set. |" docs/spine.md >/dev/null || {
  echo "spine ledger must mark the multi-family retarget complete" >&2
  exit 1
}

grep -nF "| SPINE.TRACK.CANON.0 | complete | Replace the oversized active spine with the compact track-first map. |" docs/spine.md >/dev/null || {
  echo "spine must mark the canonical track rewrite complete" >&2
  exit 1
}

grep -nF "| CLI.ARCH.AUDIT.0 | complete | Inventory print/output pressure and porcelain/plumbing doctrine. |" docs/spine.md >/dev/null || {
  echo "CLI architecture audit must be complete as docs/operator doctrine" >&2
  exit 1
}

grep -nF "| V010.CLI.25 | complete | renderer ownership foundation. |" docs/spine.md >/dev/null || {
  echo "spine must preserve the renderer foundation migration row" >&2
  exit 1
}

grep -nF "| V010.CLI.25 | complete | renderer ownership foundation. |" docs/spine.md >/dev/null || {
  echo "spine must mark V010.CLI.25 complete after renderer foundation implementation" >&2
  exit 1
}

grep -nF "| V010.CLI.26 | complete | base CLI grammar and command catalog. |" docs/spine.md >/dev/null || {
  echo "spine must preserve the base CLI grammar row" >&2
  exit 1
}

grep -nF "| V010.CLI.26 | complete | base CLI grammar and command catalog. |" docs/spine.md >/dev/null || {
  echo "spine must mark V010.CLI.26 complete after command grammar implementation" >&2
  exit 1
}

grep -nF "| SPINE.CLI.REBASE.1 | complete | Rebase Operator CLI track after V010.CLI.26 grammar work. |" docs/spine.md >/dev/null || {
  echo "spine must record the full CLI track rebase after V010.CLI.26" >&2
  exit 1
}

grep -nF "| SPINE.CLI.REBASE.1 | complete | Rebase Operator CLI track after V010.CLI.26 grammar work. |" docs/spine.md >/dev/null || {
  echo "spine ledger must mark SPINE.CLI.REBASE.1 complete" >&2
  exit 1
}

grep -nF "| V010.CLI.27 | planned, not Active Next | base status and refusal grammar. |" docs/spine.md >/dev/null || {
  echo "spine must keep the planned CLI substrate row to base status/refusal grammar" >&2
  exit 1
}

grep -nF "| V010.CLI.MODELS.4 | planned | models artifacts porcelain. |" docs/spine.md >/dev/null || {
  echo "spine must remap model artifact porcelain to the models command-family layer" >&2
  exit 1
}

grep -nF "| V010.CLI.RUNTIME.0 | planned | runtime diagnostic command grammar. |" docs/spine.md >/dev/null || {
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

for removed in \
  "## Historical Delivery Ledger" \
  "## Meta-Spine / Rebase / Audit Waves" \
  "## Evidence Crosswalks" \
  "## Audit and Reconciliation" \
  "## Doctrine Appendix" \
  "## Detailed Validation Gate Archive" \
  "## Planned Row Supersession Map" \
  "## CLI Research Surface Matrix" \
  "## Preserved Current Repository and Capability State" \
  "## Preserved Runtime, Generation, Tensor, and Operator Doctrine" \
  "## 6.0 Forward Runtime Track Matrix" \
  "## 6.1 Canonical Runtime Sequences" \
  "## 6.2 v0.1.0 Master Implementation Spine" \
  "### 5.31 Detailed V010 Row Inventory and Dependency Maps" \
  "## Output Mode Taxonomy" \
  "## CLI Output UX Doctrine"; do
  if grep -nF "$removed" docs/spine.md; then
    echo "spine must not restore removed archive heading: $removed" >&2
    exit 1
  fi
done

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
   grep -nF "close one full-runtime path first" docs/spine.md ||
   grep -nF "DeepSeek V4 Flash full generation is not automatically the minimum v0.1.0 release blocker" docs/spine.md ||
   grep -nF "Qwen/Metal future portability" docs/spine.md ||
   grep -nF "Gemma source/model-class/tensor-collection profile" docs/spine.md; then
  echo "spine must not restore stale single-target or Metal-only retarget language" >&2
  exit 1
fi

echo "docs surface: ok"
