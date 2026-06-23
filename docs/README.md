# YVEX Docs

This directory is intentionally small. YVEX keeps a few dense canonical
documents instead of one Markdown file per idea.

Canonical documents:

- [Implementation spine](spine.md): the internal delivery map and technical
  planning document. It owns delivery sequencing, acceptance gates, decisions,
  runtime architecture, YVEX/YAI boundary, validation philosophy, and future
  tracks.
- [API](api.md): current public C API and future API contracts that are
  documentation only until headers, implementation, and tests exist.
- [Backend contract](backend-contract.md): CPU reference, backend ABI,
  device-tensor rules, CUDA/DGX Spark track, parity, and capability matrix.
- [Runtime filesystem](runtime-filesystem.md): runtime filesystem contract,
  path precedence, run directories, locks, and cache policy.
- [CLI interface spine](cli-interface-spine.md): canonical CLI layout,
  REPL/one-shot/server role boundaries, prompt/run/chat posture, line-editing
  boundary, and future console direction.
- [CLI runtime](cli-runtime.md): implemented CLI behavior, command table
  discipline, exit codes, stdout/stderr, TTY behavior, and future JSON/JSONL
  contracts.
- [CLI command index](cli-commands.md): quick current map of implemented
  `yvex` and `yvexd` commands, support posture, and visible non-goals.

Documentation rule:

```text
few Markdown files
dense canonical content
spine owns progress and technical doctrine
no new planning note for every micro-concept
```
