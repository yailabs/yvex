# YVEX

YVEX is a C local inference engine for open-weight models.

YVEX owns model execution: artifact loading, format parsing, tensor tables,
tokenization, prompt rendering, graph planning, backend execution, KV cache,
prefill/decode, logits, sampling, token streaming, metrics and traces.

YVEX is CLI-only. The repository-local developer entrypoints are `./yvex` and
`./yvexd`, thin launchers that dispatch to compiled binaries under
`build/bin/`: plain/rich CLI output, REPL-ready command semantics,
streaming-friendly stdout/stderr discipline, JSON/JSONL planning, logs on
stderr, and run artifacts on disk.

YVEX does not own YAI case/control/governance. YAI consumes YVEX as a local
provider boundary.

## Operator Entry Points

Repository-local:

```sh
./yvex
./yvexd
```

`./yvex` is the operator/developer CLI.
`./yvexd` is the server/provider shell.

## CLI Interface

YVEX exposes:

```text
./yvex   interactive CLI / one-shot diagnostics
./yvexd  server/provider daemon
```

The canonical CLI layout and REPL design live in:

```text
docs/cli-interface-spine.md
```

## Quick Operator Files

- [AGENTS.md](AGENTS.md) - operating contract for humans and coding agents.
- [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) - current external model artifact
  cards and support posture.
- [docs/spine.md](docs/spine.md) - canonical delivery spine and next authorized
  milestone.

The current active live target is DeepSeek V4 Flash selected embedding GGUF.
See [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md).

## Status

This repository has completed OWI closeout, M1 model gates, M2 DeepSeek
materialization hardening, artifact naming, and repository launcher packaging.

Current state:

```text
canonical spine: docs/spine.md
runtime code: core/filesystem/artifact/GGUF/model/tokenizer/graph/backend/weights/session shell implemented
public headers: implemented headers are aggregated by include/yvex/yvex.h
CLI binary: build/bin/yvex accepted-only runtime shell and cuda-info implemented
server binary: build/bin/yvexd status shell implemented
repo launchers: ./yvex and ./yvexd dispatch to build/bin/
GGUF parser: metadata and tensor directory implemented
tokenizer: fixture encode/decode implemented
CUDA backend: tensor allocation/read/write/copy and F32 embed parity implemented when driver/device are available
weights: fixture tensor bytes materialized into CPU/CUDA backend tensors
server generation: not implemented
benchmark results: none
```

No support claim exists without source implementation, CLI-visible behavior,
tests, manual proof command, clear failure behavior, and a documented
limitation.

## Documentation

- [Agent operating contract](AGENTS.md)
- [Model artifact cards](MODEL_ARTIFACTS.md)
- [Docs index](docs/README.md)
- [Implementation spine](docs/spine.md)
- [API](docs/api.md)
- [Backend contract](docs/backend-contract.md)
- [Runtime filesystem](docs/runtime-filesystem.md)
- [CLI interface spine](docs/cli-interface-spine.md)
- [CLI runtime](docs/cli-runtime.md)
- [CLI commands](docs/cli-commands.md)

## Validation

```sh
make info
make check
make check-cuda   # only on CUDA-capable hosts
```

## Quick Start

```sh
make
./yvex commands
./yvex info
./yvex inspect tests/fixtures/gguf/valid-tokenizer-simple.gguf
```

Optional local-user command links:

```sh
mkdir -p ~/.local/bin
ln -sf "$PWD/yvex" ~/.local/bin/yvex
ln -sf "$PWD/yvexd" ~/.local/bin/yvexd
```

`./yvex` is the repository-local developer entrypoint. `yvex` is the optional
global/local-user command when the launcher is linked into `PATH`.

At this phase, validation checks the reduced documentation posture and
guardrails, builds libyvex.a, builds the CLI, runs C tests, and runs CLI smoke
tests.

## License

YVEX is licensed under the MIT license.
