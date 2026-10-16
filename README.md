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

<h1 align="center">Physical Model Compiler &amp; Inference Runtime</h1>

<p align="center">
  Compile model semantics and parameter representations into authenticated execution plans.<br>
  Admit, execute and measure those plans against explicit numerical and resource constraints.
</p>

<p align="center">
  <a href="#why-yvex">Why YVEX</a> ·
  <a href="#source-to-execution">How it works</a> ·
  <a href="#program-p-physical-model-compilation">Program P</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#available-execution">Models &amp; backends</a> ·
  <a href="#measured-performance">Benchmarks</a> ·
  <a href="#inside-the-computational-core">Under the hood</a> ·
  <a href="docs/README.md">Documentation</a>
</p>

<p align="center">
  <a href="docs/guides/build.md"><img src="https://img.shields.io/badge/native-C11_%2B_Rust-8D5CF5?style=flat&amp;labelColor=211a2d" alt="Native C11 and Rust"></a>
  <a href="#platforms-and-backends"><img src="https://img.shields.io/badge/backends-CPU_%C2%B7_CUDA_%C2%B7_Metal_early-8D5CF5?style=flat&amp;labelColor=211a2d" alt="CPU, CUDA and early Metal backends"></a>
  <a href="docs/project-control/STATUS.md"><img src="https://img.shields.io/badge/status-active_development-8D5CF5?style=flat&amp;labelColor=211a2d" alt="Active development"></a>
  <a href="https://github.com/yailabs/yvex/actions/workflows/qa.yml?query=branch%3Amain"><img src="https://img.shields.io/github/actions/workflow/status/yailabs/yvex/qa.yml?branch=main&amp;label=QA&amp;style=flat&amp;labelColor=211a2d" alt="QA status on main"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-8D5CF5?style=flat&amp;labelColor=211a2d" alt="MIT license"></a>
</p>

YVEX is a native compiler and runtime for open-weight neural models, with a C11
computational core and a Rust product shell. It separates **what a model
computes**, **how its parameters are encoded**, and **how that computation runs
on a particular machine**. Authenticated runtime bindings connect the compiled
program to its exact parameters; deployment admits the selected implementation
and resource envelope.

The system executes admitted attention, hybrid, state-space and finite-decision
programs. It is under active development: bounded physical compilation and real
model execution are implemented; automatic qualified selection and competitive
DeepSeek GB10 performance remain unfinished.

<a id="why-yvex"></a>

<a id="implementation-boundaries"></a>

## Why YVEX

A weight format does not determine the algorithm that consumes it. The same
encoded parameters may need different implementations for narrow decode and
batched prefill; a smaller artifact may require more workspace or slower
computation. YVEX makes these decisions separately identifiable so they can be
checked, changed and compared without losing their source lineage.

| Decision | What YVEX keeps explicit |
| --- | --- |
| Model semantics | Source-authored operations, tensor roles, routing, state equations and numerical obligations. |
| Parameter representation | Source-to-terminal transformations, quantization, encoded geometry and authenticated byte ranges. |
| Executable realization | Compatible operations, layouts, phase geometry and numerical implementations over those parameters. |
| Runtime deployment | Device capabilities, preparation, workspace, sequence state and capacity admission. |

This separation supports physical alternatives; it does not imply that YVEX
already finds the fastest or highest-quality one automatically.

## Source to execution

![Computational lowering and parameter derivation join through exact terminal resolution, then produce physical plans and an authenticated runtime binding for deployment admission.](docs/assets/diagrams/product_pipeline.svg)

<p align="center">
  <a href="docs/architecture/compiler-ir.md">Compiler architecture</a> ·
  <a href="docs/architecture/representation-artifacts.md">Physical representations</a> ·
  <a href="docs/assets/diagrams/product_pipeline.svg">Full-size SVG</a> ·
  <a href="docs/assets/diagrams/product_pipeline.json">Editable diagram source</a>
</p>

Computation and parameter compilation follow separate paths. Family
interpretation supplies the operator equations and topology; Transformation IR
records parameter derivation into encoded package terminals. The join resolves
symbolic operands to exact lineage, shape and layout. Compiled plans enter an
authenticated runtime binding, not a runtime search by tensor filename.

Deployment specialization then checks the numerical implementation, backend
operations and complete resource envelope. An equivalent kernel can reuse the
artifact; changed parameter encoding requires a new artifact and binding.

Qwen, MiniMax and Mamba2 retain native typed programs. DeepSeek retains a
canonical operator schedule with derived attention, MoE and transformer plans;
it is not represented as one uniform full-model SSA program. The
[consumer map](docs/architecture/compiler-ir.md#current-consumer-cutover)
records the actual lowering and execution paths.

<a id="active-frontier-goal-driven-physical-compilation"></a>

<a id="physical-optimization-implemented-scope-and-remaining-gates"></a>

## Program P: physical model compilation

**Program P is the active compiler development frontier.** Its objective is to
produce and compare legal physical realizations for an exact model, hardware,
workload and quality requirement—not just choose a smaller weight encoding.

| Implemented foundation | Remaining engineering and qualification |
| --- | --- |
| Native profile resolution, bounded recipe synthesis and role-aware source/Q2 allocation | Broader workload-specific execution search and complete resource costing across alternatives. |
| Policy export, quantization planning/emission, independent structural reading and authenticated binding production | Integrated experimental selection with independent numerical and held-out model-quality evidence. |
| Graph-derived input-view alternatives, native CLI and bounded typed SDK planning | Evidence-backed recommendations and competitive complete-model throughput. |

The allocation objective currently retains source elements under an encoded
weight budget. That is not a model-quality score or a throughput prediction.
An emitted artifact, an admitted engine and a qualified recommendation are
different results. Compilation does not install or replace a running engine.

[Implemented compiler path](docs/architecture/compiler-ir.md#goal-constrained-physical-search-partial-implementation)
· [Compiler evidence](docs/evaluation/compiler-evidence.md)
· [Research questions](docs/research/physical-model-compiler.md)
· [Current Status](docs/project-control/STATUS.md)

## Quick start

Install the [native prerequisites](docs/guides/build.md): GNU Make 4.3+, a C11
compiler, Python 3, Rust/Cargo 1.98.1 and libclang. The compiler product also
requires CMake and a C++ compiler for its pinned independent GGUF reader.
CUDA is optional at build time; model execution still requires an admitted
backend. From the repository root:

```sh
make info
make -j4 all
./yvex version --json
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
./yvex model show MODEL
./yvex model load MODEL
./yvex chat --model MODEL
```

For an optional terminal workspace, run `./yvex workbench`: conversation,
Models, Compile, Activity and exact runtime inspection share the same native
owners. [Workbench controls](docs/guides/operator-runbook.md#optional-terminal-workbench).

Weights are acquired separately; `model load` does not download them.
[Acquisition and preparation](docs/guides/quickstart.md)
· [Published representations](docs/guides/model-lifecycle.md#published-representations)

### Inspect physical compilation

Use an authenticated acquired source; a catalog listing alone is not sufficient.
These commands plan candidates without loading a model:

```sh
./yvex compile optimize --list-techniques
./yvex compile optimize --guided --out-request goals.json
./yvex compile optimize --request goals.json --json
```

The guide requires a terminal and saves reproducible intent; JSON replay uses
the same native planner. Missing source authentication, calibration or a
compatible consumer remains an explicit refusal. Inspect candidates before
exporting a policy for the existing quant plan/emit pipeline.
[Production workflow](docs/architecture/compiler-ir.md#reproducible-engineering-workflow).

<a id="see-what-is-happening"></a>

### Runtime inspection

```sh
./yvex model list
./yvex host status
./yvex host logs --follow
./yvex model qualification list
```

Runtime observation and published qualification answer different questions:
**what is running now** versus **what was tested under an exact configuration**.

| Interface | Intended use |
| --- | --- |
| [Native CLI / local protocol](docs/contracts/local-protocol.md) | Chat, model lifecycle and local inspection. |
| [OpenAI-compatible HTTP](docs/contracts/openai-compatibility.md) | Application inference within the documented compatibility surface. |
| [Public management and canonical SDKs](docs/contracts/product-management.md) | Typed discovery, authorized management and bounded compiler planning. |
| [Native C API](docs/contracts/c-api.md) | Computational embedding independent of the Rust shell. |

## Available execution

These are evidenced paths—not a promise that every checkpoint, quantization or
backend works under the same model name.

| Model / family | Demonstrated execution | Scope to keep in mind |
| --- | --- | --- |
| [**DeepSeek-V4-Flash-DSpark**](docs/model-families/deepseek-v4-flash.md) | Text generation and target-verified speculation on admitted variants | Historical checkpoint used by the installed operator profile; not the 0731 candidate. |
| [**DeepSeek-V4-Flash-0731**](docs/model-families/deepseek-v4-flash.md#selected-matched-checkpoint-candidate) | Authenticated native artifacts, bindings and measured CUDA generation | Active Program P acceptance workload; independent representation quality and competitive throughput remain open. |
| [**Qwen3.8-27B · text**](docs/model-families/qwen3.8-text.md) | BF16 hybrid text execution on CUDA | Bounded readout; not all upstream modalities or checkpoints. |
| [**Qwen3.5-0.8B · text**](docs/model-families/qwen3.8-text.md#exact-small-text-checkpoint) | Exact mixed BF16/F32 artifact on macOS CPU | Two bounded upstream continuation matches, not general quality or Metal model qualification. |
| [**MiniMax-H3 FL2VA**](docs/model-families/minimax-h3.md) | Bounded component and composite media execution | Useful output and full-scale numerics remain unqualified. |
| [**Mamba-Codestral-7B-v0.1**](docs/model-families/mamba2.md) | Exact 64-layer CPU artifact execution | Hosted conversation, CUDA SSM and whole-model oracle remain open. |
| [**Laya · typed decisions**](docs/model-families/laya.md) | Exact CPU finite model and bounded local text producer | No general calibration, head-breadth or CUDA claim. |

Model names identify sources and checkpoints. Representation, artifact, runtime
binding, backend, strategy and loaded engine generation are separate attributes.
Existing deployment aliases remain valid compatibility identifiers; this
presentation does not retarget them or replace the installed checkpoint.

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

<a id="evidence-you-can-inspect"></a>

## Measured performance

The tables below are generated from immutable qualification receipts, not
manually maintained numbers. They expose the current engineering gap as well
as the demonstrated execution. **Characterization is not quality qualification.**

The Program P GB10 acceptance targets remain **unearned**: at least 20 sustained
target-only committed tok/s and 700 uncached prefill tok/s at **both 2K and 8K**,
with 24/800 as preferred checkpoints. These are engineering requirements, not
published results. The retained showcase below is not a live progress feed.

<!-- docs:benchmark showcase -->

**DeepSeek-V4-Flash-0731 · NVIDIA DGX Spark GB10 × 1 · CUDA.**

One retained publication checkpoint, not a hardware-independent speed claim.
Native protocol measurements use an isolated resident host, a separate warmup
and fresh sessions without prefix reuse; they do not describe whichever engine is installed today.
File-cache state was uncontrolled. Full build, artifact, binding and specialization identities are linked per row.

| Configuration | Exact measured scope |
| --- | --- |
| Checkpoint / build source | `7872f01b1d1f` / `b8130fd51f9b`; full identities in each receipt |
| Physical representation | goal-v1-mxfp4-routed-q2_k |
| Input / execution | chunk=512; width=1; concurrency 1; reasoning `none` |
| Sampling / transport | Temperature 0, deterministic; `product-native-v25` |
| Driver / toolkit | 580.159.03; CUDA 13.0; nvcc 13.0.88 |

| Workload / metric | Strategy · context | Input / output | Median (tok/s) | Min–max | N |
| --- | --- | ---: | ---: | ---: | ---: |
| [C hash table](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010.md) · committed decode | target-only · 4096 | 31 / 256 | 9.66 | 9.64–9.66 | 3 |
| [C hash table](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010.md) · committed decode | speculative · 4096 | 31 / 256 | 14.38 | 14.37–14.43 | 3 |
| [Long text · 2K](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-2048-20261010.md) · new prefill | target-only · 16384 | 2048 / 16 | 103.24 | 101.95–103.31 | 3 |
| [Long text · 8K](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-prefill-promessi-8192-20261010.md) · new prefill | target-only · 16384 | 8192 / 16 | 89.73 | 89.61–89.81 | 3 |

Decode excludes the first committed token and its latency. Input/output counts are server-authored;
prefill counts newly executed input positions, not reused context.
Different strategies and context bands remain separate rows, not an averaged score.

| Coding request | Server TTFT | First visible content | Complete client request |
| --- | ---: | ---: | ---: |
| [target-only](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-coding-hash-table-20261010.md) | 0.933 s | 1.461 s | 27.885 s |
| [speculative](docs/evaluation/benchmarks/generated/qualification-deepseek-0731-program-p-native-mxfp4-publication-speculative-coding-hash-table-20261010.md) | 1.231 s | 1.848 s | 19.574 s |

Performance: **CHARACTERIZED**. Independent representation quality: **BLOCKED**.
Latency cells are medians; their ranges, MAD, raw sample identities and memory/preparation
observations are in the linked receipts. No quality metric or confidence interval is invented.
These records do not establish high/maximum reasoning, other models, HTTP latency or release readiness.

<!-- /docs:benchmark -->

[All targets and experiments](docs/evaluation/benchmarks/generated/qualification-index.md)
· [Workload / reasoning matrix](docs/evaluation/benchmarks/generated/qualification-workloads.md)
· [Measurement definitions](docs/evaluation/benchmarks/methodology.md)

Loading and prepared-weight construction affect startup and memory cost;
uncached prefill and prefix reuse describe different input work; decode counts
committed tokens, not speculative proposals. Server TTFT, client-visible output,
sampling and complete-turn latency are measured separately where observed.
No single token/s value describes all of these costs. Unavailable measurements
remain unavailable, and calibration is not independent held-out evaluation.

## Inside the computational core

Compilation separates the model's equations from parameter encoding and from
the algorithms that execute them. These contracts constrain which physical
alternatives an optimization may select.

<a id="two-compilation-lanes-one-authenticated-binding"></a>

### Parameter resolution and operation lowering

| Boundary | What becomes explicit | Canonical owner |
| --- | --- | --- |
| **Computation** | Typed operands/results, dependencies, numerical obligations and state effects | [Compiler IR](docs/architecture/compiler-ir.md) |
| **Parameters** | Transformation lineage, encoded terminals, qtypes, layout and sharing | [Representations](docs/architecture/representation-artifacts.md) |
| **Parameter join** | Exact terminal references and compiled model plans in the runtime binding | [Artifact admission](docs/architecture/artifacts-admission.md) |
| **Deployment** | Compatible backend operations, preparation, workspace, state and reserve | [Specialization](docs/architecture/deployment-specialization.md) |

A storage format is not an execution algorithm. A qtype the artifact reader can
decode is not automatically legal for every fused kernel. Physical Execution IR
describes package terminals; the physical computational program describes work
over those terminals. Neither is a resident engine.

<a id="one-host-your-models-independent-sessions"></a>

<a id="request-execution-and-state-publication"></a>

<a id="state-has-geometry-publication-has-a-transaction"></a>

### Transactional generation and state

![Generation stages session state before accepted-prefix commit. Only commit reaches output; cancellation, execution failure or commit refusal follow abort and cleanup.](docs/assets/diagrams/product_runtime.svg)

<p align="center">
  <a href="docs/architecture/runtime-lifecycle.md">Runtime lifecycle</a> ·
  <a href="docs/architecture/computational-state.md">State ownership</a> ·
  <a href="docs/assets/diagrams/product_runtime.svg">Full-size SVG</a> ·
  <a href="docs/assets/diagrams/product_runtime.json">Editable diagram source</a>
</p>

Attention caches, recurrent state, speculative drafts, decoder, RNG and token
ledgers have different storage geometries. Session transactions coordinate their
commit or abort. Only the accepted prefix publishes state and output fragments;
cancellation discards uncommitted work. Failed cleanup can invalidate continuity
and require reset. Captured prefixes remain tied to their execution identity.

The host can run with zero engines. Loaded engines own executable resources;
sessions and leases retain an exact generation and prevent premature unload.
Closing a connection is not closing a session, and replacing an engine cannot
make an old session valid against the new generation.

The scheduler selects actual ready work. Execution batches and expert worklists
describe selected rows and routed populations; they do not invent useful batch
width. Backends own buffers, submission, synchronization and launch geometry.
DSpark output is counted only after target verification and committed publication.

[Computational state](docs/architecture/computational-state.md)
· [Scheduling](docs/architecture/scheduling-resources.md)
· [Generation](docs/architecture/generation-decode.md)
· [Speculation](docs/architecture/advanced-generation.md)

### Numerical behavior is part of the implementation contract

Quantized storage, decoded operands, reduction order, accumulation precision
and publication dtype are separate decisions. An optimization claiming the
same numerical class must preserve its obligations. A different arithmetic
realization needs explicit admission and independent evidence—not a looser test.

[Numerical ABI](docs/contracts/numerical-abi.md)
· [Backend execution](docs/architecture/backend-execution.md)
· [Independent references](docs/evaluation/benchmarks/generated/qualification-references.md)

### Native library and product shell

The C core owns compilation and computation; the Rust shell parses registered
operations and renders typed reports through generated FFI. `make lib` builds
the native archive independently of Cargo; `make client` builds the product.
The canonical Rust and TypeScript SDKs live in `sdk/rust` and `sdk/typescript`.
Public planning is bounded; a candidate read does not authorize production,
GPU execution or deployment.

<a id="product-boundary"></a>

YVEX works independently of YAI. In an integrated system, YAI owns Cases,
semantic memory and authority; YVEX owns the admitted computation. No YAI
semantic policy belongs in the inference runtime.

<a id="documentation"></a>

<a id="go-deeper"></a>

## Technical documentation

| Use YVEX | Build with YVEX | Work on YVEX |
| --- | --- | --- |
| [Quickstart](docs/guides/quickstart.md) | [Architecture](docs/architecture/README.md) | [Contributing](CONTRIBUTING.md) |
| [Model lifecycle](docs/guides/model-lifecycle.md) | [Public contracts](docs/contracts/README.md) | [Build guide](docs/guides/build.md) |
| [Operator reference](docs/reference/README.md) | [Evaluation and qualification](docs/evaluation/README.md) | [Tasks and current work](docs/project-control/TASKS.md) |

[Product vision](docs/product/VISION.md) · [Roadmap](ROADMAP.md)
· [Research](docs/research/README.md) · [QA and test ownership](docs/evaluation/qa.md)

General continuous batching, distributed execution and native cognitive-state
interoperation remain open or research targets. See
[Status](docs/project-control/STATUS.md) for capability boundaries; a characterized
model result is not a release guarantee or qualification of another checkpoint.

---

Code is [MIT licensed](LICENSE). [NOTICE](NOTICE.md) explains ownership and
third-party boundaries. Upstream model licenses remain separate.
