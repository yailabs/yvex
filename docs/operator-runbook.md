# YVEX Operator Runbook

Date: 2026-07-16
Status: implemented local engineering control plane; YVEX generation remains capability-gated

## Purpose

YVEX Operator is the graphical workbench for the YVEX build and execution
chain. Its primary context is one active YVEX target and the evidence that
moves that target through:

```text
source
  -> logical model
  -> Transformation IR
  -> physical lowering
  -> quantization plan
  -> GGUF writer
  -> artifact admission
  -> backend binding
  -> runtime session
  -> generation
  -> execution evidence
```

It is not a multi-provider chat frontend. YVEX is the sole primary execution
authority. Missing engine capabilities remain explicit and are never replaced
by generated fixture data or an external endpoint.

The optional OpenAI-compatible endpoint is an **external comparison endpoint**.
It exists only for explicit differential diagnostics under Settings. Its state,
jobs, sessions, usage, and output do not enter YVEX workspace, health, runtime,
generation, or execution-evidence truth.

## Architecture

The application lives in `apps/operator` and has four boundaries:

1. The React client owns lifecycle routing, URL-backed stage/tab state,
   presentation preferences, the command palette, inspectors, and the YVEX
   Generation Console.
2. The TypeScript backend-for-frontend owns secure configuration, the
   authoritative workspace, deterministic binary resolution, typed capability
   normalization, producer execution, jobs/events, and optional comparison
   transport.
3. The producer compatibility layer maps stable producer IDs to fixed argv,
   schema, timeout, output ceiling, cache policy, and structured refusal. The
   browser cannot submit an executable, argv, shell fragment, pipe, redirection,
   environment map, or producer path.
4. The YVEX engine remains the owner of build, artifact, backend, runtime, and
   generation facts. The Operator reports unsupported owners; it does not
   simulate them.

`GET /api/v1/workspace` is the single selection authority. It contains the
active target, build, artifact, backend, runtime session, active jobs,
capability snapshot, and recent evidence. The shell, Workspace, Build,
Artifacts, Runtime, and Generation Console all derive from this response.

## Information Architecture

Primary routes are:

| Route | Owner |
| --- | --- |
| `/workspace` | active target navigator, complete lifecycle, stage inspector, jobs/events/evidence |
| `/build?stage=...` | Source, Architecture, Transformation IR, Lowering, Quantization, GGUF Writer |
| `/artifacts` | typed proof/complete/supported inventory, explicit unclassified rows, and workspace selection |
| `/runtime?tab=...` | readiness, backend, sessions, generation, evaluation, benchmark |
| `/evidence` | producer registry, safe runs, cache, jobs, and missing contracts |
| `/environment` | browser/adapter/YVEX topology, binary trace, host and exposure |
| `/settings` | connection, YVEX, cache, safety, interface, optional comparison endpoint |

The global Generation Console inherits the active target, artifact, backend,
and runtime session. It has no provider selector and sends no request until a
real YVEX generation endpoint and every required capability are ready.

The hidden diagnostics route `/settings/reference-comparison` becomes useful
only after Settings explicitly enables and tests an external comparison
endpoint. There is no silent fallback between these surfaces.

## Local Startup

Node.js 24 or newer and npm are required:

```sh
cd apps/operator
npm ci
npm run dev
```

The development client listens on `http://127.0.0.1:4173` and proxies `/api` to
the adapter at `http://127.0.0.1:4317`. Both are loopback-only.

For the production-local application, one process serves the client and API:

```sh
cd apps/operator
npm run build
npm start
```

Open `http://127.0.0.1:4317`.

## Local Configuration

Non-secret settings are stored in:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/yvex/operator/config.json
```

The same private directory contains `workspace.json`, bounded comparison
session state, and `secrets.json`. The directory is mode `0700`; private files
are mode `0600`. Comparison credentials are write-only through the browser: the
API returns only whether a key is configured. No secret belongs in browser
storage, logs, screenshots, or reports.

`YVEX_OPERATOR_CONFIG_DIR` selects another absolute configuration directory for
isolated tests or deployments. Workspace selection is server state. Console
width/mode and other presentation preferences are local browser state.

## YVEX Binary Resolution

The adapter evaluates trusted candidates in this order:

1. persisted Settings override;
2. `YVEX_BIN`;
3. repository-configured candidate;
4. known local build candidates;
5. a controlled `PATH` lookup for the exact name `yvex`.

Each candidate records a redacted label, file/executable checks, identity probe,
version/protocol compatibility, and rejection reason. Selection requires an
absolute regular executable, successful realpath resolution, and the fixed
machine-readable handshake:

```text
operator-contract --output json
```

The browser cannot alter this command or append arguments. A missing explicit
override is normal when an auto-discovered binary is active; Environment labels
these states separately as **Explicit binary override** and **Active binary**.

## Workspace And Producer API

The central read/write resources are:

```text
GET  /api/v1/workspace
GET  /api/v1/targets
POST /api/v1/workspace/target
GET  /api/v1/workspace/artifacts
POST /api/v1/workspace/artifact
GET  /api/v1/build
GET  /api/v1/build/stages
GET  /api/v1/capabilities
GET  /api/v1/producers
POST /api/v1/producers/:producerId/run
GET  /api/v1/producer-runs
GET  /api/v1/jobs
GET  /api/v1/events
```

Selection requests contain only validated catalog IDs. Producer run requests
contain only a registry-owned producer ID. A missing machine-readable producer
returns `unsupported`, `blocked`, or `unavailable` with its reason and owner; it
does not trigger human-output parsing.

## External Comparison Diagnostics

Settings -> Comparison endpoint accepts an explicit enabled state, display
name, parsed OpenAI-compatible base URL, optional API key, comparison model, and
bounded timeout. A local example is:

```text
http://127.0.0.1:8080/v1
```

Disabled is the normal default and creates no YVEX blocker. **Save and test
comparison** checks model discovery and a streamed chat-completion exchange.
The isolated API namespace is:

```text
/api/v1/comparison/reference/*
```

The adapter owns all endpoint requests, streams typed SSE events, preserves
partial output after cancellation, and records bounded redacted metadata.
Remote comparison hosts are refused unless
`YVEX_OPERATOR_ALLOW_REMOTE_PROVIDERS=true`; non-loopback URLs must use HTTPS.

Every response in this diagnostics surface identifies its execution owner as
`External comparison endpoint`. It is never described as YVEX execution.

## Binding And Remote Security

The default adapter address is `127.0.0.1`. Non-loopback binding fails closed
unless all startup settings are present:

```text
YVEX_OPERATOR_HOST=<literal LAN or Tailscale address>
YVEX_OPERATOR_REMOTE_MODE=true
YVEX_OPERATOR_AUTH_TOKEN=<at least 24 characters>
YVEX_OPERATOR_ALLOWED_ORIGINS=https://operator.example.internal
```

Remote API requests require the bearer token and an exact allowed Origin. CORS
is emitted only for that Origin. The token is never returned or logged. For LAN
or Tailscale browser access, use an authenticated TLS reverse proxy that owns
token injection; do not place the token in browser storage or a URL. Settings
cannot silently change listener exposure.

## Current Capability Truth

The capability manifest uses stable IDs, dependencies, statuses, reason codes,
recovery actions, provenance, and timestamps. Adapter reachability is not
backend or runtime readiness. Host architecture is not CUDA evidence.

The current machine-readable release decision reports the DeepSeek-V4-Flash
target and build evidence available from audited producers. Complete artifact
admission, execution-capable backend selection, full-model materialization,
runtime binding, model load, model-backed prefill/KV/decode/logits/sampling,
tokenizer load, YVEX generation, streaming, cancellation, evaluation, and
benchmark remain at their exact reported unsupported, blocked, unavailable, or
not-measured state. The Generation Console renders that dependency chain and
does not synthesize progress or output.

## Validation

Run the complete Operator gate:

```sh
cd apps/operator
npm run validate
```

This verifies the dependency lock, formatting, lint, three TypeScript projects,
unit/adapter/API/component tests with at most two workers, production client and
server build, single-worker Playwright journeys at reduced priority, and the
secret/artifact scan. Manual screenshots are captured after a production build:

```sh
npm run screenshots
```

Repository-wide C, documentation, topology, and artifact guardrails remain in
`docs/runbooks/common.md`. Operator validation does not promote any C/CUDA
capability.

## Operator-Local State

Never commit model sources, emitted artifacts, local registries, configuration,
credentials, session data, logs, caches, browser output, pid files, or generated
build products. Only tiny test fixtures and intentional acceptance screenshots
belong in the repository.
