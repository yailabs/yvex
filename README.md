# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
It works below chat interfaces, provider APIs, and sampling, at the layer where
a model file stops being a blob on disk and starts becoming accountable runtime
state: parsed metadata, tensor roles, backend allocations, engine/session
ownership, and scheduled graph work.

The project exists because the open-weight stack often hides too much behind a
few convenient words: loaded, supported, compatible, running. In practice,
those words cover many different states. A `GGUF` can parse while its tensors
map to the wrong runtime roles. A tensor can be resident on `CUDA` without any
transformer work being scheduled. A daemon can expose provider-shaped endpoints
before the engine behind it can produce logits. YVEX makes those boundaries
explicit before claiming the next one.

The current code does not run a full model. It does, however, cross more than
the old parse-and-materialize boundary. YVEX can inspect `GGUF` artifacts,
materialize selected tensors on `CPU` and `CUDA`, attach those tensors to
engine-owned runtime state, execute a deterministic fixture graph over controlled
`F32` weights, and execute a real selected embedding segment over `F16`
`token_embd.weight`. The next runtime step is expanding real-model graph
segments. Prefill, decode, logits, sampling, generation, and provider generation
are not implemented.

That boundary is deliberate. YVEX is being built from the lower runtime upward,
so that later inference work has something concrete to stand on: artifact
identity, tensor identity, backend residency, runtime ownership, executable
graph proof, selected embedding execution, and only then larger real model
execution.

## What YVEX is

YVEX focuses on the native path that most local-model tools compress into words
like `loaded` or `supported`. In this project, a file path, a parsed `GGUF`, a
descriptor, a resident tensor, an engine-owned weight table, and an executable
graph are separate runtime states. Each state must be visible enough to test,
fail, and explain.

That separation is the project's core. YVEX is not a generic model-serving
product today, and it is not trying to make an incomplete runtime look like a
chat system. It is the lower layer that must become true before a model-serving
surface can be honest: files must open, tensor rows must map to roles, selected
bytes must land in owned backend storage, engine and session lifetimes must
release resources correctly, and graph execution must produce checkable outputs
before text generation is exposed.

The public command surface is broad because the runtime is being built one
boundary at a time. `inspect`, `metadata`, and `tensors` read artifact
structure. `models` names external local artifacts without committing them.
`backend` and `cuda-info` report machine state. `materialize`, `engine`, and
`session` show residency and ownership. `graph` now has a controlled execution
path. `run`, `chat`, and `yvexd` remain diagnostic surfaces until the runtime
can produce real model output.

## Why this exists

Local inference is full of overloaded words. "Loaded" can mean a path exists,
a file opened, metadata parsed, tensors mapped, weights resident on a backend,
a graph scheduled, or a server accepting requests. Those are not the same
thing. When a local runtime fails, the difference between those states is where
the useful diagnosis lives.

YVEX takes the slow route on purpose. The project first makes artifact identity
concrete, then tensor metadata, then descriptor construction, then selected
materialization, then backend residency, then engine ownership, then controlled
graph execution. Each step has to leave behind evidence: command output, tests,
cleanup behavior, and an honest unsupported boundary.

That is the gap YVEX is trying to occupy. There are many projects that can
expose a prompt box, call a backend, or wrap a provider-shaped API. YVEX is
aimed lower: the transition from artifact bytes to runtime facts, and from
runtime facts to scheduled work. It should be possible to ask where a model
failed and get a narrower answer than "loading failed" or "inference failed."

That approach is not meant to be academic. It is what lets the runtime grow
without lying to itself. A selected embedding tensor materialized on `CUDA` is
valuable, but it is not a transformer. A deterministic fixture graph is
valuable, but it is not real-model inference. A selected embedding segment is
valuable, but it is still not generation. The goal is to earn each word before
using it.

## Project posture

YVEX is intentionally narrow at each step and broad in the path it is preparing.
The current implementation is not a complete inference engine, but it is not a
throwaway inspector either. It is building the sequence of runtime states that a
local inference engine has to own if it wants to be debuggable: artifact
identity, tensor identity, backend residency, engine ownership, graph execution,
prefill, `KV`, decode, logits, sampling, and provider serving.

The project will remain opportunistic about model families. DeepSeek V4 Flash is
the current pressure target because it is large enough to force real artifact
and backend behavior now. A future model can replace that target if it better
fits the hardware profile or research path, but model-family support must be
earned through source provenance, tensor mapping, runtime execution, and tests.

## What YVEX is not

| YVEX is not | Why |
| --- | --- |
| a generic `GGUF` runner | arbitrary `GGUF` execution is not implemented |
| a chat UI | `chat` is diagnostic until real generation exists |
| a provider-compatible generation server | `yvexd` is currently a status shell |
| a benchmark leaderboard | no token/sec or model-quality numbers are published before prefill/decode/generation exist |
| a model artifact repository | real weights and generated `GGUF`s stay operator-local |

## Current state

| Area | Current state | What it proves | What it does not prove |
| --- | --- | --- | --- |
| GGUF parse | implemented | metadata and tensor directory can be read | model execution |
| Descriptor and tensor roles | implemented | artifact facts become runtime descriptors | backend residency |
| Selected materialization | implemented | selected tensor bytes move into `CPU`/`CUDA` storage | full model loading |
| Engine weight attachment | implemented | selected materialized weights are engine-owned state | graph execution |
| Fixture graph execution | implemented for controlled `F32` fixtures | one deterministic graph path runs through backend dispatch | real-model inference |
| Real-model partial graph | implemented for selected embedding segment | first real selected tensor graph segment | not prefill, decode, logits, or generation |
| Prefill, decode, logits | planned | future runtime stages | not implemented |
| Generation and server generation | unsupported | no text generation path exists | provider endpoints are status-only |

The important new line after fixture execution is the selected embedding segment.
YVEX can now move from owned real tensor state to scheduled work: an embed node
reads the selected `F16` `token_embd.weight`, dispatches through the selected
backend, writes an `F32` output buffer, and compares it with a raw-artifact
reference slice. It is still deliberately not a full model graph.

## Execution boundary

Parsing gives YVEX the artifact's declared structure: GGUF header, metadata,
tensor names, shapes, dtypes, byte offsets, and tokenizer facts where present.
Descriptor construction is the next boundary; it turns file facts into a
runtime view that later commands can reason about. Selected materialization
crosses a different line: the runtime is no longer only describing tensor
bytes, it is moving a chosen tensor into `CPU` or `CUDA`-owned storage and forcing
allocation, transfer, cleanup, and error reporting to become explicit.

Backend residency is still not graph execution. A tensor resident on `CUDA` has
crossed a memory boundary, not a transformer boundary. The runtime also has to
know who owns the weights, who can borrow them, and which cleanup path releases
them. YVEX can now attach selected materialized weights to engine-owned runtime
state, and sessions can observe that state without owning or freeing it.

YVEX can also execute the first controlled graph boundary. The graph is
intentionally small, but it is real scheduled work: the engine opens the
artifact, attaches selected weights, dispatches an embed node through the
backend, allocates output, reads back values, and reports stable output
identity. That is graph execution for a fixture. It is not real-model
inference.

The current real-model boundary is the selected embedding segment. The next
boundary is real-model graph segment expansion: more than one real op must
participate in scheduled runtime computation with real attached tensors, memory
planning, backend dispatch, regression output, cleanup, and failure tests. It
still will not be prefill, decode, logits, sampling, or generation.

This is the reason the project is useful before full inference exists: it gives
each future inference stage a concrete predecessor instead of a vague promise.

## What runs today

The current runtime has two different executable proof paths, and they should
not be confused.

The two paths are complementary: the selected-artifact path proves that YVEX can
carry real model bytes through ownership boundaries and into a constrained
embedding segment; the controlled fixture path proves that YVEX can execute a
small graph exactly.

The normal operator path works with the selected DeepSeek embedding artifact.
It proves that YVEX can carry one real model tensor from operator-local artifact
identity through `GGUF` parsing, descriptor construction, selected
materialization, backend residency, engine-owned attachment, and selected
embedding execution. That path is the one used by the model registry,
`materialize`, `engine`, `session`, `graph --execute-partial`, daemon status,
and the selected-artifact gates.

The controlled fixture path is different. It uses a tiny controlled `F32`
`GGUF` so graph execution can be checked exactly. That path proves the executor
can read attached tensor data, dispatch an embed node through the selected
backend, write an output buffer, and report stable values and checksums. It is
real graph execution for a controlled fixture, not real-model inference.

### Canonical selected-artifact path

The canonical selected-artifact flow is documented in
[docs/operator-runbook.md](docs/operator-runbook.md). The short version is:

```sh
make
./yvex models add \
  --path /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
  --alias deepseek4-v4-flash-selected-embed \
  --support-level selected-tensor-materialized

./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
```

Materialization and engine/session attachment then prove backend residency and
runtime ownership:

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
```

`CUDA`-capable hosts can run the same boundary on `CUDA`:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
```

Expected status remains explicit:

```text
weights_attached: true
weights_backend: cpu or cuda
weight_tensor_count: 1
weight_total_bytes: 1059061760
execution_ready: false
graph_execution_ready: false
```

The engine owns the attached backend tensors. The session observes that state;
it does not own the weights and does not execute a transformer graph.

The selected embedding segment executes the first real-model partial graph path:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --execute-partial \
  --partial-token 0
```

For token `0`, the selected artifact reports a real `F16` embedding segment
summary:

```text
real_partial_graph_executed: true
partial_graph_kind: token-embedding
partial_backend: cpu
partial_weight: token_embd.weight
partial_weight_dtype: F16
partial_output_dtype: F32
partial_output_count: 4096
partial_output_bytes: 16384
partial_max_abs_diff: 0
execution_ready: false
graph_execution_ready: false
status: real-partial-graph-executed
```

`CUDA`-capable hosts can run the same selected embedding segment with
`--backend cuda`; the output checksum, reference checksum, and sample values
should match the `CPU` path. This is real selected tensor participation in
scheduled graph work. It is not prefill, `KV`, decode, logits, sampling,
generation, or a transformer backend.

### Deterministic fixture graph proof

The fixture graph is the current graph-execution proof. It is intentionally
small and deterministic. The operator runbook keeps the controlled fixture
under an operator-owned `GGUF` directory, not in a temporary path, because it is
useful to keep the proof artifact named and repeatable.

```sh
./yvex gguf-emit controlled \
  --out /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --model-name yvex-m4-deepseek-fixture \
  --arch deepseek \
  --overwrite

./yvex graph \
  --model /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --backend cpu \
  --execute-fixture \
  --fixture-token 0
```

For token `0`, the current controlled fixture reports:

```text
fixture_graph_executed: true
fixture_backend: cpu
fixture_op: embed
fixture_weight: token_embd.weight
fixture_output_values: 0,4,8,12
execution_ready: false
graph_execution_ready: false
status: fixture-graph-executed
```

Token `1` should produce:

```text
fixture_output_values: 16,20,24,28
```

`CUDA`-capable hosts can run the same fixture proof with `--backend cuda`; the
`CUDA` output values should match the `CPU` fixture output. This is fixture
graph parity only. It is not a `CUDA` transformer backend.

| Path | Artifact | What it proves | What it does not prove |
| --- | --- | --- | --- |
| Selected-artifact path | DeepSeek selected `F16` embedding | real artifact identity, selected materialization, backend residency, engine attachment, selected embedding segment execution | full model graph, prefill, logits, or inference |
| Controlled fixture path | DeepSeek-arch `F32` fixture `GGUF` | deterministic graph execution, backend dispatch, output readback | real-model graph expansion or logits |

## Live artifacts and fixtures

YVEX currently uses two different kinds of artifacts. The selected `F16`
DeepSeek embedding is the canonical pressure artifact. The controlled `F32`
`GGUF` is the deterministic execution proof artifact. They serve different
purposes and should not be confused.

### Live pressure artifact

The live external pressure target is a selected DeepSeek V4 Flash embedding
GGUF documented in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md). The public docs
record artifact identity and tensor facts without publishing a developer
workstation path.

```text
alias: deepseek4-v4-flash-selected-embed
local_path: operator-local, outside repository
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
format: GGUF v3
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU: pass
CUDA: pass
execution_ready: false
```

The selected DeepSeek embedding is not "supported DeepSeek." It is the current
pressure artifact: one very large real tensor artifact that forces artifact
identity, shape, dtype, byte accounting, `CPU`/`CUDA` residency, engine
attachment, and cleanup to be real before a full graph exists. One tensor is
enough to make memory ownership and backend failure behavior concrete. It is
not enough to claim a model run.

The selected DeepSeek artifact is useful precisely because it is both partial
and heavy.
`token_embd.weight` at `[4096,129280]` in `F16` is about one billion tensor
bytes. That size is large enough to exercise long local artifact names,
checksum identity, `GGUF` v3 tensor directory layout, shape and dtype
preservation, byte accounting, `CPU`/`CUDA` allocation, backend cleanup, engine
attachment, and repeatable gate reporting.

DeepSeek is the current target because it creates useful pressure now. YVEX
should remain able to move to another open-weight model family when hardware,
model quality, or research direction changes. A new family requires explicit
source provenance, tensor mapping, artifact identity, runtime work, and tests.

### Controlled fixture artifact

The controlled `F32` fixture remains the exact execution artifact for the
deterministic graph proof path. It is small by design. It exists so executor and
backend regressions can be checked exactly beside the real selected embedding
segment.

| Artifact | Role | Current state |
| --- | --- | --- |
| DeepSeek selected `F16` embedding | canonical pressure artifact | inspect/materialize/attach pass; selected embedding partial graph pass; used by registry, engine/session, daemon status, and gates |
| Controlled DeepSeek-arch `F32` fixture | deterministic graph proof artifact | `CPU`/`CUDA` fixture graph pass; not a real model graph |

## Artifact workflow

The repository should be able to travel without the operator's model
directory. Real `GGUF`s, native safetensors, generated quantization outputs,
local registries, logs, reports, and build artifacts stay on the machine that
owns them. The repository keeps source, public headers, docs, tiny fixtures,
tests, and contracts. That separation is not just cleanliness; it prevents a
public runtime project from becoming a dump of local state.

| Class | Location |
| --- | --- |
| Source code, public headers, docs, tiny fixtures | repository |
| Real `GGUF` model artifacts | operator-local storage |
| Native safetensors / raw weights | operator-local storage |
| Generated `GGUF`s and quantization outputs | operator-local storage |
| `.yvex/models.local.json` | local ignored state |
| Build output, reports, logs | local/generated state |

YVEX includes a baseline artifact integrity check for `GGUF` structural bounds,
tensor ranges, checked byte math, and selected embedding readiness. It is not a
supply-chain security guarantee.
Those integrity checks include canonical tensor byte-range validation before
materialization or graph execution reads payload data.
They also separate storage dtype recognition from runtime compute support. For
example, `F16 token_embd.weight` is supported for the selected embedding segment,
not for full-model inference.

The repository also includes tiny corrupt `GGUF` fixture coverage for structural
refusal paths. Those files are test assets, not model artifacts.

Registered model aliases also record local file identity so YVEX can detect
when an operator-local artifact changes after registration. Digest checks are
local identity evidence, not supply-chain security or remote provenance.
Registry verification also compares recorded alias metadata against current
artifact facts, so digest identity and tensor metadata drift are reported as
different local operator signals.

Materialization is integrity-gated: structural, identity, metadata, shape, and
range checks run before backend allocation where possible, and allocation or
transfer failures report cleanup status.
Graph execution is integrity-guarded for the implemented fixture and selected
embedding partial paths: preflight runs before backend dispatch and raw
reference reads.
Operators can ask YVEX for a single local integrity report that summarizes
artifact structure, digest identity, registry metadata drift, selected embedding
readiness, materialization preflight, and graph-entry guard status.
The test suite includes a consolidated artifact-integrity regression harness for
structural corruption, identity drift, metadata drift, materialization preflight,
and graph guard refusal paths.
Artifact integrity is now a closed prerequisite module for broader graph
expansion. These checks are local runtime boundaries, not supply-chain security
or inference readiness.

The local registry exists because real artifact paths are long and
machine-specific. `.yvex/models.local.json` is ignored local state, and
`YVEX_MODELS_REGISTRY` can point commands at another local registry when a
machine needs it. An alias removes path friction, but it does not move the
artifact to a new runtime state. The command that uses the alias still has to
parse, inspect, materialize, attach, execute a fixture, or gate the artifact at
the requested boundary.

Tiny GGUF fixtures are different from real model artifacts. Fixtures belong in
tests because they validate parser edges, malformed headers, offset handling,
tokenizer metadata, tensor ranks, layout failures, and controlled graph
execution without claiming to be model inventory. Real model artifacts are
external operator assets.

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex models use local-model
./yvex inspect local-model
./yvex tensors local-model
```

`materialize`, `model-gate`, and `materialize-gate` remain low-level proof and
debug surfaces. They are useful for CI, selected-artifact checks, and exact
failure reports. They should not be mistaken for the future normal operator
path for preparing a model.

## GGUF intake and model-family mapping

GGUF is the artifact envelope, not the execution engine. Reading it gives YVEX
metadata, tensor directory rows, names, shapes, dtypes, offsets, and tokenizer
metadata where present. Those facts feed descriptors, family mapping, selected
emission, materialization, engine attachment, and eventually graph execution.
They do not become a runnable transformer until they are connected to scheduled
ops, backend kernels, scratch, KV, logits, decode, and sampling.

YVEX keeps source provenance next to GGUF work because a local runtime needs
more than the final container. The source manifest records where official
weights came from. Native safetensors inventory gives a payload-free view of
source tensors. Tensor mapping is where family-native names become YVEX roles
and proposed GGUF names. Template validation catches structural expectations
before conversion. Selected conversion keeps the scope narrow: one explicit
family, source, tensor, qtype, and output artifact.

```text
official/open weights
  -> source manifest
  -> native safetensors inventory
  -> family tensor mapping
  -> GGUF template validation
  -> selected emission
  -> selected materialization
  -> engine attachment
  -> fixture / future real graph execution
```

The command shape follows that lineage. The examples below are about
inventory, mapping, planning, and selected emission; they are not a full-model
conversion recipe.

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/native/source \
  --status in-progress \
  --out /path/to/source-manifest.json

./yvex native-weights --source /path/to/native/source
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source
./yvex convert plan --arch deepseek4 --native-source /path/to/native/source --out-plan /tmp/yvex-plan.json
./yvex convert emit --arch deepseek4 --native-source /path/to/native/source --tensor model.embed_tokens.weight --target-qtype F16 --out /tmp/yvex-selection.gguf --overwrite
```

Selected conversion is not full conversion. Family mapping is not runtime
support. A registered artifact is not execution readiness. A generated GGUF
still has to pass the later runtime boundaries.

## Quantization and qtype boundaries

Quantization in YVEX is a set of separate runtime facts, not a single
"supported" bit. A qtype can be recognized as artifact storage while no backend
kernel computes with it. A policy can allow or reject a qtype for an artifact
shape. A selected emission path can write a requested qtype. An external
quantization job or imatrix manifest can record how an artifact was produced.
Those states are useful only if they stay separate.

This split is practical, not academic. Local model support often breaks when
storage, conversion, calibration, and compute are treated as the same word.
YVEX keeps `qtype-support`, `quant-policy`, `quant-job`, and `imatrix` as
different surfaces so an operator can see whether they are looking at storage
metadata, declared policy, external provenance, calibration evidence, or actual
backend compute coverage.

| Layer | Current state | Meaning |
| --- | --- | --- |
| Storage recognition | implemented where qtypes are known | artifact metadata can identify qtype |
| Policy | implemented | qtypes can be allowed or rejected by manifest policy |
| Selected emission | implemented for selected paths | selected GGUF output can be written under explicit constraints |
| Quant-job provenance | implemented as manifest inspection | external quantization work can be recorded |
| Imatrix provenance | implemented as manifest inspection | calibration evidence can be tracked |
| Backend compute | narrow / fixture-specific | not full quantized matmul |
| Full quantized inference | unsupported | no model execution claim |

The current tools make provenance and policy explicit before kernels and graph
execution rely on them. They do not turn external quantization tools into
hidden runtime behavior, and they do not imply that every artifact described by
a manifest can be executed by YVEX.

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

## Backends and machine pressure

The runtime is constrained by the machine before it is constrained by the
README. Tensor bytes have to land somewhere. `CUDA` allocations have to fail
cleanly. qtypes have to mean different things in storage and compute. Future
`KV` and scratch budgets have to be sized before prefill and decode become
meaningful. The selected DeepSeek embedding target makes the first part of
that pressure concrete without pretending the rest of the graph exists.

The `CPU` backend is the reference lane. It gives the project a stable place to
validate parser output, selected materialization, engine attachment, fixture
graph execution, cleanup, and error reporting before accelerated behavior
enters the picture. The `CUDA` lane is real but narrow: device discovery, memory
accounting, allocation, transfer, device copy, selected materialization, engine
attachment, and controlled `F32` fixture parity where implemented. That is
backend work, not yet a CUDA transformer backend.

| Backend / platform | Current role | Current boundary |
| --- | --- | --- |
| CPU | correctness/reference path for parse, materialization, engine attachment, and fixture execution | not production inference |
| CUDA / Linux | primary acceleration target; device probe, allocation, transfer, selected materialization, and F32 fixture parity | not a full transformer backend |
| DGX Spark / GB10 | future CUDA hardware profile | no profile claim until measured |
| Metal / macOS | not implemented | no Metal support claim |
| ROCm / Strix Halo | not implemented | no ROCm support claim |

A DeepSeek-class runtime eventually has to do much more than place an
embedding tensor or execute a controlled fixture. It has to place many weights,
schedule normalization, RoPE, attention, routed experts, quantized matmul,
scratch, KV, decode, and logits. The current fixture graph is small because
executor mechanics have to be correct before those larger kernels and
scheduler decisions arrive.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Operator path

The README keeps the normal path short. The operator runbook contains the full
command-first sequence, including selected `GGUF` emission, alias registration,
materialization, engine/session attachment, selected embedding partial graph
execution, fixture graph execution, daemon status, gates, chat diagnostics,
quantization/provenance commands, validation, artifact hygiene, and debugging.

| Path | Reader | Where to go |
| --- | --- | --- |
| Normal selected-artifact path | build, register, inspect, materialize, attach, execute selected embedding segment | this README and [docs/operator-runbook.md](docs/operator-runbook.md) |
| Deterministic fixture proof | controlled `F32` graph execution | this README and [docs/operator-runbook.md](docs/operator-runbook.md) |
| Full gates and selected-artifact checks | exact validation and repeatable proof | [docs/operator-runbook.md](docs/operator-runbook.md) |
| API/ownership extension | C API and lifecycle rules | [docs/api.md](docs/api.md) |
| Behavior contract | CLI/filesystem/backend/server guarantees | [docs/contract.md](docs/contract.md) |

```sh
make
./yvex commands
./yvex models current
./yvex inspect deepseek4-v4-flash-selected-embed
```

The normal path should become shorter over time. Future operator-facing
commands should wrap materialization, gates, backend checks, registry state,
and reports behind concise presets. The low-level commands will remain useful
for CI, debugging, and exact reproducibility, but they should not be the only
way to operate the runtime.

The command surface is still low-level because the project is exposing the
boundaries it has implemented. A later CLI simplification pass should wrap
common flows into shorter presets, but the explicit commands remain useful
because they show exactly which runtime boundary is being exercised. Planned CLI
work should keep those explicit surfaces while adding shorter operator presets
for common flows such as model status, model prepare, model check, backend
doctor, and runtime proof commands.

## Evaluation and benchmarking posture

YVEX does not publish token/sec or model-quality numbers yet. That is
deliberate: without prefill, decode, logits, and generation, such numbers would
be fictional. The current measurable boundaries are fixture graph correctness
and selected embedding partial graph correctness. The controlled fixture output
and selected embedding checksum are correctness proofs, not performance
benchmarks. The next measurable boundary is larger real-model graph segment
regression.

| Measurement | Current YVEX status | First valid point |
| --- | --- | --- |
| Fixture graph correctness | available after controlled fixture execution | current fixture path |
| Real partial graph regression | available for selected embedding segment | current selected embedding partial path |
| Prefill throughput | not available | after prompt-backed prefill |
| Decode throughput | not available | after decode step |
| Logits regression | not available | after logits boundary |
| Generation quality smoke | not available | after constrained generation |
| End-to-end runtime benchmark | not available | after generation path stabilizes |

Future benchmark shape:

| Future benchmark row | Prompt size | Metric | Status |
| --- | --- | --- | --- |
| short prompt | `<512` tokens | prefill + decode tokens/sec | blocked until prefill/decode |
| medium prompt | `~10k` tokens | prefill interval tokens/sec + decode tokens/sec | planned |
| long prompt | `32k+` tokens | prefill interval tokens/sec + KV memory | planned |
| generation probe | fixed generated-token count | decode/generation tokens/sec | blocked until generation |

Capability and regression posture:

| Eval group | Purpose | Status |
| --- | --- | --- |
| Fixture vectors | catch executor/backend regressions | available for the controlled fixture path |
| Partial graph vectors | catch real-model graph segment drift | available for selected embedding segment; broader vectors planned |
| Prompt/token tests | catch tokenizer/template drift | diagnostics only |
| Logits vectors | catch output distribution regressions | planned after logits |
| Capability eval | inspect generation quality on curated tasks | future only |

Fixture eval is not model quality eval. Logits regression is not generation
quality. The runbook's materialization and gate commands can produce repeatable
selected-artifact reports, but they are not throughput benchmarks. Token/sec
tables need model artifact, backend, qtype, context length, machine, command,
and reproducibility notes. Until those pieces exist, the honest result is no
published benchmark number.

The measurement rule is simple: YVEX should only measure a boundary it actually
owns. Fixture outputs are valid today because fixture execution exists. Selected
embedding checksums are valid today because real partial embedding execution
exists. Broader partial graph vectors become valid after graph segment expansion.
Token/sec numbers become valid only after prefill and decode exist. Capability
evaluation becomes meaningful only when the same generation path used by
operators exists.

## Runtime roadmap

The next step is not "inference" in one jump. YVEX now has the first real-model
partial graph path for a selected embedding segment. The next step is expanding
from that one real op to a larger real-model graph segment with more scheduled
work, memory planning, backend dispatch, and regression coverage.

| Runtime stage | Public meaning | Status |
| --- | --- | --- |
| Controlled fixture graph | deterministic executor proof | implemented |
| Real partial graph | selected `F16` embedding segment | implemented |
| Real graph segment expansion | larger scheduled real-model segment | next |
| Prompt input | tokenizer/prompt into runtime tensors | planned |
| Prefill | prompt tokens through graph state | planned |
| Minimal KV | session-owned append/read state | planned |
| Decode | one step over existing state | planned |
| Logits | output distribution buffer | planned |
| Sampling | choose next token from logits | planned |
| Constrained generation | bounded token loop | planned |
| CLI generation | interactive runtime path | future |
| Provider generation | server API backed by runtime | future |

Only after real graph segments, prompt input, prefill, minimal KV, decode,
logits, and sampling exist does text generation become a valid runtime claim.
Evaluation and benchmarking follow the same order: fixture correctness can be
tested now; logits regression cannot exist before logits; token/sec cannot be
published before prefill and decode exist.

## Provider boundary

`./yvexd` is a provider/status shell. It accepts a direct path or registered
alias for `--model`, serves status endpoints, and reports generation as
unavailable. It is not a generation server.

| Endpoint / behavior | Current state |
| --- | --- |
| `GET /health` | implemented provider/status shell |
| `GET /metrics` | implemented metrics/status surface |
| `GET /v1/models` | implemented model listing/status |
| `--model FILE_OR_ALIAS` | explicit path or registered alias |
| generation endpoints | unsupported |
| OpenAI-compatible generation | unsupported |
| Anthropic-compatible generation | unsupported |

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

Expected model status includes:

```text
generation_available: false
```

The daemon exists so the provider boundary can be wired early without faking
model output.

## Validation

Validation ties README claims to command behavior. `make check` runs the
baseline C test surface, smoke tests exercise the CLI and daemon paths, docs
surface tests protect the public documentation boundary, and repository
surface guards keep the minimal layout from drifting. CUDA validation is
explicit because it depends on a CUDA-capable host.

| Gate | Purpose |
| --- | --- |
| `git diff --check` | patch hygiene |
| `make check` | baseline C tests |
| `make smoke` | CLI and daemon smoke |
| `tests/test_docs_surface.sh` | public docs boundary |
| `tests/test_surface.sh` | repository surface guardrail |
| `make check-cuda` | CUDA-capable host validation |

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
make check-cuda
```

`make check-cuda` requires a CUDA-capable host.

The artifact guardrails keep model weights and generated artifacts out of git:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is no committed real model weights, no generated model
`GGUF`s, no local registries, no reports/logs/build outputs, and no benchmark
artifacts. Tracked `GGUF` files are tiny parser fixtures under `tests/`.

## Source layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             provider daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA host bridge and kernel unit
gguf/               GGUF parser, conversion, family mapping, quant policy
include/yvex/       public C API
tests/              compact runners, fixtures, and vectors
docs/               API, contract, operator runbook, internal spine
```

The layout is still intentionally compact. As the runtime grows, source layout
should follow ownership boundaries: CLI should not own backend logic, server
code should not duplicate CLI runtime wiring, and tests should keep parser
fixtures, runtime fixtures, evaluation vectors, and benchmarks distinct.

## Documentation

| File | Role |
| --- | --- |
| `docs/operator-runbook.md` | command-first operator workflow |
| `docs/api.md` | public C API, ownership, and error/capability map |
| `docs/contract.md` | behavior contract for CLI, filesystem, backend, server, registry |
| `MODEL_ARTIFACTS.md` | external artifact cards and validation posture |
| `AGENTS.md` | operating rules for humans and coding agents |
| `docs/spine.md` | internal delivery map, not public product documentation |

Normal readers should start with this README and the operator runbook. The API
and contract documents are for people extending or validating the runtime.

## License

YVEX is licensed under the MIT license.
