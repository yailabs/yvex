# YVEX

YVEX is a native C inference engine for local open-weight models.

It is built below chat interfaces, provider APIs, and sampling. The project is
about the engine layer: model files becoming accountable runtime state, tensors
acquiring roles, weights becoming backend-resident, engine/session lifetimes
owning what they attach, graph work being scheduled, and eventually tokens being
produced by a runtime that can explain what it is doing.

YVEX is not a wrapper around another inference runtime, and it is not a
provider-shaped facade over an absent engine. It is being built from the lower
runtime upward so later prefill, `KV`, decode, logits, sampling, and generation
sit on real ownership boundaries rather than on vague claims such as `loaded`,
`running`, or `supported`.

The current runtime is still below full text generation, but it is already past
pure artifact inspection. It can inspect and validate local `GGUF` artifacts,
map tensor facts into runtime descriptors, materialize selected tensors on
`CPU` and `CUDA`, attach those tensors to engine-owned state, execute
deterministic fixture graphs, execute a real selected embedding segment, execute
a selected embedding-plus-RMSNorm segment over multiple real tensors, and route
explicit token input into those bounded graph paths. Those are engine proofs,
not full model execution.

The `GGUF`, artifact, registry, integrity, and model-building machinery is not
the product identity by itself. It exists because a local inference engine
cannot be honest unless tensor layout, qtype policy, backend memory, local model
identity, graph inputs, and failure boundaries are explicit.

## What YVEX is

YVEX is the native engine layer for local inference. It owns the lower runtime
path from local model artifacts to scheduled graph execution.

The important objects are not just files. They are runtime states: parsed model
facts, tensor roles, selected weights, backend allocations, engine-owned weight
tables, sessions, graph nodes, intermediate buffers, reference reads, token
inputs, and eventually token streams. YVEX keeps those states separate so they
can be tested, refused, cleaned up, and explained independently.

The public command surface is broad because YVEX exposes boundaries that most
local-inference stacks hide behind a single word. `inspect`, `metadata`, and
`tensors` show artifact facts. `models` gives operator-local artifacts stable
names without committing weights. `integrity report` summarizes local artifact
state. `materialize`, `engine`, and `session` show residency and ownership.
`graph` runs the bounded fixture and selected real-tensor graph paths. `run`,
`chat`, and `yvexd` are diagnostic/provider-shape surfaces until the engine can
produce real model output.

## Why this exists

Local inference is full of overloaded words. `Loaded` can mean a path exists, a
file opened, metadata parsed, tensors mapped, weights resident on a backend, a
graph scheduled, logits produced, or a server accepting requests. Those states
are not the same. When a local runtime fails, the difference between them is
where the useful diagnosis lives.

YVEX exists so local inference can be debugged and extended at the right runtime
boundary. A future prompt box is only honest if artifact intake, tensor mapping,
backend memory, graph execution, session state, and output stages are real below
it. The engine should be able to answer where a model stopped: artifact parse,
role mapping, identity drift, allocation, transfer, graph preflight, dispatch,
reference comparison, or an output-stage boundary that has not arrived yet.

That approach is practical. A selected embedding tensor materialized on `CUDA`
is valuable, but it is not a transformer. A deterministic fixture graph is
valuable, but it is not real-model inference. A selected embedding-plus-RMSNorm
segment is valuable, but it is still not prefill or logits. YVEX earns each word
before putting it in front of an operator.

## Model and hardware bet

YVEX currently uses DeepSeek-class open-weight models as pressure targets. The
reason is not branding; it is that large local models force real decisions about
tensor layout, memory placement, qtype boundaries, backend behavior, and runtime
ownership.

The target model family can change. YVEX should remain opportunistic about
open-weight models: if another model becomes the better local inference target,
the runtime can move, but support must be earned through tensor mapping,
artifact identity, backend residency, graph execution, and tests.

The backend lanes stay conceptually separate. `CPU` is the reference and
correctness lane. `CUDA` on Linux is the current acceleration lane. Metal on
macOS and ROCm on Strix Halo-class machines are important future local-inference
lanes because high-memory local machines matter to the project target. DGX
Spark / GB10 is a CUDA-class hardware pressure target. Naming those lanes is not
a support claim; it is engine design pressure.

## What YVEX is not

| YVEX is not | Boundary |
| --- | --- |
| a generic `GGUF` runner | container parsing is engine intake, not arbitrary model execution |
| a chat UI | `chat` is diagnostic until the engine can produce model output |
| a provider-compatible generation server | `yvexd` wires provider/status shape before generation exists |
| a benchmark leaderboard | token/sec and quality numbers wait for prefill, decode, logits, and generation |
| a model artifact repository | real weights and generated `GGUF`s stay operator-local |

## Runtime boundaries YVEX owns

| Path | Runtime role | Non-claim |
| --- | --- | --- |
| Model intake | turns local files into checked runtime facts | not model execution |
| Tensor mapping | assigns roles to named tensors and shapes | not backend residency |
| Selected materialization | moves selected weights into backend-owned storage | not full model loading |
| Engine/session ownership | attaches selected backend weights to engine state and exposes session visibility | not a transformer run |
| Controlled fixture graph | deterministic executor proof over tiny `F32` weights | not real-model inference |
| Selected embedding path | real `F16 token_embd.weight` participates in scheduled graph work | not prefill, logits, or generation |
| Selected embedding plus RMSNorm | multiple real tensors participate in scheduled graph work with memory planning and reference comparison | not prompt processing or full transformer execution |
| Token input boundary | explicit token sequences select the token consumed by bounded graph paths | not `KV`, decode, or sampling |

## Execution boundary

Parsing gives YVEX the artifact's declared structure: GGUF header, metadata,
tensor names, shapes, dtypes, byte offsets, and tokenizer facts where present.
Descriptor construction is the following boundary; it turns file facts into a
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

The current real-model boundary is the selected embedding-plus-RMSNorm segment:
more than one real op participates in scheduled runtime computation with real
attached tensors, memory planning, backend dispatch, regression output, cleanup,
and failure tests. It is still not prefill, decode, logits, sampling, or
generation.

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

A second selected-artifact path extends that proof to a larger real segment:
`token_embd.weight` feeds an embedding lookup, then `blk.0.attn_norm.weight`
feeds RMSNorm, and the final `F32` vector is compared with an independent
raw-artifact reference. This is multi-tensor graph work, not transformer
prefill.

Prompt/token input is now a runtime boundary for explicit token sequences:
`yvex input tokens` parses and bounds-checks token IDs, and `graph --tokens
IDS --token-index N` routes one validated token into the bounded fixture,
selected embedding, or selected embedding-plus-RMSNorm graph path. Prompt text
only becomes token input when executable tokenizer metadata is present; selected
artifacts without tokenizer metadata fail cleanly instead of pretending to
prefill.

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

The selected embedding-plus-RMSNorm segment expands the real graph boundary to
two real tensors and two scheduled ops:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --execute-segment \
  --segment embedding-rmsnorm \
  --partial-token 0
```

Expected status remains bounded:

```text
graph_kind: selected-embedding-rmsnorm
segment_graph_executed: true
segment_ops: 2
segment_op_0: embed
segment_op_1: rms_norm
segment_memory_plan: explicit
segment_output_count: 4096
segment_output_bytes: 16384
execution_ready: false
prefill_ready: false
logits_ready: false
generation: unsupported
status: real-segment-graph-executed
```

`CUDA`-capable hosts can run the same command with `--backend cuda`; CUDA uses a
tolerance-based max-diff check against the raw-artifact reference because RMSNorm
reduction order may produce tiny `F32` differences.

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

| Path | Runtime role | Non-claim |
| --- | --- | --- |
| Selected-artifact path | real artifact identity, selected materialization, backend residency, engine attachment, selected embedding segment execution | not full model graph, prefill, logits, or inference |
| Selected segment path | multiple real tensors, explicit segment memory plan, backend dispatch, raw-artifact reference comparison, optional explicit token sequence input | not prefill, logits, decode, or generation |
| Controlled fixture path | deterministic graph execution, backend dispatch, output readback over a tiny `F32` fixture | not real-model graph expansion or logits |

## Model intake and artifact discipline

A local inference engine cannot treat `GGUF` as a file extension only. The
engine needs the container because it carries tensor names, shapes, dtypes,
offsets, qtypes, tokenizer metadata, and model-family facts. YVEX keeps those
facts close to the runtime because graph execution and backend memory depend on
them.

The repository should be able to travel without the operator's model directory.
Real `GGUF`s, native safetensors, generated quantization outputs, local
registries, logs, reports, and build artifacts stay on the machine that owns
them. The repository keeps source, public headers, docs, tiny fixtures, tests,
and contracts.

| Class | Location |
| --- | --- |
| Source code, public headers, docs, tiny fixtures | repository |
| Real `GGUF` model artifacts | operator-local storage |
| Native safetensors / raw weights | operator-local storage |
| Generated `GGUF`s and quantization outputs | operator-local storage |
| `.yvex/models.local.json` | local ignored state |
| Build output, reports, logs | local/generated state |

The active pressure artifact is documented in
[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) without publishing a developer
workstation path:

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

That artifact is not "DeepSeek support." It is one large real tensor that
forces identity, shape, dtype, byte accounting, `CPU`/`CUDA` residency, engine
attachment, cleanup, and gate reporting to become real before the broader graph
exists. The selected embedding-plus-RMSNorm artifact extends that pressure to
two real tensors and two scheduled operations. The controlled `F32` fixture
stays separate so executor and backend behavior can be checked exactly.

Artifact integrity is the refusal boundary before runtime-owned state. Parse
checks, tensor accounting, digest identity, metadata drift checks, materialize
preflight, and graph-entry guards must pass before allocation, transfer,
dispatch, or raw reference reads. These checks are local runtime evidence, not
supply-chain security, malware analysis, model quality validation, or inference
readiness.

The local registry exists because real artifact paths are long and
machine-specific. `.yvex/models.local.json` is ignored local state, and
`YVEX_MODELS_REGISTRY` can point commands at another local registry when a
machine needs it. An alias removes path friction; it does not skip parsing,
identity checks, materialization, attachment, graph guards, or output
comparison.

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models verify local-model
./yvex integrity report --model local-model --backend cpu --require-token-embedding --partial-token 0
./yvex inspect local-model
./yvex tensors local-model
```

Tiny `GGUF` fixtures are different from real model artifacts. Fixtures belong in
tests because they validate parser edges, malformed headers, offset handling,
tokenizer metadata, tensor ranks, layout failures, and controlled graph
execution without pretending to be model inventory.

## Model-building tools

YVEX keeps source provenance next to artifact work because a local runtime needs
more than the final container. The source manifest records where open weights
came from. Native safetensors inventory gives a payload-free view of source
tensors. Tensor mapping is where family-native names become YVEX roles and
proposed `GGUF` names. Template validation catches structural expectations
before conversion. Selected conversion keeps scope narrow: one explicit family,
source, tensor, qtype, and output artifact.

```text
official/open weights
  -> source manifest
  -> native safetensors inventory
  -> family tensor mapping
  -> GGUF template validation
  -> selected emission
  -> selected materialization
  -> engine attachment
  -> bounded graph execution
```

The command shape follows that lineage. The examples below are about inventory,
mapping, planning, and selected emission; they are not a full-model conversion
recipe.

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
support. A registered artifact is not execution readiness. A generated `GGUF`
still has to pass the later engine boundaries.

## Qtype and quantization boundaries

Quantization in YVEX is a set of separate runtime facts, not a single support
bit. A qtype can be recognized as artifact storage while no backend kernel
computes with it. A policy can allow or reject a qtype for an artifact shape. A
selected emission path can write a requested qtype. An external quantization job
or imatrix manifest can record how an artifact was produced. Those states are
useful only if they stay separate.

| Subsystem | Why it exists inside the engine |
| --- | --- |
| Storage recognition | identifies qtypes in artifact metadata |
| Quant policy | makes qtype admission explicit before artifact emission or use |
| Selected emission | writes narrow `GGUF` outputs under explicit constraints |
| Quant-job provenance | records external quantization work without hiding it in runtime behavior |
| Imatrix provenance | records calibration evidence without turning it into compute coverage |
| Backend compute | remains a kernel-by-kernel property, not a qtype label |

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

These tools make provenance and policy explicit before kernels and graph
execution rely on them. They do not imply that every artifact described by a
manifest can be executed by YVEX.

## Backends and machine pressure

Local inference is constrained by memory movement, backend residency, scratch
planning, and cleanup before it is constrained by API shape. Tensor bytes have
to land somewhere. Device allocations have to fail cleanly. Future `KV`, scratch
and logits buffers have to be sized before prefill and decode become meaningful.

`CPU` keeps correctness and reference behavior legible. `CUDA` is the current
accelerated pressure lane for Linux: device discovery, memory accounting,
allocation, transfer, selected materialization, engine attachment, fixture
parity, selected embedding execution, and selected embedding-plus-RMSNorm
execution. Metal on macOS and ROCm on Strix Halo-class machines remain important
future local-inference directions because high-memory local machines are central
to the project target. DGX Spark / GB10 is a CUDA-class pressure target for
larger local runtime work.

Those names are not equal support claims. A DeepSeek-class runtime eventually
has to place many weights, schedule normalization, RoPE, attention, routed
experts, quantized matmul, scratch, `KV`, decode, and logits. The current graph
paths stay narrow because executor mechanics, ownership, and cleanup need to be
solid before those larger kernels arrive.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Operator path

The normal operator path is: name the local artifact, verify/report it,
materialize selected weights, attach engine/session state, and execute bounded
graph proofs. The runbook keeps the full command-first sequence; the README
keeps the public path short.

```sh
make
./yvex commands
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

For the complete path, use [docs/operator-runbook.md](docs/operator-runbook.md).
For C API ownership, use [docs/api.md](docs/api.md). For behavior guarantees,
use [docs/contract.md](docs/contract.md). The low-level commands remain useful
because they show exactly which runtime boundary is being exercised.

## Measurement and benchmarking posture

YVEX does not publish token/sec or model-quality numbers yet because the
generation path does not exist. That is not a lack of measurement discipline;
it is measurement discipline. The valid measurements today are
correctness/regression measurements at owned runtime boundaries: fixture graph
outputs, selected embedding checksums, and selected embedding-plus-RMSNorm
checksum/max-diff.

| Boundary | Valid measurement | Invalid claim |
| --- | --- | --- |
| Fixture graph | exact output values and checksum | model quality |
| Selected embedding | raw-artifact reference checksum and sample values | inference throughput |
| Selected embedding plus RMSNorm | checksum and max-diff against independent reference | token/sec or generation speed |
| Token input boundary | parse, bounds, token selection, and tokenizer fixture behavior | prefill or decode |
| Future generation path | eventual latency and quality evaluation | current result |

Fixture eval is not model quality eval. Logits regression is not generation
quality. Token/sec tables need model artifact, backend, qtype, context length,
machine, command, and reproducibility notes. Until prefill, decode, logits, and
generation exist in the same operator path, the honest result is no published
benchmark number.

## Expansion logic

YVEX should not jump from selected tensor execution to "inference" as a word.
The engine expands by making each boundary real before exposing the following one:
bounded graph work, explicit token input, prefill state, session-owned `KV`,
decode, logits, sampling, constrained generation, interactive CLI generation,
and provider generation backed by the same runtime. Each stage must have command
proof, tests, cleanup behavior, and documented limits before it becomes a public
claim.

## Provider boundary

`./yvexd` is an early provider/status boundary. It is useful because provider
shape can be wired before generation exists, but it must report generation
unavailable. It accepts a direct path or registered alias for `--model`, serves
status endpoints, and does not fake model output.

| Surface | Engine role | Non-claim |
| --- | --- | --- |
| `GET /health` | process and backend status | not model output |
| `GET /metrics` | metrics/status surface | not throughput benchmark |
| `GET /v1/models` | local model listing/status | not provider generation |
| `--model FILE_OR_ALIAS` | explicit path or registered alias resolution | not execution readiness |

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
