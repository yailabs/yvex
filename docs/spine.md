# YVEX v0.1.0 Execution Spine

Date: 2026-07-10
Status: execution control
Authority: product outcome, v0.1.0 sequence, release gates, and claim boundary
Current proof stage: documentation/claim refoundation only

This document controls the shortest truthful path to the v0.1.0 product
outcome. It is not a delivery ledger, command inventory, proof catalogue, or
archive. Git history preserves superseded row detail.

`docs/repair/v010-foundation-closure.md` owns the temporary priority-blocking
repair sequence. `docs/system-target.md` owns filesystem placement and module
boundaries. `docs/v010-release-doctrine.md` owns release-gate semantics.

## Product Outcome

YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend
from a complete GGUF artifact produced by YVEX.

This is the only v0.1.0 product target. Additional model families, alternate
backends, server generation, and portability work are outside v0.1.0 unless a
later explicit scope row changes the release contract.

## Current Hard Truth

| Boundary | Current truth |
| --- | --- |
| Source | The canonical local path is present, but source identity and architecture verification have not closed. |
| Full target | `deepseek4-v4-flash` is the future canonical target and is unsupported. |
| Architecture | No execution-complete typed DeepSeek-V4-Flash architecture specification exists. |
| Mapping | No complete tensor role, layout, tokenizer, output-head, attention, position, KV, or MoE map exists. |
| Artifact | YVEX has not produced a complete DeepSeek-V4-Flash GGUF. |
| GGUF foundations | `V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened; their fixture-bounded evidence does not close the full artifact boundary. |
| Quantization | Required qtypes, conversion, and quantization are incomplete. |
| Materialization | Full model materialization and CUDA residency are unsupported. |
| Runtime descriptor | No execution-complete descriptor exists for the full target. |
| CUDA | A fallback PTX path can load no-op kernels; CUDA capability must become fail-closed before runtime work advances. |
| Transformer | Family-correct attention, position handling, KV, MoE routing, expert execution, and the complete stack are unsupported. |
| Text path | The real tokenizer-to-detokenized-text autoregressive chain is unsupported. |
| Evaluation | No v0.1.0 generation evaluation exists. |
| Benchmark | Not measured. |
| Release | Blocked. |

Existing bounded proof code remains implementation residue until an owning row
removes, replaces, retains it only as an internal test fixture, or absorbs it
into a lower-level proof. Its existence does not advance this spine.

## v0.1.0 Release Contract

Exact source target:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Future canonical full target:

```text
deepseek4-v4-flash
```

Release backend and machine:

```text
CUDA on DGX Spark
```

Real generation is this complete chain:

```text
prompt text
-> real tokenizer
-> full prefill
-> family-correct attention and MoE
-> real KV writes
-> output head and vocabulary logits
-> sampling
-> decode that reads the KV state
-> multiple autoregressive tokens
-> detokenized text
```

The chain must execute over the complete YVEX-produced GGUF for the exact
target. Every stage must have implementation, refusal behavior, cleanup,
tests, operator command proof, and claim-safe documentation.

CLI acceptance, printed fixture tokens, synthetic logits, bounded tensor
segments, report production, and bounded control loops do not satisfy this
contract.

The existing aliases remain temporary bounded proof surfaces:

```text
deepseek4-v4-flash-selected-embed
deepseek4-v4-flash-selected-embed-rmsnorm
```

They are not supported model targets, complete model artifacts, or stages of
the main critical path.

### Artifact Terminology

| Term | Canonical meaning |
| --- | --- |
| Tensor proof artifact | Contains one tensor or a bounded tensor subset and can prove only the named lower-level property. |
| Complete model artifact | Contains every tensor and metadata item required to execute the exact model. |
| Supported model artifact | A complete model artifact that passes integrity, materialization, runtime, generation, evaluation, benchmark, and release gates. |

The unqualified term "model artifact" does not name a selected-tensor proof
file.

## Active Blocking Work

The foundation repair spine is lateral to the product path and
priority-blocking. No main-path runtime row advances while it is active.

```text
V010.DOCS.REFOUNDATION.0: complete
proof stage: documentation/claim refoundation only
Active Next: V010.REBASE.DEEPSEEK.0
```

The ordered repair sequence, ownership, defects, gates, and decommission map
live only in `docs/repair/v010-foundation-closure.md`. After the repair sequence
closes, durable evidence is folded into this document and the temporary repair
detail is removed. Git history is the archive.

## Main Critical Path

This path starts only after the active foundation repair spine closes:

```text
verified DeepSeek source
-> typed architecture specification
-> complete tensor role and layout map
-> complete YVEX-produced GGUF
-> full materialization and residency
-> executable runtime descriptor
-> family-correct attention, position handling and KV
-> MoE routing and expert execution
-> complete transformer stack
-> tokenizer, output head, logits and sampling
-> autoregressive generation
-> evaluation and benchmark
-> v0.1.0 release
```

Dependencies are strict. Evidence from a later node cannot close an earlier
node, and external artifacts or runners cannot close any YVEX implementation
gate.

## Release Gates

| Gate | Required evidence | Current state |
| --- | --- | --- |
| Source | Exact local source identity, revision, config, tokenizer, shard inventory, and payload-readable tensor index | blocked |
| Architecture | Typed DeepSeek-V4-Flash specification covering attention, position rules, KV, MoE, output head, and tokenizer | blocked |
| Mapping | Complete source-to-runtime role map and GGUF name/layout map with no unresolved required role | blocked |
| Artifact | Complete YVEX-produced GGUF with required metadata, qtypes, global layout integrity, and writer-reader equivalence | blocked |
| Materialization | Every required tensor admitted, materialized, placed, and released under an explicit DGX Spark residency plan | unsupported |
| Descriptor | Artifact facts project to an execution-complete, family-correct runtime descriptor | unsupported |
| CUDA | Required operations are truthful, fail-closed, reference-compared, and free of advertised no-op fallbacks | unsupported |
| Transformer | Full prefill, attention, position handling, KV writes/reads, MoE routing, expert execution, layers, and final norm execute | unsupported |
| Text generation | Tokenizer, output head, vocabulary logits, sampling, autoregressive decode, stop policy, and detokenization compose | unsupported |
| Evaluation | Repeatable quality and regression cases run over the release path | blocked |
| Benchmark | Reproducible measurements record machine, artifact identity, qtype, prompt shape, run count, timing, and memory | not-measured |
| Release | Clean validation, artifact guardrail, claim audit, operator transcript, and versioned release record all pass | blocked |

A gate changes state only through its owning implementation row and executable
proof. Documentation cannot promote a gate.

## Version Sequence

| Version | Contract |
| --- | --- |
| v0.1.0 | DeepSeek-V4-Flash text generation from a complete YVEX-produced GGUF on DGX Spark CUDA. |
| v0.1.x | Correctness and operational hardening of the same supported path; no implicit family or backend expansion. |
| v0.2.0 | Additional model or backend scope only after an explicit target decision and complete gates for that scope. |
| Later | Serving, portability, distributed execution, and broader family work remain uncommitted until separately scoped. |

The version sequence expresses dependency and scope only. It does not claim
future implementation.

## Explicit Non-Claims

YVEX does not currently claim:

- a supported DeepSeek-V4-Flash target;
- a complete or supported DeepSeek-V4-Flash model artifact;
- complete GGUF artifact or qtype ABI closure;
- source payload conversion or quantization completion;
- full materialization or residency;
- an executable DeepSeek runtime descriptor;
- complete transformer execution;
- attention-backed KV prefill or decode;
- output-head vocabulary logits or model-backed sampling;
- autoregressive DeepSeek text generation;
- CUDA model generation;
- evaluation readiness;
- measured benchmark results;
- release readiness;
- support for another model family or backend in v0.1.0.

Proof artifacts, primitive comparisons, CLI grammar, reports, help text,
fixtures, external GGUF files, and external runner output remain non-closing
evidence.

## Documentation Ownership

| Document | Owner contract |
| --- | --- |
| `docs/spine.md` | Product outcome, current hard truth, main critical path, gates, version sequence, and non-claims. |
| `docs/repair/v010-foundation-closure.md` | Temporary priority-blocking foundation repairs and implementation decommission map. |
| `docs/v010-release-doctrine.md` | Exact release meaning and gate semantics. |
| `docs/system-target.md` | Filesystem architecture and module ownership. |
| `docs/model-families.md` | Normative family-integration architecture, not release progress. |
| `docs/contract.md` | Runtime and ownership contracts for implemented surfaces. |
| `docs/api.md` | Public C API facts and lifetime boundaries. |
| `docs/runbooks/deepseek.md` | Short current-state operator boundary for the exact target. |

Historical row databases, copied spines, compatibility roadmaps, and
documentation archives are not active documentation owners.
