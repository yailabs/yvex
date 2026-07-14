#!/usr/bin/env sh
set -eu

fail() {
  printf 'docs surface: %s\n' "$1" >&2
  exit 1
}

require_file() {
  test -f "$1" || fail "missing file: $1"
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

for file in \
  README.md \
  AGENTS.md \
  PROJECT.md \
  MODEL_ARTIFACTS.md \
  NOTICE.md \
  docs/api.md \
  docs/contract.md \
  docs/model-families.md \
  docs/operator-runbook.md \
  docs/cli-output-architecture.md \
  docs/reference-architecture.md \
  docs/v010-release-doctrine.md \
  docs/topology-closure-audit.md \
  docs/system-target.md \
  docs/runbooks/README.md \
  docs/runbooks/deepseek.md \
  docs/runbooks/common.md
do
  require_file "$file"
done

test -d docs/runbooks || fail "missing directory: docs/runbooks"
test ! -e docs/spine.md || fail "obsolete project-control path exists"
test -z "$(find docs -maxdepth 1 -type d -name repair -print -quit)" ||
  fail "temporary project-control directory exists"
test ! -e docs/runbooks/glm.md || fail "unsupported GLM runbook remains"

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

runbook_count=$(find docs/runbooks -maxdepth 1 -type f -name '*.md' | wc -l | tr -d ' ')
test "$runbook_count" -eq 3 || fail "unexpected runbook count: $runbook_count"

unexpected_doc=$(find docs -maxdepth 1 -type f -name '*.md' \
  ! -name api.md \
  ! -name contract.md \
  ! -name model-families.md \
  ! -name operator-runbook.md \
  ! -name cli-output-architecture.md \
  ! -name reference-architecture.md \
  ! -name v010-release-doctrine.md \
  ! -name topology-closure-audit.md \
  ! -name system-target.md \
  -print -quit)
test -z "$unexpected_doc" || fail "unexpected canonical document: $unexpected_doc"

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
require_text AGENTS.md 'Only a `milestone` row may become Active Next'
require_text AGENTS.md 'Temporary or parallel project-control spines are forbidden.'
require_text AGENTS.md 'Permanent technical documents may link to'
require_text AGENTS.md 'but may not repeat its current state.'
require_text AGENTS.md 'DeepSeek-V4-Flash is the sole v0.1.0 release target.'
require_text AGENTS.md 'Existing Qwen, Gemma, dense/common, MoE, fixture, topology,'
require_text AGENTS.md 'docs/reference-architecture.md'
require_text AGENTS.md 'tensor proof artifact'
require_text AGENTS.md 'complete model artifact'
require_text AGENTS.md 'supported model artifact'

require_text README.md 'native C inference engine'
require_text README.md 'local open-weight models'

sh tests/test_project_ledger.sh

project=PROJECT.md
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
  "### 10.1 Decommission Obligations" \
  "## 11. Release Gates" \
  "## 12. Reference Engineering Ownership" \
  "## 13. Explicit Non-Claims" \
  "## 14. Version Sequence" \
  "## 15. Documentation Ownership And Cutover" \
  "## 16. Agent Start Checklist"
do
  require_text "$project" "$heading"
done

require_text "$project" '`PROJECT.md` is the single project-control authority for YVEX.'
require_text "$project" 'Current proof stage:'
require_text "$project" 'YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend'
require_text "$project" '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text "$project" 'deepseek4-v4-flash'
require_text "$project" 'Qwen, Gemma, and dense/common work already implemented remains active'
require_text "$project" 'The exact DeepSeek-V4-Flash source now projects to one immutable typed IR'
require_text "$project" 'One immutable IR-derived requirement set reconciles exactly against all 69,187 tensors'
require_text "$project" 'The exact 69,187-entry DeepSeek source inventory projects to 1,360 immutable logical GGUF descriptors'
require_text "$project" 'Manifest v3 binds every shard to its authoritative Hugging Face Git LFS SHA-256'
require_text "$project" 'Every one of the 69,187 mapped contributions now resolves to an identity-bound shard range'
require_text "$project" 'Production C contains no fallback PTX.'
require_text "$project" 'A no-`nvcc` build refuses every kernel before dispatch'
require_text "$project" 'The DeepSeek/v0.1 required-operation gate remains unsupported.'
require_text "$project" '| Recovered IDs | 631 |'
require_text "$project" '| Canonical IDs | 665 |'
require_text "$project" '| First-class milestones | 37 |'

for category in \
  "Selected embedding and segment commands" \
  "Bounded diagnostic prefill and KV" \
  "Diagnostic decode" \
  "Fixture logits and sampling" \
  "Bounded diagnostic generation" \
  "Selected-artifact support levels" \
  "Report-only fullmodel surfaces" \
  "Stale target, help, and claim tests"
do
  require_text "$project" "$category"
done
reject_text "$project" 'decide later'

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

reference=docs/reference-architecture.md
require_text "$reference" 'This document owns the external engineering baseline.'
require_text "$reference" 'It does not own project'
require_text "$reference" 'state, milestone state, dependency order, capability claims, or Active Next;'
for source in \
  'vLLM architecture' \
  'vllm/model_executor/models/deepseek_v4.py' \
  '96a04cb13f9c3ed86028e090784a9eb059cf5318' \
  'python/sglang/srt/models/deepseek_v4.py' \
  'GGUF specification' \
  'e920c523e3b8a0163fe498af5bf90df35ff51d25' \
  'conversion/deepseek.py' \
  'src/models/deepseek4.cpp' \
  'TensorRT-LLM architecture' \
  'NVIDIA/cutlass' \
  'Driver API module management' \
  'execution control' \
  'DeepSeek [V4 technical report v1]' \
  '60d8d70770c6776ff598c94bb586a859a38244f1' \
  'deepseek-ai/FlashMLA'
do
  require_text "$reference" "$source"
done
require_text "$reference" 'src/backend/cuda/'
require_text "$reference" 'src/model/architecture/'
require_text "$reference" '8df14cfc8c8a09b4e57f082e59593a3abce4ffb3'
require_text "$reference" 'V010.REBASE.DEEPSEEK.0'
require_text "$reference" 'V010.GGUF.ARTIFACT.ABI.1'
require_text "$reference" 'V010.RUNTIME.DEEPSEEK.MOE.0'

doctrine=docs/v010-release-doctrine.md
require_text "$doctrine" '`PROJECT.md` owns the v0.1.0 product target and active execution sequence.'
require_text "$doctrine" '## Gate State Ownership'
require_text "$doctrine" '`PROJECT.md` is the sole owner of current gate state, milestone state,'
require_text "$doctrine" 'The target is currently unsupported.'
reject_text "$doctrine" '## Current State'

require_text MODEL_ARTIFACTS.md 'Tensor proof artifact'
require_text MODEL_ARTIFACTS.md 'Complete model artifact'
require_text MODEL_ARTIFACTS.md 'Supported model artifact'
require_text MODEL_ARTIFACTS.md 'No such complete model artifact currently exists.'
require_text MODEL_ARTIFACTS.md 'decommission obligations and consuming milestones are recorded in'

deepseek_lines=$(wc -l < docs/runbooks/deepseek.md | tr -d ' ')
test "$deepseek_lines" -le 100 || fail "DeepSeek runbook is not short: $deepseek_lines"
require_text docs/runbooks/deepseek.md '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text docs/runbooks/deepseek.md 'deepseek4-v4-flash'
require_text docs/runbooks/deepseek.md 'There is no supported DeepSeek generation command to run yet.'
require_text docs/runbooks/deepseek.md 'Current milestone state, dependencies, gates, and Active Next live only in'
if grep -nE '\./yvex (generate|prefill|decode|logits|sample|graph|fullmodel|materialize)' docs/runbooks/deepseek.md; then
  fail "DeepSeek runbook retains a selected or diagnostic execution lane"
fi

require_text docs/system-target.md 'Authority: filesystem and module topology; current project state belongs only'
require_text docs/system-target.md '## GGUF Structural Reader Boundary'
require_text docs/system-target.md '## GGUF Qtype ABI Boundary'
require_text docs/topology-closure-audit.md 'point-in-time inventory'
require_text docs/topology-closure-audit.md '`PROJECT.md` owns when each finding is removed or'
require_text docs/cli-output-architecture.md '## Project State Ownership'
require_text docs/model-families.md 'exact v0.1.0 target'
require_text docs/model-families.md 'typed architecture, exact 69,187-entry source coverage, and canonical logical GGUF mapping exist; no payload conversion, complete model artifact, or runtime path'
require_text docs/contract.md 'These are implementation facts, not a runtime progress ladder.'
require_text docs/contract.md 'defined only by `PROJECT.md`.'
require_text docs/api.md 'decommission obligations in `PROJECT.md`'

repair_path=$(printf 'docs/%s' repair)
if rg -nF "$repair_path" AGENTS.md PROJECT.md MODEL_ARTIFACTS.md docs tests Makefile; then
  fail "obsolete temporary project-control path is still referenced"
fi

repair_word=repair
spine_word=spine
repair_phrase="$repair_word $spine_word"
if rg -niF "$repair_phrase" AGENTS.md PROJECT.md MODEL_ARTIFACTS.md docs tests Makefile; then
  fail "obsolete temporary project-control document is still referenced"
fi

if rg -n '^Active Next:' AGENTS.md MODEL_ARTIFACTS.md docs Makefile; then
  fail "project state is duplicated outside PROJECT.md"
fi

if rg -nF 'Active Next: V010.' Makefile tests/test_project_ledger.sh; then
  fail "current milestone is hard-coded in a generic guard"
fi

echo "docs surface: ok (project_control=PROJECT.md taxonomy=owned)"
