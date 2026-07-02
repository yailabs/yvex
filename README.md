# YVEX

YVEX is a native C inference engine for local open-weight models.

The project starts from a point that most local inference stacks hide too
quickly: model weights are not useful just because a process can open them. A
GGUF file can parse, a registry alias can exist, a daemon can answer status,
and a command can print something impressive while the actual machine path is
still unclear. YVEX exists to make that path explicit. The engine should be
able to explain where an artifact came from, why it was accepted, how its
tensors became runtime structure, where memory was placed, what graph work
actually ran, and why the next boundary is either ready or still unsupported.

That is the core idea: move from file evidence to runtime evidence without
pretending that all intermediate states are the same thing.

YVEX lives below chat interfaces, provider-compatible APIs, agent shells, and
benchmark tables. Those surfaces matter only after the lower engine path has
been implemented and checked. A local system becomes trustworthy when it can
say, with mechanical evidence, that a specific artifact was validated, a
specific tensor was materialized, a specific backend owned that memory, a
specific graph path consumed it, and a specific runtime state was created from
it. Without that, the outside interface can look complete while the inference
engine remains mostly theater.

The current codebase already owns the lower part of that path. GGUF artifacts
are parsed through structural checks, selected DeepSeek-class tensors become
descriptors, descriptors become backend-resident weights, and those weights
become engine-owned state. The controlled fixture lane gives the executor a
small world where every result can be checked directly. The selected-artifact
lane takes large F16 model bytes and forces them through scheduled graph
execution. The embedding path proves the first selected tensor slice. The
embedding-plus-RMSNorm path adds a second tensor and a second operation, so
the runtime has to handle a concrete multi-op segment instead of a single
lookup. Token input then gives the graph a sequence boundary, and the
prefill-state foundation turns that sequence into the first runtime state
derived from model execution.

KV ownership has entered the runtime as a separate boundary. The current KV
store is intentionally minimal, but it proves the shape of the ownership
problem: a session can allocate KV state, mutate it through append/read
behavior, refuse overflow, clear it, and release it. This is not decode and not
generation. It is the first point where attention-state ownership becomes
concrete enough for later runtime stages to depend on it.

The staged shape is deliberate. Transformer execution should not appear as one
dramatic jump from parser to chatbot. Artifact evidence must become runtime
description. Runtime description must become backend residency. Backend
residency must become engine-owned state. Engine state must feed graph
execution. Token input must become prefill state. Prefill state must connect to
KV. KV must support decode. Decode must produce logits. Sampling must turn
logits into accepted tokens. CLI and server surfaces should sit above that
chain, not beside it.

GGUF is the current envelope for this work because it gives YVEX a concrete
source of model facts: architecture metadata, tensor layout, storage formats,
offsets, alignment, and tokenizer data where present. YVEX does not treat
those facts as passive metadata. It turns them into execution evidence.
Registry entries, qtype policy, conversion helpers, integrity reports, and
artifact cards belong to the same route because they preserve the chain from
open weights to local runtime behavior.

YVEX is written in C because this layer rewards visible ownership. Memory needs
an owner. Cleanup needs a phase. Tensor access needs bounds. Backend residency
should not be confused with model support. A report should tell the operator
where execution stands without forcing them to infer which part of the system
actually ran. The top of the engine can become simple only if the bottom
remains precise.

## Why this project exists

Open-weight models have moved past the toy phase. The useful question is no
longer whether a local machine can run something that looks like a model. The
harder question is whether local hardware can run models capable enough to
matter while keeping the runtime understandable enough to trust.

Small models are useful because they are fast, cheap to move, and often good
enough for narrow jobs. Larger open-weight models create a different
engineering problem. They preserve more reasoning ability, more coding
competence, broader knowledge, and longer-context usefulness, but they also
expose weak runtime design immediately. Memory pressure stops being a
deployment detail and becomes architecture. Quantization policy stops being
only a file-size choice and starts affecting quality. KV layout becomes
central. Tensor placement and backend kernels become part of the product.

YVEX is aimed at that harder side of local inference.

The project avoids treating `loaded` as a meaningful end state. A model path
can be accepted while no tensor is executable. A GGUF header can parse while a
later tensor read would be unsafe. Backend memory can exist without a graph
path that uses it. A daemon can expose status before generation exists. Those
distinctions are where a local inference engine becomes debuggable. If the
runtime cannot say exactly where it stopped, every later abstraction becomes
suspect.

Selected DeepSeek-class artifacts are useful because they apply pressure
without pretending the full transformer path is already present. A tiny fixture
proves that parser and executor mechanics behave in a perfectly checkable
world. A billion-byte embedding tensor proves something else: the engine must
account for large byte ranges, move selected tensor data through the backend
path, preserve file identity, release owned memory, and still compare the
result against artifact evidence. Adding RMSNorm turns the selected slice into
a small model-backed graph segment rather than a single-tensor demonstration.

The larger goal is one coherent execution chain. Source weights become
artifact evidence. Artifact evidence becomes descriptors. Descriptors become
backend residency. Backend residency becomes engine state. Engine state feeds
graph execution. Token input becomes prefill state. Prefill state connects to
KV. KV supports decode. Decode produces logits. Sampling turns logits into
tokens. CLI and server surfaces should sit above that chain as operator
interfaces, not beside it as substitutes for runtime work.

Model-building belongs near the runtime for the same reason. A local engine
needs to know where an artifact came from, how native tensor names map into
runtime roles, why a qtype was chosen, what the emitted GGUF is expected to
contain, and which checks passed before execution. Without that paper trail,
speed numbers and capability evaluations lose their anchor.

## Runtime architecture

YVEX follows the path from model evidence to execution.

The path begins before the final runtime artifact. Source manifests record
where weights came from. Native tensor inventories describe the original weight
set before conversion. Family maps translate model-native tensor names into
YVEX roles. GGUF then becomes the concrete container the engine can open,
validate, register, and execute against.

From that artifact, YVEX builds descriptors. This is where parsed data becomes
executable meaning. A descriptor does more than say that a tensor exists. For
the selected embedding path, it explains how `token_embd.weight` is shaped, how
a token index selects a slice, how many values the graph should produce, and
which byte range belongs to that operation. The descriptor is the point where a
file stops being only bytes and starts becoming a runtime object.

Materialization is the residency boundary. CPU and CUDA paths receive selected
tensor bytes and turn them into backend-owned storage. The runtime can report
what was planned, what moved, and how cleanup behaved. Once selected weights
are resident, the engine attaches them to engine-owned state. Sessions observe
that state through a defined lifetime relationship instead of reaching back
into loader internals.

Graph execution turns the loader into an engine. The controlled fixture lane
uses tiny F32 fixtures because exactness needs a closed world. In that lane the
executor can complete the full mechanical circuit under conditions where the
answer is known: the descriptor is built, the weights are attached, the backend
is chosen, output storage is created, the result is read back, the checksum is
compared, and owned memory is released. The point is not to make the fixture
impressive. The point is to make the executor boring before model-backed
tensors make the problem larger.

Standalone primitives serve the same purpose at the operation level. RoPE,
attention, matmul/projection, and MLP are not presented as model support. They
are isolated operation boundaries. Each primitive gives YVEX a place to prove
shape handling, backend dispatch, scratch ownership, reference comparison, and
cleanup before those operations are composed inside a full transformer block.
Model-backed attention, model-backed MLP, full layer scheduling, full
transformer prefill, decode, output-head logits, vocabulary sampling, and
generation still come later.

The selected-artifact lane brings model bytes into scheduled work. Embedding
lookup proves the first selected tensor path. RMSNorm adds a second tensor, a
second operation, and an independent reference comparison. This is still a
selected slice, not full transformer execution, but it already forces the
runtime to deal with multi-tensor scheduling, intermediate storage, backend
math, output comparison, and cleanup in a way tiny fixtures cannot.

Token input moves the system toward sequence processing. Explicit token lists
become bounded runtime objects. A selected token can drive the graph. The
prefill-state foundation records the result of running the selected segment
across a sequence. It does not merely print a pile of counters. It gives the
runtime a compact account of sequence execution: every position in the bounded
input passes through the selected graph segment, each token contributes to the
aggregate state, and the resulting summary is tied to checksums, byte
accounting, and cleanup. That summary is below full transformer prefill, but
it is already runtime state rather than argument parsing.

KV is now a concrete session-owned boundary. The current store is intentionally
small, but it proves the ownership model that later attention state needs.
Shape is explicit. Capacity is bounded. Mutation is checked. Reads are bounded.
Clear and close paths are tested. Overflow is not hidden; it becomes a visible
runtime condition. Minimal KV-backed prefill binding connects the
segment-summary prefill state to a diagnostic KV store. This is still not
attention-backed transformer prefill, decode, logits, sampling, or generation.
It is the first point where sequence state and KV ownership touch.

The intended execution chain is:

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

Each arrow is a boundary. YVEX tries not to hide those boundaries behind the
word `inference`.

## Model artifacts

YVEX keeps model weights outside git. The repository carries code, public
headers, documentation, tiny fixtures, and tests. Large artifacts remain
operator-local.

Current artifacts are selected pressure artifacts. They sit between tiny
fixtures and future full-model GGUFs. Their purpose is to force implemented
runtime behavior before the whole transformer path lands.

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
| Selected embedding GGUF | Main model-tensor pressure artifact | Operator-local storage | Exercises identity, range math, backend materialization, engine attachment, and selected embedding execution over a large F16 tensor. |
| Selected embedding plus RMSNorm GGUF | First model-backed multi-tensor segment | Operator-local storage | Adds a second tensor and a second operation, making graph scheduling and RMSNorm reference checks concrete. |
| Controlled F32 fixtures | Deterministic parser and graph tests | Repository tests | Keeps parser, corruption, executor, and backend behavior exactly checkable while model weights stay local. |
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

The alias is a local handle for a specific artifact and support level. It gives
the operator a stable name while YVEX keeps checking file identity, metadata
drift, tensor readiness, and graph admission.

## Model-building path

YVEX treats model-building as part of the engine path. A local runtime should
know why an artifact has its layout. That answer lives in source manifests,
tensor inventories, family maps, qtype policy, conversion constraints, and GGUF
template checks.

A local inference engine cannot treat `GGUF` as a file extension only. A useful
local artifact is the result of a chain of evidence. Source weights establish
provenance. Native inventory describes the tensors before conversion. Family
mapping connects external names to runtime roles. Template validation defines
what the artifact is supposed to contain. Selected emission produces the narrow
GGUF that the current engine can exercise. Integrity and graph admission keep
later commands grounded in checked facts.

The current source-to-selected path is intentionally explicit:

```sh
./yvex source-manifest create --hf-repo "deepseek-ai/DeepSeek-V4-Flash" --revision "main" --local-path "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --status in-progress --out "$HOME/lab/models/gguf/deepseek/deepseek-source-manifest.json"
./yvex native-weights --source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex tensor-map --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --limit 20
./yvex convert plan --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --out-plan "$HOME/lab/models/gguf/deepseek/deepseek-selected-plan.json"
./yvex convert emit --arch deepseek4 --native-source "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" --tensor embed.weight --target-qtype F16 --out "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf" --overwrite
```

That last command emits the selected embedding artifact only. It is not full
DeepSeek conversion and not generation.

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
ROCm are future lanes because high-memory local machines matter for the kind
of inference YVEX is designed to reach.

On a CPU-only developer machine, YVEX keeps parser behavior, descriptor
construction, token input, integrity checks, cleanup paths, KV ownership tests,
and small graph correctness easy to inspect. That lane is not meant to win
speed comparisons. It is the lane where correctness should be easiest to
understand.

CUDA is the current accelerated lane. It is used for selected materialization,
fixture parity, selected embedding execution, embedding-plus-RMSNorm, and
prefill-state summaries where CUDA is available. The point is not to claim
broad GPU model support. The point is to let the same checked boundaries run
through an accelerator path with checked tensor movement.

DGX Spark or GB10-class CUDA systems are pressure targets for larger artifacts,
memory pressure, backend allocation behavior, and future graph growth.
High-memory MacBooks and Mac Studios matter for future Metal work because
unified memory and fast local storage change the shape of local inference.
Strix Halo-class systems matter for the same reason on the AMD side. Those
future lanes are not current support claims.

YVEX currently claims CPU and CUDA only for the runtime surfaces that exist
today. A backend probe is not model support. A hardware profile is not an
implemented runtime path. A build flag is not a generation claim.

## Larger-model direction

YVEX is ultimately aimed at models large enough to justify local inference as
serious infrastructure.

The relevant machines are no longer only small laptops running small dense
models. High-memory workstations, Mac Studios, DGX Spark or GB10-class CUDA
systems, and future unified-memory AMD boxes create a different question: how
much of a semi-frontier model can be made resident, how expensive KV state
becomes, which qtypes preserve quality, and where throughput collapses.

A larger model changes the runtime shape. The model may fit only under a
particular quantization profile. KV can become a first-order memory object.
Scratch space competes with resident weights. Backend placement becomes part
of the execution plan. Long-context prefill stresses a different part of the
system than token-by-token decode. Distributed execution, if it arrives, will
mainly be about capacity and long-prefill throughput before it becomes a
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

GLM-5.2 is currently an official-source tensor target only. This path report
does not inspect safetensors, emit a GGUF, or claim GLM execution.

## Current runtime surface

YVEX currently exposes controlled fixture graph proofs, standalone F32 graph
primitive proofs, selected-artifact graph slices, diagnostic prefill/KV state,
and a bounded diagnostic generation path.

The controlled lane uses tiny F32 fixtures because the executor needs a place
where correctness is not negotiable. In that lane, the values are small enough
for direct reference checks but complete enough to exercise owned-memory
behavior: data enters the backend, an operation runs, output is allocated,
results are read back, cleanup is observed, and the command reports enough
evidence to catch regressions. This is not a decorative test mode. It is the
stable ground that lets the executor grow without guessing.

The standalone primitive path does not pretend that isolated RoPE, attention,
matmul/projection, or MLP equals model execution. It gives each operation a
boundary before the transformer composes them. That boundary is useful because
every future full-model path will depend on the same boring facts: valid
shapes, owned scratch, backend dispatch, reference comparison, and cleanup.
When model-backed attention arrives, it should not also be the first time the
engine learns how to run an attention-shaped operation.

The selected-artifact lane uses operator-local DeepSeek-class artifacts.
The selected embedding path reads `token_embd.weight`, interprets it as
`[hidden_size, vocab_size]`, selects a token, converts the F16 row into an F32
vector, and checks the result against an independent raw-artifact reference
slice. That path matters because it joins artifact identity, byte-range safety,
backend residency, engine state, and graph execution on a tensor large enough
to expose large-tensor pressure.

The embedding-plus-RMSNorm path reads the same embedding row, applies RMSNorm
using the first-layer normalization weight, and checks the final vector by
checksum and max-diff. This is the current model-backed graph segment.
Model-backed attention and model-backed MLP still come later, but the segment already
establishes multi-tensor scheduling, intermediate memory planning, backend
RMSNorm support, cleanup, and reference comparison.

Token input is explicit. `yvex input tokens` parses bounded token sequences,
validates them against vocabulary facts when available, and lets graph commands
select a token by index. Prompt text becomes executable token input when
tokenizer metadata is present and the runtime path supports it.

The prefill-state foundation runs the selected embedding-plus-RMSNorm segment
across a token sequence. It is deliberately below attention-backed transformer
prefill. Its job is to turn a token sequence into owned runtime evidence. Each
token position is pushed through the selected graph segment, the segment output
contributes to a summary, and the runtime records enough checksum and
byte-accounting information to prove that the sequence was processed rather
than merely accepted as input. That summary gives later KV-backed and decode
paths something concrete to depend on.

The KV command proves the session-owned state boundary. It creates a bounded KV
store, mutates it, reads from it, reports its shape and byte footprint,
exercises clear behavior, and preserves the line between KV ownership and later
decode/logits work. This is not yet attention-backed KV, but it prevents KV
from remaining a vague future noun.

A short selected-artifact path is:

```sh
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

The selected segment path is:

```sh
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
```

The controlled block fixture path is:

```sh
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16
```

This is a controlled transformer-block-shaped fixture proof, not full model
inference.

The current prefill-state path is:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2
```

The minimal KV-backed prefill binding path is:

```sh
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

The minimal KV proof path is:

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
engine that fails with evidence instead of failing later with mystery.

## Speed and evaluation

YVEX should publish speed numbers only when the measured runtime path exists.

A useful benchmark row must identify the machine, backend, artifact, qtype,
prompt shape, context length, command, and reproducibility conditions. Prefill
and generation should be measured separately because they stress different
parts of the runtime. Long-context prefill is mostly about processing a large
token range into state. Decode is autoregressive and pays the cost of each new
token.

The public benchmark table should eventually compare CPU developer machines,
CUDA workstations, DGX Spark or GB10-class CUDA systems, high-memory Apple
Silicon machines, and future ROCm systems, but only after the relevant runtime
path exists. Today, fixture correctness is not capability evaluation. Selected
graph checks are not model quality. KV ownership tests are not prefill quality.
Logits regression becomes meaningful after an implemented logits path exists.
Capability regression belongs after generation uses the same runtime path users
run.

Measurement should follow the engine. Fixture correctness catches executor and
backend regressions early. Selected segment correctness catches tensor mapping,
dtype, backend, and graph-slice regressions. KV ownership checks catch shape,
append/read, clear, overflow, lifecycle, and cleanup regressions. Prefill
throughput becomes meaningful only when full transformer prefill exists. Decode
throughput requires existing KV-backed state. Generation throughput requires
the decode, logits, and sampling loop. Capability evaluation requires
implemented generation, not selected-slice proof.

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
answer a different runtime question.

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

Model weights stay outside git. Tracked GGUF files are tiny fixtures under
tests.

## Documentation

The public docs are intentionally small.

`docs/operator-runbook.md` is the command-first workflow for artifacts,
integrity, materialization, graph proofs, token input, prefill state, minimal
KV, daemon status, and validation. `docs/api.md` is the public C surface and
ownership map. `docs/contract.md` describes runtime behavior for CLI output,
filesystem state, registry resolution, backend boundaries, server status,
validation, and public claims. `MODEL_ARTIFACTS.md` records selected pressure
artifacts and validation posture. `AGENTS.md` records repository operating
rules. `docs/spine.md` is the internal delivery map, not product documentation.

## Acknowledgements

YVEX lives in the open-weight local inference ecosystem shaped by GGUF, GGML,
llama.cpp, CUDA tooling, and the people who kept native model execution
practical. The project is separate, but it is learning from that engineering
lineage.

## License

YVEX is licensed under the MIT license.
