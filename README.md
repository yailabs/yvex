# YVEX

YVEX is a native C inference engine for local open-weight models.

Local inference becomes serious when model weights stop being an opaque payload
and start behaving like runtime state. Opening a GGUF file is only the
beginning. The interesting work starts when the program can explain why an
artifact was accepted, how its tensors became executable structure, where
memory was placed, what graph work actually ran, and which state is available
for the next step of the model.

YVEX is built around that transition from file to runtime.

The project lives below chat surfaces, provider APIs, agent shells, and
benchmark tables. Those layers are useful only when the engine underneath them
is real. A local system should be able to say what artifact it used, what the
runtime understood about that artifact, how far execution advanced, and why a
later boundary is or is not ready. Otherwise the interface can look finished
while the machine path remains vague.

The current codebase already owns a concrete lower path. GGUF artifacts are
parsed through structural checks. Selected DeepSeek-class tensors become
descriptors, then backend-resident weights, then engine-owned state. Fixture
graphs give the executor a small exact world where output can be checked
directly. Real selected artifacts bring large F16 model bytes into scheduled
graph execution. The embedding path proves the first real tensor slice; the
embedding-plus-RMSNorm path adds a second tensor and a second operation.
Explicit token input gives the graph a sequence boundary, and the prefill-state
summary records the first sequence-level runtime state.

Minimal KV ownership has now entered the runtime as well. A session can own a
small KV store with explicit shape, allocation, append, read, clear, summary,
bounds, overflow, and lifecycle behavior. This is not decode and not
generation; it is the first owned attention-state boundary that later
KV-backed prefill can attach to.

That staged shape is deliberate. Transformer execution should not appear as one
dramatic jump from parser to chatbot. The lower chain has to become
inspectable first. Artifact evidence becomes runtime description. Runtime
description becomes backend residency. Backend residency becomes engine state.
Graph execution produces checked output. Token input becomes prefill state. KV
gives the session a place to preserve attention state. Decode, logits,
sampling, CLI generation, and server generation should arrive as continuations
of that same path.

GGUF is the current envelope for this work. It gives YVEX a concrete source of
model facts: architecture metadata, tensor layout, storage formats, offsets,
alignment, and tokenizer data where present. The runtime turns those file facts
into execution evidence. Registry entries, qtype policy, conversion helpers,
integrity reports, and artifact cards are part of the same machinery, because
they keep the route from open weights to local execution explicit enough to
debug.

YVEX is written in C because this layer rewards exact ownership. Memory needs a
visible owner. Cleanup needs a phase. Tensor access needs bounds. Backend
residency should not be confused with model support. A report should tell the
operator where the run stands without asking them to infer which part of the
system was real. The top of YVEX can become simple only if the bottom remains
precise.

## Why this project exists

Open-weight models have moved past the toy phase. The useful question is no
longer whether a local machine can run something that looks like a model. The
harder question is whether local hardware can run models capable enough to
matter, while keeping the runtime understandable enough to trust.

Small models are useful. They are fast, convenient, cheap to move, and often
good enough for narrow jobs. Larger open-weight models create a different
engineering problem. They preserve more reasoning ability, more coding
competence, broader knowledge, and longer-context usefulness, but they also
expose weak runtime design immediately. Memory pressure becomes architectural.
Quantization policy affects quality. KV layout becomes central. Tensor
placement and backend kernels stop being hidden details and become part of the
actual product.

YVEX is aimed at that harder side of local inference.

The project avoids treating "loaded" as a meaningful end state. A model path
can be accepted while no tensor is executable. A GGUF header can parse while a
later tensor read would be unsafe. Backend memory can exist without a graph
path that uses it. A daemon can expose status endpoints before generation
exists. Those distinctions are not cosmetic; they are where a local inference
engine becomes debuggable.

Selected DeepSeek-class artifacts are useful pressure targets because they
force the runtime to handle scale before the full transformer path is present.
A tiny fixture proves parser mechanics. A billion-byte embedding tensor proves
something else: large range accounting, backend transfer, digest checks,
cleanup behavior, and reference extraction all have to work under the same
command path. Adding RMSNorm turns the selected slice into a small real graph
segment rather than a single-tensor demonstration.

The larger goal is one coherent execution chain. Source weights become
artifact evidence. Artifact evidence becomes descriptors. Descriptors become
backend residency. Backend residency becomes engine state. Engine state feeds
graph execution. Token input becomes prefill state. Prefill state connects to
KV. KV supports decode. Decode produces logits. Sampling turns logits into
tokens. CLI and server surfaces should sit above that chain, not beside it.

Model-building belongs near the runtime for the same reason. A local engine
needs to know where an artifact came from, how native tensor names map into
runtime roles, why a qtype was chosen, what the emitted GGUF is expected to
contain, and which checks passed before execution. Without that paper trail,
speed numbers and capability evaluations lose their anchor.

## Runtime architecture

YVEX follows the path from model evidence to execution.

The path begins before the final runtime artifact. Source manifests record
where weights came from. Native tensor inventories describe the original weight
set. Family maps translate model-native tensor names into YVEX roles. GGUF then
becomes the concrete container the engine can open, validate, register, and
execute against.

From that artifact, YVEX builds descriptors. This is where parsed data becomes
executable meaning. A descriptor does more than say that a tensor exists. For
the selected embedding path, it explains how `token_embd.weight` is shaped, how
a token index selects a slice, how many output values the graph should produce,
and which byte range belongs to the operation.

Materialization is the residency boundary. CPU and CUDA paths receive selected
tensor bytes and turn them into backend-owned storage. The runtime can report
what was planned, what moved, and how cleanup behaved. Once selected weights
are resident, the engine attaches them to engine-owned state. Sessions observe
that state through a defined lifetime relationship.

Graph execution turns the loader into an engine. The controlled fixture lane
gives executor code a small exact world where output can be checked directly.
The selected real-artifact lane brings real model bytes into scheduled work.
Embedding lookup proves the first real tensor path. RMSNorm adds a second
tensor, a second operation, and an independent reference comparison.

Token input moves the system toward sequence processing. Explicit token lists
become bounded runtime objects. A selected token can drive the graph. The
prefill-state foundation records what happened across a sequence: positions,
processed tokens, graph summaries, checksums, byte accounting, and cleanup
behavior.

KV is now a real session-owned boundary. The current store is intentionally
minimal, but it proves the ownership model that later attention state needs:
shape is explicit, capacity is bounded, appends and reads are checked, clear
and close paths are tested, and overflow is a visible runtime condition.
Minimal KV-backed prefill binding now connects the segment-summary prefill
state to a small diagnostic KV store. This is still not attention-backed
transformer prefill, decode, logits, sampling, or generation.

```text
source weights
  -> artifact evidence
  -> runtime descriptor
  -> backend residency
  -> engine ownership
  -> graph execution
  -> token input
  -> prefill state
  -> KV ownership
  -> KV-backed prefill
  -> decode
  -> logits
  -> sampling
  -> generation
  -> server surface
```

## Model artifacts

YVEX keeps real model weights outside git. The repository carries code, public
headers, documentation, tiny fixtures, and tests. Large artifacts remain
operator-local.

Current artifacts are selected pressure artifacts. They sit between tiny
fixtures and future full-model GGUFs. Their purpose is to force real runtime
behavior before the whole transformer path lands.

Start by making the operator-local layout explicit:

```sh
make
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

The path commands do not download weights, create artifacts, register aliases,
or claim runtime support; they make the operator-local storage layout explicit.

| Artifact class | Role | Location | Why it matters |
| --- | --- | --- | --- |
| Native source weights | Source evidence and tensor inventory | Operator-local storage | Preserves original tensor names, shapes, and family-specific facts before GGUF emission. |
| Selected embedding GGUF | Main real-tensor pressure artifact | Operator-local storage | Exercises identity, range math, backend materialization, engine attachment, and selected embedding execution over a large F16 tensor. |
| Selected embedding plus RMSNorm GGUF | First real multi-tensor segment | Operator-local storage | Adds a second tensor and a second operation, making graph scheduling and RMSNorm reference checks concrete. |
| Controlled F32 fixtures | Deterministic parser and graph tests | Repository tests | Keeps parser, corruption, executor, and backend behavior exactly checkable while real weights stay local. |
| Future full model GGUF | Full local inference target | Operator-local storage | Becomes relevant when the runtime owns full tensor placement, transformer blocks, KV-backed prefill, decode, logits, and sampling. |
| Quantization and imatrix manifests | Provenance and calibration evidence | Operator-local or tiny test/docs fixtures | Keeps quantization decisions visible while storage recognition and compute support remain separate facts. |

The selected embedding artifact is large enough to matter. A tiny fixture
checks mechanics; a billion-byte tensor pressures range accounting, transfer
behavior, allocation limits, cleanup, and reference reads. That is why it is
useful before the full model path exists.

The embedding-plus-RMSNorm artifact adds the first normalization weight, such
as `blk.0.attn_norm.weight`, and the metadata needed to compute embedding
lookup followed by normalization. This gives the runtime a concrete multi-op
slice before attention, MLP, routed experts, logits, and sampling arrive.

[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) is the artifact card surface. It
records selected pressure artifacts, digest facts, tensor facts, and validation
posture without leaking workstation paths.

A typical selected-artifact registration looks like this:

```sh
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --alias deepseek4-v4-flash-selected-embed --support-level selected-tensor-materialized
./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
```

This remains the low-level registration path until a prepare preset exists.

The alias is a local handle for a specific artifact and support level. It gives
the operator a stable name while YVEX keeps checking file identity, metadata
drift, tensor readiness, and graph admission.

## Model-building path

YVEX treats model-building as part of the engine path. A local runtime should
know why an artifact has its layout. That answer lives in source manifests,
tensor inventories, family maps, qtype policy, conversion constraints, and GGUF
template checks.

A local inference engine cannot treat `GGUF` as a file extension only. A useful
local artifact is the result of a chain of evidence: source weights, tensor
inventory, mapping rules, template expectations, qtype decisions, and
conversion constraints. YVEX keeps that chain explicit so runtime behavior can
be traced back to artifact construction.

```text
official/open weights
  -> source manifest
  -> native tensor inventory
  -> family tensor mapping
  -> GGUF template validation
  -> selected emission
  -> local registry alias
  -> integrity report
  -> materialization
  -> engine attachment
  -> graph execution
```

Each stage answers a different question. The source manifest records
provenance. Native inventory describes the tensors before conversion. Family
mapping connects external names to runtime roles. Template validation defines
what the artifact is supposed to contain. Selected emission produces the narrow
GGUF that the current engine can exercise. Integrity and graph admission keep
later commands grounded in checked facts.

The low-level source-to-selected path is explicit:

```sh
./yvex source-manifest create --hf-repo "deepseek-ai/DeepSeek-V4-Flash" --revision "main" --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --status in-progress --out "$HOME/lab/models/gguf/deepseek/deepseek-source-manifest.json"
./yvex native-weights --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex tensor-map --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex convert plan --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out-plan "$HOME/lab/models/gguf/deepseek/deepseek-selected-plan.json"
./yvex convert emit --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --tensor embed.weight --target-qtype F16 --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --overwrite
```

This emits the selected embedding artifact only; it is not full DeepSeek
conversion and not generation.

Quantization follows the same discipline. A qtype can describe artifact storage
before backend compute support exists for a graph path. A policy can allow a
storage format while the executor still requires another representation. A
quant-job manifest can record external work without turning that work into a
runtime claim. Imatrix evidence can document calibration without pretending to
be inference.

This separation becomes more important as YVEX approaches full-model
execution. Performance and quality will depend on qtype choices, backend
kernels, attention layout, routing behavior, KV format, and logits stability.
The engine builds the paper trail early because later debugging will be much
harder without it.

## Backends and machines

Local inference is a machine problem. Memory size, storage speed, backend
ownership, copy behavior, and kernel coverage determine what the runtime can
prove.

CPU is the reference lane. CUDA is the current acceleration lane. Metal and
ROCm are future lanes because high-memory local machines matter for the kind of
inference YVEX is designed to reach.

| Machine class | Backend lane | Engine role | Public posture |
| --- | --- | --- | --- |
| CPU-only developer machine | CPU reference | Parser behavior, descriptor construction, token input, integrity checks, cleanup paths, KV ownership tests, and small graph correctness stay easy to inspect. | Reference and diagnostics lane. |
| CUDA workstation | CUDA | Current accelerated lane for selected materialization, fixture parity, selected embedding execution, embedding-plus-RMSNorm, and prefill-state summaries where CUDA is available. | Current acceleration lane. |
| DGX Spark / GB10-class CUDA machine | CUDA pressure target | Useful for larger artifacts, memory pressure, backend allocation behavior, and future graph growth. | Hardware pressure target. |
| High-memory MacBook / Mac Studio | Metal direction | Unified memory and fast local storage make this class important for future local inference. | Future backend lane. |
| Strix Halo-class system | ROCm direction | AMD unified-memory systems are relevant for future local workstation inference. | Future backend lane. |

YVEX currently claims CPU and CUDA only for the runtime surfaces that exist
today.

## Larger-model direction

YVEX is ultimately aimed at models large enough to justify local inference as
serious infrastructure.

The relevant machines are no longer only small laptops running small dense
models. High-memory workstations, Mac Studios, DGX Spark / GB10-class CUDA
systems, and future unified-memory AMD boxes create a different question: how
much of a semi-frontier model can be made resident, how expensive the KV state
becomes, which qtypes preserve quality, and where throughput collapses.

A larger model changes the runtime shape. The model may fit only under a
particular quantization profile. KV can become a first-order memory object.
Scratch space competes with resident weights. Backend placement becomes part of
the execution plan. Long-context prefill stresses a different part of the
system than token-by-token decode. Distributed execution, if it arrives, will
be mainly about capacity and long-prefill throughput before it becomes a
pleasant generation path.

YVEX prepares for that by keeping placement, ownership, and measurement
explicit. Full-model materialization is separate from selected-tensor
materialization. KV ownership is separate from prefill summaries. Decode is
separate from logits. Sampling is separate from generation. The separation is
how the runtime stays debuggable as the model class becomes harder.

For larger source-tensor pressure targets, start with path reporting:

```sh
./yvex model-target inspect glm-5.2-official-safetensors --paths
```

GLM-5.2 is currently an official-source tensor target only; this path report
does not inspect safetensors, emit a GGUF, or claim GLM execution.

## Current runtime surface

YVEX currently exposes controlled fixture graph proofs, standalone F32 graph
primitive proofs, and selected real-artifact graph slices.

The controlled lane uses tiny F32 fixtures. Its job is exactness. Fixture graph
proofs cover descriptor construction, weight attachment, backend dispatch,
output allocation, readback, cleanup, checksum behavior, and a bounded
transformer-block-shaped path. This keeps executor mechanics boring.

Standalone primitives prove RoPE, attention, matmul/projection, and MLP
operation boundaries over deterministic F32 inputs. Real model attention, real
model MLP, full layer scheduling, full transformer prefill, decode, logits,
sampling, and generation still come later.

The selected real-artifact lane uses operator-local DeepSeek-class artifacts.
The selected embedding path reads `token_embd.weight`, interprets it as
`[hidden_size, vocab_size]`, selects a token, converts the F16 row into an F32
vector, and checks the result against an independent raw-artifact reference
slice.

The embedding-plus-RMSNorm path reads the same embedding row, applies RMSNorm
using a real first-layer normalization weight, and checks the final vector by
checksum and max-diff. This is the current real-model graph segment. Real model
attention and real model MLP still come later, but the segment already
establishes multi-tensor scheduling, intermediate memory planning, backend
RMSNorm support, cleanup, and reference comparison.

Token input is explicit. `yvex input tokens` parses bounded token sequences,
validates them against vocabulary facts when available, and lets graph commands
select a token by index. Prompt text becomes executable token input when
tokenizer metadata is present and the runtime path supports it.

The prefill-state foundation runs the selected embedding-plus-RMSNorm segment
across a token sequence. It records positions, processed tokens, per-token
summaries, checksums, output bytes, and cleanup status. It sits below
attention-backed transformer prefill, but it is already runtime state rather
than plain argument parsing.

The KV command proves the new session-owned state boundary. It creates a
bounded KV store, appends values, reads them back, reports shape and byte
accounting, exercises clear behavior, and preserves the line between KV
ownership and later decode/logits work.

A short selected-artifact path:

```sh
make
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

The selected segment path:

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
```

The controlled block fixture path:

```sh
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
```

This is a controlled transformer-block-shaped fixture proof, not full model
inference.

The current prefill-state path:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2
```

The minimal KV-backed prefill binding path:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

The minimal KV proof path:

```sh
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0
```

The operator runbook contains longer transcripts, CUDA variants, fixture
proofs, expected fields, and failure-mode examples.

## Integrity and admission

YVEX treats artifact integrity as local correctness and corruption safety.

The integrity layer exists before materialization and graph execution. It
checks whether the runtime can trust the artifact facts required by the next
stage. Structural GGUF validity, tensor directory consistency, name uniqueness,
shape accounting, dtype recognition, byte-count math, tensor ranges, selected
token slices, digest identity, registry metadata drift, and graph preflight all
belong here.

The normal operator report is:

```sh
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
```

For a registered alias, the report can include digest identity, registry drift,
selected tensor readiness, materialization preflight, and graph guard status.
For a raw path, it reports current file facts directly. Backend readiness
appears when a backend is part of the command.

Admission control is useful because it stops the runtime at the right phase.
Structural problems stop before materialization. Digest drift stops before
alias assumptions are trusted. Metadata drift stops before graph dispatch.
Token range errors stop before reference extraction. The result is a local
engine that fails with evidence.

## Speed and evaluation

YVEX should publish speed numbers when the measured runtime path exists.

A useful benchmark row must identify the machine, backend, artifact, qtype,
prompt shape, context length, command, and reproducibility conditions. Prefill
and generation should be measured separately because they stress different
parts of the runtime. Long-context prefill is mostly about processing a large
token range into state. Decode is autoregressive and pays the cost of each new
token.

The public benchmark table should eventually look like this:

| Machine | Backend | Artifact / qtype | Prompt | Prefill t/s | Generation t/s |
| --- | --- | --- | --- | --- | --- |
| CPU developer machine | CPU | fixtures / selected artifacts | diagnostic tokens | correctness row | diagnostic row |
| CUDA workstation | CUDA | selected artifacts now, full GGUF later | short and medium prompts | measured after real prefill | measured after decode/logits/sampling |
| DGX Spark / GB10-class system | CUDA | large DeepSeek-class artifacts | medium and long prompts | pressure row after full prefill | pressure row after generation |
| High-memory MacBook / Mac Studio | Metal | future local GGUF path | long prompts and agent-style workloads | future row | future row |
| Strix Halo-class system | ROCm | future local GGUF path | short and long prompts | future row | future row |

Evaluation should follow the same rule. Fixture correctness is not capability
evaluation. Selected graph checks are not model quality. KV ownership tests are
not prefill quality. Logits regression becomes meaningful after a real logits
path exists. Capability regression belongs after generation uses the same
runtime path users run.

| Measurement | Runtime boundary | Purpose |
| --- | --- | --- |
| Fixture correctness | Controlled graph execution | Catches executor and backend regressions early. |
| Selected segment correctness | Real selected graph slice | Catches tensor mapping, dtype, backend, and graph-slice regressions. |
| KV ownership checks | Session-owned KV state | Catches shape, append/read, clear, overflow, lifecycle, and cleanup regressions. |
| Prefill throughput | Real transformer prefill | Measures the cost of turning prompt tokens into runtime state. |
| Decode throughput | Existing KV-backed state | Measures autoregressive next-token execution. |
| Generation throughput | Decode, logits, sampling loop | Measures user-visible local generation. |
| Capability regression | Real generation path | Detects prompt, tokenizer, logits, sampling, and output regressions. |

## Server and operator surface

`yvexd` is the current daemon boundary. It reports health, metrics, model
listing, model references, and generation status. This is useful before
generation because server shape should grow with the runtime instead of being
bolted on afterward.

Provider-backed generation should arrive when the lower path exists: KV-backed
prefill, decode, logits, sampling, streaming, and failure reporting. Until
then, the daemon remains a status and diagnostics surface.

The CLI stays explicit. Low-level commands are visible because they cross
different runtime boundaries. A future operator UX can compress common flows
into presets, but the evidence should remain available. `models`, `integrity`,
`materialize`, `engine`, `session`, `graph`, `input`, `prefill`, and `kv` each
answer a different question.

## Build and validation

The normal build is:

```sh
make
```

The standard validation gate is:

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

CUDA-capable hosts should also run:

```sh
make check-cuda
```

The artifact guardrail is:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Real model weights stay outside git. Tracked GGUF files are tiny fixtures under
`tests/`.

## Documentation

The public docs are intentionally small.

- [docs/operator-runbook.md](docs/operator-runbook.md): command-first workflow
  for artifacts, integrity, materialization, graph proofs, token input, prefill
  state, minimal KV, daemon status, and validation.
- [docs/api.md](docs/api.md): public C surface, ownership rules, report
  structs, backend capabilities, token input, prefill summaries, KV state,
  materialization summaries, graph results, and integrity surfaces.
- [docs/contract.md](docs/contract.md): runtime behavior for CLI output,
  filesystem state, registry resolution, backend boundaries, server status,
  validation, and public claims.
- [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md): selected pressure artifacts and
  validation posture.
- [AGENTS.md](AGENTS.md): repository operating rules.
- [docs/spine.md](docs/spine.md): internal delivery map, not product
  documentation.

## Acknowledgements

YVEX lives in the open-weight local inference ecosystem shaped by GGUF, GGML,
llama.cpp, CUDA tooling, and the people who kept native model execution
practical. The project is separate, but it is learning from that engineering
lineage.

## License

YVEX is licensed under the MIT license.
