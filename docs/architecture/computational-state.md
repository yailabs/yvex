<!-- docs:metadata
title: Computational State
id: yvex.architecture.computational-state
document: architecture-plane
status: mixed
owner: runtime
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: computational-state
related: [yvex.architecture.runtime-lifecycle, yvex.architecture.advanced-generation]
-->

# Computational State

**Sessions isolate state; transactions coordinate publication across distinct geometries.**

[Up](README.md)

Computational continuity is broader than a KV cache. Attention, recurrent,
convolution, draft, token, decoder and RNG state share lifecycle coordination;
they do not share one storage geometry or become application semantic memory.

## State map

| Lifetime | Contents | Meaning |
| --- | --- | --- |
| Engine generation | Immutable programs, bindings and admitted resources | Shared executable basis |
| Session | Attention/recurrent banks, workspace and committed position | Isolated mutable computation |
| Transaction | Target/draft candidates, token/RNG/decoder participants | All publish the accepted prefix or abort |
| Captured prefix | Identity-bound snapshot of admitted state domains | Reusable only through compatible attach |
| Borrowed result | Producer-owned publication generation | Expires before workspace reuse |

Not every state class is checkpointable or restart-persistent. Latent/media
execution has its own bounded component lifetimes. Native persistent cognitive
state E and unfinished deliberation L remain
[research](../research/native-computational-state.md), not synonyms for current prefixes.

## Failure example

A speculative suffix is rejected. The runtime publishes the target-verified
prefix across every participant, discards the suffix, and does not replay
accepted target rows. A later cancellation reports already committed progress.
It cannot pretend the committed prefix never happened.

## Sessions and transactional state

Direct causal attention consumes the same transactional logical K/V provider on
CPU and CUDA. CUDA appends to its admitted candidate device bank. Explicit CPU
execution assembles a temporary backend-owned F32 prefix from the provider's
uncompressed contiguous history plus the current projections, executes the
existing exact attention operation, releases the temporary, and stages one host
delta. It creates no second persistent state bank. Capacity includes the
temporary prefix and backend score scratch; execution reports those temporary
extents separately from logical state and zero discrete GPU transfer bytes.
Malformed history, unsupported backends, cancellation and cleanup failures do
not publish a completed result. The enclosing common transaction owns abort.

Execution capacity is runner-independent. `src/runtime/capacity.c` consumes
compiler-owned state geometry, residency and live backend facts; generation and
Decision Readout only supply bounded workload requirements. Ordinary hybrid
prefix qualification therefore needs no temporary generation context. This
keeps persistent pages, row geometry and reserve policy below Output Runners
without introducing another state provider or family runtime.

`src/runtime/session_summary.c` owns typed sequence-resource projection and
non-overlapping session resource aggregation for both the host and readout.
Sequence execution is protected by the session lease; foreign busy observers
consume the last published sequence summary, while the execution owner or an
idle observer may refresh it. Attention provider summaries retain their own
locks. This resource observation is distinct from the stricter, all-domain
committed-state identity, which refuses in-flight observations.

Each execution session borrows one engine and owns mutable state: attention
providers, backend state residency, workspace, committed position, cancellation
state, and a unique batch-source lineage. Server sessions add token ledger,
conversation transcript, incremental decoder, RNG/sampling state, and turn
publication state. Sessions sharing an engine never share mutable sequence
state or workspace.

Different attention classes keep their own geometry and representation. Their
lifecycle converges on the generic provider protocol: begin candidate, stage,
prepare commit, publish commit, abort, reset, invalidate, capture, attach, and
close. Transaction coordination uses a bounded participant collection; target
state, draft state, token ledger, decoder, RNG, and publication remain distinct
participants rather than one homogeneous KV object.

The common attention provider distinguishes request cancellation from lifecycle
invalidation. Cancellation before beginning a layer leaves committed continuity
valid; cancellation within a model batch marks private work failed until abort.
The session coordinator returns cancellation after successful cleanup, without
poisoning target or draft state. Genuine cleanup/accounting failures remain
fail-closed. Conversation-turn recovery remains an application lifetime above
this provider guarantee.

A conversation turn is an ordered collection of typed content parts, not one
attachment and not one session. Parts distinguish text, image, audio, video,
file, and tensor kinds; each has a content digest and may link a derived form to
its original with `derived_from_content_identity`. Modality changes do not
replace the session. Capability admission compares the exact part kinds with
the loaded specialization before numerical work. The current reference CLI
stages multiple local attachments for the next turn and appends text on submit;
future architecture verticals may consume the same contract without adding a
modality-specific session type.

Speculative candidate rows are never publication authority. Target verification
selects one prefix-addressable checkpoint; all participants either prepare and
publish that exact accepted prefix or abort. Rejected suffix state is discarded
and accepted target rows are not replayed. Cancellation after an atomic commit
reports the committed prefix instead of pretending to rewind it.


## Implementation and evidence

[src/runtime/sequence_state.c](../../src/runtime/sequence_state.c) · [include/yvex/internal/sequence_state.h](../../include/yvex/internal/sequence_state.h) · [include/yvex/internal/runtime_prefix.h](../../include/yvex/internal/runtime_prefix.h) · [include/yvex/internal/execution_transaction.h](../../include/yvex/internal/execution_transaction.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Engine and Session Runtime](runtime-lifecycle.md) · [Speculation and Finite Execution](advanced-generation.md)
