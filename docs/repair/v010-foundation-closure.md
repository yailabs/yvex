# v0.1.0 Foundation Closure Repair Spine

Date: 2026-07-10
Status: priority-blocking
Authority: temporary foundation repair sequence
Product spine: `../spine.md`

This is not a second product roadmap. It owns the ordered repairs required
before the DeepSeek runtime path can advance truthfully. While this spine is
active, the main critical path is blocked.

After closure, durable results are folded into `docs/spine.md` and this active
detail is removed. Git history is the archive; no compatibility copy is kept.

## Transition

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

| Field | Current value |
| --- | --- |
| Completed row | `V010.DOCS.REFOUNDATION.0` |
| Completed proof stage | documentation/claim refoundation only |
| Active Next | `V010.REBASE.DEEPSEEK.0` |
| Main-path state | blocked until this repair sequence closes |
| Runtime/artifact promotion | none |

## Repair Rows

| Row / status | Owner | Concrete defect or missing capability | Required outcome | Acceptance gate | Dependency / next row |
| --- | --- | --- | --- | --- | --- |
| `V010.DOCS.REFOUNDATION.0` / complete | `docs/spine.md`, this file, canonical claim docs, `tests/test_docs_surface.sh` | The primary spine treated diagnostic and report evidence as product progress and targeted multiple families. | Compact DeepSeek-only execution spine, separate repair spine, corrected artifact terms, explicit unsupported truth, and permanent focused guards. | Main spine is at most 350 lines; exact target and source are locked; ABI `.0` rows are reopened; no runtime claim is promoted; docs guard passes. | Foundation row; next `V010.REBASE.DEEPSEEK.0`. |
| `V010.REBASE.DEEPSEEK.0` / active | `src/model/target/`, source-target reports, target CLI renderers and tests | Target catalogs and release decisions still center selected aliases and multi-family candidates; the full target is not canonical in code. | Install `deepseek4-v4-flash` as the exact unsupported v0.1.0 target bound to the canonical local source; demote old candidates from release selection. | Typed target/list/decision reports and CLI tests expose the full target and source, refuse support, and name the next qtype ABI repair. | Depends on docs refoundation; next `V010.GGUF.QTYPE.ABI.1`. |
| `V010.GGUF.QTYPE.ABI.1` / blocked | `src/gguf/yvex_gguf_qtype.c` and GGUF qtype tests | `.0` proves bounded byte geometry but does not establish the exact DeepSeek qtype set and refusal contract required by the complete artifact path. | Define exact GGUF qtype identifiers, block geometry, storage sizing, ambiguity refusal, and DeepSeek-required coverage without claiming backend arithmetic. | Reference geometry tests cover required, unknown, ambiguous, overflow, and malformed cases; `.1` evidence supersedes `.0`. | Depends on DeepSeek rebase; next `V010.GGUF.ARTIFACT.ABI.1`. |
| `V010.GGUF.ARTIFACT.ABI.1` / blocked | `src/gguf/yvex_gguf_container.c`, `yvex_gguf_metadata.c`, `yvex_gguf_tensor_info.c`, `yvex_gguf_reader.c` | `.0` closes tiny-fixture parsing only and is insufficient for the metadata and tensor directory required by the exact complete model. | Define and validate the complete DeepSeek container, metadata, tensor-info, and failure ABI without reading payload bytes in header-only validation. | DeepSeek-shaped fixtures cover required metadata, tensor counts, ranks, types, malformed values, overflow, truncation, and explicit refusal; `.1` supersedes `.0`. | Depends on qtype ABI `.1`; next `V010.GGUF.LAYOUT.INTEGRITY.1`. |
| `V010.GGUF.LAYOUT.INTEGRITY.1` / blocked | `src/gguf/yvex_gguf_range_map.c`, `yvex_gguf_layout_map.c`, `src/artifact/yvex_artifact_integrity.c` | Global offsets, alignment, qtype-sized ranges, overlap, truncation, and aggregate overflow are not proven for a complete artifact layout. | Produce typed global layout facts and fail closed on every invalid range before payload access or materialization. | Reference fixtures cover overlap, aliasing, misalignment, out-of-file ranges, aggregate overflow, qtype size mismatch, and valid multi-tensor layout. | Depends on artifact ABI `.1`; next `V010.CUDA.FAILCLOSED.0`. |
| `V010.CUDA.FAILCLOSED.0` / blocked | `src/backend/cuda/cuda_ops.c`, CUDA capability reports and CUDA tests | The non-nvcc embedded PTX advertises callable entry points whose bodies return without computation. | Refuse unavailable kernels and advertise support only for an implementation that executes and passes reference comparison. | Every advertised CUDA op has host binding, non-no-op kernel evidence, reference parity, failure path, and cleanup; fallback builds report unsupported. | Depends on layout integrity; next `V010.MODEL.ARCH.IR.0`. |
| `V010.MODEL.ARCH.IR.0` / blocked | `src/model/target/` architecture/profile owners and typed headers | No execution-complete typed DeepSeek-V4-Flash architecture specification exists. | Represent layers, attention and position rules, KV geometry, MoE routing/expert topology, norms, output head, tokenizer requirements, and validation blockers as typed facts. | Exact config evidence builds one deterministic typed specification; missing, inconsistent, unknown, and unsupported fields refuse precisely. | Depends on CUDA fail-closed truth; next `V010.MAP.GGUF.DEEPSEEK.0`. |
| `V010.MAP.GGUF.DEEPSEEK.0` / blocked | `src/model/target/` role-map owners plus `src/gguf/yvex_gguf_name_map.c` and `yvex_gguf_layout_map.c` | Source tensor names are not completely mapped to runtime roles and GGUF names/layout for the exact architecture. | Map every required global, per-layer, attention, position, MoE, expert, norm, tokenizer, and output role with shape and layout constraints. | The 46-shard header inventory maps exactly once to required roles; missing, duplicate, ambiguous, extra-required, shape, and expert-index failures are tested. | Depends on architecture IR; next `V010.SOURCE.PAYLOAD.STREAM.0`. |
| `V010.SOURCE.PAYLOAD.STREAM.0` / blocked | `src/source/` payload reader owner and `src/gguf/conversion.c` consumer boundary | Current source work is header-only and cannot stream complete tensor payloads into conversion. | Add bounded, overflow-safe shard payload reads with explicit offsets, ownership, cleanup, short-read refusal, and no whole-model allocation. | Reference tests stream tensors across shard boundaries and cover missing shard, truncation, offset overflow, short read, cancellation, and cleanup. | Depends on complete map; next `V010.QUANT.2`. |
| `V010.QUANT.2` / blocked | `src/gguf/quant.c`, backend qtype owners, quantization tests | Policy and byte geometry do not provide role-correct quantization or truthful compute/refusal coverage. | Quantize required mapped roles or refuse them explicitly, with reference dequantization and error bounds for every emitted qtype. | Per-role reference vectors cover geometry, numeric tolerance, edge blocks, unsupported qtypes, allocation failure, and cleanup. | Depends on payload streaming; next `V010.GGUF.WRITER.1`. |
| `V010.GGUF.WRITER.1` / blocked | `src/gguf/yvex_gguf_writer.c` and explicit file-writer tests | The writer owner currently refuses and cannot serialize a complete artifact safely. | Serialize metadata, tensor-info, alignment, qtype-sized data, offsets, and payloads through explicit local-file IO with atomic failure cleanup. | Deterministic multi-tensor fixtures match independent expectations; short write, overflow, collision, cancellation, partial-file cleanup, and overwrite policy are tested. | Depends on quantization; next `V010.ARTIFACT.EMIT.DEEPSEEK.0`. |
| `V010.ARTIFACT.EMIT.DEEPSEEK.0` / blocked | `src/artifact/` emission coordinator with GGUF writer and model-target facts | No complete DeepSeek-V4-Flash artifact is produced by YVEX. | Emit one complete model artifact for `deepseek4-v4-flash` from the canonical source with identity and manifest facts, without claiming runtime support. | Required tensor/metadata counts and bytes match the typed spec and map; no missing roles; emitted identity is stable; failure leaves no admitted artifact. | Depends on writer `.1`; next `V010.GGUF.ROUNDTRIP.1`. |
| `V010.GGUF.ROUNDTRIP.1` / blocked | `src/gguf/yvex_gguf_roundtrip.c`, `src/artifact/yvex_artifact_roundtrip_gate.c` | `.0` does not prove writer-reader equivalence for a complete YVEX-produced artifact. | Reopen the emitted file through YVEX and compare metadata, tensor directory, qtypes, shapes, offsets, ranges, payload identity samples, and aggregate identity. | Complete artifact roundtrip passes deterministic equivalence; mutation, truncation, reordered/duplicate metadata, range drift, and payload corruption refuse. | Depends on complete emission; closes repair spine and hands off to full materialization/residency. |

## Decommission Map

This table records existing implementation residue, not product progress. Every
surface has a concrete disposition and owning future row.

| Obsolete surface | Code-grounded locations | Disposition | Owning future row / acceptance |
| --- | --- | --- | --- |
| Selected embedding and selected segment commands | `src/cli/input/yvex_graph_args.c`, `src/cli/render/yvex_graph_render.c`, selected modes in `src/graph/yvex_graph_primitive.c`, `tests/cli/partial_graph.sh`, `tests/cli/segment_graph.sh` | Absorb backend/reference comparisons into lower-level internal proofs; remove selected product commands and aliases. | `V010.GRAPH.DEEPSEEK.TRANSFORMER.0`; full transformer tests own public graph proof and selected command discovery disappears. |
| Bounded diagnostic prefill and KV | `src/generation/yvex_prefill.c`, `yvex_kv.c`, `yvex_kv_report.c`, KV/prefill CLI adapters and tests | Replace with family-correct prefill plus attention-backed KV; retain only storage math that remains a valid low-level unit. | `V010.RUNTIME.DEEPSEEK.ATTENTION.KV.0`; prefill writes model K/V and decode reads the same owned state. |
| Diagnostic decode | command/help and summary advance in `src/generation/yvex_decode.c`, plus CLI tests | Replace with one model-backed decode step, then remove summary-only command behavior. | `V010.RUNTIME.DEEPSEEK.DECODE.0`; decode consumes the executable descriptor and attention-backed KV. |
| Fixture logits and sampling | `src/generation/yvex_logits.c`, `yvex_sampling.c`, `yvex_sampling_report.c`, sample/logits CLI surfaces and tests | Retain numeric selection cases only as internal test fixtures; replace public behavior with final-norm/output-head vocabulary logits and sampling. | `V010.RUNTIME.DEEPSEEK.LOGITS.SAMPLING.0`; sampled tokens derive from full model vocabulary logits. |
| Bounded diagnostic generation | `src/generation/yvex_generation*.c`, `src/cli/commands/yvex_generate_cli.c`, generate input/render/catalog files, `tests/cli/generation.sh` | Replace with the real tokenizer/prefill/KV/decode/logits/sample/append/stop loop; remove diagnostic token-printing path. | `V010.RUNTIME.DEEPSEEK.GENERATION.0`; multiple detokenized autoregressive tokens pass release-path tests. |
| Selected-artifact support levels | `include/yvex/model_gate.h`, `src/model/artifacts/yvex_model_artifact_gate.c`, registry/report owners, prepare renderers, model gate/registry tests | Remove selected states from model support; represent any retained subset only as a tensor proof artifact with a named proof gate. | `V010.ARTIFACT.SUPPORT.CUTOVER.0`; only complete artifacts can enter supported model artifact gates. |
| Report-only fullmodel surfaces | `src/cli/model_artifacts/yvex_fullmodel_surface.c`, `src/cli/render/yvex_fullmodel*.c`, `tests/cli/fullmodel.sh` | Replace descriptor/materialization reports with typed full-target APIs where useful; remove reports that only restate missing runtime behavior. | `V010.RUNTIME.DESCRIPTOR.DEEPSEEK.0`; one typed executable descriptor report remains and obsolete report commands disappear. |
| Stale target, help, and claim tests | `src/model/target/yvex_model_target_{catalog,decision,candidates}.c`, `src/cli/yvex_cli.c`, generate examples/render help, `tests/cli/core.sh`, `models.sh`, `generation.sh` | Remove multi-family/selected release choices and stale examples; replace with exact full-target refusal, then the real generation command contract. | `V010.REBASE.DEEPSEEK.0` for target truth and `V010.CLI.DEEPSEEK.GENERATE.0` for final command/help proof. |

## Closure

This repair spine closes only when `V010.GGUF.ROUNDTRIP.1` passes over the
complete YVEX-produced DeepSeek artifact and every earlier repair gate is still
green. Closure does not claim materialization, runtime execution, generation,
evaluation, benchmark evidence, or release readiness.
