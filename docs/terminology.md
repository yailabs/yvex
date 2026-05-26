# Terminology

- Artifact: a model file or model package CLORI can inspect.
- Descriptor: normalized metadata derived from an artifact.
- Tensor table: tensor names, shapes, dtypes, quantization, offsets and sizes.
- Backend: execution adapter boundary for CPU, Metal, CUDA or external engines.
- KV cache: decode-time cache state and accounting surface.
- Receipt: evidence emitted by CLORI for execution, benchmark or inspection.
- Benchmark: repeatable measurement of model/backend/workload behavior.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

No inference implementation exists in this bootstrap delivery.
