# v0.1.0 Foundation Closure Repair Spine

Date: 2026-07-10
Status: priority-blocking
Authority: temporary foundation repair sequence
Project control: `../../PROJECT.md`

This is not a second product roadmap. It owns the ordered repairs required
before the DeepSeek runtime path can advance truthfully. While this spine is
active, the main critical path is blocked.

After closure, durable results are folded into `PROJECT.md` and this active
detail is removed. Git history is the archive; no compatibility copy is kept.

## Transition

```text
V010.DOCS.REFOUNDATION.0
-> V010.PROJECT.RECOVERY.0
-> V010.DOCS.ARCHITECTURE.0
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
| Completed rows | `V010.DOCS.REFOUNDATION.0`, `V010.PROJECT.RECOVERY.0` |
| Completed proof stage | documentation/claim refoundation only |
| Active Next | `V010.DOCS.ARCHITECTURE.0` |
| Next implementation row | `V010.REBASE.DEEPSEEK.0`, blocked by documentation architecture |
| Main-path state | blocked until this repair sequence closes |
| Runtime/artifact promotion | none |

## Repair Rows

| Row / status | Owner | Concrete defect or missing capability | Required outcome | Acceptance gate | Dependency / next row |
| --- | --- | --- | --- | --- | --- |
| `V010.DOCS.REFOUNDATION.0` / complete | Current claim docs, project-control history, `tests/test_docs_surface.sh` | The old project landscape treated diagnostic and report evidence as product progress and targeted multiple release families. | DeepSeek-only release truth, separate repair ownership, corrected artifact terms, reopened ABI boundaries, explicit unsupported states, and permanent claim guards. | Exact target/source and unsupported gates remain locked; ABI `.0` rows remain reopened; no runtime claim is promoted. The former size/heading mechanics are superseded by project recovery. | Foundation row; next `V010.PROJECT.RECOVERY.0`. |
| `V010.PROJECT.RECOVERY.0` / complete | `PROJECT.md`, this file, direct project-control consumers, structural docs guards | Refoundation removed the track registry, implementation history, milestone dependencies, and living project-control function. | Root project authority with recovered architectural tracks, conclusive milestone contract, traceable history, evidence demotion, reference baseline, and no compatibility spine. | `PROJECT.md` is canonical; all 24 old track responsibilities are accounted for; diagnostic lanes are evidence; hard truth is unchanged; guards reject a recreated `docs/spine.md`; no source behavior changes. | Depends on docs refoundation; next `V010.DOCS.ARCHITECTURE.0`. |
| `V010.DOCS.ARCHITECTURE.0` / active | Canonical documentation taxonomy and guards, with `PROJECT.md` retaining project state | Current canonical docs mix architecture, operator, API, evidence, and historical ownership; the verified vLLM, SGLang/DS4, ggml/llama.cpp, TensorRT-LLM/NVIDIA, and DeepSeek reference baseline is not mapped to YVEX owners. | Standardize document ownership, reference/paper taxonomy, architecture descriptions, current DS4 terminology, and migration rules without changing runtime capability. | One non-duplicated documentation system maps external reference concepts to YVEX owners, preserves claim truth, passes permanent guards, and leaves implementation gates unsupported. | Depends on project recovery; next `V010.REBASE.DEEPSEEK.0`. |
| `V010.REBASE.DEEPSEEK.0` / blocked | `src/model/target/`, source-target reports, target CLI renderers and tests | Target catalogs and release decisions still center selected aliases and multi-family candidates; the full target is not canonical in code. | Install `deepseek4-v4-flash` as the exact unsupported v0.1.0 target bound to the canonical local source; demote old candidates from release selection. | Typed target/list/decision reports and CLI tests expose the full target and source, refuse support, and name the next qtype ABI repair. | Depends on documentation architecture; next `V010.GGUF.QTYPE.ABI.1`. |
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

## Track Recovery Map

The count below is the number of current conclusive milestones that directly
own the old lane, not the number of historic row labels. Shared milestones are
counted in every lane whose architecture responsibility they close. Historic
reports, fixtures, selected slices, and diagnostic loops remain traceable in
`PROJECT.md` as evidence.

| Original track | Resulting project track / owner | Restored conclusive milestone count | Merged or demoted diagnostic work | Removed duplication | Remaining capability gap |
| --- | --- | ---: | --- | --- | --- |
| `TRACK.SCOPE` | `TRACK.PROJECT`, `TRACK.CLAIMS`, `TRACK.RELEASE` | 3 | Target reports and claim maps are evidence for project/release decisions. | Multi-family scope and repeated spine-control rows are superseded. | Documentation architecture, exact target rebase, and release closure. |
| `TRACK.SOURCE` | `TRACK.SOURCE` | 2 | Manifests, downloads, header inventories, and role-coverage reports are source evidence. | Per-report acceptance rows merge into verification and streaming. | Exact source verification and payload trust/streaming. |
| `TRACK.MAP` | `TRACK.MAP` | 1 | Naming, output-head, tokenizer, missing-role, and gate reports become map evidence. | Family/report micro-rows merge into one exact DeepSeek map. | Complete source-role-GGUF-layout mapping. |
| `TRACK.QUANT` | `TRACK.QUANT` | 2 | Policy and role-support reports plus `.0` fixtures remain evidence. | Calibration and matrix reports become acceptance subtasks. | Reopened qtype ABI and role-correct quantization/compute refusal. |
| `TRACK.ARTIFACT` | `TRACK.ARTIFACT` | 5 | Listing, tiny emission, and selected files become fixture/tensor-proof evidence. | Writer/parse/register micro-rows merge into ABI, writer, emission, roundtrip, and support cutover. | Complete YVEX GGUF and truthful admission. |
| `TRACK.INTEGRITY` | `TRACK.INTEGRITY` | 1 | Existing identity, range, corruption, drift, and preflight checks feed complete-layout tests. | Repeated gate reports merge into one global layout/admission closure. | Complete qtype-sized multi-tensor integrity. |
| `TRACK.MODEL` | `TRACK.ARCHITECTURE` | 2 | Class, context, attention, KV, and requirement reports become architecture evidence. | Family-class/report rows merge into architecture IR and runtime descriptor. | Execution-complete DS4 IR and descriptor. |
| `TRACK.TENSOR` | `TRACK.TENSOR`, consumed by `TRACK.MAP` | 1 | Collection inventories and missing-role reports remain header evidence. | Per-role collection rows merge into complete mapping coverage. | Every required tensor mapped with shape/layout constraints. |
| `TRACK.RESIDENCY` | `TRACK.RESIDENCY` | 1 | Placement and selected materialization reports remain lower-level evidence. | Forty-two storage/residency micro-rows merge into streaming and full materialization acceptance. | Complete DGX Spark materialization, placement, movement, and cleanup. |
| `TRACK.BACKEND` | `TRACK.BACKEND` | 1 | CPU/CUDA probes, allocation, transfer, and bounded parity remain primitive evidence. | Hardware/build/report rows become acceptance metadata or future scope. | Fail-closed CUDA plus required real operations. |
| `TRACK.GRAPH` | `TRACK.EXECUTION` | 2 | Primitive and selected graph proofs remain reference evidence. | Seventy-one primitive/attention/MoE/layer micro-rows merge into MoE and complete transformer milestones. | Family-correct attention, MoE, repeated layers, and final norm. |
| `TRACK.PREFILL` | `TRACK.PREFILL` | 1 | Context reports, chunk plans, and diagnostic summaries remain tests/subtasks. | Context/prefill report rows merge into real attention/KV prefill. | Full transformer prefill writing model KV. |
| `TRACK.KV` | `TRACK.KV`, closed with `TRACK.PREFILL` | 1 | Bounded store, shape, capacity, append/read, and lifecycle remain unit evidence. | Diagnostic KV rows merge into one attention-backed KV slice. | Family-correct writes, decode reads, reuse, and cleanup. |
| `TRACK.DECODE` | `TRACK.DECODE` | 1 | State, position, summary, and trace surfaces remain evidence. | Step/report rows merge into one repeated model-backed decode milestone. | Decode over transformer state and prior KV. |
| `TRACK.LOGITS` | `TRACK.LOGITS` | 1 | Buffer, checksum, top-k, and synthetic logits remain fixtures. | Report/staged-output micro-rows merge into output-head vocabulary logits. | Real final norm and output projection. |
| `TRACK.SAMPLING` | `TRACK.SAMPLING`, closed with `TRACK.LOGITS` | 1 | Candidate, selection, temperature, and seeded vectors remain internal fixtures. | Strategy/report rows merge into sampling over real logits. | Truthful deterministic/stochastic model sampling. |
| `TRACK.TOKENIZER` | `TRACK.TOKENIZER` | 1 | Token-ID and special-token diagnostics remain fixtures. | Metadata, stop, trace, and report rows merge into exact tokenizer I/O. | Prompt encoding, EOS/stop, and detokenization. |
| `TRACK.GENERATION` | `TRACK.GENERATION` | 1 | Bounded composition, trace, cancellation, append, and cleanup remain lifecycle evidence. | Fifty-three runtime/generation/trace micro-rows merge into one autoregressive milestone. | Multiple model-backed tokens and detokenized text. |
| `TRACK.OPERATOR` | `TRACK.OPERATOR`, with `TRACK.TOPOLOGY` history | 1 | CLI grammar, renderers, audits, diagnostic commands, and transcripts remain operator evidence. | Command-family and doctor/report rows merge into final release-path operator acceptance. | One truthful command over real generation. |
| `TRACK.SERVE` | `TRACK.SERVE` | 0 | Daemon status shell remains supporting evidence only. | Planned endpoint/protocol micro-rows move to future scope. | Runtime-backed serving is post-v0.1.0. |
| `TRACK.EVAL` | `TRACK.EVAL` | 1 | Lower-level fixture and primitive tests stay with their owning milestones. | Per-stage eval rows merge into one release-path evaluation milestone. | Generation-backed correctness/capability evidence. |
| `TRACK.BENCH` | `TRACK.BENCH` | 1 | Machine/artifact/qtype/context fields become one benchmark contract. | Per-metric report rows merge into one measured acceptance milestone. | No measurements exist. |
| `TRACK.RELEASE` | `TRACK.RELEASE`, supported by `TRACK.CLAIMS` | 1 | Existing docs/layout/natural/artifact guards are release evidence. | CI/docs/transcript/tag micro-rows merge into one release milestone. | All product gates, measurement, transcript, and tag remain blocked. |
| `TRACK.POST010` | `TRACK.SERVE`, `TRACK.DISTRIBUTED`, `TRACK.PORTABILITY`, `TRACK.MODELS`, `TRACK.ACCELERATION`, `TRACK.POST010` | 0 | Speculative reports and pressure targets remain research input only. | Duplicate speculative/post-release row sets collapse into explicit future tracks. | No post-v0.1 scope is active. |

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
