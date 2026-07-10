#!/usr/bin/env sh
set -eu

tmp="${TMPDIR:-/tmp}/yvex-docs-surface.$$"
trap 'rm -f "$tmp"' EXIT

fail() {
  printf 'docs surface: %s\n' "$1" >&2
  exit 1
}

require_file() {
  test -f "$1" || fail "missing file: $1"
}

require_dir() {
  test -d "$1" || fail "missing directory: $1"
}

require_text() {
  file=$1
  value=$2
  grep -nF "$value" "$file" >/dev/null ||
    fail "$file missing required text: $value"
}

reject_text() {
  file=$1
  value=$2
  if grep -nF "$value" "$file" >/dev/null; then
    fail "$file retains forbidden text: $value"
  fi
}

assert_ordered() {
  file=$1
  shift
  previous=0
  for value in "$@"; do
    hit=$(grep -nF "$value" "$file" | head -n 1 || true)
    test -n "$hit" || fail "$file missing ordered value: $value"
    line=${hit%%:*}
    test "$line" -gt "$previous" ||
      fail "$file has out-of-order value: $value"
    previous=$line
  done
}

for file in \
  README.md \
  AGENTS.md \
  MODEL_ARTIFACTS.md \
  docs/api.md \
  docs/contract.md \
  docs/model-families.md \
  docs/operator-runbook.md \
  docs/cli-output-architecture.md \
  docs/v010-release-doctrine.md \
  docs/topology-closure-audit.md \
  docs/spine.md \
  docs/system-target.md \
  docs/repair/v010-foundation-closure.md \
  docs/runbooks/README.md \
  docs/runbooks/deepseek.md \
  docs/runbooks/glm.md \
  docs/runbooks/common.md
do
  require_file "$file"
done

require_dir docs/repair
require_dir docs/runbooks

archive_path=docs/arc""hive

for path in \
  docs/README.md \
  docs/backend-contract.md \
  docs/cli-commands.md \
  docs/cli-interface-spine.md \
  docs/cli-runtime.md \
  docs/runtime-filesystem.md \
  "$archive_path" \
  docs/legacy \
  docs/spines
do
  test ! -e "$path" || fail "obsolete or archive path exists: $path"
done

root_doc_count=$(find docs -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')
test "$root_doc_count" -eq 9 || fail "unexpected root docs count: $root_doc_count"

repair_doc_count=$(find docs/repair -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')
test "$repair_doc_count" -eq 1 || fail "unexpected repair docs count: $repair_doc_count"

runbook_count=$(find docs/runbooks -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')
test "$runbook_count" -eq 4 || fail "unexpected runbook count: $runbook_count"

for heading in \
  "## 0. Repository Contract" \
  "## 1. Execution Order" \
  "## 2. Ownership" \
  "## 3. Source File Contracts" \
  "## 4. C Implementation Rules" \
  "## 5. CLI Rules" \
  "## 6. Runtime And Backend Rules" \
  "## 7. Source, Tensor, And Artifact Rules" \
  "## 8. Evidence Stages" \
  "## 9. Claims" \
  "## 10. Docs" \
  "## 11. Tests" \
  "## 12. Validation" \
  "## 13. Review Failure" \
  "## 14. Final Rule"
do
  require_text AGENTS.md "$heading"
done

require_text AGENTS.md '`docs/spine.md` is the compact product execution spine.'
require_text AGENTS.md 'Temporary priority-blocking repair spines live under `docs/repair/`.'
require_text AGENTS.md 'tensor proof artifact'
require_text AGENTS.md 'complete model artifact'
require_text AGENTS.md 'supported model artifact'
require_text AGENTS.md 'Do not use unqualified "model artifact" for a selected-tensor proof file.'

require_text README.md 'native C inference engine'
require_text README.md 'local open-weight models'

spine_lines=$(wc -l < docs/spine.md | tr -d ' ')
test "$spine_lines" -le 350 || fail "main spine exceeds 350 lines: $spine_lines"

for heading in \
  "## Product Outcome" \
  "## Current Hard Truth" \
  "## v0.1.0 Release Contract" \
  "## Active Blocking Work" \
  "## Main Critical Path" \
  "## Release Gates" \
  "## Version Sequence" \
  "## Explicit Non-Claims" \
  "## Documentation Ownership"
do
  require_text docs/spine.md "$heading"
done

main_h2_count=$(grep -c '^## ' docs/spine.md)
test "$main_h2_count" -eq 9 || fail "main spine must have exactly 9 H2 sections"

require_text docs/spine.md 'YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend'
require_text docs/spine.md '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text docs/spine.md 'deepseek4-v4-flash` is the future canonical target and is unsupported.'
require_text docs/spine.md 'V010.DOCS.REFOUNDATION.0: complete'
require_text docs/spine.md 'proof stage: documentation/claim refoundation only'
require_text docs/spine.md 'Active Next: V010.REBASE.DEEPSEEK.0'
require_text docs/spine.md '`V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened'
require_text docs/spine.md 'Full model materialization and CUDA residency are unsupported.'
require_text docs/spine.md 'Not measured.'

for term in \
  "Tensor proof artifact" \
  "Complete model artifact" \
  "Supported model artifact"
do
  require_text docs/spine.md "$term"
done

for old in \
  "## 0. Dashboard" \
  "## 1. Capability Map" \
  "## 5. Tracks" \
  "### Canonical Row Trackmap" \
  "## 7. Row Contract Families" \
  "Appendix: Canonical Vocabulary" \
  "multi-family supported generation" \
  "DeepSeek/Qwen/Gemma"
do
  reject_text docs/spine.md "$old"
done

critical_path=$(awk '
  /^## Main Critical Path$/ { in_path = 1; next }
  in_path && /^## / { exit }
  in_path { print }
' docs/spine.md)

printf '%s\n' "$critical_path" > "$tmp"

for forbidden in selected diagnostic fixture report-only CLI; do
  if printf '%s\n' "$critical_path" | grep -i "$forbidden" >/dev/null; then
    fail "main critical path contains non-product evidence: $forbidden"
  fi
done

assert_ordered "$tmp" \
  "verified DeepSeek source" \
  "typed architecture specification" \
  "complete tensor role and layout map" \
  "complete YVEX-produced GGUF" \
  "full materialization and residency" \
  "executable runtime descriptor" \
  "family-correct attention, position handling and KV" \
  "MoE routing and expert execution" \
  "complete transformer stack" \
  "tokenizer, output head, logits and sampling" \
  "autoregressive generation" \
  "evaluation and benchmark" \
  "v0.1.0 release"

repair=docs/repair/v010-foundation-closure.md
require_text "$repair" 'Status: priority-blocking'
require_text "$repair" 'This is not a second product roadmap.'
require_text "$repair" 'the main critical path is blocked.'
require_text "$repair" 'Git history is the archive; no compatibility copy is kept.'
require_text "$repair" '| Row / status | Owner | Concrete defect or missing capability | Required outcome | Acceptance gate | Dependency / next row |'
require_text "$repair" '| Active Next | `V010.REBASE.DEEPSEEK.0` |'

assert_ordered "$repair" \
  "V010.DOCS.REFOUNDATION.0" \
  "V010.REBASE.DEEPSEEK.0" \
  "V010.GGUF.QTYPE.ABI.1" \
  "V010.GGUF.ARTIFACT.ABI.1" \
  "V010.GGUF.LAYOUT.INTEGRITY.1" \
  "V010.CUDA.FAILCLOSED.0" \
  "V010.MODEL.ARCH.IR.0" \
  "V010.MAP.GGUF.DEEPSEEK.0" \
  "V010.SOURCE.PAYLOAD.STREAM.0" \
  "V010.QUANT.2" \
  "V010.GGUF.WRITER.1" \
  "V010.ARTIFACT.EMIT.DEEPSEEK.0" \
  "V010.GGUF.ROUNDTRIP.1"

for category in \
  "Selected embedding and selected segment commands" \
  "Bounded diagnostic prefill and KV" \
  "Diagnostic decode" \
  "Fixture logits and sampling" \
  "Bounded diagnostic generation" \
  "Selected-artifact support levels" \
  "Report-only fullmodel surfaces" \
  "Stale target, help, and claim tests"
do
  require_text "$repair" "$category"
done

for disposition in \
  "Absorb backend/reference comparisons" \
  "Replace with family-correct prefill" \
  "Retain numeric selection cases only as internal test fixtures" \
  "Remove selected states from model support"
do
  require_text "$repair" "$disposition"
done

if grep -niF 'decide later' "$repair" >/dev/null; then
  fail "repair decommission map contains decide-later disposition"
fi

for row in \
  V010.GGUF.QTYPE.ABI.1 \
  V010.GGUF.ARTIFACT.ABI.1 \
  V010.GGUF.LAYOUT.INTEGRITY.1 \
  V010.CUDA.FAILCLOSED.0 \
  V010.MODEL.ARCH.IR.0 \
  V010.MAP.GGUF.DEEPSEEK.0 \
  V010.SOURCE.PAYLOAD.STREAM.0 \
  V010.QUANT.2 \
  V010.GGUF.WRITER.1 \
  V010.ARTIFACT.EMIT.DEEPSEEK.0 \
  V010.GGUF.ROUNDTRIP.1
do
  require_text "$repair" "| \`$row\` / blocked |"
done

doctrine=docs/v010-release-doctrine.md
require_text "$doctrine" 'YVEX generates real text with'
require_text "$doctrine" '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text "$doctrine" 'deepseek4-v4-flash'
require_text "$doctrine" 'The target is currently unsupported.'
require_text "$doctrine" 'An unqualified model artifact is never a selected-tensor proof file.'
require_text "$doctrine" '`V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened.'
reject_text "$doctrine" 'every supported generation family in {DeepSeek, Qwen, Gemma}'
reject_text "$doctrine" 'No single family can close v0.1.0.'

require_text MODEL_ARTIFACTS.md 'Tensor proof artifact'
require_text MODEL_ARTIFACTS.md 'Complete model artifact'
require_text MODEL_ARTIFACTS.md 'Supported model artifact'
require_text MODEL_ARTIFACTS.md 'No such complete model artifact currently exists.'
reject_text MODEL_ARTIFACTS.md '## Active Artifact'
reject_text MODEL_ARTIFACTS.md '## Historical Validation Artifact'
reject_text MODEL_ARTIFACTS.md 'deepseek4-v4-flash-selected-embed'

deepseek_lines=$(wc -l < docs/runbooks/deepseek.md | tr -d ' ')
test "$deepseek_lines" -le 100 || fail "DeepSeek runbook is not short: $deepseek_lines"
require_text docs/runbooks/deepseek.md '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text docs/runbooks/deepseek.md 'deepseek4-v4-flash'
require_text docs/runbooks/deepseek.md 'There is no supported DeepSeek generation command to run yet.'
require_text docs/runbooks/deepseek.md 'V010.REBASE.DEEPSEEK.0'
require_text docs/runbooks/deepseek.md '../repair/v010-foundation-closure.md'
if grep -nE '\./yvex (generate|prefill|decode|logits|sample|graph|fullmodel|materialize)' docs/runbooks/deepseek.md; then
  fail "DeepSeek runbook retains a selected or diagnostic execution lane"
fi

require_text docs/system-target.md 'V010.REBASE.DEEPSEEK.0` is Active Next.'
require_text docs/system-target.md '## Reopened: V010.GGUF.ARTIFACT.ABI.0'
require_text docs/system-target.md '## Reopened: V010.GGUF.QTYPE.ABI.0'
require_text docs/topology-closure-audit.md 'docs/repair/v010-foundation-closure.md'
require_text docs/topology-closure-audit.md 'this audit does not set Active Next.'
require_text docs/cli-output-architecture.md 'V010.REBASE.DEEPSEEK.0'
require_text docs/cli-output-architecture.md 'CLI output cleanup is not the current product blocker.'

require_text docs/model-families.md 'exact v0.1.0 target'
require_text docs/model-families.md 'unsupported; no complete model artifact or runtime path'
require_text docs/contract.md 'The implemented proof and ownership surfaces are:'
require_text docs/contract.md 'These are implementation facts, not a runtime progress ladder.'
require_text docs/api.md 'Several current surfaces are legacy bounded proofs pending the'
require_text docs/api.md 'not product runtime capability.'

echo "docs surface: ok (spine_lines=$spine_lines)"
