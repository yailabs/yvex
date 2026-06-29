# YVEX Operator Runbook

## What this runbook is

This runbook is the current operator command atlas for YVEX.

It is designed to be used after every implementation wave.

It is not a product tutorial.

It is not a generation claim.

It does not hide YVEX behind a single magical command yet.

The primary unit in this runbook is a copy-pack lane. A copy-pack lane is a
complete operator flow that can be pasted as a block. Use the lane that matches
the task. Do not paste the whole file.

The current short entry is path configuration plus model-target path reporting.
Artifact preparation still uses the lower-level source, conversion, and
registry commands until a prepare preset exists.

Current YVEX exposes source intake, YVEX-produced artifact creation, artifact
identity, registry selection, backend materialization, graph proof, runtime
diagnostics, daemon status, and validation.

The future shape is shorter: prepare model, run/chat, serve, stream, evaluate,
and benchmark. That future shape is not claimed until decode, logits, sampling,
generation, and serving exist.

Run commands from the `yailabs/yvex` source repository root.

Real source tensors, generated GGUFs, reports, reference artifacts, local
registries, logs, and caches stay outside the source repository.

## How to use copy-pack lanes

Every copy-pack lane states what it is for, what it requires, what it writes,
whether it is safe to rerun, where to stop, and what boundary it proves.

There are no shell loops.

There are no `if` blocks.

There are no `export` walls.

There are no multiline YVEX commands.

Every YVEX command is one physical line.

When a lane depends on a local artifact, the lane says so. Commands use the
default operator-local path convention:

```sh
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf
$HOME/lab/models/hf/glm/GLM-5.2
$HOME/lab/models/reports/deepseek/
$HOME/lab/models/reference/
```

For another model, quantization, or artifact, replace only the path or alias in
the command block you copy.

Do not change the YVEX source repository to store real model artifacts.

## Operator storage layout

Source tensors: `$HOME/lab/models/hf/<family>/<model>/`

YVEX-produced GGUF artifacts: `$HOME/lab/models/gguf/<family>/`

External reference artifacts: `$HOME/lab/models/reference/<family>/`

Generated reports: `$HOME/lab/models/reports/<family>/`

Local registries: `$HOME/lab/models/registry/`

The source repository stays clean.

Publishing a YVEX-produced GGUF later means uploading it to an artifact
distribution target such as Hugging Face Hub, not committing the real GGUF to
the source repository.

No real `.safetensors`, `.bin`, `.dat`, or `.gguf` file belongs in the YVEX
source repository. Tiny parser fixtures under tests are the only exception.

## Lane 0 — Configure operator storage

Purpose:
  Make YVEX remember the operator-local model root and show the canonical
  storage layout.

Requires:
  Repository root.

Writes:
  `.yvex/operator-paths.conf`
  `$HOME/lab/models` directory tree

Safe to rerun:
  yes

Stop after:
  paths resolve commands report expected DeepSeek and GLM locations.

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
./yvex paths resolve --family glm --kind source
./yvex paths resolve --family glm --kind gguf
./yvex paths resolve --family glm --kind reports
./yvex paths resolve --family glm --kind reference
./yvex paths resolve --family glm --kind registry
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

## Lane 1 — Fast regression after a wave

Purpose:
  Quickly check that command discovery, path resolution, model-target reporting,
  tokenizer fixture diagnostics, and minimal KV diagnostics still work.

Requires:
  Repository root.
  Configured model root is useful but not mandatory.
  No real model artifact required.

Writes:
  No real model artifact.
  May touch normal runtime/test output produced by existing commands.

Safe to rerun:
  yes

Stop after:
  minimal KV diagnostics pass and no model payload path is required.

Boundary:
  read/diagnostic command surface only
  no source conversion
  no alias refresh
  no materialization
  no daemon
  no generation claim

```sh
make
./yvex version
./yvex --version
./yvex commands
./yvex info
./yvexd --help
./yvex paths
./yvex paths resolve --family deepseek --kind source
./yvex paths resolve --family deepseek --kind gguf
./yvex paths resolve --family glm --kind source
./yvex model-target classes
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
./yvex model-target inspect glm-5.2-official-safetensors
./yvex model-target inspect glm-5.2-official-safetensors --paths
./yvex backend cpu
./yvex tokenizer tests/fixtures/gguf/valid-tokenizer-simple.gguf
./yvex tokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex detokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --ids 0,1
./yvex prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --user "hello" --tokens
./yvex input prompt --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex input tokens --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --tokens 0,1
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

## Lane 2 — DeepSeek selected embedding: source tensors to selected graph

Purpose:
  Start from local DeepSeek source tensors, emit the selected embedding GGUF,
  register it, validate it, materialize it, attach it, and execute the selected
  embedding graph.

Requires:
  `$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash` exists.

Writes:
  source manifest
  conversion plan
  selected embedding GGUF
  local model registry entry
  operator reports from integrity/gates when commands write reports

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
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
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

## Lane 3 — DeepSeek selected segment: RMSNorm, prefill, and KV diagnostics

Purpose:
  Validate and exercise the selected embedding-plus-RMSNorm segment artifact
  through graph segment execution, prefill summary, and minimal KV-backed
  binding.

Requires:
  `deepseek4-v4-flash-selected-embed-rmsnorm` GGUF exists.

Writes:
  local model registry entry for the segment alias

Safe to rerun:
  yes, but it refreshes the segment alias.

Stop after:
  segment-summary prefill with minimal KV binding completes.

Boundary:
  selected embedding-plus-RMSNorm segment only
  segment-summary prefill only
  minimal diagnostic KV binding only
  not attention-backed transformer prefill
  not decode
  not logits
  not sampling
  not generation

```sh
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
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

## Lane 4 — Graph-only regression

Purpose:
  Exercise standalone graph primitives, controlled fixture graph, and
  controlled transformer block fixture.

Requires:
  No real model artifact.

Writes:
  controlled tiny GGUF fixture under operator-local model storage

Safe to rerun:
  yes, but it overwrites the controlled tiny fixture.

Stop after:
  controlled transformer block fixture executes.

Boundary:
  controlled graph proofs only
  standalone primitives only
  controlled block fixture only
  not real transformer block over model weights
  not layer scheduling
  not full transformer prefill
  not decode/logits/sampling/generation

```sh
./yvex graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1
./yvex gguf-emit controlled --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --model-name yvex-controlled-fixture --arch deepseek --overwrite
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"
./yvex graph --model "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --backend cpu --execute-fixture --fixture-token 0
./yvex graph --model "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --backend cpu --execute-fixture --fixture-token 1
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
```

## Lane 5 — CUDA pressure lane

Purpose:
  Run CUDA probe, CUDA primitive graph proofs, and CUDA selected artifact graph
  checks.

Requires:
  CUDA-capable host.
  Registered selected aliases for selected artifact commands.

Writes:
  No model artifact.
  May create normal command output.

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

## Lane 6 — Daemon and accepted-only runtime lane

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

## Lane 7 — GLM source-target status lane

Purpose:
  Check GLM target path reporting and source-download status.

Requires:
  GLM download status commands require an existing pid/log after a download has
  been started.

Writes:
  No YVEX artifact.

Safe to rerun:
  yes after GLM download state exists.

Stop after:
  source path, download process/log, shard count, and source size are visible.

Boundary:
  GLM source evidence only
  not GLM inventory completion
  not GLM GGUF emission
  not GLM runtime execution
  not GLM generation

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
ps -p "$(cat "$HOME/lab/models/logs/glm52-safetensors-download.pid")" -o pid,stat,etime,cmd
tail -n 40 "$HOME/lab/models/logs/glm52-safetensors-download.log"
find "$HOME/lab/models/hf/glm/GLM-5.2" -maxdepth 1 -type f -name "*.safetensors" | wc -l
du -sh "$HOME/lab/models/hf/glm/GLM-5.2"
```

## Lane 8 — Full repository validation and hygiene

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
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

## Lane 9 — Low-level command atlas

Purpose:
  Provide compact low-level reference commands without becoming the primary
  workflow.

Requires:
  Repository root.
  Relevant local artifact or manifest paths for artifact-specific commands.

Writes:
  Depends on the command group selected.

Safe to rerun:
  depends on the command group selected.

Stop after:
  the specific diagnostic or low-level command family answers the question.

Boundary:
  reference only
  not a replacement for lanes 0 through 8
  no model prepare claim
  no model check claim
  no graph check claim
  no generation claim

Full help:

```sh
./yvex help backend
./yvex help chat
./yvex help commands
./yvex help convert
./yvex help cuda-info
./yvex help detokenize
./yvex help engine
./yvex help graph
./yvex help gguf-template
./yvex help gguf-emit
./yvex help help
./yvex help imatrix
./yvex help info
./yvex help inspect
./yvex help input
./yvex help integrity
./yvex help kv
./yvex help materialize
./yvex help materialize-gate
./yvex help metadata
./yvex help model-gate
./yvex help model-target
./yvex help models
./yvex help native-weights
./yvex help paths
./yvex help plan
./yvex help prefill
./yvex help prompt
./yvex help quant-job
./yvex help quant-policy
./yvex help qtype-support
./yvex help run
./yvex help session
./yvex help source-manifest
./yvex help tensor-map
./yvex help tokenize
./yvex help tokenizer
./yvex help tensors
./yvex help version
```

Artifact inspection by alias/path:

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex metadata deepseek4-v4-flash-selected-embed
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex metadata "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex inspect deepseek4-v4-flash-selected-embed-rmsnorm
./yvex tensors deepseek4-v4-flash-selected-embed-rmsnorm
```

Integrity and gates:

```sh
./yvex integrity check --model deepseek4-v4-flash-selected-embed
./yvex integrity check --model deepseek4-v4-flash-selected-embed --require-token-embedding --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex integrity check --model deepseek4-v4-flash-selected-embed-rmsnorm --require-token-embedding --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --require-token-embedding --partial-token 0
./yvex model-gate check --model deepseek4-v4-flash-selected-embed --label deepseek-v4-flash-selected-embedding --family deepseek4 --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --report-out "$HOME/lab/models/reports/deepseek/deepseek-model-gate-cpu.txt"
./yvex materialize-gate check --model deepseek4-v4-flash-selected-embed --label deepseek-v4-flash-selected-embedding --family deepseek4 --scope selected-tensor --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --repeat 3 --check-cleanup --report-out "$HOME/lab/models/reports/deepseek/deepseek-materialize-gate-cpu.txt"
```

Materialization and runtime attachment:

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
./yvex plan deepseek4-v4-flash-selected-embed --backend cpu --seq 1 --ctx 16
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
./yvex materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
./yvex session deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
```

Quant/template/intake manifests:

```sh
./yvex qtype-support
./yvex quant-policy inspect --policy "$HOME/lab/models/reports/deepseek/policy.json"
./yvex quant-policy validate --policy "$HOME/lab/models/reports/deepseek/policy.json"
./yvex quant-job inspect --manifest "$HOME/lab/models/reports/deepseek/quant-job.json"
./yvex quant-job validate --manifest "$HOME/lab/models/reports/deepseek/quant-job.json"
./yvex imatrix inspect --manifest "$HOME/lab/models/reports/deepseek/imatrix.json"
./yvex imatrix validate --manifest "$HOME/lab/models/reports/deepseek/imatrix.json"
./yvex gguf-template inspect --template "$HOME/lab/models/reports/deepseek/template.json"
./yvex gguf-template validate --template "$HOME/lab/models/reports/deepseek/template.json"
./yvex gguf-template compare --template "$HOME/lab/models/reports/deepseek/template.json" --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash"
```

## Full implemented command inventory

- `backend`: backend lanes
- `chat`: daemon and accepted-only runtime lanes
- `commands`: fast regression lane
- `convert`: source intake lanes
- `cuda-info`: backend lanes
- `detokenize`: fast regression lane
- `engine`: materialization and runtime attachment lanes
- `graph`: graph lanes
- `gguf-template`: quant/template/intake manifest lanes
- `gguf-emit`: graph lanes
- `help`: full help lane
- `imatrix`: quant/template/intake manifest lanes
- `info`: fast regression lane
- `inspect`: artifact inspection lanes
- `input`: fast regression lane, prefill and KV lanes
- `integrity`: integrity and gate lanes
- `kv`: prefill and KV lanes
- `materialize`: materialization and runtime attachment lanes
- `materialize-gate`: integrity and gate lanes
- `metadata`: artifact inspection lanes
- `model-gate`: integrity and gate lanes
- `model-target`: model lanes, model target path reporting lanes, fast regression lane
- `models`: artifact registration lanes
- `native-weights`: source intake lanes
- `paths`: configure once lane, fast regression lane, path resolution
- `plan`: materialization and runtime attachment lanes
- `prefill`: prefill and KV lanes
- `prompt`: fast regression lane
- `quant-job`: quant/template/intake manifest lanes
- `quant-policy`: quant/template/intake manifest lanes
- `qtype-support`: quant/template/intake manifest lanes
- `run`: daemon and accepted-only runtime lanes
- `session`: materialization and runtime attachment lanes
- `source-manifest`: source intake lanes
- `tensor-map`: source intake lanes
- `tokenize`: fast regression lane
- `tokenizer`: fast regression lane
- `tensors`: artifact inspection lanes
- `version`: fast regression lane

## Future one-command shape

Future shape: prepare model, run/chat, serve, stream, evaluate, and benchmark.

That future shape is not claimed by the current implementation.

## Current boundary

Current YVEX can inspect, emit selected artifacts, register aliases, validate
integrity, materialize selected tensors, attach engine/session state, run
controlled graph proofs, run selected graph slices, run segment-summary prefill,
run minimal KV diagnostics, run daemon status checks, and run accepted-only
chat/run diagnostics. It can also configure operator-local model roots and
report model target paths.

Current YVEX does not implement complete transformer execution, complete
transformer prefill, model prepare preset, model check preset, graph check
preset, decode, logits, sampling, generation, provider generation, capability
evaluation, or benchmarks.

## Manual debug fallback

Use this order before assuming a runtime bug:

1. `./yvex commands`
2. `./yvex help <command>`
3. `./yvex model-target list`
4. `./yvex models current`
5. `./yvex models list`
6. `./yvex inspect <model-or-alias>`
7. `./yvex tensors <model-or-alias>`
8. `./yvex integrity check --model <model-or-alias>`
9. `./yvex backend cpu`
10. `./yvex cuda-info`
11. Rerun CPU before CUDA.
12. Check `git status` before committing.
