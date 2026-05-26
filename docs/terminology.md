# Terminology

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

- Artifact: a model file or model package CLORI can inspect.
- Descriptor: normalized metadata derived from an artifact.
- Tensor table: tensor names, shapes, dtypes, quantization, offsets and sizes.
- Quant profile: normalized view of quantization strategy and memory impact.
- Backend: execution adapter boundary for a future runtime implementation.
- Execution IR: planned internal representation between descriptors and
  backend execution.
- KV cache: decode-time cache state and accounting surface.
- Prefill: planned execution phase that processes input context.
- Decode: planned execution phase that produces output tokens or neural
  outputs.
- Metric: measured runtime evidence, not a benchmark claim by itself.
- Receipt: structured evidence emitted by CLORI for inspection, execution or
  benchmark activity.
- Benchmark: repeatable measurement of model, backend, hardware and workload
  behavior from real runs.
- NET-compatible node: a future CLORI adapter shape that lets YAI invoke CLORI
  through NET without giving CLORI YAI authority.

No inference implementation exists in this bootstrap delivery.
