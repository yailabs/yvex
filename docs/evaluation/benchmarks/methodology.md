<!-- docs:metadata
title: Benchmark Methodology
id: yvex.evaluation.benchmarks.methodology
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Benchmark Methodology

**Compare exact experimental configurations and preserve unavailable evidence.**

[Up](README.md)

## Existing runtime evidence remains authoritative

Runtime benchmark schema v5 already owns independently reopenable baselines,
JSON/CSV reports and identity-bound attention charts. Its
[publication contract](../../contracts/benchmark-publication.md),
[validator](../../../tests/support/validate_runtime_benchmark.py) and
[internal ABI](../../../include/yvex/internal/benchmark.h) remain unchanged.

The documentation observation envelope imports selected measured facts with an
exact original pointer. It does not replace the producer format, loosen its
admission rules or retain large raw profiles in Git. Raw runtime evidence stays
at its established operator-owned location.

## One observation, many views

Structured JSON → generated Markdown table → deterministic SVG → HTML reader → PDF.
Run `make docs-benchmarks` after changing an admitted observation. `make docs-check`
rejects stale projections. Never type the same chart values independently into prose.

## Required context

The [observation schema](schema/observation.schema.json) retains source commit/tree
and stability, run/date, model/revision, artifact/binding, representation,
backend/device, runtime configuration, workload, warm/cold state, sequence lengths,
concurrency, memory, metric/unit, samples, scope and limitations.

Unavailable facts are `null` or explicitly `unknown`. An imported historical
characterization with missing context cannot be relabeled a benchmark. Fixture
data is marked FIXTURE in the record, table and figure and supports tooling only.
Real benchmark records require exact provenance and sample counts.

Engineering adapters retain an external source capture before measurement:
the committed Git tree, binary diff, untracked source files and exact executable
bytes, each hash-bound in a manifest. Ignored build products, model payloads and
runtime stores are not collected as source. A dirty-delta digest alone does not
make the source recoverable. Capture refuses concurrent source/executable changes
and never overwrites an earlier receipt. Capturing bytes is not proof that the
executable was built from that source; the build receipt must establish that
relationship separately. Historical missing source deltas remain unavailable.

## Comparison gate

Compare only compatible metric definitions, scopes, source/model/artifact/numeric
identities, backend/device, runtime/build configurations, workload lengths,
concurrency and warm/cold conditions. Differences require separate series and an
explicit experiment; the renderer never asserts cross-record comparability.

Retain distribution and sample count for latency. Component timing is not model
throughput. Wall/device/host spans may overlap; do not add nested durations.
Mapped, prepared, physical state and RSS byte classes may overlap as well.

## Qualification targets and local receipts

Family conformance establishes reusable grammar or numerical semantics only at
the family contract's declared scope. Checkpoint reference evidence comes from
an independent implementation of an exact upstream checkpoint. Representation
quality measures one physical transformation against that checkpoint's reference;
a different quantization needs its own evidence. Backend execution and deployment
performance additionally bind implementation, hardware topology and configuration.
Product-path evidence measures the boundary the user actually experiences.
None of these planes inherits PASS from another.

Independent capture summaries under `references/` project authenticated raw
inference evidence into the [reference matrix](generated/qualification-references.md).
They retain checkpoint, independent executable, representation/environment receipt
identities, workload inputs, bounded output populations and source-parser results.
They do not certify YVEX continuation agreement. A natural-EOS grammar failure
remains FAIL even when the request completed; a truncated reasoning output does
not establish a reasoning-to-final transition. Large tensor and output manifests
remain external, referenced by digest. Family capture owners authenticate them;
the generic qualification validator checks the compact publication contract.

The canonical qualification validator is `tools/qualification.py`. Its target
references existing producer identities, rather than recomputing source,
artifact or binding lineage. Null means unavailable; it prevents qualification
or comparison requiring that dimension. Local paths, timestamps, PIDs and
observed clocks belong to provenance, not target identity. Exact device UUIDs,
clock/thermal observations and raw evidence locations remain receipt provenance.

`origin=local` identifies a local receipt, not a maintainer guarantee.
`origin=yvex-published` identifies a maintainer-published record, **not** automatic
qualification of all its planes. Each plane independently declares QUALIFIED,
CHARACTERIZED, BLOCKED, UNQUALIFIED or UNSUPPORTED. The published matrix is sparse:
an absent row is no claim, not proof that execution is impossible. Successful
compilation, loading or generation does not establish quantization quality.

Receipts under `qualification/` reference producer evidence and supply the
[generated target matrix](generated/qualification-index.md), exhaustive target
details and machine projection. Generate them with `make docs-benchmarks`; do not
edit generated tables. Required quality metrics come from the family/checkpoint
pack, not a universal tolerance. Full-distribution KL/RMS cannot be recovered
from a top-k slice. Unavailable reference metrics are omitted with an explicit
non-claim, never populated with zero.

## Product lanes and reasoning matrix

Keep controlled-engine, native local protocol and HTTP compatibility results
separate. A controlled-engine experiment may select a declared engineering
profile. Product-native measurement preserves the operator-selected strategy,
context and prefill geometry and exercises fresh and reused sessions. HTTP is
an adapter measurement, not a substitute for local chat. No lane silently loads
a different model or changes the selected product profile.

Sampling selection is not just the word `product`: native requests retain the
C-owned default `stochastic=0` even with `temperature=1`, while the OpenAI
adapter derives stochastic sampling from positive temperature and supplies a
seed when absent. A default HTTP request is therefore not a transport-only
comparison with default native chat. Retain the resolved policy and mark an
unprojected generated seed unavailable. Use explicitly matched deterministic
policies for an adapter-cost experiment, without relabeling them product defaults.
Only admitted HTTP fields are transmitted; neutral native filter defaults are
recorded as resolved provider facts, not invented OpenAI request parameters.

The native `TURN_STARTED` acknowledgement also retains a client-relative arrival
time. It bounds the work before the internal turn timer, including connection,
admission and generation-context preparation; it is not pure server setup time.
The typed measurement observer correlates this arrival by request identity.
Its `integration.qualification-native` QA control uses an isolated protocol
fixture, not a model or performance sample. Duplicate, foreign and invalid-timing
observations refuse; historical receipts lacking an arrival keep it unavailable.
Subtracting server TTFT from client-visible TTFT does not, by itself, measure
network latency. First-fragment publication remains unavailable unless the
producer explicitly supplies it.

Native local receipts also retain the number of nonempty final/reasoning
fragments observed and the maximum and arithmetic-mean interval between
consecutive visible fragments. These per-turn gap metrics exclude the interval
before first content (TTFT), empty fragments and control-only channels; at least
two visible fragments are required. Missing or single-fragment observations
remain unavailable, not zero. Each performance sample is a complete turn's gap
statistic, not a pooled token average. The raw journal preserves receive-relative
monotonic times and fragment hashes without model prose. Observer/hash/journal
work and model, publication or transport waits can contribute to the intervals;
they are not isolated network delay, GPU token timing or REPLAI/PTY paint timing.
Legitimate verified speculative blocks may arrive close together. The observer
does not delay, animate or change their publication. Historical receipts do not
acquire missing observations from a new client version.

Suite manifests own immutable logical cases, provenance/license, applicability,
output bounds, sampling and admissible reasoning/strategy axes. Discovery uses
the supported suite schema, not a family name or filename suffix. Unknown suite
versions and duplicate suite selectors refuse catalog generation; unrelated
vector schemas are not projected as qualification suites. Use the same
case identity across those axes where valid. DeepSeek uses none, high and
maximum with target-only and DSpark recorded separately. Maximum keeps the
source-authored instruction; reasoning is not suppressed for speed. Unrun cells
are UNQUALIFIED; a transition not naturally reached is NOT MEASURED.

Retain total prompt, reused prefix and newly executed prefill positions. Prefill
rate divides new positions by their prefill wall time. Retain server first-token,
first fragment publication (when observable) and client first visible content
as different facts. Decode after the first committed token is distinct from
TTFT and complete request time. Reasoning and final channel counts/rates and
their source-authored transition remain separate. Fragment count is not token
count. Speculative evidence also records real proposals, verification population,
accepted/rejected/discarded tokens, accepted prefixes and phase costs.

The shared metric-publication rules exclude fewer than 32 post-first committed
positions from `decode.post-first.committed`; a ten-token greeting remains a
latency/phase observation, not a sustained-decode sample. This eligibility floor
does not, by itself, establish steady-state or release throughput. Likewise,
`prefill.uncached` requires a positive newly executed population and zero reused
prefix tokens. Reused turns retain their new-position counts and prefill wall
time, but do not become uncached-prefill benchmarks. Missing or malformed counts
are unavailable, not zero. Both Rust and the engineering adapter consume the
same generated population rules; a mixed eligible/ineligible repetition group
cannot silently discard short samples and publish the remainder.

The native `generated_token_identity` (projected as `token_identity` in local
observations) includes execution/state lineage, not only sampled token IDs.
Different target-only and speculative identities do not establish different
continuations. Ordered fragment channel/extent/byte-digest agreement establishes
bounded published-content agreement only; differing packetization is inconclusive
without retained bytes. Neither observation establishes logit equivalence or
representation quality. The measurement adapter validates complete deliveries
before constructing these fragment manifests.

Repeated multi-turn samples may generate different assistant histories. The
native runner retains all completed observations but separates later inputs by
their exact input identity. Such metric rows carry an explicit `/input-IDENTITY`
suffix; their samples and statistics never cross history groups. This separation
is not deterministic replay evidence: differing greedy output still needs its
own runtime investigation. Unknown input identity refuses receipt generation.

QA device locks are advisory and do not reserve hardware against ordinary
operator clients. Measurement adapters also retain sampled compute-process
observations throughout each timed interval, not just an idle check before
loading. Foreign activity or an unavailable observation is sticky: later idle
cleanup cannot turn a contaminated sample into comparable performance evidence.
The observer retains its cadence and probe time; OBSERVED_CLEAR means only that
its sampled process lists were clear, never uninterrupted exclusive reservation.
Observation gaps and thermal/clock changes remain separate measurement limits.

`tools/qualification_run.py` owns the generic CUDA process-list observer used by
native and HTTP adapters. A wrapper may cancel only its owned Rust qualification
child through that command's tested SIGINT/settlement contract. It does not kill
the host, signal a foreign process, retry a turn or reinterpret unsettled cleanup.
Standalone local receipts leave resource observation unavailable; a maintainer
import must join the external witness before making an isolation claim.
Retain all raw outcomes when excluding contaminated timing samples, including
their exact selection and reason. Do not silently drop a slow sample or pool
its latency with unobserved contention. Historical missing isolation remains
explicit rather than being backfilled with a fictional all-clear observation.

The native channel phase rates are not post-first sustained decode rates:
reasoning runs from prefill completion to the source-authored boundary (or decode
end), and final content runs from that boundary (or prefill completion) to decode
end. Keep these producer-defined denominators in the metric name/definition.
An absent reasoning/final channel has unavailable first-token/rate measurements,
not a zero-latency result. Client fragment arrival is separately measured.

A displayed **20 tok/s is not a complete claim**. Its target must expose exact
checkpoint/representation, build, backend/hardware/topology, runtime and reasoning
configuration, strategy, transport, workload, denominator and sample statistics.
Synthetic counting may characterize an engineering control; it cannot alone
describe interactive product performance. A single run is characterization,
not a stable performance qualification. Retain median, range and dispersion;
keep profiler runs out of timed comparison samples.

Receipts may retain bounded diagnostic facts under `provenance.diagnostics`:
exact case, nonnegative value or unavailable `null`, unit, definition and
evidence pointer. The same generated rules validate Python and Rust consumers;
CLI detail and public pages label them separately from timed benchmark samples.
Diagnostics cannot supply a throughput metric, qualify a missing performance
gate or be ranked by the performance comparison command. Raw activity streams
remain external and hash-bound. A profiled turn is not silently imported as an
unprofiled performance series. Both Python and Rust comparison gates require
explicitly unprofiled performance samples; unknown profiling state also refuses.

Native/HTTP measurement captures declare instrumentation separately from the
qualification target. Pass `--profiled` for diagnostic runs, including externally
attached profilers. Known CUDA-injection/preload environment markers conservatively
mark the capture profiled without disclosing their values. The native importer
requires the same explicit unprofiled declaration in admission, observation and
closure records; legacy missing/unknown state refuses rather than becoming
`profiled=false`. This declaration is not proof against undisclosed external
attachment and does not alter the underlying model or metric denominator.

For mapped weights, Linux mapping RSS, process major/minor faults and backing-I/O
counters are diagnostic observations, not CUDA allocation or GPU paging facts.
A sampled full RSS mapping is consistent with zero explicit device-copy bytes;
it does not prove page locking, future residency or optimal access bandwidth.
Keep sampling/profiling overhead explicit. Measure kernel execution and control
costs separately before attributing inference latency to weight residency.

Full `/proc/PID/smaps` walks are heavyweight residency instrumentation, not a
free background observer: walking a large registered mapping can stall or
compete with the computation being characterized. Use separate diagnostic
turns, or boundary-only snapshots outside the timed interval, for such probes.
Retain their actual cadence and duration. Never subtract probe time from a
request denominator to manufacture an uninstrumented result. Instrumented and
uninstrumented series have different measurement conditions and must not be
pooled or compared as an unchanged experiment; repeat the equivalent workload
without the expensive observer before accepting a performance effect. The
lightweight process-list isolation witness remains separate and retains its
own measured probe overhead. Historical missing observer facts are unavailable,
not a fictional zero-cost witness.

### Memory placement and cross-engine comparisons

Keep two experiments separate: comparing complete engines measures all their
implementation differences; comparing placements within one engine can isolate
a placement hypothesis. A faster engine does not prove that its allocation
policy caused the gain. A smaller OS **used** value does not prove a smaller
physical footprint: clean mapped weights may reside in file cache and process
RSS at the same time. `MemAvailable` may include those reclaimable pages; it is
not a reservation preserving the model's warm working set. The
[operator interpretation](../../guides/operator-runbook.md#mapped-weights-ram-and-ssd-streaming)
owns the counter and data-path explanation.

For a residency A/B, freeze the source/build and exact checkpoint, target/draft
weights and physical representation; declare the changed backing/placement axis.
Hold device/topology, workload/rendered tokens, context, prefill geometry,
strategy, reasoning, sampling, sequence state and concurrency constant. A qtype
name alone is not a matching representation. Authenticate any adapter or
container transformation. An otherwise useful cross-engine result with different
weights/numerical obligations remains an explicitly different experiment, not a
causal residency comparison.

Record load separately from fresh/continued prompt prefill, first-token latency,
post-first committed decode and complete client-visible turns. Retain model
mapping RSS/PSS, anonymous/file/shared classes, page-lock observations, swap,
faults and backing-I/O populations where available, as well as mandatory state
and workspace high-water. Do not sum overlapping byte classes. CPU counters do
not stand in for GPU faults, translation cost or bandwidth measurements. Retain
cold/warm policy and repetitions; a warm file mapping is not SSD streaming solely
because its storage backing is a file. OS pressure and cache reclamation are
separate conditions, never unreported changes to the comparison.

Detailed paging/profiling runs are diagnostic and excluded from timed samples.
Never clear system caches or create memory pressure on an operator-owned host.
A measured warm-residency snapshot earns neither a permanent-residency guarantee
nor a claim that copying or locking all weights is faster. A proposed retained
placement must demonstrate complete-request benefit, numerical correctness,
resource admission and lifecycle safety under its exact target identity.

The comparison validator refuses implicit differences in relevant identities,
workload, reference or denominator. A declared experiment may vary permitted
axes but does not automatically rank results. Quality regression never changes
checkpoint or reference corpus. A fully bound CHARACTERIZED performance record
may be compared at its measured scope; comparability does not promote any plane
to QUALIFIED. An incomplete identity or unknown/profiled timing still refuses.
Reports follow the permanent
[agent qualification protocol](../../../AGENTS.md#model-representation-and-performance-qualification-reports).

## Operator inspection and local measurement

The Rust shell projects the canonical records and manifests through the operator
registry. Inspection does not load a model, run Python, or start a benchmark.
An older installed executable may predate this projection; check its exact build
before assuming these commands are present:

```sh
yvex model qualification list
yvex model qualification show deepseek-throughput3-isolated-candidate --json
yvex model qualification suite --json
yvex model qualification compare left.json right.json \
  --metric decode.post-first.committed --case coding.metal/turn-0 --json
```

`show` accepts a published target ID, exact target digest, unambiguous variant or
local receipt file. `compare` refuses missing relevant provenance, a different
checkpoint/reference for quality, or undeclared configuration changes. `--vary`
declares an experiment, never implicit comparability or automatic ranking.

The native local measurement command consumes the existing C-owned private
protocol. A local characterization is not full-model qualification:

```sh
yvex model qualification run v4-flash --suite deepseek-product \
  --case coding.metal --reasoning high --samples 3 \
  --receipt-dir /absolute/new/evidence-directory --json
```

Use an idle already-loaded exact variant. The command does not load/unload or
switch strategy/context/chunk. It refuses active work, leases, attached clients,
unknown suite/artifact relationships and overwriting evidence. `--variant`
selects an exact loaded choice; `--socket` selects an isolated native host;
`--sampling greedy` is an explicit override of the default product sampling.
Each repetition owns a new benchmark session; multi-turn cases reuse only that
session. The private journal preserves partial outcomes for reconciliation;
an unresolved delivery is never automatically retried or blindly closed.
SIGINT/SIGTERM retain pending cancellation until native admission, while the
response reader continues draining. A typed completed cancellation, or a
successful terminal result racing interruption, permits closing only the owned
test session and checking engine generation/count/activity. The suite then exits
without a performance receipt. The cancellation ACK alone is not settlement.
Correlated terminal refusals are settled failures, not indeterminate delivery;
their partial-state facts are retained before the same scoped cleanup. Failed
case outcomes appear in the workload matrix even without any performance samples.
Do not promote a bounded reasoning-capacity refusal to successful decode evidence.

Local receipts expose unknown producer identities rather than treating the CLI
build as the Host build. They are CHARACTERIZED local observations, not official
qualification, and incomplete identities deliberately prevent performance
comparison. The final model gate still requires independent checkpoint reference,
frozen producer provenance, lifecycle evidence and representative repeated runs.

An installed producer and its measurement adapter may come from different exact
source snapshots. The native adapter's `--producer-source` selects the preserved
producer checkout; its clean/dirty identity must match the installed executable's
typed version record. A clean build may omit only the empty source-delta digest.
The receipt retains the adapter snapshot and client digest separately, checks
both sources and binaries throughout sampling, and records final host/engine
state. A newer benchmark client never supplies an older server's build identity.

Closed native captures can be projected into the same structured receipt authority
without running another inference request:

```sh
python3 tools/qualification_run.py native-receipts \
  --run /external/closed/native-capture --suite tests/vectors/deepseek_competitive.json \
  --relationship docs/evaluation/benchmarks/qualification/deepseek-published-native-coding-16.json \
  --output /external/new/receipt.json --id exact-native-result --title 'Exact native result' \
  --skip-first 1
```

The relationship must join the identical artifact/binding to the suite's immutable
checkpoint; names are not a provenance oracle. Import authenticates captured
producer/adapter bytes, rederives each metric from native events and refuses
changed workload/configuration, generation/cleanup mismatch, tampering or observed
accelerator contention. Source archives and raw observations remain external.
An explicit `--skip-first 1` retains the first repetition in raw evidence while
publishing only the repeated warm series. It cannot select a lucky subset.
Different multi-turn published histories form separate input groups. Fewer than
33 committed tokens cannot produce a sustained post-first-token row; absent
reasoning, publication timing and load measurements are unavailable, never zero.
Import earns timing characterization, not model-quality or upstream conformance.
The default receipt origin is local; public maintainer adoption remains deliberate.

For independent DeepSeek capture, `tests/reference/deepseek_inference.py`
authenticates the pinned upstream encoder/tokenizer and prepares exact inputs.
Preparation returns BLOCKED when independent model outputs are absent. Reference
admission checks checkpoint, implementation provenance and each case/mode input
identity against bounded external raw captures containing `input_identity`,
`output_token_ids` and `text`. Captures are referenced by relative path and SHA-256;
the output digest binds UTF-8 text. Raw captures must also retain exact prompt
token IDs, sampling, requested output bound and successful finish reason. Empty
suites, missing modes, cancelled captures and shorter admission probes do not
satisfy the reference corpus. Capture presence is not YVEX numerical
conformance. The official four encoding vectors remain a separate existing gate.

Controlled continuation comparison uses the C-authored `prompt_token_identity`
and ordered `generated_tokens` projection of `bench transformer generate`.
The existing tokenizer identity encoding checks exact prepared input token IDs;
the adapter never retokenizes generated prose or parses human stdout. The generic
`reference-generation` adapter consumes the immutable prepared-input and
independent-capture manifests, authenticates raw reference request/response,
and observes the first divergent token and matching greedy prefix. It keeps
target-only and DSpark separate. Exact prefix agreement is characterization
across the explicitly different numerical realizations, not an invented
representation-quality tolerance. Tokens after divergence have different causal
contexts and cannot provide teacher-forced same-top-token accuracy. EOS samples
are distinguished from model-committed positions. Full-distribution quality
metrics remain unavailable without an appropriate independent logits oracle.
The adapter also refuses an executable whose Make/Cargo-authored version record
does not match the exact source snapshot. Native explicit greedy sampling uses
neutral temperature 1 and no stochastic draws; the independent adapter selects
greedy with temperature 0. Both choose argmax, but their parameter records stay
distinct. The adapter does not change the product sampling contract.

```sh
python3 tools/qualification_run.py reference-generation \
  --inputs /external/exact/inputs.json --reference /external/exact/capture/reference.json \
  --binary ./yvex --artifact /absolute/exact/model.gguf \
  --binding /absolute/exact/runtime-binding --target admitted-target \
  --context 4096 --chunk 64 --strategy target-only \
  --case coding.metal --reasoning none --reasoning high --reasoning maximum \
  --output /external/new/candidate-reference
```

This direct engineering path includes isolated model preparation and teardown;
its elapsed command time is not warm resident-host product performance. The
Task candidate's generation operator also projects the existing backend host-staging summary
after execution and before cleanup: allocated capacity, current cursor and
actual high-water cursor are distinct facts. Unavailable is null. These bytes
are neither artifact residency nor total physical memory; observing them adds
no CUDA wait or execution policy. This diagnostic projection is not yet part
of the installed producer or this evidence-authority publication. Native
protocol and HTTP timing continue to use their distinct measurement lanes.
Source-authored output grammar is checked independently: a captured EOS response
can still violate the family grammar, and a length-truncated response does not
prove the reasoning-to-final transition. Neither result becomes a quality PASS.
Use `--report /absolute/new/reference-observation.json` to project the compact
capture/grammar view after authenticated input and raw-output validation. Review
that external observation before importing it under `references/`; regeneration
and drift checks use the same generic validator as the public view.

### Reproducible independent producer capture

`tools/qualification_reference.py` is the shared independent-producer adapter.
It consumes prepared token inputs and an external
`yvex.qualification.reference-producer.v1` plan; it contains no benchmark prompts
or family grammar. The current adapter is `llama.cpp-completion-v1`, with explicit
greedy sampling, fresh uncached prompts, one sequence and F32 KV. Those are
reference configuration facts, not product defaults or a higher-precision oracle.
Another family can use the same adapter after its own input owner authenticates
the prepared manifest; another producer requires an explicitly admitted adapter.

The plan retains `name`, `adapter`, exact upstream `repository`, `revision` and
`tree`, local `checkout`, `binary`, `artifact`, `checkpoint_manifest`, and
`representation_receipt`. Each of the last four files has its corresponding
`_sha256` field. `libraries` maps exact library paths to digests. `context`,
`chunk`, `port` and `limitations` are required. Paths are replay locations only;
the source, executable, library and transformation digests bind identity.
The representation receipt must describe any container transformation, its
unchanged/changed tensor bytes and reproducible transformation code. Do not
substitute a different container just because the independent consumer accepts it.

The current same-weight DeepSeek reference has an immutable
[`deepseek_reference.json`](../../../tests/vectors/deepseek_reference.json)
recipe. `tools/qualification_gguf.py` consumes that recipe with the exact clean
pinned independent reader (and its NumPy dependency), without importing a YVEX
decoder. It aliases three metadata names, excludes the draft-only directory,
embeds BF16 bits exactly into F32, and packs target tensors densely. Every other
target byte is unchanged. Exhaustive 65,536-pattern embedding and full tensor
readback precede the expected output-container digest gate. This is a
reference-only transformation, not an admitted runtime variant:

```sh
/absolute/reference-python/bin/python tools/qualification_gguf.py \
  --source /absolute/exact-mixed-artifact.gguf --output /external/new/reference.gguf \
  --recipe tests/vectors/deepseek_reference.json \
  --reader-checkout /absolute/exact-independent-llama-checkout
```

The resulting external `.json` receipt supplies `representation_receipt` in the
producer plan. Pin the exact reader/source identities and dependency environment;
do not infer higher-precision quality from this lossless widening operation.

For the current pinned DeepSeek checkpoint, first prepare the authenticated
inputs using the existing tokenizer-reference environment:

```sh
/absolute/tokenizer-reference/bin/python tests/reference/deepseek_inference.py \
  --source /absolute/pinned-checkpoint --output /external/new/inputs.json
```

This preparation exits BLOCKED (2) without independent outputs. It is not a
failed tokenizer or a completed inference gate. Then use a reviewed exact
producer plan and idle GPU; no operator service is adopted or interrupted:

```sh
python3 tools/qualification_reference.py --inputs /external/new/inputs.json \
  --producer /external/producer-plan.json --output /external/new/capture \
  --case chat.short --case coding.metal --case math.reasoning \
  --case extraction.json --case writing.runbook --case conversation.coding \
  --case reasoning.schedule --case tools.weather
```

Case selection preserves all prepared reasoning modes for each selected case.
The adapter checks clean pinned source, weights/checkpoint receipt, exact prompt
token parity, loaded executable and loaded library identities. It retains the
actual raw request and response before deriving continuation facts. A shortened
request, altered policy, failed delivery or moving producer refuses; no generation
is retried. Partial captures remain external and cannot satisfy complete suite
coverage. Only its owned loopback process receives graceful interruption at
teardown; `closed.json` records that outcome separately from YVEX cleanup.

The resulting `yvex.qualification.reference-captures.v1` record is admitted by
the family validator only with complete representative coverage, matching raw
request/response evidence and clean independent teardown:

```sh
/absolute/tokenizer-reference/bin/python tests/reference/deepseek_inference.py \
  --source /absolute/pinned-checkpoint --output /external/new/rechecked-inputs.json \
  --reference /external/new/capture/reference.json \
  --report /external/new/reference-observation.json
```

Older family-owned captures retain their original schema and evidence identity;
they are not silently upgraded to the stronger raw-producer-response contract.

Independent multi-turn reference inputs use actual captured assistant history,
not a hand-authored expected reply. The DeepSeek input owner admits only a
naturally completed response accepted by its exact upstream parser. It then
renders the next source-authored request, including the upstream policy for
retaining or dropping earlier reasoning. A malformed or truncated prior reply
gets an explicit unqualified continuation disposition; no terminator is inserted
to make history usable:

```sh
/absolute/tokenizer-reference/bin/python tests/reference/deepseek_inference.py \
  --source /absolute/pinned-checkpoint --output /external/new/continuation-inputs.json \
  --continue-from /external/capture/reference.json
python3 tools/qualification_reference.py --inputs /external/new/continuation-inputs.json \
  --producer /external/producer-plan.json --output /external/new/continuation-capture \
  --case conversation.coding/turn-1
```

The preparation-only command still exits 2 until the second inference exists.
Validate that capture with the same `--continue-from` plus `--reference` and
`--report`; each derived input binds its predecessor's actual capture digest.
This measures independently rendered history, not native retained-state prefix
reuse. Those product-path measurements remain separate.

The [DwarfStar continuation methodology](https://github.com/antirez/ds4/blob/80ebbc396aee40eedc1d829222f3362d10fa4c6c/gguf-tools/quality-testing/README.md)
and [llama.cpp distribution comparison](https://github.com/ggml-org/llama.cpp/blob/master/tools/perplexity/README.md)
inform distinct continuation and probability-quality controls. Neither supplies
YVEX evidence by analogy. Checkpoint-matched quality, numerical tolerances and
YVEX continuation agreement remain separate gates after a successful capture.

### Publishing an independent continuation comparison

The generic `continuation-receipts` adapter authenticates a completed
`reference-generation` run against its retained source/executable bytes, build
projection, prepared inputs and raw independent requests/responses. It recomputes
the comparison rather than trusting a handwritten result. Moving source, altered
raw output, a contended/unavailable resource interval or unsettled owned teardown
refuses publication. It never runs inference or fills historical missing
provenance from today's executable.

```sh
python3 tools/qualification_run.py continuation-receipts \
  --run /external/comparison \
  --inputs /external/inputs.json --reference /external/capture/reference.json \
  --suite tests/vectors/deepseek_product.json --output /external/new/receipts \
  --id my-exact-comparison --title 'Exact independent continuation comparison'
```

The default origin is local. Maintainers may deliberately publish a reviewed
characterization with `--origin yvex-published`; that changes publication origin,
not qualification state. Per-mode receipts feed the existing generated target
and workload views. Their checkpoint-reference plane is CHARACTERIZED,
representation quality remains BLOCKED without its admitted oracle/tolerance,
and no performance or product-path gate inherits success. A zero matching
prefix is a real disagreement, not missing data; unavailable distribution metrics
remain NOT MEASURED. Different free-running contexts after divergence cannot
be compared as teacher-forced probability accuracy.

Engineering generation JSON now projects the existing native admitted plan's
runtime/binding/tokenizer/prompt/kernel identities and context/chunk/output
geometry. These are typed C facts, not values inferred by a benchmark script.
No C record or protocol layout changes. Older captures retain null unprojected
identities; source-order arithmetic and numerical class remain unchanged.

## Chart choice

Use bounded bars for comparable quantities, lines for ordered progression,
distributions for latency and Pareto/scatter views for trade-offs. The initial
generator deliberately supports one metric/unit per bounded bar figure.
Additional chart types need an observation schema and validation appropriate to
their meaning. No chart promotes Status automatically.

## Current imported observations

The Mamba readout durations and Qwen bounded state allocations were already
recorded before this migration. Their original execution commit/date and some
environment facts were not retained in those paragraphs. They remain historical
characterization with explicit gaps, not new performance evidence.

No release-wide model benchmark is produced by this documentation milestone.
