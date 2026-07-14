# YVEX System Target

Date: 2026-07-14
Status: filesystem and module ownership contract
Authority: filesystem and module topology; current project state belongs only
to `PROJECT.md`

This document records the target filesystem architecture for the execution-core
and control-plane boundary. It is an implementation contract, not a capability
claim.

## Current Actual Tree

Current core areas:

```text
src/cli/                 CLI dispatch, input, surfaces, render, IO
src/source/              source manifests, provenance, inventory, payload trust/streaming
src/model/target/        model-target catalogs, exact coverage, maps, gates, qtype reports
src/model/architecture/  immutable typed family architecture specifications
src/model/artifacts/     model registry/ref/gate/report/write ownership
src/model/               dtype/model tables and runtime descriptor target
src/gguf/                GGUF parser plus target ABI/writer/roundtrip owners
src/artifact/            artifact IO, identity, integrity, descriptor gates
src/graph/               graph construction, plans, bind/execute target owners
src/backend/             backend abstraction, tensor/qtype/report ownership
src/generation/          legacy proof cells and future runtime implementation owners
```

## Target Tree Summary

```text
input -> command -> surface router -> report/domain -> render -> cli/io

file writer -> explicit local files only
source facts -> architecture IR -> coverage -> contribution map -> transformation IR
payload session -> bounded chunks -> transformation execution -> quantization
GGUF ABI -> artifact descriptor -> materialization -> runtime descriptor
runtime descriptor -> backend tensor binding -> graph bind -> graph execute
graph execute -> prefill -> KV -> decode -> logits -> sampling -> generation
```

No domain file owns command grammar or operator byte output. No renderer owns
domain algorithms. No writer owns command output.

## Gap Table

| Area | Current state | Target state | Next row |
| --- | --- | --- | --- |
| GGUF artifact ABI | scalable file-backed v3 reader, typed refusal, target-scale budgets, immutable view, and zero payload reads | preserve the structural reader as input to canonical layout and later artifact admission | see `PROJECT.md` |
| GGUF qtype storage | pinned IDs 0-39, exact row geometry, and typed refusal are canonical | preserve storage independently from decoder, quantizer, emitter, and compute support | see `PROJECT.md` |
| GGUF layout integrity | canonical directory-order spans, power-of-two alignment, zero padding, truncation/tail refusal, and snapshot drift checks | preserve global layout as input to complete-model artifact admission | see `PROJECT.md` |
| CUDA truth | context, bundle, functions, and exact variants are typed; no-bundle builds refuse kernels and generated-bundle variants have GB10 reference proof | preserve fail-closed admission while later architecture/runtime rows define and prove the DeepSeek operation set | see `PROJECT.md` |
| Architecture IR | immutable typed DeepSeek-V4-Flash model, 43 main-layer, and MTP topology consumed by release-target profiling | preserve the source-to-IR boundary and feed complete tensor-role derivation without source reparsing | see `PROJECT.md` |
| Tensor coverage | the IR-derived 69,187-slot DeepSeek requirement set reconciles exactly against one retained verified header snapshot | preserve one-to-one source coverage as the admission input to GGUF mapping | see `PROJECT.md` |
| GGUF names/layout | immutable 69,187-contribution to 1,360-descriptor concrete DeepSeek GGUF lowering plan | preserve the format projection while moving artifact-neutral transformation semantics to the compilation owner | see `PROJECT.md` |
| Source payload | verified snapshot-bound shard/range indexes, payload trust, and bounded transactional streaming | preserve as artifact-neutral byte input to transformation execution without decoding or reinterpretation | see `PROJECT.md` |
| Transformation plan | no artifact-neutral transformation IR exists | bind every logical output to exact source contributions and payload-range identity before physical lowering | see `PROJECT.md` |
| Quantization | policy and geometry facts | consume the canonical transformation plan for role-correct quantization or explicit refusal | see `PROJECT.md` |
| GGUF writer | explicit refusal | complete deterministic emitted bytes | V010.GGUF.WRITER.1 |
| Artifact emission | tensor proof files only | complete YVEX-produced DeepSeek GGUF | V010.ARTIFACT.EMIT.DEEPSEEK.0 |
| GGUF roundtrip | explicit refusal/bounded fixtures | complete artifact writer-reader equivalence | V010.GGUF.ROUNDTRIP.1 |
| Materialization | tensor proof only | every required tensor resident with cleanup | main path after repair closure |
| Runtime descriptor | target owner seed and reports | executable DeepSeek runtime descriptor | main path after materialization |
| Graph/backend | primitive and bounded proof residue | backend-bound complete transformer path | main path after descriptor |

## Owner Rules

- CLI input owns `argc/argv` parsing only.
- CLI command adapters dispatch only.
- CLI surfaces route command families only.
- CLI renderers format typed facts.
- CLI IO writes operator bytes.
- Explicit writer modules write local files only.
- Core shard-index foundation owns allocation-free canonical key admission and
  deterministic lookup over caller-owned entries. Source payload consumes it;
  future artifact shard owners may reuse it without inheriting source policy or
  implying an artifact payload reader.
- Source JSON owns bounded structured parsing primitives, without source policy.
- Source provenance owns pinned repository/revision and manifest facts.
- Source family adapters own raw configuration and tokenizer sidecar facts.
- Source inventory owns indexed or explicitly header-derived shard inventory
  and the single canonical safetensors header pass. Its retained immutable
  snapshot carries deterministic tensor identity and lookup facts to consumers.
- Source payload owns snapshot-bound shard and tensor-range indexes, payload
  trust identity, bounded page/chunk plans, secure read-only handle admission,
  exact positioned reads, resource budgets, and transactional consumer
  delivery. It consumes the retained snapshot and never reparses headers.
- Model-target coverage owns IR-derived source requirements and exact snapshot
  reconciliation; it does not own source IO, GGUF naming, transforms, or
  payload access.
- The DeepSeek mapping owner composes IR and complete coverage into indexed
  source contributions and projects them into typed transforms, canonical GGUF
  names, logical GGML shapes, metadata prerequisites, and deterministic
  identity. This is a concrete GGUF lowering, not the artifact-neutral
  transformation owner. GGUF name/layout primitives remain their format
  owners; neither side reads payloads or emits physical file bytes.
- The DeepSeek payload handoff owner binds every canonical mapping contribution
  to the common source payload index and plan. It proves coverage and pressure
  facts but performs no source transform, quantization, or artifact emission.
- The future compilation owner consumes architecture, coverage, source
  contribution, payload-range, physical-format, quantization, residency, and
  backend requirements to construct immutable plans and variant identities. It
  does not perform source IO, quantization, writing, allocation, kernel
  execution, evaluation, or benchmarking.
- Transformation-plan construction is metadata-only. Transformation execution
  consumes exact payload chunks later, and quantization may not rediscover
  source names, roles, aggregation axes, or scaling companions.
- Source verification coordinates those owners and decides blockers; it does
  not parse JSON, rescan headers, render, serialize, or independently read
  tensor payloads.
- Source writers atomically publish verifier-approved manifests and explicit
  derived inventory outside official model source trees. Payload trust is
  published only after complete digest verification or explicitly local
  sealing; partial trust state is never published.
- Model target owns target catalogs, maps, gates, and qtype reports.
- Model architecture owns immutable normalized topology built from successful
  strict source verification. It does not reopen source files, classify tensor
  names, map roles, or infer runtime support.
- Model artifacts own registry, references, gates, typed artifact reports, and
  explicit file writers.
- GGUF owns container ABI, metadata ABI, tensor_info ABI, canonical qtype
  identity and row-aware storage geometry,
  ranges, reader, writer, roundtrip, emitted names, emitted layout, descriptor,
  and GGUF reports.
- Artifact owns YVEX artifact descriptors, materialization boundary, roundtrip
  gates, identity, integrity, and artifact reports.
- Model owns runtime descriptor projection.
- Graph owns bind plans and graph execution boundary.
- Backend owns exact tensor, primitive, bundle, failure, cleanup, and qtype
  support/refusal facts.

## Forbidden Placements

- Domain command grammar.
- Domain operator byte output.
- Renderer file IO or backend execution.
- CLI files in `CORE_SRCS`.
- Writer modules writing command output.
- GGUF parsing language that implies materialization.
- Materialization language that implies backend execution.
- Backend language that implies graph or generation support.
- Payload, quantizer, or writer logic that becomes the transformation-semantics
  authority.
- Compilation coordination that reimplements source IO, quantization, writing,
  residency, backend execution, evaluation, or benchmark measurement.

## GGUF Target Map

| Owner | Boundary |
| --- | --- |
| `include/yvex/gguf_qtype.h` | public qtype identity, admission, and typed storage result |
| `include/yvex/artifact.h` | read-only file handle, optional explicit mapping, and exact positioned reads |
| `include/yvex/gguf.h` | public reader budgets, typed parse result, immutable view, metrics, and accessors |
| `include/yvex/gguf_layout.h` | public typed global layout result, failure categories, byte totals, and IO metrics |
| `src/gguf/gguf.c` | file-backed GGUF v3 decoding and owned metadata/tensor view |
| `src/gguf/yvex_gguf_container.c` | magic/version/container ABI |
| `src/gguf/yvex_gguf_metadata.c` | metadata key/value ABI |
| `src/gguf/yvex_gguf_tensor_info.c` | tensor_info name/rank/type/shape ABI |
| `src/gguf/yvex_gguf_qtype.c` | pinned qtype registry and row-aware tensor storage |
| `src/gguf/yvex_gguf_layout_integrity.c` | canonical ordered layout, padding, aggregate span, tail, and drift admission |
| `src/gguf/yvex_gguf_range_map.c` | bounded local range arithmetic and canonical layout projection |
| `src/gguf/yvex_gguf_reader.c` | reader policy, resource defaults, typed failure ABI, and report projection |
| `src/gguf/yvex_gguf_writer.c` | writer refusal until writer row |
| `src/gguf/yvex_gguf_roundtrip.c` | writer-reader equivalence boundary |
| `src/gguf/yvex_gguf_name_map.c` | emitted GGUF tensor names |
| `src/gguf/yvex_gguf_layout_map.c` | emitted tensor layout/range plan |
| `src/gguf/yvex_gguf_descriptor.c` | GGUF descriptor facts |
| `src/gguf/yvex_gguf_report.c` | typed GGUF report facts |

## Artifact And Materialization Target Map

| Owner | Boundary |
| --- | --- |
| `src/artifact/yvex_artifact_descriptor.c` | YVEX artifact descriptor facts |
| `src/artifact/yvex_artifact_materialize.c` | materialization refusal/input contract |
| `src/artifact/yvex_artifact_roundtrip_gate.c` | emitted artifact roundtrip gate |
| `src/artifact/yvex_artifact_report.c` | typed artifact summary facts |

## Runtime Descriptor Target Map

| Owner | Boundary |
| --- | --- |
| `src/model/yvex_runtime_descriptor.c` | artifact descriptor to runtime descriptor projection |
| `src/model/yvex_runtime_descriptor_report.c` | descriptor blocker report facts |

## Model Architecture Target Map

| Owner | Boundary |
| --- | --- |
| `src/model/architecture/yvex_deepseek_v4_ir.h` | private typed model, layer, attention, position, KV, mHC, MoE, output, tokenizer, source-constraint, failure, and borrowed-accessor ABI |
| `src/model/architecture/yvex_deepseek_v4_ir.c` | exact-source admission, cross-field validation, normalized topology derivation, immutable allocation, and cleanup |
| `src/source/yvex_source_inventory.[ch]` | retained immutable source tensor snapshot, deterministic identity, indexed lookup, one-header-pass and zero-payload-read accounting |
| `src/model/target/yvex_deepseek_tensor_coverage.[ch]` | complete IR-derived DeepSeek requirement construction, exact source reconciliation, typed refusal, deterministic coverage identity, and immutable lifetime |
| `src/model/target/yvex_deepseek_gguf_map.[ch]` | canonical indexed DeepSeek contribution/descriptor/metadata plan, source-forced transforms, naming provenance, deterministic identity, and typed refusal |
| `src/model/target/yvex_tensor_collection_report.c` | release-target collection projection from canonical coverage; Qwen/Gemma evidence remains separate |
| `src/model/target/yvex_missing_role_report.c` | release-target missing-role projection from canonical coverage |
| `src/model/target/yvex_mapping_gate_report.c` | operational projection of the canonical mapping plan and payload-streaming handoff |
| `src/model/target/yvex_model_class_profile.c` | strict source-verification coordination and report ownership for the canonical release target; Qwen/Gemma lexical evidence remains separate |
| `src/cli/render/yvex_model_target_render.c` | presentation of typed IR facts without architecture decisions |

## Graph And Backend Target Map

| Owner | Boundary |
| --- | --- |
| `src/graph/yvex_graph_bind.c` | runtime descriptor roles to graph bind plan |
| `src/graph/yvex_graph_execute.c` | future graph execution refusal |
| `src/backend/yvex_backend_tensor.c` | backend tensor allocation/bind boundary |
| `src/backend/yvex_backend_qtype.c` | backend qtype compute/refusal matrix |
| `src/backend/yvex_backend_report.c` | typed device, context, bundle, exact-variant, and memory reports |
| `src/backend/cuda/cuda_capability.c` | atomic generated-bundle admission, exact CUDA capability, launch/sync demotion, and cleanup failure |
| `src/backend/cuda/cuda_ops.c` | validated host launch binding for admitted exact variants |
| `src/backend/cuda/cuda_kernels.cu` | canonical bounded device kernels; generated bundle remains build output |
| `src/backend/cuda/cuda_qtype.c` | CUDA qtype capability/refusal facts |

## CLI Target Map

| Layer | Owner |
| --- | --- |
| Entry | `src/cli/yvex_cli.c` |
| Input | `src/cli/input/yvex_<surface>_args.c` |
| Command | `src/cli/commands/yvex_<surface>_cli.c` |
| Family surface | `src/cli/model_artifacts/*_surface.c` where required |
| Render | `src/cli/render/yvex_<surface>_render.c` |
| Operator IO | `src/cli/io/*` |

## GGUF Structural Reader Boundary

The artifact handle keeps a read-only file descriptor and maps the full file
only when a payload consumer explicitly requests `map`. The GGUF reader uses
exact positioned reads for the variable-size header, metadata, and tensor
directory, then owns copied length-aware values and names until close. Reader
metrics distinguish structural and payload bytes; structural open always
reports zero payload bytes. The separate canonical layout owner borrows the
opened artifact and parsed view, enforces power-of-two alignment and exact
directory-order padded continuation, validates zero padding and the complete
file span, and detects snapshot drift. It reads padding only and reports zero
tensor payload bytes. Complete-model artifact admission remains separate.

## GGUF Qtype ABI Boundary

The qtype owner admits the pinned GGUF on-disk range, accounts removed and
outside-baseline identities, and derives bytes from the complete shape with
`ne[0]` as row width. Dtype, range, integrity, conversion, and memory-plan
owners project these facts instead of copying geometry. This boundary does not
provide reference dequantization, quantization, emission, backend arithmetic,
artifact completion, or runtime support. Current milestone state belongs only
to `PROJECT.md`.

## Forbidden Claims

This target does not claim any completed implementation state for:

- quantization
- the GGUF writer
- GGUF writer-to-reader equivalence
- a complete or supported model artifact
- artifact materialization
- executable runtime descriptors
- graph execution
- full prefill, attention-backed KV, MoE, decode, logits, sampling, or generation
- eval, benchmark, performance-rate evidence, or release readiness
