# YVEX System Target

Date: 2026-07-09
Status: docs/control + filesystem ownership seed
Authority: docs/spine.md

This document records the target filesystem architecture for the execution-core
and control-plane boundary. It is an implementation contract, not a capability
claim.

## Current Actual Tree

Current core areas:

```text
src/cli/                 CLI dispatch, input, surfaces, render, IO
src/source/              source manifests, scans, native weight headers
src/model/target/        model-target catalogs, maps, gates, qtype reports
src/model/artifacts/     model registry/ref/gate/report/write ownership
src/model/               dtype/model tables and runtime descriptor target
src/gguf/                GGUF parser plus target ABI/writer/roundtrip owners
src/artifact/            artifact IO, identity, integrity, descriptor gates
src/graph/               graph construction, plans, bind/execute target owners
src/backend/             backend abstraction, tensor/qtype/report ownership
src/generation/          diagnostic generation cells and future runtime path
```

## Target Tree Summary

```text
input -> command -> surface router -> report/domain -> render -> cli/io

file writer -> explicit local files only
GGUF ABI -> artifact descriptor -> materialization -> runtime descriptor
runtime descriptor -> backend tensor binding -> graph bind -> graph execute
graph execute -> prefill -> KV -> decode -> logits -> sampling -> generation
```

No domain file owns command grammar or operator byte output. No renderer owns
domain algorithms. No writer owns command output.

## Gap Table

| Area | Current state | Target state | Next row |
| --- | --- | --- | --- |
| GGUF ABI | complete/report-only + fixture-proof | container/metadata/tensor_info ABI closed | V010.GGUF.QTYPE.ABI.0 |
| GGUF qtype | complete/report-only + fixture-proof | byte geometry and storage-size refusal facts closed | V010.QUANT.2 |
| Quant | qtype role reports plus GGUF byte geometry | compute/refusal matrix | V010.QUANT.2 |
| GGUF writer | explicit refusal | concrete emitted bytes | V010.GGUF.WRITER.0 |
| GGUF names | planned emitted-name owner | role to GGUF name map | V010.MAP.GGUF.NAMES.0 |
| GGUF layout | planned layout owner | role to range/qtype layout map | V010.MAP.GGUF.LAYOUT.0 |
| GGUF roundtrip | explicit refusal | emitted bytes parse back | V010.GGUF.ROUNDTRIP.0 |
| Artifact emission | selected/report-only | generation-capable artifact bytes | V010.ARTIFACT.EMIT.2 |
| Materialization | selected proof only | required tensor byte materialization | V010.ARTIFACT.MATERIALIZE.0 |
| Runtime descriptor | target owner seed | GGUF facts project to runtime descriptor | V010.RUNTIME.DESCRIPTOR.GGUF.0 |
| Graph/backend | primitive/report-only | backend-bound graph path | V010.GRAPH.24 |

## Owner Rules

- CLI input owns `argc/argv` parsing only.
- CLI command adapters dispatch only.
- CLI surfaces route command families only.
- CLI renderers format typed facts.
- CLI IO writes operator bytes.
- Explicit writer modules write local files only.
- Source owns source files and header inventories.
- Model target owns target catalogs, maps, gates, and qtype reports.
- Model artifacts own registry, references, gates, typed artifact reports, and
  explicit file writers.
- GGUF owns container ABI, metadata ABI, tensor_info ABI, qtype geometry,
  ranges, reader, writer, roundtrip, emitted names, emitted layout, descriptor,
  and GGUF reports.
- Artifact owns YVEX artifact descriptors, materialization boundary, roundtrip
  gates, identity, integrity, and artifact reports.
- Model owns runtime descriptor projection.
- Graph owns bind plans and graph execution boundary.
- Backend owns tensor and qtype support/refusal facts.

## Forbidden Placements

- Domain command grammar.
- Domain operator byte output.
- Renderer file IO or backend execution.
- CLI files in `CORE_SRCS`.
- Writer modules writing command output.
- GGUF parsing language that implies materialization.
- Materialization language that implies backend execution.
- Backend language that implies graph or generation support.

## GGUF Target Map

| Owner | Boundary |
| --- | --- |
| `src/gguf/yvex_gguf_container.c` | magic/version/container ABI |
| `src/gguf/yvex_gguf_metadata.c` | metadata key/value ABI |
| `src/gguf/yvex_gguf_tensor_info.c` | tensor_info name/rank/type/shape ABI |
| `src/gguf/yvex_gguf_qtype.c` | qtype byte geometry |
| `src/gguf/yvex_gguf_range_map.c` | absolute range/alignment checks |
| `src/gguf/yvex_gguf_reader.c` | parser-result reader facts |
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

## Graph And Backend Target Map

| Owner | Boundary |
| --- | --- |
| `src/graph/yvex_graph_bind.c` | runtime descriptor roles to graph bind plan |
| `src/graph/yvex_graph_execute.c` | future graph execution refusal |
| `src/backend/yvex_backend_tensor.c` | backend tensor allocation/bind boundary |
| `src/backend/yvex_backend_qtype.c` | backend qtype compute/refusal matrix |
| `src/backend/yvex_backend_report.c` | typed backend capability reports |
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

## Next Row Sequence

```text
SPINE.SYSTEM.TARGET.0
-> V010.GGUF.ARTIFACT.ABI.0
-> V010.GGUF.QTYPE.ABI.0
-> V010.QUANT.2
-> V010.GGUF.WRITER.0
-> V010.MAP.GGUF.NAMES.0
-> V010.MAP.GGUF.LAYOUT.0
-> V010.GGUF.ROUNDTRIP.0
-> V010.ARTIFACT.EMIT.2
-> V010.ARTIFACT.MATERIALIZE.0
-> V010.RUNTIME.DESCRIPTOR.GGUF.0
```

## Follow-Up: V010.GGUF.ARTIFACT.ABI.0

`V010.GGUF.ARTIFACT.ABI.0` closed the GGUF container, metadata,
tensor_info, and ABI-visible range boundary using typed facts and tiny fixture
proof. The writer, writer-reader roundtrip, artifact materialization, and
runtime descriptor projection remain planned.

## Follow-Up: V010.GGUF.QTYPE.ABI.0

`V010.GGUF.QTYPE.ABI.0` closed GGUF qtype byte geometry and storage-size
refusal facts. Backend compute/refusal, qtype selection, source tensor
conversion, writer, writer-reader roundtrip, and artifact emission remain
planned.

## Forbidden Claims

This target does not claim any completed implementation state for:

- quantization
- the GGUF writer
- GGUF writer-to-reader equivalence
- generation-capable artifact bytes
- artifact materialization
- executable runtime descriptors
- graph execution
- real prefill, attention-backed KV, decode, logits, sampling, or generation
- eval, benchmark, performance-rate evidence, or release readiness
