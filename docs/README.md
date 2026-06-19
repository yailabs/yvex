# YVEX Docs

This directory is the documentation root for YVEX.

Canonical current documents:

- [Implementation spine](spine.md): project authority, technical contract,
  roadmap, CLI-only doctrine, YAI boundary, validation matrix.
- [Roadmap](roadmap.md): reset, documentation, implementation, runtime, backend,
  and model-support tracks.
- [Validation](validation.md): current guardrails and future code-first
  validation posture.

Focused technical documents:

- [API](api.md): public C API rules, object families, status/error shape, and
  major runtime APIs.
- [Runtime filesystem](runtime-filesystem.md): config/cache/state paths, run
  directories, artifacts, receipt rule, and config precedence.
- [CLI runtime](cli-runtime.md): CLI-only doctrine, commands, REPL, exit codes,
  stdout/stderr, TTY behavior, and JSON/JSONL versioning.
- [CLI layout](cli-layout.md): rich output sections, chat layout, status lines,
  and JSON/JSONL envelopes.
- [Logging and tracing](logging-tracing.md): log domains, trace flags, trace
  event shape, and profile shape.
- [Metrics](metrics.md): load, tensor, memory, prefill, decode, sampling, KV,
  MoE, and server metrics.
- [Backend contract](backend-contract.md): backend kinds, device tensor
  ownership, sync policy, CPU reference, capability matrix, and parity.
- [CUDA-first](cuda-first.md): DGX Spark target, CUDA commands, stream policy,
  cuBLAS ownership, error mapping, and implementation phases.
- [Model ladder](model-ladder.md): M0-M6 progression, support statuses, and
  model page template.
- [YAI provider boundary](yai-provider-boundary.md): YAI/YVEX ownership split,
  provider flow, and receipt import rule.
- [Failure taxonomy](failure-taxonomy.md): failure classes, error shape, parser
  failures, and backend failures.
- [Delivery box template](delivery-box-template.md): standard wave template.

P0.6 removed the old scaffold surface from the active docs tree.
