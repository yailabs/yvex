# Common Operator Runbook

## What This File Is

This runbook contains model-independent YVEX operator lanes.

Use model-specific runbooks for DeepSeek or GLM work.

This file does not claim full model generation, provider generation,
evaluation, or benchmark capability.

## Lane 0 — Fast regression after a wave

Purpose:
  Quickly check command discovery, path resolution, model-target reporting,
  fullmodel report/materialization-plan/materialize/descriptor/family-runtime
  help, attention report help, tokenizer fixture diagnostics, and minimal KV
  diagnostics.

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
  bounded fullmodel proof/refusal, descriptor, family-runtime, and attention diagnostics only
  no uncontrolled full backend allocation
  no daemon
  no generation claim

```sh
make
./yvex version
./yvex --version
./yvex commands
./yvex info
./yvex help generate
./yvex help fullmodel
./yvex help attention
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
./yvex fullmodel report --model glm-5.2-official-safetensors --backend cpu
./yvex fullmodel materialization-plan --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
./yvex fullmodel materialize --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --dry-run
./yvex fullmodel descriptor --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
./yvex attention report --model glm-5.2-official-safetensors --family glm --backend cpu
./yvex backend cpu
./yvex tokenizer tests/fixtures/gguf/valid-tokenizer-simple.gguf
./yvex tokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex detokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --ids 0,1
./yvex prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --user "hello" --tokens
./yvex input prompt --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex input tokens --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --tokens 0,1
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

## Lane 1 — Graph-only regression

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

## Lane 2 — Daemon and accepted-only diagnostics

Purpose:
  Check daemon status surface and accepted-only diagnostic chat/run commands.

Requires:
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
./yvexd --host 127.0.0.1 --port 18081 --one-request
./yvexd --host 127.0.0.1 --port 18082 --one-request
./yvex chat --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
./yvex run --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --prompt "hello"
```

## Lane 3 — Full repository validation and hygiene

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

## Lane 4 — Low-level command atlas

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
  not a replacement for model-specific runbooks
  no model check claim
  no graph check claim
  no generation claim

Full help:

```sh
./yvex help backend
./yvex help attention
./yvex help chat
./yvex help commands
./yvex help convert
./yvex help cuda-info
./yvex help detokenize
./yvex help engine
./yvex help fullmodel
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

## Full Implemented Command Inventory

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
- `models`: artifact registration and selected prepare lanes
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

## Manual Debug Fallback

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
