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

This repository is in pre-implementation cutover.

Current state:

```text
canonical spine: docs/spine.md
runtime code: not implemented
public headers: not implemented
CLI binary: not implemented
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

- [Implementation spine](docs/spine.md)
- [Roadmap](docs/roadmap.md)
- [Validation](docs/validation.md)
- [Docs index](docs/README.md)

## Validation

```sh
make info
make check
```

At this phase, validation checks the canonical documentation posture and
guardrails. Code compilation begins in A0.

## License

YVEX is licensed under the MIT license.
