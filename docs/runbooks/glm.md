# GLM Operator Runbook

## What This File Is

This runbook is the GLM-5.2 source-target operator path for YVEX.

GLM-5.2 is currently official-source tensor evidence only.

This file does not describe GLM execution.

This file does not describe GLM generation.

This file does not describe GLM GGUF emission.

YVEX uses GLM as a huge-MoE source-tensor pressure target for future inventory,
model-class profiling, tensor mapping, quantization policy, YVEX-produced GGUF
planning, storage layout, and storage-stream planning.
`yvex fullmodel report --model glm-5.2-official-safetensors` currently reports
the source-only unsupported boundary without inspecting GLM safetensors.
`yvex fullmodel materialization-plan --model glm-5.2-official-safetensors`
refuses as source-only because materialization planning needs a GGUF tensor
inventory. `yvex fullmodel materialize --model glm-5.2-official-safetensors`
also refuses as source-only and does not inspect GLM safetensors. `yvex
fullmodel descriptor --model glm-5.2-official-safetensors` reports the same
source-only runtime descriptor boundary without inspecting GLM safetensors.
`yvex fullmodel family-runtime --model glm-5.2-official-safetensors --family
glm` also refuses as source-only and does not inspect GLM safetensors.
`yvex attention report --model glm-5.2-official-safetensors --family glm`
uses the same source-only refusal boundary: no GLM safetensors inspection and no
GLM attention/runtime claim.
`yvex kv report --model glm-5.2-official-safetensors --family glm` uses the
same source-only refusal boundary: no GLM safetensors inspection and no GLM KV
runtime claim.
`yvex context report --model glm-5.2-official-safetensors --family glm` uses
the same source-only refusal boundary: no GLM safetensors inspection and no GLM
runtime context claim.
`yvex moe report --model glm-5.2-official-safetensors --family glm` uses the
same source-only refusal boundary: no GLM safetensors inspection and no GLM MoE
runtime claim.
`yvex tensor-collection report --model glm-5.2-official-safetensors --family
glm --collection moe` uses the same source-only refusal boundary: no GLM
safetensors inspection, no tensor materialization, and no GLM MoE runtime
claim.

## GLM Paths And Target

Target:

```text
glm-5.2-official-safetensors
```

Expected source path:

```text
$HOME/lab/models/hf/glm/GLM-5.2
```

Source repository:

```text
zai-org/GLM-5.2
```

Report path:

```text
$HOME/lab/models/reports/glm/
```

Reference path:

```text
$HOME/lab/models/reference/glm/
```

## Lane 0 — Configure GLM Operator Storage

Purpose:
  Configure the operator-local root and show GLM target paths.

Requires:
  Repository root.

Writes:
  `.yvex/operator-paths.conf`
  `$HOME/lab/models` directory tree

Safe to rerun:
  yes

Stop after:
  GLM target path is visible.

Boundary:
  no model download
  no artifact creation
  no alias registration
  no runtime support claim

```sh
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex paths resolve --family glm --kind source
./yvex paths resolve --family glm --kind gguf
./yvex paths resolve --family glm --kind reports
./yvex paths resolve --family glm --kind reference
./yvex paths resolve --family glm --kind registry
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

## Lane 1 — GLM Source-Target Status

Purpose:
  Check GLM target path reporting.

Requires:
  Repository root.

Writes:
  No YVEX artifact.

Safe to rerun:
  yes

Stop after:
  GLM source path and planned artifact boundary are visible.

Boundary:
  GLM source evidence only
  not GLM inventory completion
  not GLM tensor mapping
  not GLM GGUF emission
  not GLM materialization planning over source-only safetensors
  not GLM materialization proof over source-only safetensors
  not GLM runtime descriptor over GGUF tensors
  not GLM family-runtime adapter over GGUF tensors
  not GLM attention class support over source-only safetensors
  not GLM KV cache support over source-only safetensors
  not GLM context support over source-only safetensors
  not GLM runtime execution
  not GLM generation

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
./yvex fullmodel report --model glm-5.2-official-safetensors --backend cpu
./yvex fullmodel materialization-plan --model glm-5.2-official-safetensors --backend cpu
./yvex fullmodel materialize --model glm-5.2-official-safetensors --backend cpu
./yvex fullmodel descriptor --model glm-5.2-official-safetensors --backend cpu
./yvex fullmodel family-runtime --model glm-5.2-official-safetensors --family glm --backend cpu
./yvex attention report --model glm-5.2-official-safetensors --family glm --backend cpu
./yvex kv report --model glm-5.2-official-safetensors --family glm --backend cpu
./yvex context report --model glm-5.2-official-safetensors --family glm --backend cpu
./yvex moe report --model glm-5.2-official-safetensors --family glm --backend cpu --include-blockers
./yvex tensor-collection report --model glm-5.2-official-safetensors --family glm --collection moe --backend cpu --include-blockers
```

## Lane 2 — Start GLM Source Download

Purpose:
  Start an external GLM safetensors download into operator-local storage.

Requires:
  Repository root.
  Hugging Face `hf` CLI installed and authenticated if the local environment
  requires a token.

Writes:
  `$HOME/lab/models/hf/glm/GLM-5.2`
  `$HOME/lab/models/logs/glm52-safetensors-download.log`
  `$HOME/lab/models/logs/glm52-safetensors-download.pid`

Safe to rerun:
  yes, after confirming no previous GLM download is still active.

Stop after:
  download process, log, and target path are visible.

Boundary:
  external source download only
  no YVEX artifact creation
  not GLM inventory completion
  not GLM GGUF emission
  not GLM runtime execution
  not GLM generation

Use `hf`, not the deprecated `huggingface-cli`. The GLM-5.2 source target uses
`zai-org/GLM-5.2`; `THUDM/GLM-5.2` is not the repository ID for this target.

```sh
export YVEX_MODELS="$HOME/lab/models"
export HF_BIN="${HF_BIN:-hf}"
export GLM_HF_REPO="zai-org/GLM-5.2"

TARGET="$YVEX_MODELS/hf/glm/GLM-5.2"
PID_FILE="$YVEX_MODELS/logs/glm52-safetensors-download.pid"
LOG_FILE="$YVEX_MODELS/logs/glm52-safetensors-download.log"

mkdir -p "$TARGET" "$YVEX_MODELS/logs"

nohup "$HF_BIN" download \
  "$GLM_HF_REPO" \
  --include "*.safetensors" \
  --local-dir "$TARGET" \
  > "$LOG_FILE" 2>&1 &

echo $! > "$PID_FILE"

sleep 5
tail -n 40 "$LOG_FILE"
ps -p "$(cat "$PID_FILE")" -o pid,stat,etime,cmd
```

If the log says `Repository not found`, first verify `GLM_HF_REPO` is
`zai-org/GLM-5.2` and then check local Hugging Face authentication or network
access. That failure happens before YVEX sees any source artifact.

If the log says `huggingface-cli` is deprecated, rerun this lane with `hf`.

## Lane 3 — GLM Download Status

Purpose:
  Inspect a GLM source download that has already been started outside YVEX.

Requires:
  Existing GLM download pid/log files.

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

## Current GLM Boundary

Current GLM state is source-target evidence only.

YVEX can:

- report GLM target paths;
- record GLM as an official-source huge-model target;
- refuse source-only fullmodel family-runtime reporting without inspecting
  safetensors;
- refuse source-only attention reporting without inspecting safetensors;
- refuse source-only KV reporting without inspecting safetensors;
- refuse source-only context reporting without inspecting safetensors;
- refuse source-only MoE tensor collection reporting without inspecting
  safetensors;
- plan future source inventory, model-class profiling, tensor mapping,
  quantization policy, YVEX-produced GGUF emission, and storage-stream work.

YVEX does not currently implement:

- GLM source tensor inventory completion;
- GLM tensor mapping;
- GLM quantization policy;
- GLM YVEX-produced GGUF;
- GLM runtime execution;
- GLM full materialization;
- GLM storage-stream execution;
- GLM generation;
- GLM benchmarks.
