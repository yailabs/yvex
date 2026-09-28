<!-- docs:metadata
title: Build YVEX
id: yvex.guides.build
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Build YVEX

**Prepare the native toolchain and build the supported local product.**

[Up](README.md)

## Prerequisites

Builds provide one executable product. `yvex serve` owns the private Unix
listener and bounded loopback OpenAI-compatible listener in the foreground.
`yvex model load` resolves a launchable logical model to an exact deployment
and asks that host to create an engine generation. Several generations may
coexist within the admitted host bound. Other `yvex` modes own native clients
and finite offline engineering operations.

Loading a model requires one complete registry startup profile. A text runtime
binds one admitted GGUF to its exact runtime binding, target, backend, and
default context capacity. A text load may request a different bounded context
with `--ctx N`; the server checks the compiled model limit and current resource
envelope before creating a new engine generation. A composite runtime instead
binds an installed component root
to its target, backend, and capability mode without inventing a singular
artifact or text-runtime binding. Inspect the product catalog first:

```sh
./yvex model list --wide
./yvex model show MODEL
```

The table contains one row per proven logical model and exposes its selected
format, quantization or precision, representation size, state, backend,
location, and number of alternatives. `model show` expands the exact lineage.
Only READY models can cross the engine boundary. If none is ready, complete the
pull and preparation path or the one-time advanced
[registration procedure](source-preparation.md#registering-an-existing-model). Backend selection is
part of the resolved deployment and never falls back silently.

Provider credentials remain owned by the installed provider CLI; YVEX records
only redacted observations and never persists a raw token. Discover the exact
implemented account operations and inspect current state with:

```sh
./yvex help source accounts
./yvex source accounts providers --output table
./yvex source accounts status --output table
./yvex source accounts whoami huggingface --output table
```

Authentication and removal remain explicit:

```sh
./yvex source accounts login huggingface
./yvex source accounts logout huggingface
```

The Hugging Face integration delegates credentials to the installed `hf`
client; the GitHub integration delegates them to `gh`. `--json` provides the
complete redacted machine projection. Account commands never print a token.
