<!-- docs:metadata
title: Runtime Benchmark Publication Contract
id: yvex.contracts.benchmark-publication
document: reference
status: current
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Runtime Benchmark Publication Contract

**The existing runtime baseline and chart lifecycle remains an evidence owner.**

[Up](README.md)

## Qualification, Benchmark, And Chart Contract

`qualify` executes the production runtime path and reports software-contract,
numerical-conformance, runtime-qualification and component-benchmark status as
separate facts. It also reports that model behavior evaluation, model quality
evaluation, agent runtime/evaluation and release qualification are unavailable.
The command does not invoke source-tree tests or link the test-only oracle.

`profile` and `benchmark` also use the production runtime path. Benchmark
records identify their scope as `attention_component`, require correctness and
runtime preconditions, and keep correctness, structural-runtime and performance
status independent. Samples separate cold preparation, first execution,
publication and cleanup from steady-state execution. Distributions report
minimum, mean, dispersion, median, p50, p90, p95, p99 and maximum together with
allocation, transfer, launch, residency and workspace counters.

`--write-baseline --baseline FILE` writes a versioned identity-bound external
baseline. A later compatible run or `benchmark compare --baseline OLD
--current NEW` may compare two records. The schema-five key binds the build,
artifact, materialization, logical model, runtime binding, numeric policy,
runtime and execution descriptors, semantic and executable graphs, residency,
workspace, state layout, kernel bundle, machine, device, phase, attention
class, scope, mode, geometry, capture bucket, trace policy and iteration count.
Compatibility comparison retains both commits as provenance while excluding
the commit alone from the workload-equivalence key. An incompatible comparison
refuses and identifies the first mismatched field.

`benchmark compare --max-regression-bps N` enables an explicit caller-owned
ceiling for latency, inverse throughput, memory, transfers, allocation counts,
and launch counts. No threshold is implied when the option is absent:
performance remains `measured`. The policy identity and comparison identity
bind the selected ceiling and result. A compatible comparison exits nonzero
when any measured dimension exceeds the ceiling; structural runtime failures
remain separate and cannot be converted into performance allowances.

`--chart PATH.svg` is available on `profile`, `benchmark`, and `benchmark
compare`; it is not a global attention option. It writes a deterministic SVG
containing cold preparation, warm latency distributions, resident/workspace
bytes, resident H2D bytes, and kernel/graph launch, capture, replay, and node
counters, optionally against a compatible baseline. The schema-five baseline
seals those structural counters, timings and complete reproducibility identity.
Schemas one through four require regeneration.
Baseline and SVG publication are independently atomic and no-replace; an SVG
failure never withdraws an already admitted baseline. JSON, CSV, baseline, and
generated SVG outputs are external operator assets and are not tracked. They
are not full-model benchmark or release evidence.
