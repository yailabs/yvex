# YVEX Operator Runbook

Date: 2026-07-16
Status: implemented local control plane and repository operation boundary

## Purpose

YVEX Operator is a loopback-first engineering workbench for inspecting the
machine-readable YVEX boundary and using an explicitly separate reference-model
lane. It does not add native inference capability to the C/CUDA engine.

The v0.1.0 product target remains DeepSeek-V4-Flash from:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

The future canonical full target is `deepseek4-v4-flash`. Native model loading,
tokenization, generation, streaming, and cancellation remain unsupported. The
Operator reports those blockers and never redirects a native request to the
reference provider.

## Architecture

The web application lives in `apps/operator` and has four explicit boundaries:

1. The React client owns routing, URL-backed tabs and domain selection,
   capability-gated controls, the command palette, and the global chat dock.
2. The TypeScript adapter owns configuration, authentication, deterministic
   binary resolution, producer execution, jobs/events, sessions, and provider
   transport.
3. The producer compatibility layer exposes fixed IDs, fixed argv templates,
   bounded execution, typed JSON schemas, cache policy, and structured refusal.
4. The native YVEX and reference-provider lanes remain visibly separate. Only
   the reference-provider lane can generate text until the complete native
   capability chain is reported ready.

The browser cannot submit an executable, argv array, environment map, shell
fragment, filesystem traversal, or arbitrary provider URL for execution.

## Local Startup

Node.js 24 or newer and npm are required. Install and start from the repository
root:

```sh
cd apps/operator
npm ci
npm run dev
```

The development client listens on `http://127.0.0.1:4173` and proxies `/api` to
the adapter at `http://127.0.0.1:4317`. Both listeners are loopback-only.

For a production-local build:

```sh
cd apps/operator
npm run build
npm start
```

The production adapter serves the built client and API from
`http://127.0.0.1:4317`.

## Local Configuration

Non-secret settings are stored in:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/yvex/operator/config.json
```

Provider credentials are stored separately in `secrets.json`. The directory is
mode `0700`; both files are mode `0600`. The adapter returns only
`apiKeyConfigured: true` after a key is saved and never returns or logs the key.
Chat sessions are local adapter state. Dock width, layout mode, and other
presentation preferences remain in browser storage; provider secrets do not.

`YVEX_OPERATOR_CONFIG_DIR` selects another absolute local configuration
directory for tests or isolated deployments. Settings changes invalidate the
relevant binary, producer, provider, and capability observations.

## YVEX Binary Resolution

The adapter evaluates trusted candidates in this order and records a redacted
trace for every candidate:

1. the persisted Settings candidate;
2. `YVEX_BIN`;
3. `YVEX_OPERATOR_REPOSITORY_BIN`, or the repository-root `yvex` candidate;
4. known `build/yvex`, `build/bin/yvex`, and `bin/yvex` outputs;
5. controlled `PATH` entries named exactly `yvex`.

Selection requires an absolute regular executable, successful realpath
resolution, and the fixed machine-readable handshake:

```text
operator-contract --output json
```

The returned identity, protocol, and schema versions must validate. The web API
cannot alter the handshake or append argv. Missing, non-executable, malformed,
timed-out, and incompatible candidates remain inspectable and are not cached as
successes. Settings accepts only a server-validated absolute path and does not
disclose its directory to the browser.

Useful trusted startup variables are:

| Variable | Purpose |
| --- | --- |
| `YVEX_BIN` | Environment binary candidate after the persisted setting |
| `YVEX_OPERATOR_REPOSITORY_ROOT` | Repository root used for candidate labels and defaults |
| `YVEX_OPERATOR_REPOSITORY_BIN` | Explicit repository build candidate |
| `YVEX_MODELS_ROOT` | Server-owned model root passed only to allowlisted producers |
| `YVEX_OPERATOR_BINARY_TTL_MS` | Short binary observation TTL |
| `YVEX_OPERATOR_MAX_OUTPUT_BYTES` | Producer output ceiling |

## Reference Provider

Settings → Reference provider accepts an enabled state, display name, parsed
OpenAI-compatible base URL, optional API key, default model, and bounded timeout.
A local endpoint is first-class, for example:

```text
http://127.0.0.1:8080/v1
```

Use **Test provider** before opening a chat. The adapter checks model discovery
and a streamed chat-completion exchange. It owns all provider requests, streams
typed SSE events to the browser, preserves partial output on cancellation, and
records only bounded redacted event metadata. Remote provider hosts are refused
unless `YVEX_OPERATOR_ALLOW_REMOTE_PROVIDERS=true`; non-loopback provider URLs
must use HTTPS.

Every chat header and completed assistant message identifies the execution owner
as `Reference provider`, plus the configured provider and remote model. Token
usage is shown only when the provider supplies it; timings are derived from the
actual request and stream.

## Binding And Remote Security

The default adapter address is the literal loopback address `127.0.0.1`. Remote
binding is refused unless all of the following startup settings are present:

```text
YVEX_OPERATOR_HOST=<literal LAN or Tailscale address>
YVEX_OPERATOR_REMOTE_MODE=true
YVEX_OPERATOR_AUTH_TOKEN=<at least 24 characters>
YVEX_OPERATOR_ALLOWED_ORIGINS=https://operator.example.internal
```

Remote API requests require `Authorization: Bearer <token>` and an exact allowed
Origin. CORS is emitted only for that Origin. The token is never returned by the
API or written to logs. For browser use over a LAN or Tailscale, terminate TLS in
an authenticated reverse proxy, restrict it to the declared Origin, and inject
the bearer token server-side. Do not place the token in browser storage or a URL.
Remote mode is never inferred or enabled by Settings.

## Execution Lanes And Capability Truth

The capability manifest is server truth with stable IDs, dependency lists,
reason codes, recovery actions, provenance, timestamps, and schema version.
Adapter reachability is not backend or runtime readiness.

The native lane requires the complete reported chain: admitted artifact,
tokenizer, backend, runtime binding, model load, generation, streaming, and
cancellation. At this delivery boundary, structural and selected-proof engine
evidence may be visible through typed producers, but complete materialization,
runtime binding, model load, prefill, KV-backed decode, real logits, sampling,
tokenization, native generation, streaming, and cancellation are unavailable or
unsupported. CPU or CUDA is shown ready only when a stable YVEX producer reports
it. Host architecture is never CUDA evidence.

The reference-provider lane is operational independently and is never counted as
native capability.

## Validation

Run the complete Operator gate:

```sh
cd apps/operator
npm run validate
```

The gate verifies the lockfile, formatting, lint, three TypeScript projects,
unit/adapter/API/component tests, the production client/server build, Playwright
critical flows, and a secret/artifact worktree scan. Individual commands are:

```sh
npm run format
npm run lint
npm run typecheck
npm test
npm run test:browser
npm run build
npm run scan
```

Repository-wide C, documentation, topology, and artifact guardrails remain in
`runbooks/common.md`.

## Runbook Index

| Runbook | Current purpose | Capability boundary |
| --- | --- | --- |
| `runbooks/deepseek.md` | Exact v0.1.0 source boundary, metadata verification, and read-only exhaustive payload trust procedure | no transform, complete artifact, or runtime procedure |
| `runbooks/common.md` | Build, validation, documentation guards, artifact hygiene, and operator-local cleanup | validation does not create runtime capability |

Model-family architecture is defined in `model-families.md`. Release gates are
defined in `v010-release-doctrine.md`. Current project state, dependencies, and
Active Next are defined only in `../PROJECT.md`.

## Operator-Local State

The following remain outside git:

- model sources and emitted GGUF files;
- local registries and artifact identities;
- configuration secrets, sessions, reports, logs, pid files, caches, and partial
  downloads;
- generated backend outputs and build products.

Repository guardrails are listed in `runbooks/common.md` and
`MODEL_ARTIFACTS.md` at the repository root.
