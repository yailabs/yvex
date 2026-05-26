# YAI NET Spine Reference

Source repository: yai
Source path: docs/spines/net-spine.md
Reference version: NET.SPINE.0.1
Authority: YAI repository
Mode: copied compatibility reference

This file is copied into CLORI to keep CLORI aligned with YAI NET contracts.
This file is not authoritative.
When this reference diverges from YAI, YAI wins.

---

# NET Spine

Reference version: NET.SPINE.0.1
Authority: YAI repository
Status: Boundary and roadmap foundation

NET is the root-level communication substrate inside YAI.

NET owns:
- local node identity
- endpoint registry
- LAN discovery
- remote endpoint registry
- stream envelopes
- transport adapters
- node health
- capability advertisement
- routing
- invocation transport
- trace and receipt transport
- transport metrics

NET does not own:
- case authority
- policy resolution
- memory truth
- graph truth
- fact truth
- journal truth
- neural execution
- model loading
- model decoding
- operator approval
- action eligibility

YAI controls authority.
NET moves streams.
External nodes execute capabilities.

## Scope

This spine is separate from the main YAI delivery spine. It defines the NET
component roadmap without rewriting or expanding the existing 120-wave YAI main
spine.

NET is canonical in this repository. External repositories may copy this file
as a compatibility reference, but copied references are not authoritative.

`interfaces/transports/` remains the contract and vocabulary layer for transport
schemas, envelopes, readiness matrices and handoff language. NET does not move
those contracts into YAI; NET implements the YAI-side runtime communication
substrate that can consume and honor those contracts.

CLORI is an external NET-compatible node target, not a dependency vendored into
YAI.

## Deliveries

### NET.SPINE.0 - Root Substrate Boundary

Create root-level `net/` boundary as YAI communication substrate.

### NET.SPINE.1 - Terminology and Ownership Canon

Define NET vocabulary: node, endpoint, stream, capability, route, health,
transport, invocation.

### NET.SPINE.2 - Stream Envelope Canon

Define canonical stream envelope for request, response, chunk, error, metric
and receipt transport.

### NET.SPINE.3 - Node Identity Canon

Define stable local/remote node identity without leaking machine internals into
`system/`.

### NET.SPINE.4 - Capability Advertisement Canon

Define how nodes advertise capabilities such as `neural.llm.decode`,
`neural.embedding.encode` and `metrics.stream`.

### NET.SPINE.5 - Local Endpoint Registry

Define local endpoint registry for services available on the same machine.

### NET.SPINE.6 - Health / Readiness / Liveness Contract

Define health, readiness and liveness probes.

### NET.SPINE.7 - Local Process and Service Lifecycle Boundary

Define process/service lifecycle boundary without making `system/` start or
supervise tools directly.

### NET.SPINE.8 - Localhost Transport Boundary

Define localhost transport for local daemons.

### NET.SPINE.9 - Unix Socket / Local IPC Boundary

Define local IPC boundary for future Unix socket or platform-native channels.

### NET.SPINE.10 - LAN Discovery Boundary

Define LAN discovery as NET-owned, not system-owned.

### NET.SPINE.11 - Remote Endpoint Registry

Define remote endpoint registry for static or configured nodes.

### NET.SPINE.12 - Transport Adapter Boundary

Define transport adapter abstraction: HTTP, gRPC/future, socket/future,
stream/future.

### NET.SPINE.13 - Streaming Protocol Boundary

Define streaming protocol for chunked outputs, partial metrics and completion
receipts.

### NET.SPINE.14 - Invocation Envelope Canon

Define invocation envelope independent from CLORI, tools or future nodes.

### NET.SPINE.15 - Routing and Selection Boundary

Define routing and selection: capability match, node health, latency, policy
handoff.

### NET.SPINE.16 - Backpressure / Queue / Retry Boundary

Define queue, retry, timeout and backpressure semantics.

### NET.SPINE.17 - Trace and Receipt Transport Boundary

Define how trace ids, run ids, receipts and metrics move through NET.

### NET.SPINE.18 - Security and Trust Boundary

Define trust boundary: NET transports, but YAI authorizes.

### NET.SPINE.19 - Observability and Metrics Boundary

Define NET metrics: discovery latency, connect latency, stream overhead, queue
wait, transport errors.

### NET.SPINE.20 - NET CLI Control Surface

Expose CLI surface: `yai net nodes`, `yai net health`,
`yai net capabilities`, `yai net trace`.

### NET.SPINE.21 - system/ Extraction Pass

Move or quarantine network/discovery assumptions currently inside `system/`.

### NET.SPINE.22 - engine/ Extraction Pass

Move or quarantine network/endpoint assumptions currently inside `engine/`.

### NET.SPINE.23 - External Capability Node Contract

Define generic external node contract.

### NET.SPINE.24 - CLORI Node Compatibility Contract

Define CLORI as one NET-compatible node type, not a NET dependency.

### NET.SPINE.25 - Transport Benchmark Harness

Create repeatable NET transport benchmark harness.

### NET.SPINE.26 - Local/LAN/Remote Test Matrix

Define local, localhost, LAN and remote test matrix.

### NET.SPINE.27 - NET/YAI Carrier Integration Boundary

Define how YAI carrier invocation uses NET without owning transport.

### NET.SPINE.28 - NET Reference Copy Contract for External Repos

Define how external repos copy/reference NET.SPINE.

### NET.SPINE.29 - NET v0 Freeze

Freeze NET v0 boundary.
