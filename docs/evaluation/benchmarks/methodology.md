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

## Comparison gate

Compare only compatible metric definitions, scopes, source/model/artifact/numeric
identities, backend/device, runtime/build configurations, workload lengths,
concurrency and warm/cold conditions. Differences require separate series and an
explicit experiment; the renderer never asserts cross-record comparability.

Retain distribution and sample count for latency. Component timing is not model
throughput. Wall/device/host spans may overlap; do not add nested durations.
Mapped, prepared, physical state and RSS byte classes may overlap as well.

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
