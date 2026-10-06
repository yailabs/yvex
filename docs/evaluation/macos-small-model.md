<!-- docs:metadata
title: macOS Small-Model CPU Qualification
id: yvex.evaluation.macos-small-model
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# macOS Small-Model CPU Qualification

**Exact Qwen CPU generation qualified and source-faithful text artifact publicly verified.**

[Up](README.md)

## Qualified small-checkpoint boundary — 2026-10-02

This record retains the original raw-completion and publication claim. The
later [exact CPU conversation qualification](qwen-small-conversation.md) adds
source-authenticated chat through a new binding to the unchanged artifact;
it does not rewrite the historical evidence below or qualify Metal inference.

The operator resumed `MACOS.SMALLMODEL.CLI.0` on `feature/macos-metal`, starting
at clean `10ac0d8da586b110f32104af22d83e51a9293b9d`. Selection is `73d3cce4`;
family-owned exact-target catalogs are introduced at `cc7a9c79`. The qualified
implementation is `17e5e0a14fdb934cdb19a4cc720fd798d13f5283`, tree
`bfa57cf065c44c82ac78f9ff3743d57eadc89935`. All final execution and QA below
use that clean, unchanged source. It is published on the same pressure branch;
this report's later closure commits change only documentation. Publication retry
starts at clean `59e52268b53bb6bc11df2b58bccf814b439c84bf`. Published main
remains `67a7905ea9deb98b0704629a1f979634e19007fb`. SDK/Studio/YAI and primary
Exon checkouts/services are outside this work.

The pinned source is [Qwen/Qwen3.5-0.8B](https://huggingface.co/Qwen/Qwen3.5-0.8B/tree/2fc06364715b967f1860aea9cf38778875588b17).
Canonical v3 source verification covers eight files, 1,766,544,632 bytes, one
checksummed 1,746,942,600-byte shard and 488 tensor headers. The exact text
projection emits 320 tensors: 284 BF16 and 36 F32. Vision's 153 tensors and
MTP's 15 tensors remain deferred. The tied embedding/output parameter is stored
once; no separate output head is fabricated.

The source-faithful GGUF is 1,528,566,432 bytes, with 1,504,791,232 tensor
payload bytes. Its full-file SHA-256 is
`0c5776eb6b1f2abb3a35f2324aabc4d8b7693856650b799e88161f7167feded6`.
A controlled clean-commit compiler replay takes 11.63574625 seconds and emits
byte-identical output. The pinned official ggml reader
`af97976c7810cdabb1863172f31c432dab767de7` accepts GGUF v3, 45 metadata fields
and all 320 tensors. Fresh full-file integrity binds all descriptors to the
sealed physical plan, checks exact ranges/types/bytes and passes. Normal
`model prepare` is READY after explicit current integrity verification; an
expired verification cache requires revalidation, not a policy exemption.

### Architectural delta and problems handled

| Owner | Actual defect / requirement and handling |
| --- | --- |
| Family catalog / common compiler | One-target-per-family assumptions prevented exact small-checkpoint registration. Families now enumerate their exact targets; adapter-only ambiguity fails closed, while the admitted 27B target retains its identity. |
| Qwen source / physical recipe | The exact checkpoint has alternate indexed shard naming, mixed source types, tied output and no generation-config sidecar. Source-qualified import accepts the strict actual indexed stem, preserves the 36 F32 vectors and uses an explicit YVEX raw greedy policy; no absent sidecar is invented. |
| Generic operation / physical wire | Operation name/version resolves exactly; version zero retains legacy version 1. Gated-delta v2 represents BF16 projection/conv/time bias with F32 decay/norm/state. Old v1 remains unchanged. |
| Output plan / residency / logits | Output-plan v3, schema 5, authenticates tied-token-embedding output. One prepared physical tensor serves both roles. Earlier separate-head layouts remain readable and enforce their original policy. |
| CPU backend | Source-order F32-accumulating BF16 linear realization, explicit publication rounding, gate/add operations and checked resource lifetime use the existing backend vtable. Cancellation/refusal and retained cleanup are tested. |
| CPU recurrence / stateful attention | Existing F32 recurrence and causal attention contracts execute through the common program/state lifecycle. CPU attention assembles a bounded temporary prefix plus score scratch, includes it in capacity, stages through the canonical logical provider and releases before publication/abort. CUDA's device-bank realization remains separate. |
| Common model admission | CPU opens its actual backend during preflight; CUDA residency facts are conditioned on the selected CUDA kind. No hidden fallback or third-backend workaround is introduced. |
| Managed source publication | Existing Darwin adoption failed at Linux-only atomic publication. The source owner now calls the existing platform no-replace primitive; Mac/Linux adoption and replacement refusal pass. |
| Reader build | The official container oracle explicitly disables CUDA, Metal and OpenMP. CPU/Metal builds require no CUDA tooling; Linux CPU qualification requires no Apple tools/frameworks. |

These are canonical, backend-neutral changes for the independently resumed
small-model Task, reviewable separately from Metal implementation. They use
existing operation, resource, binding and state owners. No separate Metal
runtime, family generation loop or weakened system reserve is introduced.

### Actual native generation and independent oracle

Both final native runs complete on the M5 Pro **CPU**, with context capacity
32, one-token prefill chunks, greedy target-only execution and eight new tokens:

| Prompt | Actual generated IDs | Native detokenized bytes |
| --- | --- | --- |
| `The capital of Italy is` | `21047,13,198,760,6511,314,9338,369` | ` Rome.\nThe capital of France is` |
| `def add(a, b):` | `198,262,460,264,478,292,271,727` | `\n    return a + b\n\ndef` |

Both report `status=complete`, eight sampled and eight committed tokens, and
`stop_reason=max-new-tokens`. Generated byte counts are 31 and 22. YVEX's typed
generated-text digests are respectively
`941f0c583f159d8eac1621ae4acfbb1c54882f82c140f4ae2a24df9844274dff` and
`1788814db90c8a6c2a6daaee5c4ee5ef4915bc62a58703c256c8d50c91e80f4c`.
They hash the length-delimited generation-text domain and byte payload, rather
than only raw UTF-8. Native detokenization reproduces the bytes from actual IDs;
independent digest reconstruction agrees. The separate raw UTF-8 SHA-256 values
are retained in `numerical-comparison-final.json`.

An independent PyTorch 2.14.1 / Transformers 5.18.0 CPU eager reference loads
the pinned original safetensors, with all 36 source F32 recurrent vectors
restored explicitly. It produces exactly the same two eight-token prefixes.
This criterion supports these bounded greedy outputs; it does not establish
bitwise whole-model logits, long-context behavior or model quality. Reference
execution is an oracle; native generation uses YVEX throughout.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Preparation / replay | Verified exact source, sealed physical plan, native emitter | 320 text tensors, exact BF16/F32 types | Same full artifact bytes | Clean replay SHA-256 equals qualified artifact; independent reader accepts structure | Exact full-file and descriptor identity | PASS | Source-faithful text representation |
| Native CPU generation | Independent upstream source-precision reference | Two raw prompts, eight greedy tokens each | Matching continuation and decoded bytes | Both complete with eight committed tokens; both prefixes match | Exact 16 token IDs, lengths and typed text digests | PASS | Bounded native CLI generation |
| Normal host lifecycle | Existing CLI/protocol and isolated runtime namespace | Exact CPU profile, context 32 | Load/unload one engine; stop owned host | Generation 1 loaded, then unloaded; owned process exits 0 | Exact artifact identity/state and cleanup | PASS | Normal CLI engine lifecycle |
| CPU/backend/state refusal | Existing numerical/state/resource contracts | Analytic F32 accumulation/RNE, owner failures, cancellation after transaction begin | Correct rounding and fail-closed cleanup | Registered unit tests pass, including source-order cancellation and retained-close retry | Explicit expected values/ownership/state | PASS | Affected implementation contracts |
| macOS regression | Registered CI and native contracts | Clean exact implementation | Retain affected CPU/source/host behavior | CI 120 PASS; native 14 PASS | Zero FAIL/SKIP/BLOCKED/ERROR | PASS | Mac regression |
| Linux CPU separation | Same common source in disposable Exon checkout; CUDA disabled | Clean exact implementation | No Apple tooling; retained CPU behavior | CI 120 PASS; native 14 PASS | Zero FAIL/SKIP/BLOCKED/ERROR | PASS | Linux CPU regression/platform separation |
| Metal regression | Independent host bit oracle and existing failure owner | Existing shared-buffer F32 embedding/fault fixtures | Retain bounded GPU foundation | Metal 2 PASS | Zero bit mismatches; checked refusal/cleanup | PASS | Metal primitive foundation only |
| Release projection | Canonical v4 catalog, full integrity, reviewed exact assessment | This artifact, upstream Apache-2.0 license, lineage and runtime evidence | Offline eligible release | `release-ready-final.json` is `READY_TO_PUBLISH` | Exact-subject checksummed evidence | PASS | Publication eligibility, not upload |
| Hugging Face publication | Native Hub API, anonymous public inspection and canonical registry/release projection | Qualified five-file package, exact remote commit and public card | Public exact distribution without changing artifact identity | Mac OAuth upload succeeds; anonymous API and full small-file reads verify public visibility, card rendering and metadata; GGUF LFS digest/size agree | Exact SHA-256/size, Git identities for metadata and unchanged artifact-set identity | PASS | Exact public artifact and canonical location |

Final immutable run identities at `17e5e0a1`:

| Qualification | Run identity |
| --- | --- |
| Mac CI 120 PASS | `9203ba3fa5aa1534ba6d123329e4bdca1d3258d94322aab0ffddbfb118094f6f` |
| Mac native 14 PASS | `fdd3818ace884413b3ffd4c2ac2d2c9a3f2c9d61ea8c34b9bb6bd797fe06df22` |
| Mac Metal 2 PASS | `fc2f34a4305f4e316bcbd6a9d89ecc4a88c3d14f38a4f9d65ecb4fb05b28922b` |
| Linux CI 120 PASS | `d60ea5f2869eba5aa4ad5833b480ce46ebd7eb50b7f65bd07804a5c048b45250` |
| Linux native 14 PASS | `b204d0ca8cfacd11d2e933aa6954e08e53fbc92492f6aa240e9a2b80dfaadbf8` |

### Memory, storage and timing facts

The hardware has 25,769,803,776 bytes (24 GiB) of shared system RAM. The selected
model execution is CPU; no whole-model GPU residency or bandwidth was measured.
`/usr/bin/time -l` observes maximum process RSS of 1,652,899,840 bytes for the
Italy run and 1,652,883,456 bytes for the code run, about 1.54 GiB each. Its
separate peak-memory-footprint metric is not interchangeable with RSS. Observer
wall durations are 16.935147292 and 17.940701167 seconds, including CLI startup
and engine work; these are single bounded runs, not a latency/throughput benchmark.

The GGUF has 1,528,569,856 filesystem-allocated bytes. Controlled replay's
sampled allocation high-water is that same extent, a lower bound; exact peak
preparation storage and historical download timing were not measured. Full-file
integrity time is a separate current checksum measurement. Actual per-buffer
physical residency and model working set remain unknown. Shared hardware does
not make CPU execution a Metal execution claim. Foundation-only resource/copy
facts remain in [the Metal report](macos-metal.md#storage-and-observed-resource-facts).

### Native command and retained local evidence

Run from `/Users/mothx/Developer/YAI/yvex-metal`:

```sh
./yvex bench transformer generate \
  --target qwen3.5-0.8b \
  --artifact /Users/mothx/lab/models/representations/qwen3.5-0.8b/ba86df2b7b2d716095da083b3f43da734f9c6c21290b32f679da50eb46c3c990/model.gguf \
  --runtime-binding /Users/mothx/lab/models/registry/qwen/qwen3.5-0.8b-bindings/c7a79f477cff58e328f854ade7fa1028d24a2e71bada0631449983e4feedf250.yvex-runtime-binding \
  --backend cpu --generation-mode target-only --strategy greedy \
  --text 'The capital of Italy is' \
  --context-capacity 32 --max-new-tokens 8 --prefill-chunk-tokens 1 --output json
```

The JSON reports actual token IDs and typed evidence; `inspect tokenizer decode`
on those IDs and the same artifact exposes text bytes. Ordinary `model prepare`,
`serve`, `model load qwen3.5-0.8b --ctx 32`, `model unload` and `host stop` use the
existing host lifecycle. Interactive conversation/chat policy is independently
unsupported by this integration; the qualified command uses raw completion.

All actual source tensors, representations and bindings remain under
`/Users/mothx/lab/models`. Raw and immutable release evidence is retained at
`/Users/mothx/lab/models/evidence/releases/qwen3.5-0.8b-macos-20261001`:
`build-replay-clean`, `runtime-italy-final`, `runtime-code-final`, full integrity
and fresh tensor manifest, numerical comparison, host lifecycle, exact QA
receipts, reviewed assessment, `release-ready-final.json`, prepared public card/
license/manifest and `publication-attempt-final.json`. A SHA-256 metadata index
binds retained records. Independent reference records remain under
`evidence/qwen-small-oracle`. Weights, runtime dumps and registries stay out of Git.

### Verified Hugging Face publication

The initial Exon create attempt returned 403 because its existing token was
read-only; `publication-attempt-final.json` retains that historical failure.
The operator then logged in directly on the Mac. The existing local OAuth
credential created and uploaded the repository without transferring credentials
between machines. The distribution is now public:

[Qwen3.5-0.8B Text GGUF](https://huggingface.co/yailabs/Qwen3.5-0.8B-Text-GGUF/tree/7eab0fd727ccd5148f127466934790f344d44d38),
immutable commit `7eab0fd727ccd5148f127466934790f344d44d38`.

The full GGUF's authoritative remote LFS SHA-256/size match the exact qualified
local file. This does not claim a second full remote-payload download. Anonymous
full reads independently verify README, LICENSE, manifest and tensor manifest
against local SHA-256 and the provider's Git blob identities. The public model
page returns 200, contains the rendered card heading and explicit CPU/Metal
scope, and official card metadata validation passes. The complete file set
includes the five intended files plus provider `.gitattributes`.

Publication is added through the installed common registry API, preserving
both existing artifact entries and all earlier registry facts. Fresh catalog v4
joins the exact published location to the same logical/artifact identity.
The canonical release projection returns `PUBLISHED`, retaining artifact-set
identity `sha256:93ba00bd319ef61cf9fc549c06ea3b685e5d203095f1a139fcffe13b08c97f71`.
Public manifest SHA-256 is
`455816818b0348e019b19ee78d43bd78222941db2667a496da08fe30ef262590`.

Raw upload observations, anonymous HTTP/card evidence, full publication receipt,
before/after registry snapshots, fresh catalog and `release-published-final.json`
are retained with the earlier qualification evidence. Public distribution of
this exact artifact is distinct from YVEX product or general release readiness.
No credential-scope blocker remains for this publication.

### Earned boundary and remaining work

The selected Task is COMPLETE at this exact scope: native small-model
acquisition, compilation, explicit CPU execution, two bounded independent
continuations, CLI engine lifecycle and verified public artifact distribution.
The earlier Mamba memory refusal remains historical evidence, not a blocker for
this small artifact.
Primary Exon work and main remain unchanged; the published temporary branch is
reviewable, and no heavy model was run on Exon. No new macro ADR or architecture
is selected. Documentation routes the result through Task, Status, family and
Evaluation owners.

Metal-specific matmul/normalization/recurrence/attention realizations and their
numerical contracts, plus capability-driven capacity and specialization
lifetime, remain required before a Metal engine can execute this model. There
is no full-model Metal, chat, vision/MTP, low-precision, performance, general
model or YAI/Studio composition claim here.

`progression_decision=proceed`, `downstream_safe=true` only for the exact CPU
artifact, bounded native generation, verified publication and retained Metal
primitive foundation. Nothing selects or closes the next Metal milestone.

## Historical first attempt — 2026-10-01

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
