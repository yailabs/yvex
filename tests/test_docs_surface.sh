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
require_text AGENTS.md 'The logical model is not identified by GGUF or another physical container.'
require_text AGENTS.md '`TRACK.COMPILATION` owns immutable artifact-neutral transformation plans,'
require_text AGENTS.md 'The quantizer consumes canonical transformation truth and may not'
require_text AGENTS.md 'docs/reference-architecture.md'
require_text AGENTS.md 'tensor proof artifact'
require_text AGENTS.md 'complete model artifact'
require_text AGENTS.md 'supported model artifact'

require_text README.md 'native C model-compilation and execution system'
require_text README.md 'not release-ready or'
require_text README.md 'currently supported**.'
require_text README.md '[`PROJECT.md`](PROJECT.md) is the sole authority for current state'
require_text README.md '## What YVEX Builds'
require_text README.md '## Model Identity Is Not Artifact Identity'
require_text README.md '## Compilation Architecture'
require_text README.md '## End-to-End Target and Current State'
require_text README.md '## Engineering Method'
require_text README.md '## Repository Orientation'

for boundary in \
  'Verified source' \
  'Logical model' \
  'Transformation IR' \
  'Physical variant' \
  'Physical lowering' \
  'Artifact' \
  'Runtime binding' \
  'Execution evidence'
do
  require_text README.md "$boundary"
done

require_text README.md 'logical_model_id != physical_variant_id != artifact_id'
require_text README.md 'M --> T['
require_text README.md 'T --> V['
require_text README.md 'V --> L['
require_text README.md 'L --> A['
require_text README.md 'A --> R['
require_text README.md 'R --> E['
require_text README.md 'Planning plane — immutable facts'
require_text README.md 'Byte-execution plane — owned mutable resources'
require_text README.md 'Only trusted source payload streaming is implemented'
require_text README.md 'P   &= \Pi(M; C_p, C_h, C_w)'
require_text README.md 'V   &= \mathcal{T}(S, P)'
require_text README.md 'A_F &= \mathcal{E}_F(V)'
require_text README.md 'B_H &= \mathcal{B}_H(A_F)'
require_text README.md 'E   &= \mathcal{O}\left(\mathcal{R}(B_H, X)\right)'
require_text README.md 'V^{*} \in \mathcal{F}\left(\varepsilon(V), m(V), \ell(V), e(V)\right)'
reject_text README.md '\operatorname'
require_text README.md 'Constraint solving, measurement feedback, hardware/workload-aware selection,'
require_text README.md 'future compilation lanes.'

require_text README.md '| Transformation IR | active — not implemented |'
require_text README.md '| Quantization and reference dequantization | blocked |'
require_text README.md '| GGUF writer and complete artifact | blocked |'
require_text README.md '| Autoregressive text generation | unsupported |'
require_text README.md '| Benchmark | not-measured; benchmark results are not measured |'
require_text README.md '[DeepSeek-V4-Flash](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash)'
require_text README.md 'sole v0.1.0 release target, not a currently supported generation target'
require_text README.md 'Qwen and Gemma'
require_text README.md 'GLM remains'

for evidence in \
  '| Source shards admitted | 46 / 46 |' \
  '| Source tensors indexed | 69,187 |' \
  '| Upstream-verified shard bytes | 159,617,149,040 |' \
  '| Mapped source contributions | 69,187 |' \
  '| Logical GGUF descriptors | 1,360 |' \
  '| Pinned-standard descriptors | 1,328 |' \
  '| YVEX MTP extension descriptors | 32 |' \
  '| Complete payload passes | 1 |' \
  '| Short reads | 0 |' \
  '| Digest mismatches | 0 |' \
  '| Identity drift | 0 |'
do
  require_text README.md "$evidence"
done

reject_text README.md '**YVEX is a native C inference engine'
reject_text README.md 'Active Next:'
reject_text README.md 'YVEX currently generates DeepSeek text'
reject_text README.md 'YVEX supports DeepSeek-V4-Flash'
reject_text README.md 'YVEX compiles arbitrary models'
reject_text README.md 'YVEX selects Pareto-optimal variants'
reject_text README.md 'YVEX performs complete full-model quantization'
reject_text README.md 'YVEX produces complete DeepSeek GGUF artifacts'
reject_text README.md 'YVEX executes the complete transformer'
reject_text README.md 'YVEX is release-ready'
reject_text README.md 'production-ready'
reject_text README.md 'blazing fast'
reject_text README.md 'state of the art'

if grep -nE 'V010\.|POST010\.' README.md; then
  fail 'README exposes internal project-control IDs'
fi
if grep -nE '(/home/|/Users/|\$HOME/)' README.md; then
  fail 'README exposes a local filesystem path'
fi
if grep -nF 'flowchart LR' README.md; then
  fail 'README architecture diagrams must remain top-down'
fi

mermaid_count=$(grep -c '^```mermaid$' README.md)
test "$mermaid_count" -ge 2 ||
  fail "README must contain at least two Mermaid diagrams: $mermaid_count"
flowchart_td_count=$(grep -c '^flowchart TD$' README.md)
test "$flowchart_td_count" -eq "$mermaid_count" ||
  fail "README Mermaid diagrams must be top-down: $flowchart_td_count/$mermaid_count"
awk '
/^```mermaid$/ {
  if (in_mermaid) exit 1
  in_mermaid = 1
  opened++
  next
}
in_mermaid && /^```$/ {
  in_mermaid = 0
  closed++
}
END {
  if (in_mermaid || opened != closed) exit 1
}
' README.md || fail "README Mermaid fences are unbalanced"

math_count=$(grep -c '^```math$' README.md)
test "$math_count" -eq 2 ||
  fail "README must contain the pipeline and Pareto formulations: $math_count"
awk '
/^```math$/ {
  if (in_math) exit 1
  in_math = 1
  opened++
  next
}
in_math && /^```$/ {
  in_math = 0
  closed++
}
END {
  if (in_math || opened != closed) exit 1
}
' README.md || fail "README math fences are unbalanced"

for target in $(grep -oE '\]\([^)]+\)' README.md |
  sed 's/^](//; s/)$//' |
  grep -Ev '^(https?://|mailto:|#)' || true)
do
  target=${target%%#*}
  test -e "$target" || fail "README local link does not resolve: $target"
done

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
require_text "$project" 'The sealed artifact-neutral Transformation IR now projects to 1,360 immutable GGUF lowering descriptors'
require_text "$project" 'Manifest v3 binds every shard to its authoritative Hugging Face Git LFS SHA-256'
require_text "$project" 'all 69,187 contributions and mapping identity `1aecbbe25b04de0d` remain exact'
require_text "$project" 'Production C contains no fallback PTX.'
require_text "$project" 'A no-`nvcc` build refuses every kernel before dispatch'
require_text "$project" 'the dedicated DeepSeek attention path executes encoded projections'
require_text "$project" 'Remaining DeepSeek MoE and transformer operations are unsupported.'
require_text "$project" '| Recovered IDs | 631 |'
require_text "$project" '| Explicit new IDs | 48 |'
require_text "$project" '| Canonical IDs | 679 |'
require_text "$project" '| First-class milestones | 40 |'
require_text "$project" '### 3.5 Model Compilation Boundaries'
require_text "$project" '| `TRACK.COMPILATION` | Artifact-neutral transformation IR'
require_text "$project" 'verified source facts'
require_text "$project" 'verified payload session'
require_text "$project" 'V010.MODEL.TRANSFORM.IR.0'
require_text "$project" 'family roles or transformation semantics. Quantization consumes canonical'
require_text "$project" 'transformation truth and may not rediscover source names, roles, aggregation'
require_text "$project" 'memory, persistent KV, and temporary scratch. Completed source payload'
require_text "$project" 'streaming is build-time source access; it is not inference-time SSD expert'

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
  'deepseek-ai/FlashMLA' \
  'a2bcc5c86678b72a86b7aadc29b643a5ce63c747' \
  '0893eac771d532b7110f1f7581d3f4cd0b9172bf' \
  '02cedf6e4e421ac48d271452bf3836cb57caf297' \
  '1e7394829291360bdcf07036cbe5411631d2d33b' \
  'ae754e9ed8b650e78b921906b2ba8af65ea408ab' \
  '80ebbc396aee40eedc1d829222f3362d10fa4c6c' \
  'SSD expert streaming' \
  'quality validation'
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
require_text MODEL_ARTIFACTS.md 'GGUF is the'
require_text MODEL_ARTIFACTS.md 'v0.1.0 release lowering, not the identity of the logical model.'

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
require_text docs/system-target.md '| Transformation plan | no artifact-neutral transformation IR exists |'
require_text docs/topology-closure-audit.md 'point-in-time inventory'
require_text docs/topology-closure-audit.md '`PROJECT.md` owns when each finding is removed or'
require_text docs/cli-output-architecture.md '## Project State Ownership'
require_text docs/model-families.md 'exact v0.1.0 target'
require_text docs/model-families.md 'typed architecture, exact 69,187-entry source coverage, and a concrete GGUF lowering map exist; no artifact-neutral transformation plan, payload conversion, complete model artifact, or runtime path'
require_text docs/contract.md 'These are implementation facts, not a runtime progress ladder.'
require_text docs/contract.md 'defined only by `PROJECT.md`.'
require_text docs/contract.md '### Model Compilation Contract'
require_text docs/contract.md 'Source payload streaming remains build-time access and does'
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
