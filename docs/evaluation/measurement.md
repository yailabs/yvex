<!-- docs:metadata
title: Measurement and Observation
id: yvex.evaluation.measurement
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Measurement and Observation

**Typed observations report execution without controlling it.**

[Up](README.md)

## Measurement plane

One execution-measurement schema qualifies work with scope, clock, composition,
unit, duration, and explicit rate denominators. Cumulative decode covers the
complete committed decode interval; rolling decode uses its own recent work and
duration, currently up to 32 token intervals. Host and device measurements may
overlap and retain that fact rather than being forced into a synthetic sum.
The stage projection subtracts only disjoint host spans from total generation
wall and publishes the remaining wall as `unattributed`; overlapping
attention, component, and synchronization spans remain excluded from that sum.
An ordinary decode span owns only its model step, so its output, sampling,
state, detokenization, and publication children remain separately additive. A
speculative decode span instead encloses one complete draft/verify/commit
iteration; those child facts remain visible but are not added to the enclosing
span a second time. This composition difference is typed rather than inferred
from a family name.
MiniMax evaluation/frame/sample work uses the same generic record without
introducing media semantics into scheduler or telemetry ownership.

Admission checks configured limits, cgroup capacity, live system availability,
and backend memory facts before creating large resources. Live availability is
not part of model/package identity. Failure closes acquired resources and never
publishes a partially ready engine.

One engine-owned resource catalog records canonical mappings, component
resources, prepared tensor/group/layout views, backend handles, executable
caches, sequence state, workspace, and temporaries as separate typed entries.
Entries bind engine generation, package provenance, specialization and
admission identities when applicable, numerical class, byte classes,
dependencies, borrows, readiness, and release policy. A prepared entry may be
published or evicted independently; eviction invalidates its handle without
changing the canonical mapping or package identity.

The current text residency owner still selects one primary backing for the
complete admitted tensor population. The catalog makes selective preparation
and independent release legal and is exercised with a bounded synthetic
prepared layout, but no rejected DeepSeek cache or optimized selective weight
layout is retained. `prepared_bytes` alone therefore remains no performance or
residency claim.


## Evidence plane

### Source-to-output qualification coverage

Program P joins the existing authorities below; these are not independent
telemetry stores or a new sum of request costs. Each observation retains its
own target, clock, workload population and availability. Resource classes and
host/device intervals may overlap. An unavailable measurement is not zero.

| Boundary | Canonical authority / observation | Claim boundary |
| --- | --- | --- |
| Source, compilation, artifact and binding | Authenticated source, compiler/quant production receipts and independent reader | Production time and structural validity do not establish model quality |
| Specialization and admission | Deployment/resource-capacity owners | Live availability is volatile; encoded artifact size alone is not execution fit |
| Loading and weight preparation | Engine lifecycle and resource catalog | Mapped, explicit and prepared bytes remain distinct; addressability is not physical residency |
| Request, tokenization and prefix | Generation profile and exact prompt/reused/new-position counters | Prefix reuse is not prefetch; only newly executed positions enter the prefill denominator |
| Prefill, forward and decode | Typed execution measurements and exact committed populations | Autoregressive, recurrent, finite-forward and iterative workloads retain different applicable metrics |
| Logits, sampling, verification and commit | Existing scoped generation profile and speculative terminal counters | Nested/enclosing/overlapping phases cannot be added indiscriminately |
| Detokenization and producer publication | Scoped generation profile | Producer publication is not transport receive or terminal paint |
| Client delivery | Native/HTTP adapters, monotonic fragment observations | Fragments are not token events; missing individual token correspondence prevents token-ITL claims |

The native qualification runner retains received completed profile events in
each observation's `execution_phases`. It copies the producer's scope, clock,
composition, work unit and availability; no second profiler is enabled. Durations,
denominators and rates without their native availability bit are `null`.
Duplicate completed phases, foreign Session/turn lineage and unsupported typed
measurement versions refuse. Untyped diagnostic counters remain only in the raw
journal. No received typed phase means `execution_phase_status=unavailable`;
it does not imply all phases were measured at zero cost. The runner does not
request a higher producer trace level to fill this gap.

These observations are retained in the existing qualification receipt, not
automatically promoted to comparable performance rows or independent model
evidence. The additional client projection/journaling work belongs to the
measurement adapter's exact build; old timing samples are not silently upgraded.

Production execution publishes lightweight counters, typed events, physical
facts, and compact lineage handles. Audit, numerical conformance, profiling, and
benchmark owners assemble richer records outside the ordinary execution ABI.
Trace verbosity does not change numerical behavior, and production does not
materialize full hidden/logit/state rows merely to hash them.

Cold package identities and transactional state identities remain complete.
Within one authenticated engine generation, transient batches use generation,
source ordinal, and engine-owned handles. Retained evidence can join those
handles back to package, binding, specialization, session, and operation
lineage.
