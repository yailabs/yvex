<!-- docs:metadata
title: Contracts
id: yvex.contracts
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Contracts

**Exact producer/consumer obligations, separate from explanatory architecture.**

[Documentation](../README.md)

Choose the boundary your code actually crosses. Public APIs, local wire
protocols and internal computational records have distinct compatibility
rules. For the system model, read [Architecture](../architecture/README.md).

## Owners and reading paths

| Integrate a client | Contract | What it owns |
| --- | --- | --- |
| Embed the core | [C API](c-api.md) | Public native entrypoints and lifetime obligations |
| Use the local host | [Local protocol v25](local-protocol.md) | Private same-user transport and typed requests |
| Connect an inference application | [OpenAI profile v3](openai-compatibility.md) | Bounded compatibility, streaming and refusals |
| Manage a server | [Network management](network-management.md) | HTTPS, pairing, discovery and local public socket |
| Operate models and sessions | [Product management v2](product-management.md) · [Lifecycle map](product-management-lifecycle-map.md) | Grants, Jobs, recovery and operation classification |
| Request a finite decision | [Remote producer v1](finite-decision-remote.md) | Exact producer, candidate and result identity |
| Use the technical SSH carrier | [Remote bootstrap v1](remote-management.md) | Separate compatibility transport; not the ordinary HTTPS product path |

| Extend the computational core | Exact owner |
| --- | --- |
| Programs, components and computed indices | [Programs](computational-programs.md) · [Components](component-programs.md) · [Indices](index-programs.md) |
| Stored bytes and authenticated admission | [Artifacts](artifacts.md) · [Model storage](model-storage.md) |
| Execution lifetimes and arithmetic | [Hosted runtime](runtime.md) · [Internal runtime ABI](runtime-abi.md) · [Numerical ABI](numerical-abi.md) |
| Observable facts and qualification | [Events](events-telemetry.md) · [Benchmark publication](benchmark-publication.md) · [Model release](model-release.md) |
