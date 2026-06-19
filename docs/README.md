# YVEX Docs

This directory is intentionally small. YVEX keeps a few dense canonical
documents instead of one Markdown file per idea.

Canonical documents:

- [Implementation spine](spine.md): technical authority for identity, scope,
  runtime architecture, YVEX/YAI boundary, model ladder, failure taxonomy,
  validation philosophy, guardrails, and future tracks.
- [Roadmap](roadmap.md): working progress document, milestone state, delivery
  sequence, gates, decisions, and code-quality review.
- [API](api.md): current public C API and future API contracts that are
  documentation only until headers, implementation, and tests exist.
- [Backend contract](backend-contract.md): CPU reference, backend ABI,
  device-tensor rules, CUDA/DGX Spark track, parity, and capability matrix.
- [Runtime filesystem](runtime-filesystem.md): future B0 filesystem contract,
  path precedence, run directories, locks, and cache policy.
- [CLI runtime](cli-runtime.md): implemented CLI behavior, command table
  discipline, exit codes, stdout/stderr, TTY behavior, and future JSON/JSONL
  contracts.

Documentation rule:

```text
few Markdown files
dense canonical content
roadmap owns progress
spine owns technical doctrine
no new planning note for every micro-concept
```
