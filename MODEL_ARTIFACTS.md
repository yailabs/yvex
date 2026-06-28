# YVEX Model Artifacts

Model artifacts are external operator assets. They are not committed to this
repository.

Artifact paths are intentionally described as operator-local. The repository
records artifact identity, tensor facts, validation posture, and support
boundary, but not a developer workstation filesystem path.

## Policy

```text
full model support: no
full inference: no
generation: no
execution_ready: false
```

YVEX currently validates selected tensor GGUF artifacts, materializes selected
weights on CPU/CUDA, attaches them to engine/session state, and executes bounded
selected embedding and selected embedding-plus-RMSNorm graph proofs. Full model
GGUF emission, full materialization, prefill, decode, logits, sampling, and
generation are outside the current artifact cards.

## Active Artifact

DeepSeek V4 Flash selected embedding GGUF:

```text
local_path: operator-local, outside repository
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
file_bytes: 1059062080
format: GGUF v3
tensor_count: 1
tensor: token_embd.weight
rank: 2
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
support: selected-tensor-materialized
CPU: pass
CUDA: pass
execution_ready: false
```

Proof:

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

## Historical Validation Artifact

Qwen3-8B selected embedding GGUF:

```text
local_path: operator-local, outside repository
sha256: 7a07929f6b357d293011a8224d9fa5bc4a7eb37daed1ca1cd5dfc9278b987cb9
file_bytes: 1244660000
format: GGUF v3
tensor_count: 1
tensor: token_embd.weight
rank: 2
dims: [4096,151936]
dtype: F16
tensor_bytes: 1244659712
support: selected-tensor-materialized
CPU: pass
CUDA: pass
execution_ready: false
```

This artifact remains historical validation evidence only. DeepSeek is the
active live model target.

## Unsupported

```text
Full DeepSeek V4 Flash GGUF: not produced by YVEX yet
Full DeepSeek materialization: not attempted
Full Qwen3-8B GGUF: not active
Full inference path: not available
Generation: not available
Benchmarks: none
```
