# YVEX Inner Delivery Spine

Date: 2026-06-23
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 1. Authority

`docs/spine.md` is the only internal delivery map. Public docs must not expose
delivery IDs, delivery status rows, handoff language, or implementation diary
text.

## 2. Current Repository State

```text
root-first C source layout
native root binaries: ./yvex and ./yvexd
public headers: include/yvex/
CUDA implementation: cuda/ with C host bridge and CUDA kernel unit
GGUF domain and family mapping: gguf/
docs: docs/api.md, docs/contract.md, docs/spine.md
tests: tests/
generated output: build/
local operator state: .yvex/
```

## 3. Current Capability

Implemented:

```text
GGUF inspection
metadata and tensor table parsing
tensor/model descriptor
tokenizer fixture path
prompt rendering diagnostics
graph/planner substrate
CPU backend
CUDA tensor movement/kernel parity subset
selected-tensor GGUF emission
selected-tensor materialization
local model registry
model alias resolution for one-shot commands
model gate and materialization gate diagnostics
server/provider status shell
```

Unsupported:

```text
full model execution
prefill
decode
sampling
generation
OpenAI-compatible generation
inference benchmarks
execution_ready: true
```

## 4. Inner Delivery Spine

| ID | Status | Title |
| --- | --- | --- |
| P0 | complete | Repository reset and technical spine |
| A0 | complete | Core C skeleton and public status API |
| B0 | complete | Runtime filesystem and artifact path layer |
| C0 | complete | GGUF parser and tensor directory |
| C1 | complete | Tensor/model descriptor layer |
| D0 | complete | Tokenizer and prompt diagnostics |
| E0 | complete | Graph and planner substrate |
| F0 | complete | CPU backend and tensor movement |
| G0 | complete | Runtime/session shell |
| H0 | complete | CLI diagnostics surface |
| I0 | complete | Chat diagnostic shell |
| J0 | complete | Metrics, traces, and run artifacts |
| K0 | complete | Server/provider status shell |
| L0 | complete | CUDA backend probe and tensor movement |
| OWI.FINAL.0 | complete | Open-weight intake closeout |
| RUNTIME.KV.0 | complete | KV cache policy |
| RUNTIME.KV.1 | planned | Static KV size estimator |
| RUNTIME.KV.2 | planned | CUDA KV allocation proof |
| RUNTIME.KV.3 | planned | GPU paged KV allocator skeleton |
| RUNTIME.KV.4 | planned | Host RAM spill and cold-cache experiments |
| RUNTIME.KV.5 | planned | KV quantization policy |
| M1 | complete | Real model conversion/materialization gate |
| ARTIFACT.NAMING.0 | complete | GGUF artifact naming contract |
| M2 | complete | DeepSeek materialization hardening |
| CLI.PACKAGE.0 | complete | Repository CLI packaging baseline |
| REPO.OPERATING.0 | complete | Operating handbook and artifact cards |
| CLI.CONSOLE.0 | complete | CLI interface doctrine |
| CLI.MODELS.0 | complete | Local model selection spine |
| CLI.MODELS.1 | complete | Local model registry implementation |
| CLI.MODELS.2 | complete | One-shot model alias resolution |
| DOCS.PUBLIC.0 | complete | Public documentation boundary cleanup |
| CLI.PACKAGE.1 | complete | Minimal compiled-binary packaging baseline |
| REPO.LAYOUT.1 | complete | Root-first C source layout collapse |
| REPO.LAYOUT.2 | complete | Root source compression and native root binaries |
| DOCS.MIN.0 | complete | Minimal documentation surface |
| REPO.SURFACE.0 | complete | CUDA, GGUF, model family, and test surface refoundation |
| CUDA.SURFACE.0 | complete | CUDA kernel translation unit |
| REPO.SURFACE.1 | complete | Natural C surface and code style refoundation |
| TEST.SURFACE.0 | next | Test vectors and runner consolidation |
| CLI.MODELS.3 | planned | Model selection in canonical REPL |
| CLI.MODELS.4 | planned | Model alias resolution in yvexd |
| M3 | paused | Materialized-weight engine attachment |
| M4 | paused | First executable fixture graph path |
| M5 | paused | First real-model partial graph execution |
| M6 | paused | Prefill runtime foundation |
| M7 | paused | Decode and logits runtime foundation |
| M8 | paused | First constrained generation path |

## 5. Tracks

### Core Runtime

```text
parser/model/tokenizer/graph/backend/session shell implemented
real graph execution not implemented
generation not implemented
```

### Open Weight Intake

```text
source manifest, native inventory, template, mapping, quant policy, imatrix,
selected GGUF emission, reference purge, and closeout completed
native full-model quantization not implemented
```

### Model Support Ladder

```text
selected-tensor materialization proven
DeepSeek selected artifact is the active live target
full model materialization not reached
engine attachment paused
execution/prefill/decode/generation paused
```

### Runtime KV Cache

```text
policy complete
estimator/allocation/paged/spill/quantization planned
implementation not active
```

### CLI / Model Registry / Console

```text
root binaries complete
model registry complete
one-shot alias resolution complete
REPL selected model planned after test surface cleanup
yvexd alias resolution later
line editing later
```

### Repository Layout / Docs

```text
root-first source layout complete
root source compression complete
native root binaries complete
CUDA surface promoted to cuda/
first CUDA kernel translation unit complete
GGUF parser/tooling extracted to gguf/
family mapping consolidated into gguf/
test vector surface established
natural C source style pass complete
single root internal header complete
public documentation boundary complete
minimal docs surface complete
```

## 6. Active Next

```text
TEST.SURFACE.0 - Test vectors and runner consolidation
```

## 7. Validation Gate

```sh
make clean
make check
make smoke
make check-cuda
git diff --check
sh tests/test_docs_surface.sh
```

Additional guardrails:

```text
public docs boundary
build/bin public-doc path guard
artifact guardrail
forbidden external reference guardrail
claim guardrail
local registry guardrail
```

## 8. Non-Negotiable Rules

- No support claim without code, tests, and command proof.
- No generated model artifacts in git.
- No internal delivery IDs outside `docs/spine.md`.
- No inference or generation claim until implemented.
- No benchmark claim without benchmark implementation and proof.
- No docs sprawl beyond `docs/api.md`, `docs/contract.md`, and `docs/spine.md`.
- Keep DeepSeek as the active live model target unless this spine changes.
- Keep Qwen as historical validation evidence unless this spine changes.
