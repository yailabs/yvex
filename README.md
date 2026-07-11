# YVEX

**YVEX is a native C inference engine for local open-weight models.** It starts
from the idea that running a model locally only becomes serious when the engine
**owns the path from source weights to generated text.** A **70B dense model**
already needs about **140 GB** just for FP16 weights; with **4-bit
quantization** that drops near the **35 GB range** before overhead, but that
saving only matters if the runtime can keep the rest of the execution under
control. **KV cache, backend buffers, dequantization cost, memory traffic, and
long-context state** can quickly eat the headroom that quantization just gave
back. On hardware like a **128 GB DGX Spark-class machine**, a **CUDA
workstation**, or future **Metal targets with unified memory**, the real
question is no longer “can the file load?” The real question is whether the
engine can turn compressed weights into a **stable, measurable, efficient run.**

**YVEX is built as a set of low-level primitives for managing local models with
deep control over execution.** C makes sense here because the engine has to deal
directly with ownership, lifetimes, byte ranges, alignment, backend buffers,
graph state, and cleanup. The first operational target is **CUDA on DGX
Spark-class hardware with 128 GB of RAM**; Metal comes next for Apple
unified-memory machines, while ROCm and Strix Halo stay tied to available
hardware. The project comes from a bigger idea around controlling the harnesses
around models, but the first layer sits lower: **rebuild the generation path
from weights to tokens until each step can be measured and changed.** The main
pressure comes from two model classes. **Dense models are the controlled
baseline** because every token walks through the whole parameter stack, which
makes numerical correctness, layout, kernels, and quantization easier to
isolate. **MoE models change the game:** DeepSeek-V3, for example, has **671B
total parameters** but activates about **37B per token**, so the engine has to
reason about total capacity, active compute, expert routing, resident memory,
and state movement as separate costs. **That is the concrete target for YVEX:**
make larger open-weight models manageable on personal hardware, find where
memory and compute are actually being spent, and turn that diagnosis into
faster, reproducible, configurable local inference.

Current implementation state, release gates, and the active engineering row
live in [`PROJECT.md`](PROJECT.md).

## The Engineering Thesis

Local inference is not one parser, one kernel, or one backend call. It is a
chain of ownership decisions. Source identity, tensor geometry, artifact
layout, resident memory, graph state, KV state, and generated tokens must remain
connected strongly enough that the engine can explain both a result and a
refusal.

C keeps that connection visible. File offsets become checked byte ranges;
qtype geometry becomes allocation size; allocations acquire explicit owners;
backend launches consume declared buffers; cleanup follows the same path in
reverse. The language is useful here because it does not hide the boundaries
where incorrect arithmetic or unclear lifetime would become unsafe execution.

This is also the basis for optimization. Performance work is meaningful only
when it can be tied to a specific artifact, memory placement, primitive,
runtime phase, and machine. YVEX builds that accounting into the engine before
trying to hide it behind a higher-level interface.

## From Source Weights to Text

YVEX treats local generation as one continuous dependency chain:

```text
source weights
  -> verified source inventory
  -> typed model architecture
  -> tensor role and layout mapping
  -> qtype policy and GGUF production
  -> structural integrity and admission
  -> materialization and backend residency
  -> executable runtime descriptor
  -> transformer prefill and KV state
  -> decode, output head, logits, and sampling
  -> detokenized text
```

Each transition turns one kind of evidence into facts owned by the next phase.
The artifact layer does not decide that a tensor is executable. Materialization
does not decide that a graph is complete. A backend primitive does not decide
that generation exists. Higher stages only become available when the lower
owners expose the exact state, failure behavior, and cleanup contract they need.

The chain is the engine architecture, not a progress checklist. `PROJECT.md`
records which parts are implemented, blocked, or unsupported without forcing
that changing state into this README.

## Design Principles

**Own the complete path.** Source, artifact, memory, graph, and text stages may
have separate modules, but no stage becomes an opaque handoff that the engine
cannot inspect.

**Fail closed.** Missing provenance, ambiguous geometry, unsupported qtypes,
invalid layouts, unavailable kernels, and incomplete runtime state are explicit
refusals. Success is never inferred from a report, filename, or fallback.

**Keep capability axes separate.** Recognizing storage does not imply
quantization, reference decoding, backend compute, materialization, graph
execution, or generation. Each capability has its own implementation and proof.

**Keep common infrastructure family-aware.** Source intake, GGUF, qtypes,
integrity, materialization, descriptors, and backend interfaces remain common.
Attention, position handling, KV geometry, MoE, tokenizer, and execution rules
enter through typed family boundaries rather than target-name conditionals.

**Compare before optimizing.** CPU references, bounded fixtures, corruption
cases, and backend comparisons establish numerical and lifecycle behavior.
Optimization follows measured disagreement or pressure, not assumption.

**Measure executable paths.** Benchmarks name the machine, backend, artifact,
qtype, prompt shape, context, command, and run conditions. Fixture timings and
planned runtime stages are not product performance.

## Models and Hardware Strategy

Dense models are the controlled baseline for tensor layout, numerical
correctness, quantization, and kernel work. Mixture-of-experts models add the
harder problem: total parameter capacity and active per-token compute diverge,
so routing, expert residency, bandwidth, and KV pressure must be modeled
independently.

The engine is multi-family by construction. Qwen and Gemma provide dense and
family-variation evidence for common infrastructure. DeepSeek provides the
release pressure for large MoE execution. A family profile or tensor map is an
engineering input, not a support claim; the exact release contract and family
states remain in `PROJECT.md`.

CPU is the reference lane. CUDA on DGX Spark-class hardware is the first
accelerated release path. Metal and ROCm are separate portability programs that
must establish the same allocation, primitive, graph, cleanup, and generation
boundaries on their own hardware. Backend availability never inherits runtime
support from another lane.

## Artifacts, Admission, and Local State

YVEX treats GGUF as an executable container contract, not a filename suffix.
Metadata, qtype geometry, tensor shapes, directory order, offsets, alignment,
padding, byte ranges, and file identity must agree before later phases may rely
on them.

| Term | Meaning |
| --- | --- |
| Tensor proof artifact | One tensor or a bounded subset used to prove a named parser, layout, materialization, primitive, or lifecycle property. |
| Complete model artifact | Every tensor and metadata item required to execute one exact model. |
| Supported model artifact | A complete artifact that passes integrity, materialization, runtime, generation, evaluation, benchmark, and release gates. |

Admission is deliberately narrower than model support. It proves only that the
checks owned by the current boundary passed and that the next implemented owner
may consume the resulting facts. Model completeness, numerical payload trust,
backend execution, and generation remain separate decisions.

Real weights, downloaded sources, generated model files, registries, reports,
logs, and build output stay operator-local. Git contains source, public headers,
documentation, tests, and tiny synthetic fixtures. The complete artifact and
registry contract lives in [`MODEL_ARTIFACTS.md`](MODEL_ARTIFACTS.md).

## Build and Validate

Build the native binaries and run the repository validation:

```sh
make
make smoke
make check
make check-docs
```

CUDA-capable hosts also run:

```sh
make check-cuda
```

The complete contributor validation and artifact guardrails are defined in
[`AGENTS.md`](AGENTS.md). Implemented operator workflows and refusal behavior
live in [`docs/operator-runbook.md`](docs/operator-runbook.md).

## Project Documentation

Each document has one durable responsibility. Project state is not repeated
across technical contracts.

| Document | Responsibility |
| --- | --- |
| [`PROJECT.md`](PROJECT.md) | Current engineering state, complete track and wave ledger, dependencies, release gates, and Active Next. |
| [`docs/reference-architecture.md`](docs/reference-architecture.md) | External papers, specifications, implementations, and their YVEX owners. |
| [`docs/system-target.md`](docs/system-target.md) | Filesystem topology and module ownership. |
| [`docs/model-families.md`](docs/model-families.md) | Family integration architecture and implemented family facts. |
| [`MODEL_ARTIFACTS.md`](MODEL_ARTIFACTS.md) | GGUF and artifact terminology, admission, identity, integrity, and lifecycle. |
| [`docs/api.md`](docs/api.md) | Public C API, ownership, lifetime, and error surfaces. |
| [`docs/contract.md`](docs/contract.md) | Implemented runtime, CLI, filesystem, failure, and claim contracts. |
| [`docs/operator-runbook.md`](docs/operator-runbook.md) | Current operator procedures, refusals, and recovery. |
| [`docs/v010-release-doctrine.md`](docs/v010-release-doctrine.md) | Release identity and gate-closure semantics. |
| [`AGENTS.md`](AGENTS.md) | Repository ownership, implementation, testing, and validation rules. |

## Engineering Lineage

YVEX studies GGUF, GGML, llama.cpp, vLLM, SGLang, DeepSeek reference work,
TensorRT-LLM, CUTLASS, and native CUDA tooling for proven formats, ownership
boundaries, model behavior, and kernel techniques. Those projects inform the
engineering; YVEX does not inherit their APIs, process models, runtime support,
or compatibility claims. The pinned reference map is maintained in
[`docs/reference-architecture.md`](docs/reference-architecture.md).

## License

YVEX is licensed under the MIT license.
