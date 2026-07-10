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
  PROJECT.md \
  MODEL_ARTIFACTS.md \
  docs/api.md \
  docs/contract.md \
  docs/model-families.md \
  docs/operator-runbook.md \
  docs/cli-output-architecture.md \
  docs/v010-release-doctrine.md \
  docs/topology-closure-audit.md \
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

test ! -e docs/spine.md || fail "obsolete project-control path exists: docs/spine.md"

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

require_text AGENTS.md '`PROJECT.md` is the single living engineering control file.'
require_text AGENTS.md 'The project control file has no arbitrary line or heading-count limit.'
require_text AGENTS.md 'Git history is recovery evidence, not a substitute for the current ledger.'
require_text AGENTS.md 'Only a `milestone` row may become Active Next'
require_text AGENTS.md 'DeepSeek-V4-Flash is the sole v0.1.0 release target.'
require_text AGENTS.md 'It is not the sole'
require_text AGENTS.md 'engineering scope.'
require_text AGENTS.md 'they are not tracks or independent product milestones.'
require_text AGENTS.md 'Temporary priority-blocking repair spines live under `docs/repair/`.'
require_text AGENTS.md 'tensor proof artifact'
require_text AGENTS.md 'complete model artifact'
require_text AGENTS.md 'supported model artifact'
require_text AGENTS.md 'Do not use unqualified "model artifact" for a selected-tensor proof file.'

require_text README.md 'native C inference engine'
require_text README.md 'local open-weight models'

project=PROJECT.md

sh tests/test_project_ledger.sh

for heading in \
  "## 1. Authority And Update Contract" \
  "## 2. Rank, State, And Proof Semantics" \
  "## 3. Product, Release, And Engineering Scope" \
  "## 4. Current Hard Truth" \
  "## 5. Active Work And Critical Path" \
  "## 6. Family Capability Matrix" \
  "## 7. Track Registry And Dashboard" \
  "## 8. First-Class Milestone Roadmap" \
  "## 9. Complete Track/Wave Ledger" \
  "## 10. Evidence Lanes" \
  "## 11. Release Gates" \
  "## 12. Reference Engineering Baseline" \
  "## 13. Explicit Non-Claims" \
  "## 14. Version Sequence" \
  "## 15. Documentation Ownership And Cutover" \
  "## 16. Agent Start Checklist"
do
  require_text "$project" "$heading"
done

require_text "$project" '`PROJECT.md` is the single project-control authority for YVEX.'
require_text "$project" 'Git history is recovery evidence, not a substitute for current project state.'
require_text "$project" 'Only a `milestone`'
require_text "$project" 'YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend'
require_text "$project" '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text "$project" 'deepseek4-v4-flash'
require_text "$project" 'DeepSeek-V4-Flash is the only model whose complete source-to-text chain closes'
require_text "$project" 'Qwen, Gemma, and dense/common work already implemented remains active'
require_text "$project" 'No execution-complete DeepSeek architecture IR exists.'
require_text "$project" 'a no-op fallback can be advertised as support'
require_text "$project" 'Active Next: V010.DOCS.ARCHITECTURE.0'
require_text "$project" 'V010.PROJECT.RECOVERY.0: partial'
require_text "$project" 'V010.PROJECT.RECOVERY.1: complete'
require_text "$project" 'recovered unique IDs: **631**'
require_text "$project" 'total canonical IDs: **665**'
require_text "$project" 'first-class milestones: **37**'

for term in \
  "Tensor proof artifact" \
  "Complete model artifact" \
  "Supported model artifact"
do
  require_text "$project" "$term"
done

for rank in milestone capability evidence subtask migration future; do
  require_text "$project" "| \`$rank\` |"
done

if grep -nE '^### 9\.[0-9]+ TRACK\.(ARCHITECTURE|EXECUTION|MODELS|PROJECT|CLAIMS|TOPOLOGY)$' "$project"; then
  fail "attempted recovery track name became canonical"
fi

for reference in \
  'vLLM architecture' \
  'DeepSeek-V4 engineering' \
  'SGLang runtime' \
  'DeepSeek-V4 model' \
  'GGUF specification' \
  'TensorRT-LLM' \
  'DeepSeek-V4-Flash'
do
  require_text "$project" "$reference"
done

repair=docs/repair/v010-foundation-closure.md
require_text "$repair" 'Status: priority-blocking'
require_text "$repair" 'Project control: `../../PROJECT.md`'
require_text "$repair" 'This is not a second product roadmap.'
require_text "$repair" 'the main critical path is blocked.'
require_text "$repair" 'Git history is the archive; no compatibility copy is kept.'
require_text "$repair" '| Row / status | Owner | Concrete defect or missing capability | Required outcome | Acceptance gate | Dependency / next row |'
require_text "$repair" '| Active Next | `V010.DOCS.ARCHITECTURE.0` |'
require_text "$repair" '| Partial rows | `V010.PROJECT.RECOVERY.0` |'
require_text "$repair" '| `V010.PROJECT.RECOVERY.0` / partial |'
require_text "$repair" '| `V010.PROJECT.RECOVERY.1` / complete |'
require_text "$repair" '| `V010.DOCS.ARCHITECTURE.0` / active |'
require_text "$repair" '| `V010.REBASE.DEEPSEEK.0` / blocked |'
require_text "$repair" '## Ledger Recovery Evidence'
reject_text "$repair" '## Track Recovery Map'

project_active=$(sed -n 's/^Active Next: \([^[:space:]]*\)$/\1/p' "$project")
repair_active=$(sed -n 's/^| Active Next | `\([^`]*\)` |$/\1/p' "$repair")
test -n "$project_active" || fail "PROJECT.md has no machine-readable Active Next"
test -n "$repair_active" || fail "repair spine has no machine-readable Active Next"
test "$project_active" = "$repair_active" ||
  fail "Active Next drift: PROJECT=$project_active repair=$repair_active"

assert_ordered "$repair" \
  "V010.DOCS.REFOUNDATION.0" \
  "V010.PROJECT.RECOVERY.0" \
  "V010.PROJECT.RECOVERY.1" \
  "V010.DOCS.ARCHITECTURE.0" \
  "V010.REBASE.DEEPSEEK.0" \
  "V010.GGUF.QTYPE.ABI.1" \
  "V010.GGUF.ARTIFACT.ABI.1" \
  "V010.GGUF.LAYOUT.INTEGRITY.1" \
  "V010.CUDA.FAILCLOSED.0" \
  "V010.MODEL.ARCH.IR.0" \
  "V010.TENSOR.COVERAGE.DEEPSEEK.0" \
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

if grep -niF 'decide later' "$repair" >/dev/null; then
  fail "repair decommission map contains decide-later disposition"
fi

for row in \
  V010.REBASE.DEEPSEEK.0 \
  V010.GGUF.QTYPE.ABI.1 \
  V010.GGUF.ARTIFACT.ABI.1 \
  V010.GGUF.LAYOUT.INTEGRITY.1 \
  V010.CUDA.FAILCLOSED.0 \
  V010.MODEL.ARCH.IR.0 \
  V010.TENSOR.COVERAGE.DEEPSEEK.0 \
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
require_text "$doctrine" '`PROJECT.md` owns the v0.1.0 product target and active execution sequence.'
require_text "$doctrine" 'YVEX generates real text with'
require_text "$doctrine" '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text "$doctrine" 'deepseek4-v4-flash'
require_text "$doctrine" 'The target is currently unsupported.'
require_text "$doctrine" 'An unqualified model artifact is never a selected-tensor proof file.'
require_text "$doctrine" '`V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened.'
require_text "$doctrine" '`V010.DOCS.ARCHITECTURE.0` is Active Next'
reject_text "$doctrine" 'every supported generation family in {DeepSeek, Qwen, Gemma}'
reject_text "$doctrine" 'No single family can close v0.1.0.'

require_text MODEL_ARTIFACTS.md 'Tensor proof artifact'
require_text MODEL_ARTIFACTS.md 'Complete model artifact'
require_text MODEL_ARTIFACTS.md 'Supported model artifact'
require_text MODEL_ARTIFACTS.md 'No such complete model artifact currently exists.'
require_text MODEL_ARTIFACTS.md 'recorded in `PROJECT.md`.'
reject_text MODEL_ARTIFACTS.md '## Active Artifact'
reject_text MODEL_ARTIFACTS.md '## Historical Validation Artifact'
reject_text MODEL_ARTIFACTS.md 'deepseek4-v4-flash-selected-embed'

deepseek_lines=$(wc -l < docs/runbooks/deepseek.md | tr -d ' ')
test "$deepseek_lines" -le 100 || fail "DeepSeek runbook is not short: $deepseek_lines"
require_text docs/runbooks/deepseek.md '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text docs/runbooks/deepseek.md 'deepseek4-v4-flash'
require_text docs/runbooks/deepseek.md 'There is no supported DeepSeek generation command to run yet.'
require_text docs/runbooks/deepseek.md 'V010.DOCS.ARCHITECTURE.0'
require_text docs/runbooks/deepseek.md 'V010.REBASE.DEEPSEEK.0'
require_text docs/runbooks/deepseek.md '../../PROJECT.md'
require_text docs/runbooks/deepseek.md '../repair/v010-foundation-closure.md'
if grep -nE '\./yvex (generate|prefill|decode|logits|sample|graph|fullmodel|materialize)' docs/runbooks/deepseek.md; then
  fail "DeepSeek runbook retains a selected or diagnostic execution lane"
fi

require_text docs/system-target.md 'Authority: `PROJECT.md`'
require_text docs/system-target.md 'V010.DOCS.ARCHITECTURE.0` is Active'
require_text docs/system-target.md 'V010.REBASE.DEEPSEEK.0` is blocked by documentation architecture.'
require_text docs/system-target.md '## Reopened: V010.GGUF.ARTIFACT.ABI.0'
require_text docs/system-target.md '## Reopened: V010.GGUF.QTYPE.ABI.0'
require_text docs/topology-closure-audit.md 'docs/repair/v010-foundation-closure.md'
require_text docs/topology-closure-audit.md 'this audit does not set Active Next.'
require_text docs/topology-closure-audit.md 'V010.DOCS.ARCHITECTURE.0` Active Next.'
require_text docs/cli-output-architecture.md 'V010.DOCS.ARCHITECTURE.0'
require_text docs/cli-output-architecture.md 'CLI output cleanup is not the current product blocker.'

require_text docs/model-families.md 'exact v0.1.0 target'
require_text docs/model-families.md 'unsupported; no complete model artifact or runtime path'
require_text docs/model-families.md 'belong in `PROJECT.md`.'
require_text docs/contract.md 'The implemented proof and ownership surfaces are:'
require_text docs/contract.md 'These are implementation facts, not a runtime progress ladder.'
require_text docs/contract.md 'defined only by `PROJECT.md`.'
require_text docs/api.md 'Several current surfaces are legacy bounded proofs pending the'
require_text docs/api.md 'not product runtime capability.'

echo "docs surface: ok (project_control=PROJECT.md active_next=$project_active)"
