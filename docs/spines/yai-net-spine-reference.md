# YAI NET Spine Reference

Source repository: yai
Source path: docs/spines/net-spine.md
Reference version: NET.SPINE.0.2
Authority: YAI repository
Mode: copied compatibility reference

This file is copied into CLORI to keep CLORI aligned with YAI NET contracts.
This file is not authoritative.
When this reference diverges from YAI, YAI wins.

---

# NET Spine

Reference version: NET.SPINE.0.2
Authority: YAI repository
Status: Root component scaffold started

NET is the root-level runtime communication substrate inside YAI.

YAI controls authority.
NET moves streams.
External nodes execute capabilities.

CLORI extension:
YAI controls authority.
NET moves streams.
CLORI executes neural computation.

This spine is separate from the main YAI delivery spine.
It does not rewrite or expand the existing 120-wave YAI main spine.

NET is canonical in this repository.
External repositories may copy this file as a compatibility reference.
Copied references are not authoritative.

`interfaces/transports` remains the contract and vocabulary layer.
NET is the YAI-side runtime communication substrate that may consume, honor or
adapt those contracts.

## Deliveries

### NET.SPINE.0 - Root Component Scaffold + Boundary Guard

Create the root `net/` component layout, public vocabulary headers,
README-only source roots, component docs, Makefile targets and boundary guard.
Add YAI-side docs that point to NET without expanding the main YAI spine.
No discovery, transport, routing, server or CLORI support is implemented.

### NET.SPINE.1 - Canonical Terms + File/Header Discipline

Add canonical term mapping across docs and headers, then wire guard coverage for
file placement, include guards and public/private header posture. Connect NET
terms to `interfaces/transports` vocabulary without moving interface contracts.

### NET.SPINE.2 - Stream Envelope Types + Fixtures

Create stream envelope structs/types, JSON fixture examples and schema notes.
Add tests that validate request, response, chunk, metric, receipt, error and
complete event fixtures. It does not implement live streaming yet.

### NET.SPINE.3 - Node Identity Types + Local Machine Projection

Add stable node identity types, local machine projection notes and fixtures for
local, localhost, LAN, remote and external node ids. Guard against leaking
machine internals into `system/` authority material.

### NET.SPINE.4 - Capability Advertisement Types + Registry Seed

Create capability advertisement types and seed registry fixtures for
`neural.llm.decode`, `neural.embedding.encode`, `metrics.stream`,
`receipt.emit` and generic external capabilities. Add validation that
capabilities are descriptive, not authority.

### NET.SPINE.5 - Local Endpoint Registry Skeleton

Implement the local endpoint registry skeleton and fixture-backed tests for
registered, missing and unavailable local endpoints. Keep endpoint truth inside
NET and policy decisions inside YAI.

### NET.SPINE.6 - Health / Readiness / Liveness Probe Skeleton

Add probe result types, fixture examples and a no-network probe skeleton for
unknown, alive, ready, degraded and unavailable states. Do not start or
supervise services.

### NET.SPINE.7 - Local Service Lifecycle Contract

Create lifecycle contract docs and types for service status handoff. Add guards
that prevent `system/` from becoming a generic process supervisor through NET.

### NET.SPINE.8 - Localhost Transport Adapter Skeleton

Implement a localhost transport adapter skeleton with no live HTTP behavior.
Add fixtures and tests for adapter configuration, unsupported endpoints and
boundary errors.

### NET.SPINE.9 - Local IPC / Unix Socket Adapter Skeleton

Add local IPC adapter skeleton files and fixtures for future Unix socket or
platform-native channels. Keep socket operations unimplemented until a later
transport wave.

### NET.SPINE.10 - LAN Discovery Adapter Skeleton

Create LAN discovery adapter skeleton and docs that reserve discovery posture
inside NET. Add guards preventing LAN discovery assumptions from spreading into
`system/` or `engine/`.

### NET.SPINE.11 - Remote Endpoint Registry Skeleton

Add remote endpoint registry skeleton, static configuration fixtures and tests
for configured, unavailable and unsupported remote endpoints. Do not implement
remote network calls.

### NET.SPINE.12 - Transport Adapter Interface

Implement the public transport adapter interface and adapter status types.
Connect localhost, IPC, LAN and remote adapter skeletons to the interface
without adding live transport behavior.

### NET.SPINE.13 - Streaming Runtime Skeleton

Add streaming runtime skeleton, event queue types and fixture validation for
ordered stream events. Do not implement background runtime services.

### NET.SPINE.14 - Invocation Envelope + Request Correlation

Create invocation envelope types, request correlation ids and fixtures tying
invocation, stream, trace and receipt ids together. Add tests for missing and
malformed correlation material.

### NET.SPINE.15 - Router Skeleton + Selection Inputs

Implement router skeleton types for capability match, node health, latency and
policy handoff inputs. Add tests that prove router output is advisory until YAI
authority accepts it.

### NET.SPINE.16 - Queue / Retry / Timeout / Backpressure Skeleton

Add queue, retry, timeout and backpressure types plus fixture examples for
waiting, retryable, expired and rejected invocations. No worker scheduler is
implemented in this wave.

### NET.SPINE.17 - Trace / Receipt Transport Types

Create trace and receipt transport types and tests for propagation through
stream envelopes. Guard the rule that NET transports receipts but YAI decides
authority.

### NET.SPINE.18 - Trust / Security Boundary Enforcement Guard

Add enforcement guard coverage for trust boundary language, forbidden authority
claims and accidental writes into YAI graph, memory, facts or journal from NET.

### NET.SPINE.19 - NET Metrics Collector Skeleton

Implement metrics collector skeleton types and fixtures for discovery latency,
connect latency, handshake latency, first chunk latency, stream bytes, queue
wait and transport error counts.

### NET.SPINE.20 - NET CLI Read-Only Control Surface

Add read-only `yai net` command skeletons for nodes, health, capabilities and
trace inspection. Commands may report scaffold/planned status only until backing
runtime behavior exists.

### NET.SPINE.21 - system/ Network Assumption Extraction

Audit `system/` for provider health, endpoint checks, carrier transport,
LAN/discovery and service lifecycle assumptions. Extract or quarantine findings
behind NET docs, guards or adapter skeletons.

### NET.SPINE.22 - engine/ Endpoint Assumption Extraction

Audit `engine/` for endpoint state, external service status, live transport
residue and provider runtime assumptions. Extract or quarantine findings so
engine remains data-plane, not transport owner.

### NET.SPINE.23 - External Capability Node Contract

Create the generic external capability node contract with fixtures, tests and
compatibility docs. Keep external execution separate from YAI authority.

### NET.SPINE.24 - CLORI Node Compatibility Contract

Add CLORI-specific compatibility contract, fixture examples and receipt
boundary tests. CLORI remains external and is not vendored into YAI.

### NET.SPINE.25 - Transport Benchmark Harness v0

Create benchmark harness v0 for transport overhead fixtures and repeatable
measurement shape. Do not publish benchmark claims until real runs exist.

### NET.SPINE.26 - Local / Localhost / LAN / Remote Test Matrix

Add a test matrix covering local, localhost, LAN and remote postures across
registry, health, routing inputs, streaming and receipt transport boundaries.

### NET.SPINE.27 - YAI Carrier Invocation Through NET Boundary

Connect YAI carrier invocation docs and skeleton handoff types to NET transport
boundaries. Preserve the rule that carriers and YAI authority own execution
eligibility while NET owns stream movement.

### NET.SPINE.28 - External Reference Copy Sync Contract

Add sync contract and guard instructions for external repositories that copy
`docs/spines/net-spine.md`, including CLORI's non-authoritative compatibility
reference.

### NET.SPINE.29 - NET v0 Freeze

Freeze NET v0 layout, headers, docs, guard coverage, fixture schema posture and
integration boundaries before runtime transport work expands beyond skeletons.
