<!-- docs:metadata
title: 0009 — Rust product shell over the C/CUDA engine
id: yvex.decisions.0009-rust-product-shell
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# 0009 — Rust product shell over the C/CUDA engine

[Up](README.md)

Status: accepted. Date: 2026-10-02.

## Context

The operator authorized a complete product-shell migration, not a second CLI
or a Rust wrapper around C dispatch. REPLAI already supplies native Rust editing
and semantic presentation. The C shell's application orchestration, parsing,
serializers and terminal adapters were not computational-library responsibilities.

## Decision

The sole product executable remains `yvex`. `src/cli/rust/main.rs` owns its
entrypoint; the Rust shell owns grammar projection, application dispatch,
human/machine output, foreground host coordination and native chat.
`config/operator/registry.json` remains the sole authored grammar. Generated
JSON is embedded by Cargo; the independent generated C descriptor remains a
tested projection, not a second parser or product dispatcher.

C/CUDA owns source, artifacts, compilation, deployment, models, runtime,
scheduling and backend execution. Compiler-derived private bindings and a
small auditable `ffi` module adapt typed operations and results. Ordinary shell
code denies unsafe Rust. No human C output crosses this boundary. Runtime
requests reuse the C-owned typed client and private wire; no Rust protocol fork
or engine state replica is introduced. Public C headers remain independent.

GNU Make owns native compilation, generated authorities and product assembly.
`make lib` needs neither Cargo nor REPLAI; `make client` invokes Cargo's own
incremental build and atomically publishes one executable. Native build and
Rust-shell/toolchain identities remain separately representable. Install uses
the package manifest and GNU installation directories, including `DESTDIR`.

Native Rust REPLAI consumption supersedes only ADR 0007's C-consumer adapter.
The producer's generic terminal ownership and independent C ABI remain valid.
One pin authenticates revision, tree and archive. Make realizes a checked,
generated external source workspace for Cargo; no floating revision, vendored
tracked copy or required sibling checkout exists. Rust consumes semantic
documents, composed prompts, driven interaction, completion and quiet output
directly. YVEX retains candidate meaning, sessions, channels and cancellation.

## Consequences and qualification

The superseded C shell leaves production membership. Its meaningful machine,
refusal, lifecycle and presentation oracles move to the current consumer tests;
human byte identity is not a compatibility promise. CLI JSON, public C ABI,
private wire v24 and OpenAI/remote-management semantics do not change merely
because the entrypoint language changes.

Process-scoped signal notifications have joined workers and bounded wake state;
REPLAI remains the sole terminal capture/restoration owner. This does not export
the retired C terminal-scope API or promise an embeddable Rust signal API.

Linux software/PTY evidence and actual macOS qualification remain separate
gates in the selected [Task](../project-control/TASKS.md). Retained native macOS
foundation evidence does not qualify this new shell automatically. A build or
source-package pass does not qualify model execution or customer distribution.
Cargo inputs join the fail-closed legal policy; candidates remain UNQUALIFIED
until an exact compiled package's component/notices closure is reviewed.

## Alternatives rejected

- Two permanent CLIs or a C parser/renderer fallback: duplicate product authority.
- Rust implementations of engine state or the local wire: wrong ownership.
- Separate daemon/helper product: violates the single-executable lifecycle.
- Independent Cargo REPLAI revision: breaks immutable producer authentication.

No DeepSeek optimization, A03, Laya or remote mutation follows from this decision.
