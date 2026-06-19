# YVEX

YVEX is a C local inference engine for open-weight models.

YVEX owns model execution: artifact loading, format parsing, tensor tables,
tokenization, prompt rendering, graph planning, backend execution, KV cache,
prefill/decode, logits, sampling, token streaming, metrics and traces.

YVEX is CLI-only in this repository phase. The terminal surface is plain/rich
CLI output, REPL, streaming tokens, status lines, JSON/JSONL, logs on stderr,
and run artifacts on disk. It does not implement a TUI.

YVEX does not own YAI case/control/governance. YAI consumes YVEX as a local
provider boundary.

## Status

This repository is in the A0.2 documentation consolidation and code-quality
gate phase.

Current state:

```text
canonical spine: docs/spine.md
runtime code: A0.1 core skeleton implemented
public headers: version/status/error/log implemented
CLI binary: build/bin/yvex implemented for info/help/commands/version
GGUF parser: not implemented
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
- [Roadmap](docs/roadmap.md)
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
