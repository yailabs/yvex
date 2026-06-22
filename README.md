# YVEX

YVEX is a C local inference engine for open-weight models.

YVEX owns model execution: artifact loading, format parsing, tensor tables,
tokenization, prompt rendering, graph planning, backend execution, KV cache,
prefill/decode, logits, sampling, token streaming, metrics and traces.

YVEX is CLI-only. The current user-facing executable surface is
`build/bin/yvex`: plain/rich CLI output, REPL-ready command semantics,
streaming-friendly stdout/stderr discipline, JSON/JSONL planning, logs on
stderr, and run artifacts on disk.

YVEX does not own YAI case/control/governance. YAI consumes YVEX as a local
provider boundary.

## Status

This repository has completed M0, fixture weight materialization into backend
tensors.

Current state:

```text
canonical spine: docs/spine.md
runtime code: core/filesystem/artifact/GGUF/model/tokenizer/graph/backend/weights/session shell implemented
public headers: implemented headers are aggregated by include/yvex/yvex.h
CLI binary: build/bin/yvex accepted-only runtime shell and cuda-info implemented
server binary: build/bin/yvexd status shell implemented
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

- [Docs index](docs/README.md)
- [Implementation spine](docs/spine.md)
- [API](docs/api.md)
- [Backend contract](docs/backend-contract.md)
- [Runtime filesystem](docs/runtime-filesystem.md)
- [CLI runtime](docs/cli-runtime.md)
- [CLI commands](docs/cli-commands.md)

## Validation

```sh
make info
make check
make check-cuda   # only on CUDA-capable hosts
```

At this phase, validation checks the reduced documentation posture and
guardrails, builds libyvex.a, builds the CLI, runs C tests, and runs CLI smoke
tests.

## License

YVEX is licensed under the MIT license.
