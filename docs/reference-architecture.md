# YVEX Reference Architecture Map

Date: 2026-07-11
Status: external reference ownership contract
Authority: primary papers, specifications, and implementation references mapped
to YVEX owners

This document owns the external engineering baseline. It does not own project
state, milestone state, dependency order, capability claims, or Active Next;
those belong only to `../PROJECT.md`.

References are used to identify proven decomposition, exact model behavior,
container rules, numerical contracts, and independent comparison points. YVEX
still owns its C API, process model, memory lifetime, failure behavior, tests,
and support claims.

## Ownership Rules

- Prefer the original paper, format specification, official documentation, or
  direct implementation source over summaries and blog posts.
- Record the exact upstream revision when a milestone turns a reference into
  executable behavior or a golden comparison.
- Translate concepts through the owning YVEX type or module; do not reproduce a
  Python class hierarchy as C file structure.
- External support, output, artifacts, and benchmarks do not prove YVEX
  support.
- Conflicting sources are blockers until the exact local model source and a
  validated implementation contract resolve them.

## Reference-To-Owner Map

| Primary reference | Concept studied | YVEX owner | Consuming milestone | YVEX does not inherit or claim |
| --- | --- | --- | --- | --- |
| [vLLM architecture](https://docs.vllm.ai/en/latest/design/arch_overview/) and direct [DeepSeek-V4 model source](https://github.com/vllm-project/vllm/blob/main/vllm/model_executor/models/deepseek_v4.py) | Engine lifecycle, model-runner boundary, input preparation, model integration, attention/KV handoff, and backend selection | `src/runtime/`, `src/model/target/`, `src/graph/`, `src/generation/` | `V010.MODEL.ARCH.IR.0`, both runtime-descriptor milestones, `V010.GRAPH.DEEPSEEK.ATTENTION.0`, `V010.RUNTIME.DEEPSEEK.KV.0`, `V010.RUNTIME.DEEPSEEK.GENERATION.0` | Python APIs, worker processes, scheduling policy, distributed topology, serving interfaces, or model support |
| [SGLang runtime](https://github.com/sgl-project/sglang/tree/main/python/sglang/srt) and direct [DeepSeek-V4 model source](https://github.com/sgl-project/sglang/blob/main/python/sglang/srt/models/deepseek_v4.py) | Model loader/runner decomposition, DeepSeek layer construction, attention variants, KV use, MoE routing/expert execution, and prefill/decode composition | `src/model/target/`, `src/graph/`, `src/generation/`, `src/backend/cuda/` | `V010.MODEL.ARCH.IR.0`, `V010.GRAPH.DEEPSEEK.ATTENTION.0`, `V010.RUNTIME.DEEPSEEK.KV.0`, `V010.RUNTIME.DEEPSEEK.PREFILL.0`, `V010.RUNTIME.DEEPSEEK.MOE.0`, `V010.GRAPH.DEEPSEEK.TRANSFORMER.0` | SGLang APIs, Python runtime, scheduler, distributed execution, kernel availability, or automatic compatibility |
| Pinned ggml `af97976c7810cdabb1863172f31c432dab767de7`: [GGUF specification](https://github.com/ggml-org/ggml/blob/af97976c7810cdabb1863172f31c432dab767de7/docs/gguf.md), [`ggml_type` and row API](https://github.com/ggml-org/ggml/blob/af97976c7810cdabb1863172f31c432dab767de7/include/ggml.h), and [type traits](https://github.com/ggml-org/ggml/blob/af97976c7810cdabb1863172f31c432dab767de7/src/ggml.c); direct llama.cpp [conversion](https://github.com/ggml-org/llama.cpp/blob/master/convert_hf_to_gguf.py) and [quantization](https://github.com/ggml-org/llama.cpp/blob/master/tools/quantize/quantize.cpp) sources | Container metadata and tensor directory ABI, alignment, qtype geometry, naming/layout transforms, conversion, quantization, and reader/writer comparison | `src/gguf/`, `src/artifact/`, `src/model/target/` | `V010.GGUF.QTYPE.ABI.1`, `V010.GGUF.ARTIFACT.ABI.1`, `V010.GGUF.LAYOUT.INTEGRITY.1`, `V010.MAP.GGUF.DEEPSEEK.0`, `V010.QUANT.2`, `V010.GGUF.WRITER.1`, `V010.GGUF.ROUNDTRIP.1` | llama.cpp CLI/API compatibility, qtypes outside the pinned on-disk baseline, converter correctness without YVEX tests, or artifact support from parse acceptance alone |
| NVIDIA [TensorRT-LLM architecture](https://nvidia.github.io/TensorRT-LLM/architecture/overview.html) and [CUTLASS](https://github.com/NVIDIA/cutlass) | CUDA execution boundaries, KV/resource ownership, model-engine separation, Blackwell matrix kernels, low-precision layouts, MoE execution, synchronization, and profiling | `src/backend/cuda/`, `src/graph/yvex_graph_plan.c`, `src/graph/yvex_memory_plan.c` | `V010.CUDA.FAILCLOSED.0`, `V010.GRAPH.DEEPSEEK.ATTENTION.0`, `V010.RUNTIME.DEEPSEEK.MOE.0`, `V010.GRAPH.DEEPSEEK.TRANSFORMER.0`, `V010.BENCH.DEEPSEEK.0` | TensorRT engine format, plugins, executor/process model, multi-GPU scope, fused-kernel availability, performance, or backend support |
| DeepSeek [V4 technical report](https://arxiv.org/abs/2606.19348), official [model card](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash), exact [config](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash/blob/main/config.json), tokenizer/encoding sidecars, and [FlashMLA](https://github.com/deepseek-ai/FlashMLA) | Exact architecture identifier, hybrid attention, compressed KV, position rules, mHC, MoE topology, tokenizer/encoding contract, source dtype/quantization facts, and reference kernel behavior | `src/source/`, `src/model/target/`, `src/tokenizer/`, `src/graph/`, `src/backend/cuda/` | `V010.REBASE.DEEPSEEK.0`, `V010.MODEL.ARCH.IR.0`, `V010.TENSOR.COVERAGE.DEEPSEEK.0`, `V010.MAP.GGUF.DEEPSEEK.0`, `V010.RUNTIME.DEEPSEEK.TOKENIZER.0`, attention/MoE milestones | Facts absent from the local source, assumptions inferred from the family name, external runtime output, external GGUF support, or DeepSeek support in YVEX |

## Validation Boundary

### Pinned GGUF Qtype Storage Baseline

`V010.GGUF.QTYPE.ABI.1` uses ggml commit
`af97976c7810cdabb1863172f31c432dab767de7`. At that revision the GGUF
specification defines the on-disk type range through ID 39. The implementation
also defines `NVFP4`, `Q1_0`, and `Q2_0` as IDs 40 through 42. YVEX records
those identities and their upstream geometry but refuses them as outside the
pinned on-disk baseline.

`include/yvex/gguf_qtype.h` and `src/gguf/yvex_gguf_qtype.c` own the admitted
identity and geometry. Storage uses `ne[0]` as row width, requires exact block
division, and multiplies row bytes by the checked product of the remaining
dimensions. Dtype, parser, range, integrity, conversion, and memory-plan owners
consume that registry. Storage admission remains independent from reference
dequantization, quantization, emission, and backend compute.

Each consuming milestone must record which upstream revision and local source
facts it used, the YVEX owner that implements the result, the independent test
or comparison, and any unresolved divergence. A reference becomes evidence only
through that milestone's executable tests. Until then it is design input.
