# DeepSeek Operator Runbook

## What This File Is

This runbook is the DeepSeek model-scoped operator path for YVEX.

Its primary paths start from local DeepSeek source tensors and reach the current
selected graph boundary.

It is not a full model generation runbook.

It is not full DeepSeek conversion.

It is not full model execution.

The current DeepSeek path proves selected source-to-artifact emission, local
alias registration, integrity, selected tensor materialization, engine/session
attachment, selected graph execution, bounded diagnostic decode/logits/sampling,
and bounded diagnostic generation-loop control with explicit stop-policy
reporting, local cancellation, partial-output preservation, cleanup reporting,
trace-level diagnostics over the selected segment, and fullmodel inventory/
placement blocker reporting over GGUF metadata and tensor-directory facts.
The fullmodel materialization-plan lane reports placement phases, residency
class, backend fit, preflight blockers, and cleanup plan. The fullmodel
materialize lane proves the bounded tiny/full-ish allocation-and-release
boundary where role coverage is complete, and cleanly refuses the current
selected DeepSeek runtime slices as not full-model artifacts. The fullmodel
descriptor lane reports tensor roles, collections, residency expectations,
graph, prefill, KV, decode, logits, sampling, output-head, tokenizer, backend
requirements, and blockers without executing the model.
The fullmodel family-runtime lane maps descriptor facts into DeepSeek-specific
runtime adapter facts: tensor roles, tensor collections, attention/KV, MoE,
output-head/logits, graph/backend requirements, blockers, and next runtime
dependencies. It is still report-only.
The attention report lane maps those family-runtime facts into attention
class/status, head layout, Q/K/V/O role requirements, RoPE/position
requirements, mask rules, KV requirements, context blockers, graph primitive
versus full-transformer attention distinction, backend requirements, blockers,
and next runtime dependencies. It is report-only.

## DeepSeek Paths And Artifacts

Source tensors:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Selected embedding GGUF:

```text
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Selected embedding-plus-RMSNorm GGUF:

```text
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf
```

Selected embedding alias:

```text
deepseek4-v4-flash-selected-embed
```

Selected segment alias:

```text
deepseek4-v4-flash-selected-embed-rmsnorm
```

## Lane 0 — Configure DeepSeek Operator Storage

Purpose:
  Configure the operator-local root and show DeepSeek target paths.

Requires:
  Repository root.

Writes:
  `.yvex/operator-paths.conf`
  `$HOME/lab/models` directory tree

Safe to rerun:
  yes

Stop after:
  DeepSeek selected target paths are visible.

Boundary:
  no model download
  no artifact creation
  no alias registration
  no runtime support claim

```sh
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex paths
./yvex paths resolve --family deepseek --kind source
./yvex paths resolve --family deepseek --kind gguf
./yvex paths resolve --family deepseek --kind reports
./yvex paths resolve --family deepseek --kind reference
./yvex paths resolve --family deepseek --kind registry
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

## Lane 1 — DeepSeek from safetensors to selected embedding graph

Purpose:
  Use the current selected-artifact prepare preset to emit the selected
  embedding GGUF from local DeepSeek source tensors, refresh the alias, validate
  it, materialize it, attach it, and execute the selected embedding graph.

Requires:
  `$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash` exists.

Writes:
  source manifest
  conversion plan
  selected embedding GGUF
  local model registry entry

Safe to rerun:
  yes, with `--overwrite`.

Stop after:
  selected embedding graph partial executes.

Boundary:
  selected embedding artifact preparation only
  not full DeepSeek conversion
  not full model execution
  not complete transformer prefill
  not decode
  not logits
  not sampling
  not generation

```sh
cd "$HOME/lab/yvex"
make
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex models prepare deepseek4-v4-flash-selected-embed --overwrite
./yvex models current
./yvex models inspect deepseek4-v4-flash-selected-embed
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity check --model deepseek4-v4-flash-selected-embed
./yvex integrity check --model deepseek4-v4-flash-selected-embed --require-token-embedding --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
./yvex plan deepseek4-v4-flash-selected-embed --backend cpu --seq 1 --ctx 16
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

Dry-run and skip variants:

```sh
./yvex models prepare deepseek4-v4-flash-selected-embed --dry-run
./yvex models prepare deepseek4-v4-flash-selected-embed --overwrite --no-register
./yvex models prepare deepseek4-v4-flash-selected-embed --overwrite --no-use
./yvex models prepare deepseek4-v4-flash-selected-embed --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --overwrite
./yvex models prepare deepseek4-v4-flash-selected-embed --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out-dir "$HOME/lab/models/gguf/deepseek" --overwrite
```

Unsupported target proofs:

```sh
./yvex models prepare deepseek4-v4-flash-selected-embed-rmsnorm --dry-run
./yvex models prepare glm-5.2-official-safetensors --dry-run
```

## Lane 2 — DeepSeek Low-Level Source-To-Graph Chain

Purpose:
  Start from local DeepSeek source tensors, emit the selected embedding GGUF,
  register it, validate it, materialize it, attach it, and execute the selected
  embedding graph through the explicit low-level chain.

Requires:
  `$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash` exists.

Writes:
  source manifest
  conversion plan
  selected embedding GGUF
  local model registry entry
  operator reports from integrity commands

Safe to rerun:
  yes, but it overwrites the selected embedding GGUF and refreshes the alias.

Stop after:
  selected embedding graph partial executes.

Boundary:
  selected embedding only
  not full DeepSeek conversion
  not full model execution
  not complete transformer prefill
  not decode
  not logits
  not sampling
  not generation

```sh
cd "$HOME/lab/yvex"
make
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
find "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" -maxdepth 1 -type f | head
find "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" -maxdepth 1 -type f -name "*.safetensors" | wc -l
du -sh "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash"
./yvex source-manifest create --hf-repo "deepseek-ai/DeepSeek-V4-Flash" --revision "main" --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --status in-progress --out "$HOME/lab/models/gguf/deepseek/deepseek-source-manifest.json"
./yvex native-weights --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex tensor-map --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex convert plan --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out-plan "$HOME/lab/models/gguf/deepseek/deepseek-selected-plan.json"
./yvex convert emit --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --tensor embed.weight --target-qtype F16 --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --overwrite
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex metadata "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex models remove deepseek4-v4-flash-selected-embed
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --alias deepseek4-v4-flash-selected-embed --support-level selected-tensor-materialized
./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex models list
./yvex models inspect deepseek4-v4-flash-selected-embed
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity check --model deepseek4-v4-flash-selected-embed
./yvex integrity check --model deepseek4-v4-flash-selected-embed --require-token-embedding --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
./yvex plan deepseek4-v4-flash-selected-embed --backend cpu --seq 1 --ctx 16
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

## Lane 3 — DeepSeek Selected Segment: RMSNorm, Prefill, And KV Diagnostics

Purpose:
  Validate and exercise the selected embedding-plus-RMSNorm segment artifact
  through graph segment execution, prefill summary, minimal KV-backed binding,
  bounded diagnostic decode/logits/sampling, and bounded diagnostic generation
  loop control with stop-policy reporting, trace/cancel/context examples, and
  stable operator-facing help/refusal semantics.

Requires:
  `deepseek4-v4-flash-selected-embed-rmsnorm` GGUF exists.

Writes:
  local model registry entry for the segment alias

Safe to rerun:
  yes, but it refreshes the segment alias.

Stop after:
  bounded diagnostic generation loop reporting completes.

Boundary:
  selected embedding-plus-RMSNorm segment only
  segment-summary prefill only
  minimal diagnostic KV binding only
  bounded diagnostic decode/logits/greedy sampling only
  bounded diagnostic generated-token append/accounting only
  bounded diagnostic stop-policy reporting only
  bounded diagnostic trace reporting only
  stable text diagnostic output only
  not attention-backed transformer prefill
  not real model decode
  not real output-head logits
  not real vocabulary sampling
  not tokenizer-quality text generation
  not full model generation
  not provider generation
  not streaming generation
  not evaluation
  not benchmark

```sh
./yvex help generate
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
./yvex models remove deepseek4-v4-flash-selected-embed-rmsnorm
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf" --alias deepseek4-v4-flash-selected-embed-rmsnorm --support-level selected-tensor-materialized
./yvex models inspect deepseek4-v4-flash-selected-embed-rmsnorm
./yvex models verify deepseek4-v4-flash-selected-embed-rmsnorm
./yvex inspect deepseek4-v4-flash-selected-embed-rmsnorm
./yvex tensors deepseek4-v4-flash-selected-embed-rmsnorm
./yvex integrity check --model deepseek4-v4-flash-selected-embed-rmsnorm --require-token-embedding --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex session deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex input tokens --model deepseek4-v4-flash-selected-embed-rmsnorm --tokens 0,1
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
./yvex decode --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex logits --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex sample --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 2 --trace-level full
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 1
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --cancel-after-steps 2 --trace-level full
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 4
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3 --context-length 5
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

## Lane 4 — DeepSeek Fullmodel Inventory And Placement Report

Purpose:
  Report tensor inventory, qtype/dtype bytes, tensor collection candidates,
  DeepSeek role coverage, placement pressure, and runtime blockers for selected
  DeepSeek artifacts.

Requires:
  Registered selected aliases or direct GGUF paths.

Writes:
  No model artifact.
  No registry change.

Safe to rerun:
  yes

Stop after:
  selected artifacts report incomplete full-model inventory, partial
  materialization plans, selected-slice materialization proof refusal, partial
  runtime descriptors, and generation stays unsupported-full-model.

Boundary:
  metadata and tensor-directory inventory only
  bounded proof allocation only for controlled tiny/full-ish artifacts
  selected DeepSeek artifacts refuse fullmodel materialization proof
  descriptor reports requirements and blockers only
  family-runtime maps descriptor facts into DeepSeek adapter facts only
  attention report maps adapter facts into attention requirements only
  KV report maps attention/family facts into KV cache class requirements only
  no uncontrolled full backend allocation
  no full transformer attention
  no real QKV projection from model tensors
  no real attention-backed KV writes
  no real KV reads by decode
  no full model execution
  no real DeepSeek generation
  no provider generation
  no streaming generation
  no evaluation
  no benchmark
  no throughput

```sh
./yvex help fullmodel
./yvex fullmodel report --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda
./yvex fullmodel report --model "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf" --target deepseek4-v4-flash --backend cpu --limit-tensors 20
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --residency resident
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --residency host-staged
./yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --residency hybrid
./yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda
./yvex fullmodel descriptor --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex fullmodel descriptor --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex fullmodel descriptor --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda
./yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu
./yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu
./yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu
./yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cuda
./yvex help attention
./yvex attention report --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu
./yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu
./yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu
./yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cuda
./yvex help kv
./yvex kv report --model deepseek4-v4-flash-selected-embed --family deepseek --backend cpu
./yvex kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --include-attention --include-context --include-residency --include-blockers
./yvex kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu
./yvex kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cuda
```

## Lane 5 — DeepSeek CUDA Pressure Lane

Purpose:
  Run CUDA probe, CUDA primitive graph proofs, and CUDA selected artifact graph
  checks.

Requires:
  CUDA-capable host.
  Registered selected aliases for selected artifact commands.

Writes:
  No model artifact.

Safe to rerun:
  yes on CUDA-capable hosts.

Stop after:
  CUDA selected segment prefill with minimal KV binding completes.

Boundary:
  CUDA over implemented selected/materialized/graph surfaces only
  not full model execution
  not generation
  not benchmark

```sh
./yvex cuda-info
./yvex backend cuda
./yvex graph --backend cuda --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cuda --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cuda --execute-op --op matmul --m 1 --k 8 --n 8
./yvex graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cuda --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cuda --execute-partial --partial-token 0
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

## Lane 6 — DeepSeek Daemon And Accepted-Only Diagnostics

Purpose:
  Check daemon status surface and accepted-only diagnostic chat/run commands.

Requires:
  Registered selected embedding alias for model-status daemon path.
  Tokenizer fixture for chat/run diagnostics.

Writes:
  No model artifacts.

Safe to rerun:
  yes

Stop after:
  daemon one-request commands and accepted-only diagnostics complete.

Boundary:
  daemon status only
  accepted-only diagnostic path
  not provider completion
  not streaming
  not generation

```sh
./yvexd --model deepseek4-v4-flash-selected-embed --backend cpu --host 127.0.0.1 --port 18080 --one-request
./yvexd --host 127.0.0.1 --port 18081 --one-request
./yvexd --host 127.0.0.1 --port 18082 --one-request
./yvex chat --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
./yvex run --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --prompt "hello"
```

## Lane 7 — DeepSeek Validation And Hygiene

Purpose:
  Run repository validation, CUDA validation, and artifact guardrails before
  commit.

Requires:
  Repository root.
  CUDA validation requires CUDA-capable host.

Writes:
  No model artifact.
  May create normal build/test output.

Safe to rerun:
  yes

Stop after:
  validation commands pass and artifact guardrails show no tracked real model
  artifacts.

Boundary:
  validation only
  no model capability claim
  no benchmark claim
  no artifact commit

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
make check-cuda
git status --short
git ls-files "*.safetensors" "*.bin" "*.dat"
git ls-files "*.gguf"
```

## Current DeepSeek Boundary

Current DeepSeek support is selected-artifact support only.

YVEX can:

- report selected DeepSeek target paths;
- emit the selected embedding GGUF from local source tensors;
- prepare the selected embedding artifact through the current preset;
- register the selected embedding alias;
- inspect, tensor-list, and metadata-list the selected GGUF;
- run integrity reports;
- materialize selected tensors on CPU/CUDA;
- attach selected materialized state to engine/session;
- execute selected embedding graph;
- execute selected embedding-plus-RMSNorm segment when the segment artifact exists;
- create segment-summary prefill state;
- bind that state to minimal diagnostic KV;
- run bounded diagnostic decode/logits/greedy sampling over the selected segment;
- run a bounded diagnostic generation loop with token append/accounting and
  stop-policy reporting over the selected segment;
- use stable `yvex generate` help, command inventory, refusal wording,
  trace/cancel/context examples, and text diagnostic output;
- report DeepSeek runtime family adapter facts from fullmodel descriptor
  inventory without executing a full transformer path;
- report DeepSeek attention class facts, Q/K/V/O role requirements, RoPE/mask
  requirements, attention KV requirements, graph primitive/full-transformer
  distinctions, and next runtime dependencies without executing attention.
- report DeepSeek KV cache class facts, diagnostic-versus-real KV boundary,
  layout/dtype/indexing/capacity/residency requirements, context and attention
  dependencies, and next runtime blockers without allocating full runtime KV.

YVEX does not currently implement:

- full DeepSeek conversion;
- full DeepSeek materialization;
- full DeepSeek materialization proof;
- complete transformer execution;
- full transformer attention;
- real QKV projection from model tensors;
- real attention-backed KV writes;
- real KV reads by decode;
- paged, chunked, SSD-backed, or quantized KV runtime;
- CUDA full KV allocation proof;
- complete transformer prefill;
- real DeepSeek decode;
- real output-head logits;
- real vocabulary sampling;
- tokenizer-quality text generation;
- full model generation;
- provider generation;
- streaming generation;
- evaluation;
- benchmarks.
