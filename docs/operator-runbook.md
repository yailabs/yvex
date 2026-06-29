# YVEX Operator Runbook

## What this runbook is

This runbook is the current operator command atlas for YVEX.

It is designed to be used after every implementation wave.

It is not a product tutorial.

It is not a generation claim.

It does not hide YVEX behind a single magical command yet.

The current short entry is path configuration plus model-target path reporting.
Artifact preparation still uses the lower-level source, conversion, and
registry commands until a prepare preset exists.

Current YVEX exposes the actual engine boundaries:

- source intake;
- YVEX-produced artifact creation;
- artifact identity;
- registry selection;
- backend materialization;
- graph proof;
- runtime diagnostics;
- daemon status;
- validation.

The future shape is shorter:

```text
prepare model
run/chat
serve
stream
evaluate
benchmark
```

That future shape is not claimed until decode, logits, sampling, generation,
and serving exist.

Run commands from the `yailabs/yvex` source repository root.

Real source tensors, generated GGUFs, reports, reference artifacts, local
registries, logs, and caches stay outside the source repository.

## How to read commands

Every command in this runbook is intended to be copied directly.

There are no shell loops.

There are no `if` blocks.

There are no `export` walls.

There are no multiline YVEX commands.

When a command depends on a local artifact, the section says so.

Path convention used below:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf
$HOME/lab/models/hf/glm/GLM-5.2
$HOME/lab/models/reports/deepseek/
$HOME/lab/models/reference/
```

For another model, quantization, or artifact, replace only the path or alias in
the command you copy.

Do not change the YVEX source repository to store real model artifacts.

## Operator storage layout

Source tensors:

```text
$HOME/lab/models/hf/<family>/<model>/
```

YVEX-produced GGUF artifacts:

```text
$HOME/lab/models/gguf/<family>/
```

External reference artifacts:

```text
$HOME/lab/models/reference/<family>/
```

Generated reports:

```text
$HOME/lab/models/reports/<family>/
```

Local registries:

```text
$HOME/lab/models/registry/
```

The source repository stays clean.

Publishing a YVEX-produced GGUF later means uploading it to an artifact
distribution target such as Hugging Face Hub, not committing the real GGUF to
the source repository.

No real `.safetensors`, `.bin`, `.dat`, or `.gguf` file belongs in the YVEX
source repository.

Tiny parser fixtures under tests are the only exception.

## Configure once lane

Use this lane once per local checkout or operator machine.

```sh
./yvex paths configure --models-root "$HOME/lab/models" --create
```

```sh
./yvex paths
```

```sh
./yvex paths resolve --family deepseek --kind source
```

```sh
./yvex paths resolve --family deepseek --kind gguf
```

```sh
./yvex paths resolve --family deepseek --kind reports
```

```sh
./yvex paths resolve --family deepseek --kind reference
```

```sh
./yvex paths resolve --family deepseek --kind registry
```

```sh
./yvex paths resolve --family glm --kind source
```

```sh
./yvex paths resolve --family glm --kind gguf
```

```sh
./yvex paths resolve --family glm --kind reports
```

```sh
./yvex paths resolve --family glm --kind reference
```

```sh
./yvex paths resolve --family glm --kind registry
```

Boundary:

```text
path configuration only
no model download
no artifact creation
no alias registration
no runtime support claim
```

## Model lanes

### DeepSeek selected embedding lane

Use this lane for the selected embedding GGUF.

Alias:

```text
deepseek4-v4-flash-selected-embed
```

YVEX-produced GGUF:

```text
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Purpose:

```text
selected token embedding materialization and selected graph slice
```

Boundary:

```text
not complete model execution
not complete transformer prefill
not decode
not logits
not generation
```

### DeepSeek selected embedding-plus-RMSNorm lane

Use this lane for the selected segment GGUF.

Alias:

```text
deepseek4-v4-flash-selected-embed-rmsnorm
```

YVEX-produced GGUF:

```text
$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf
```

Purpose:

```text
selected embedding plus RMSNorm segment execution and segment-summary prefill
```

Boundary:

```text
not complete transformer prefill
not attention-backed KV
not decode
not logits
not generation
```

### GLM-5.2 official source tensor lane

Use this lane for downloaded GLM-5.2 official safetensors.

Target:

```text
glm-5.2-official-safetensors
```

Source path:

```text
$HOME/lab/models/hf/glm/GLM-5.2
```

Purpose:

```text
huge source tensor intake, future model-class profile, future tensor mapping, future YVEX-produced GGUF, future storage-stream planning
```

Boundary:

```text
source evidence only
not GLM execution
not GLM generation
not GLM benchmark
```

### External reference artifact lane

Use this lane only for comparison artifacts not produced by YVEX.

Purpose:

```text
compare artifact layout, qtype choice, shard layout, external runner behavior
```

Boundary:

```text
not YVEX-produced artifact
not YVEX runtime execution
not YVEX benchmark
not model support
```

## Model target path reporting lanes

### DeepSeek selected embedding target paths

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
```

### DeepSeek selected embedding-plus-RMSNorm target paths

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

### GLM-5.2 official safetensors target paths

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

### Explicit model root override

Use this when checking a different operator-local model root without changing
configuration.

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths --models-root "$HOME/lab/models"
```

Boundary:

```text
target path reporting only
no safetensors inspection
no GGUF inspection
no artifact creation
no alias registration
no runtime support claim
generation: unsupported
```

## Backend lanes

### CPU lane

CPU is the reference lane.

```sh
./yvex backend cpu
```

Use CPU first when debugging model artifacts, graph proofs, and runtime
diagnostics.

### CUDA lane

Use CUDA only on CUDA-capable hosts.

```sh
./yvex cuda-info
```

```sh
./yvex backend cuda
```

CUDA backend availability is not a model capability claim.

A CUDA probe does not prove generation.

A CUDA graph primitive proof does not prove complete model execution.

## Fast regression lane

Run these after most implementation waves.

```sh
make
```

```sh
./yvex version
```

```sh
./yvex --version
```

```sh
./yvex commands
```

```sh
./yvex info
```

```sh
./yvexd --help
```

```sh
./yvex paths configure --models-root "$HOME/lab/models" --create
```

```sh
./yvex paths resolve --family deepseek --kind source
```

```sh
./yvex paths resolve --family deepseek --kind gguf
```

```sh
./yvex paths resolve --family glm --kind source
```

```sh
./yvex model-target classes
```

```sh
./yvex model-target list
```

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed
```

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
```

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm
```

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

```sh
./yvex model-target inspect glm-5.2-official-safetensors
```

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

```sh
./yvex paths
```

```sh
./yvex paths --run
```

```sh
./yvex paths --run --create
```

```sh
./yvex backend cpu
```

```sh
./yvex tokenizer tests/fixtures/gguf/valid-tokenizer-simple.gguf
```

```sh
./yvex tokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
```

```sh
./yvex detokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --ids 0,1
```

```sh
./yvex prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --user "hello" --tokens
```

```sh
./yvex input prompt --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
```

```sh
./yvex input tokens --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --tokens 0,1
```

```sh
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

Expected boundary:

```text
tokenizer fixture diagnostics pass
model-target registry is visible
operator-local paths resolve
model-target path reporting is visible
no model payload is inspected by path reporting
minimal KV diagnostics pass
no generation claim
```

## Full help lane

Use this lane when checking that the command surface is still discoverable.

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

## Source intake lanes

### DeepSeek source-to-selected-GGUF lane

Use these commands when this source directory exists:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Use model-target path reporting first to see the expected source and artifact
locations. The commands below are the low-level source-to-selected-GGUF path
until a prepare preset exists.

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
```

```sh
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
```

```sh
./yvex source-manifest create --hf-repo "deepseek-ai/DeepSeek-V4-Flash" --revision "main" --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --status in-progress --out "$HOME/lab/models/gguf/deepseek/deepseek-source-manifest.json"
```

```sh
./yvex native-weights --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
```

```sh
./yvex tensor-map --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
```

```sh
./yvex convert plan --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out-plan "$HOME/lab/models/gguf/deepseek/deepseek-selected-plan.json"
```

```sh
./yvex convert emit --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --tensor embed.weight --target-qtype F16 --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --overwrite
```

```sh
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

```sh
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

```sh
./yvex metadata "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

Expected boundary:

```text
selected embedding GGUF emitted
not complete DeepSeek GGUF
not generation
```

### GLM-5.2 source tensor lane

Use this lane only for source evidence while the GLM safetensors exist or are
downloading.

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

Current boundary:

```text
GLM is source evidence only
GLM source inventory is planned
GLM conversion is planned
GLM YVEX-produced GGUF is planned
GLM execution is unsupported
```

## Artifact registration lanes

This is the current low-level registration path. A prepare preset is planned
but not current.

### Register DeepSeek selected embedding GGUF

```sh
./yvex models remove deepseek4-v4-flash-selected-embed
```

```sh
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --alias deepseek4-v4-flash-selected-embed --support-level selected-tensor-materialized
```

```sh
./yvex models use deepseek4-v4-flash-selected-embed
```

```sh
./yvex models current
```

```sh
./yvex models list
```

```sh
./yvex models inspect deepseek4-v4-flash-selected-embed
```

```sh
./yvex models verify deepseek4-v4-flash-selected-embed
```

### Register DeepSeek selected embedding-plus-RMSNorm GGUF

Run this lane when the segment GGUF exists.

```sh
./yvex models remove deepseek4-v4-flash-selected-embed-rmsnorm
```

```sh
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf" --alias deepseek4-v4-flash-selected-embed-rmsnorm --support-level selected-tensor-materialized
```

```sh
./yvex models inspect deepseek4-v4-flash-selected-embed-rmsnorm
```

```sh
./yvex models verify deepseek4-v4-flash-selected-embed-rmsnorm
```

## Artifact inspection lanes

### DeepSeek selected embedding by alias

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
```

```sh
./yvex tensors deepseek4-v4-flash-selected-embed
```

```sh
./yvex metadata deepseek4-v4-flash-selected-embed
```

### DeepSeek selected embedding by path

```sh
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

```sh
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

```sh
./yvex metadata "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

### DeepSeek selected segment by alias

```sh
./yvex inspect deepseek4-v4-flash-selected-embed-rmsnorm
```

```sh
./yvex tensors deepseek4-v4-flash-selected-embed-rmsnorm
```

## Integrity and gate lanes

### Selected embedding integrity

```sh
./yvex integrity check --model deepseek4-v4-flash-selected-embed
```

```sh
./yvex integrity check --model deepseek4-v4-flash-selected-embed --require-token-embedding --partial-token 0
```

```sh
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
```

### Selected segment integrity

```sh
./yvex integrity check --model deepseek4-v4-flash-selected-embed-rmsnorm --require-token-embedding --partial-token 0
```

```sh
./yvex integrity report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --require-token-embedding --partial-token 0
```

### Selected embedding gates

```sh
./yvex model-gate check --model deepseek4-v4-flash-selected-embed --label deepseek-v4-flash-selected-embedding --family deepseek4 --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --report-out "$HOME/lab/models/reports/deepseek/deepseek-model-gate-cpu.txt"
```

```sh
./yvex materialize-gate check --model deepseek4-v4-flash-selected-embed --label deepseek-v4-flash-selected-embedding --family deepseek4 --scope selected-tensor --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --repeat 3 --check-cleanup --report-out "$HOME/lab/models/reports/deepseek/deepseek-materialize-gate-cpu.txt"
```

## Materialization and runtime attachment lanes

### CPU selected embedding

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
```

```sh
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
```

```sh
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
```

```sh
./yvex plan deepseek4-v4-flash-selected-embed --backend cpu --seq 1 --ctx 16
```

### CUDA selected embedding

```sh
./yvex cuda-info
```

```sh
./yvex backend cuda
```

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

```sh
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
```

```sh
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
```

### CPU selected segment

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
```

```sh
./yvex engine --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
```

```sh
./yvex session deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu
```

## Graph lanes

### Standalone graph ops, CPU

```sh
./yvex graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
```

```sh
./yvex graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
```

```sh
./yvex graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
```

```sh
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
```

```sh
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1
```

### Standalone graph ops, CUDA

```sh
./yvex graph --backend cuda --execute-op --op rope --position 7 --head-dim 8
```

```sh
./yvex graph --backend cuda --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
```

```sh
./yvex graph --backend cuda --execute-op --op matmul --m 1 --k 8 --n 8
```

```sh
./yvex graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
```

### Controlled tiny GGUF fixture

```sh
./yvex gguf-emit controlled --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --model-name yvex-controlled-fixture --arch deepseek --overwrite
```

```sh
./yvex inspect "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"
```

```sh
./yvex tensors "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"
```

```sh
./yvex graph --model "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --backend cpu --execute-fixture --fixture-token 0
```

```sh
./yvex graph --model "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf" --backend cpu --execute-fixture --fixture-token 1
```

### Controlled transformer block fixture

```sh
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
```

### Selected embedding graph

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cuda --execute-partial --partial-token 0
```

### Selected embedding-plus-RMSNorm segment graph

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1
```

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1
```

## Prefill and KV lanes

### Minimal KV diagnostics

```sh
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

### Segment-summary prefill

```sh
./yvex input tokens --model deepseek4-v4-flash-selected-embed-rmsnorm --tokens 0,1
```

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1
```

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

### CUDA segment-summary prefill

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1
```

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

## Daemon and accepted-only runtime lanes

### Daemon model status

```sh
./yvexd --model deepseek4-v4-flash-selected-embed --backend cpu --host 127.0.0.1 --port 18080 --one-request
```

### Daemon health and metrics

```sh
./yvexd --host 127.0.0.1 --port 18081 --one-request
```

```sh
./yvexd --host 127.0.0.1 --port 18082 --one-request
```

### Accepted-only chat and run diagnostics

```sh
./yvex chat --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
```

```sh
./yvex run --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --prompt "hello"
```

Boundary:

```text
accepted-only diagnostic path
not generation
not provider completion
not streaming
```

## Quant/template/intake manifest lanes

### Qtype support

```sh
./yvex qtype-support
```

### Quant policy

Use when a policy file exists.

```sh
./yvex quant-policy inspect --policy "$HOME/lab/models/reports/deepseek/policy.json"
```

```sh
./yvex quant-policy validate --policy "$HOME/lab/models/reports/deepseek/policy.json"
```

### Quant job

Use when a quant job manifest exists.

```sh
./yvex quant-job inspect --manifest "$HOME/lab/models/reports/deepseek/quant-job.json"
```

```sh
./yvex quant-job validate --manifest "$HOME/lab/models/reports/deepseek/quant-job.json"
```

### Imatrix

Use when an imatrix manifest exists.

```sh
./yvex imatrix inspect --manifest "$HOME/lab/models/reports/deepseek/imatrix.json"
```

```sh
./yvex imatrix validate --manifest "$HOME/lab/models/reports/deepseek/imatrix.json"
```

### GGUF template

Use when a GGUF template exists.

```sh
./yvex gguf-template inspect --template "$HOME/lab/models/reports/deepseek/template.json"
```

```sh
./yvex gguf-template validate --template "$HOME/lab/models/reports/deepseek/template.json"
```

```sh
./yvex gguf-template compare --template "$HOME/lab/models/reports/deepseek/template.json" --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash"
```

## Repository validation lanes

### Standard validation

```sh
git diff --check
```

```sh
make check
```

```sh
make smoke
```

```sh
sh tests/test_docs_surface.sh
```

```sh
sh tests/test_surface.sh
```

```sh
sh tests/test_source_layout.sh
```

```sh
sh tests/test_code_natural.sh
```

### CUDA validation

```sh
make check-cuda
```

### Artifact hygiene

```sh
git status --short
```

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
```

```sh
git ls-files '*.gguf'
```

Expected:

```text
no downloaded source tensors tracked
no real GGUF artifacts tracked
tiny fixtures only
```

## GLM source download status

Use these commands to inspect the GLM-5.2 source tensor download.

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

```sh
ps -p "$(cat "$HOME/lab/models/logs/glm52-safetensors-download.pid")" -o pid,stat,etime,cmd
```

```sh
tail -n 40 "$HOME/lab/models/logs/glm52-safetensors-download.log"
```

```sh
find "$HOME/lab/models/hf/glm/GLM-5.2" -maxdepth 1 -type f -name "*.safetensors" | wc -l
```

```sh
du -sh "$HOME/lab/models/hf/glm/GLM-5.2"
```

Boundary:

```text
GLM source tensors are source evidence
not GLM execution
not GLM GGUF emission
not GLM generation
```

## Full implemented command inventory

```text
backend — backend lanes
chat — daemon and accepted-only runtime lanes
commands — fast regression lane
convert — source intake lanes
cuda-info — backend lanes
detokenize — fast regression lane
engine — materialization and runtime attachment lanes
graph — graph lanes
gguf-template — quant/template/intake manifest lanes
gguf-emit — graph lanes
help — full help lane
imatrix — quant/template/intake manifest lanes
info — fast regression lane
inspect — artifact inspection lanes
input — fast regression lane, prefill and KV lanes
integrity — integrity and gate lanes
kv — prefill and KV lanes
materialize — materialization and runtime attachment lanes
materialize-gate — integrity and gate lanes
metadata — artifact inspection lanes
model-gate — integrity and gate lanes
model-target — model lanes, model target path reporting lanes, fast regression lane
models — artifact registration lanes
native-weights — source intake lanes
paths — configure once lane, fast regression lane, path resolution
plan — materialization and runtime attachment lanes
prefill — prefill and KV lanes
prompt — fast regression lane
quant-job — quant/template/intake manifest lanes
quant-policy — quant/template/intake manifest lanes
qtype-support — quant/template/intake manifest lanes
run — daemon and accepted-only runtime lanes
session — materialization and runtime attachment lanes
source-manifest — source intake lanes
tensor-map — source intake lanes
tokenize — fast regression lane
tokenizer — fast regression lane
tensors — artifact inspection lanes
version — fast regression lane
```

## Future one-command shape

Future shape:

```text
prepare model
run/chat
serve
stream
evaluate
benchmark
```

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

```text
1. ./yvex commands
2. ./yvex help <command>
3. ./yvex model-target list
4. ./yvex models current
5. ./yvex models list
6. ./yvex inspect <model-or-alias>
7. ./yvex tensors <model-or-alias>
8. ./yvex integrity check --model <model-or-alias>
9. ./yvex backend cpu
10. ./yvex cuda-info
11. rerun CPU before CUDA
12. check git status before committing
```
