<!-- docs:metadata
title: Command and operation architecture
id: yvex.reference.commands
document: reference
status: current
owner: docs
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Command and operation architecture

**Current procedure or lookup, linked to its technical owners.**

[Up](README.md)

YVEX is one inference/compiler/runtime product with four distinct process
roles: finite offline work, one persistent hosted runtime, native local
clients, and external compatibility clients. Command spelling never changes a
process role according to TTY state, loaded engines, or host presence.

## Domain objects

| Object | Authority |
| --- | --- |
| Model | logical semantic identity shared by related representations |
| Source | acquired payload, exact revision, provenance, and verification state |
| Artifact | immutable compiled physical package |
| Profile | durable deployment configuration naming an artifact, backend, and admitted runtime policy |
| Engine | process-local loaded engine generation owned by the host |
| Session | mutable conversation/generation state bound to one exact engine generation |
| Host | foreground process owning engines, sessions, scheduling, execution, listeners, and logs |

These names are not aliases. In particular, a profile is not a loaded engine.
They are exact plumbing authorities, but they are not peer concepts in the
ordinary product workflow. The porcelain `model` projection joins them only by
authenticated lineage and presents one logical model with its source and
representation variants. Similar display names never create lineage.

## Product grammar and plumbing

```text
yvex
yvex help [COMMAND...]
yvex help --advanced
yvex help --json
yvex version [--json]

yvex serve
yvex chat [--model MODEL]

yvex host status|logs|stop
yvex inspect ...

yvex model search|pull|prepare|load|unload|list|show|active|push
yvex model status|stop
```

The memorable lifecycle is:

```text
search -> pull -> prepare -> load -> chat
                    push -> outward distribution
```

`pull` and `push` move or reference representations. `load` and `unload`
change runtime residency. Those meanings never overlap. Exact engineering
plumbing remains available through `help --advanced`:

```text
yvex source ...
yvex artifact ...
yvex profile ...
yvex engine ...
yvex session ...
yvex compile ...
yvex bench ...
```

Provider account plumbing is registry-addressable as `source accounts
providers|status|whoami|login|logout|ensure`. These are distinct operations,
not opaque arguments passed through one generic command row. Human tables and
redacted JSON share the same account observations; credential ownership stays
with the installed provider CLI.

Bare `yvex` prints the compact command map and exits successfully. It never
starts, attaches to, or probes a host and never enters chat. Interactive human
generation requires `yvex chat`. Programmatic generation uses the private
typed protocol through native clients or the admitted loopback provider API;
there is no public one-shot generation command.

The old `run`, `server`, `execute`, `profile`-as-a-verb, and implementation
top-level namespaces are not aliases. Obvious retired spellings fail with a
bounded migration hint and never dispatch their former operation.

## Process roles

### Foreground host

`yvex serve` always attempts to become the persistent foreground host. It owns
the private Unix socket, engine manager, scheduler, sessions, backend
execution, telemetry, and optional loopback OpenAI listener. A one-time TTY
boot report is followed by an ordinary line-oriented host log stream. The
process does not read an administrative command language from stdin, own the
alternate screen, clear scrollback, or become a client.

If a healthy host already owns the socket, `yvex serve` refuses and identifies
the socket and inspection command. It never attaches to that host. Native
administration is performed from another process with `host`, `engine`, and
`session` commands.

### Native chat client

`yvex chat` is the sole public REPL. It requires a TTY and connects to the
private local protocol. It opens no artifacts, initializes no CUDA state,
starts no host, and loads no engine. A missing host produces the explicit
`yvex serve` remediation; a missing engine points to `yvex model load`.

The linear editor is the externally linked REPLAI ABI 1 implementation. It owns
grapheme editing, history navigation, paste, resize/redraw and exact terminal
restoration. YVEX supplies prompt values, history admission, registry completion
and interrupt meaning. During generation the editor is closed; YVEX owns stream
meaning and protocol cancellation while REPLAI owns quiet feedback and terminal
restoration. See [the dependency decision](../decisions/0007-external-terminal-editor.md)
for the exact pin, build requirements and lifecycle evidence. Registry-derived
slash operations remain limited to conversation and session use:

```text
/help /status /context
/new /sessions /session /use /detach /reset /cancel /close
/attach /attachments /attachments-clear
/think-low /think /think-max /nothink
/quit
```

Reasoning policy persists for the attached session until changed. The client
does not describe it as a next-turn-only setting. Load, unload, compile,
acquire, and host-stop operations are deliberately absent from the REPL.
`/attach PATH` seals one local media object for the next turn; repeated calls
stage multiple ordered parts. `/attachments` lists that bounded stage and
`/attachments-clear` discards it. Submission appends the typed text part,
clears the stage after server acceptance, and retains the same session identity
for later multipart turns. `/use NAME` selects an existing session; attachment
staging never creates or replaces one.

### Hosted model use and exact administration

`host` addresses the already-running process. Porcelain `model load` accepts a
logical model selector and resolves only its proven launchable
representations. With no model on a TTY it renders a small linear model
selector; if that model has several valid deployments it renders a second
variant selector. Numeric rows are temporary conveniences. Before the native
request, the CLI has resolved one exact profile identity; the host then creates
an immutable process-local engine generation. `model load MODEL --ctx N` and
advanced `engine load PROFILE --ctx N` request a load-only text context through
the typed local protocol; common runtime capacity admission checks the compiled
model limit and available resources. Neither command resizes a loaded engine.
Non-TTY ambiguity fails and
requires `MODEL` and `--variant` rather than guessing.
Each launchable profile also carries the same directional input/output
capability summary later published by its engine, so an orchestrator can select
a READY deployment before demand activation without inferring from its name.

`model unload MODEL` resolves the resident exact generation and drains it
without stopping the host. Advanced `engine load PROFILE` and `engine unload
ENGINE` remain deterministic plumbing for qualification and exact lifecycle
inspection, but ordinary users never need a profile alias. Session commands
address server-owned mutable state and cross the same private protocol. None of
these clients opens a package directly.

`model active` projects loaded engine generations from the same typed engine
catalog used by `engine list`: backend, execution mode, active/idle state,
attached sessions/clients, model leases, directional capabilities, and H12
resource/placement facts. Its JSON schema is `yvex.model.active.v1`; consumers
never parse the human presentation.

### Offline work

`model search` discovers without downloading. `model pull` parses one
deterministic source locator, pins remote provider identity before acquisition,
and delegates to source ownership. Local pull explicitly chooses a managed copy
or verified external reference. `model prepare` delegates verification,
compilation, artifact emission, and deployment creation to their existing
owners; it does not manufacture support when a family lacks a full-package
binding. `model push` exports one exact chosen representation and never means
runtime loading.

Source commands inspect exact provenance, artifact commands inspect immutable
compiled packages, and profile commands inspect deployment configuration.
Specialized compiler phases remain discoverable below `compile`. Bounded
component execution and measurement live under `bench`; read-only engineering
evidence lives under `inspect`. Offline commands neither require nor start a
host.

Remote `model pull` is terminal-attached but terminal-independent. The command
starts or reopens one source-owned supervised acquisition and renders its typed
state; it does not parent the provider for the duration of the transfer. The
detached supervisor is a narrow acquisition worker, not the hosted runtime, a
generic daemon or a job scheduler. It owns one immutable provider/repository/
revision/selection generation and exits at a terminal acquisition state.

`model status` and the advanced `source status` reopen the durable operation.
`model stop`/`source stop` authenticate the supervisor or provider process
identity before signaling, and resume creates a new operation generation for
the same immutable target. Legacy v1 foreground acquisitions remain observable
and explicitly controllable but are never silently adopted by the supervisor.

All acquisition presentation consumes `yvex.model.acquisition.status.v2`:

- a color-capable TTY receives an in-place, width-bounded state surface;
- redirected/plain/log output is append-only and emitted on meaningful state
  or progress changes;
- JSON contains typed lifecycle, health, nullable progress and compatibility
  facts with no ANSI or canonical presentation strings.

Rendering does not inspect `/proc`, provider logs or cache directories. The
source/provider adapter supplies process activity and stable structured events;
the source operation owns reconciliation and durable truth; CLI code only
parses intent, attaches, controls and renders. Provider human output remains an
audit stream. Completion of acquisition still does not replace source
verification, compilation, artifact admission or model support.

## Protocol planes

Native commands and chat use private local protocol v24 over a UID-owned Unix
socket. That protocol carries YVEX engine generations, sessions, KV identity,
lifecycle, ordered typed content/provenance, model leases and directional
capabilities, typed progress, cancellation, resource facts, and telemetry.

External compatibility consumers use the separate loopback HTTP OpenAI
profile. The adapter owns no model, engine, session, KV, scheduler, or backend
state and never parses CLI output. Native clients do not route through HTTP.

## Canonical operation authority

`config/operator/registry.json` is the strict `yvex.operator.registry.v1`
operation authority. The build
validates it and emits immutable descriptors plus a content identity. Those
descriptors drive parsing, dispatch, human help, JSON discovery, shell
completion, and the chat slash catalog. Domain owners retain capability,
semantic validation, and typed result authority; the registry does not create
support by naming an operation.

Default help projects only the product map. `help --advanced` exposes admitted
advanced and engineering leaves grouped by registry-authored domain and parent
command, with concise child verbs instead of repeated full command paths.
`help --json` is a stable structured
projection with exact operation and command identities. Human and machine
renderers consume the same typed result and machine output contains no ANSI.
Registry-generated shell completion applies the same projection at an empty
top-level position: only `chat`, `help`, `host`, `inspect`, `model`, `serve`,
and `version` are suggested. Advanced roots remain executable and regain their
full registry-driven subcommand completion after the user explicitly types the
root.

## Output and errors

Product results go to stdout and product errors to stderr. Human views respect
TTY detection and `NO_COLOR`; redirected and structured output is
non-decorative. Submission-time errors surface immediately. Host/backend
failures are returned through the correct typed completion boundary rather
than being inferred from log prose.

The foreground host is the logging producer. `yvex host logs` projects its
typed stream from another process; `--json` selects the machine JSONL
projection and `--follow` makes continuous intent explicit. No second logging
authority or administrative REPL exists.

## Non-claims

This local product architecture does not establish public HTTP serving,
authentication, TLS, remote security, full OpenAI compatibility, distributed
serving, model quality, benchmark authority, or release qualification.

## Human presentation and machine inventory

The common human grammar is identity → aligned responsive fields → blockers and
secondary facts. Normal model lists emphasize state, execution, format, size and
available representations. `show`, audit and JSON retain deep lineage. `--wide`
explicitly adds useful metadata; terminal width does not silently opt into it.
Engine entries retain their detailed facts in responsive records. Session fleets
and benchmark comparisons use lightweight tables, with structural record fallback
at narrow widths. Important fields are not elided.
Color supplements content; NO_COLOR, dumb terminals and redirected output retain
the same facts. Human layout is not a stable machine schema.

Static positional cardinality/types/actions and flag descriptions, types, enum
values, conflicts, requirements and repeatability are registry-owned and shared
by parsing, leaf help, discovery and shell completion. Compound action-dependent
requirements remain in the domain parser; an action listed by grammar is not a
capability claim. Leaf help exposes argument requirements and admitted enum
spellings rather than an opaque `[arguments ...]` pack. Domain admission stays
outside that registry. `--json` selects
the command's existing structured contract; `--output` exists only where a real
normal/table/audit choice is supported. `--audit` and `--wide` retain their command
meaning, not an invented universal envelope.

Acquisition lifecycle paths are canonically `source status|stop|resume|cleanup`.
The existing nested `source acquire status|stop|resume|cleanup` spellings remain
registry-authored compatibility aliases, not parser-only subcommands.

`source acquire --json` emits one terminal `yvex.model.pull.v1` result on stdout;
provider output and progress remain on stderr. The detached supervisor publishes
a bounded structured result before its terminal operation state. An attaching
client consumes that result, never reconstructing it from provider/human logs.
An unavailable terminal result fails closed, and starting a replacement supervisor
retires the previous result rather than reusing stale publication.

`version --json` emits `yvex.version.v1`: product version, observed private local
protocol version, registry identity, build commit/tree/state and build identity.
It is a build-fact projection, not a public remote compatibility negotiation.
`inspect cuda bandwidth` is the engineering diagnostic `system.cuda.bandwidth`;
it measures bounded allocation/transfers and does not mutate resident engines.

Discovery includes API-only dispositions for HTTP health/catalog/chat preflight,
the finite-decision producer, and the existing read-only remote management facts.
This inventory neither creates CLI paths nor promotes runtime maturity. Finite
decisions are not chat. Remote mutation remains deferred. QA and programmatic
one-shot generation remain deliberately absent from ordinary product commands.
