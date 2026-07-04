# Model Families

Model-family integration in YVEX is the staged transformation of an upstream
tensor set into engine-owned runtime semantics.

A family is not a name in a model card. It is an architectural class whose
configuration, tokenizer, tensor names, tensor shapes, attention structure,
feed-forward structure, KV semantics, and artifact constraints can be mapped
into YVEX runtime roles. A model family becomes executable only after those
roles are present in an artifact, admitted by integrity checks, placed by a
residency plan, consumed by graph lowering, and used by prefill, KV, decode,
logits, sampling, and generation code.

This document is the technical architecture contract for family integration. It
does not record delivery rows, active-next decisions, release gates, command
proof history, or implementation ledgers. Those belong in the internal spine.

## Integration Thesis

YVEX integrates models from tensors upward.

```text
source tensor set
  -> source/config/tokenizer evidence
  -> native tensor inventory
  -> family architecture signature
  -> tensor collection candidates
  -> canonical role mapping
  -> artifact contract
  -> artifact identity and integrity
  -> runtime descriptor
  -> residency plan
  -> backend lowering
  -> graph lowering
  -> prefill and KV
  -> decode
  -> logits
  -> sampling
  -> generation
```

A model is not integrated because YVEX knows its repository name. It is
integrated only when YVEX can explain how native tensors become canonical
roles, how those roles become backend-resident objects, how graph execution
consumes them, and how the resulting state participates in the runtime path.

Operational transcripts belong in runbooks. Artifact cards describe concrete
files. Benchmark records describe measurements. This file defines the
architecture that must be true before any of those surfaces can claim model
behavior.

## Architectural Invariants

| Invariant | Meaning |
| --- | --- |
| Source precedes artifact | Native tensors, config, and tokenizer facts must be understood before a runtime artifact can be trusted. |
| Identity is backend-neutral | A model target names a model/source object, not CUDA, Metal, CPU, hardware, benchmark, or future execution. |
| Classification precedes mapping | Dense, sparse, MoE, hybrid, multimodal, source-only, or selected-slice class must be known before canonical roles are assigned. |
| Collection validation precedes role mapping | Lexical tensor names are not enough; layer, shape, dtype, shard, and family rules must be consistent. |
| Role mapping precedes descriptor | A runtime descriptor cannot be valid if native tensors are not mapped to canonical engine roles. |
| Artifact contract precedes support | GGUF validity is not enough; the artifact must satisfy family-specific runtime requirements. |
| Integrity precedes residency | Tensor ranges, shapes, dtypes, qtypes, and metadata must be checked before backend allocation or transfer. |
| Residency precedes execution | Graph execution consumes resident runtime state, not abstract tensor names. |
| Graph proof precedes runtime claim | A mapped tensor set is not runtime support until the corresponding graph path executes. |
| KV proof precedes decode | Decode waits for real attention-backed KV writes and reads, not diagnostic KV alone. |
| Generation precedes evaluation | Capability evaluation only has meaning once generation uses the same runtime path users run. |
| Benchmark follows measured runtime | Hardware fit, shape estimates, or primitive tests are not throughput evidence. |

## Canonical Objects

| Object | Definition |
| --- | --- |
| Model family | A repeated architecture pattern: tensor naming, block grammar, tokenizer behavior, attention layout, FFN/MoE structure, KV semantics, and runtime expectations. |
| Model target | A concrete model/source instance inside a family, with size, source location, revision, tokenizer, config, license, and expected artifact class. |
| Backend pressure | A field describing which backend questions a target stresses. It is not target identity. |
| Source tensor set | The upstream tensor collection before YVEX conversion or artifact production. |
| Native tensor inventory | Header/config-level listing of native names, shapes, dtypes, shards, config facts, and tokenizer files. |
| Architecture signature | The structured family/target facts required before role mapping and runtime planning can be meaningful. |
| Tensor collection | A group of native tensors that appear to implement one architectural role family, such as attention, FFN, MoE experts, output, or tokenizer state. |
| Canonical role map | Mapping from native tensor names into YVEX runtime roles with layer/expert/shape/dtype semantics preserved. |
| Artifact contract | The metadata, tensor coverage, layout, qtype policy, tokenizer facts, and integrity rules required for a YVEX runtime artifact. |
| Runtime descriptor | The canonical model description derived from artifact facts and role mapping. |
| Residency plan | Backend placement for weights, KV, scratch, experts, streamed tensors, output state, and cleanup. |
| Backend lowering | Translation from canonical roles and graph operations into backend-specific memory, launch, synchronization, and kernel behavior. |
| Graph lowering | Translation from descriptor facts into executable graph operations. |
| Runtime support | Execution through the real path: prefill, KV, decode, logits, sampling, and generation. |

A family may be classified without an artifact. An artifact may exist without
runtime support. A descriptor may exist without materialization. A selected
graph slice may execute without full model execution. These are distinct
architectural states.

## Model Identity vs Backend Pressure

Backend is an execution decision, not model identity.

| Layer | Example | Meaning | Not |
| --- | --- | --- | --- |
| Family | Qwen | Architecture family | Backend |
| Target | `qwen3-8b` | Concrete model/source target | Runtime support |
| Artifact | `qwen3-8b` YVEX GGUF | Produced runtime container | Generation |
| Backend | CUDA / Metal / CPU | Execution implementation | Model identity |
| Hardware profile | Spark / Apple Silicon | Machine pressure | Backend support |

A target id must not encode CUDA, Metal, CPU, hardware, runtime, benchmark, or
future execution path. Backend pressure can be recorded as a field because it
affects future artifact, qtype, residency, graph, and kernel decisions. It must
not replace model identity.

Correct split:

```text
target_id: qwen3-8b
family: Qwen
backend_selection: deferred
backend_pressure: metal-planned
runtime_claim: unsupported
generation: unsupported-full-model
benchmark_status: not-measured
```

```text
target_id: gemma-4-12b-it
family: Gemma
backend_selection: deferred
backend_pressure: cpu-cuda-baseline-planned
runtime_claim: unsupported
generation: unsupported-full-model
benchmark_status: not-measured
```

Qwen can force Metal portability questions. Gemma can force smaller dense
runtime questions. Neither target id should carry those backend decisions.

## GGUF Artifact Container

GGUF is the runtime artifact container. It is not CUDA identity. It is not
Metal identity. It is not generation.

A YVEX-produced GGUF should remain backend-neutral where possible. Backend
feasibility is decided by qtype support, tensor layout, residency, backend op
coverage, graph lowering, and runtime execution. A backend may impose artifact
constraints later, but those constraints must be stated as artifact/qtype/layout
requirements, not as model identity.

A structurally valid GGUF can still fail the model-family artifact contract if
required roles are missing, qtypes are unsupported, tokenizer metadata is
incomplete, or graph lowering has no consumer.

## Source Tensor Boundary

The source tensor boundary records the model before YVEX gives it runtime
meaning.

Required source facts include:

```text
model family
model target
source repository or local source path
revision or snapshot identity
configuration files
tokenizer files
tensor shard list
native tensor names
tensor shapes
tensor dtypes
parameter class
context configuration
attention configuration
feed-forward configuration
MoE configuration when present
license posture
```

The source tensor boundary is read-only. It does not imply conversion, artifact
production, materialization, graph execution, or runtime support. Its purpose is
to prevent later runtime claims from floating above unexamined model bytes.

Current source/model-family commands include:

```sh
./yvex model-target inspect qwen3-8b
./yvex model-target class-profile qwen3-8b --audit
./yvex model-target tensor-collection qwen3-8b --audit
./yvex model-target inspect gemma-4-12b-it
./yvex model-target class-profile gemma-4-12b-it --audit
./yvex model-target tensor-collection gemma-4-12b-it --audit
./yvex source-manifest report --family qwen --release v0.1.0 --audit
./yvex source-manifest report --family gemma --release v0.1.0 --audit
./yvex models download qwen3-8b --models-root "$HOME/lab/models" --auth auto --audit
./yvex models download gemma-4-12b-it --models-root "$HOME/lab/models" --auth auto --audit
```

Detailed operator flow belongs in runbooks. Model Families owns architecture.

Source download reports, receipts, manifests, and header-only inventories are
source-intake evidence. They do not verify upstream identity by themselves, hash
payloads, load tensor payload bytes, emit GGUF, register runtime artifacts,
materialize tensors, execute runtime paths, generate, evaluate, benchmark, or
mark a release ready.

## Qwen And Gemma Source Pressure

Qwen and Gemma are backend-neutral source-pressure lanes, not supported runtime
families.

`qwen3-8b` names a Qwen source target. It currently carries source target facts,
a header-metadata-only model-class profile, and a header-only tensor collection
inventory. Its Metal pressure is recorded as backend pressure because Qwen can
force future unified-memory and backend lowering questions, but Metal is not
part of the target identity.

`gemma-4-12b-it` names a Gemma source target. It currently carries source
target facts, a header-metadata-only model-class profile, and a header-only
tensor collection inventory. Its CPU/CUDA baseline pressure is recorded as
backend pressure because Gemma can force dense runtime and artifact-shape
questions, but CUDA is not part of the target identity.

Both lanes still require canonical role mapping, artifact contracts, runtime
descriptors, graph lowering, and runtime execution before family support can be
claimed.

## Family Architecture Signature

A family must expose an architecture signature before YVEX can do meaningful
runtime work.

The signature should include:

```text
family name
target id
source repository or local source path
source/config status
tokenizer status
architecture class
decoder class
dense/sparse/MoE class
layer count
hidden size
intermediate size
attention type
number of attention heads
number of KV heads
head dimension
position mechanism
context length
normalization type
MLP type
MoE presence
expert count
active expert count
shared expert policy
output head policy
tokenizer policy
qtype/dtype constraints
backend pressure
runtime blockers
```

The architecture signature is not inferred from the model name alone. It is
derived from source/config/tokenizer/tensor metadata and becomes reliable only
after role mapping and artifact validation. An architecture signature is not
runtime support.

## Family Classification

Family classification assigns the model to a runtime class.

| Class | Runtime meaning |
| --- | --- |
| Dense decoder | Fixed layer path; every token executes the same attention and dense feed-forward structure. |
| Sparse / MoE decoder | Shared path plus routed expert path; each token selects a subset of experts. |
| Hybrid | Architecture combines dense and sparse regions or special non-standard blocks. |
| Multimodal | Runtime requires non-text input towers, projectors, encoders, or cross-modal state. |
| Source-only | Source facts can be inspected, but no artifact or runtime path exists. |
| Selected-slice | A narrow real-tensor slice exists for pressure testing, but not full runtime support. |
| Unsupported | Required facts or runtime structures are absent or intentionally out of scope. |

Dense and sparse classification must be explicit. A release line may contain
both dense and MoE variants; those variants must be treated as separate runtime
classes even if they share branding, tokenizer lineage, or training family.

## Decoder Block Grammar

YVEX sees a model family as a grammar of required operations.

Dense decoder grammar:

```text
token embedding
-> attention norm
-> Q/K/V projection
-> position transform
-> attention score path
-> softmax/value accumulation
-> O projection
-> residual
-> FFN norm
-> dense MLP gate/up/down or equivalent
-> activation
-> residual
-> final norm
-> output head
```

Sparse/MoE decoder grammar:

```text
token embedding
-> attention norm
-> Q/K/V projection
-> position transform
-> attention
-> O projection
-> residual
-> MoE norm
-> router logits
-> top-k expert selection
-> expert dispatch
-> expert compute
-> expert accumulation
-> residual
-> final norm
-> output head
```

The grammar is not executable until native tensor names become canonical roles,
those roles satisfy an artifact contract, the artifact passes integrity,
residency is planned, backend op coverage exists, and graph lowering consumes
the roles.

## Canonical Tensor Role Space

YVEX maps native tensors into canonical runtime roles.

The common role space begins with:

```text
token embedding
attention norm
Q projection
K projection
V projection
O projection
position / RoPE metadata
feed-forward norm
MLP gate
MLP up
MLP down
final norm
output head
tokenizer
```

Sparse models extend the role space:

```text
router
routing bias or correction terms
expert gate
expert up
expert down
expert norm when present
shared expert gate
shared expert up
shared expert down
expert indexing metadata
expert activation policy
expert accumulation policy
```

A tensor role map is valid only if it preserves shape, dtype, layer index,
expert index, shard position, tied-weight semantics, and family-specific naming
rules. Role mapping is the first point where source tensors become
engine-addressable runtime structure.

## Tensor Collection Promotion

Header metadata becomes runtime structure through a promotion chain:

```text
lexical pattern counters
-> candidate collection
-> validated collection
-> canonical role map
-> runtime descriptor
-> graph consumer
-> runtime-consumed state
```

Collection states:

```text
pattern-observed:
  tensor names match known lexical patterns.

collection-candidate:
  tensors appear to belong to a structural group, but shape/layer/expert rules are not fully validated.

collection-validated:
  names, shapes, dtypes, layer indices, and family rules are consistent.

role-mapped:
  native tensors are mapped to canonical YVEX roles.

graph-consumable:
  graph lowering has a consumer for the mapped roles.

runtime-consumed:
  implemented runtime execution consumes the roles.
```

The current Qwen model-class profile is lexical/header-metadata-only. It does
not map roles, validate complete tensor collections, or close runtime support.

## Attention Class Contract

A family attention contract must specify:

```text
attention class: MHA | MQA | GQA | MLA | family-specific
Q projection tensor rule
K projection tensor rule
V projection tensor rule
O projection tensor rule
num_heads
num_kv_heads
head_dim
RoPE or position rule
RoPE base/scale where known
causal mask rule
sliding-window or full-context policy
KV layout requirement
prefill write behavior
decode read behavior
attention dtype/qtype expectation
backend primitive requirements
scratch requirements
```

A standalone attention primitive is not attention support. A Q/K/V
name-pattern counter is not role mapping. A role map is not execution. An
attention report is not full transformer attention.

## KV Semantics Contract

KV is model-family dependent runtime state. Required KV facts include:

```text
layer count
attention head count
KV head count
head dimension
position indexing rule
cache layout
dtype/qtype
residency
capacity
prefill append/write rule
decode read rule
clear/reinit behavior
overflow behavior
paged/chunked future policy
```

Diagnostic KV proves ownership, shape, lifecycle, append/read mechanics, and
cleanup. Real KV support requires attention-backed K/V writes during prefill
and decode reads from that same state.

## Feed-Forward Contract

Dense FFN contract:

```text
gate projection
up projection
activation
elementwise combine
down projection
residual
```

MoE FFN contract:

```text
router
expert selection
expert gate/up/down
routing weight
expert output accumulation
shared expert when present
residual
```

A routed expert primitive is not MoE support. MoE support requires router
logits, top-k selection, expert dispatch, expert execution, accumulation, graph
integration, runtime state, tests, and command proof.

## Artifact Contract

The artifact contract defines what a runtime artifact must contain before YVEX
can treat it as an executable object.

A family-specific artifact contract includes:

```text
artifact format
architecture identifier
model target identity
tokenizer metadata
required global tensors
required per-layer tensors
required per-expert tensors
qtype policy by role
dtype policy by role
tensor ordering
alignment
byte-range validity
shape/rank/dtype constraints
output-head policy
KV metadata if stored
RoPE/position metadata
source manifest link
identity/digest status
support level
```

Artifact validity is a contract against runtime requirements, not only file
format validity.

A structurally valid GGUF file is not enough. A known alias is not enough. A
tensor table that parses is not enough. The artifact must satisfy the role
coverage and runtime requirements for the stage being entered. Selected
artifacts may satisfy a narrow contract. Full-runtime artifacts must satisfy the
complete contract for the chosen family class.

## Runtime Descriptor

The runtime descriptor is the bridge from artifact facts to execution
requirements. It is derived after artifact integrity and family mapping; it is
not inferred from model name alone.

A descriptor records:

```text
family
target
architecture class
dense or sparse class
layer structure
tensor collections
mapped tensor roles
tokenizer facts
attention layout
position mechanism
KV layout
feed-forward or MoE layout
qtype profile
backend requirements
residency expectations
graph requirements
prefill requirements
decode requirements
logits requirements
sampling requirements
support state
blockers
```

For dense models, the descriptor describes a fixed layer path. For sparse
models, the descriptor also describes routing, expert topology, active expert
policy, shared expert policy, and expert residency requirements.

The descriptor does not execute the model. It defines the execution contract
the runtime must satisfy.

## Backend Lowering Contract

Backend lowering is the translation from canonical roles and graph operations
into backend-specific memory, launch, synchronization, and kernel behavior.

CUDA lowering concerns:

```text
device allocation
host-to-device transfer
resident tensor lifetime
kernel launch shape
scratch ownership
memory coalescing
shared memory use
register pressure
tensor-core eligibility
fallback policy
cleanup
```

Metal lowering concerns:

```text
buffer allocation
unified-memory visibility
command buffer lifecycle
compute pipeline creation
threadgroup shape
resource synchronization
fallback policy
cleanup
```

A backend lowering plan is not backend runtime support. A CUDA primitive is not
CUDA full runtime. A Metal feasibility lane is not Metal backend
implementation.

## Residency Contract

Residency defines where runtime objects live.

For dense models, residency concerns:

```text
embedding weights
attention weights
dense MLP weights
norm weights
output head
KV cache
scratch buffers
logits buffer
```

For sparse models, residency adds:

```text
router weights
shared experts
resident routed experts
staged routed experts
streamed routed experts
expert cache
expert transfer path
expert dispatch buffers
expert accumulation buffers
```

Residency must be planned before full execution can be claimed. Materializing
one selected tensor does not imply full-model residency. Mapping all tensor
roles does not imply memory fit. Hardware plausibility does not imply backend
support.

## Graph Lowering Contract

Graph lowering converts descriptor facts into executable graph structure.

Dense lowering requires:

```text
embedding lookup
attention norm
Q/K/V projection
position operation
attention
O projection
residual
feed-forward norm
dense MLP
final norm
output head
```

Sparse lowering requires the dense attention path plus:

```text
router logits
top-k selection
expert lookup
expert dispatch
expert execution
expert weighting
expert accumulation
residual merge
```

Graph lowering is valid only when every required role has an executable
operation and every operation has backend support or a clear blocker. A selected
graph proof may lower only part of the graph. A full-runtime claim requires the
complete path.

## Dense Decoder Class

A dense decoder model has a fixed execution path per token.

Dense integration requires these collections:

| Collection | Required roles |
| --- | --- |
| Embedding | token embedding |
| Attention | attention norm, Q, K, V, O |
| Position | RoPE or equivalent position metadata |
| Feed-forward | FFN norm, gate/up/down or equivalent dense MLP tensors |
| Output | final norm, output head |
| Tokenizer | tokenizer model, special tokens, prompt format, EOS and stop facts |
| Runtime metadata | layer count, hidden size, head count, KV head count, context length, qtype profile |

Dense models are the cleanest class for proving the first full-runtime path
because they avoid router and expert semantics. The runtime still has to solve
attention, KV, dense MLP, output-head logits, sampling, tokenizer behavior, and
memory placement.

## Dense Promotion Path

```text
family classification
  -> source/config profile
  -> native tensor inventory
  -> dense collection validation
  -> dense role map
  -> tokenizer contract
  -> dense artifact contract
  -> dense runtime descriptor
  -> dense layer proof
  -> dense prefill
  -> real KV writes
  -> real decode reads
  -> output-head logits
  -> sampling
  -> generation
  -> evaluation
  -> benchmark
```

Dense promotion must not depend on router, expert indexing, expert dispatch, or
expert residency. If those concepts are required, the target is not dense.

## Dense Support States

| State | Architectural meaning |
| --- | --- |
| dense-source-profiled | Source and config facts are known. |
| dense-tensor-inventoried | Native tensors are listed with shape, dtype, and shard facts. |
| dense-role-mapped | Dense roles are mapped into canonical YVEX roles. |
| dense-artifact-contracted | Required dense artifact layout is defined. |
| dense-layer-proven | A complete dense transformer layer executes. |
| dense-prefill-proven | Real dense prefill produces runtime state. |
| dense-kv-proven | Real attention-backed KV writes and reads work. |
| dense-decode-proven | Decode advances through real dense runtime state. |
| dense-logits-proven | Output-head logits are produced. |
| dense-generation-proven | Sampling and append loop generate text through the dense runtime. |
| dense-evaluated | Capability regression uses the dense runtime path. |
| dense-benchmarked | Prefill and generation throughput are measured. |

## Sparse and MoE Class

A sparse or MoE model has a conditional execution path. A token passes through
shared structure, then a router selects one or more experts. The selected
experts execute, their outputs are weighted or accumulated, and the result
returns to the layer stream.

Sparse integration introduces distinctions dense integration does not need:

```text
total parameters
active parameters
shared parameters
routed expert parameters
resident parameters
streamed parameters
executed parameters
```

These quantities must remain separate. Active parameters describe compute.
Total parameters describe artifact size. Resident parameters describe memory
pressure. Streamed parameters describe movement policy. Executed parameters
describe the actual graph path for a token or batch.

## Sparse Tensor Collections

Sparse models require the dense collections plus MoE-specific collections.

| Collection | Required roles |
| --- | --- |
| Router | router weight, routing dtype, top-k policy, score normalization |
| Experts | expert gate/up/down tensors, layer index, expert index, expert count |
| Shared experts | shared expert tensors when used by the family |
| Accumulation | routing weights, combine rules, residual integration |
| Runtime metadata | expert count, active expert count, shared expert policy, routing policy |

Sparse role mapping is complete only when YVEX can identify which tensors are
shared, which tensors are routed, which expert each tensor belongs to, and how
expert selection affects execution.

## Sparse Promotion Path

```text
family classification
  -> source/config profile
  -> native tensor inventory
  -> sparse collection validation
  -> sparse role map
  -> expert inventory
  -> router contract
  -> expert contract
  -> tokenizer contract
  -> sparse artifact contract
  -> sparse runtime descriptor
  -> router proof
  -> expert execution proof
  -> expert dispatch proof
  -> sparse layer proof
  -> sparse prefill
  -> real KV writes
  -> real decode reads
  -> output-head logits
  -> sampling
  -> generation
  -> evaluation
  -> benchmark
```

The sparse path cannot skip router proof, expert execution proof, expert
residency policy, or expert dispatch. Sparse support begins only when routing
and expert execution participate in the actual runtime path.

## Sparse Support States

| State | Architectural meaning |
| --- | --- |
| sparse-source-profiled | Source and config facts are known. |
| sparse-tensor-inventoried | Native tensors are listed with shape, dtype, shard, layer, and expert facts. |
| sparse-classified | The target is confirmed as sparse or MoE. |
| sparse-role-mapped | Shared, router, and expert roles are mapped. |
| expert-inventoried | Expert count, layer grouping, tensor names, and active expert policy are known. |
| router-mapped | Router tensors and routing behavior are mapped. |
| sparse-artifact-contracted | Required sparse artifact layout is defined. |
| router-proven | Router logits and selection behavior are checked or executed. |
| expert-proven | At least one selected expert executes correctly. |
| expert-dispatch-proven | Runtime can select and dispatch experts from routing output. |
| sparse-layer-proven | Attention, router, experts, accumulation, and residual form one sparse layer proof. |
| sparse-prefill-proven | Real sparse prefill produces runtime state. |
| sparse-kv-proven | Real attention-backed KV writes and reads work through the sparse path. |
| sparse-decode-proven | Decode advances through real sparse runtime state. |
| sparse-logits-proven | Output-head logits are produced after sparse execution. |
| sparse-generation-proven | Sampling and append loop generate text through the sparse runtime. |
| sparse-evaluated | Capability regression uses the sparse runtime path. |
| sparse-benchmarked | Prefill and generation throughput are measured. |

## Family Adapter Contract

A family adapter translates family-specific source and artifact facts into YVEX
canonical runtime facts. The adapter owns interpretation, not execution.

It must define:

```text
architecture class
layer structure
native-to-canonical tensor mapping
required tensor collections
optional tensor collections
global tensors
per-layer tensors
per-expert tensors
tied tensors
position mechanism
attention layout
KV layout
feed-forward class
router semantics when sparse
expert topology when sparse
output-head structure
tokenizer and prompt constraints
accepted qtypes
required backend operations
residency assumptions
current blockers
```

The adapter must expose blockers precisely. If a tensor is mapped but no graph
operation can consume it, that is a graph blocker. If a graph operation exists
but no backend kernel supports it, that is a backend blocker. If the tensor
exists but cannot be placed, that is a residency blocker.

A family adapter does not make a model supported. It makes support or refusal
mechanically explainable.

## Family Adapter Output Contract

Adapter output is explanation, not execution.

```text
family_profile:
  family:
  target:
  class:
  source_status:
  config_status:
  tokenizer_status:
  metadata_status:
  tensor_count:
  dense_or_sparse:
  attention_class:
  ffn_class:
  moe_status:
  role_mapping_status:
  artifact_contract_status:
  runtime_descriptor_status:
  backend_selection:
  backend_pressure:
  runtime_claim:
  generation:
  benchmark_status:
```

The output should identify the next missing architectural fact without claiming
the runtime behavior that fact would enable.

## Decode, Logits, and Sampling Contract

Generation is the final runtime path, not a surface label.

The family must provide:

```text
real prefill state
real KV state
decode step
final hidden state
final norm
output head
vocabulary projection
logits buffer
sampling policy
EOS policy
stop-token policy
detokenization path
prompt continuation rules
```

A selected segment can prove that real tensors execute. It cannot prove
generation quality. Evaluation and benchmark work must wait until decode,
logits, sampling, and detokenization use the same runtime path that users run.

## Backend Pressure and Family Shape

CUDA and Metal are not model families. They are backend execution environments.
A family creates backend pressure through tensor shape, layout, dtype/qtype,
attention mode, KV size, MLP/MoE structure, output-head size, and residency.

YVEX has bounded CUDA primitive hardening. That proves selected primitive
behavior against references. It does not make any family CUDA-supported.

Metal remains a future backend pressure lane. Qwen can force Metal questions,
but Qwen identity remains backend-neutral.

## Performance-Relevant Family Facts

Family facts shape runtime cost:

```text
hidden size
intermediate size
layer count
attention type
head count
KV head count
context length
vocab size
output-head size
expert count
active expert count
shared expert policy
qtype/dtype mix
tensor shard layout
residency pressure
prefill/decode ratio
```

Two families with similar parameter counts can have very different runtime cost
because of attention layout, KV shape, output-head size, MoE activation, tensor
layout, and residency.

Backend performance is chosen after family facts become role-mapped runtime
facts.

## Current Family Posture

This table records posture, not support claims.

| Family | Current YVEX posture | Runtime class | Current evidence | Missing before runtime |
| --- | --- | --- | --- | --- |
| DeepSeek | selected-slice pressure | sparse/MoE | selected embedding and embedding-plus-RMSNorm graph slices | full artifact, tensor role map, MoE runtime, output head, generation |
| GLM | source/storage pressure | sparse/MoE | huge source/storage pressure reports | source completion, model-class, tensor map, artifact, storage/residency |
| Qwen | backend-neutral source target | dense candidate / family-dependent | `qwen3-8b` target, Qwen model-class profile, and Qwen tensor collection inventory | tensor role map, artifact, backend/runtime |
| Gemma | tensor-collection-profiled source target | dense candidate | `gemma-4-12b-it` target, Gemma model-class profile, and Gemma tensor collection inventory | tensor role map, artifact, runtime |
| Phi/Llama/Mistral | candidate families | dense/sparse depending target | architectural candidates | no current source target |

Current posture vocabulary includes `source-target-profiled`,
`model-class-profiled`, `tensor-collection-profiled`,
`source/storage-pressure`, `selected-slice-proof`, and `runtime-unsupported`.

## Support-Level Lattice

YVEX uses support levels as a lattice, not as marketing status.

Shared early path:

```text
not-studied
-> paper-studied
-> source-profiled
-> tensor-inventoried
-> family-classified
-> role-mapped
```

Dense branch:

```text
dense-artifact-contracted
-> dense-layer-proven
-> dense-prefill-proven
-> dense-kv-proven
-> dense-decode-proven
-> dense-logits-proven
-> dense-generation-proven
```

Sparse/MoE branch:

```text
expert-inventoried
-> router-mapped
-> sparse-artifact-contracted
-> router-proven
-> expert-proven
-> expert-dispatch-proven
-> sparse-layer-proven
-> sparse-prefill-proven
-> sparse-kv-proven
-> sparse-decode-proven
-> sparse-logits-proven
-> sparse-generation-proven
```

A support level must be backed by implementation evidence, not intention.

## Family-to-Target Promotion

A family becomes a concrete target when the target is specific enough to
produce source and artifact facts.

Required target facts:

```text
family
target name
architecture class
parameter class
dense or sparse class
source location
revision or snapshot
configuration facts
tokenizer facts
license posture
expected artifact class
backend pressure
runtime reason
first proof target
```

A target should be chosen because it closes or stresses a runtime layer: dense
transformer path, sparse router path, expert residency, tokenizer behavior,
long-context KV, output-head logits, quantization, backend placement, or
benchmark measurement. Popularity alone is not a target-selection rule.

## Target-to-Artifact Promotion

A target becomes an artifact only when YVEX can define the artifact contract.

Required artifact facts:

```text
source tensor inventory
canonical role map
required tensor subset or full tensor set
metadata requirements
tokenizer requirements
qtype policy
tensor count expectations
tensor byte expectations
shape constraints
dtype constraints
integrity checks
support level
```

Selected artifacts are valid. They are pressure artifacts, not fake artifacts.
They let YVEX test real tensor behavior before full-model support exists. A
selected artifact must still declare its support boundary.

## Artifact-to-Runtime Promotion

An artifact becomes runtime support only through execution.

Dense runtime promotion:

```text
artifact integrity
-> materialization
-> embedding
-> attention
-> dense MLP
-> full dense layer
-> full dense prefill
-> KV writes
-> decode reads
-> logits
-> sampling
-> generation
```

Sparse runtime promotion:

```text
artifact integrity
-> materialization
-> embedding
-> attention
-> router
-> expert selection
-> expert execution
-> expert accumulation
-> sparse layer
-> full sparse prefill
-> KV writes
-> decode reads
-> logits
-> sampling
-> generation
```

The artifact-to-runtime path is the real support boundary. Everything before it
is classification, planning, or selected proof.

## Family Classification Table

This table records families as integration classes, not support claims.

| Family | Runtime class | Architectural pressure | Current posture |
| --- | --- | --- | --- |
| DeepSeek | Sparse / MoE | Large sparse runtime, expert routing, KV pressure, high-end local inference | selected-slice-proof |
| GLM | Sparse / MoE | Huge source inventory, model-class pressure, reasoning/coding target class | source/storage-pressure |
| Qwen | Dense or Sparse / MoE depending target | Dense/sparse comparison, tokenizer/runtime comparison, portability pressure | tensor-collection-profiled for `qwen3-8b` |
| Gemma | Dense | Smaller local runtime and device-oriented pressure | tensor-collection-profiled for `gemma-4-12b-it` |
| Phi | Dense / compact reasoning | Small reasoning under constrained hardware | candidate family |
| Llama | Dense / sparse / multimodal depending target | Ecosystem baseline and common runtime assumptions | candidate family |
| Mistral | Dense / sparse depending target | Efficient local/server runtime and smaller sparse baselines | candidate family |

## Dense Candidate Matrix

| Family | Candidate scale | Runtime reason | First architectural proof |
| --- | --- | --- | --- |
| Qwen dense | small to medium | Fixed dense transformer path and tokenizer study | source/config profile and dense role map |
| Gemma | small to medium | Single-machine dense runtime pressure | model-class profile, then dense artifact contract |
| Phi | small | Compact reasoning under low memory | tokenizer report and dense graph slice |
| Llama dense | medium to large | Ecosystem compatibility baseline | dense role map and descriptor |
| Mistral dense | small to medium | Efficient local/server runtime | dense prefill target |

Dense candidates should close the fixed transformer path before YVEX uses them
as benchmark or serving targets.

## Sparse Candidate Matrix

| Family | Candidate scale | Runtime reason | First architectural proof |
| --- | --- | --- | --- |
| DeepSeek | large MoE | Current sparse pressure target | MoE tensor collections and sparse layer proof |
| GLM | large MoE | Source and architecture pressure | expert inventory and source/config profile |
| Qwen MoE | small to large MoE | Dense/sparse comparison inside one family | router and expert role map |
| Llama MoE | large sparse/multimodal | Future sparse plus multimodal pressure | family classification report |
| Mistral MoE | smaller MoE | Simpler sparse baseline | expert dispatch proof |

Sparse candidates should close router, expert, sparse residency, and sparse
decode behavior before they become user-facing targets.

## Hardware Fit

Hardware fit is a planning input, not support.

YVEX separates:

```text
model might fit on a machine
artifact can be parsed
tensors can be mapped
weights can be materialized
graph can execute
runtime can prefill
runtime can decode
runtime can generate
runtime has been evaluated
runtime has been benchmarked
```

Only the later stages represent user-visible runtime behavior. Hardware class
matters because it shapes the residency and backend lowering contracts, not
because it grants runtime capability.

## Model-Family Failure Modes

Practical failure modes include:

| Failure mode | Meaning |
| --- | --- |
| source exists but config/tokenizer is missing | Source bytes are present, but architecture/tokenization cannot be trusted. |
| source tensors are present but metadata is malformed | Header evidence cannot produce reliable tensor facts. |
| tensor names are observed but roles are ambiguous | Lexical patterns are insufficient for canonical role mapping. |
| family classification is known but role map is incomplete | The architecture class is known, but runtime roles are not. |
| role map is complete but artifact contract is missing | YVEX knows roles but has no artifact requirements to enforce. |
| artifact is valid GGUF but qtype is unsupported | File structure is valid, but compute/storage policy blocks execution. |
| artifact materializes but graph consumer is missing | Tensors become resident, but no graph path consumes them. |
| graph primitive exists but target tensor layout is unsupported | The operation exists, but family/artifact layout does not match it. |
| backend supports an op but residency plan fails | Compute is possible, but placement or memory lifecycle is not. |
| prefill runs but writes no real KV | Runtime state is not attention-backed decode state. |
| decode exists but has no real KV read path | Decode cannot advance model state from attention history. |
| output head is missing or unmapped | Hidden states cannot become vocabulary logits. |
| tokenizer/stop boundary is missing | Token ids may exist, but user-facing text/stop behavior is incomplete. |
| benchmark is attempted before runtime path exists | Measurement would attach numbers to a non-existent execution path. |

## Non-Claims

This document does not claim support for any model family.

It does not claim dense runtime support. It does not claim sparse or MoE runtime
support. It does not claim tokenizer support, full artifact support, backend
support, generation, serving, evaluation, or benchmark performance.

Family-specific non-claims:

```text
Qwen model-class profile is not Qwen runtime support.
Gemma model-class profile is not Gemma runtime support.
A downloaded source tree is not source readiness.
A source manifest is not model execution.
A lexical tensor pattern is not a role map.
A role map is not graph execution.
A CUDA primitive is not CUDA runtime.
A Metal feasibility lane is not Metal support.
A backend pressure lane is not backend support.
A GGUF artifact is not generation.
A selected graph slice is not full model execution.
A benchmark target is not a benchmark result.
```

The contract is:

```text
family classification
  -> target selection
  -> tensor inventory
  -> canonical role map
  -> artifact contract
  -> runtime descriptor
  -> residency plan
  -> backend lowering
  -> graph lowering
  -> runtime execution
  -> generation
  -> evaluation
  -> benchmark
```

Dense and sparse families share the early path. They diverge at role mapping,
residency, graph lowering, and execution semantics. That divergence is the
architectural reason this file exists.
