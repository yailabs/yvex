<!-- docs:metadata
title: Source and Provenance
id: yvex.architecture.source-provenance
document: architecture-plane
status: mixed
owner: source
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: source-provenance
related: [yvex.architecture.model-semantics, yvex.architecture.representation-artifacts]
-->

# Source and Provenance

**Authenticate the exact source before interpreting or executing it.**

[Up](README.md)

A model download, a verified snapshot and a prepared artifact are different objects.
Source owns acquisition progress and immutable provenance; downstream compilation
reads authenticated facts and bounded tensor ranges.

## Lifecycle and ownership

1. Discovery names a provider/repository and immutable revision.
2. Acquisition owns a durable operation, generation, progress and stop/resume state.
3. Verification authenticates metadata and payload independently.
4. Compilation consumes the sealed source basis; it does not own the transfer.

The acquisition operation survives the foreground client. Supervisor loss, a
stale PID or a foreign generation cannot silently resume a different transfer.
Unknown provider progress remains unknown. A successful download does not
establish a model's architecture or executable support.

Explicit repository, family, revision and file-selection acquisition resolves
that exact selection, including supervisor re-entry, rather than an ambiguous
retained short name. Metadata-only and complete-payload selections may coexist;
an ambiguous short-name control request still refuses. A supervisor that never
publishes its process identity leaves STARTING only for the bounded startup
window, then becomes stopped/resumable. A late worker must still own the exact
live operation before starting a provider; this does not time out a healthy
download merely because payload progress is unavailable.

Atomic sidecar publication and bounded reading compose through one opened file
identity. The generic metadata reader sizes and reads that descriptor, rather
than sizing a pathname which can be replaced before open. A concurrent reader
therefore sees an old or new complete published record, not a mixed-size view.
Bounds and nonregular-file refusal remain independent of that snapshot rule;
this is not payload verification or an inference-performance claim.

Explicit source-status audit uses the native safetensors header owner to
distinguish malformed headers from declared tensor extents beyond the available
payload. This is a bounded structural observation, not payload authentication
or admission. Routine acquisition progress counts files and bytes without
reading tensor headers; an audit never makes a truncated source admissible.

## Source, library and recommendation

The Model Library joins remote representations, local sources, packages and
launchable profiles. It projects existing facts. A recommendation hint does not
admit an artifact or prove that a proposed representation can be built.

**Illustrative:** losing a terminal during acquisition does not turn partial
bytes into a verified source. Resume the same operation, verify its immutable
basis, then prepare a representation.

## Source intake and trust

Source intake records repository/revision, configuration, tokenizer sidecars,
shards, tensor names, shapes, dtypes, and byte ranges. Structural inventory and
payload trust are separate. A source can be inventoried without having every
payload digest authenticated.

The retained source snapshot is immutable and indexed. Downstream owners
consume typed facts and exact bounded ranges; they do not rescan source headers
or infer semantics from filenames.

Payload admission has an explicit bootstrap boundary. `source verify`
promotes verified metadata/header provenance to source-manifest v3 only after
reading every shard and matching its authoritative provider SHA-256. The v3
manifest lives outside the source snapshot, binds the ordered aggregate payload
identity, and can be reopened without a second full payload pass. Transformation
planning still consumes zero payload bytes; execution admits ranges only from
that trusted identity.

The native model-preparation selector consumes a Safetensors checkpoint
acquisition, not a metadata-only acquisition of the same revision. Coexisting
metadata therefore does not make a single payload source ambiguous. Multiple
matching payload acquisitions still refuse automatic selection. The catalog's
format is a selection filter, not authentication: the ordinary exact-revision,
source-manifest and payload admission gates remain mandatory before compilation.


## Exact identity computation

The shared core SHA-256 owner computes canonical source, artifact, binding and
execution identities. Linux little-endian AArch64 hosts with an observed SHA2
CPU capability may use the instruction-backed integer compression function;
other hosts retain the portable implementation. Capability detection is cached
atomically without changing the process-wide instruction baseline. Incremental
updates, finalization/refusal, little-endian scalar and length-delimited text
encodings remain identical. This is exact computation of existing identities,
not a new provenance owner, reduced authentication or changed model numerics.

Independent digest/control evidence and bounded complete-model effects are
retained in [Evaluation](../evaluation/retained-observations.md#exact-identity-computation-on-arm64-2026-10-08).
There is no new public record, identity schema or macOS acceleration claim.

## Implementation and evidence

[include/yvex/source.h](../../include/yvex/source.h) · [include/yvex/catalog.h](../../include/yvex/catalog.h) · [src/source](../../src/source) · [src/model/remote.c](../../src/model/remote.c)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Model Semantics and Admission](model-semantics.md) · [Representation, Quantization and Artifacts](representation-artifacts.md)
