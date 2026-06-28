# YVEX

YVEX is a native C inference engine for local open-weight models.

The project starts from a simple requirement: a local model should become a real
runtime, not a vague object hidden behind the word "loaded". When YVEX opens a
model artifact, the interesting part is not that a file was found or that a CLI
accepted a path. The interesting part is whether the model can be turned into
owned execution state: known tensors, checked byte ranges, backend-resident
weights, scheduled graph work, token positions, prefill state, KV, decode,
logits, sampling, and eventually generated tokens that come from the same
runtime chain.

YVEX is being built for the whole local transformer path. It begins below chat
interfaces, provider APIs, sampling loops, and agent shells because those
surfaces are only meaningful when the machine underneath can explain what it
did. A server endpoint can exist before inference exists. A prompt box can
exist before KV exists. A benchmark table can exist before the measured path is
real. YVEX takes the opposite route: make the lower runtime accountable first,
then let user-facing surfaces grow out of it.

The current implementation owns the early part of that path. It can inspect and
validate GGUF artifacts, map selected tensor facts into runtime descriptors,
materialize selected weights on CPU and CUDA, attach those weights to
engine-owned state, execute controlled fixture graphs, execute selected F16
embedding over real model bytes, execute an embedding-plus-RMSNorm slice over
multiple real tensors, accept explicit token sequences, and build a prefill
state summary from implemented graph slices.

That is not the final transformer run yet, but it is already the beginning of
one. The selected graph slices are small on purpose: they give the runtime a
hard surface to stand on before attention, MLP, routed experts, full KV, decode,
logits, and sampling arrive. A local engine that cannot explain its first
tensor read will not become more trustworthy when the graph becomes larger.

The long-term shape is a native local runtime that remains inspectable from
artifact bytes to generated tokens. YVEX should know which file it is using,
which digest was recorded, which tensor names mapped to runtime roles, which
backend owns a buffer, which graph slice ran, which token positions were
processed, where KV lives, which logits were produced, and how a CLI or server
response is backed by actual model execution.

GGUF, registry files, qtype metadata, conversion helpers, integrity reports, and
artifact cards are part of that engine story. They are not side utilities
around a future chat program. GGUF is the current envelope YVEX uses to learn
what a model says about itself: architecture metadata, tensor names, ranks,
shapes, dtypes, offsets, qtypes, alignment, and tokenizer data where present.
The runtime has to understand those facts before it can honestly say that a
model is executable.

YVEX is written in C because this layer should be explicit. Allocation, cleanup,
byte ranges, backend handles, tensor views, graph outputs, and command-visible
reports are not incidental details. They are the material of a local inference
engine. The top of the system should eventually feel simple; the bottom should
never be vague.

## Why this project exists

Local inference is becoming interesting again, but the words around it are
still too coarse.

"Loaded" can mean that a path exists. It can mean the header parsed. It can
mean metadata was read. It can mean tensor names were listed. It can mean
selected weights moved to CUDA. It can mean a graph ran. It can mean logits
were produced. It can mean an HTTP server accepted a request. Those states are
very different, and confusing them is one of the easiest ways to make local
inference look better than it is.

YVEX exists to separate those states.

When a local model fails, the useful question is not simply "did inference
fail?" The useful question is where the execution chain stopped. Maybe the
artifact changed after registration. Maybe a tensor range would read outside
the file. Maybe a qtype is recognized as storage but has no compute path yet.
Maybe backend allocation failed and cleanup succeeded. Maybe graph preflight
rejected a shape. Maybe token input was valid, but the artifact had no
executable tokenizer metadata for prompt text. Maybe a prefill state exists,
but KV ownership has not been created yet.

Those are not cosmetic distinctions. They are the places where a local
inference project either becomes a runtime or remains a wrapper.

The problem becomes sharper with large open-weight models. Small models can
hide weak abstractions. DeepSeek-class artifacts cannot. Tensor size, qtype
layout, memory movement, tokenizer facts, normalization rules, future KV shape,
and backend pressure all force the runtime to become precise. YVEX uses
selected DeepSeek-class artifacts as pressure targets because they expose these
problems early, before the full model path is ready.

The project is narrow in the code it claims today and broad in the path it is
preparing. It does not try to accept every GGUF file just because the parser can
read one. It does not claim model support just because one selected tensor can
run. It does not turn a provider-shaped daemon into generation. It builds the
path a future generation surface must use: artifact evidence, model identity,
tensor roles, backend memory, graph execution, token state, KV, decode, logits,
sampling, and serving.

This is also why model-building belongs close to the runtime. Source manifests,
native tensor inventories, family maps, GGUF templates, selected emission,
qtype policy, quantization manifests, imatrix evidence, local registry aliases,
integrity checks, and graph guards all describe the same route from source
weights to local execution. If the engine cannot explain where an artifact came
from, how tensors map to runtime roles, what a qtype means, and what the backend
can compute, then later speed numbers are just decoration.

## Engine architecture

YVEX follows the path from model evidence to execution.

The path begins before the final GGUF. Native source weights, source manifests,
tensor inventories, and model-family maps describe what the model is supposed
to contain. GGUF becomes the concrete local artifact the runtime can open,
validate, register, and execute against. YVEX treats that artifact as an
operating boundary, not as a magic file extension.

From the artifact, the engine builds descriptors. A descriptor is the runtime's
interpreted view of the model facts it can use: which tensor is an embedding,
which shape convention applies, which dtype is stored, which byte range is
safe, which backend can receive the tensor, and which graph path is allowed to
consume it. Parsing alone is not enough. The runtime needs facts with roles.

Materialization is the first point where those facts become memory. At that
boundary, YVEX is no longer only describing bytes on disk; it is asking CPU or
CUDA storage to own selected bytes. That makes allocation, transfer, cleanup,
and byte accounting visible. Once selected weights are resident, the engine can
attach them to engine-owned state, and a session can observe that state without
pretending to own it.

Graph execution is where the project becomes an engine. The controlled fixture
lane proves exact executor mechanics over tiny F32 data. The selected
real-artifact lane proves that real model tensors can participate in scheduled
work. The first selected slice performs embedding lookup. The next selected
slice adds RMSNorm. These slices are small compared with a full transformer
block, but they already exercise the machinery that larger blocks need: tensor
roles, backend dispatch, scratch/output planning, reference comparison, and
cleanup.

Token input and prefill state move the engine from single-token probes toward
sequence processing. Explicit token lists are parsed and bounded. A token can
be selected by index and routed into graph execution. The prefill state
foundation records positions, per-token graph summaries, checksums, output
bytes, and cleanup behavior. It is the first place where a token sequence
becomes runtime state instead of a loose command argument.

The next engine steps are concrete. Minimal KV ownership has to give the
session a real place to store attention state. Real prefill has to bind token
ranges to that KV state. Decode has to advance existing state by one position.
Logits have to become an owned buffer. Sampling has to sit over logits with
reproducibility rules. Only then can a generation loop, an interactive CLI, and
a provider server become honest user surfaces.

## Model weights and artifact strategy

YVEX keeps real model weights outside git. The repository should travel without
private operator files, generated GGUFs, local registries, reports, logs, or
machine-specific paths. The codebase contains source, headers, docs, tiny
fixtures, and tests. Real artifacts live on the machine that owns them.

The current pressure artifacts are selected artifacts. They are not toy files
and they are not the full model. They exist to push real tensor size, dtype,
shape, identity, range validation, backend transfer, graph dispatch, and
reference comparison through the runtime before full-model execution lands.

| Artifact class | Current role | Where it lives | Why it matters |
| --- | --- | --- | --- |
| Native source weights | Source evidence and tensor inventory | Operator-local storage | Lets YVEX reason about original tensor names, shapes, and model-family mapping before GGUF emission. |
| Selected embedding GGUF | Main real-tensor pressure artifact | Operator-local storage | Exercises identity, range math, backend materialization, engine attachment, and selected embedding execution over a large F16 tensor. |
| Selected embedding plus RMSNorm GGUF | First multi-tensor segment artifact | Operator-local storage | Adds a second real tensor and a second op so graph scheduling, scratch/output planning, RMSNorm, and reference comparison become concrete. |
| Controlled F32 fixtures | Deterministic parser and graph tests | Repository tests | Keeps parser, corruption, executor, and backend behavior exactly checkable without committing real weights. |
| Future full model GGUF | Full local inference target | Operator-local storage | Becomes relevant when the runtime owns full tensor placement, larger graph execution, KV, decode, logits, and sampling. |
| Quantization and imatrix manifests | Provenance and calibration evidence | Operator-local or tiny test/docs fixtures | Keeps quantization decisions visible without confusing storage recognition with compute support. |

The active selected embedding artifact contains `token_embd.weight` with shape
`[4096,129280]`, dtype `F16`, and roughly one billion tensor bytes. That size is
useful. A small fixture can prove parser behavior, but it cannot force the
runtime to confront real memory pressure, large range math, backend transfer,
and cleanup. The selected embedding artifact does.

The selected embedding-plus-RMSNorm artifact adds the first RMSNorm weight, such
as `blk.0.attn_norm.weight`, and the metadata needed to compute embedding lookup
followed by normalization. This is the first larger real-model graph slice. It
gives the runtime a place to prove multi-tensor scheduling before attention,
MLP, routed experts, logits, and sampling are added.

[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) is the artifact card surface. It should
document external artifacts, digest facts, tensor facts, selected pressure
artifacts, and validation posture without leaking workstation paths. It is not
a model zoo and not a download page. It is the public place where the
repository can say which operator-local artifacts have evidence behind them.

A typical selected-artifact registration looks like this:

```sh
./yvex models add \
  --path /path/to/operator/model.gguf \
  --alias deepseek4-v4-flash-selected-embed \
  --support-level selected-tensor-materialized

./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
```

The alias is a local handle for a specific artifact and support level. It lets
the operator use stable names while YVEX keeps checking file identity, metadata
drift, tensor readiness, and graph preflight.

## Model-building pipeline

YVEX treats model-building as part of the engine path. A local runtime should
know why an artifact has the layout it has. It should know which source tensors
were observed, which family mapping was applied, which qtypes were selected,
and which emission constraints produced the final GGUF.

A local inference engine cannot treat `GGUF` as a file extension only. A useful
local artifact is not just a file that parses. It is the result of a chain of
evidence: source weights, tensor inventory, mapping rules, template
expectations, qtype decisions, and conversion constraints. YVEX keeps that
chain explicit so runtime behavior can be traced back to artifact construction.

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

Each step answers a different question. The source manifest records where the
weights came from. Native inventory says which tensors exist before conversion.
Tensor mapping connects family-native names to runtime roles. Template
validation states what an artifact is expected to contain. Selected emission
produces the narrow artifact the current runtime can exercise. Integrity and
graph guards make sure later commands are not trusting malformed or stale
assumptions.

Quantization follows the same rule. A qtype can be recognized as artifact
storage while no backend kernel computes with it. A policy can allow a storage
format for one artifact while a graph path still refuses to run it. A quant-job
manifest can record external work without turning that work into runtime
support. Imatrix evidence can document calibration without becoming a compute
claim.

This separation will matter more when YVEX reaches full-model execution.
Performance and quality will depend on exact qtype choices, backend kernels,
attention layout, routing behavior, KV format, and logits stability. The engine
is building the paper trail and runtime facts now because later debugging will
be much harder without them.

## Machines and backend lanes

Local inference is not abstract. Machine class, memory size, backend ownership,
copy behavior, and kernel coverage change what the engine can prove.

YVEX keeps backend lanes visible for that reason. CPU is not merely a fallback.
CUDA is not merely "the same but faster". Metal and ROCm are not decoration.
They represent different local-machine futures, different memory models,
different kernel work, and different operator expectations.

| Machine class | Backend lane | Engine role | Public posture |
| --- | --- | --- | --- |
| CPU-only developer machine | CPU reference | Parser behavior, descriptor construction, token input, integrity checks, cleanup paths, and small graph correctness stay easy to inspect. | Reference and diagnostics lane. |
| CUDA workstation | CUDA | Current accelerated lane for selected materialization, fixture parity, selected embedding execution, embedding-plus-RMSNorm, and prefill state summaries where CUDA is available. | Current accelerated pressure lane. |
| DGX Spark / GB10-class CUDA machine | CUDA pressure target | Useful for larger artifacts, memory pressure, backend allocation behavior, and future graph growth. | Hardware pressure target for CUDA-class work. |
| High-memory MacBook / Mac Studio | Metal direction | Important class for future local inference because unified memory and fast local storage change what can be attempted on personal machines. | Future backend lane, not current support. |
| Strix Halo-class system | ROCm direction | Important AMD local-machine class for future unified-memory inference work. | Future backend lane, not current support. |

The current public implementation claim is CPU plus CUDA for the runtime stages
that exist today. The other lanes are named because the project is about local
inference as a machine problem, not because the code already supports them.

## What runs now

YVEX currently runs two graph lanes.

The controlled lane uses tiny F32 fixtures. Its purpose is exactness. A fixture
graph can prove descriptor construction, weight attachment, backend dispatch,
output allocation, readback, cleanup, and checksum behavior without model
complexity hiding a bug. This is where executor mechanics should be boring.

The selected real-artifact lane uses operator-local DeepSeek-class artifacts and
real model bytes. The selected embedding path reads `token_embd.weight`,
interprets it as `[hidden_size, vocab_size]`, selects a token, converts the F16
row into an F32 vector, and checks the result against an independent
raw-artifact reference slice.

The embedding-plus-RMSNorm path adds one more tensor and one more operation. It
reads the embedding, applies RMSNorm using a real first-layer normalization
weight, and checks the final vector by checksum and max-diff. This is the first
larger graph slice on the road to transformer block execution. Attention and
MLP are not present yet, but the slice already gives them a place to attach:
multi-tensor scheduling, intermediate memory planning, backend RMSNorm support,
cleanup, and reference comparison.

Token input is explicit. `yvex input tokens` parses bounded token sequences,
validates them against vocabulary facts when available, and lets graph commands
select a token by index. Prompt text is a separate boundary. It only becomes
executable token input when tokenizer metadata is present and the runtime knows
how to use it.

The prefill state foundation turns token sequences into runtime state. The
current implementation uses the selected embedding-plus-RMSNorm slice per
token. It records positions, processed tokens, per-token summaries, checksums,
output byte accounting, and cleanup status. It is still below attention KV and
logits, but it is no longer just token parsing.

The short selected-artifact path looks like this:

```sh
make
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

The selected segment path is explicit about the graph slice:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --execute-segment \
  --segment embedding-rmsnorm \
  --tokens 0,1 \
  --token-index 0
```

The prefill state path is explicit about sequence processing:

```sh
./yvex prefill \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --segment embedding-rmsnorm \
  --tokens 0,1,2
```

The runbook contains the longer transcripts, expected fields, CUDA variants,
fixture proofs, failure modes, and gate commands.

## Integrity and refusal gates

The integrity layer is local correctness and corruption safety. It is not
supply-chain security, malware detection, sandboxing, author authenticity,
remote provenance, or model-quality validation. It prevents inconsistent local
artifacts from entering materialization or graph execution.

YVEX checks structural GGUF validity, tensor directory consistency, duplicate
and empty tensor names, rank and dimension accounting, dtype storage
recognition, byte-count math, tensor byte ranges, selected token slices,
SHA-256 identity, registry metadata drift, selected embedding readiness,
materialization preflight, graph preflight, cleanup after injected failures, and
repeat behavior after refusal.

The normal operator report is:

```sh
./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
```

For a registered alias, the report can include digest identity, registry
metadata drift, selected tensor readiness, materialization preflight, and graph
guard status. For a raw path, it reports current file facts without pretending
registry metadata exists. If no backend is supplied, backend readiness is not
checked.

The important behavior is early refusal. If structural integrity fails,
materialization should not allocate. If a registered alias has a digest
mismatch, the engine should not trust it silently. If metadata drift invalidates
registered tensor facts, graph execution should not dispatch. If a selected
token is out of range, the reference slice should not be read. A good local
runtime is not one that says "pass" often; it is one that says "stop" at the
right place.

## Speed and evaluation shape

YVEX should publish speed numbers only for paths it actually owns. Today the
useful measurements are correctness and regression measurements: fixture
outputs, selected embedding checksums, embedding-plus-RMSNorm max-diff, token
parsing behavior, prefill state summaries, materialization byte accounting,
graph guard phases, cleanup behavior, and CPU/CUDA parity for implemented paths.

The table below is the benchmark shape YVEX should use once each runtime path
exists. Empty cells should not be filled with estimates.

| Machine | Backend | Artifact / qtype | Prompt | Prefill | Decode / generation |
| --- | --- | --- | --- | --- | --- |
| CPU developer machine | CPU | tiny fixtures / selected artifacts | short diagnostic prompt or explicit tokens | correctness only | not a speed target |
| CUDA workstation | CUDA | selected artifacts now, full GGUF later | short and medium prompts | measured once real prefill lands | measured once decode, logits, and sampling land |
| DGX Spark / GB10-class system | CUDA | large DeepSeek-class artifacts | medium and long prompts | pressure row once full prefill exists | pressure row once generation exists |
| High-memory MacBook / Mac Studio | Metal | future local GGUF path | short, long, and agent-style prompts | future row | future row |
| Strix Halo-class system | ROCm | future local GGUF path | short and long prompts | future row | future row |

The benchmark methodology should measure intervals, not just whole-run
averages. Prefill should be measured by token ranges and context frontiers.
Decode should be measured at defined context sizes. Generation should report
prompt size, generated tokens, sampling mode, machine, backend, artifact
identity, qtype, command, and reproducibility notes.

| Measurement | What it measures | Why it matters |
| --- | --- | --- |
| Fixture correctness | Exact values and checksums over tiny controlled graphs | Catches executor and backend regressions before real model complexity enters. |
| Selected segment correctness | Checksum and max-diff against raw-artifact reference | Catches tensor mapping, dtype, backend, and graph-slice regressions. |
| Prefill throughput | Token interval throughput over real transformer prefill | Measures the cost of turning prompt tokens into runtime state. |
| Decode throughput | Next-token execution speed over existing state | Measures the autoregressive path after context is present. |
| Generation throughput | End-to-end prompt plus generated tokens | Measures the user-visible local run once sampling exists. |
| Capability regression | Task-level output stability over the same runtime path users run | Detects prompt rendering, tokenizer, logits, sampling, and model-output regressions. |

YVEX does not need fake token/sec numbers to look serious. It needs numbers tied
to a command, a machine, an artifact, and a runtime path that exists.

## Larger-model execution direction

The first target is a single-machine local runtime that is honest from artifact
intake through generation. Larger models and memory pressure are still part of
the reason the project exists, so the direction should be visible early without
becoming a support claim.

The engine should eventually reason about what fits, what streams, what is
resident, what belongs to KV, what belongs to scratch, what can be
reconstructed, and what must remain on a backend. That does not require
claiming SSD streaming or distributed inference today. It requires designing
the lower runtime so those decisions have a place to live later.

For a single-machine path, the immediate questions are memory accounting, qtype
policy, backend placement, scratch planning, KV layout, and cleanup. For future
multi-machine work, the questions change: layer ownership, activation
transport, worker identity, route completeness, KV partitioning, and recovery
after worker failure. Those belong after the single-machine engine path is real.

## Server and operator surface

`yvexd` is the current provider/status boundary. It can report health, metrics,
model listing, direct paths, registry aliases, and generation unavailability.
That is useful because server shape should grow with the runtime instead of
being bolted on after the engine is finished.

Provider-backed generation should arrive only when the same runtime path can
prefill, own KV, decode, produce logits, sample, and stream tokens. Until then,
`yvexd` is a status and diagnostics surface.

The operator CLI is intentionally explicit. Low-level commands remain visible
because they cross different runtime boundaries. A later operator UX can make
common paths shorter, but it should not hide the evidence. `models`,
`integrity`, `materialize`, `engine`, `session`, `graph`, `input`, and `prefill`
each answer a different question.

## Build and validation

The normal build is small:

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

The artifact guardrail is equally important:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is that no real model weights are tracked. Tracked GGUF files
are tiny fixtures under `tests/`.

## Documentation

If you are looking for a specific surface, use the canonical docs below.

- [docs/operator-runbook.md](docs/operator-runbook.md): command-first workflow
  for artifacts, integrity, materialization, graph proofs, token input, prefill
  state, daemon status, and validation.
- [docs/api.md](docs/api.md): C API, ownership rules, report structs, backend
  capabilities, token input, prefill state summaries, materialization
  summaries, graph results, and integrity surfaces.
- [docs/contract.md](docs/contract.md): behavior and claim contract for CLI
  output, filesystem state, registry, backend, server, validation, and public
  boundaries.
- [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md): external artifact cards, selected
  pressure artifacts, digest facts, tensor facts, and validation posture.
- [AGENTS.md](AGENTS.md): repository operating rules for humans and coding
  agents.
- [docs/spine.md](docs/spine.md): internal delivery map, not product
  documentation.

## License

YVEX is licensed under the MIT license.
