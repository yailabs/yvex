<!-- docs:metadata
title: 0010 — Native product management and explicit connection authority
id: yvex.decisions.0010-native-product-management
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0010 — Native product management and explicit connection authority

[Up](README.md)

Status: accepted. Date: 2026-10-06.

## Context

Ordinary Model, Build, Engine and Session work must be available to native clients
through public YVEX owners. Read-only discovery plus hidden shell commands would
leave Studio dependent on private implementation and make response loss unsafe.
The operator selected HTTPS for remote clients, explicit producer authorization,
protected native client credentials and automatic same-user local discovery.

## Decision

The single standalone `yvex` executable exposes versioned
[product management](../contracts/product-management.md). CLI and SDK are sibling
clients of the existing typed domain owners; neither parses the other's output.
Source, immutable Package, deployment profile, Host, Engine generation and Session
retain separate identities. Product operations publish explicit capabilities.
Durable, peer-scoped Jobs retain request identity and uncertain completion;
observation recovers a lost acknowledgement without blindly repeating a mutation.
Native Host and Session lifetime fences precede effects at their owning layer.

The [network contract](../contracts/network-management.md) selects pinned rustls
HTTPS with explicit local pairing approval and per-request grant revocation.
The producer stores credential digests; the native client owns protected secret
storage. mDNS supplies untrusted discovery hints. A same-user public Unix socket
uses owner-only paths, OS peer identity and a selected service identity precondition.
All transports dispatch the same 36-operation contract. Existing scoped SSH and
finite-decision protocols retain distinct grants; management enrollment grants no
inference credential, YAI trust, Participant authority or Case permission.

The service can exist with a stopped or empty computational Host. Starting it
never replaces another Host. Listener/service provisioning and registry credential
administration remain explicit local owners. Arbitrary file transfer requires a
bounded transfer/admission contract before remote promotion. Training requires
its own producer semantics before capability or UI publication.

## Consequences and qualification

SDK and Studio depend on public schemas and operation identity, not private paths,
CLI spelling or runtime brand. Persistent service TLS identity is separate from
network addresses and computational Host lifetime. Requests, workers, framing,
retention and deadlines are bounded; revocation does not delete durable work.

[Evaluation](../evaluation/product-management-control-plane.md) distinguishes
controlled-domain, native TLS/vault/tiny-CPU and actual desktop-to-isolated-Spark
HTTPS evidence. That boundary does not establish an installed operator service,
physical-LAN discovery, real-model quality, GPU qualification or release readiness.
The public interface has extension points for future training and richer outputs;
no unsupported future capability is granted merely by reserving a screen.

## Alternatives rejected

- Hidden CLI, SSH scripts or private persistence reads in Studio: duplicate owners.
- Unauthenticated LAN management: discovery cannot establish trust.
- A second standalone management executable: duplicates product installation.
- Brand/version branching as capability authority: does not prove operation support.
- One ambiguous Model object or a second acquisition scheduler: loses native identity.
