# YVEX System Target

Date: 2026-07-10
Status: filesystem and module ownership contract
Authority: `docs/spine.md`; temporary repair ordering in
`docs/repair/v010-foundation-closure.md`

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
src/generation/          legacy proof cells and future runtime implementation owners
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
| GGUF artifact ABI | `.0` reopened; tiny-fixture evidence only | complete DeepSeek container/metadata/tensor-info ABI | V010.GGUF.ARTIFACT.ABI.1 |
| GGUF qtype ABI | `.0` reopened; bounded byte geometry only | exact required geometry and refusal ABI | V010.GGUF.QTYPE.ABI.1 |
| GGUF layout integrity | bounded range fixtures | complete-artifact global layout and corruption refusal | V010.GGUF.LAYOUT.INTEGRITY.1 |
| CUDA truth | fallback PTX can expose no-op entry points | fail-closed advertised capability with reference parity | V010.CUDA.FAILCLOSED.0 |
| Architecture IR | profile/report facts only | typed execution-complete DeepSeek specification | V010.MODEL.ARCH.IR.0 |
| GGUF names/layout | partial and planned maps | complete DeepSeek role/name/layout map | V010.MAP.GGUF.DEEPSEEK.0 |
| Source payload | header-only inventory | bounded payload streaming across source shards | V010.SOURCE.PAYLOAD.STREAM.0 |
| Quantization | policy and geometry facts | role-correct quantization or explicit refusal | V010.QUANT.2 |
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

## Active Repair Sequence

```text
V010.DOCS.REFOUNDATION.0
-> V010.REBASE.DEEPSEEK.0
-> V010.GGUF.QTYPE.ABI.1
-> V010.GGUF.ARTIFACT.ABI.1
-> V010.GGUF.LAYOUT.INTEGRITY.1
-> V010.CUDA.FAILCLOSED.0
-> V010.MODEL.ARCH.IR.0
-> V010.MAP.GGUF.DEEPSEEK.0
-> V010.SOURCE.PAYLOAD.STREAM.0
-> V010.QUANT.2
-> V010.GGUF.WRITER.1
-> V010.ARTIFACT.EMIT.DEEPSEEK.0
-> V010.GGUF.ROUNDTRIP.1
```

`V010.DOCS.REFOUNDATION.0` is complete at documentation/claim refoundation
only. `V010.REBASE.DEEPSEEK.0` is Active Next. The main materialization/runtime
sequence cannot advance until the repair spine closes.

## Reopened: V010.GGUF.ARTIFACT.ABI.0

`V010.GGUF.ARTIFACT.ABI.0` produced useful typed facts and tiny fixture proof,
but it did not establish the complete DeepSeek metadata and tensor-directory
contract. It is not a valid completed foundation for full artifact work.
`V010.GGUF.ARTIFACT.ABI.1` owns the replacement gate.

## Reopened: V010.GGUF.QTYPE.ABI.0

`V010.GGUF.QTYPE.ABI.0` produced useful bounded byte-geometry and refusal facts,
but it did not establish the exact required qtype contract for the complete
DeepSeek artifact path. It is not a valid completed foundation.
`V010.GGUF.QTYPE.ABI.1` owns the replacement gate.

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
