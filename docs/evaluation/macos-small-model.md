<!-- docs:metadata
title: macOS Real Small-Model Execution Attempt
id: yvex.evaluation.macos-small-model
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# macOS Real Small-Model Execution Attempt

**Real weights acquired and one full artifact compiled; model execution and publication remain blocked.**

[Up](README.md)

The operator selected `MACOS.SMALLMODEL.CLI.0` on 2026-10-01 and authorized
Hugging Face publication of the resulting model representation. Work starts
on clean `feature/macos-metal` at `a4aa9252649501c05b493aef918a4de4c5755fed`.
The selection-only commit is `9ab4be5a499f5cc1f953f78ddee08614e83aa28c`, tree
`710fd4e1af22e75e9eac7c1965a8a3bc4fa70af7`; all execution attempts use this
clean, unchanged source and a native binary reporting that commit. No production
source, numerical contract or resource policy changed. Main and primary Exon
checkouts remain untouched. The earlier [Metal foundation](macos-metal.md)
remains qualified at its primitive scope.

The tested target is macOS arm64, Apple M5 Pro, 25,769,803,776 bytes of system
RAM (24 GiB). The native build uses the existing pinned REPLAI dependency and
`RUSTUP_TOOLCHAIN=1.98.1`. A disposable ignored Python environment supplies
Hugging Face CLI 2.1.1. Public acquisition uses `--auth never`; credentials are
not needed for these source repositories.

## Exact sources and preparation

| Model | Immutable upstream revision | Actual acquisition | Preparation result |
| --- | --- | --- | --- |
| [Mamba-Codestral-7B-v0.1](https://huggingface.co/mistralai/Mamba-Codestral-7B-v0.1/tree/4f086c08c1e0f07bdc50ca25125dbbf7475d21da) | `4f086c08c1e0f07bdc50ca25125dbbf7475d21da` | 12 files, three selected BF16 shards, 14,574,191,162 bytes; consolidated alternative excluded | `model prepare` returns `READY`, publishes the full GGUF and binding |
| [Qwen3.5-0.8B](https://huggingface.co/Qwen/Qwen3.5-0.8B/tree/2fc06364715b967f1860aea9cf38778875588b17) | `2fc06364715b967f1860aea9cf38778875588b17` | Eight selected files, one mixed BF16/F32 shard, 1,766,544,632 bytes | `model prepare` returns `BLOCKED`: no exact source-to-ready compiler binding |

Mamba is selected first because its exact source, compiler and CPU deployment
already have authoritative family contracts. Preparation performs full source
payload verification and publishes `yvex.source_manifest.v3`, with 579 header
tensors and upstream payload verification. The download report's earlier
`payload_hash_verified=false` does not describe that later verification stage.

The compiled Mamba GGUF has 579 BF16 tensors, 14,570,807,296 payload bytes and
14,574,491,136 full-file bytes. A separate full artifact integrity pass checks
all 579 tensor ranges, shapes, dtypes and byte counts, with zero errors/warnings.
Its SHA-256 is exactly the retained family artifact identity:

```text
bf0053bf02a235563342a0281acfc73e17eb287542a7109e975db414458a77d3
```

The common compiler publishes binding v17 under
`f8eae71cdef043cea3fc0b46264026f65944892c6b1703c7761c3eab61afabb6`.
The physical variant is
`a10c94158265f53ae6e6add7a11d1fae3851e3b51d22129d63701e80fdfff0c9`.
The ordinary catalog projects the resulting CPU profile as `READY` and
launchable. These are preparation facts; successful model admission is a
separate gate.

Qwen's sole 1,746,942,600-byte tensor shard is independently read in full and
matches the immutable provider LFS SHA-256:

```text
04b1c301231dd422b8860db31311ab2721511346a32cb1e079c4c4e5f1fe4696
```

Before/after file identity remains unchanged during that checksum. This proves
acquired tensor bytes, not YVEX family verification, compilation or generation.

## Native execution and actual output

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Native build | Existing manifest and compiler/link rules | Clean source, GNU Make, pinned native producer | Existing product builds | `gmake -j8 all` exits 0 | Exact binary commit `9ab4be5a` | PASS | Native product availability |
| Source acquisition | Real Hugging Face provider and pinned revisions | Mamba three-shard selection; Qwen single shard | Selected complete local bytes | Both `model-download-pass`; no partial files | Exact selection, sizes and revisions | PASS | Actual upstream acquisition |
| Mamba preparation/integrity | Existing family compiler, source manifest and full GGUF hash | 579 real BF16 tensors | Full source-faithful artifact and authenticated binding | READY; expected full-file hash; all 579 structure checks valid | Exact identities/bytes | PASS | Compiled artifact and binding |
| Qwen preparation | Existing authoritative source-to-ready binding lookup | Actual 0.8B checkpoint | Exact admitted compiler path | `BLOCKED`, `changed=false` | Exact refusal | BLOCKED | Small checkpoint is not currently admitted |
| Host lifecycle | Existing local protocol v24 | Normal foreground `serve`, status, memory, model load and stop | Host starts; load creates an engine or refuses before publication | Host READY with zero engines; model load refuses memory; owned host stops cleanly | No fabricated capacity or CPU/GPU fallback | PASS for host; model BLOCKED | Real local host and fail-closed admission |
| Real model generation attempt | Existing common generation operator | CPU, target-only, greedy; text `def add(a, b):`; context 32; chunk 1; eight requested new tokens | Complete native generated bytes | Memory refusal before prompt acceptance | Prompt/sample/committed token counts and generated bytes all zero | BLOCKED | No real generation output obtained |
| Hugging Face publication | Existing release contract | Compiled Mamba artifact with missing new successful runtime validation | Qualified release and verified remote bytes | Local publication-readiness record is `BLOCKED_VALIDATION`; no upload | No publication receipt or invented repository commit | BLOCKED | No newly published usable-model claim |
| Documentation and control | Canonical project-control, documentation and QA registry checks | This report and affected Task/Status/family routes | Counts, routes, publication and registry remain consistent | `test-project-control`, `test-documentation-architecture`, `test-docs-surface` and `check-qa-registry` pass | Exact exit 0; final diff has no whitespace errors | PASS | Reviewable evidence/control update only |

The existing engineering command `bench transformer generate --text` routes
raw completion through the common generation owner. It does not require
inventing a conversation template. The actual requested prompt was:

```text
def add(a, b):
```

Its actual stdout is:

```text
status: refused
prompt_tokens: 0
sampled_tokens: 0
model_committed_tokens: 0
execution_mode: target-only
generated_text_bytes: 0
generated_text_digest:
stop_reason: none
reason: available system memory cannot preserve the required system reserve after model residency
```

Actual stderr identifies `runtime.model` with the same reason. The command
exits 4. No generated answer, token sequence or whole-model numerical comparison
exists for this Mac run. The normal `model load --ctx 32` reaches the same
admission refusal; it does not create an engine.

## Resource blocker and canonical owners

The model-open preflight in `runtime.core` requires the admitted payload,
system reserve and largest transient tensor before publishing residency:

```text
14,570,807,296 payload bytes
 8,589,934,592 required reserve bytes
   268,435,456 maximum tensor bytes
23,429,177,344 required bytes (about 21.82 GiB)
```

A subsequent independent observation through the unchanged native
`yvex_platform_system_memory` mechanism reports 16,750,372,405 available bytes
(about 15.60 GiB) out of 24 GiB total. Availability is a sampled OS observation,
not a frozen value for both earlier refusal instants. The default mandatory
reserve is 8 GiB; context reduction cannot remove this model-open requirement.
No fixture-memory hook, reserve weakening, forced paging policy or user-app
termination was used. Disk has ample space; the blocker is admitted memory.
Model residency and actual model working set are unmeasured because admission
refuses before an engine exists. Prepared on-disk bytes are not resident RAM.

| Owner | Exact boundary and required next work |
| --- | --- |
| `runtime.core` / `runtime.capacity` | Existing payload + reserve + transient preflight refuses this BF16 7B workload under current available memory. Any new admission class for mapped/streamed CPU weights needs a canonical physical-memory contract and independent refusal/workspace qualification; deleting the reserve check is not a solution. |
| `source.catalog` | The small Qwen revision has no authoritative source-qualified target/binding. Add and verify its real source identity through canonical family integration; a repository-name or family alias cannot confer execution support. |
| `model.family.qwen3_5` / `graph.family.qwen3_5` | The downloaded small checkpoint declares tied embeddings/output weights. Existing outer/text validation rejects ties, logits policy requires a separate output head, and current complete-artifact admission is the exact 851-tensor 27B record. These are code-verified additional boundaries, not the error emitted by this first failed preparation. Support requires correct tied-parameter semantics and an independently qualified exact small-model artifact/binding. |
| Native conversation boundary | The admitted Mamba checkpoint owns no chat template. Even after memory admission, ordinary hosted chat remains an independently unsupported surface. Raw completion is an existing engineering path, but it has not executed successfully here. |
| Metal backend and common model runtime | The qualified foundation provides shared storage and F32 row selection only. Full numerical operations, model/capacity admission and specialization lifetime remain unqualified. This CPU attempt earns no Metal-model claim. |
| Release/publication | Both upstream cards declare Apache-2.0, but license and byte integrity alone do not close runtime validation. Successful current execution plus exact release evidence is needed before publishing the requested usable representation. Authenticated `yailabs` access was observed on Exon; no credential was copied to the Mac. |

No production workaround was introduced on the pressure branch. In particular,
the small source was not relabelled as the admitted 27B target, tied parameters
were not fabricated, and a failed generation was not replaced with fixture or
assistant-written text. These owner boundaries explain why the requested full
Mac outcome remains BLOCKED rather than COMPLETE.

## Retained evidence and local files

Real sources and representations remain under `/Users/mothx/lab/models`.
The compiled file is `representations/mamba-codestral-7b-v0.1/` followed by the
physical variant above and `model.gguf`. Its binding remains under
`registry/mamba2/mamba-codestral-7b-v0.1-bindings/`. Both acquired models remain
in the normal local catalog for continuation.

Raw outputs, the full source manifest, physical plan, artifact integrity report,
Qwen full-shard checksum, native memory observation and a checksum index are
retained in `/Users/mothx/lab/models/evidence/releases/mamba-codestral-macos-20261001`.
Its `publication-readiness.json` records the exact artifact, proposed repository
`yailabs/Mamba-Codestral-7B-v0.1-GGUF`, zero generated bytes and the validation
blocker. This local planning record is not a qualified release or publication
receipt. No repository or remote artifact was created for this Task.

The acquisition supervisors and owned host are stopped. No heavy model was
executed on Exon, and no primary source tree, service or CUDA job was replaced.
No new ADR, numerical ABI or runtime architecture was selected. Documentation
records the observed exit in Tasks, Status and family/evaluation routes.

`progression_decision=blocked_external`, `downstream_safe=false` for real-model
load/generation/publication on this Mac. Source acquisition and the Mamba
artifact/binding are retained successful lower gates. Existing native CPU
fixture and Metal primitive qualification remain valid at their earlier scope.
