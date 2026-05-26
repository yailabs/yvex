# CLORI

CLORI is a Neural Execution Engine for local AI models and runtime control.

## Status

CLORI is early and not production-ready. The repository currently contains the
public project posture, initial documentation, validation checks and compatibility
references needed before runtime code is added.

It does not implement inference yet. It does not implement serving yet. It has
no benchmark results yet.

## Why This Exists

Local model work often collapses many different concerns into one opaque
runtime: file inspection, model metadata, tensor layout, quantization, memory
pressure, KV cache behavior, prefill and decode timing, streaming, metrics and
reports.

CLORI exists to make those runtime boundaries explicit. The project starts with
local model artifact and runtime inspection because that is the shortest path to
useful evidence, but the architecture is not limited to small models or one
family of workloads.

## What CLORI Is

CLORI is a standalone Neural Execution Engine. Its first practical target is
local model artifact/runtime inspection and model execution foundations: turning
model files into descriptors, tensor maps, runtime plans, backend boundaries,
decode surfaces, metrics and receipts.

CLORI can be used independently by local model users. It is not YAI, and its
public identity does not depend on YAI. It may later become a NET-compatible
external node, but that compatibility is planned rather than implemented.

Larger models and additional neural workloads are design targets. They are not
current implementation claims.

## What CLORI Is Not

CLORI is not:

- YAI
- a production system
- a working inference engine
- a working server
- a benchmark leaderboard
- a YAI authority component
- a NET discovery or routing component

This scaffold does not claim GGUF, CUDA, Metal, MLX, llama.cpp, OpenAI API or
YAI integration support.

## Operational Model

CLORI is organized around a simple execution evidence chain:

```text
artifact -> descriptor -> tensor map -> runtime plan -> backend -> decode -> metrics -> receipt
```

The chain is intentionally narrow at the start. Each stage should become
measurable before it becomes clever.

## Design Constraints

CLORI should expose runtime behavior without pretending that inspection is the
same as execution. Documentation, descriptors, metrics and receipts must remain
separate from generated benchmark evidence.

The engine should be useful for small local models first, while keeping its
architecture open to larger models, embeddings, rerankers, classifiers and other
future neural workloads.

## Current Validation

```sh
make info
make check
```

## Repository Layout

```text
docs/       architecture, boundary, spine, integration and benchmark notes
src/        source placeholder
protocols/ protocol placeholder
tests/      test placeholder
benches/    benchmark placeholder
examples/   example placeholder
```

## Documentation

- [Architecture](docs/architecture.md)
- [Boundary](docs/boundary.md)
- [Terminology](docs/terminology.md)
- [Model runtime](docs/model-runtime.md)
- [Metrics](docs/metrics.md)
- [Artifact format](docs/artifact-format.md)
- [Model descriptor](docs/model-descriptor.md)
- [Backend API](docs/backend-api.md)
- [Serving boundary](docs/serving.md)
- [Observability](docs/observability.md)
- [CLORI spine](docs/spines/clori-spine.md)
- [YAI NET reference](docs/spines/yai-net-spine-reference.md)
- [YAI NET compatibility](docs/integration/yai-net-compatibility.md)
- [Benchmark canon](docs/benchmark/benchmark-canon.md)

## License And Contributions

CLORI is licensed under the MIT license. Technical feedback is welcome while the
repository is still in scaffold form.

## Current Limitations

- no inference implementation
- no serving implementation
- no NET integration
- no GGUF parser
- no tokenizer
- no CUDA, Metal or MLX backend
- no llama.cpp adapter
- no OpenAI-compatible API
- no benchmark results
