# YVEX Model Artifacts

This file records external model artifacts used by YVEX validation.

Model artifacts are not committed to the repository.

## 1. Global Posture

```text
full model support: no
inference: no
generation: no
execution_ready: false
```

Current artifact support:

```text
selected tensor GGUF: yes
full model GGUF: no
full model materialization: no
real graph execution: no
```

## 2. Artifact Lifecycle

| Stage                 | Meaning                                       | Current Status |
| --------------------- | --------------------------------------------- | -------------- |
| source-manifest       | Official source tree recorded.                | complete       |
| native-inventory      | Safetensors metadata inventory recorded.      | complete       |
| conversion-plan       | Native tensor mapping/conversion plan exists. | complete       |
| selected-gguf         | Selected tensor GGUF emitted by YVEX.         | complete       |
| selected-materialized | Selected tensor materialized CPU/CUDA.        | complete       |
| full-gguf             | Full model GGUF emitted by YVEX.              | not reached    |
| full-materialized     | Full model weights materialized.              | not reached    |
| graph-executable      | Real graph path executes.                     | not reached    |
| prefill-ready         | Prefill exists.                               | not reached    |
| decode-ready          | Decode/logits exists.                         | not reached    |
| generation-ready      | Generation exists.                            | not reached    |

## 3. Support Levels

| Support Level                  | Meaning                                               | Reached |
| ------------------------------ | ----------------------------------------------------- | ------- |
| `none`                         | No usable proof.                                      | no      |
| `descriptor-only`              | GGUF parses and descriptor/tensor table can be built. | yes     |
| `selected-tensor-materialized` | Selected tensor GGUF validates and materializes.      | yes     |
| `full-weights-materialized`    | Full model weights materialize.                       | no      |
| `partial-graph-executable`     | Some real model graph path executes.                  | no      |
| `prefill-ready`                | Prefill is implemented.                               | no      |
| `decode-ready`                 | Decode/logits are implemented.                        | no      |
| `generation-ready`             | Generation path is implemented.                       | no      |

## 4. Active Target

DeepSeek V4 Flash is the active live target from M2 onward.

Qwen3-8B is retained as historical OWI/M1 evidence only.

## 5. Artifact Naming Grammar

```text
<family>-<model>-<scope>-<artifact-class>-<qprofile>-<calibration>-<producer>-<schema>.gguf
```

Example:

```text
deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Field meanings:

| Field          | Example     | Meaning                                        |
| -------------- | ----------- | ---------------------------------------------- |
| family         | `deepseek4` | Architecture/family adapter name.              |
| model          | `v4-flash`  | Model variant.                                 |
| scope          | `selected`  | Selected tensor, partial model, or full model. |
| artifact-class | `embed`     | Which artifact/tensor group the file contains. |
| qprofile       | `F16`       | Quantization/storage profile.                  |
| calibration    | `noimatrix` | Calibration/imatrix status.                    |
| producer       | `yvex`      | Produced by YVEX.                              |
| schema         | `v1`        | Naming/schema version.                         |

## 6. Artifact Summary Table

| Active     | Family    | Model    | Scope    | Artifact Class | QProfile | Calibration | Producer | Schema | Path                                                                                                  | SHA256                                                             |   Size Bytes | Tensor Count | CPU  | CUDA | Support                      | Execution |
| ---------- | --------- | -------- | -------- | -------------- | -------- | ----------- | -------- | ------ | ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------ | -----------: | -----------: | ---- | ---- | ---------------------------- | --------- |
| yes        | deepseek4 | v4-flash | selected | embed          | F16      | noimatrix   | yvex     | v1     | `/home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf` | `5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab` | `1059062080` |          `1` | pass | pass | selected-tensor-materialized | false     |
| historical | qwen3     | 8b       | selected | embed          | F16      | noimatrix   | yvex     | v1     | `/home/dgmothx/lab/models/gguf/qwen/qwen3-8b-selected-embed-F16-noimatrix-yvex-v1.gguf`               | `7a07929f6b357d293011a8224d9fa5bc4a7eb37daed1ca1cd5dfc9278b987cb9` | `1244660000` |          `1` | pass | pass | selected-tensor-materialized | false     |

## 7. DeepSeek V4 Flash - Active Artifact Card

### Identity

```text
family: deepseek4
model: v4-flash
scope: selected
artifact_class: embed
qprofile: F16
calibration: noimatrix
producer: yvex
schema: v1
```

### File

```text
path: /home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
file_bytes: 1059062080
```

### Tensor Specs

```text
format: GGUF
version: 3
tensor_count: 1
tensor: token_embd.weight
role: token_embedding
rank: 2
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
```

### Materialization

```text
CPU: pass
CUDA: pass
support_level: selected-tensor-materialized
execution_ready: false
```

### Proof Commands

```sh
cd ~/lab/yvex

export DS_GGUF="$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"

./yvex inspect "$DS_GGUF"
./yvex tensors "$DS_GGUF"
./yvex materialize --model "$DS_GGUF" --backend cpu
./yvex materialize --model "$DS_GGUF" --backend cuda
```

Model gate:

```sh
./yvex model-gate check \
  --model "$DS_GGUF" \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4096,129280 \
  --expect-dtype F16 \
  --expect-bytes 1059061760 \
  --backend cpu \
  --backend cuda \
  --require-cpu \
  --require-cuda
```

## 8. Qwen3-8B - Historical Artifact Card

Qwen3-8B remains historical OWI/M1 evidence only.

### Identity

```text
family: qwen3
model: 8b
scope: selected
artifact_class: embed
qprofile: F16
calibration: noimatrix
producer: yvex
schema: v1
```

### File

```text
path: /home/dgmothx/lab/models/gguf/qwen/qwen3-8b-selected-embed-F16-noimatrix-yvex-v1.gguf
sha256: 7a07929f6b357d293011a8224d9fa5bc4a7eb37daed1ca1cd5dfc9278b987cb9
file_bytes: 1244660000
```

### Tensor Specs

```text
format: GGUF
version: 3
tensor_count: 1
tensor: token_embd.weight
role: token_embedding
rank: 2
dims: [4096,151936]
dtype: F16
tensor_bytes: 1244659712
```

### Materialization

```text
CPU: pass
CUDA: pass
support_level: selected-tensor-materialized
execution_ready: false
```

## 9. Full Model Status

```text
Full DeepSeek V4 Flash GGUF:
  not produced by YVEX yet

Full DeepSeek V4 Flash materialization:
  not attempted

Full Qwen3-8B GGUF:
  not active

Inference:
  not implemented
```

## 10. Non-Goals

```text
no full model support claim
no inference claim
no generation claim
no benchmark claim
no execution_ready true
```
