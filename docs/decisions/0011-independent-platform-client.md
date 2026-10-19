<!-- docs:metadata
title: Independent platform client
id: yvex.decisions.0011-independent-platform-client
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Independent platform client

YVEX owns the public protocol and its MIT client implementation. The canonical Rust
crate is `sdk/rust`, independently buildable with its own Cargo lock/workspace; the
safe TypeScript projection is `@yvex/sdk` in `sdk/typescript`. It has no YAI semantic
or Studio dependency. Optional finite native FFI requires an explicitly supplied
public installation, never private runtime headers.

Historical extraction placed this code under yai-sdk without a semantic dependency
on YAI. The initial migration retained an exact dependency and reexport, without
a second protocol implementation or generator. The operator-authorized
2026-10-19 continuation retires that transitional facade through an explicit
breaking YAI SDK release. Studio and other clients consume YVEX directly;
YAI SDK owns only Core client contracts. Third-party clients receive the same
management, trust, credential-reference and recovery boundaries. Migration and
retained MIT test provenance are documented in the [SDK guide](../../sdk/README.md).

The existing CLI and public service share producer domain owners. This decision
does not turn terminal narrative into a client API, expose private computational
protocols or require a local service for offline compiler commands.

The producer owns lifecycle/admission, including profile readiness and Build plan
identity. The SDK owns exact request identity, transport, credential persistence
and read freshness. Consumer presentation may retain safe observations but cannot
promote an unavailable Host, old Engine generation or discovered certificate into
current authority.

Training remains unimplemented. Future Dataset/TrainingRun/Checkpoint/Evaluation
owners must publish real capabilities and exact lineage into the existing source,
package and deployment lifecycle. No empty future DTOs or granted capability IDs
are introduced by this extraction.
