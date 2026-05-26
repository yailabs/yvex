# CLORI

CLORI is a Neural Execution Engine for local AI models and runtime control.

## Status

CLORI is early and not production-ready. This repository is a scaffold: it does
not implement inference, serving, NET compatibility or benchmark execution yet.
No benchmark results exist yet.

## Why This Exists

Local model work needs inspectable runtime boundaries: artifacts, descriptors,
tensor layout, quantization posture, memory pressure, KV cache behavior,
decoding surfaces, metrics and receipts. CLORI exists to make those foundations
explicit before implementation grows around them.

## What CLORI Is

CLORI is a standalone Neural Execution Engine. Its first practical target is
local model artifact/runtime inspection and model execution foundations. The
architecture must not be limited to small models.

CLORI is not YAI. CLORI may later become a NET-compatible external node.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

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

Planned flow:

```text
artifact -> descriptor -> tensor table -> memory/KV accounting -> backend boundary -> decode surface -> metrics -> receipt -> benchmark report
```

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
