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

## GLM Paths And Target

Target:

```text
glm-5.2-official-safetensors
```

Expected source path:

```text
$HOME/lab/models/hf/glm/GLM-5.2
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
  not GLM runtime execution
  not GLM generation

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

## Lane 2 — GLM Download Status

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
