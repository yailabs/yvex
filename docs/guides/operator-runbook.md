<!-- docs:metadata
title: Operator Runbook
id: yvex.guides.operator-runbook
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Operator Runbook

**Start, observe and stop the host and its retained engines safely.**

[Up](README.md)

## First verified startup

First check whether this user already owns a ready server:

```sh
./yvex host status
```

If it reports `ready`, do not start another server; proceed to chat or runtime
inspection. If it refuses because no server is present, inspect one READY
logical model:

```sh
./yvex model list --wide
./yvex model show MODEL
```

Then start the host in the first terminal:

```sh
./yvex serve
```

Foreground operation is intentional: keep this terminal open. The host owns the
local socket, OpenAI listener, telemetry, and bounded engine manager. Its
terminal renders operational events but never reads chat or lifecycle commands.
Select and load the intended model from another terminal:

```sh
./yvex model load
./yvex model list --wide
./yvex host status
```

The chooser first presents launchable logical models and, only when needed,
their physically distinct variants. It shows format, precision, size, backend,
and mode but never asks for a profile alias. Scripts use `model load MODEL` and
add `--variant VARIANT` when the catalog reports several choices. The same
terminal can then enter chat with `yvex chat`, while other protocol clients and
OpenAI consumers remain independent.

The host publishes its socket before any engine exists. `model load` resolves
the selected registry profile, authenticates its package and binding, seals the
deployment specialization, builds admitted engine resources, then publishes
one loaded engine generation. Product state becomes LOADED; advanced `engine
list` exposes exact loading, loaded, draining, unloading, and failure facts as
applicable. `host status` continues to report the independent host lifecycle.

Large engines can spend substantial time in load. Typed events and bounded
status expose real completed stages without inventing a percentage. A failed
load releases its partial engine resources and leaves the host, socket, OpenAI
listener, other engines, and telemetry alive. Package context, parallel
capacity, prefill chunk, generation mode, and backend come from the admitted
startup profile, except that a load-only `--ctx` may select a bounded context
for the new text-engine generation. When compatible execution width exists, independent active
workers may rendezvous at the engine scheduler for real MoE or output-head
rows; same-session mutation remains serialized. This is compatible-operation
batching, not global ready-sequence continuous batching.


## What “load the model” means

The relevant commands have different responsibilities:

- `yvex model list` derives one logical catalog from exact source, artifact,
  deployment, and resident-engine facts;
- `yvex serve` starts the model-neutral persistent host;
- `yvex model load MODEL` resolves one launchable representation and exact
  profile, then publishes one engine generation;
- `yvex model unload MODEL` retires that generation only after dependent
  sessions and leases are released, without stopping the host;
- `yvex model active` projects loaded generations, exact activity, clients,
  sessions, leases, directional capabilities, and resource placement;
- advanced `yvex engine list|show|load|unload` retains exact profile and
  generation control for engineering and qualification;
- `yvex host memory` reports current process, mapped, host-resident, and
  device-resident memory facts;
- `yvex chat` uses the already resident model through the local protocol and
  never creates another model copy.

The host keeps the immutable artifact mapping as canonical backing. A compiled
artifact-backed placement registers that mapping once for CUDA addressability;
compiler-required derived layouts instead own their separately accounted managed
storage. `host memory` reports mapped, prepared, resident, sequence-state,
workspace, device, RSS, and capacity facts separately. Authentication and
selected resources complete before the engine becomes `loaded`; host readiness
does not depend on one engine.


## Foreground host and client terminal

The [startup procedure](#first-verified-startup) assigns host lifetime to the
foreground terminal and administration/chat to clients. The host renders its
boot report and operational stream, never a command prompt. Duplicate
`yvex serve` refuses rather than attaching or taking over listeners.

After a model is loaded, `yvex chat --session main` and `yvex host logs`
use the same typed runtime authority. Logs support `--verbose` and `--json`;
prompts and answers are excluded by default. Stop the shared host only when
its users have agreed to shutdown.


## OpenAI-compatible application provider

The same `yvex serve` process owns the application listener. After
`runtime.ready`, verify it without starting another process:

```sh
curl -fsS http://127.0.0.1:8001/health
```

Configure compatible applications with `base_url=http://127.0.0.1:8001/v1`
and a local non-secret API-key placeholder. One-line readiness and request
checks are:

```sh
curl -fsS http://127.0.0.1:8001/health
curl -fsS http://127.0.0.1:8001/v1/models
curl -fsS http://127.0.0.1:8001/v1/chat/completions -H 'Content-Type: application/json' -d '{"model":"deepseek4-v4-flash-dspark","messages":[{"role":"user","content":"Hello"}],"stream":false}'
```

The model identifier must match `GET /v1/models`; it is not a quantization
preset name. The adapter is loopback-only, opens no second model, owns no KV,
and executes no tools. Exact Chat Completions,
Responses, SSE, function-call, stop, JSON-object, error, and unsupported-field
semantics are in [`openai-compatibility.md`](../contracts/openai-compatibility.md).


## Session lifecycle

Named sessions retain their own transcript, committed token ledger, sampling
state, and typed persistent sequence/component state while sharing immutable
model resources. KV, recurrent, and convolution classes depend on the admitted
model; a session is not synonymous with a KV cache:

```sh
./yvex session new main
./yvex session list
./yvex session show main
./yvex session attach main
./yvex session detach main
./yvex session reset main
./yvex session state save main /var/lib/yvex/main-state.yvex
./yvex session state restore main /var/lib/yvex/main-state.yvex 1073741824
./yvex session close main
```

Client disconnect and detach do not close the engine. A partial or cancelled
turn can retain model-committed state and is never silently marked complete.
The current local protocol reports the exact engine generation, committed position,
token/text counts, state generations, failure class, and reset requirement.
Reset clears sequence/component state, tokens, transcript, decoder, and RNG policy without
closing the engine or host.

State checkpoints are immutable and restore only when their model, binding,
artifact, engine generation, scope, and committed position match the live
session. The restore byte bound is mandatory. This operation currently protects
model state inside one live semantic session on the same engine generation; it
is not yet a cross-restart conversation restore.


## Status, metrics, and logs

Use compact status for normal operation:

```sh
./yvex host status
./yvex host status --json
./yvex model list --wide
./yvex host memory
```

`model list` marks resident logical models LOADED without turning generations
into peer models. Advanced `engine list` reports exact generation,
specialization, backend, residency, and readiness facts. `host memory`
separates artifact/mapped/prepared model spans, device-addressable and explicit
device allocation, typed session state, arena/workspace/transient current and
peak, process RSS, placement, and whether physical residency was measured. On
UMA, addressable mapped weights are not reported as zero GPU use merely because
the explicit device allocator owns zero bytes; unknown page placement remains
`not measured`. The displayed classes can overlap and must not all be summed.

Follow typed server activity independently of the foreground host stream:

```sh
./yvex host logs
./yvex host logs --follow
./yvex host logs --verbose
./yvex host logs --json
```

The foreground server stream and `host logs` project each request as one
coherent compact unit. Normal rows use `REQUEST`, `PROMPT`, `PREFILL`, `FIRST`,
`DECODE`, `DONE`, `CANCELLED`, and `FAIL`. Named fields show generated tokens,
position, phase and elapsed seconds; `decode-avg` is cumulative subsequent
decode throughput and `rolling[count/window]` is recent throughput, both in
tok/s. Prefill shows actual token counts and available tok/s, not percentages;
consecutive intermediate updates are coalesced to a one-second cadence.
`RESOURCES` rows identify host-wide process RSS, workspace, session state and
request/queue counts, with explicit units. Long identifiers are shortened only
in this human projection.
They group speculative cycles, show queue pressure only when contended, and
finish with one stable terminal row. Native internal connection churn, duplicate load
lifecycle detail, token fragments, and profiler rows are suppressed. `host
status` remains a current snapshot; `host logs` contains
chronology only and never prepends status sections. Without `--follow`, the
command returns after a bounded recent retained event tail.
`host logs --follow` remains attached for live events. `host logs --verbose`
exposes each typed speculative cycle and supplied prefill update. `host logs --json` emits the
canonical complete JSONL event record, including typed detail omitted by the compact
human view. Prompts and answers remain absent from every projection by default.

Large engine loads use server-authored `LOAD` phases. `artifact-verification`
reports actual bytes and `residency` actual tensors when a denominator exists.
Other lifecycle phases retain their full names and show activity and elapsed
time; available rates use the corresponding work units, never token/s for bytes.

`HTTP` rows expose external OpenAI requests, including `GET /v1/models` before
any generation. They show a correlated `http-N` ID, endpoint template, observed
peer, and closure status/outcome/duration; generation adds the acquired session
ID. An SSE stream can fail after HTTP 200. Through an SSH tunnel, the peer is
the loopback forwarding socket, not the originating application's identity.
Prompts, credentials, request headers and arbitrary URLs are excluded; see the
[transport observation contract](../contracts/events-telemetry.md#external-http-access).

Raw server-event JSONL is selected at startup with `--logs json`. Increase
`--trace-level` from `summary` to `stages`, `tokens`, or `full` only when the
additional volume is required. Text content remains excluded unless the host is
started with the explicit `--trace-content` opt-in.


## Graceful shutdown

Release one engine while retaining the host and its other engines:

```sh
./yvex model unload MODEL
./yvex host status
```

Unload refuses while a live session or model lease still requires the engine.
Close dependent sessions and release leases explicitly; detach alone is not
session closure. Once admitted, retirement drains work and releases engine
resources while leaving the host ready. Host shutdown is a separate operation:

```sh
./yvex host stop
```

The host refuses new work, drains or cancels queued and active requests under
their typed state, closes sessions, closes the model exactly once, emits the
terminal shutdown event, and removes its socket and singleton lock.
