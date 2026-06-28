# YVEX Inner Delivery Spine

Date: 2026-06-28
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 1. Authority

`docs/spine.md` is the only internal delivery map. Public docs must not expose
delivery IDs, status ledgers, handoff language, or implementation diary text.

Status changes in this file require implemented behavior, tests, command proof,
failure-path coverage, cleanup/lifecycle behavior, and explicit boundaries. The
spine must not collapse artifact parsing, tensor residency, engine ownership,
graph execution, token input, prefill state, KV, decode, logits, sampling, and
generation into one vague inference milestone.

Git history should tell the technical story in natural subjects. Internal spine
IDs may appear here and in final reports, but commit subjects should describe
behavior, not milestone labels.

## 2. Implementation Doctrine

No scaffold milestone is complete. A file, API shape, command option,
placeholder executor, future hook, or empty provider stub is not an
implementation boundary unless the same wave also provides real behavior that
depends on it.

A completion decision needs all of the following:

```text
implemented behavior
command-visible or API-visible proof
tests
error paths
cleanup/lifecycle behavior
explicit boundary documentation
no unsupported capability claim
```

Normal operator paths should be short, discoverable, and preset-driven where
possible. Long flags belong to diagnostics, gates, provenance, and exact CI
checks. Help should show the short path first and advanced flags later.

DeepSeek selected artifacts are the current live pressure targets. They are
large enough to force real parser, layout, backend, transfer, cleanup, graph,
and failure boundaries. They are not the whole system. Runtime code must remain
model-family-aware, with family mapping and runtime adapters made explicit
rather than hidden behind generic support claims.

## 3. Current Repository State

```text
root-first C source layout
native root binaries: ./yvex and ./yvexd
public headers: include/yvex/
CUDA implementation: cuda/ with C host bridge and CUDA kernel unit
GGUF domain and family mapping: gguf/
docs: docs/api.md, docs/contract.md, docs/operator-runbook.md, docs/spine.md
public README: prose-first runtime boundary, public-safe artifact wording
artifact docs: operator-local paths only, no personal absolute paths
tests: compact runners, fixtures, vectors, and integrity regression harnesses
generated output: build/
local operator state: .yvex/
```

Documentation roles:

```text
README.md: public technical thesis and project overview
docs/api.md: public C API surface, headers, ownership, error/capability map
docs/contract.md: runtime/CLI/filesystem/backend/server behavior contract
docs/operator-runbook.md: command-first operator workflow
MODEL_ARTIFACTS.md: external artifact cards and selected pressure facts
AGENTS.md: repository operating rules
docs/spine.md: internal delivery map
```

## 4. Current Capability

Implemented and audited:

```text
GGUF inspection and tensor table parsing
tensor/model descriptor construction
tokenizer fixture path and prompt diagnostics
graph/planner substrate
CPU backend selected materialization
CUDA probe, tensor movement, kernel unit, and parity subset
controlled GGUF emission
selected-tensor GGUF emission
DeepSeek selected embedding alias and selected materialization
model gate and materialization gate diagnostics
local model registry and alias resolution
qtype support separated by policy/storage/emit/quantize/compute
server/provider status shell
engine-owned selected materialized weight attachment
session visibility into engine-attached weight state
deterministic controlled F32 fixture graph execution
real selected F16 token embedding partial graph execution
real selected embedding plus RMSNorm graph segment
explicit prompt/token input boundary
segment-summary prefill state foundation from validated token sequences
minimal session-owned KV ownership and append/read boundary
minimal KV-backed prefill binding from segment-summary state
standalone RoPE/position graph op boundary
standalone F32 attention primitive boundary
artifact integrity validator and corruption fixture suite
file identity digest enforcement
registry metadata drift diagnostics
canonical tensor byte-range validation
canonical tensor shape/dtype accounting
materialization integrity gate
graph execution integrity guard
consolidated artifact integrity regression harness
operator integrity report
```

Current live targets:

```text
alias: deepseek4-v4-flash-selected-embed
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU materialization: pass
CUDA materialization: pass
execution_ready: false

alias: deepseek4-v4-flash-selected-embed-rmsnorm
tensors: token_embd.weight, blk.0.attn_norm.weight
token_embd.weight dims: [4096,129280]
blk.0.attn_norm.weight dims: [4096]
dtype: F16
tensor_bytes: 1059069952
CPU segment execution: pass
CUDA segment execution: pass
execution_ready: false
```

Unsupported / not advanced:

```text
full model execution
full DeepSeek materialization
full GGUF conversion
full supply-chain security
full transformer prefill
decode
logits-producing runtime path
sampling
generation
interactive generation
provider generation endpoint
OpenAI-compatible generation
Anthropic-compatible generation
evaluation suite
inference benchmarks
benchmark performance
advanced Runtime KV capacity
execution_ready: true
```

M8 is not the final prefill path. It is a segment-summary prefill-state
foundation built from validated token input and implemented selected graph
segments. PREFILL.1 binds that summary to minimal session-owned KV state with
diagnostic values. Full transformer prefill still requires graph op expansion,
first block execution, layer scheduling, and real attention K/V projection.

## 5. Unified Delivery Ledger

The table is the spine. Every delivery row is recorded here; sections after the
table explain dependency logic, validation, and rules without duplicating status
tables.

| ID | Status | Area | Title | Completion boundary |
| --- | --- | --- | --- | --- |
| P0 | complete | foundation | Repository reset and technical spine | root project shape and internal delivery authority established |
| A0 | complete | foundation | Core C skeleton and public status API | root C binary, status surface, and public skeleton compile |
| B0 | complete | foundation | Runtime filesystem and artifact path layer | filesystem helpers and artifact path behavior implemented |
| C0 | complete | gguf | GGUF parser and tensor directory | GGUF metadata and tensor directory parse with failures |
| C1 | complete | gguf | Tensor/model descriptor layer | parsed tensor facts become model descriptors |
| D0 | complete | tokenizer | Tokenizer and prompt diagnostics | tokenizer fixture path and prompt diagnostics exist |
| E0 | complete | graph | Graph and planner substrate | graph/planner structures and diagnostics compile and test |
| F0 | complete | backend | CPU backend and tensor movement | CPU tensor allocation/movement boundary implemented |
| G0 | complete | session | Runtime/session shell | engine/session lifecycle shell exists |
| H0 | complete | cli | CLI diagnostics surface | repository-local CLI exposes diagnostic commands |
| I0 | complete | cli | Chat diagnostic shell | diagnostic chat shell reports unsupported generation honestly |
| J0 | complete | cli | Metrics, traces, and run artifacts | metrics/trace/profile/run-artifact diagnostics exist |
| K0 | complete | server | Server/provider status shell | yvexd health/metrics/models status shell exists |
| L0 | complete | backend | CUDA backend probe and tensor movement | CUDA probe, allocation, movement, and parity subset implemented |
| OWI.0 | complete | intake | Open weight intake and GGUF toolchain spine | source-to-GGUF intake flow mapped |
| OWI.1 | complete | intake | Source manifest provenance contract | source manifest command and contract implemented |
| OWI.2 | complete | intake | Safetensors native weight inventory | native tensor inventory command implemented |
| OWI.3 | complete | intake | GGUF template contract validator | template validation surface implemented |
| OWI.4 | complete | intake | Tensor mapping adapter contract | model-family tensor mapping implemented |
| OWI.5 | complete | intake | Quantization policy manifest | qtype policy manifest validation implemented |
| OWI.6 | complete | intake | Calibration imatrix contract | imatrix manifest validation implemented |
| OWI.7 | complete | intake | Controlled GGUF emission roundtrip | tiny controlled GGUF emission and parse proof exists |
| OWI.8 | complete | intake | Open weight conversion bridge | selected conversion planning/bridge exists |
| OWI.9 | complete | intake | DeepSeek quantization job bridge | DeepSeek quant job bridge and validation exist |
| OWI.FINAL.0 | complete | intake | Open-weight intake closeout | intake closeout docs, tests, and guardrails complete |
| ARTIFACT.NAMING.0 | complete | artifact | GGUF artifact naming contract | canonical artifact alias/name rules implemented |
| RUNTIME.KV.0 | complete | kv-policy | KV cache policy | KV policy documented without runtime claim |
| M1 | complete | runtime | Real model conversion/materialization gate | selected real artifact gate and materialization proof exists |
| M2 | complete | runtime | DeepSeek materialization hardening | selected DeepSeek materialization hardened on CPU/CUDA |
| CLI.PACKAGE.0 | complete | cli | Repository CLI packaging baseline | root binaries and CLI packaging baseline established |
| CLI.PACKAGE.1 | complete | cli | Minimal compiled-binary packaging baseline | compiled root binaries remain repository-local and validated |
| CLI.CONSOLE.0 | complete | cli | CLI interface doctrine | CLI claim and command-output doctrine established |
| CLI.MODELS.0 | complete | cli | Local model selection spine | model selection design and registry shape established |
| CLI.MODELS.1 | complete | cli | Local model registry implementation | local model registry add/list/use/current/remove works |
| CLI.MODELS.2 | complete | cli | One-shot model alias resolution | model-taking commands resolve aliases or paths |
| CLI.MODELS.3 | complete | cli | Model selection in canonical REPL | chat/console uses selected model state where allowed |
| CLI.MODELS.4 | complete | cli | Model alias resolution in yvexd | daemon resolves explicit model aliases |
| REPO.OPERATING.0 | complete | docs | Operating handbook and artifact cards | AGENTS and artifact cards define repository rules |
| REPO.LAYOUT.1 | complete | layout | Root-first C source layout collapse | source layout collapsed to root-first style |
| REPO.LAYOUT.2 | complete | layout | Root source compression and native root binaries | native root binaries and source compression complete |
| REPO.SURFACE.0 | complete | layout | CUDA, GGUF, model family, and test surface refoundation | core source surfaces refounded and tested |
| REPO.SURFACE.1 | complete | layout | Natural C surface and code style refoundation | root C surface made natural and guarded |
| CUDA.SURFACE.0 | complete | backend | CUDA kernel translation unit | CUDA kernels live in CUDA translation unit |
| CODE.NATURAL.0 | complete | layout | Natural translation unit rewrite | translation units rewritten to project style |
| CODE.NATURAL.1 | complete | layout | Final translation unit hygiene pass | final source hygiene pass complete |
| TEST.SURFACE.0 | complete | test | Test vectors and runner consolidation | compact test runners and vectors consolidated |
| DOCS.PUBLIC.0 | complete | docs | Public documentation boundary cleanup | public docs cleaned of internal delivery leakage |
| DOCS.MIN.0 | complete | docs | Minimal documentation surface | canonical docs surface established |
| DOCS.OPERATOR.RUNBOOK.0 | complete | docs | Canonical operator runbook | command-first runbook exists |
| SPINE.REBASE.1 | complete | docs | Execution-chain audit and M3-M8 technical rebase | runtime ladder through M8 mapped |
| SPINE.REBASE.2 | complete | docs | Runtime track rebase before M3 | M3/M4 path clarified before implementation |
| SPINE.REBASE.3 | complete | docs | End-to-end runtime and operator roadmap | runtime/operator roadmap expanded |
| SPINE.REBASE.4 | complete | docs | Artifact integrity and measurement target rebase | artifact integrity module added before graph expansion |
| ARTIFACT.INTEGRITY.0 | complete | artifact | Artifact integrity threat model and validator baseline | baseline GGUF validator and required tensor readiness exist |
| ARTIFACT.INTEGRITY.1 | complete | artifact | File identity and digest enforcement | local file size/SHA-256 identity recorded and enforced |
| ARTIFACT.INTEGRITY.2 | complete | artifact | GGUF structural corruption fixture suite | tiny corrupt GGUF corpus and refusal harness exist |
| ARTIFACT.INTEGRITY.3 | complete | artifact | Tensor directory offset and byte-range validation | canonical tensor range validator gates reads |
| ARTIFACT.INTEGRITY.4 | complete | artifact | Shape, rank, dtype, and byte-count overflow hardening | canonical shape/dtype accounting feeds range validation |
| ARTIFACT.INTEGRITY.5 | complete | artifact | Registry alias metadata drift diagnostics | registry metadata summary compare and drift reports exist |
| ARTIFACT.INTEGRITY.6 | complete | artifact | Materialization integrity gate | materialization preflight, phase, and cleanup reporting exist |
| ARTIFACT.INTEGRITY.7 | complete | artifact | Graph execution integrity guard | graph preflight, dispatch/reference guards, and cleanup reports exist |
| ARTIFACT.INTEGRITY.8 | complete | artifact | Corrupt artifact regression harness | consolidated integrity regression matrix exists |
| ARTIFACT.INTEGRITY.9 | complete | artifact | Operator integrity report and doctor integration | operator-facing integrity report aggregates existing checks |
| ARTIFACT.INTEGRITY.FINAL.0 | complete | artifact | Artifact integrity closeout before graph expansion | integrity module closed before broader graph work |
| M3 | complete | runtime | Materialized-weight engine attachment | selected backend-resident weights attach to engine/session ownership |
| M4 | complete | graph | First executable fixture graph path | controlled F32 fixture graph executes on CPU/CUDA where available |
| M5 | complete | graph | First real-model partial graph execution | selected F16 token_embd.weight participates in scheduled graph work |
| M6 | complete | graph | Real-model graph segment expansion | embedding-plus-RMSNorm segment executes over multiple real tensors |
| M7 | complete | input | Prompt/token input boundary | validated token sequences route into implemented graph paths |
| M8 | complete | prefill | Prefill state foundation | segment-summary prefill state created from validated token sequence |
| SPINE.REBASE.5 | complete | docs | Unified full inference engine spine | all delivery rows consolidated into one ledger and dependency map |
| M9 | complete | kv | Minimal KV ownership and append/read boundary | session-owned KV shape, allocation, append/read, lifecycle, cleanup, and context overflow behavior |
| PREFILL.1 | complete | prefill | KV-backed prefill state binding | M8 segment-summary state connects to minimal KV ownership without decode/logits claim |
| GRAPH.OPS.0 | complete | graph | RoPE and position operation boundary | position-dependent graph op implemented with tests and backend rules |
| GRAPH.OPS.1 | complete | graph | Attention primitive boundary | attention inputs, masks, scratch, backend dispatch, and failure paths implemented |
| GRAPH.OPS.2 | next | graph | Projection and matmul primitive boundary | matmul/projection path implemented with dtype/qtype/backend limits |
| GRAPH.OPS.3 | planned | graph | MLP and routed-expert primitive boundary | feed-forward or routed expert slice implemented with explicit tensor roles and backend support |
| GRAPH.BLOCK.0 | planned | graph | First transformer block execution | one block executes through normalization, attention, residual, MLP path with owned scratch |
| GRAPH.LAYERS.0 | planned | graph | Layer scheduler and repeated block execution | scheduler can run repeated blocks over token positions with cleanup and failure reporting |
| PREFILL.2 | planned | prefill | First real transformer prefill path | validated prompt tokens run through implemented layer path into KV-backed prefill state |
| PREFILL.3 | planned | prefill | Chunked prefill and scratch lifecycle | chunked token ranges, scratch reuse, cleanup, and context-boundary behavior implemented |
| PREFILL.4 | planned | prefill | Prefill diagnostics and regression reports | prefill positions, memory, KV rows, checksums, and failure phases visible |
| PREFILL.5 | planned | prefill | Prefill throughput measurement gate | benchmarkable prefill command exists, but only after real transformer prefill path |
| M10 | planned | decode | Decode step over existing runtime state | one decode step advances existing KV-backed state by one position |
| DECODE.1 | planned | decode | Decode lifecycle and repeatability | repeat decode, interrupt, cleanup, and context-end behavior tested |
| M11 | planned | logits | Logits production boundary | logits buffer ownership, dtype, backend tolerance, and diagnostics implemented |
| LOGITS.1 | planned | logits | Logprob/top-k diagnostics | dump-logits or logprob diagnostic path implemented over real logits buffer |
| M12 | planned | logits | Deterministic logits regression | stable vector tests for logits with model/artifact identity and backend tolerance |
| M13 | planned | sampling | Sampling boundary | greedy and stochastic sampling over logits with seed and parameter validation |
| SAMPLING.1 | planned | sampling | Sampling diagnostics and reproducibility | sampling reports seed, parameters, stop reason, and failure behavior |
| M14 | planned | generation | First constrained generation loop | decode -> logits -> sample -> append token loop with stop conditions and token accounting |
| GEN.LOOP.1 | planned | generation | Generation state and interruption | interruption, cleanup, trace, and partial-output behavior tested |
| M15 | planned | cli | Interactive CLI generation path | CLI/REPL generation uses real runtime generation loop |
| M16 | planned | server | Provider/server generation boundary | daemon/server generation uses runtime-backed generation path |
| M17 | planned | profile | Trace/profile hardening for generation | traces and profiles identify artifact/backend/graph/KV/decode/logits/sampling/server failures |
| FULLMODEL.0 | planned | model | Full model inventory and placement plan | full artifact tensor inventory, memory budget, and backend placement report |
| FULLMODEL.1 | planned | model | Full model materialization plan | selected-family full tensor placement and materialization preflight without generation claim |
| FULLMODEL.2 | planned | model | Full model materialization proof | full required tensor set materializes or fails with phase/cleanup reports |
| FULLMODEL.3 | planned | model | Full model runtime descriptor | model descriptor covers all tensors needed by prefill/decode/logits path |
| FAMILY.RUNTIME.0 | planned | model | Runtime family adapter boundary | model-family-specific tensor roles and graph requirements exposed as reports |
| KV.MIN.0 | planned | kv | Minimal KV shape and ownership | session-owned KV shape, allocation, and release |
| KV.MIN.1 | planned | kv | KV append/read boundary | append/read by layer/head/position with checks and diagnostics |
| KV.MIN.2 | planned | kv | KV lifecycle in session state | save/clear/reinit behavior inside session lifecycle |
| KV.MIN.3 | planned | kv | KV diagnostics and failure reporting | KV size, position range, overflow, and cleanup reports |
| RUNTIME.KV.1 | planned | kv-capacity | Static KV size estimator | memory estimator by context, layers, dtype/qtype, backend |
| RUNTIME.KV.2 | planned | kv-capacity | CUDA KV allocation proof | CUDA KV allocation and cleanup behavior |
| RUNTIME.KV.3 | planned | kv-capacity | Paged KV allocator skeleton | page table and ownership boundary without performance claim |
| RUNTIME.KV.4 | planned | kv-capacity | Host spill and cold-cache experiments | host/disk spill experiments behind explicit unsupported boundary |
| RUNTIME.KV.5 | planned | kv-capacity | KV quantization policy | KV qtype policy and diagnostics, no compute claim unless implemented |
| MODEL.LIFECYCLE.0 | planned | model-ux | Unified model status report | concise model status over registry, artifact, backend, qtype, graph readiness |
| MODEL.LIFECYCLE.1 | planned | model-ux | Model prepare preset | wraps registry checks, integrity, backend checks, materialization, and reports |
| MODEL.LIFECYCLE.2 | planned | model-ux | Model check preset | operator-friendly artifact/backend/qtype/CUDA check |
| MODEL.LIFECYCLE.3 | planned | model-ux | Model doctor flow | common local artifact, registry, backend, CUDA, and command failures diagnosed |
| MODEL.LIFECYCLE.4 | planned | model-ux | Model-class profile | memory and artifact profile for large local models |
| MODEL.LIFECYCLE.5 | planned | model-ux | Artifact cache and report hygiene | generated reports/cache/logs managed outside git |
| CLI.UX.0 | planned | cli | Command taxonomy and help layout | short path first, advanced diagnostics later |
| CLI.UX.1 | planned | cli | Concise normal-path commands | one-line presets over low-level gates |
| CLI.UX.2 | planned | cli | Colorized terminal output and severity | pass/warn/fail/unsupported with NO_COLOR and non-TTY fallback |
| CLI.UX.3 | planned | cli | Structured output modes | stable JSON/text output for scripts without color pollution |
| CLI.UX.4 | planned | cli | Interactive REPL line editing and history | usable local REPL shell |
| CLI.UX.5 | planned | cli | REPL slash-command cleanup | discoverable slash commands and help |
| CLI.UX.6 | planned | cli | Doctor command | environment, registry, backend, CUDA, artifact, and runtime checks |
| CLI.UX.7 | planned | cli | Operator profiles | workstation, CUDA, future large hardware profiles |
| CLI.UX.8 | planned | cli | One-line command recipes | normal paths documented and command-visible |
| SERVER.RUNTIME.0 | planned | server | Runtime-backed model state in daemon | daemon reflects real runtime state, not only status shell |
| SERVER.RUNTIME.1 | planned | server | Daemon execution-state diagnostics | server reports model/artifact/backend/runtime readiness without generation |
| SERVER.GEN.0 | planned | server | First provider generation endpoint | provider endpoint backed by real constrained generation loop |
| SERVER.API.0 | planned | server | OpenAI-compatible API boundary | compatibility only after runtime generation exists |
| SERVER.API.1 | planned | server | Anthropic-compatible API boundary | compatibility only after runtime generation exists |
| SERVER.STREAM.0 | planned | server | Streaming response boundary | stream tokens from real generation loop |
| SERVER.OBS.0 | planned | server | Server traces and metrics | request/runtime failure reports and metrics |
| EVAL.FIXTURE.0 | planned | eval | Fixture graph correctness vectors | exact fixture output regression |
| EVAL.PARTIAL.0 | planned | eval | Real partial graph regression vectors | selected embedding/segment checksum and max-diff vectors |
| EVAL.PREFILL.0 | planned | eval | Prefill state regression | token positions, prefill summaries, KV readiness and failure states |
| EVAL.KV.0 | planned | eval | KV append/read correctness vectors | KV shape, append/read, overflow, cleanup vectors |
| EVAL.DECODE.0 | planned | eval | Decode-step regression | one-step decode over existing KV state |
| EVAL.LOGITS.0 | planned | eval | Deterministic logits regression | logits vectors with backend tolerance and artifact identity |
| EVAL.SAMPLING.0 | planned | eval | Sampling determinism checks | greedy/seeded stochastic sampling checks |
| EVAL.GEN.0 | planned | eval | Constrained generation smoke/eval | small generation cases through the same runtime users run |
| EVAL.CAPABILITY.0 | planned | eval | Capability regression suite | curated task suite after generation path exists |
| BENCH.PREFILL.0 | planned | bench | Prefill throughput benchmark | token interval throughput over real transformer prefill |
| BENCH.DECODE.0 | planned | bench | Decode throughput benchmark | generated-token decode speed over existing state |
| BENCH.GEN.0 | planned | bench | Generation throughput benchmark | prompt plus generated-token throughput |
| BENCH.MEMORY.0 | planned | bench | Runtime memory pressure benchmark | model, scratch, KV, backend memory pressure |
| BENCH.SERVER.0 | planned | bench | Provider latency benchmark | request, queue, streaming latency after server generation |
| BENCH.RUNTIME.0 | planned | bench | End-to-end runtime benchmark harness | reproducible machine/backend/artifact/qtype/context command harness |
| BACKEND.PROFILE.0 | planned | backend | CPU correctness profile | reference/debug profile and limitations |
| BACKEND.PROFILE.1 | planned | backend | Local CUDA workstation profile | CUDA workstation capability and memory report |
| BACKEND.PROFILE.2 | planned | backend | DGX Spark / GB10 profile | CUDA-class hardware pressure profile |
| BACKEND.PROFILE.3 | planned | backend | Large-memory future hardware profile | high-memory machine planning profile |
| BACKEND.PROFILE.4 | planned | backend | Backend failure and memory pressure reports | allocation/transfer/op failure diagnostics |
| BACKEND.PROFILE.5 | planned | backend | Backend capability matrix | op and memory capability matrix |
| BACKEND.METAL.0 | planned | backend | Metal feasibility profile | future lane only, no support claim |
| BACKEND.ROCM.0 | planned | backend | ROCm/Strix Halo feasibility profile | future lane only, no support claim |
| LAYOUT.RUNTIME.0 | planned | layout | Runtime module boundary audit | runtime ownership separated from CLI/server glue |
| LAYOUT.GRAPH.0 | planned | layout | Graph/executor module separation | graph scheduler/executor boundaries clarified |
| LAYOUT.CLI.0 | planned | layout | CLI command taxonomy cleanup | command parsing organized by domain |
| LAYOUT.SERVER.0 | planned | layout | Server/runtime boundary cleanup | daemon does not duplicate CLI runtime wiring |
| LAYOUT.TEST.0 | planned | layout | Test fixture/eval/bench separation | parser fixtures, runtime fixtures, eval vectors, benches separated |
| LAYOUT.DOCS.0 | planned | layout | Public/internal docs boundary cleanup | public docs stay product-facing; spine remains internal |
| LAYOUT.BUILD.0 | planned | layout | Build targets by backend/profile | build layout reflects CPU/CUDA/future backend profiles |
| DOCS.README.2 | planned | docs | README inference vision refresh | public README reflects engine vision without internal delivery IDs |
| DOCS.RUNBOOK.1 | planned | docs | Generic model-class operator flow | runbook no longer only selected-artifact/deepseek-heavy |
| DOCS.RUNBOOK.2 | planned | docs | Reduced-flag normal path | operator presets documented separately from debug flags |
| DOCS.RUNBOOK.3 | planned | docs | Failure-mode cookbook | common refusal/failure states documented |
| DOCS.RUNBOOK.4 | planned | docs | Verticalized operator runbook | normal/debug/intake/CI/large-model paths separated |
| DOCS.CONTRACT.1 | planned | docs | Runtime contract after generation path matures | public behavior contract updated after runtime generation |
| DOCS.API.1 | planned | docs | Public API map after inference maturity | API docs updated after KV/decode/logits/generation surfaces mature |
| DOCS.PUBLIC.1 | planned | docs | Public docs without internal delivery IDs | public docs remain free of spine IDs |
| DOCS.README.TARGETS.0 | planned | docs | README measurement target table after benchmark harness | public speed/eval table updated only after harness exists |
| DOCS.DIAGRAMS.0 | planned | docs | Artifact-to-runtime boundary diagram | diagram explains artifact identity through engine ownership |
| DOCS.DIAGRAMS.1 | planned | docs | Operator path diagram | diagram explains normal/debug/intake paths |
| DOCS.DIAGRAMS.2 | planned | docs | Runtime ladder diagram | diagram explains transformer execution path |
| DOCS.DIAGRAMS.3 | planned | docs | Integrity/refusal diagram | diagram explains corruption and drift refusal |
| DOCS.DIAGRAMS.4 | planned | docs | Eval/bench measurement map | diagram maps measurements to runtime boundaries |
| DOCS.DIAGRAMS.5 | planned | docs | Backend/hardware target matrix | diagram explains CPU/CUDA/future lanes |
| DOCS.DIAGRAMS.6 | planned | docs | README visual integration | diagrams integrated only where they clarify real boundaries |

## 6. Dependency Map

```text
artifact/source evidence
  -> artifact identity/integrity
  -> selected/full model tensor mapping
  -> materialization/backend residency
  -> engine/session ownership
  -> controlled graph proof
  -> selected real graph segment proof
  -> graph op expansion
  -> first transformer block
  -> layer scheduler
  -> token input
  -> prefill state foundation
  -> minimal KV ownership
  -> minimal KV-backed prefill binding
  -> KV-backed transformer prefill
  -> decode
  -> logits
  -> logits regression
  -> sampling
  -> constrained generation
  -> CLI generation
  -> provider generation
  -> eval/bench/profile hardening
```

M8 is not the final prefill path. It is the first prefill-state foundation.
PREFILL.1 binds that foundation to minimal session-owned KV state, but it does
not compute real attention K/V. A full transformer run still requires graph op
expansion, transformer block execution, layer scheduling, real transformer
prefill, decode, logits, sampling, and a generation loop.

The current executable graph slices are controlled fixture embedding, selected
real embedding, and selected real embedding-plus-RMSNorm. Those slices prove
parser, materialization, backend, graph scheduling, reference comparison, and
cleanup boundaries. They do not prove full model materialization, transformer
attention, MLP, routed experts, logits, sampling, or generation.

The standalone RoPE operation proves a position-dependent F32 graph op over a
small deterministic vector on CPU and CUDA where available. It validates
head_dim parity, position, byte accounting, backend op support, output
allocation, dispatch, reference comparison, cleanup, checksum, and max-diff
reporting. It does not prove attention, QKV projection, transformer block
execution, layer scheduling, decode, logits, sampling, or generation.

The standalone attention primitive proves explicit F32 Q/K/V scaled dot-product
attention for one query over a bounded key/value prefix on CPU and CUDA where
available. It validates `seq_len`, position bounds, head dimension, causal mask,
scratch sizing, backend op support, output allocation, dispatch, reference
comparison, cleanup, checksum, softmax diff, and max-diff reporting. It does not
project Q/K/V from model tensors, write real KV cache rows, execute a
transformer block, schedule layers, run full transformer prefill, decode,
produce logits, sample, or generate.

Full model materialization and placement are explicit planned work because the
runtime must inventory and place the complete required tensor set before a real
transformer path can rely on it. Decode cannot be meaningful until graph/layer
execution and real transformer prefill create runtime state worth advancing.

Evaluation must follow implemented runtime boundaries. Fixture eval is not model
quality. Logits regression is not capability eval. Capability eval starts only
when the same generation path users run exists.

Benchmarks must follow implemented runtime paths. No token/sec number is valid
without the measured command, machine, backend, artifact identity, qtype,
context, and reproducibility note.

Metal and ROCm rows are feasibility lanes only. They do not claim implemented
backend support.

## 7. Active Next

```text
GRAPH.OPS.2 - Projection and matmul primitive boundary
```

Next implementation: GRAPH.OPS.2. It must introduce the projection/matmul
primitive boundary with explicit dtype/qtype/backend limits, shape accounting,
scratch/output ownership, dispatch, reference comparison, failure paths, and
cleanup behavior. It must not claim transformer block execution, layer
scheduling, full transformer prefill, decode, logits, sampling, generation,
server generation, evaluation, or benchmark readiness.

After PREFILL.1, the next runtime work is not automatically decode. The spine
expects graph/layer expansion rows to determine whether decode can run over
meaningful transformer state.

## 8. Validation Gate

Baseline:

```sh
make clean
make check
make smoke
make check-cuda
git diff --check
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

Current command-surface audit:

```sh
./yvex commands
./yvex help graph
./yvex help input
./yvex help prefill
./yvex help engine
./yvex help session
./yvex help integrity
./yvex help models
```

Selected artifact closeout proof set:

```sh
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cuda --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cuda --execute-partial --partial-token 0
```

Selected segment proof set:

```sh
./yvex models verify deepseek4-v4-flash-selected-embed-rmsnorm
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

Standalone graph op proof set:

```sh
./yvex graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cuda --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cuda --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
```

Spine structure proof:

```sh
grep -nF 'CLI.UX.''*' docs/spine.md && exit 1 || true
grep -nF 'SERVER.''*' docs/spine.md && exit 1 || true
grep -nF 'EVAL.''*' docs/spine.md && exit 1 || true
grep -nF 'BENCH.''*' docs/spine.md && exit 1 || true
grep -nF 'KV.MIN.''*' docs/spine.md && exit 1 || true
grep -nF 'RUNTIME.KV.''*' docs/spine.md && exit 1 || true
grep -nF 'BACKEND.PROFILE.''*' docs/spine.md && exit 1 || true
grep -nF 'LAYOUT.''*' docs/spine.md && exit 1 || true
grep -nF 'DOCS.''*' docs/spine.md && exit 1 || true

grep -nF 'PREFILL.1' docs/spine.md
grep -nF 'GRAPH.OPS.0' docs/spine.md
grep -nF 'GRAPH.BLOCK.0' docs/spine.md
grep -nF 'GRAPH.LAYERS.0' docs/spine.md
grep -nF 'FULLMODEL.0' docs/spine.md
grep -nF 'DECODE.1' docs/spine.md
grep -nF 'LOGITS.1' docs/spine.md
grep -nF 'BENCH.GEN.0' docs/spine.md
grep -nF 'BACKEND.METAL.0' docs/spine.md
grep -nF 'BACKEND.ROCM.0' docs/spine.md
```

Additional guardrails:

```text
public docs boundary
artifact guardrail
forbidden external reference guardrail
claim guardrail
local registry guardrail
public path leak guardrail
no runtime file change for spine-only rebases
```

## 9. Non-Negotiable Rules

- No support claim without code, tests, and command proof.
- No generated model artifacts in git.
- No personal absolute artifact paths in public docs.
- No internal delivery IDs outside `docs/spine.md`.
- No inference or generation claim until implemented.
- No benchmark claim without benchmark implementation and proof.
- No status promotion without command proof from the validation/audit gate.
- No M-series completion status until the relevant runtime state exists in code
  and tests.
- No scaffold milestone completion.
- No commit subject should use the internal delivery ID as the primary story.
- No long-flag operator flow may remain the only normal path once concise
  presets exist.
- Do not collapse materialization, engine ownership, graph execution, prompt
  input, prefill, KV, decode, logits, sampling, generation, CLI generation, and
  server generation into one wave.
- No decode milestone may be promoted before KV-backed prefill and relevant
  graph/layer execution exist.
- No logits milestone may be promoted before an actual runtime path produces a
  logits buffer.
- No sampling milestone may be promoted before logits exist.
- No generation milestone may be promoted before decode, logits, and sampling
  are integrated.
- No CLI or server generation surface before the lower runtime generation loop
  exists.
- No provider generation milestone may be promoted before CLI/runtime generation
  exists.
- No provider compatibility claim before server generation exists and is tested.
- No benchmark milestone may be promoted before the measured runtime path exists.
- No benchmark number without model artifact, backend, quant, context, machine,
  command, and reproducibility note.
- No model-family support claim without artifact, mapping, runtime path, and
  tests.
- No graph execution path may trust tensor offset, byte range, shape, dtype, or
  element count without validation.
- No artifact identity claim without digest or equivalent gate evidence.
- No registry alias should be treated as stable identity if the underlying file
  changed.
- No corruption fixture may be committed if it is a real model artifact; corrupt
  fixtures must be tiny.
- No integrity report may imply supply-chain security beyond the checks actually
  implemented.
- No wildcard delivery rows in the unified ledger.
- No secondary track tables duplicating the unified ledger.
- Full transformer execution requires graph op expansion, transformer block
  execution, layer scheduling, KV-backed prefill, decode, logits, sampling, and
  generation.
- Do not claim advanced Runtime KV capacity work until the relevant allocator,
  estimator, spill, or quantization behavior exists in code and tests.
- No docs sprawl beyond `docs/api.md`, `docs/contract.md`,
  `docs/operator-runbook.md`, and `docs/spine.md`.
- Keep DeepSeek as the active live model target unless this spine changes.
- Keep Qwen as historical validation evidence unless this spine changes.
