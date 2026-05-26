# CLORI Spine

Reference version: CLORI.SPINE.0.1
Authority: CLORI repository
Status: Boundary and roadmap foundation

CLORI is a standalone Neural Execution Engine repository.

CLORI owns:
- model artifact inspection
- model descriptor
- tensor table
- quant profile
- architecture registry
- family profile
- execution IR
- backend abstraction
- memory accounting
- KV cache
- decoder/sampler
- neural output generation
- inference metrics
- benchmark receipts
- standalone CLI/server
- NET-compatible node adapter

CLORI does not own:
- YAI case authority
- YAI policy
- YAI memory
- YAI graph
- YAI facts
- YAI journal
- YAI carrier authority
- YAI action approval
- NET discovery
- NET routing
- NET trust authority

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Scope

This spine belongs to the external CLORI repository. It must not be inserted
into the YAI repository main spine. CLORI imports a copied, non-authoritative
NET spine reference only for compatibility alignment.

## Deliveries

### CLORI.SPINE.0 - Repository Foundation + Public Boundary

Create standalone repository foundation.

### CLORI.SPINE.1 - Product / Engine Doctrine

Define CLORI as Neural Execution Engine, not only an LLM runner.

### CLORI.SPINE.2 - Terminology Canon

Define canonical vocabulary: artifact, descriptor, tensor, backend, KV, decode,
receipt, benchmark.

### CLORI.SPINE.3 - YAI/NET Compatibility Reference Import

Copy YAI NET.SPINE reference and declare compatibility boundary.

### CLORI.SPINE.4 - Artifact Format Boundary

Define artifact format layer independent from model family.

### CLORI.SPINE.5 - GGUF Inspect Canon

Define GGUF inspect as first vertical target.

### CLORI.SPINE.6 - Model Descriptor Canon

Define normalized model descriptor.

### CLORI.SPINE.7 - Tensor Table Canon

Define tensor table: name, shape, dtype, quant, offset, placement, bytes.

### CLORI.SPINE.8 - Quant Profile Canon

Define quantization profile.

### CLORI.SPINE.9 - Memory Estimate Canon

Define memory estimate formulas for weights, KV, activations, scratch and
backend reserve.

### CLORI.SPINE.10 - Architecture Registry

Define architecture registry: MHA, GQA, MQA, MoE, embedding, reranker,
classifier, vision, audio.

### CLORI.SPINE.11 - Family Profile Registry

Define family profiles: Qwen, Llama, DeepSeek, Mistral first.

### CLORI.SPINE.12 - Tokenizer / Prompt Renderer Boundary

Define prompt/input renderer without letting family profile become chat
authority.

### CLORI.SPINE.13 - Execution IR Skeleton

Define execution IR before backend execution.

### CLORI.SPINE.14 - Backend API Boundary

Define backend API.

### CLORI.SPINE.15 - Stub Backend

Create stub backend for contracts and tests.

### CLORI.SPINE.16 - llama.cpp Baseline Adapter Boundary

Define llama.cpp as baseline adapter, not final architecture.

### CLORI.SPINE.17 - CPU Reference Execution Research Boundary

Define CPU reference research boundary.

### CLORI.SPINE.18 - Metal / Apple Backend Boundary

Define Apple/Metal backend posture.

### CLORI.SPINE.19 - CUDA Backend Boundary

Define CUDA backend posture.

### CLORI.SPINE.20 - Memory Manager

Implement memory manager boundary.

### CLORI.SPINE.21 - KV Cache Manager

Define KV cache states: empty, prefilled, decoding, reusable, stale, evicted.

### CLORI.SPINE.22 - Decoder / Sampler Policy

Define decoder/sampler policy.

### CLORI.SPINE.23 - Streaming Decode Surface

Define streaming token surface.

### CLORI.SPINE.24 - Metrics Core

Define metrics core.

### CLORI.SPINE.25 - CLORI Receipt Canon

Define `clori.receipt.v1`.

### CLORI.SPINE.26 - Benchmark Harness Foundation

Create benchmark harness foundation.

### CLORI.SPINE.27 - Benchmark Prompt/Workload Catalog

Create prompt/workload catalog.

### CLORI.SPINE.28 - Hardware Profile Catalog

Create hardware profile catalog.

### CLORI.SPINE.29 - Model Size / Quant Matrix

Create model size/quant performance matrix.

### CLORI.SPINE.30 - Standalone CLI

Create standalone CLI.

### CLORI.SPINE.31 - Local Model Library

Create local model library.

### CLORI.SPINE.32 - Standalone Server

Create standalone server.

### CLORI.SPINE.33 - OpenAI-Compatible Adapter

Expose OpenAI-compatible adapter.

### CLORI.SPINE.34 - CLORI Native API

Expose CLORI-native API.

### CLORI.SPINE.35 - NET-Compatible Node Adapter

Expose NET-compatible node adapter.

### CLORI.SPINE.36 - YAI Invocation Receipt Contract

Define YAI invocation and receipt contract.

### CLORI.SPINE.37 - Multi-Model Registry

Support multi-model registry.

### CLORI.SPINE.38 - Large Model Memory Planner

Support large model memory planner.

### CLORI.SPINE.39 - Remote Node Benchmark

Benchmark remote node execution.

### CLORI.SPINE.40 - Local/LAN/Remote Execution Matrix

Benchmark local, localhost, LAN and remote execution.

### CLORI.SPINE.41 - Prefix/KV Reuse Metrics

Measure prefix reuse and KV reuse.

### CLORI.SPINE.42 - Speculative Decode Boundary

Define speculative decode boundary.

### CLORI.SPINE.43 - Native MTP / Future Decode Acceleration Boundary

Define MTP/future acceleration boundary.

### CLORI.SPINE.44 - Embedding Output v0

Add embedding output contract.

### CLORI.SPINE.45 - Reranker Output v0

Add reranker output contract.

### CLORI.SPINE.46 - Classifier Output v0

Add classifier output contract.

### CLORI.SPINE.47 - Vision Encoder Future Boundary

Reserve vision encoder boundary.

### CLORI.SPINE.48 - Audio Encoder Future Boundary

Reserve audio encoder boundary.

### CLORI.SPINE.49 - Error / Fallback / Degraded Mode Canon

Define degraded mode, fallback and execution errors.

### CLORI.SPINE.50 - Observability Dashboard Data Contract

Define observability dashboard data contract.

### CLORI.SPINE.51 - Community Benchmark Report Format

Define public benchmark report format.

### CLORI.SPINE.52 - Optimization Report Format

Define optimization report format.

### CLORI.SPINE.53 - Packaging / Release Boundary

Define packaging and release boundary.

### CLORI.SPINE.54 - Public Documentation Boundary

Define public documentation boundary.

### CLORI.SPINE.55 - YAI Integration Example

Create YAI integration example.

### CLORI.SPINE.56 - Standalone User Example

Create standalone user example.

### CLORI.SPINE.57 - Regression Benchmark Gate

Add regression benchmark gate.

### CLORI.SPINE.58 - Compatibility Freeze

Freeze compatibility contract.

### CLORI.SPINE.59 - CLORI v0 Freeze

Freeze CLORI v0.
