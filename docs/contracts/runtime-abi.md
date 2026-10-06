<!-- docs:metadata
title: Internal Runtime ABI
id: yvex.contracts.runtime-abi
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Internal Runtime ABI

**Exact internal engine, session, execution and hosted-turn records.**

[Up](README.md)

## Common Internal Runtime

The model engine and execution session are deliberately non-installed
contracts consumed through `<yvex/internal/runtime.h>` by the operator binary.
The internal runtime is family-neutral. Its main objects are:

| Object | Ownership |
| --- | --- |
| `yvex_runtime_binding` | immutable content-addressed package bridge from an admitted artifact to compiled model/operator truth |
| `yvex_family_compiler_adapter` | compilation-only family projection that seals policy into the binding and never enters runtime model state |
| `yvex_model_engine` | one opened package generation with mappings, imported plans, backend specializations, engine resources, scheduler, and attached sessions |
| engine specialization | process-local mapping from PEIR package decisions to implementation classes admitted by one backend/device |
| `yvex_runtime_execution_session` | mutable backend context, reusable workspace, committed target state, bounded speculative candidate state, cancellation and CUDA Graph registry |
| execution profile | generation-bound selection of one engine specialization, workload, kernel bundle, mode, evidence class, and typed operation resolutions |

Model-execution descriptor schema v2 is a non-installed fieldwise projection
of source/family context, optional attention geometry, MoE, output, DSpark and
state facts. Runtime binding v17 persists and authenticates it together with the canonical
operator graph and identity, Physical Execution IR v5 package decisions,
compiled model plan, and pointer-free tokenizer/conversation policy. The
explicit v14/v15/v16 readers authenticate legacy bytes and normalize only canonical
package records to PEIR v5; an unsupported legacy derived-layout requirement
refuses. Bindings v7 through v13 remain explicit rebuild boundaries, and a
binding without the retained operator schedule must be rebuilt rather than
receiving an inferred topology.
Hardware-profile,
workload-profile, capacity-plan and phase-roofline schemas begin at v1 as
internal contracts. Server options schema v5 owns host/listener policy and the
context-aware model-loader callback independently from
`yvex_server_engine_options`; engine schema v2 separates
engine kind from text execution strategy while retaining alias, package,
backend, capacity, memory, and generation facts. Engine schema v1 is refused
before the added fields are read.
The source-authored conversation boundary admits provider request/wire schema
v4, tokenizer plan v5, tokenizer provider result v2, and local protocol v25.
Runtime event schema v6, generation plan schema v7, and generation result
schema v5 are current. Generation plan ABI v5 added the workload-profile identity
required to bind phase evidence to the compiled workload. Generation result
schema v5 adds the identity-bearing committed-token extent of a
source-output-channel boundary; the target-only continuation extent is derived
from the final committed extent. Runtime event schema v5 projects those facts,
engine lifecycle, rolling committed progress and aggregate telemetry pressure
without serializing the C result layout.

Phase-roofline v1 accepts both its original complete record and an additive
availability mask. A zero mask retains the original all-facts meaning; new
writers mark measured facts explicitly. The ledger retains all seven phase
slots while reporting measured, missing and rooflined masks, so absence is
never projected as a zero measurement. This does not earn a schema bump: the
contract is non-persisted, non-wire, rebuilt with every binary, and has no old
binary reader. Duration and work remain mandatory for every admitted record.
The graph execution owner also accumulates repeated phase deltas with checked
arithmetic and refuses any availability change within one phase; generation
does not keep a second accumulation policy.

The internal generation result carries an optional phase ledger. The result
layout is rebuilt with every product binary, is not serialized as C object
memory, and remains excluded from semantic generation identity. Generation
plan ABI v5 binds the ledger's workload-profile identity instead of comparing
it with the distinct per-request profiling identity. Result validation checks
that like-for-like identity and the availability masks when a ledger is
present; old internal results with no ledger retain their zero-initialized
meaning. The phase-ledger change did not alter the wire or event schema.

CUDA producers currently expose exact active-weight and launch facts for
target-only prefill and decode together with their exact H2D/D2H/D2D movement
and synchronization facts. Output projection exposes exact active-weight,
activation, temporary, launch, movement and synchronization facts, including
its bounded CUDA status transfer without treating that transfer as full-
vocabulary D2H. Compatible width-N rows contribute one aggregate output-head
execution instead of multiplying that work into each logical row. Draft and
verification sweeps merge that aggregate once with their transformer facts.
Their transformer active weight, state, activation, temporary and occupancy
stay explicitly unavailable; a partial record is not a complete roofline.

For production evidence, CUDA attention publishes core and envelope activations
through the existing caller-owned device output and does not materialize a
duplicate host row. The publication remains semantically identity-bound;
audit and forensic evidence continue to carry numerical host rows and their
checksums. Persistent attention state still follows the existing host-visible
candidate and residency transaction, so this change neither claims nor changes
the state-provider ABI.

The non-installed CUDA graph execution ABI accepts explicit timing,
session-stream and deferred-completion flags. Production attention pieces
borrow the session stream and return before a graph-local wait; their existing
layer publication barrier owns completion. Audit timing uses an isolated stream
and completes immediately. Launch-graph v3 and graph-executable v2 identities
bind that stream policy. No installed declaration, persisted model contract,
protocol or compiled-profile schema changed.

Accepted-prefix selection returns a v1 in-process physical-facts record. It
derives CUDA H2D and synchronization deltas from the session-owned state
residency counters and states the zero D2H, D2D and kernel facts explicitly.
This changed one internal source signature rebuilt with the product; it did not
change the state-provider ABI, generation ABI, protocol or persisted state.
Repeated phase occupancy, once supplied, is a checked work-unit-weighted mean;
failed accumulation leaves the prior measurement unchanged.

The binding is generated transactionally outside the repository, named by its
content identity and independently reopened. Runtime open validates it against
the exact admitted artifact. Runtime execution does not read source headers or
payloads and does not rebuild Transformation IR, quantization plans or GGUF
writer plans.

A descriptor-bearing binding is emitted as v8 beside existing v7 data. Old v7
writers cannot represent model geometry; v7 readers reject v8 as expected.
The rebuilt reader admits both versions and refuses malformed, truncated or
identity-mismatched descriptor payloads.

One first runtime-model admission performs a complete artifact hash and one
GGUF directory admission. A later open of the exact local filesystem snapshot
may consume a rebuildable verified-reopen lease and records zero payload bytes
hashed for that open; an absent, malformed, or stale lease falls back to the
complete hash. Residency schema v7 selects an admitted immutable backing.
Descriptor tensors retain exact artifact offsets. When the compiled physical
plan requires no derived asset and CUDA can register the immutable artifact
mapping, that mapping is the production weight backing. Readiness consumes the
returned device address without requiring a whole-artifact prefetch. Plans
requiring derived layouts select managed CUDA residency and complete their
prefetch before readiness. Neither alternative may be substituted silently.
Warm operations reuse the same verified handle,
immutable descriptor, attention graph and weight backing. Before and after
execution, snapshot drift invalidates the model, sessions, residency, workspace,
graph executables and candidate state.

Sessions own mutable state. `yvex_runtime_session_prepare_persistent_state`
seals the provider layout and CPU/CUDA residency for an exact capacity;
`yvex_runtime_session_reset_persistent_state` clears committed content while
retaining compatible allocation. The graph-state ABI provides checked
begin/stage/commit/abort, committed/candidate views, summaries, invalidation,
and release. These declarations are internal C contracts, not installed ABI.

Prepared steady-state execution performs no host or device allocation, weight
read, upload, workspace resize or graph capture. The runtime refuses requests
outside the prepared capacities instead of resizing a captured execution
implicitly.


## Physical Execution And Candidate-State ABI

`<yvex/internal/execution.h>` owns Physical Execution IR schema v5 package
decisions, hardware/workload/capacity records, physical evidence records, and
device-value views. PEIR binds canonical terminal role, qtype, row geometry,
encoded range, consumer, stable layout, and sharing; it contains no selected
backend, activation, kernel family, request width, or live resource fact.

Runtime specialization maps each PEIR decision to a typed implementation
record for one backend/device. It owns activation representation, admitted real
widths, equivalent fallback class, and hardware crossover. The non-persisted
execution profile binds that specialization to one engine generation and
workload. The removed execution-shape registry is not a compatibility surface;
transient compatibility keys carry only the facts needed to form real batches.

`<yvex/internal/execution_batch.h>` owns the typed execution-batch and expert-
worklist contracts. The specialization contains only deployment-stable
implementation facts; one runtime instance binds actual sources, rows,
provenance, expert buckets,
offsets, populations and route weights. A bucket contains rows for exactly one
compatible expert. CUDA may select an equivalent microkernel and execute a
bounded tail, but it cannot regroup routes, merge sessions, or fabricate width.
The copied observation is pointer-free developer evidence, not execution authority.

`<yvex/internal/candidate.h>` owns prefix projection from an attention
candidate delta. It can reconstruct any admitted verified prefix without
rerunning accepted target rows. The logical state provider and backend
residency still publish the same generation atomically; candidate projection is
not a second persistent-state authority.


## Internal Generation And Hosted Turn Boundary

`include/yvex/internal/generation.h` owns the family-neutral generation plan,
prompt admission, exact suffix prefill, target-only and speculative execution,
accepted-prefix transaction, stop reasons, incremental committed-text
publication, partial progress, and result validation. Its implementation is
split among the admitted runtime generation, session, speculation, context,
and result owners rather than exposing another ABI. It borrows one admitted
model engine and one execution session; it does not reopen artifacts or
duplicate tokenizer, Transformer, logits, sampling, or KV semantics.

The speculative boundary consumes a family-projected draft plan and generic
proposal/verification contracts. Candidate tokens and draft RNG state remain
private until full-target verification determines the accepted result. Model,
token-ledger, incremental-decoder, text, and RNG participants prepare and
publish one accepted prefix together; rejection and cancellation abort every
uncommitted participant. Existing generated-token and completion-usage counts
remain committed-target counts.

`<yvex/server.h>` exposes the local protocol, persistent host, engine manager,
engine generations, server session, typed event, metrics snapshot, and thin
protocol-client lifecycles. `yvex serve` starts with zero engines;
`yvex_server_engine_load`, `yvex_server_engine_unload`, and
`yvex_server_engine_snapshot` own the in-process lifecycle. Server sessions
retain independent execution state, exact token ledgers, transcripts, and turn
records across client detach and are bound to one exact generation. The next
turn reuses state only after exact token-prefix admission and prefills only the
new suffix.

Streaming sends a fragment after model, decoder, and internal text commit.
Client delivery failure preserves committed state and reports a partial turn.
The runtime-client object lane in `yvex` links the protocol/client surface only
and cannot call the internal generation or runtime-model APIs directly. Offline
routes in the same ELF have separately guarded engine dependencies.


## Runtime Binding And Operator Actions

The offline command lane provides the direct production consumer for the internal ABI:

```text
yvex bench attention prepare
yvex inspect attention describe
yvex inspect attention capabilities
yvex inspect attention plan
yvex bench attention execute
yvex bench attention compare
yvex inspect attention state
yvex bench attention state validate|exercise
yvex inspect attention residency
yvex bench attention capture|replay
yvex bench attention graph list|inspect|warmup|update|invalidate|release
yvex bench attention trace|profile|component|qualify
yvex bench attention benchmark compare
yvex bench moe
yvex bench transformer execute
```

`prepare` is the compiler-side producer for an external runtime binding.
Execution actions require the binding and do not regenerate it. `plan` seals a
request descriptor without numerical dispatch. State actions allocate and
exercise the real process-local persistent provider through multiple production
executions, including causal read-after-write and clear/reuse. Graph-registry
actions operate on the same session rather than report-only labels.
Registry inspection reports captured kernel, copy and memset nodes plus capture,
instantiation, update and replay timings. It is not a persistent cross-process
graph cache.

The canonical probe preserves real model width, heads, bindings, qtypes,
position policy and attention history geometry. It is deterministic diagnostic
input, not prompt text. Production activation prefill instead selects
`--input tensor-file --input-file FILE`, validates the schema-v1 bundle, and
reports `activation_prefill_ready` separately from
`full_model_prefill_ready`.

`bench moe` requires explicit artifact and runtime-binding paths,
`--backend cpu|cuda`, `--input tensor-file`, `--input-file FILE`, `--scope
full`, and `--progress off`. It calls the production runtime MoE API directly;
it does not run a fixture, test executable, Make target, or second process.

`bench transformer execute` requires explicit artifact, runtime binding, and a
schema-v1 `--input token-ids --input-file FILE`. It accepts `--backend
cpu|cuda`, `--phase prefill`, positive chunk/context capacities, and
`--progress off`. It calls the production transformer API directly and reports
normalized hidden and persistent-state facts without invoking tokenizer,
logits, or generation owners.
