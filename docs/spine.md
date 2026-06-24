# YVEX Inner Delivery Spine

Date: 2026-06-24
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 1. Authority

`docs/spine.md` is the only internal delivery map. Public docs must not expose
delivery IDs, delivery status rows, handoff language, or implementation diary
text.

Status changes in this file require code, tests, command proof, and explicit
limits. The spine must not convert parser success, tensor residency, engine
ownership, graph execution, prefill, decode, logits, sampling, or generation
into one vague inference milestone.

## 2. Current Repository State

```text
root-first C source layout
native root binaries: ./yvex and ./yvexd
public headers: include/yvex/
CUDA implementation: cuda/ with C host bridge and CUDA kernel unit
GGUF domain and family mapping: gguf/
docs: docs/api.md, docs/contract.md, docs/spine.md
public README: prose-first runtime boundary, public-safe artifact wording
artifact docs: operator-local paths only, no personal absolute paths
tests: compact runners, fixtures, and vectors
generated output: build/
local operator state: .yvex/
```

## 3. Current Capability

Implemented and audited:

```text
GGUF inspection
metadata and tensor table parsing
tensor/model descriptor construction
tokenizer fixture path
prompt rendering diagnostics
graph/planner substrate
CPU backend selected materialization
CUDA probe, tensor movement, kernel unit, and CPU/CUDA parity subset
controlled GGUF emission
selected-tensor GGUF emission
DeepSeek selected embedding alias and selected materialization
model gate and materialization gate diagnostics
model-gate pass for active selected artifact with CPU/CUDA required
materialize-gate pass for active selected artifact with repeat and cleanup
local model registry
model alias resolution for one-shot commands
qtype support separated by policy/storage/emit/quantize/compute
quant-policy fixture validation
quant-job fixture validation
imatrix fixture validation
server/provider status shell
```

Current live target:

```text
alias: deepseek4-v4-flash-selected-embed
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU materialization: pass
CUDA materialization: pass
execution_ready: false
```

Unsupported / not advanced:

```text
full model execution
full DeepSeek materialization
full GGUF conversion
canonical REPL selected-model state
yvexd model alias resolution
engine ownership of materialized selected weights
fixture graph execution
real-model partial graph execution
prefill
decode
logits-producing runtime path
sampling
generation
OpenAI-compatible generation
Anthropic-compatible generation
inference benchmarks
benchmark performance
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
| CODE.NATURAL.0 | complete | Natural translation unit rewrite |
| CODE.NATURAL.1 | complete | Final translation unit hygiene pass |
| TEST.SURFACE.0 | complete | Test vectors and runner consolidation |
| SPINE.REBASE.1 | complete | Execution-chain audit and M3-M8 technical rebase |
| CLI.MODELS.3 | next | Model selection in canonical REPL |
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
materialization produces backend-resident tensors only
engine does not yet own materialized selected weights
real graph execution not implemented
prefill/decode/logits/sampling/generation not implemented
```

### Open Weight Intake

```text
source manifest complete
native safetensors inventory complete
GGUF template validation complete
family tensor mapping complete
qtype policy complete
quant-job and imatrix manifest surfaces complete
controlled GGUF emission complete
selected GGUF emission complete
DeepSeek selected embedding materialization proven
native full-model quantization not implemented
full-model GGUF conversion not implemented
```

### Model Support Ladder

```text
controlled GGUF emission proven
selected-tensor materialization proven
DeepSeek selected embedding is the active live target
model-gate and materialize-gate pass on CPU/CUDA
full model materialization not reached
engine attachment not reached
execution/prefill/decode/generation not reached
execution_ready remains false
```

### Runtime KV Cache

```text
policy complete
estimator/allocation/paged/spill/quantization planned
implementation not active
KV ownership is not a generation claim
```

### CLI / Model Registry / Console

```text
root binaries complete
model registry complete
one-shot alias resolution complete
REPL selected-model state not implemented
chat still requires explicit --model
yvexd alias resolution not implemented
CLI.MODELS.3 remains next
CLI.MODELS.4 remains planned
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
test runners consolidated
natural C source style pass complete
natural translation unit rewrite complete
final translation unit hygiene complete
private headers scoped by backend, console, and server
run artifact helpers split from console private boundary
public documentation boundary complete
public artifact path hygiene complete
README prose-first public boundary complete
minimal docs surface complete
```

## 6. Runtime Ladder Dependencies

```text
selected artifact proof
  -> materialized backend tensor
  -> engine/session ownership
  -> fixture graph execution
  -> real partial graph execution
  -> prefill
  -> decode/logits
  -> constrained generation
```

Selected-tensor materialization is evidence of residency, not execution. M3 must
attach materialized selected weights to engine/session ownership without
implying graph execution. M4 must execute deterministic fixture graphs before
M5 touches real-model graph segments. M5 must prove real tensor participation in
scheduled computation before prefill. M6 owns prompt-token to KV/logit-producing
boundaries. M7 produces logits before any sampling boundary exists. M8 is the
first place where constrained generation can be discussed as an implemented
runtime path.

The staged ladder is:

```text
M3 — Materialized-weight engine attachment
Attach selected materialized weights to engine/session ownership without
claiming graph execution. Define ownership boundaries between artifact
descriptor, tensor table, backend tensor storage, engine state, and session
lifecycle.

M4 — First executable fixture graph path
Execute a deterministic tiny fixture graph over controlled weights. Prove graph
node ordering, backend dispatch, memory plan, output comparison, and failure
reporting on small non-model fixtures.

M5 — First real-model partial graph execution
Execute a constrained real-model graph segment against the active DeepSeek
selected artifact or another explicit partial artifact. No generation claim. No
prefill claim. The target is real tensor participation in scheduled computation.

M6 — Prefill runtime foundation
Turn prompt/token input into scheduled graph work with explicit KV/logit-
producing boundaries. Define prompt token ownership, sequence positions, KV
shape/allocation policy, graph scratch, and backend execution report.

M7 — Decode and logits runtime foundation
Execute a one-step continuation over existing runtime/KV state and produce
logits through the implemented boundary. Separate logits production from
sampling. Preserve deterministic diagnostics and failure classes.

M8 — First constrained generation path
Add the first bounded sampling loop only after logits and decode are real.
Define sampling inputs/outputs, stop conditions, token accounting,
trace/profile output, and explicit generation support boundary.
```

## 7. Active Next

```text
CLI.MODELS.3 - Model selection in canonical REPL
```

`SPINE.REBASE.1` has completed the audit/rebase checkpoint. The next
implementation work remains `CLI.MODELS.3`; do not begin `CLI.MODELS.4` or
M3-M8 work until the REPL selected-model state is implemented and validated.

## 8. Validation Gate

Baseline:

```sh
make clean
make check
make smoke
make check-cuda
git diff --check
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

Execution-chain audit set:

```sh
./yvex gguf-emit controlled --out /tmp/yvex-controlled.gguf --overwrite
./yvex inspect /tmp/yvex-controlled.gguf
./yvex tensors /tmp/yvex-controlled.gguf
./yvex materialize --model /tmp/yvex-controlled.gguf --backend cpu
./yvex materialize --model /tmp/yvex-controlled.gguf --backend cuda
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex qtype-support
```

When the operator-local selected artifact and CUDA host are available,
`model-gate` and `materialize-gate` must run against the active selected artifact
with CPU/CUDA required. `materialize-gate` should include repeat and cleanup
verification for hardening passes.

Additional guardrails:

```text
public docs boundary
build/bin public-doc path guard
artifact guardrail
forbidden external reference guardrail
claim guardrail
local registry guardrail
public path leak guardrail
```

## 9. Non-Negotiable Rules

- No support claim without code, tests, and command proof.
- No generated model artifacts in git.
- No personal absolute artifact paths in public docs.
- No internal delivery IDs outside `docs/spine.md`.
- No inference or generation claim until implemented.
- No benchmark claim without benchmark implementation and proof.
- No status promotion without command proof from the validation/audit gate.
- No M3-M8 status advance until the relevant runtime state exists in code and
  tests.
- Do not collapse materialization, engine ownership, graph execution, prefill,
  decode, logits, sampling, and generation into one wave.
- No docs sprawl beyond `docs/api.md`, `docs/contract.md`, and
  `docs/spine.md`.
- Keep DeepSeek as the active live model target unless this spine changes.
- Keep Qwen as historical validation evidence unless this spine changes.
