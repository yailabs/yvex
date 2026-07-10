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
require_text AGENTS.md 'they are not tracks or independent product milestones.'
require_text AGENTS.md 'Temporary priority-blocking repair spines live under `docs/repair/`.'
require_text AGENTS.md 'tensor proof artifact'
require_text AGENTS.md 'complete model artifact'
require_text AGENTS.md 'supported model artifact'
require_text AGENTS.md 'Do not use unqualified "model artifact" for a selected-tensor proof file.'

require_text README.md 'native C inference engine'
require_text README.md 'local open-weight models'

project=PROJECT.md

for heading in \
  "## Authority And Update Contract" \
  "## Product Outcome" \
  "## Current Hard Truth" \
  "## Architecture Map" \
  "## Track Model" \
  "## Milestone Contract" \
  "## Delivery State" \
  "## Recovered Implementation History" \
  "## Reference Baseline" \
  "## Release Gates" \
  "## Version Sequence" \
  "## Explicit Non-Claims" \
  "## Documentation Ownership"
do
  require_text "$project" "$heading"
done

require_text "$project" '`PROJECT.md` is the single project-control authority for YVEX.'
require_text "$project" 'The project file has no arbitrary line limit or fixed heading count.'
require_text "$project" 'YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend'
require_text "$project" '$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash'
require_text "$project" '`deepseek4-v4-flash` is the future canonical target and is unsupported.'
require_text "$project" '`V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened'
require_text "$project" 'Full model materialization and CUDA residency are unsupported.'
require_text "$project" 'A fallback PTX path can advertise no-op kernels as supported'
require_text "$project" 'Not measured.'
require_text "$project" 'Active Next: V010.DOCS.ARCHITECTURE.0'
require_text "$project" 'V010.REBASE.DEEPSEEK.0: blocked by documentation architecture'

for term in \
  "Tensor proof artifact" \
  "Complete model artifact" \
  "Supported model artifact"
do
  require_text "$project" "$term"
done

for heading in \
  "### Primary Release Tracks" \
  "### Supporting Architectural Tracks" \
  "### Future Tracks" \
  "### Evidence Lanes"
do
  require_text "$project" "$heading"
done

for track in \
  TRACK.SOURCE \
  TRACK.ARCHITECTURE \
  TRACK.MAP \
  TRACK.TENSOR \
  TRACK.QUANT \
  TRACK.ARTIFACT \
  TRACK.INTEGRITY \
  TRACK.RESIDENCY \
  TRACK.BACKEND \
  TRACK.EXECUTION \
  TRACK.PREFILL \
  TRACK.KV \
  TRACK.DECODE \
  TRACK.LOGITS \
  TRACK.SAMPLING \
  TRACK.TOKENIZER \
  TRACK.GENERATION \
  TRACK.EVAL \
  TRACK.BENCH \
  TRACK.RELEASE
do
  require_text "$project" "\`$track\`"
done

for track in TRACK.PROJECT TRACK.CLAIMS TRACK.TOPOLOGY TRACK.OPERATOR; do
  require_text "$project" "\`$track\`"
done

for track in \
  TRACK.SERVE \
  TRACK.DISTRIBUTED \
  TRACK.PORTABILITY \
  TRACK.MODELS \
  TRACK.ACCELERATION \
  TRACK.POST010
do
  require_text "$project" "\`$track\`"
done

if grep -nE '`TRACK\.(DIAGNOSTIC|REPORT|FIXTURE|SELECTED)' "$project"; then
  fail "diagnostic/report/fixture/selected evidence was promoted to a track"
fi

require_text "$project" 'Diagnostic/report/fixture/selected work is evidence, not a primary track.'
require_text "$project" 'Evidence must name its owning track, the property it proves, the lowest true'
require_text "$project" 'A tensor proof artifact is not a complete or supported model artifact.'
require_text "$project" 'Diagnostic runtime is not real prefill, KV, decode, logits, sampling, or generation.'

require_text "$project" 'A milestone is a complete architectural or functional slice that changes the'
require_text "$project" 'It has exactly one owning track and a downstream consumer.'
require_text "$project" 'the full repository validation required by `AGENTS.md`;'
require_text "$project" 'A row is not milestone-complete merely because it:'
for non_closing in \
  'adds a report, CLI command, help entry, or structured output;' \
  'creates a fixture or tensor proof artifact;' \
  'exposes a selected tensor, graph segment, or bounded diagnostic loop;' \
  'renames or moves code, or wraps an existing monolith;' \
  'records a plan, audit, checklist, or documentation update.'
do
  require_text "$project" "$non_closing"
done

critical_path=$(awk '
  /^### Main Critical Path$/ { in_path = 1; next }
  in_path && /^### / { exit }
  in_path { print }
' "$project")

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

require_text "$project" 'Recovery compared `10ad6c3:docs/spine.md`'
require_text "$project" 'with `690d1b18:docs/spine.md` and its repair owner'
for anchor in \
  V010.SOURCE.1-.7 \
  V010.GGUF.QTYPE.ABI.0 \
  V010.GGUF.ARTIFACT.ABI.0 \
  V010.INTEGRITY.0-.6 \
  CUDA.KERNEL.0 \
  V010.GRAPH.22 \
  V010.GEN.0-.7 \
  TOPOLOGY.CELL.MODEL_TARGET.0 \
  V010.CI.4-.7
do
  require_text "$project" "$anchor"
done

for reference in \
  'vLLM architecture overview' \
  'SGLang runtime' \
  'DeepSeek-V4 implementation' \
  'GGUF specification' \
  'TensorRT-LLM architecture' \
  'DeepSeek-V4-Flash model card and report'
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
require_text "$repair" '| `V010.PROJECT.RECOVERY.0` / complete |'
require_text "$repair" '| `V010.DOCS.ARCHITECTURE.0` / active |'
require_text "$repair" '| `V010.REBASE.DEEPSEEK.0` / blocked |'

project_active=$(sed -n 's/^Active Next: \([^[:space:]]*\)$/\1/p' "$project" | head -n 1)
repair_active=$(sed -n 's/^| Active Next | `\([^`]*\)` |$/\1/p' "$repair" | head -n 1)
test -n "$project_active" || fail "PROJECT.md has no machine-readable Active Next"
test -n "$repair_active" || fail "repair spine has no machine-readable Active Next"
test "$project_active" = "$repair_active" ||
  fail "Active Next drift: PROJECT=$project_active repair=$repair_active"

assert_ordered "$repair" \
  "V010.DOCS.REFOUNDATION.0" \
  "V010.PROJECT.RECOVERY.0" \
  "V010.DOCS.ARCHITECTURE.0" \
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

require_text "$repair" '## Track Recovery Map'
require_text "$repair" '| Original track | Resulting project track / owner | Restored conclusive milestone count |'
for old_track in \
  TRACK.SCOPE TRACK.SOURCE TRACK.MAP TRACK.QUANT TRACK.ARTIFACT TRACK.INTEGRITY \
  TRACK.MODEL TRACK.TENSOR TRACK.RESIDENCY TRACK.BACKEND TRACK.GRAPH \
  TRACK.PREFILL TRACK.KV TRACK.DECODE TRACK.LOGITS TRACK.SAMPLING \
  TRACK.TOKENIZER TRACK.GENERATION TRACK.OPERATOR TRACK.SERVE TRACK.EVAL \
  TRACK.BENCH TRACK.RELEASE TRACK.POST010
do
  require_text "$repair" "| \`$old_track\` |"
done

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
