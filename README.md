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

## Model Selection

Current one-shot model commands accept explicit model paths or registered model
aliases.

The active DeepSeek selected artifact is documented in
[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md). A local model registry is implemented
through `./yvex models` so users can register and select models by alias instead
of remembering absolute paths. Alias-or-path resolution is implemented for
one-shot model commands such as `inspect`, `tensors`, `metadata`,
`materialize`, `model-gate`, and `materialize-gate`.

Example alias:

```text
deepseek4-v4-flash-selected-embed
```

Typical registry flow:

```sh
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex models list
./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

## Quick Operator Files

- [AGENTS.md](AGENTS.md) - operating contract for humans and coding agents.
- [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) - current external model artifact
  cards and support posture.
- [docs/spine.md](docs/spine.md) - internal delivery map.

The current active live target is DeepSeek V4 Flash selected embedding GGUF.
See [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md).

## Status

YVEX currently supports:

```text
GGUF inspection
metadata and tensor table parsing
selected-tensor GGUF emission
selected-tensor materialization on CPU/CUDA
local model artifact registry
model alias resolution for one-shot commands
model gate and materialization gate diagnostics
server/provider status shell
```

YVEX does not yet support:

```text
full-model execution
prefill
decode
sampling
generation
OpenAI-compatible generation
inference benchmarks
```

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
