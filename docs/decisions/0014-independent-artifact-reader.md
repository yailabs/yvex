<!-- docs:metadata
title: 0014 — Independent structural reader in physical production
id: yvex.decisions.0014-independent-artifact-reader
document: reference
status: current
owner: artifact
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0014 — Independent structural reader in physical production

**Accepted selection within Program P; model-quality qualification is separate.**

## Context

Complete native production proof already requires an independent pinned GGUF
reader. A test-only checker leaves the ordinary compiler unable to publish a
new binding through that proof. Accepting an arbitrary script, successful exit
code or user-authored receipt would not establish the required parser authority.

## Decision

The Rust product links the exact `ggml-base` parser revision already required
by artifact admission. The authenticated archive and build receipt are owned by
`config/gguf_reference.json` and the build pipeline. Generated FFI derives from
the actual external headers. The adapter only reads independent structural
facts; YVEX artifact/compilation owners retain admission and binding decisions.
No external inference backend, quantizer, trainer or family policy is adopted.

The opt-in emission/binding operation verifies the temporary artifact before
publication, and passes the resulting snapshot-bound fact directly to native
admission. It neither imports externally asserted success nor persists a new
parallel admission database. The existing writer, source authentication,
physical compatibility and binding-publication lifecycles remain authoritative.

Artifact and binding publications have distinct durable outcomes. If the latter
refuses after the former succeeds, report a retained artifact plus precise
binding refusal with nonzero exit. Never delete the artifact to simulate a
transaction spanning two different lifetimes. No engine is loaded implicitly.

## Consequences and alternatives

`make client` additionally needs CMake and a C++ compiler/runtime; `make lib`
remains independent of this product verification dependency and Cargo. The
external source/library are authenticated on reuse, build identity records their
receipt, and packages carry the original upstream license. Distribution closure
and platform qualification are still separate gates.

Rejected alternatives: a second YVEX parser presented as an independent oracle;
arbitrary helper execution; copied test code or a test executable as a production
dependency; trusting a caller's `accepted` flag; weakening complete-artifact
admission for planner-generated candidates. Independent structural acceptance
does not establish upstream logits, quantization quality or inference speed.
