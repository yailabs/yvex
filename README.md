<!-- docs:metadata
title: YVEX
id: yvex
document: product
status: mixed
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/brand/yvex-readme-stacked-dark.png">
    <img src="docs/assets/brand/yvex-readme-stacked-light.png" alt="YVEX" width="190">
  </picture>
</p>

<h1 align="center">Open weights. Native execution.</h1>

<p align="center">
  <strong>Compile the model. Own the execution. Know what ran.</strong><br>
  A native model compiler and stateful runtime.<br>
  Rust at the product boundary. C and hardware backends at the computational core.
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> ·
  <a href="#source-to-execution">How it works</a> ·
  <a href="#available-execution">Models &amp; backends</a> ·
  <a href="docs/evaluation/benchmarks/generated/qualification-index.md">Qualification</a> ·
  <a href="docs/README.md">Documentation</a>
</p>

<p align="center">
  <a href="docs/guides/build.md"><img src="https://img.shields.io/badge/native-C11_%2B_Rust-8D5CF5?style=flat&amp;labelColor=211a2d" alt="Native C11 and Rust"></a>
  <a href="#platforms-and-backends"><img src="https://img.shields.io/badge/backends-CPU_%C2%B7_CUDA_%C2%B7_Metal_early-8D5CF5?style=flat&amp;labelColor=211a2d" alt="CPU, CUDA and early Metal backends"></a>
  <a href="docs/project-control/STATUS.md"><img src="https://img.shields.io/badge/status-active_development-8D5CF5?style=flat&amp;labelColor=211a2d" alt="Active development"></a>
  <a href="https://github.com/yailabs/yvex/actions/workflows/qa.yml?query=branch%3Amain"><img src="https://img.shields.io/github/actions/workflow/status/yailabs/yvex/qa.yml?branch=main&amp;label=QA&amp;style=flat&amp;labelColor=211a2d" alt="QA status on main"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-8D5CF5?style=flat&amp;labelColor=211a2d" alt="MIT license"></a>
</p>

YVEX connects the entire computational lifecycle: authenticated open-weight
sources, compiled programs, physical weight representations and live model
execution. One native product takes a model from **source to session**, keeping
its identity, resource requirements and qualification evidence attached.

**Built for people who want control below the API.** Inspect what you acquired,
what you compiled, what is actually loaded, and what the evidence proves.

## Why YVEX

| Design choice | What it gives you |
| --- | --- |
| **Compile before execution** | Model semantics and parameter lineage become explicit programs and bindings, rather than topology rediscovered while serving. |
| **Separate model from representation** | A checkpoint, a quantization, an executable layout and a resident engine keep distinct identities. |
| **Make state a first-class resource** | Attention, recurrent and speculative state retain their own geometry, isolation and transactional lifetime. |
| **Keep the host alive** | Load and unload engine generations without replacing the transport. Inspect the service even with no model loaded. |
| **Measure the complete path** | Preparation, memory, prefill, committed decode and client-visible latency are separate evidence—not one unexplained speed number. |

## Source to execution

![YVEX compilation lifecycle: authenticated source, native compiler, artifact and binding, deployment admission, engine generation, isolated session.](docs/assets/diagrams/product_pipeline.svg)

<p align="center">
  <a href="docs/architecture/compiler-ir.md">Compiler architecture</a> ·
  <a href="docs/architecture/representation-artifacts.md">Physical representations</a> ·
  <a href="docs/assets/diagrams/product_pipeline.json">Editable diagram source</a>
</p>

The compiler preserves **what the model means**. Deployment decides **which
implementation can run here**. The engine owns executable resources; a session
owns mutable state. Backends execute admitted operations and numerical classes.

### The next step: goal-driven compilation

**Program P is in progress.** Its bounded native planner already checks physical
recipe candidates against declared constraints and executable consumers. The
larger goal is to build and justify a hardware-appropriate representation from
workload, quality and memory requirements.

Static feasibility is not a measured recommendation. The complete optimization
and independent qualification exit remains open.
[Current implementation](docs/architecture/compiler-ir.md#goal-constrained-physical-search-partial-implementation)
· [Selected delivery](docs/project-control/TASKS.md)

## One host. Your models. Independent sessions.

![Native and HTTP clients reach a persistent host, which routes by model and engine generation. Sessions have independent mutable state; the host can stay available with zero engines.](docs/assets/diagrams/product_runtime.svg)

<p align="center">
  <a href="docs/architecture/runtime-lifecycle.md">Runtime lifecycle</a> ·
  <a href="docs/architecture/computational-state.md">State ownership</a> ·
  <a href="docs/assets/diagrams/product_runtime.json">Editable diagram source</a>
</p>

| Connect through | Use it for | Contract |
| --- | --- | --- |
| **Native CLI / local protocol** | Local chat, model lifecycle and operator inspection | [Local protocol](docs/contracts/local-protocol.md) |
| **OpenAI-compatible HTTP** | Application inference through the bounded compatibility adapter | [Compatibility surface](docs/contracts/openai-compatibility.md) |
| **Public management + SDK** | Discover, enroll and manage a server through typed operations | [Authenticated management](docs/contracts/network-management.md) |
| **Native C API** | Embed the computational core without the Rust shell | [C API](docs/contracts/c-api.md) |

<a id="product-boundary"></a>

YVEX is usable independently. When connected to YAI, **YAI owns Cases, semantic
memory and authority; YVEX owns computation**. Graphical clients consume those
contracts rather than taking over either domain.

## Quick start

Prepare the [native toolchain](docs/guides/build.md), then build from the
repository root:

```sh
make info
make -j4 all
./yvex help
./yvex model list
```

Select a prepared model with admitted chat support from your catalog. If no
host is running, start it in one terminal:

```sh
./yvex serve
```

In another terminal, replace `MODEL` with its catalog identifier:

```sh
./yvex host status
./yvex model load MODEL
./yvex chat --model MODEL
```

Weights are acquired separately; `model load` does not download them.
[Acquisition and preparation](docs/guides/quickstart.md)
· [Published representations](docs/guides/model-lifecycle.md#published-representations)

### See what is happening

```sh
./yvex model list
./yvex host status
./yvex host logs --follow
./yvex model qualification list
```

Runtime observation and published qualification answer different questions:
**what is running now** versus **what was tested under an exact configuration**.

## Available execution

These are evidenced paths—not a promise that every checkpoint, quantization or
backend works under the same model name.

| Model / family | Demonstrated execution | Scope to keep in mind |
| --- | --- | --- |
| [**DeepSeek V4 Flash / DSpark**](docs/model-families/deepseek-v4-flash.md) | Text generation and target-verified speculation on admitted variants | Checkpoint and representation gates remain separate; 0731 and performance work are active. |
| [**Qwen3.8-27B · text**](docs/model-families/qwen3.8-text.md) | BF16 hybrid text execution on CUDA | Bounded readout; not all upstream modalities or checkpoints. |
| [**Qwen3.5-0.8B · text**](docs/model-families/qwen3.8-text.md#exact-small-text-checkpoint) | Exact mixed BF16/F32 artifact on macOS CPU | Two bounded upstream continuation matches, not general quality or Metal model qualification. |
| [**MiniMax-H3 FL2VA**](docs/model-families/minimax-h3.md) | Bounded component and composite media execution | Useful output and full-scale numerics remain unqualified. |
| [**Mamba-Codestral**](docs/model-families/mamba2.md) | Exact 64-layer CPU artifact execution | Hosted conversation, CUDA SSM and whole-model oracle remain open. |
| [**Laya · typed decisions**](docs/model-families/laya.md) | Exact CPU finite model and bounded local text producer | No general calibration, head-breadth or CUDA claim. |

### Platforms and backends

| Platform | Backend | Current boundary |
| --- | --- | --- |
| **Linux · x86-64 / arm64** | CPU / NVIDIA CUDA | Native host and CLI; execution depends on the exact admitted model, artifact and device. GB10 evidence is target-specific. |
| **macOS · Apple Silicon** | CPU | Native host and CLI, terminal lifecycle and bounded Qwen 0.8B generation. |
| **macOS · Apple Silicon** | Metal · **early** | M5 Pro device/pipeline, shared buffers, copy/zero and F32 embedding primitives. **Not full-model GPU execution.** |

[Backend evidence](docs/architecture/backend-execution.md)
· [macOS integration](docs/evaluation/macos-main-integration.md)
· [Complete capability matrix](docs/project-control/STATUS.md)

<a id="evidence-and-current-limits"></a>

## Evidence you can inspect

**A model name is not a qualification. A tok/s number is not a benchmark.**

YVEX keeps family conformance, checkpoint references, representation quality,
backend execution, deployment performance and product behavior separate. A
result belongs to the exact build, model, representation, hardware,
configuration and workload that earned it.

| Question | Start here |
| --- | --- |
| What exact targets have records? | [Generated qualification catalog](docs/evaluation/benchmarks/generated/qualification-index.md) |
| How are results measured and compared? | [Benchmark methodology](docs/evaluation/benchmarks/methodology.md) |
| What do the independent evidence planes prove? | [Evaluation guide](docs/evaluation/README.md) |
| What remains unfinished? | [Current Status](docs/project-control/STATUS.md) and [selected Tasks](docs/project-control/TASKS.md) |

YVEX is under active development. A characterized result is not a release
guarantee. General continuous batching, distributed execution and native
cognitive-state interoperation remain open or research targets. Model licenses
and qualification boundaries stay attached to each source and variant.

<a id="documentation"></a>

## Go deeper

| Use YVEX | Build with YVEX | Work on YVEX |
| --- | --- | --- |
| [Quickstart](docs/guides/quickstart.md) | [Architecture](docs/architecture/README.md) | [Contributing](CONTRIBUTING.md) |
| [Model lifecycle](docs/guides/model-lifecycle.md) | [Public contracts](docs/contracts/README.md) | [Build guide](docs/guides/build.md) |
| [Operator reference](docs/reference/README.md) | [Product vision](docs/product/VISION.md) | [Roadmap](ROADMAP.md) |

<p align="center">
  <strong>Trace the source. Inspect the execution. Keep the evidence.</strong><br>
  <a href="docs/README.md">Explore the documentation →</a>
</p>

---

Code is [MIT licensed](LICENSE). [NOTICE](NOTICE.md) explains ownership and
third-party boundaries. Upstream model licenses remain separate.
