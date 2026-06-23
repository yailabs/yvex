# YVEX

YVEX is a C local runtime for open-weight model artifacts. It owns local
artifact loading, GGUF parsing, tensor inspection, backend materialization, and
runtime diagnostics.

YVEX is CLI-first. `make` builds the repository-local binaries directly:

```sh
make
./yvex version
./yvex commands
```

`./yvex` is the operator/developer CLI. `./yvexd` is the server/provider daemon.

## Local Models

Model artifacts live outside the repository. The local registry lets operators
use aliases instead of long paths.

```sh
./yvex models add --path "$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
./yvex models list
./yvex models use deepseek4-v4-flash-selected-embed
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

The current external artifact cards live in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md).

## Runtime Posture

Implemented now:

```text
GGUF inspection
metadata and tensor table parsing
selected-tensor GGUF emission
selected-tensor materialization on CPU/CUDA
local model registry
model alias resolution for one-shot commands
model gate and materialization gate diagnostics
server/provider status shell
```

Not implemented yet:

```text
full-model execution
prefill
decode
sampling
generation
OpenAI-compatible generation
inference benchmarks
```

## Source Layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA implementation
gguf/               GGUF parser and artifact tooling
models/             model-family adapters
include/yvex/       public C headers
tests/              tests, fixtures, and vectors
docs/               api, contract, internal spine
```

Generated local state is ignored:

```text
build/
.yvex/
./yvex
./yvexd
```

## Documentation

```text
AGENTS.md            operating rules for humans and coding agents
MODEL_ARTIFACTS.md   external artifact cards
docs/api.md          public C API surface
docs/contract.md     runtime/backend/filesystem/CLI contract
docs/spine.md        internal delivery roadmap
```

## Validation

```sh
make check
make smoke
make check-cuda
```

`make check-cuda` requires a CUDA-capable host. No support claim exists without
source implementation, tests, command-visible proof, and documented limits.

## License

YVEX is licensed under the MIT license.
