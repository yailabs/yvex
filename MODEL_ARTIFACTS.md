# YVEX Model Artifacts

Date: 2026-07-11
Status: artifact policy

Complete and supported model artifacts are external operator assets. They are
never committed to this repository. The repository may contain only tiny GGUF
fixtures under `tests/`.

## Terminology

| Term | Meaning |
| --- | --- |
| Tensor proof artifact | A file containing one tensor or a bounded tensor subset, used only to prove a named parser, range, materialization, primitive, or reference property. |
| Complete model artifact | A file containing every tensor and metadata item required to execute one exact model. |
| Supported model artifact | A complete model artifact that passes integrity, materialization, runtime, generation, evaluation, benchmark, and release gates. |

The unqualified term "model artifact" does not refer to a selected-tensor proof
file. A structurally valid GGUF is not necessarily complete or supported.

Tensor byte accounting consumes the canonical row-aware GGUF qtype owner.
Block-quantized shapes require `ne[0]` to divide exactly by the block width;
flattened element-count alignment and partial-row padding are not admitted.
Known storage geometry does not imply a decoder, quantizer, emitter, compute
kernel, complete artifact, or supported artifact.

## v0.1.0 Target

Exact source:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Future canonical full target:

```text
deepseek4-v4-flash
```

Required artifact outcome:

```text
a complete GGUF for DeepSeek-V4-Flash produced by YVEX
```

No such complete model artifact currently exists. The target, artifact,
materialization, runtime, generation, evaluation, benchmark, and release states
remain unsupported or blocked as recorded in `PROJECT.md`.

## Native Structural Reader

The canonical native reader parses GGUF v3 container, metadata, and tensor
directory records through bounded positioned reads. Its immutable parsed view
owns decoded strings, arrays, tensor facts, and lookup indexes. Structural
inspection neither maps nor copies the complete artifact and reads zero tensor
payload bytes.

Parser budgets bound hostile counts and declared strings or arrays while
admitting the known DeepSeek-scale directory. Duplicate identifiers, malformed
records, refused qtypes, arithmetic failures, and short reads produce typed
non-success results. Normal, table, audit, and integrity projections consume
that operational result; building a report cannot convert a rejection into an
accepted artifact.

Addressable tensor range facts are not global layout admission. Tensor order,
padding, overlap, truncation across the complete data section, and aggregate
layout integrity remain blocked on `V010.GGUF.LAYOUT.INTEGRITY.1`.

## Admission Contract

A complete model artifact must record or prove:

- exact source and target identity;
- architecture and tokenizer metadata;
- every required global, layer, attention, position, KV, MoE, expert, norm, and
  output-head tensor role;
- exact qtype, shape, layout, offset, alignment, range, and byte accounting;
- deterministic writer output and writer-reader equivalence;
- artifact identity and corruption refusal;
- materialization and runtime descriptor compatibility.

A supported model artifact additionally requires the runtime, generation,
evaluation, benchmark, and release gates in `docs/v010-release-doctrine.md`.

## Existing Proof Files

Legacy selected DeepSeek files and aliases may still exist outside the
repository while their owning decommission rows are pending. They are tensor
proof artifacts only. Their digest, range, materialization, or primitive
evidence must be named by the specific proof and must not be promoted to model
support.

The canonical decommission obligations and consuming milestones are recorded in
`PROJECT.md`. This policy does not retain an artifact card or historical
validation catalogue for those files.

## Repository Guardrail

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected result:

- no tracked safetensors, bin, or dat model payloads;
- tracked GGUF files are tiny test fixtures only;
- no local registries, reports, source downloads, emitted artifacts, logs, or
  generated backend outputs are tracked.

## Non-Claims

This policy does not claim complete GGUF emission, qtype compute or
quantization coverage, writer completion, roundtrip completion, full materialization,
runtime execution, DeepSeek generation, CUDA generation, evaluation evidence,
benchmark measurement, or release readiness.
