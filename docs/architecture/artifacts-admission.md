<!-- docs:metadata
title: Artifact and Package Admission
id: yvex.architecture.artifacts-admission
document: architecture-plane
status: mixed
owner: artifact
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: artifacts-admission
related: [yvex.architecture.representation-artifacts, yvex.architecture.runtime-lifecycle]
-->

# Artifact and Package Admission

**Authenticate a complete package before publishing a usable mapping.**

[Up](README.md)

A structurally valid container may still be incomplete, unsupported or bound to
the wrong source. Admission verifies the claimed representation and semantic
coverage before materialization lends bytes to execution.

## Gates and lifetimes

| Gate | What it establishes | What it does not establish |
| --- | --- | --- |
| Container validation | Bounds and structural integrity | Required model roles |
| Artifact admission | Exact identity, coverage and supported representation | Device feasibility |
| Materialization | Authenticated mappings and retained storage lifetime | Executed numerics |
| Runtime binding | Compatible compiled/package facts | Release qualification |

The mapped backing must outlive its borrowers. A reopened lease authenticates
the same artifact; a pathname alone does not preserve identity. Missing
mandatory tensors, malformed layouts or changed identities refuse before a
partially ready engine is exposed.

See the [artifact contract](../contracts/artifacts.md) for exact requirements
and [storage contract](../contracts/model-storage.md) for persistent ownership.

## Artifact emission and admission

The GGUF writer plans exact metadata, tensor directory order, alignment,
padding, ranges, and payload bytes before transactional publication. Native
reader and global-layout owners independently validate the emitted container.

Complete-artifact admission additionally binds model metadata, tokenizer
facts, every required tensor role, qtype support, source/derivation/variant
identities, and exact file identity. Structural GGUF validity is necessary but
not sufficient.

An expected-identity catalog must retain the source lineage of each physical
row; a family-wide source default cannot stand in for a checkpoint. Equal
structural inventory/mapping identities can coexist with different payloads
and revisions. Catalog reconstruction selects expected facts, not observed byte
integrity: the reopened artifact must still authenticate its complete file
identity before runtime use. A catalog entry is not numerical, model-quality or
performance qualification.

Registry drift comparison uses the structural projection of a support level.
A container snapshot observes at most `selected-tensor-materialized`; a
registered `generation-ready` label records a different, later evidence scope.
Their labels alone do not establish metadata drift. Digest, extent, tensor
metadata and selected-embedding readiness comparisons remain exact. Neither
comparison nor integrity verification copies the stronger registered label
into the current file observation. A metadata/readiness-status PASS here is
not a generation-readiness certificate: authenticated complete-artifact and
runtime-binding admission, backend capacity and actual engine/session readiness
remain independently mandatory. Missing structural facts still refuse.

Artifact materialization builds and commits an authenticated package mapping:
checked tensor bindings and bounded access to file-backed package ranges used by
the runtime descriptor, PEIR construction and runtime binding. It does not
import a concrete model family, choose a deployment implementation, infer
consumers from tensor names, execute a graph, or establish model support.

Backend/model weight materialization is a separate later mechanism. It turns
admitted package bindings into host, CUDA-addressable-host, device, staged, or
derived executable resources for an engine deployment. Its resources do not
become artifact materialization records or PEIR facts. A derived representation
becomes package truth only if it is deliberately emitted and admitted as a
separately authenticated asset.


## Implementation and evidence

[src/artifact](../../src/artifact) · [include/yvex/artifact.h](../../include/yvex/artifact.h) · [include/yvex/materialization.h](../../include/yvex/materialization.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Representation, Quantization and Artifacts](representation-artifacts.md) · [Engine and Session Runtime](runtime-lifecycle.md)
