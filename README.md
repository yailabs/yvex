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

This repository has completed C0, the artifact and GGUF header/probe base.

Current state:

```text
canonical spine: docs/spine.md
runtime code: C0 core/filesystem/artifact skeleton implemented
public headers: version/status/error/log/fs/artifact/gguf implemented
CLI binary: build/bin/yvex implemented for info/help/commands/version/paths/inspect
GGUF parser: header/probe only
tokenizer: not implemented
CUDA backend: not implemented
server: not implemented
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

## Validation

```sh
make info
make check
```

At this phase, validation checks the reduced documentation posture and
guardrails, builds libyvex.a, builds the CLI, runs C tests, and runs CLI smoke
tests.

## License

YVEX is licensed under the MIT license.
