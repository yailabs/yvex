<div align="center">
  <strong>CLORI</strong>
  <br />
  <strong>transparent neural execution engine for local model runtime inspection</strong>
  <br />
  <span>Artifacts, descriptors, tensors, memory, KV cache, decode surfaces, metrics, receipts, and benchmark reports.</span>

  <br />

  ![Status](https://img.shields.io/badge/status-skeleton-475569?style=flat&labelColor=1f2937)
  ![Runtime](https://img.shields.io/badge/runtime-planned-0f766e?style=flat&labelColor=1f2937)
  ![License](https://img.shields.io/badge/license-MIT-374151?style=flat&labelColor=1f2937)
</div>

# CLORI

CLORI is a Neural Execution Engine for inspecting, running, measuring and
serving local models with explicit runtime boundaries. It is designed to make
model execution visible: artifact metadata, model descriptors, tensor layout,
quantization, memory pressure, KV cache behavior, prefill/decode timing,
streaming, metrics and benchmark receipts.

CLORI is not YAI. CLORI does not own case authority. It can be useful on its
own for local model users, and it can later become a NET-compatible execution
node for YAI.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Status

CLORI is an early open-source-oriented repository skeleton. It is public for
technical review, roadmap alignment and community-facing development, and it is
not production-ready.

Inference is not implemented in this bootstrap state. Serving is not
implemented. NET integration is not implemented. Benchmark results are not
claimed until real runs exist.

## Why This Exists

Local model runtimes often hide the details that matter when a user is trying
to choose, inspect, run or compare models: what artifact was loaded, what
tensors exist, how quantization affects memory, what backend was used, what the
KV cache is doing, how prefill differs from decode, and what measurements came
from an actual run.

CLORI exists to make those runtime details explicit and inspectable.

The first practical target is local LLM/GGUF inspection, benchmarking and
execution. The architecture is not limited to small models: it is meant to
scale from small local models to larger models and future neural workloads.

## What CLORI Is

CLORI is a standalone Neural Execution Engine. Its planned surface centers on
model artifact inspection, normalized descriptors, tensor tables, quantization
profiles, memory estimates, backend boundaries, execution IR, KV cache
accounting, decode/sampler policy, streaming surfaces, metrics, receipts and
benchmark reports.

CLORI can be used without YAI. A local model user should eventually be able to
inspect artifacts, understand runtime pressure, run repeatable workloads and
compare behavior without adopting YAI's case/control runtime.

CLORI can also later integrate with YAI through NET-compatible node contracts.
In that posture, YAI controls authority, NET moves streams and CLORI executes
neural computation.

## What CLORI Is Not

CLORI is not:

- YAI
- a case authority system
- a policy engine
- a graph, memory, fact or journal authority
- a production-ready inference engine
- a model zoo
- a cloud platform
- a benchmark leaderboard
- an API-compatible serving layer in this bootstrap state
- a working YAI integration in this bootstrap state

Mentions of GGUF, llama.cpp, CUDA, Metal, MLX, API-compatible serving or YAI
integration describe roadmap boundaries only unless implemented and validated in
this repository.

## Operational Model

CLORI's planned runtime model is evidence-first:

```text
artifact -> descriptor -> tensor table -> memory/KV plan -> backend boundary -> prefill/decode -> metrics -> receipt -> benchmark report
```

- `artifact`: a model file or package to inspect.
- `descriptor`: normalized model metadata.
- `tensor table`: tensor names, shapes, dtypes, quantization, offsets and
  sizes.
- `memory/KV plan`: estimates and runtime accounting for weights, activation
  pressure, scratch space, backend reserve and KV cache behavior.
- `backend boundary`: adapter surface for future execution backends.
- `prefill/decode`: planned execution phases with separate timing and metrics.
- `metrics`: measured runtime evidence, not marketing claims.
- `receipt`: structured evidence for inspection, execution or benchmark runs.
- `benchmark report`: repeatable report material derived from real runs.

## Design Constraints

CLORI is built around a few constraints:

- Runtime behavior should be visible, not hidden behind a single text output.
- Benchmark reports must come from real runs, not assumed support.
- Model descriptors and tensor tables should be independent from model family.
- Backend boundaries should be explicit and replaceable.
- The architecture must not be limited to small local models.
- Future neural workloads may include embeddings, rerankers, classifiers,
  vision encoders and audio encoders.
- CLORI can become a NET-compatible node, but NET discovery, routing and trust
  authority remain outside CLORI.
- CLORI can be useful without YAI.

## Current Validation

Repository-level entrypoints:

```sh
make info
make check
```

The current checks validate repository skeleton files, boundary language and
the absence of fake inference claims. They do not validate model execution,
serving, backend support or benchmark results.

## Current Implementation Surface

This repository currently contains documentation and placeholders only:

- public README and license material
- CLORI spine and boundary documents
- YAI/NET compatibility reference documents
- benchmark canon placeholder
- source, test, example, protocol and benchmark placeholder directories
- doc-only `make info` and `make check` targets

There is no inference implementation in this bootstrap state.

## Repository Layout

```text
README.md    public project posture
LICENSE      MIT license
Makefile     doc-only info and check targets
docs/        architecture, boundary, terminology, benchmark and integration docs
src/         source placeholder
tests/       test placeholder
benches/     benchmark placeholder
examples/    example placeholder
protocols/   protocol placeholder
```

## Documentation

- [Architecture](docs/architecture.md)
- [Boundary](docs/boundary.md)
- [Terminology](docs/terminology.md)
- [Model descriptor](docs/model-descriptor.md)
- [Artifact format](docs/artifact-format.md)
- [Backend API](docs/backend-api.md)
- [Metrics](docs/metrics.md)
- [Serving](docs/serving.md)
- [Observability](docs/observability.md)
- [Benchmark canon](docs/benchmark/benchmark-canon.md)
- [YAI NET compatibility](docs/integration/yai-net-compatibility.md)
- [YAI receipt boundary](docs/integration/yai-receipt-boundary.md)
- [CLORI spine](docs/spines/clori-spine.md)
- [Copied YAI NET spine reference](docs/spines/yai-net-spine-reference.md)

## License And Contributions

CLORI is intended as an open-source, community-facing project. The current
repository license is [MIT](LICENSE).

Technical feedback, design review and focused contributions are welcome as the
runtime boundary becomes concrete. Early contributions should preserve the
core posture: no fake benchmark claims, no hidden authority transfer from YAI
to CLORI, and no production-readiness claims before implementation and
validation exist.

## Current Limitations

- Initial repository skeleton.
- Not production-ready.
- Inference is not implemented.
- Serving is not implemented.
- NET integration is not implemented.
- Local model execution is not implemented.
- GGUF inspection is a first practical target, not implemented support.
- CUDA, Metal, MLX, llama.cpp and OpenAI-compatible surfaces are not claimed as
  supported.
- Benchmark results are not claimed until real runs exist.
- YAI integration is planned through NET-compatible contracts, not working in
  this bootstrap state.
