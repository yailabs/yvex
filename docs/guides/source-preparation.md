<!-- docs:metadata
title: Acquire and Prepare Models
id: yvex.guides.source-preparation
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Acquire and Prepare Models

**Move from discovery through authenticated source to an admitted representation.**

[Up](README.md)

## Discover, acquire, and prepare a model

The [model lifecycle guide](model-lifecycle.md) owns the complete acquisition,
external-tool interoperability, cache, preparation, and safe-removal workflow.
The [storage contract](../contracts/model-storage.md) owns its invariants.

Search remote or local catalogs without downloading payloads:

```sh
./yvex model search "MiniMax H3"
./yvex model search "Qwen" --author Qwen --page 1 --limit 20
./yvex model search "local-name" --provider local
```

Remote availability and YVEX support are separate columns. Search never pulls,
prepares, or loads anything. Acquire one representation with a deterministic
locator:

```sh
./yvex model pull hf://OWNER/REPOSITORY
./yvex model pull hf://OWNER/REPOSITORY@IMMUTABLE_REVISION --format safetensors
./yvex model pull hf://OWNER/GGUF_REPOSITORY --format gguf --variant VARIANT
./yvex model pull hf://OWNER/REPOSITORY --format safetensors --dry-run
```

If the locator omits `@REVISION`, YVEX resolves the current provider reference
to an immutable identity before acquisition and reports the repository,
revision, chosen representation, and bytes. Multiple representation classes or
variants produce a line-oriented TTY selector. Non-TTY callers must pass
`--format` and, when still ambiguous, `--variant`; no provider file glob is
required in the normal workflow. Credentials come from the existing provider
account owner and tokens are never printed.

For remote providers, `--dry-run` is the metadata-only acquisition check: it resolves the
immutable provider revision and representation inventory, but downloads no
payload and creates no YVEX source, catalog, receipt, or transfer-log state.
The human default is a bounded repository/revision/representation summary;
add `--verbose` to inspect the provider's complete planned file inventory.

Local files and directories use the same distribution verb:

```sh
./yvex model pull /mnt/models/MODEL --managed
./yvex model pull file:///mnt/models/model.gguf --managed
./yvex model pull /mnt/external/MODEL --reference
```

`--managed` creates and verifies a durable YVEX-owned copy. `--reference`
records a verified external dependency without copying tens of gigabytes; if
the path disappears the model becomes BLOCKED. In a TTY, omitting both asks
which storage policy to use. Automation must choose explicitly. Unsupported
locators such as `ssh://` fail as unavailable transports and are never passed
to `scp` or a shell.

Choose the model and, optionally, a root with `--models-root PATH`; YVEX owns
the internal directories created by `pull` and `prepare`. The
[storage layout and responsibility split](../contracts/model-storage.md#what-users-choose-and-yvex-manages)
explains `source/`, `representations/`, records, evidence, caches and temporary
state, including the difference between new managed output and retained
historical directories. You do not need to reproduce those subdirectories to
import a model. An optional `inbox/` is an explicit intake path, not a watched
folder: run `model pull` on its contents. First intake establishes full content
identity, including during a dry-run. Later operations reuse current verification
receipts; changed file snapshots invalidate that reuse. See the
[idempotency and disk-cost rules](model-lifecycle.md#repeated-commands-interruptions-and-disk-cost)
for the exact behavior.

Prepare the acquired model through the high-level owner:

```sh
./yvex model prepare MODEL
./yvex model prepare MODEL --quant CANONICAL_PRESET
./yvex model pull hf://OWNER/REPOSITORY --format safetensors --prepare
```

Preparation delegates verification, family recognition, mapping, quantization,
materialization, package validation, and deployment creation to their canonical
owners. It succeeds only when that complete family/representation binding is
implemented. A canonical quantization preset is validated exactly; missing
full-package emission fails rather than silently using another qtype. An
already admitted GGUF is verified and bound without forced requantization.
`model pull --prepare` combines distribution and preparation but never loads
the host. Authenticated streamed preparation is not currently admitted, so
`--stream` refuses and does not claim that bytes avoided the machine.
Machine callers run `model pull --json` and `model prepare --json` as separate
operations; combining `--prepare` and `--json` is rejected so stdout remains
one valid JSON document with one operation schema.

Long acquisitions expose the existing status, stop, and resume lifecycle:

```sh
./yvex model status MODEL
./yvex model stop MODEL
./yvex model pull hf://OWNER/REPOSITORY --resume
```

Once READY, start the host and load the logical model:

```sh
./yvex serve
./yvex model load
./yvex model list --wide
./yvex host status
```

The TTY chooser shows model names and physical facts, never profile aliases.
Automation uses `model load MODEL` and supplies `--variant` only if several
launchable representations remain. The protocol request still carries one
exact profile and creates one exact generation. Detailed source, artifact,
profile, and engine commands remain available through `help --advanced` for
compiler work and qualification.

For a 32k text-context test, use `./yvex model load MODEL --ctx 32768` on a
host running the matching local protocol. `./yvex engine list --json` then
reports the actual admitted `context_capacity`. This does **not** resize a
loaded engine or silently unload it: close sessions and explicitly unload the
old generation first, then request the new one. A 32k request can still refuse
because of the compiled model maximum or available host/device resources.

Before admitting a GB10 performance result, inspect the compiled CUDA image and
run the bounded bandwidth fixture:

```sh
./yvex inspect cuda
./yvex inspect cuda bandwidth
```

The first command reports whether the admitted bundle is native, its exact architecture and its
content identity. The default build selects native code only when local hardware detection is
unambiguous and supported by `nvcc`; otherwise it remains an explicitly reported portable-PTX build.

The second command performs five timed samples over one 32 MiB working set and
reports CUDA streaming traffic, asynchronous D2D copy and coherent host access
separately. Its evidence identity binds every elapsed sample and the admitted
kernel bundle. The result is a machine-state observation; it is not the 273 GB/s
hardware specification and is not a benchmark claim when another workload is
active.


## Verifying source payloads for compilation

Source acquisition and source compilation are separate operations. After an
exact snapshot and its v1 acquisition manifest exist outside the repository,
publish the payload-trusted v3 manifest before constructing Transformation IR:

```sh
./yvex source verify \
  --source /srv/yvex/sources/DeepSeek-V4-Flash-DSpark \
  --models-root /srv/yvex \
  --source-manifest /srv/yvex/manifests/deepseek-v4-flash-dspark-source.json
```

This finite offline command first rechecks revision, sidecars, index and every
Safetensors header. It then reads every admitted shard, compares its SHA-256
with the pinned provider metadata and transactionally publishes the aggregate
payload identity. It refuses when authoritative shard digests are unavailable;
the product command does not turn a local-only seal into upstream evidence. A
current upstream-verified v3 manifest reopens without rereading 167 GB of
payload. The manifest is mutable provenance and should remain outside the
source snapshot whose bytes it identifies.

This command neither maps tensors nor emits an artifact. The subsequent
compilation stages consume the published payload identity and retained source
inventory through their typed owners.


## Probing a physical-variant candidate

Before paying the storage and admission cost of a complete candidate GGUF, run
one decision from its sealed physical plan against the real source payload:

```sh
./yvex compile quant probe \
  --target deepseek4-v4-flash-dspark \
  --source /srv/yvex/sources/DeepSeek-V4-Flash-DSpark \
  --models-root /srv/yvex \
  --source-manifest /srv/yvex/manifests/deepseek-v4-flash-dspark-source.json \
  --policy /srv/yvex/plans/candidate-policy.json \
  --imatrix-manifest /srv/yvex/calibration/deepseek-v4-flash.imatrix \
  --backend cuda \
  --plan /srv/yvex/plans/candidate.plan \
  --tensor blk.21.ffn_down_exps.weight
```

The command executes exactly the selected terminal from the identity-bound
plan and reports its encoded bytes and reconstruction metrics. It does not
publish an artifact, alter the registry, or claim whole-model quality. Use it
as the role-level funnel before promoting a surviving policy to complete
artifact emission.


## Registering an existing model

The ordinary selector consumes a complete local registry profile. When an
artifact and binding already exist but no profile was recorded, import them
once with the advanced registry operation and absolute paths:

```sh
./yvex profile create \
  --alias my-deepseek-dspark-profile \
  --family deepseek4 \
  --model v4-flash-dspark \
  --scope runtime \
  --class iq2xxs \
  --path /srv/yvex/models/deepseek-v4-flash-dspark-bootstrap-q2-v1.gguf \
  --runtime-binding /srv/yvex/models/deepseek-v4-flash-dspark.yvex-runtime-binding \
  --target deepseek4-v4-flash-dspark \
  --backend cuda \
  --execution-strategy speculative \
  --ctx 4096 \
  --support-level selected-tensor-materialized
```

This operation reads the GGUF, records its identity and metadata, checks that
the startup profile is structurally complete, and stores it in the user-local
registry. It does not establish runtime admission; `yvex model load`
authenticates the artifact and binding again when it opens the engine. Normal
subsequent use contains no paths or environment variables:

`--support-level` records only the artifact inspection/materialization stage.
The binding, target, backend, mode, and context fields separately own startup
profile readiness; the registry profile and the model live in the server remain
separate facts.

```sh
./yvex model show v4-flash-dspark
./yvex serve
./yvex model load v4-flash-dspark
```

Run `model load` from another terminal. If several launchable representations
exist, the TTY selector shows their physical facts; automation supplies
`--variant`. The foreground host continues to own only server lifetime and its
event stream. Advanced `profile list` reads exact deployment entries and
`engine list` reads exact resident generations. Loading and unloading does not
require restarting the host.

Generation mode is part of the startup profile. `dspark` requires a binding
that contains target, draft, and target-verification plans; `target-only` is
the explicit reference/debug mode. A DSpark startup refusal is not permission
to fall back silently. Select a compatible profile or repair the
artifact/binding.
