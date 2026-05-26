# Terminology

- Artifact: model file or package CLORI may inspect. It is input material, not
  proof that execution support exists.
- Descriptor: normalized model metadata derived from an artifact or catalog.
- Tensor table: tensor inventory with names, shapes, dtypes, placement and byte
  accounting.
- Quant profile: quantization posture and its expected memory/runtime impact.
- Architecture profile: normalized description of model architecture traits.
- Family profile: model-family defaults and interpretation rules.
- Backend: execution adapter boundary for a runtime implementation.
- Runtime plan: prepared execution shape before backend execution.
- KV cache: decode-time cache state and accounting surface.
- Prefill: prompt/context processing phase before token generation.
- Decode: output generation phase.
- Token stream: ordered stream of generated token events.
- Metric: measured runtime evidence.
- Receipt: structured evidence emitted by CLORI activity.
- Benchmark run: reproducible measurement over model, workload and environment.
