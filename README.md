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

<h1 align="center">Compiler and runtime for neural model inference</h1>

<p align="center">
  Model interpretation, parameter transformation and physical execution.<br>
  C11 computational library, Rust command-line client, CPU and CUDA backends;<br>
  early Metal primitives with separate qualification.
</p>

<p align="center">
  <a href="#quick-start">Quick start</a> ·
  <a href="#source-to-execution">How it works</a> ·
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

YVEX compiles authenticated model sources into parameter artifacts and runtime
bindings. Family interpretation supplies operator semantics, tensor roles,
routing rules and state equations. The compiler resolves these against an
exact parameter representation; deployment checks the selected numerical
implementation, backend operations and resource requirements before loading.

Execution covers admitted attention, hybrid and state-space programs. The
implementation paths differ by family; neither a readable weight format nor an
available backend establishes that a particular model can execute.

<a id="why-yvex"></a>

## Implementation boundaries

| Boundary | Mechanism |
| --- | --- |
| Computational program | Typed operations or a retained operator schedule carry parameter references, data dependencies, state effects and numerical obligations. |
| Parameter representation | Transformation IR tracks source-to-terminal derivation; the artifact fixes encoded tensor geometry, qtype and byte ranges. |
| Executable specialization | Parameter resolution and backend legalization determine compatible operations, buffers, workspace and state capacity. |
| Session transaction | Attention/recurrent state, draft state, token ledger, decoder and RNG participate in accepted-prefix commit or abort. |
| Measurement | Receipts distinguish preparation, newly executed prefill, committed decode, server TTFT and client-visible delivery for an exact configuration. |

## Source to execution

![Computation and parameter compilation: model operators and source-to-terminal transformations join through exact parameter resolution before physical planning, binding and deployment admission.](docs/assets/diagrams/product_pipeline.svg)

<p align="center">
  <a href="docs/architecture/compiler-ir.md">Compiler architecture</a> ·
  <a href="docs/architecture/representation-artifacts.md">Physical representations</a> ·
  <a href="docs/assets/diagrams/product_pipeline.json">Editable diagram source</a>
</p>

The join resolves a symbolic parameter to its authenticated terminal, encoded
range and layout. It is not a runtime lookup by tensor filename. The resulting
binding retains compiled model plans; deployment specialization adds the
device-specific implementation and resource envelope without rewriting the
artifact's parameter identity.

Qwen, MiniMax and Mamba2 retain native typed programs. DeepSeek retains a
canonical operator schedule with derived attention, MoE and transformer plans;
it is not represented as one uniform full-model SSA program. The
[consumer map](docs/architecture/compiler-ir.md#current-consumer-cutover)
records the actual lowering and execution paths.

<a id="one-host-your-models-independent-sessions"></a>

## Request execution and state publication

![A generation request resolves an engine generation and session; compiled prefill, decode or verification stages candidate state. Accepted-prefix commit publishes state and token fragments; cancellation or failure discards uncommitted work.](docs/assets/diagrams/product_runtime.svg)

<p align="center">
  <a href="docs/architecture/runtime-lifecycle.md">Runtime lifecycle</a> ·
  <a href="docs/architecture/computational-state.md">State ownership</a> ·
  <a href="docs/assets/diagrams/product_runtime.json">Editable diagram source</a>
</p>

Prefill executes input positions not supplied by compatible reused state.
Incremental decode extends the committed sequence; speculative execution first
verifies draft proposals with the target model. Proposed tokens are not output:
the accepted prefix must commit across the participating state providers before
publication. Cancellation preserves already committed progress and discards
uncommitted work. Failed cleanup can invalidate continuity and require reset.

The host routes requests by model and engine generation. Sessions and leases
retain that generation; unload refuses while they own it. Closing a transport
connection is distinct from closing a session, and replacing an engine cannot
make an old session valid against the new generation.

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

For an optional terminal workspace, run `./yvex workbench`: conversation,
Models, Compile, Activity and exact runtime inspection share the same native
owners. [Workbench controls](docs/guides/operator-runbook.md#optional-terminal-workbench).

Weights are acquired separately; `model load` does not download them.
[Acquisition and preparation](docs/guides/quickstart.md)
· [Published representations](docs/guides/model-lifecycle.md#published-representations)

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

## Measured performance

The tables below are generated from immutable qualification receipts, not
manually maintained numbers. They expose the current engineering gap as well
as the demonstrated execution. **Characterization is not quality qualification.**

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

## Inside the computational core

Compilation separates the model's equations from parameter encoding and from
the algorithms that execute them. These contracts constrain which physical
alternatives an optimization may select.

<a id="two-compilation-lanes-one-authenticated-binding"></a>

### Parameter resolution and operation lowering

| Boundary | What becomes explicit | Canonical owner |
| --- | --- | --- |
| **Computation** | Family semantics → typed programs or retained operator schedules → execution dependencies, state and effects | [Compiler IR](docs/architecture/compiler-ir.md) |
| **Parameters** | Source ranges → transformations → quantization/layout → authenticated package terminals | [Weight layouts](docs/architecture/representation-artifacts.md) |
| **Parameter join** | Symbolic operands resolve to exact terminal lineage; physical operations and the compiled plan enter the runtime binding | [Artifact admission](docs/architecture/artifacts-admission.md) |
| **Deployment** | Numerical implementation, backend compatibility, workspace and resource envelope are admitted for the target | [Specialization](docs/architecture/deployment-specialization.md) |

A storage format is not an execution algorithm. A qtype the artifact reader can
decode is not automatically legal for every fused kernel. Physical Execution IR
describes package terminals; the physical computational program describes work
over those terminals. Neither is a resident engine.

### State has geometry. Publication has a transaction.

Attention caches, recurrent state, speculative drafts, RNG and token ledgers do
not share one storage layout. They share coordinated **commit or abort**.
Each session owns its mutable state; engine generations fence stale references.
A captured prefix is tied to execution identity, not interchangeable memory.

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

<a id="active-frontier-goal-driven-physical-compilation"></a>

### Physical optimization: implemented scope and remaining gates

**Program P is in progress.** `compile optimize` exposes bounded, deterministic
candidate planning through the native compiler. Goals constrain hardware,
workload, memory and quality; recipes must have an executable consumer.

| Established distinction | Still to earn |
| --- | --- |
| Static feasibility versus runtime admission | Complete resource and lifecycle qualification for each selected realization |
| Candidate ordering versus measured ranking | An independently qualified, workload-specific recommendation |
| Produced artifact versus installed engine | Explicit deployment and complete-model evidence, never automatic rollout |

The current planner resolves model/workload constraints, synthesizes fixed and
role-allocated parameter policies, and inspects bound computational programs.
The allocation objective maximizes source-retained elements under an encoded
weight budget; that objective is neither a quality metric nor a throughput
prediction. Equivalent input-view lowering removes eligible intermediate
materialization in physical programs. Component qualification of that pass does
not establish a complete-model speedup or a qualified recipe recommendation.

[Current Program P implementation](docs/architecture/compiler-ir.md#goal-constrained-physical-search-partial-implementation)
· [Research target](docs/research/physical-model-compiler.md)
· [Selected delivery](docs/project-control/TASKS.md)

## Evidence you can inspect

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

<a id="go-deeper"></a>

## Technical documentation

| Use YVEX | Build with YVEX | Work on YVEX |
| --- | --- | --- |
| [Quickstart](docs/guides/quickstart.md) | [Architecture](docs/architecture/README.md) | [Contributing](CONTRIBUTING.md) |
| [Model lifecycle](docs/guides/model-lifecycle.md) | [Public contracts](docs/contracts/README.md) | [Build guide](docs/guides/build.md) |
| [Operator reference](docs/reference/README.md) | [Product vision](docs/product/VISION.md) | [Roadmap](ROADMAP.md) |

---

Code is [MIT licensed](LICENSE). [NOTICE](NOTICE.md) explains ownership and
third-party boundaries. Upstream model licenses remain separate.
