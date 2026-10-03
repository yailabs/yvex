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


## Implementation and evidence

[include/yvex/source.h](../../include/yvex/source.h) · [include/yvex/catalog.h](../../include/yvex/catalog.h) · [src/source](../../src/source) · [src/model/remote.c](../../src/model/remote.c)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Model Semantics and Admission](model-semantics.md) · [Representation, Quantization and Artifacts](representation-artifacts.md)
