# YVEX Roadmap

This roadmap is extracted from `docs/spine.md`. The spine remains authoritative.

## P - Pre-Implementation Reset

```text
P0    Repository reset and technical spine
P0.1  Current repo audit
P0.2  Spine technical densification pass
P0.3  Documentation and validation cutover plan
P0.4  Workspace / local namespace cutover
P0.5  Focused docs extraction
P0.6  Legacy surface purge
P0.7  Remote / origin cutover
P0.8  Runtime / system design
```

P0 keeps runtime code out of the repository until the public identity,
documentation authority, validation posture, and implementation contract are
stable.

P0.5 extracts focused documents from the spine. P0.6 purges the old scaffold
surface. P0.7 cuts over the remote/origin identity to YVEX. P0.8 defines the
exact A0 runtime system design before any runtime code appears.

## A - Code-First Foundation

```text
A0    C codebase skeleton
A0.1  Core skeleton maturity / file header discipline / CLI command contract
```

A0 is the first runtime-code milestone. It follows
`docs/runtime-system-design.md` and creates headers, core status/error/log
implementation, a static library, a minimal CLI, and tests.

A0.1 hardens that skeleton before B0: every source-like file gets an explicit
file header, the CLI uses command metadata, Makefile exposes core and CLI smoke
targets, and `docs/source-style.md` defines the source discipline.

## Runtime Tracks

```text
B0 runtime filesystem
C0 artifact and GGUF
D0 tensor/model layer
E0 tokenizer and prompt
F0 graph and planner
G0 CPU backend
H0 engine/session runtime
I0 CLI/chat runtime
J0 metrics and tracing
K0 yvexd server/provider
L0 CUDA/DGX Spark backend
M0-M8 model support ladder
```

## Guardrails

```text
no fake inference claims
no fake provider support
no fake model support
no benchmark claims without run artifacts
no TUI
no terminal UI dependency
no terminal dashboard implementation
no panel implementation
```

YVEX support claims must be command-visible, tested, and documented.
