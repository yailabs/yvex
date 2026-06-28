# YVEX

YVEX is a native C inference engine for local open-weight models.

The project is built around one idea: local inference should be understandable
all the way down to the machine. A model file should not become a vague
"loaded" object. It should become runtime-owned state: parsed model facts,
tensor roles, backend-resident weights, graph work, token state, prefill state,
KV ownership, decode, logits, sampling, and eventually a generation surface
that can explain what happened when something fails.

YVEX is not trying to be a thin shell around another engine. It is not trying to
make a prompt box appear before the lower runtime is real. The goal is a
self-owned local inference path: model intake, artifact identity, tensor
mapping, backend memory, engine/session lifetime, graph scheduling, token
processing, and serving all tied to the same native runtime.

The current implementation is still early in that full path, but it is already
executing real pieces of it. YVEX can parse and validate local GGUF artifacts,
map selected tensor facts into runtime descriptors, materialize selected weights
on CPU and CUDA, attach those weights to engine-owned state, execute controlled
fixture graphs, execute a real selected embedding segment over F16 model bytes,
execute an embedding-plus-RMSNorm segment over multiple real tensors, accept
explicit token sequences, and build a prefill state summary over implemented
selected graph segments.

Those pieces are not random demos. They are the lower floors of the same
transformer execution path. The point is to make each floor inspectable before
building the next one. First the engine has to know which artifact it is using.
Then it has to know which tensors are safe to read. Then it has to move bytes
into backend-owned memory. Then it has to schedule graph work and compare output
against a reference. Then token sequences become runtime state. After that, KV,
decode, logits, sampling, and provider generation can be added without
pretending that "running" is one indivisible thing.

YVEX uses GGUF, registry files, qtype metadata, conversion tooling, integrity
checks, and artifact cards because a local inference engine needs all of that
close to the execution path. GGUF is not the identity of the project. It is the
container boundary the engine currently uses to learn what the model says about
itself: architecture metadata, tensor names, shapes, dtypes, byte offsets,
qtypes, alignment, and tokenizer facts. The runtime has to understand those
facts before it can claim that a model is executable.

The long-term shape is simple: a local model should become a local runtime, not
just a file opened by a CLI. That runtime should know what it owns, what it
borrowed, what it moved to a backend, what graph it scheduled, which token
positions it processed, where KV lives, which logits were produced, and how a
server or agent surface is backed by actual model execution.

## Why YVEX exists

Local inference has become interesting again, but the words around it are still
too weak.

"Loaded" might mean that a path exists. It might mean the header parsed. It
might mean the tensor directory was listed. It might mean a few selected weights
moved to CUDA. It might mean a graph actually ran. It might mean logits were
produced. It might mean an HTTP server accepted a request. Those are not the
same state. When a local model fails, the useful question is not "did inference
fail?" but where the execution chain stopped.

YVEX exists to make that chain visible.

A serious local engine should be able to tell an operator that an artifact
changed after registration, that a tensor range would read outside the file,
that a dtype is recognized as storage but not supported by a compute path, that
a backend allocation failed and cleanup happened, that a graph guard refused
dispatch, that token input was valid but no tokenizer path exists for prompt
text, or that a prefill state exists but KV ownership has not yet been created.

That kind of visibility matters more as models get larger. A small model can
hide a lot of runtime sloppiness. A DeepSeek-class model cannot. Tensor sizes,
qtype boundaries, memory movement, tokenizer layout, expert routing, KV size,
and backend pressure all expose weak abstractions. YVEX uses selected
DeepSeek-class artifacts as pressure targets because they force the runtime to
become honest before the full model path exists.

The project is deliberately narrow at the point of implementation and broad in
the path it is preparing. It does not try to support every GGUF file just
because the parser can read one. It does not claim DeepSeek support because a
selected embedding tensor can run. It does not turn provider-shaped endpoints
into generation. It builds the chain that a future provider endpoint must stand
on.

This is also why the artifact tooling lives next to the runtime. Source
manifests, native tensor inventories, tensor maps, GGUF templates, selected
emission, qtype policy, quantization manifests, imatrix evidence, local registry
aliases, integrity gates, and graph checks are not unrelated utilities. They are
part of the route from source weights to local execution. If the engine cannot
explain where an artifact came from, how tensors map to runtime roles, which
qtype is only storage, and what the backend actually supports, then later
performance numbers are not very meaningful.

YVEX is written in C because the runtime boundary is the point. Ownership,
allocation, cleanup, byte ranges, backend handles, tensor views, and
command-visible reports should be explicit. The project should eventually feel
simple at the top, but it should not be vague underneath.

## Architecture view

YVEX is organized around the path from model evidence to execution.

The path begins before GGUF. Native source weights, source manifests, tensor
inventories, and family tensor maps describe what the model is supposed to be.
GGUF then becomes the local artifact envelope: the concrete file that contains
metadata, tensor directory entries, qtypes, offsets, alignment, and tokenizer
data where present. YVEX does not treat that envelope as magic. It parses it,
validates it, maps it, and refuses it when the file contradicts what later
execution would need.

After artifact intake, the engine builds runtime descriptors. A descriptor is
not just a parsed file. It is the bridge between artifact facts and runtime
roles: which tensor is an embedding, which dtype it stores, which shape
convention is expected, how many bytes the engine may read, which backend can
receive it, and which graph path can use it.

Materialization is the next boundary. At that point the runtime is no longer
describing bytes on disk; it is asking a backend to own them. CPU and CUDA
materialization exercise allocation, transfer, cleanup, and byte accounting.
The engine then attaches selected backend-resident weights into engine-owned
state, and sessions can observe that state without owning or freeing it.

Graph execution is where the runtime stops being a loader and starts being an
engine. YVEX has a controlled fixture graph path for exact executor checks, and
a selected real-artifact path for real model bytes. The selected path started
with embedding lookup and now includes embedding plus RMSNorm. That graph slice
is small compared with a full transformer block, but it is the first place where
real model tensors, backend dispatch, memory planning, reference comparison,
and cleanup all meet.

Token input and prefill state move the runtime from single selected-token probes
toward sequence processing. Explicit token lists are parsed and bounded. A
selected token can drive graph execution. The prefill state foundation records
token positions, per-token segment summaries, checksums, output bytes, and
cleanup behavior. It is the first state object on the road to real KV ownership.

From there, the engine path is clear: minimal KV ownership, decode over existing
state, logits, deterministic logits regression, sampling, constrained
generation, interactive CLI generation, provider-backed generation, and runtime
traces/profiles. Those are not marketing categories. They are ownership
boundaries.

## Model weights and artifact strategy

YVEX keeps real model weights outside git. The repository should travel without
carrying private operator files, generated GGUFs, local registries, reports,
logs, or machine-specific paths. The codebase contains source, headers, docs,
tiny fixtures, and tests. Real artifacts stay on the machine that owns them.

The current pressure artifacts are intentionally selected artifacts. They are
not the full model yet. They exist to push real tensor size, shape, dtype,
identity, range validation, backend memory, and graph dispatch through the
runtime before full-model execution lands.

| Artifact class | Current role | Where it lives | Why it matters |
| --- | --- | --- | --- |
| Native source weights | Source evidence and tensor inventory | Operator-local storage | Lets YVEX reason about original tensor names, shapes, and family mapping before GGUF emission. |
| Selected embedding GGUF | Main real-tensor pressure artifact | Operator-local storage | Exercises identity, range math, backend materialization, engine attachment, and selected embedding execution over a large F16 tensor. |
| Selected embedding plus RMSNorm GGUF | First multi-tensor segment artifact | Operator-local storage | Adds a second real tensor and a second op so graph scheduling, scratch/output planning, RMSNorm, and reference comparison become concrete. |
| Controlled F32 fixtures | Deterministic graph and parser tests | Repository tests | Keeps executor, parser, corruption, and backend behavior exactly checkable without committing real model weights. |
| Future full model GGUF | Full local inference target | Operator-local storage | Becomes relevant when the runtime owns the larger graph, KV, decode, logits, and sampling path. |
| Quantization and imatrix manifests | Provenance and calibration evidence | Operator-local or tiny test/docs fixtures | Keeps quantization decisions visible without pretending that storage recognition equals compute support. |

The active selected embedding artifact contains `token_embd.weight` with shape
`[4096,129280]`, dtype `F16`, and roughly one billion tensor bytes. That is
enough to force real backend behavior. A fake runtime can parse a small fixture
and look convincing; it cannot casually move a billion-byte tensor through
identity checks, materialization, engine attachment, graph dispatch, and
reference comparison without exposing its assumptions.

The selected embedding-plus-RMSNorm artifact adds the first RMSNorm weight, such
as `blk.0.attn_norm.weight`, and the metadata needed to compute embedding lookup
followed by RMSNorm. This is the first larger real-model graph slice. It proves
that the runtime can schedule more than one op over more than one real tensor
before attention, MLP, routed experts, logits, and sampling arrive.

[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) is the artifact card surface. It should
describe external artifacts, digest facts, tensor facts, selected pressure
artifacts, and validation posture without leaking workstation paths. It is not
a model zoo and not a download page.

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

The alias is a local handle for a specific artifact and support level. It is not
a claim that a complete DeepSeek model is executable. It lets the operator use
stable names while the runtime continues to check file identity, metadata drift,
tensor readiness, and graph preflight.

## Model-building pipeline

YVEX treats model-building as part of the inference engine, not as a separate
afterthought. A local runtime should know why an artifact has the layout it has.
It should know which source tensors were observed, which model family mapping
was applied, which qtypes were selected, and which emission constraints produced
the final GGUF.

A local inference engine cannot treat `GGUF` as a file extension only. The
container carries model architecture, metadata, tensor names, tensor ranks,
dimensions, dtypes, byte offsets, qtypes, alignment, and tokenizer information
where present. Those facts are not just displayed by the CLI; they become
runtime preconditions.

The current model-building flow is intentionally narrow:

```text
official/open weights
  -> source manifest
  -> native safetensors inventory
  -> family tensor mapping
  -> GGUF template validation
  -> selected emission
  -> registry alias
  -> integrity report
  -> materialization
  -> engine attachment
  -> graph execution
```

Each step answers a different question. The source manifest answers where the
weights came from. Native inventory answers what tensors exist before
conversion. Tensor mapping answers how family-native names become runtime roles.
Template validation answers what the GGUF should contain. Selected emission
produces the small artifact the current runtime can actually exercise.
Integrity and graph guards make sure later commands are not trusting malformed
or stale assumptions.

Quantization follows the same discipline. A qtype can be recognized as artifact
storage while no backend kernel computes with it. A quantization policy can
allow a qtype for one artifact shape while another path cannot run it. A
quant-job manifest can record external work without hiding it inside runtime
behavior. Imatrix evidence can describe calibration without becoming a compute
claim.

This separation will matter more when YVEX reaches full-model execution. At
that point, performance and quality will depend on exact qtype choices, backend
kernels, routing decisions, attention layout, KV format, and logits stability.
The project is building the paperwork and runtime facts now because later
debugging will be much harder without them.

## Machines and backend lanes

Local inference is not abstract. Machine class, memory size, backend ownership,
copy behavior, and kernel coverage change what the engine can prove.

YVEX keeps backend lanes visible for that reason. CPU is not just a fallback.
CUDA is not just a faster version of CPU. Metal and ROCm are not decorations in
a README. They represent different local-machine futures, different memory
models, different kernel work, and different operator expectations.

| Machine class | Backend lane | Engine role | Public posture |
| --- | --- | --- | --- |
| CPU-only developer machine | CPU reference | Parser behavior, descriptor construction, token input, integrity checks, cleanup paths, and small graph correctness stay easy to inspect. | Reference and diagnostics lane. |
| CUDA workstation | CUDA | Current accelerated lane for selected materialization, fixture parity, selected embedding execution, embedding-plus-RMSNorm, and prefill state summaries where CUDA is available. | Current accelerated pressure lane. |
| DGX Spark / GB10-class CUDA machine | CUDA pressure target | Useful for exercising larger local artifacts, memory pressure, backend allocation behavior, and future graph growth. | Hardware pressure target for CUDA-class work. |
| High-memory MacBook / Mac Studio | Metal direction | Important class for future local inference because unified memory and fast local storage change what can be attempted on personal machines. | Future backend lane, not current support. |
| Strix Halo-class system | ROCm direction | Important AMD local-machine class for future unified-memory inference work. | Future backend lane, not current support. |

The current public implementation claim is CPU plus CUDA for the runtime
boundaries that exist today. The other lanes matter because the project is not
only about one backend. It is about a local inference engine that can eventually
make different machine classes explicit rather than hiding them behind a generic
`device` flag.

## What runs now

YVEX currently runs two kinds of graph proof.

The first is the controlled lane. It uses tiny F32 fixtures, not real model
weights. The point is exactness. A fixture graph can prove descriptor
construction, weight attachment, backend dispatch, memory allocation, output
readback, cleanup, and checksum behavior without model complexity hiding bugs.
This lane is where executor mechanics should be boring and deterministic.

The second is the selected real-artifact lane. It uses operator-local
DeepSeek-class artifacts and real model bytes. The selected embedding path reads
`token_embd.weight`, interprets the tensor as `[hidden_size, vocab_size]`,
selects a token, converts the F16 row into an F32 output vector, and checks the
result against an independent raw-artifact reference slice.

The embedding-plus-RMSNorm path adds another tensor and another op. It reads the
embedding, applies RMSNorm using a real first-layer RMSNorm weight and explicit
epsilon, and checks the final vector by checksum and max-diff. This is the
first larger graph slice on the road to transformer block execution. It creates
a place for attention and MLP work to attach later because multi-tensor
scheduling, intermediate memory planning, backend RMSNorm support, cleanup, and
reference comparison already exist.

Token input is now explicit. `yvex input tokens` parses bounded token sequences,
validates them against vocabulary facts when available, and lets graph commands
select a token by index. Prompt text is different: it only becomes executable
token input when tokenizer metadata is present and executable for that artifact.
This keeps prompt handling from being silently confused with prefill.

The prefill state foundation is the first place where a token sequence becomes
runtime state instead of a loose command argument. The current implementation
uses the selected embedding-plus-RMSNorm segment as the per-token graph slice.
It records positions, processed tokens, per-token summaries, checksums, output
byte accounting, and cleanup status. It is still below attention KV and logits,
but it is no longer just token parsing.

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

The selected segment path is explicit about the slice it runs:

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

The runbook contains the longer transcripts, expected output fields, CUDA
variants, fixture proof, failure modes, and gate commands.

## Integrity and refusal gates

The integrity layer is local correctness and corruption safety. It is not
supply-chain security, malware detection, sandboxing, author authenticity,
remote provenance, or model-quality validation. It is the layer that prevents
inconsistent local artifacts from entering materialization or graph execution.

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
mismatch, the engine should not silently trust it. If metadata drift invalidates
registered tensor facts, graph execution should not dispatch. If a selected
token is out of range, the reference slice should not be read. A good local
runtime is not one that says "pass" often; it is one that says "stop" at the
right boundary.

## Speed and evaluation shape

YVEX should publish speed numbers only for runtime paths it owns. Today the
useful measurements are correctness and regression measurements: fixture
outputs, selected embedding checksums, embedding-plus-RMSNorm max-diff, token
parsing behavior, prefill state summaries, materialization byte accounting,
graph guard phases, cleanup behavior, and CPU/CUDA parity for implemented paths.

The speed table below is the public shape YVEX should use when the runtime owns
the relevant path. It is included here so benchmarks have a fixed vocabulary
before numbers appear. Empty cells should not be filled with estimates.

| Machine | Backend | Artifact / qtype | Prompt | Prefill | Decode / generation |
| --- | --- | --- | --- | --- | --- |
| CPU developer machine | CPU | tiny fixtures / selected artifacts | short diagnostic prompt or explicit tokens | correctness only | not a speed target |
| CUDA workstation | CUDA | selected artifacts now, full GGUF later | short and medium prompts | measured once real prefill lands | measured once decode/logits/sampling land |
| DGX Spark / GB10-class system | CUDA | large DeepSeek-class artifacts | medium and long prompts | intended pressure row | intended pressure row |
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
| Decode throughput | Next-token execution speed over existing state | Measures the autoregressive path after context is already present. |
| Generation throughput | End-to-end prompt plus generated tokens | Measures the user-visible local run once sampling exists. |
| Capability regression | Task-level output stability over the same runtime path users run | Detects prompt rendering, tokenizer, logits, sampling, and model-output regressions. |

YVEX does not need fake token/sec numbers to look serious. It needs numbers that
are tied to a command, a machine, an artifact, and a runtime path that actually
exists.

## Larger-model execution direction

The first target is a single-machine local runtime path that is honest from
artifact intake through generation. Larger models and memory pressure are part
of the project's reason to exist, so the README should name the direction
clearly without turning it into a support claim.

The engine should eventually be able to reason about what fits, what streams,
what is resident, what belongs to KV, what belongs to scratch, what can be
reconstructed, and what must remain on a backend. That does not require claiming
SSD streaming or distributed execution today. It requires designing the lower
runtime so those decisions have real places to live later.

For a single-machine path, the immediate questions are memory accounting, qtype
policy, backend placement, scratch planning, KV layout, and cleanup. For future
multi-machine work, the questions become layer ownership, activation transport,
worker identity, route completeness, KV partitioning, and recovery after worker
failure. Those ideas belong after the single-machine engine path is real, not
before.

## Server and operator surface

`yvexd` is the current provider/status boundary. It can report health, metrics,
model listing, direct paths, registry aliases, and generation unavailability.
That is useful because server shape should not be bolted on after the engine is
finished; it should grow with the runtime while staying honest about what the
runtime can back.

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
