<!-- docs:metadata
title: Interfaces and Protocols
id: yvex.architecture.interfaces-protocols
document: architecture-plane
status: mixed
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: interfaces-protocols
related: [yvex.architecture.runtime-lifecycle, yvex.architecture.advanced-generation]
-->

# Interfaces and Protocols

**Clients project typed system behavior without acquiring execution ownership.**

[Up](README.md)

One `yvex` executable exposes the operator product. The C surface, private local
protocol, OpenAI compatibility adapter and restricted remote-management bootstrap
have different trust and compatibility boundaries.

## Integration map

| Consumer | Supported seam | Boundary |
| --- | --- | --- |
| Local operator | CLI and generated operation registry | Human output is not a machine API |
| Embedded integrator | [C API](../contracts/c-api.md) | Installed vs internal ABI tiers remain explicit |
| Same-user client | [Local protocol](../contracts/local-protocol.md) | Exact version and generation checks |
| Application provider | [OpenAI adapter](../contracts/openai-compatibility.md) | Bounded compatibility, not universal API parity |
| Remote operator | [Remote management](../contracts/remote-management.md) | Read-only enrolled identity/status bootstrap |

## YAI boundary

YAI owns Case meaning, memory, workflow, authority and effects. YVEX owns source,
model, compiled representation and computational execution. A YAI consumer may
use a qualified public seam; it must not infer producer capability from private
structs, a model name or a research target. The finite local producer does not
establish a YAI ABI, and future W → E state ingress remains unimplemented.

## Provider progress

The OpenAI transport forwards real session prefill/committed-decode observations.
Its bounded frame timeout measures inactivity. Independent connection admission
keeps discovery responsive and refuses saturation; external total deadlines
remain external. Telemetry-ring retention cannot erase a direct progress fact.
This correction does not qualify long-request latency or complete YAI workloads.
## Interactive terminal path

<!-- docs:diagram interactive_boundary -->
```mermaid
%% yvex-figure: interactive_boundary
%%{init: {"themeVariables": {"background": "transparent"}}}%%
flowchart TB
  n_terminal["EXTERNAL<br/>Terminal / user<br/>input and displayed results"]:::external
  subgraph n_panel_0["a  REPLAI inside chat"]
    direction TB
  n_editor["EXTERNAL<br/>REPLAI · C ABI 1<br/>editing / cursor / paste"]:::external
  end
  subgraph n_panel_1["b  Client"]
    direction TB
  n_client["INTERFACE<br/>YVEX client adapter<br/>grammar / history admission"]:::interface
  n_protocol["INTERFACE<br/>Typed local protocol<br/>bounded UDS requests"]:::interface
  n_render["INTERFACE<br/>YVEX rendering<br/>semantic result formatting"]:::interface
  end
  subgraph n_panel_2["c  Persistent yvex serve"]
    direction TB
  n_runtime["RUNTIME<br/>Hosted execution<br/>engine admission / routing"]:::runtime
  n_events["EVIDENCE<br/>Typed results / events<br/>committed channels / progress"]:::evidence
  end
  n_terminal --> n_editor
  n_editor --> n_client
  n_client -. request .-> n_editor
  n_client --> n_protocol
  n_protocol -. request .-> n_runtime
  n_runtime --> n_events
  n_events --> n_render
  n_client --> n_render
  n_render --> n_terminal
  n_panel_0 ~~~ n_panel_1 ~~~ n_panel_2
  classDef semantic fill:#efe5fc,stroke:#7541ba,color:#261b38
  classDef physical fill:#f4effb,stroke:#8054b2,color:#261b38
  classDef runtime fill:#eeeafb,stroke:#6a4ca3,color:#261b38
  classDef mutable fill:#fff3db,stroke:#8e6920,color:#261b38
  classDef interface fill:#edf3fb,stroke:#456789,color:#261b38
  classDef external fill:#f2f2f4,stroke:#707078,color:#261b38
  classDef evidence fill:#eaf5ef,stroke:#3d7255,color:#261b38
  style n_panel_0 fill:transparent,stroke:#b8a5d0
  style n_panel_1 fill:transparent,stroke:#b8a5d0
  style n_panel_2 fill:transparent,stroke:#b8a5d0
```

[Static figure](../assets/diagrams/interactive_boundary.svg) · [Editable source](../assets/diagrams/interactive_boundary.json)
<!-- /docs:diagram -->

*Figure 5 — Interactive ownership. Submission returns UTF-8 after editor
restoration; the YVEX adapter constructs typed content and request semantics.
Panels (a) and (b) live in the same chat process: REPLAI is a statically linked
dependency, not another process or model runtime.
The figure shows the continuing path, not a concurrent editor during generation.*
[Editable source](../assets/diagrams/interactive_boundary.json).

[ADR 0007](../decisions/0007-external-terminal-editor.md) owns the editor split
and [`config/replai.json`](../../config/replai.json) owns its exact pin. YVEX
retains prompt values, history admission, slash-completion decisions, reconnect,
attachments and semantic rendering. Ctrl-C while editing is a generic REPLAI
event; during generation it enters YVEX cancellation and quiet-output handling.
EOF, exit and transport failure follow their distinct close/recovery paths.
Historical `repl_` helper names do not establish another editor.

The private `src/cli/io/terminal/` contract exposes terminal observation,
opaque output/capture scopes and interrupt counts, not descriptors, signals or
console layouts. The current POSIX implementation adapts the editor entrypoint,
captures interrupts, and joins its watch before application context expires.
The client callback decides which generation to cancel; terminal capture never
owns engine/session meaning. Output-state admission and restoration failures
stop chat instead of continuing with uncertain terminal state.

This is an interface portability boundary, not cross-platform qualification.
Linux PTY tests exercise the implementation and the pinned REPLAI ABI, which
now has Linux/macOS POSIX qualification. YVEX itself remains Linux-qualified;
a macOS or Windows port must qualify its
platform adapter, editor dependency and local transport; neither a POSIX signal
nor a Windows console event is a generic request type.

[Real chat PTY tests](../../tests/repl_pty.sh), their
[consumer assertions](../../tests/replai_consumer.py), and the
[tiny runtime vertical](../../tests/integration/tiny_vertical.sh) qualify the
composition. The classical REPL decomposition is explained in the
[external guide](https://github.com/mothx9/replai/blob/master/docs/repl.md);
its default branch does not change the pinned dependency.


## Implementation and evidence

[src/server](../../src/server) · [src/client](../../src/cli/io/client.c) · [include/yvex/server.h](../../include/yvex/server.h) · [config/operator/registry.json](../../config/operator/registry.json)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Engine and Session Runtime](runtime-lifecycle.md) · [Speculation and Finite Execution](advanced-generation.md)

## Product processes

`yvex` has three mechanically separated lanes:

- the runtime-client lane uses the private local protocol for chat,
  runtime administration, sessions, live model inspection,
  cancellation, and the single human/JSON log surface;
- the foreground `serve` lane owns the persistent multi-engine host and its
  independently loaded engine generations;
- the finite offline-engine lane calls admitted library owners for compilation,
  artifact operations, inspection, direct component execution, profiling, and
  system facts.

The runtime-client lane cannot open an artifact, initialize CUDA, execute a
Transformer, or host a model. The offline lane closes all resources before the
process exits and never owns persistent sessions. The server lane is explicit
in the invocation and never shells out to or executes a hidden binary.

The foreground server lane owns one private Unix listener, one loopback
OpenAI-compatible listener, one telemetry authority, and a bounded set of
engine generations. Each loaded engine owns its immutable runtime model,
scheduler, sessions, state, and resources. HTTP and native clients bind work to
an exact alias and generation rather than a process-global model pointer.

The [restricted remote-management bootstrap](../contracts/remote-management.md)
is a separate, optionally supervised OpenSSH listener on any supported host.
Its forced `yvex management protocol` process is finite and can answer exact
device identity and local host status while the inference host is stopped.
It authenticates enrolled client keys through OpenSSH and reads host facts
through the existing private local protocol; it neither exposes that socket
remotely nor creates a second host lifecycle. Mutating remote control and a
production deployment qualification are not yet implemented.
