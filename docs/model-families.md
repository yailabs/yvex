# Model Families

Model-family integration in YVEX is the staged transformation of an upstream tensor set into engine-owned runtime semantics.

A family is not a name in a model card. It is an architectural class whose tensors, configuration, tokenizer, attention structure, feed-forward structure, KV semantics, and artifact constraints can be mapped into the YVEX runtime.

The central classification is dense versus sparse. Dense and sparse models share the early source and artifact stages, but they diverge at tensor role mapping, graph lowering, residency planning, execution semantics, and promotion criteria. A dense decoder path is fixed for every token. A sparse or MoE path is conditional: routing selects the expert subgraph that participates in each token step.

Operational commands belong in model-specific runbooks. Artifact facts belong in artifact cards. Performance belongs in benchmark records. This file defines the architectural contract that sits before those concrete surfaces.

## Integration Thesis

YVEX integrates models from tensors upward.

The source tensor set is the first architectural boundary. From that boundary, the engine derives inventory, classification, role mapping, artifact constraints, runtime descriptors, residency requirements, graph requirements, and execution state.

```text
source tensor set
  -> native tensor inventory
  -> family classification
  -> canonical role mapping
  -> artifact contract
  -> artifact identity and integrity
  -> runtime descriptor
  -> residency plan
  -> materialization
  -> graph lowering
  -> runtime state
  -> generation path
```

A model is not integrated because the repository knows its name. It is integrated only when YVEX can explain how its native tensors become runtime roles, how those roles become backend-resident objects, how graph execution consumes them, and how the resulting state participates in prefill, KV, decode, logits, sampling, and generation.

The promotion path is therefore evidence-based. Every stage must leave behind a checkable artifact: an inventory, a role map, a descriptor, a materialization report, a graph proof, a runtime proof, or a benchmark record.

## Architectural Invariants

The following invariants hold for all model families.

| Invariant                          | Meaning                                                                                                    |
| ---------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| Source precedes artifact           | Native tensors and configuration must be understood before a runtime artifact can be trusted.              |
| Classification precedes mapping    | Dense, sparse, hybrid, multimodal, or source-only class must be known before canonical roles are assigned. |
| Role mapping precedes descriptor   | A runtime descriptor cannot be valid if native tensors are not mapped to canonical engine roles.           |
| Artifact contract precedes support | A valid file format is not enough; the artifact must satisfy the family-specific runtime contract.         |
| Integrity precedes residency       | Tensor ranges, shapes, dtypes, and metadata must be checked before backend allocation or transfer.         |
| Residency precedes execution       | Graph execution consumes backend-resident state, not abstract tensor names.                                |
| Graph proof precedes runtime claim | A mapped tensor set is not runtime support until the corresponding graph path executes.                    |
| KV proof precedes decode           | Decode is not supported until real attention-backed KV state is written and read by the runtime.           |
| Generation precedes evaluation     | Capability evaluation only has meaning once generation uses the same runtime path users run.               |
| Benchmark follows measured runtime | Hardware fit, estimated memory, or command shape is not a benchmark.                                       |

These invariants prevent model-family study from turning into unsupported runtime claims.

## Canonical Objects

YVEX separates the architectural objects involved in model integration.

| Object                  | Definition                                                                                                                                                         |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Model family            | A repeated architectural pattern: tensor naming, block structure, tokenizer behavior, attention layout, FFN/MoE structure, KV semantics, and runtime expectations. |
| Model target            | A specific model instance inside a family, with size, source location, revision, tokenizer, config, license, and expected hardware class.                          |
| Source tensor set       | The upstream tensor collection before YVEX conversion or artifact production.                                                                                      |
| Native tensor inventory | A checked listing of native names, shapes, dtypes, shards, config facts, and tokenizer files.                                                                      |
| Family classification   | The architectural class assigned to the target: dense, sparse/MoE, hybrid, multimodal, source-only, selected-slice, or unsupported.                                |
| Canonical role map      | The mapping from native tensor names into YVEX runtime roles.                                                                                                      |
| Artifact contract       | The metadata, tensor coverage, layout, qtype policy, tokenizer facts, and integrity rules required for a runtime artifact.                                         |
| Runtime descriptor      | The canonical model description derived from artifact facts and role mapping.                                                                                      |
| Residency plan          | The backend placement plan for weights, KV, scratch, experts, streamed tensors, and output state.                                                                  |
| Materialization proof   | Evidence that required tensors can become backend-resident under the selected placement policy.                                                                    |
| Graph lowering          | Translation from descriptor facts into executable graph operations.                                                                                                |
| Runtime support         | Execution through the real path: prefill, KV, decode, logits, sampling, and generation.                                                                            |

A family may be classified without an artifact. An artifact may exist without runtime support. A runtime descriptor may exist without materialization. A selected graph slice may execute without full generation. These are distinct architectural states.

## Source Tensor Boundary

The source tensor boundary records the model before YVEX gives it runtime meaning.

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

The source tensor boundary is read-only. It does not imply conversion, artifact production, materialization, graph execution, or support.

Its purpose is to prevent later runtime claims from floating above unexamined model bytes.

## Qwen And Gemma Source Pressure

Qwen/Metal and Gemma are future source-pressure lanes, not supported runtime
lanes. The current command-visible source surface is:

```sh
./yvex model-target list
./yvex model-target inspect qwen-metal-portability
./yvex model-target inspect qwen-metal-portability --paths
./yvex model-target inspect gemma-dense-portability
./yvex model-target inspect gemma-dense-portability --paths
./yvex source-manifest report --family qwen --release v0.1.0
./yvex source-manifest report --family qwen --release v0.1.0 --output table
./yvex source-manifest report --family qwen --release v0.1.0 --audit
./yvex source-manifest report --family gemma --release v0.1.0
./yvex source-manifest report --family gemma --release v0.1.0 --output table
./yvex source-manifest report --family gemma --release v0.1.0 --audit
```

`qwen-metal-portability` is source-target profile only. It is a pressure-target
slot for Qwen family, the `<models_root>/hf/qwen/qwen-metal-portability` source
path convention, official source tensor expectation, future-YVEX-produced-GGUF
artifact class, and Apple Silicon / Metal portability pressure. It is pending
source/config verification.

`gemma-dense-portability` is source-target profile only. It is a pressure-target
slot for Gemma family, the `<models_root>/hf/gemma/gemma-dense-portability`
source path convention, official source tensor expectation,
future-YVEX-produced-GGUF artifact class, and dense-candidate-pending-source-config
pressure. It is pending source/config verification.

Source artifact class fields are command-visible in normal/table/audit source
reports and in target audit reports. The stable values used by these pressure
lanes include:

```text
official-source-tensors-planned
official-safetensors
official-safetensors-huge
official-config-tokenizer-sidecars
YVEX-produced-selected-GGUF
future-YVEX-produced-GGUF
external-GGUF-reference
external-runner-reference
unknown-source-artifact
```

These values classify evidence only. They do not claim source readiness,
artifact emission, runtime support, generation, or benchmark readiness.

Source footprint fields are also command-visible for Qwen and Gemma source
pressure reports. They count only top-level regular files and byte sizes from
the configured or explicit source directory. The report classifies
`.safetensors`, `.bin`, `.dat`, JSON/config/tokenizer sidecars, total bytes,
sidecar bytes, other bytes, largest file, and footprint class without reading
tensor payloads, parsing safetensors headers, parsing tokenizer JSON, hashing
files, creating manifests, or proving provenance.

Source provenance fields are command-visible as classification state only. They
report whether the source is planned official or a local/explicit path, expose
authority/status vocabulary, keep revision/commit/tag unknown when not known,
and report README/LICENSE presence without parsing contents. They do not perform
remote lookup, hash files, verify upstream identity, create manifests, prove
source identity, or imply source readiness.

Native safetensors inventory fields are command-visible for Qwen and Gemma as
header-only source evidence. They open top-level `.safetensors` files, read
header lengths and JSON header bytes, count tensor records, dtype/rank/shape
summaries, declared data bytes, and malformed headers, and keep payload bytes
unloaded. Malformed headers are reported as source evidence, not executed or
treated as runtime readiness.

Source tensor metadata inventory is also header-derived only. It records tensor
names, file placement, dtype, rank, shape, element counts, declared byte spans,
largest tensor metadata, dtype/rank distributions, and lexical name-pattern
summaries without loading tensor payloads. The name-pattern summary is
lexical-only; it does not map tensors to runtime roles, infer Qwen or Gemma
model classes, or imply runtime readiness.

Source manifest/provenance hardening is command-visible for Qwen and Gemma
source pressure reports. It reports manifest expectation, path/status, shallow
schema/family/target consistency, manifest sub-status fields, and explicit
no-create/no-remote/no-hash/no-payload boundaries. It does not create
manifests, perform remote lookup, hash files, load tensor payloads, prove source
readiness, infer model classes, or imply runtime readiness.

The report checks only local source-path pressure facts: concrete target slot,
configured source path, source path existence, config/tokenizer file visibility,
top-level safetensors presence, top-level source footprint, provenance fields,
native safetensors inventory, source tensor metadata inventory, source manifest
status, blockers, and next rows.
It does not download sources, create source manifests, imply source readiness,
emit artifacts, materialize tensors, load tensor payloads, implement Metal,
execute Qwen or Gemma runtime paths, generate, evaluate, benchmark, or mark a
release ready.

Source family/profile fields, source artifact class fields, source footprint
fields, source provenance fields, native safetensors inventory, and source
tensor metadata inventory are command-visible for Qwen and Gemma. The next
source pressure step is source manifest/provenance hardening without tensor
payload loading.

## Family Classification

Family classification assigns the model to a runtime class.

The primary classes are:

| Class                | Runtime meaning                                                                             |
| -------------------- | ------------------------------------------------------------------------------------------- |
| Dense decoder        | Fixed layer path; every token executes the same attention and dense feed-forward structure. |
| Sparse / MoE decoder | Shared path plus routed expert path; each token selects a subset of experts.                |
| Hybrid               | Architecture combines dense and sparse regions or special non-standard blocks.              |
| Multimodal           | Runtime requires non-text input towers, projectors, encoders, or cross-modal state.         |
| Source-only          | Source facts can be inspected, but no artifact or runtime path exists.                      |
| Selected-slice       | A narrow real-tensor slice exists for pressure testing, but not full runtime support.       |
| Unsupported          | Required facts or runtime structures are absent or intentionally out of scope.              |

Dense and sparse classification must be explicit. A family release line may contain both dense and MoE variants; those variants must be treated as separate runtime classes even if they share branding, tokenizer lineage, or training family.

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

A tensor role map is valid only if it preserves shape, dtype, layer index, expert index, shard position, and family-specific naming semantics.

Role mapping is not cosmetic. It is the first point where source tensors become engine-addressable runtime structure.

## Artifact Contract

The artifact contract defines what a runtime artifact must contain before YVEX can treat it as an executable object.

A family-specific artifact contract includes:

```text
architecture identifier
model target identity
required metadata
layer count
hidden size
attention layout
head count
KV head count
context length
tokenizer facts
required tensor collections
tensor role coverage
shape constraints
dtype constraints
qtype policy
output-head configuration
dense or sparse block structure
integrity requirements
support level
```

A structurally valid GGUF file is not enough.

A known alias is not enough.

A tensor table that parses is not enough.

The artifact must satisfy the role coverage and runtime requirements for the stage being entered. Selected artifacts may satisfy a narrow contract. Full-runtime artifacts must satisfy the complete contract for the chosen family class.

## Runtime Descriptor

The runtime descriptor is the bridge from artifact facts to execution requirements.

It is derived after artifact integrity and family mapping. It is not inferred from model name alone.

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

For dense models, the descriptor describes a fixed layer path.

For sparse models, the descriptor also describes routing, expert topology, active expert policy, shared expert policy, and expert residency requirements.

The descriptor does not execute the model. It defines the execution contract the runtime must satisfy.

## Dense Decoder Class

A dense decoder model has a fixed execution path per token.

The canonical dense layer is:

```text
hidden state
  -> attention norm
  -> Q/K/V projection
  -> position operation
  -> attention
  -> O projection
  -> residual
  -> feed-forward norm
  -> dense MLP
  -> residual
```

The canonical output path is:

```text
last hidden state
  -> final norm
  -> output head
  -> logits
  -> sampling
```

Dense integration requires the following collections.

| Collection       | Required roles                                                                     |
| ---------------- | ---------------------------------------------------------------------------------- |
| Embedding        | token embedding                                                                    |
| Attention        | attention norm, Q, K, V, O                                                         |
| Position         | RoPE or equivalent position metadata                                               |
| Feed-forward     | FFN norm, gate/up/down or equivalent dense MLP tensors                             |
| Output           | final norm, output head                                                            |
| Tokenizer        | tokenizer model, special tokens, prompt format, EOS and stop facts                 |
| Runtime metadata | layer count, hidden size, head count, KV head count, context length, qtype profile |

Dense models are the cleanest class for proving the first full-runtime path because they avoid router and expert semantics. The runtime still has to solve attention, KV, dense MLP, output-head logits, sampling, and memory placement, but token execution follows a single fixed graph.

## Dense Promotion Path

Dense promotion follows a fixed path.

```text
family classification
  -> source/config profile
  -> native tensor inventory
  -> dense role map
  -> tokenizer contract
  -> dense artifact contract
  -> selected artifact
  -> selected embedding proof
  -> selected attention or MLP proof
  -> full dense layer proof
  -> full dense prefill
  -> real KV writes
  -> real decode reads
  -> output-head logits
  -> sampling
  -> generation
  -> serving
  -> evaluation
  -> benchmark
```

Dense promotion must not depend on router, expert indexing, expert dispatch, or expert residency. If those concepts are required, the target is not dense.

## Dense Support States

| State                     | Architectural meaning                                             |
| ------------------------- | ----------------------------------------------------------------- |
| dense-source-profiled     | Source and config facts are known.                                |
| dense-tensor-inventoried  | Native tensors are listed with shape, dtype, and shard facts.     |
| dense-role-mapped         | Dense roles are mapped into canonical YVEX roles.                 |
| dense-artifact-contracted | Required dense artifact layout is defined.                        |
| dense-selected-artifact   | A narrow dense artifact exists.                                   |
| dense-slice-proven        | A selected dense graph slice executes.                            |
| dense-layer-proven        | A complete dense transformer layer executes.                      |
| dense-prefill-proven      | Real dense prefill produces runtime state.                        |
| dense-kv-proven           | Real attention-backed KV writes and reads work.                   |
| dense-decode-proven       | Decode advances through real dense runtime state.                 |
| dense-logits-proven       | Output-head logits are produced.                                  |
| dense-generation-proven   | Sampling and append loop generate text through the dense runtime. |
| dense-evaluated           | Capability regression uses the dense runtime path.                |
| dense-benchmarked         | Prefill and generation throughput are measured.                   |

## Sparse and MoE Class

A sparse or MoE model has a conditional execution path.

A token passes through shared structure, then a router selects one or more experts. The selected experts execute, their outputs are weighted or accumulated, and the result returns to the layer stream.

The canonical sparse layer is:

```text
hidden state
  -> attention norm
  -> Q/K/V projection
  -> position operation
  -> attention
  -> O projection
  -> residual
  -> MoE norm
  -> router logits
  -> expert selection
  -> selected expert execution
  -> expert accumulation
  -> residual
```

Sparse integration introduces distinctions that dense integration does not need:

```text
total parameters
active parameters
shared parameters
routed expert parameters
resident parameters
streamed parameters
executed parameters
```

These quantities must remain separate. Active parameters describe compute. Total parameters describe artifact size. Resident parameters describe memory pressure. Streamed parameters describe movement policy. Executed parameters describe the actual graph path for a token or batch.

A sparse model is not a dense model with a bigger MLP. It requires router semantics, expert topology, expert selection, expert residency, dispatch, accumulation, and sparse-specific failure modes.

## Sparse Tensor Collections

Sparse models require the dense collections plus MoE-specific collections.

| Collection       | Required roles                                                                                                        |
| ---------------- | --------------------------------------------------------------------------------------------------------------------- |
| Embedding        | token embedding                                                                                                       |
| Attention        | attention norm, Q, K, V, O                                                                                            |
| Position         | RoPE or equivalent position metadata                                                                                  |
| Router           | router weight, routing dtype, top-k policy, score normalization                                                       |
| Experts          | expert gate/up/down tensors, layer index, expert index, expert count                                                  |
| Shared experts   | shared expert tensors when used by the family                                                                         |
| Accumulation     | routing weights, combine rules, residual integration                                                                  |
| Output           | final norm, output head                                                                                               |
| Tokenizer        | tokenizer model, special tokens, prompt format, EOS and stop facts                                                    |
| Runtime metadata | layer count, hidden size, head count, KV head count, context length, expert count, active expert count, qtype profile |

Sparse role mapping is complete only when YVEX can identify which tensors are shared, which tensors are routed, which expert each tensor belongs to, and how expert selection affects execution.

## Sparse Promotion Path

Sparse promotion is longer than dense promotion.

```text
family classification
  -> source/config profile
  -> native tensor inventory
  -> sparse role map
  -> expert inventory
  -> router contract
  -> expert contract
  -> tokenizer contract
  -> sparse artifact contract
  -> selected non-expert artifact
  -> selected expert artifact
  -> selected embedding proof
  -> selected attention proof
  -> router proof
  -> expert execution proof
  -> expert dispatch proof
  -> sparse layer proof
  -> full sparse prefill
  -> real KV writes
  -> real decode reads
  -> output-head logits
  -> sampling
  -> generation
  -> serving
  -> evaluation
  -> benchmark
```

The sparse path cannot skip router proof.

It cannot skip expert execution proof.

It cannot skip expert residency policy.

It cannot collapse expert tensors into a generic MLP role.

Sparse support begins only when routing and expert execution participate in the actual runtime path.

## Sparse Support States

| State                      | Architectural meaning                                                               |
| -------------------------- | ----------------------------------------------------------------------------------- |
| sparse-source-profiled     | Source and config facts are known.                                                  |
| sparse-tensor-inventoried  | Native tensors are listed with shape, dtype, shard, layer, and expert facts.        |
| sparse-classified          | The target is confirmed as sparse or MoE.                                           |
| sparse-role-mapped         | Shared, router, and expert roles are mapped.                                        |
| expert-inventoried         | Expert count, layer grouping, tensor names, and active expert policy are known.     |
| router-mapped              | Router tensors and routing behavior are mapped.                                     |
| sparse-artifact-contracted | Required sparse artifact layout is defined.                                         |
| sparse-selected-artifact   | A narrow sparse artifact exists.                                                    |
| router-proven              | Router logits and selection behavior are checked or executed.                       |
| expert-proven              | At least one selected expert executes correctly.                                    |
| expert-dispatch-proven     | Runtime can select and dispatch experts from routing output.                        |
| sparse-layer-proven        | Attention, router, experts, accumulation, and residual form one sparse layer proof. |
| sparse-prefill-proven      | Real sparse prefill produces runtime state.                                         |
| sparse-kv-proven           | Real attention-backed KV writes and reads work through the sparse path.             |
| sparse-decode-proven       | Decode advances through real sparse runtime state.                                  |
| sparse-logits-proven       | Output-head logits are produced after sparse execution.                             |
| sparse-generation-proven   | Sampling and append loop generate text through the sparse runtime.                  |
| sparse-evaluated           | Capability regression uses the sparse runtime path.                                 |
| sparse-benchmarked         | Prefill and generation throughput are measured.                                     |

## Family Adapter Contract

A family adapter translates family-specific source and artifact facts into YVEX canonical runtime facts.

The adapter owns interpretation, not execution.

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

The adapter must expose blockers precisely. If a tensor is mapped but no graph operation can consume it, that is a graph blocker. If a graph operation exists but no backend kernel supports it, that is a backend blocker. If the tensor exists but cannot be placed, that is a residency blocker.

A family adapter does not make a model supported. It makes support or refusal mechanically explainable.

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

Residency must be planned before full execution can be claimed. Materializing one selected tensor does not imply full-model residency. Mapping all tensor roles does not imply memory fit. Hardware plausibility does not imply backend support.

The residency contract must separate:

```text
planned placement
allocated placement
resident placement
streamed placement
executed placement
released placement
```

This separation is especially important for sparse models, where total parameter count and active parameter count do not describe memory behavior by themselves.

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

Graph lowering is valid only when every required role has an executable operation and every operation has backend support or a clear blocker.

A selected graph proof may lower only part of the graph. A full-runtime claim requires the complete path.

## KV Contract

KV is model-family dependent runtime state.

The family adapter and descriptor must define:

```text
attention class
head count
KV head count
head dimension
context length
sliding or full context policy
compression policy when present
KV dtype
KV layout
KV residency
prefill write pattern
decode read pattern
save/load semantics
```

Diagnostic KV does not satisfy this contract. It can prove ownership and lifecycle mechanics, but it does not prove attention-backed prefill or decode.

A family is KV-supported only when real attention execution writes KV during prefill and decode reads that KV to advance the model state.

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

A selected segment can prove that real tensors execute. It cannot prove generation quality. Evaluation and benchmark work must wait until decode, logits, sampling, and detokenization use the same runtime path that users run.

## Support-Level Lattice

YVEX uses support levels as a lattice, not as marketing status.

```text
not-studied
  -> paper-studied
  -> source-profiled
  -> tensor-inventoried
  -> family-classified
  -> role-mapped
  -> artifact-contracted
  -> artifact-produced
  -> integrity-gated
  -> residency-planned
  -> materialization-proven
  -> graph-slice-proven
  -> layer-proven
  -> prefill-proven
  -> kv-proven
  -> decode-proven
  -> logits-proven
  -> generation-proven
  -> served
  -> evaluated
  -> benchmarked
```

Dense and sparse families share the early part of the lattice. They diverge after role mapping because their graph and residency obligations differ.

A support level must be backed by implementation evidence, not intention.

## Family-to-Target Promotion

A family becomes a concrete target when the target is specific enough to produce source and artifact facts.

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
expected hardware class
runtime reason
first proof target
```

A target should be chosen because it closes or stresses a runtime layer: dense transformer path, sparse router path, expert residency, tokenizer behavior, long-context KV, output-head logits, quantization, backend placement, or benchmark measurement.

Popularity alone is not a target-selection rule.

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

Selected artifacts are valid. They are pressure artifacts, not fake artifacts. They let YVEX test real tensor behavior before full-model support exists.

A selected artifact must still declare its support boundary.

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

The artifact-to-runtime path is the real support boundary. Everything before it is classification, planning, or selected proof.

## Family Classification Table

This table records families as integration classes, not support claims.

| Family        | Runtime class             | Architectural pressure                                                      | Current posture                |
| ------------- | ------------------------- | --------------------------------------------------------------------------- | ------------------------------ |
| DeepSeek      | Sparse / MoE              | Large sparse runtime, expert routing, KV pressure, high-end local inference | Active sparse pressure family  |
| GLM           | Sparse / MoE              | Huge source inventory, model-class pressure, reasoning/coding target class  | Source/storage pressure family |
| Qwen dense    | Dense                     | Dense ladder, tokenizer/runtime comparison, smaller full-runtime candidate  | Candidate dense family         |
| Qwen MoE      | Sparse / MoE              | Dense/sparse comparison inside one release line                             | Candidate sparse family        |
| Llama dense   | Dense                     | Ecosystem baseline and common runtime assumptions                           | Candidate dense family         |
| Llama MoE     | Sparse / MoE / multimodal | Sparse and multimodal future pressure                                       | Future sparse family           |
| Gemma         | Dense / lightweight       | Smaller local runtime and device-oriented pressure                          | Candidate dense family         |
| Phi           | Dense / compact reasoning | Small reasoning under constrained hardware                                  | Candidate dense family         |
| Mistral dense | Dense                     | Efficient local/server dense runtime candidate                              | Later dense family             |
| Mistral MoE   | Sparse / MoE              | Smaller sparse reference class                                              | Later sparse family            |

The table is architectural. It does not imply parsing support, artifact support, runtime support, generation, evaluation, or benchmark results.

## Dense Candidate Matrix

| Family        | Candidate scale | Runtime reason                                   | First architectural proof                |
| ------------- | --------------- | ------------------------------------------------ | ---------------------------------------- |
| Qwen dense    | small to medium | Fixed dense transformer path and tokenizer study | Source/config profile and dense role map |
| Gemma         | small to medium | Single-machine dense runtime pressure            | Dense artifact contract                  |
| Phi           | small           | Compact reasoning under low memory               | Tokenizer report and dense graph slice   |
| Llama dense   | medium to large | Ecosystem compatibility baseline                 | Dense role map and descriptor            |
| Mistral dense | small to medium | Efficient local/server runtime                   | Dense prefill target                     |

Dense candidates should close the fixed transformer path before YVEX uses them as benchmark or serving targets.

## Sparse Candidate Matrix

| Family      | Candidate scale         | Runtime reason                            | First architectural proof                     |
| ----------- | ----------------------- | ----------------------------------------- | --------------------------------------------- |
| DeepSeek    | large MoE               | Current sparse pressure target            | MoE tensor collections and sparse layer proof |
| GLM         | large MoE               | Source and architecture pressure          | Expert inventory and source/config profile    |
| Qwen MoE    | small to large MoE      | Dense/sparse comparison inside one family | Router and expert role map                    |
| Llama MoE   | large sparse/multimodal | Future sparse plus multimodal pressure    | Family classification report                  |
| Mistral MoE | smaller MoE             | Simpler sparse baseline                   | Expert dispatch proof                         |

Sparse candidates should close router, expert, sparse residency, and sparse decode behavior before they become user-facing targets.

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

Only the later stages represent user-visible support.

Hardware class matters because it shapes the residency contract, not because it grants runtime capability.

## Non-Claims

This document does not claim support for any model family.

It does not claim dense runtime support.

It does not claim sparse or MoE runtime support.

It does not claim tokenizer support, full artifact support, backend support, generation, serving, evaluation, or benchmark performance.

It defines the architectural path by which a model family becomes eligible for support.

The contract is:

```text
family classification
  -> target selection
  -> tensor inventory
  -> canonical role map
  -> artifact contract
  -> runtime descriptor
  -> residency plan
  -> graph lowering
  -> runtime execution
  -> generation
  -> evaluation
  -> benchmark
```

Dense and sparse families share the early path. They diverge at role mapping, residency, graph lowering, and execution semantics. That divergence is the architectural reason this file exists.
