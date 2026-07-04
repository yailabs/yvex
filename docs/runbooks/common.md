# Common Operator Runbook

## What This File Is

This runbook contains model-independent YVEX operator lanes.

Use model-specific runbooks for DeepSeek or GLM work.

This file does not claim full model generation, provider generation,
evaluation, or benchmark capability.

Normal output is compact by default. List-like commands use readable tables by
default; `--output table` only forces that renderer where it is available. Use
`--audit` for row-promotion evidence and full diagnostic fields.

## Lane 0 — Fast regression after a wave

Purpose:
  Quickly check command discovery, path resolution, model-target reporting,
  target decision, full-runtime candidate, and dense candidate reporting,
  Qwen/Metal pressure reporting, fullmodel report/materialization-plan/
  materialize/descriptor/family-runtime help, attention/KV/context/MoE/
  tensor-collection report help, tokenizer fixture diagnostics, and minimal KV
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
  target decision report only
  full-runtime candidate report only
  dense candidate report only
  Qwen/Metal pressure target report only
  bounded fullmodel proof/refusal, descriptor, family-runtime, attention, KV, context, MoE, and tensor-collection diagnostics only
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
./yvex help kv
./yvex help context
./yvex help moe
./yvex help tensor-collection
./yvexd --help
./yvex paths
./yvex paths resolve --family deepseek --kind source
./yvex paths resolve --family deepseek --kind gguf
./yvex paths resolve --family glm --kind source
./yvex paths resolve --family qwen --kind source
./yvex paths resolve --family gemma --kind source
./yvex model-target classes
./yvex model-target list
./yvex model-target list --output table
./yvex model-target inspect deepseek4-v4-flash-selected-embed
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths --audit
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths --audit
./yvex model-target inspect glm-5.2-official-safetensors
./yvex model-target inspect glm-5.2-official-safetensors --paths --audit
./yvex model-target inspect qwen3-8b
./yvex model-target inspect qwen3-8b --paths
./yvex model-target class-profile qwen3-8b
./yvex model-target class-profile qwen3-8b --output table
./yvex model-target class-profile qwen3-8b --audit
./yvex model-target inspect gemma-4-12b-it
./yvex model-target inspect gemma-4-12b-it --paths
./yvex model-target decision --help
./yvex model-target decision --release v0.1.0 --output table
./yvex model-target decision --release v0.1.0 --audit --include-candidates --include-pressure-targets --include-blockers --include-critical-path --include-next
./yvex model-target candidate --help
./yvex model-target candidate --release v0.1.0 --output table
./yvex model-target candidate --release v0.1.0 --audit --include-candidates --include-pressure-targets --include-blockers --include-next
./yvex model-target dense-candidate --help
./yvex model-target dense-candidate --release v0.1.0 --output table
./yvex model-target dense-candidate --release v0.1.0 --audit --include-candidates --include-requirements --include-blockers --include-next
./yvex model-target qwen-metal --help
./yvex model-target qwen-metal --release v0.1.0 --output table
./yvex model-target qwen-metal --release v0.1.0 --audit --include-candidates --include-hardware --include-backend --include-source --include-blockers --include-next
./yvex source-manifest report --family qwen --release v0.1.0
./yvex source-manifest report --family qwen --release v0.1.0 --output table
./yvex source-manifest report --family qwen --release v0.1.0 --audit
./yvex source-manifest report --family gemma --release v0.1.0
./yvex source-manifest report --family gemma --release v0.1.0 --output table
./yvex source-manifest report --family gemma --release v0.1.0 --audit
./yvex fullmodel report --model glm-5.2-official-safetensors --backend cpu --audit
./yvex fullmodel materialization-plan --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --audit
./yvex fullmodel materialize --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --dry-run --audit
./yvex fullmodel descriptor --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu --audit
./yvex attention report --model glm-5.2-official-safetensors --family glm --backend cpu --audit
./yvex kv report --model glm-5.2-official-safetensors --family glm --backend cpu --audit
./yvex context report --model glm-5.2-official-safetensors --family glm --backend cpu --audit
./yvex moe report --model glm-5.2-official-safetensors --family glm --backend cpu --include-blockers --audit
./yvex tensor-collection report --model glm-5.2-official-safetensors --family glm --collection moe --backend cpu --include-blockers --audit
./yvex backend cpu
./yvex tokenizer tests/fixtures/gguf/valid-tokenizer-simple.gguf
./yvex tokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex detokenize tests/fixtures/gguf/valid-tokenizer-simple.gguf --ids 0,1
./yvex prompt tests/fixtures/gguf/valid-tokenizer-simple.gguf --user "hello" --tokens
./yvex input prompt --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --text "hello"
./yvex input tokens --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --tokens 0,1
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

`model-target class-profile qwen3-8b` is header-metadata-only model-class
evidence. It counts lexical tensor-name patterns from safetensors headers and
does not map tensor roles, load payloads, execute runtime paths, generate,
evaluate, benchmark, or mark a release ready.

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

## Lane 5 — Source tensor download

Purpose:
  Download source tensor trees into the operator models root through the
  YVEX-owned `models` namespace, then write source-intake sidecars.

Requires:
  Repository root.
  Provider account status checked with `yvex accounts`.
  Installed Hugging Face CLI as `hf`, or `YVEX_HF_CLI` for tests.
  Installed GitHub CLI as `gh`, or `YVEX_GH_CLI` for GitHub release assets.
  Token in `HF_TOKEN` or `GH_TOKEN` only when the upstream provider requires it.

Writes:
  Source files under `<models_root>/hf/<family>/<target>`.
  Logs under `<models_root>/logs`.
  Receipt, download report, source manifest, and native inventory under
  `<models_root>/reports/<family>`.
  Download registry sidecar under `<models_root>/registry/<family>`.

Safe to rerun:
  Dry-run is safe. Non-dry-run delegates overwrite/resume behavior to
  `hf download` and rewrites YVEX sidecars.

Stop after:
  `status: model-download-pass` and source-manifest/native-inventory stages
  pass, or after `status: model-download-dry-run` confirms the command shape.

Boundary:
  source tensors only
  no remote identity verification
  no payload hashing
  no tensor payload loading by YVEX inventory
  no conversion
  no quantization
  no GGUF emission
  no runtime artifact registration
  no materialization
  no graph/runtime execution
  no generation/eval/benchmark claim

```sh
./yvex models download gemma-4-12b-it --models-root "$HOME/lab/models" --dry-run --auth never --audit
./yvex models download gemma-4-12b-it --models-root "$HOME/lab/models" --auth auto --audit
./yvex models download gemma-4-31b-it --models-root "$HOME/lab/models" --auth auto --audit
./yvex models download qwen3-8b --models-root "$HOME/lab/models" --auth auto --audit
./yvex models download --provider github --repo OWNER/REPO --release TAG --asset "*.gguf" --models-root "$HOME/lab/models" --auth auto --audit
./yvex source-manifest report --family gemma --release v0.1.0 --audit
./yvex source-manifest report --family qwen --release v0.1.0 --audit
```

## Lane 5A - Provider accounts

Purpose:
  Inspect and connect local provider CLIs before source acquisition.

Requires:
  Repository root.
  Hugging Face CLI for Hugging Face sources.
  GitHub CLI for GitHub release assets.

Writes:
  Non-secret local account observations to `accounts.local.json` under the
  YVEX config directory.

Safe to rerun:
  Status and whoami are read-only observations. Login delegates to the provider
  CLI credential store. Ensure may report blocked in non-interactive shells.

Boundary:
  local provider account state only
  no raw token storage by YVEX
  no hosted auth service
  no custom OAuth implementation
  no MCP or YAI account integration
  no source verification
  no model execution
  no materialization
  no generation/eval/benchmark claim

```sh
./yvex accounts providers
./yvex accounts status --audit
./yvex accounts login huggingface
./yvex accounts whoami huggingface --audit
./yvex accounts login github
./yvex accounts whoami github --audit
```

## Lane 5B - Live source download

Purpose:
  Keep long source downloads visibly alive while YVEX remains the controller.

Requires:
  Repository root.
  Provider CLI and account state as in Lane 5.

Writes:
  The same source files, stdout/stderr logs, receipt, report, manifest, native
  inventory, and registry sidecars as Lane 5.

Stop after:
  `stage: download pass`, `stage: progress-stream pass`, and either
  `stage: progress-ticks pass` for long runs or `stage: progress-ticks skipped`
  for fast runs. If the operator interrupts the command, stop after
  `stage: download interrupted`, `child_signal_forwarded: true`,
  `orphan_check_status: pass`, and `status: model-download-interrupted`.

Boundary:
  live progress is based on provider pipe streaming and local source-directory
  `stat` scans only; it does not load tensor payloads, hash payloads, emit GGUF,
  materialize tensors, execute runtime work, generate, evaluate, or benchmark.
  Interrupted runs preserve partial source files and provider logs. They do not
  delete provider lock files.

```sh
./yvex models download gemma-4-12b-it \
  --models-root "$HOME/lab/models" \
  --auth required \
  --progress live \
  --tick-seconds 2 \
  --audit \
  2>&1 | tee "$HOME/lab/models/logs/yvex-gemma4-12b-it-download.live.log"

./yvex models download gemma-4-31b-it \
  --models-root "$HOME/lab/models" \
  --auth required \
  --progress live \
  --tick-seconds 2 \
  --audit \
  2>&1 | tee "$HOME/lab/models/logs/yvex-gemma4-31b-it-download.live.log"
```

## Full Implemented Command Inventory

- `accounts`: local provider account boundary
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
- `model-target`: model lanes, model target path, Qwen/Gemma source-target profiles, Qwen model-class profile, target decision, and full-runtime candidate reporting lanes, fast regression lane
- `models`: source tensor download, artifact registration, and selected prepare lanes
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
- `source-manifest`: source intake lanes, Qwen/Gemma source pressure report-only lanes
- `source-manifest report --audit`: source artifact class/status, target artifact class/status, origin/authority, provenance, sidecar, tensor container, source footprint, native safetensors header inventory, source tensor metadata inventory, and payload-read boundary fields
- `source-manifest report`: source footprint counts top-level regular files and bytes only; it does not load tensor payloads, create manifests, or imply source readiness
- `source-manifest report`: source provenance fields classify local/planned source state only; they do not perform remote lookup, hash files, prove identity, or imply source readiness
- `source-manifest report`: native safetensors inventory reads safetensors headers only; payload bytes are not loaded, malformed headers are reported, and header inventory is not runtime readiness
- `source-manifest report`: source tensor metadata inventory derives tensor names, file placement, dtype, rank, shape, byte spans, distributions, and lexical name-pattern summaries from safetensors headers only; it does not load payloads, map runtime roles, infer model classes, or imply runtime readiness
- `source-manifest report`: source manifest/provenance hardening reports manifest expectation, path/status, shallow schema/family/target consistency, and no-create/no-remote/no-hash/no-payload boundaries; it does not prove source readiness
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
4. `./yvex model-target decision --release v0.1.0`
5. `./yvex model-target candidate --release v0.1.0`
6. `./yvex model-target dense-candidate --release v0.1.0`
7. `./yvex models current`
8. `./yvex models list`
9. `./yvex inspect <model-or-alias>`
10. `./yvex tensors <model-or-alias>`
11. `./yvex integrity check --model <model-or-alias>`
12. `./yvex backend cpu`
13. `./yvex cuda-info`
14. Rerun CPU before CUDA.
15. Check `git status` before committing.
