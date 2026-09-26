# YVEX Temporary Architectural Ledger

Status: temporary non-canonical research ledger

The workflow restrictions in this ledger apply only to a separately selected
TEMP research task. They are not persistent instructions for ordinary YVEX
engineering and do not override `AGENTS.md` or `ROADMAP.md`.

> This file is a versioned research surface, not a YVEX authority.
>
> It exists to preserve repository archaeology, architectural reconstruction,
> design reasoning, contradictions, working decisions, open questions, and
> future handoff material while canonical YVEX architecture remains owned by
> its existing repository authorities.
>
> Nothing becomes implemented, supported, qualified, accepted, planned,
> release-ready, or canonical merely because it appears in this file.
>
> Code, schemas, tests, contracts, architecture documents, family records,
> release records, and `ROADMAP.md` retain their existing authority.
>
> `ROADMAP.md` remains the sole live macro project-control authority.
>
> During that selected research task, the complete YVEX repository is read-only
> except for this exact path:
>
> ```text
> docs/TEMP.md
> ```

---

## 1. Purpose

YVEX is undergoing architectural refoundation and expansion.

The purpose of this ledger is to reconstruct the actual system with enough
precision that future architectural work extends real YVEX owners instead of
building parallel abstractions on an inaccurate mental model.

The research program is particularly interested in:

* compiler and IR boundaries;
* the active typed-program refoundation;
* physical model compilation;
* representation synthesis;
* quantization and physical recipes;
* hardware discovery and hardware topology;
* workload and capacity planning;
* storage, materialization, residency, and streaming;
* heterogeneous and future distributed execution;
* model acquisition and representation selection;
* artifact production and admission;
* deployment specialization;
* runtime and serving;
* state and scheduling;
* model-family breadth;
* Architecture Spectrum pressure;
* typed CLI/application surfaces;
* future YAI Studio integration.

The primary failure mode this document is designed to prevent is:

```text
incorrect reconstruction
    ->
incorrect abstraction
    ->
incorrect implementation
```

This document is therefore intentionally allowed to repeat important canonical
facts.

That redundancy exists for continuity, not authority duplication.

A future engineer or model should be able to open TEMP, inspect its references,
and reconstruct why an architectural conclusion was reached without depending
on previous conversation history.

Redundancy is permitted.

Untraceable authority is not.

---

## 2. Research Workflow

The current workflow deliberately separates reasoning from canonical
implementation.

```text
YVEX repository truth
        |
        v
repository archaeology
        |
        v
architectural discussion
        |
        v
working conclusions
        |
        v
docs/TEMP.md
        |
        v
independent reconciliation
        |
        v
canonical YVEX changes
```

TEMP sits between investigation and canonical implementation.

It is deliberately one step removed from production authority.

During the current phase:

* ChatGPT and the human operator perform architectural analysis and decide which
  conclusions are mature enough to persist;
* DeepSeek acts primarily as repository researcher and TEMP research recorder;
* Codex or another future implementation agent may later reconcile selected TEMP
  material against the then-current repository before updating canonical code,
  tests, contracts, architecture documents, or `ROADMAP.md`.

TEMP must preserve:

```text
what is known
what is inferred
what is proposed
what is accepted for research
what remains uncertain
what appears contradictory
```

---

## 3. Frozen Implementation Baseline

The current code-level checkpoint is:

```text
repository: yailabs/yvex
branch: models2
implementation_baseline:
  be53689d4ec355efdf750515ebf012f18da463e6
```

Commit meaning:

```text
IN-PROGRESS MAINTENANCE.ARCHITECTURE.REFOUNDATION.1 CHECKPOINT
```

It does NOT mean:

```text
Refoundation .1 complete
Refoundation .1 qualified
consumer cutover complete
ROADMAP milestone closed
release qualification achieved
```

At the moment this baseline was established:

```text
origin/models2 = be53689d4ec355efdf750515ebf012f18da463e6
worktree = clean
```

This implementation baseline is intentionally frozen for the current
documentation/research phase.

Subsequent `models2` commits may advance because `docs/TEMP.md` changes.

Therefore distinguish:

```text
implementation baseline
!=
current research-ledger HEAD
```

A later TEMP-only commit does not mean implementation changed.

---

## 4. Research Ledger State

Maintain this table whenever TEMP is updated.

| Field                            | Value                                      |
| -------------------------------- | ------------------------------------------ |
| Repository                       | `yailabs/yvex`                             |
| Branch                           | `models2`                                  |
| Frozen implementation baseline   | `be53689d4ec355efdf750515ebf012f18da463e6` |
| TEMP path                        | `docs/TEMP.md`                             |
| TEMP authority                   | `NON-CANONICAL`                            |
| Allowed research write surface   | `docs/TEMP.md` only                        |
| Current TEMP parent              | `UNSET`                                    |
| Current TEMP commit              | `UNSET`                                    |
| Last repository reconciliation   | `UNSET`                                    |
| Last architectural research pass | `UNSET`                                    |

Every substantial research pass must record the actual inspected repository
HEAD in addition to the frozen implementation baseline.

---

## 5. Repository Safety Contract

This section is mandatory.

### 5.1 Read authority

The research agent may inspect the complete repository.

This includes:

* source;
* headers;
* canonical documentation;
* contracts;
* tests;
* schemas;
* configuration;
* generated-authority inputs;
* Git history;
* commit metadata;
* diffs;
* blame/history;
* current branch and remote state;
* existing evidence and reports.

Repository-wide reading and searching are encouraged.

### 5.2 Write authority

The research agent may intentionally modify exactly one repository path:

```text
docs/TEMP.md
```

The effective access rule is:

```text
READ  = complete YVEX repository
WRITE = docs/TEMP.md only
```

No other tracked or untracked repository path may be intentionally:

```text
created
modified
deleted
renamed
formatted
generated
repaired
staged
committed
```

### 5.3 Canonical surfaces are read-only

Examples:

```text
ROADMAP.md
README.md
AGENTS.md
CHANGELOG.md
CONTRIBUTING.md
Makefile

src/**
include/**
tests/**
tools/**
config/**
.github/**

docs/** except docs/TEMP.md
```

This list is explanatory.

The actual rule remains:

```text
ONE WRITABLE FILE: docs/TEMP.md
```

---

## 6. Observe, Do Not Repair

An observed problem is research evidence.

It is not authorization to fix the problem.

The research agent must not:

* repair a stale build artifact;
* rebuild a dependency;
* update a dependency;
* replace a dependency pin;
* regenerate generated output;
* clean a cache;
* delete a build directory;
* fix a warning;
* modify a test;
* repair canonical documentation;
* update the ROADMAP;
* normalize formatting;
* clean the worktree;
* alter runtime state;
* acquire model weights.

Required behavior:

```text
observe
    ->
investigate
    ->
classify
    ->
record
    ->
stop mutation
```

Forbidden behavior:

```text
observe
    ->
decide it is harmless
    ->
repair it
```

---

## 7. No Build or Execution During Archaeology

Unless a later explicitly approved task changes this rule, repository
archaeology must not run:

```text
make
cmake
ninja
cargo
meson
ctest
pytest
pip
npm
pnpm
yarn
./yvex
```

Do not:

* run YVEX;
* run the test suite;
* run benchmarks;
* run generators;
* acquire models;
* update dependencies;
* rebuild REPLAI;
* clean build products;
* generate new evidence.

Existing source, tests, reports, logs, manifests, receipts, and evidence may be
read.

Tests are evidence to inspect, not commands to execute during this phase.

---

## 8. Git Safety Contract

Before editing TEMP, inspect at least:

```sh
git branch --show-current
git rev-parse HEAD
git status --short
git fetch origin models2
git rev-parse origin/models2
```

Expected branch:

```text
models2
```

Do not assume that current `HEAD` equals the frozen implementation baseline,
because TEMP-only commits may have advanced the branch.

### 8.1 Existing unrelated changes

If future sessions encounter modifications outside `docs/TEMP.md`:

* do not modify them;
* do not stage them;
* do not restore them;
* do not clean them;
* do not hide them;
* do not absorb them into the TEMP commit.

If they make safe TEMP-only work impossible:

```text
BLOCKED_BY_SHARED_WORKTREE
```

### 8.2 Remote divergence

If safe continuation requires merging, rebasing, resetting, or otherwise
changing repository state outside the TEMP boundary:

```text
BLOCKED_BY_REMOTE_ADVANCE
```

or:

```text
BLOCKED_BY_STALE_BASE
```

as appropriate.

Record exact commit identities.

Do not autonomously reconcile divergence.

### 8.3 Forbidden Git operations

Without explicit human authorization:

```text
git reset
git reset --hard
git clean
git restore
git checkout -- <path>
git stash
git merge
git rebase
git cherry-pick
git revert
git pull
git commit --amend
git push --force
git push --force-with-lease
```

Do not rewrite published history.

---

## 9. Documentation Discipline

TEMP should follow the intellectual style of serious YVEX documentation.

### 9.1 Language

Write technical documentation in English.

Preserve exact:

* identifiers;
* paths;
* command names;
* schema names;
* maturity states;
* family names;
* architecture terminology;
* repository vocabulary.

Do not casually rename existing concepts.

### 9.2 Style

Prefer:

* precise technical prose;
* explicit ownership;
* explicit non-responsibilities;
* bounded claims;
* exact symbols;
* source references;
* compact diagrams;
* structured tables;
* explicit evidence limits;
* explicit non-claims.

Avoid:

* marketing language;
* generic AI prose;
* vague praise;
* speculative certainty;
* unsupported terminology;
* unclassified idea dumps;
* fake completion language.

### 9.3 Existing concepts have priority

Before introducing a new term, search the repository for an existing authority
that may already own the intended meaning.

A new abstraction must earn its existence.

---

## 10. Required Non-Equivalences

The following distinctions must remain explicit unless repository evidence
demonstrates otherwise:

```text
model != source
source != representation
representation != artifact
artifact != profile
profile != engine
engine != session

logical identity != physical representation
physical representation != deployment specialization
deployment specialization != backend-local launch detail

storage != materialization
materialization != residency
device-addressable != physically device-resident
mapped bytes != GPU page residency
process RSS != device memory

family recognition != executable support
artifact completeness != execution support
component execution != complete model execution

telemetry != execution authority
measurement != qualification

generation != evaluation
evaluation != benchmark
benchmark != release qualification

search/calibration evidence != independent final qualification

Architecture Spectrum != supported-model catalog
```

Additional important non-equivalences discovered during research should be
recorded here.

---

## 11. Epistemic Classes

Every substantial TEMP claim must use one of these classifications:

```text
CANONICAL CONTEXT
REPOSITORY FACT
INFERENCE
PROPOSAL
OPEN QUESTION
ACCEPTED WORKING DECISION
CONTRADICTION
STALE OBSERVATION
```

### 11.1 CANONICAL CONTEXT

A bounded restatement or mirrored excerpt from an existing canonical or
normative YVEX owner.

The original source retains authority.

### 11.2 REPOSITORY FACT

A claim directly supported by the exact repository state inspected.

Evidence may come from:

* implementation;
* headers;
* schemas;
* tests;
* generated authorities;
* normative contracts;
* current architecture documentation;
* family records;
* current ROADMAP state.

The claim must stop where the evidence stops.

### 11.3 INFERENCE

A falsifiable conclusion derived from one or more repository facts.

A strong inference remains an inference.

### 11.4 PROPOSAL

A candidate future design, schema, API, algorithm, policy, command,
abstraction, or architecture.

A proposal must not use implemented-state language.

### 11.5 OPEN QUESTION

A boundary current evidence does not resolve.

Missing evidence remains missing.

### 11.6 ACCEPTED WORKING DECISION

A research decision explicitly accepted by the human operator during
architectural discussion.

It records alignment.

It does not update canonical YVEX authority.

### 11.7 CONTRADICTION

Two or more relevant sources appear materially inconsistent, ambiguous, or in
conflict.

Do not silently reconcile them.

### 11.8 STALE OBSERVATION

A previously useful TEMP claim no longer describes the current repository
snapshot.

Preserve its history and mark it stale.

---

## 12. Entry Identity and Provenance

Use stable TEMP identifiers:

```text
CC-      canonical context
RF-      repository fact
INF-     inference
PROP-    proposal
OQ-      open question
WD-      accepted working decision
CON-     contradiction
STALE-   stale observation
```

Example:

```text
RF-20260917-001
```

### 12.1 Standard repository fact

```text
classification: REPOSITORY FACT
id: RF-YYYYMMDD-NNN

claim:
  <one bounded claim>

provenance:
  repository: yailabs/yvex
  branch: models2
  implementation_baseline:
    be53689d4ec355efdf750515ebf012f18da463e6
  inspected_commit: <40-character SHA>
  path: <repository-relative path>
  symbol_or_section: <exact symbol, schema, heading, test, or identifier>
  lines: <real range | unspecified>

observed:
  <what the cited evidence directly establishes>

not_proven:
  <nearby conclusions that the evidence does not establish>

related_authorities:
  - <path + symbol/section>
  - <path + symbol/section>

checked_at:
  <ISO date>
```

### 12.2 Durable research address

Line numbers help navigation but are not durable semantic identity.

The primary research address is:

```text
commit + path + symbol_or_section
```

Never invent line numbers.

When necessary:

```text
lines: unspecified
```

### 12.3 Multi-source claims

If a conclusion depends on several sources, cite all relevant owners.

Do not use a convenient README paragraph as sole support for a claim that
depends on implementation, tests, and contracts.

---

## 13. Canonical Authority Map

Current working authority hierarchy:

| Information                           | Authority                           |
| ------------------------------------- | ----------------------------------- |
| Implemented behavior                  | Code, schemas, executable contracts |
| Demonstrated properties               | Applicable tests and evidence       |
| Mandatory agent rules                 | `AGENTS.md`                         |
| Macro project state and direction     | `ROADMAP.md`                        |
| Current architectural organization    | Architecture documentation          |
| Normative interface behavior          | Contracts                           |
| Family-specific evidence and barriers | Family records                      |
| Release gates                         | Release doctrine                    |
| Version-specific release scope        | Release records                     |
| Command metadata                      | Operator registry                   |
| Production source membership          | Source ownership authority           |
| Historical chronology                 | Git                                 |
| Temporary architectural research      | `docs/TEMP.md`                      |

TEMP owns only temporary research.

---

## 14. Mandatory Repository Reading Map

Initial archaeology should substantially inspect at least:

| Subject                  | Primary owner                              |
| ------------------------ | ------------------------------------------ |
| Agent rules              | `AGENTS.md`                                |
| Project state            | `ROADMAP.md`                               |
| Public product surface   | `README.md`                                |
| Documentation routing    | `docs/README.md`                           |
| Engineering method       | `docs/development/agentic-engineering.md`  |
| QA model                 | `docs/development/qa.md`                   |
| Source ownership         | `docs/development/source-ownership.md`     |
| GB10 targets/evidence    | `docs/development/gb10-targets.md`         |
| Reference evidence       | `docs/development/reference-baseline.md`   |
| System architecture      | `docs/architecture/system.md`              |
| Compilation architecture | `docs/architecture/compilation.md`         |
| Runtime architecture     | `docs/architecture/runtime.md`             |
| Command architecture     | `docs/architecture/commands.md`            |
| Artifact contract        | `docs/contracts/artifacts.md`              |
| Runtime contract         | `docs/contracts/runtime.md`                |
| Model storage            | `docs/contracts/model-storage.md`          |
| Events/telemetry         | `docs/contracts/events-telemetry.md`       |
| C API                    | `docs/contracts/c-api.md`                  |
| Local protocol           | `docs/contracts/local-protocol.md`         |
| Model lifecycle          | `docs/model-lifecycle.md`                  |
| Family integration       | `docs/model-families/integration.md`       |
| DeepSeek family record   | `docs/model-families/deepseek-v4-flash.md` |
| Mamba2 family record     | `docs/model-families/mamba2.md`            |
| MiniMax family record    | `docs/model-families/minimax-h3.md`        |
| Release doctrine         | `docs/releases/doctrine.md`                |
| v0.1 scope               | `docs/releases/v0.1.md`                    |
| Production ownership     | `config/source_owners.tsv`                 |
| QA catalog               | `config/qa/registry.json`                  |
| QA obligations           | `config/qa/obligations.json`               |
| Operation authority      | `config/operator/registry.json`            |

This is a starting map, not an exhaustive limit.

Repository references should be followed into their real owners and consumers.

---

## 15. Implementation Areas to Trace

Important repository areas include:

```text
src/source/

src/model/
src/model/compilation/
src/model/families/

src/ir/

src/graph/
src/graph/families/

src/artifact/
src/gguf/

src/deployment/

src/runtime/

src/backend/

src/server/

src/cli/
```

Corresponding public/internal interfaces under:

```text
include/yvex/
include/yvex/internal/
```

should be inspected where necessary.

Important concepts to trace include:

```text
source identity
logical model identity
catalog
model library
family semantics
Transformation IR
Semantic Model IR
Program / Execution IR
physical program
PEIR
physical variant
artifact identity
artifact admission
runtime binding
deployment specialization
hardware profile
workload profile
capacity planning
materialization
residency
engine resources
state
scheduler
backend capability
telemetry
evidence
```

---

## 16. Tests as Evidence

Tests are not decoration.

For important architectural claims, identify which tests actually exercise the
claimed property.

Potential areas include:

```text
IR construction and verification
IR serialization
family adaptation
Qwen architecture
DeepSeek execution
Mamba2 state / SSM
artifact admission
runtime binding
capacity planning
model catalog/library
engine lifecycle
session/state
residency/resources
telemetry
operator registry
tiny vertical / integration execution
```

A test proves only the property it actually exercises.

A passing component test does not establish complete model support.

---

## 17. Seeded Architectural Story

The following is a research hypothesis.

It must yield to repository truth.

```text
verified source
    |
    v
logical model / exact source identity
    |
    v
family interpretation
    |
    v
Semantic Model IR
    |
    v
Program / Execution IR
    |
    +------------------+
    |                  |
    |          Transformation IR
    |                  |
    |                  v
    +-------- physical representation / policy
                       |
                       v
               Physical Execution IR
                       |
                       v
                artifact emission
                       |
                       v
               artifact admission
                       |
                       v
                runtime binding
                       |
                       v
            deployment specialization
                       |
                       v
                engine generation
                       |
                       v
                 session / state
                       |
                       v
         scheduler / batches / worklists
                       |
                       v
                backend execution
                       |
                       v
                  typed results
                       |
                       v
        telemetry / measurements / evidence
                       |
                       v
       evaluation / benchmark / release
```

For every major boundary establish:

```text
owner
inputs
outputs
identity
lifetime
persistence
consumers
non-responsibilities
failure boundary
evidence boundary
```

Record where the simplified diagram:

* matches reality;
* compresses several boundaries;
* omits a real identity;
* uses transitional terminology;
* describes a target rather than current implementation.

---

## 18. Seeded Subsystem Authority Map

Current architecture indicates approximately:

| Boundary                                             | Owner to verify                                       |
| ---------------------------------------------------- | ----------------------------------------------------- |
| Source provenance, inventory, bounded payload trust  | `src/source/`                                         |
| Provider/remote discovery                            | `src/accounts/`, `src/model/remote.c`, catalog owners |
| Local source/package/model catalog                   | `src/model/` owners                                   |
| Family interpretation                                | `src/model/families/`                                 |
| Transformation and physical compilation              | `src/model/compilation/`                              |
| Typed computational IR                               | `src/ir/`                                             |
| Family graph composition                             | `src/graph/` and family graph owners                  |
| GGUF/qtypes/writer                                   | `src/gguf/`                                           |
| Artifact integrity/admission                         | `src/artifact/`                                       |
| Materialization                                      | model materialization owners                          |
| Capacity/deployment planning                         | `src/deployment/` and related interfaces              |
| Runtime binding/engines/sessions/residency/scheduler | `src/runtime/`                                        |
| Device capabilities and kernels                      | `src/backend/`                                        |
| Generation composition                               | typed generation/runtime owners                       |
| Persistent host and routing                          | `src/server/`                                         |
| Command projection                                   | operator registry + `src/cli/`                        |

The archaeology pass must replace approximate ownership with exact paths and
symbols where useful.

---

## 19. Identity and Lifetime Ledger

Start with these known product concepts:

```text
Model
Source
Artifact
Profile
Engine
Session
Host
```

For each identity-bearing object reconstruct:

| Property         | Question                           |
| ---------------- | ---------------------------------- |
| Authority        | Who creates and owns it?           |
| Semantic meaning | What distinction does it encode?   |
| Lifetime         | How long can it exist?             |
| Persistence      | Serialized or process-local?       |
| Authentication   | What establishes trust?            |
| Parent lineage   | Which identities precede it?       |
| Consumers        | Who consumes it?                   |
| Invalidations    | What makes it stale?               |
| Non-equivalences | What must not be confused with it? |

Special attention:

```text
logical model identity
source snapshot identity
transformation identity
physical variant identity
artifact identity
PEIR/package identity
runtime binding identity
deployment specialization identity
engine generation
session identity
state identity/version
execution batch lineage
expert worklist lineage
telemetry/evidence identity
```

---

## 20. Refoundation .1

The frozen implementation checkpoint belongs to:

```text
MAINTENANCE.ARCHITECTURE.REFOUNDATION.1
```

This refoundation is not merely a code cleanup.

The research objective is to reconstruct the actual migration toward a typed,
multi-level computational architecture.

Study separately:

```text
source interpretation
Semantic Model IR
Program / Execution IR
Transformation IR
parameter physical projection
Physical IR
Physical Execution IR
Target / Schedule IR
runtime binding
runtime state
backend implementation
evidence publication
```

Do not collapse these concepts merely because they participate in one
source-to-execution path.

### 20.1 Consumer cutover matrix

For each important consumer record:

| Field               | Meaning                                |
| ------------------- | -------------------------------------- |
| Family              | Model family                           |
| Consumer            | Real executing consumer                |
| Previous authority  | Superseded/transitional representation |
| New authority       | Typed representation actually consumed |
| Status              | Migrated / transitional / not migrated |
| Compatibility layer | Remaining importer/legacy form         |
| Runtime dependency  | Actual downstream consumer             |
| Tests               | Applicable tests                       |
| Evidence            | What has been demonstrated             |
| Remaining non-claim | What remains unproved                  |

Seed hypotheses requiring verification include:

* substantial Qwen forward/output physical SSA consumption;
* selected DeepSeek stages migrated to typed programs;
* incomplete DeepSeek heterogeneous composition;
* incomplete MiniMax neural-program cutover;
* pure-SSM representability without fake attention/KV obligations;
* incomplete A01/Mamba2 executable support.

The existence of an IR structure is not sufficient.

Find the executing consumer.

---

## 21. Program P: Physical Model Compiler

Seed relationship:

```text
quantization
    ⊂
representation synthesis
    ⊂
physical model compilation
```

The target research problem is approximately:

```text
immutable verified source
      +
semantic model
      +
execution objective
      +
quality constraints
      +
target-machine facts
      |
      v
representation space
      |
      v
sensitivity / calibration evidence
      |
      v
feasibility filtering
      |
      v
candidate recipes
      |
      v
bounded candidate builds / probes
      |
      v
quality + resource + performance observations
      |
      v
Pareto frontier
      |
      v
selected reproducible recipe
      |
      v
deterministic build
      |
      v
artifact admission
      |
      v
independent qualification
```

This target must not be described as an implemented automatic service unless
repository evidence proves it.

### 21.1 Existing substrate to map

Investigate:

```text
source verification
family semantic projection
Transformation IR
physical policy
physical variant
qtype support
quantization synthesis
PEIR
artifact construction
artifact admission
runtime binding
hardware profile
workload profile
capacity planning
materialization
residency
measurement
model library
```

### 21.2 Strategic gap to map

Investigate what remains open/manual around:

```text
search-space construction
sensitivity analysis
calibration-driven choice
candidate generation
feasibility pruning
cost modeling
empirical candidate trials
multi-objective optimization
Pareto selection
automatic recommendation
reproducible search provenance
```

Existing owners should be extended before parallel infrastructure is proposed.

---

## 22. Physical Recipe Research

classification: OPEN QUESTION

Central question:

> What exactly constitutes a physical recipe in YVEX?

Do not answer this by immediately inventing a new struct.

First map every existing decision affecting package representation or
deployment.

Candidate dimensions to investigate:

```text
dtype / qtype
per-role representation
per-layer representation
per-tensor representation
group geometry
channel geometry
scale representation
layout
packing
alignment
sharing
source transformation
artifact/container representation
runtime-state representation
backend implementation eligibility
materialization requirements
placement constraints
residency classes
workload reserve
calibration identity
quality constraints
hardware compatibility
```

Classify each actual decision as:

```text
semantic model fact
artifact-affecting physical decision
package metadata
deployment constraint
deployment specialization
backend-local implementation detail
runtime mutable state
measurement/evidence
search-only metadata
```

Do not place transient machine state into artifact identity merely because it
influences selection.

---

## 23. Hardware and Resource Baseline

Known starting symbols include:

```text
yvex_execution_hardware_profile
yvex_execution_workload_profile
yvex_execution_capacity_plan_request
yvex_execution_capacity_plan
```

Known starting owner:

```text
include/yvex/internal/deployment.h
```

Bootstrap-observed hardware-profile concepts include:

```text
backend
device index
compute capability
SM count
copy-engine count
L2
total memory
usable memory
sustainable read bandwidth
sustainable copy bandwidth
host page geometry
device page geometry
unified addressing
coherent host memory
virtual memory
graph capture
native architecture code
identity
```

The previous repository study observed that the capacity request accepts one
selected hardware-profile pointer.

That local observation must be checked against the complete repository before a
broader architectural conclusion is recorded.

---

## 24. Hardware Research Vocabulary

classification: ACCEPTED WORKING DECISION

For TEMP research, keep these concepts distinct:

| Concept     | Working meaning                                         |
| ----------- | ------------------------------------------------------- |
| Inventory   | Which hardware/storage resources exist                  |
| Topology    | How those resources are structurally related            |
| Capability  | Which execution/representation classes are legal        |
| Calibration | Deliberate measurement of concrete machine behavior     |
| Profiling   | Detailed characterization of execution behavior         |
| Telemetry   | Observations emitted during actual operation            |
| Evidence    | Provenance and trust attached to measurements or claims |

This vocabulary guides research.

It is not yet a canonical YVEX schema.

---

## 25. Future Hardware Pressure

classification: OPEN QUESTION

Evaluate the current architecture against future requirements without claiming
they exist today:

```text
multiple GPUs in one host
heterogeneous CPU/GPU systems
unified-memory systems
multiple DGX Spark nodes
Mac + DGX Spark
large-memory Macs
DGX Station / servers
multi-node LAN execution
NVMe / SSD-backed representations
file-backed parameter tables
conditional sparse parameter memory
selective residency
streaming parameter populations
tensor parallelism
pipeline parallelism
expert parallelism
other sharding and partitioning strategies
```

For every future capability distinguish:

```text
describable
plannable
buildable
admissible
deployable
executable
qualified
```

Representability is not execution.

Execution is not qualification.

---

## 26. Storage, Materialization, Residency, and Resources

Map relationships among:

```text
persistent source storage
artifact/package storage
mapped package bytes
host-canonical resources
device-addressable mappings
prepared layouts
explicit device allocations
workspace
activation arenas
session state
transient resources
logical movement
physical page residency
process RSS
device-memory observations
```

Important investigation areas include:

```text
src/model/materialization*
src/runtime/
src/backend/
artifact mapping owners
residency owners
runtime resource catalogs
runtime contracts
events/telemetry contracts
```

Record where YVEX already distinguishes:

```text
storage backing
working set
prepared representation
logical residency
physical residency
backend resource
```

Record where implementation still assumes one selected device, one primary
backing, one complete tensor population, or another simplification that matters
for future Program P.

Do not infer physical page residency from addressability alone.

---

## 27. Model Catalog and Library

Known starting symbols include:

```text
yvex_remote_model
yvex_model_representation
yvex_local_source_record
yvex_local_package_record
yvex_model_library
yvex_model_library_entry
yvex_model_artifact_fact
yvex_model_runtime_profile_fact
```

Primary starting owner:

```text
include/yvex/catalog.h
```

Reconstruct:

```text
what each record owns
what each record only projects
how logical identity is joined
how remote representations relate to local source
how artifacts relate to models
how profiles relate to launchability
how publications are represented
what existing recommendation fields mean
who authors recommendation fields
```

Do not introduce a second model registry without demonstrating an ownership gap.

---

## 28. Model Lifecycle

Seed operator lifecycle:

```text
search
    ->
pull
    ->
prepare
    ->
load
    ->
chat
```

Separate operations include:

```text
unload
evict
storage inspection
push
```

Investigate exact states and identities around:

```text
remote availability
remote representation
local source
verified source
derived representation
artifact
artifact admission
runtime binding
READY deployment/profile
loaded engine generation
session
```

Do not manufacture one combined lifecycle enum from a documentation diagram.

Preserve:

```text
downloaded != prepared
prepared != loaded
loaded != actively used
unloaded != evicted
source available != executable
```

---

## 29. Representation Recommendation Gap

classification: OPEN QUESTION

The current library already answers significant parts of:

> What models, sources, artifacts, publications, and launchable profiles are
> known?

The future product direction introduces a different question:

> Given this logical model, execution objective, and available hardware, what
> representation should exist?

Research separately:

```text
existing local artifact
existing remote artifact
existing local source
buildable representation
representation requiring preparation
launchable deployment
hardware-compatible candidate
unsupported candidate
candidate requiring new compilation
```

Do not conflate:

```text
discovery
recommendation
compilation
admission
deployment
execution
```

Recommendation may suggest.

Admission proves.

---

## 30. CLI, API, and Product Projection

YVEX exposes one product executable:

```text
yvex
```

Known domain objects include:

```text
Model
Source
Artifact
Profile
Engine
Session
Host
```

Command metadata is driven by:

```text
config/operator/registry.json
```

The operation registry does not own domain capability.

Research the actual path among:

```text
operation registry
CLI parser
dispatch
typed domain/application API
typed result
human renderer
JSON renderer
local protocol
offline operation
runtime-client operation
persistent host
```

Future YAI Studio integration should consume typed YVEX authority.

The intended architectural direction is not:

```text
GUI
    ->
parse CLI text
```

but rather:

```text
CLI --------+
            |
YAI Studio -+-> shared typed YVEX authority
            |
other API --+
```

The exact application-facing boundary still requires repository-grounded
analysis.

---

## 31. Family Boundary

A model family owns irreducible model semantics.

Candidate owner levels include:

```text
src/model/families/<family>.c
src/graph/families/<family>.c
src/backend/<backend>/families/<family>.*
```

Verify the exact current contract.

Family ownership must not silently absorb generic:

```text
artifact I/O
allocation
session lifecycle
generic persistent-state storage
workspace ownership
backend memory management
telemetry
claim promotion
```

The current architecture forbids a parallel runtime per family.

Backend and machine identity must not be smuggled into logical model identity.

---

## 32. Current Family Evidence

Reconstruct from repository truth.

Seed matrix:

| Family / target            | Current boundary to verify                         | Important non-claim                                        |
| -------------------------- | -------------------------------------------------- | ---------------------------------------------------------- |
| DeepSeek V4 Flash / DSpark | Source-to-hosted text and speculation              | No release quality/performance promotion                   |
| Qwen3.8-27B text           | BF16 text specialization and hybrid text execution | Text support does not imply all upstream modalities        |
| MiniMax H3                 | Composite/component/media execution                | Bounded execution does not prove useful full-model quality |
| Mamba2                     | Source/roles/state/component evidence              | No complete READY hosted generation                        |
| Gemma observations         | Source/header/candidate-role observations          | Not executable family support                              |

For each family inspect:

```text
model family owner
graph family owner
backend specialization if present
family documentation
tests
ROADMAP state
artifact evidence
runtime evidence
release evidence
```

Evidence depth remains family-specific.

Do not flatten it into one generic "supported" state.

---

## 33. Architecture Spectrum

The Architecture Spectrum is an architectural pressure/falsification suite.

It is not automatically the YVEX v1 product-support catalog.

Bootstrap matrix requiring verification:

| ID  | Pressure                                         | State   |
| --- | ------------------------------------------------ | ------- |
| A01 | Pure SSM                                         | PARTIAL |
| A02 | Pure recurrent                                   | PLANNED |
| A03 | Encoder-decoder                                  | PLANNED |
| A04 | Diffusion language model                         | PLANNED |
| A05 | Hybrid SSM + attention                           | PLANNED |
| A06 | Hybrid SSM + attention + MoE                     | PLANNED |
| A07 | Local/sliding attention                          | PLANNED |
| A08 | Encoder-only / retrieval                         | PLANNED |
| A09 | Audio / speech                                   | PLANNED |
| A10 | Unified image generation                         | PLANNED |
| A11 | Phase-asymmetric conditional sparse architecture | PLANNED |

For every row recover:

```text
exact target
architectural pressure
assumption being falsified
current state
explicit non-authorization
```

A PLANNED pressure target is not implementation authorization.

The final v1 support set remains a separate future decision.

---

## 34. Telemetry, Measurement, and Evidence

YVEX already contains substantial typed telemetry.

Therefore:

```text
"YVEX has no telemetry"
```

is not an acceptable research conclusion.

Map separately:

```text
operational events
execution measurements
resource observations
runtime telemetry
detailed profiling
audit evidence
numerical evidence
behavior evaluation
benchmark evidence
release qualification
```

The open question relevant to current research is narrower:

> Which inventory, topology, calibration, planner-search, distributed-resource,
> and hardware-evidence concepts remain absent or insufficient?

Observation remains downstream of execution authority.

---

## 35. Evidence Ladder

Preserve distinct evidence levels.

At minimum:

```text
software test
structural/source proof
tensor proof
artifact proof
numerical conformance
runtime/lifecycle qualification
component execution
component benchmark
complete generation
behavior evaluation
whole-model benchmark
release qualification
```

Examples:

```text
tensor proof != complete artifact
complete artifact != supported artifact
artifact admission != execution
materialization != execution
component execution != generation
component benchmark != whole-model benchmark
generation != behavior evaluation
search evidence != final qualification
```

Missing evidence remains missing.

---

## 36. Contradiction Register

When relevant authorities disagree, record the conflict.

| ID        | Topic | Source A | Source B | Conflict | Resolution |
| --------- | ----- | -------- | -------- | -------- | ---------- |
| `CON-...` |       |          |          |          | `OPEN`     |

Every contradiction should include:

```text
inspected commit
path A
symbol/section A
line range A if available

path B
symbol/section B
line range B if available

precise disagreement
authority analysis
additional evidence needed
```

Do not silently "fix" contradictory sources inside TEMP.

---

## 37. Open Question Register

| ID       | Question | Blocks | Evidence needed | Status |
| -------- | -------- | ------ | --------------- | ------ |
| `OQ-...` |          |        |                 | `OPEN` |

A well-formed open question is useful research output.

Do not close uncertainty for cosmetic completeness.

---

## 38. Accepted Working Decisions

Only research decisions explicitly accepted during architectural discussion
belong here.

Template:

```text
classification: ACCEPTED WORKING DECISION
id: WD-YYYYMMDD-NNN

decision:
  ...

reason:
  ...

supported_by:
  - RF-...
  - INF-...

affected_research:
  ...

canonical_status:
  NON-CANONICAL
```

Silence is not acceptance.

A TEMP working decision is not a ROADMAP decision.

---

## 39. Proposal Admission Rule

A proposal is allowed only after current ownership has been mapped.

Template:

```text
classification: PROPOSAL
id: PROP-YYYYMMDD-NNN

problem:
  ...

current_owner:
  ...

repository_facts:
  - RF-...

proposed_change:
  ...

why_current_owner_is_insufficient:
  ...

authority_after_change:
  ...

explicit_non_responsibilities:
  ...

identity_effect:
  ...

persistence_effect:
  ...

runtime_effect:
  ...

compatibility_effect:
  ...

evidence_required:
  ...

open_questions:
  ...
```

Do not create a parallel:

```text
loader
registry
scheduler
runtime
state manager
telemetry plane
compiler
artifact authority
```

without proving that the existing owner cannot correctly absorb the new
requirement.

---

## 40. Initial Reconciliation Targets

The first archaeology phase should give special attention to the following.

### 40.1 PEIR versus deployment specialization

Determine exactly what current PEIR owns.

Determine exactly what it excludes.

Check:

```text
architecture documentation
artifact contract
compiler implementation
binding implementation
runtime specialization
tests
```

Record genuine drift as a contradiction.

### 40.2 Hardware profile versus topology

Determine whether current planning is fundamentally based on:

```text
one selected hardware profile
```

or whether a broader device/topology authority already exists elsewhere.

Do not answer from one struct alone.

### 40.3 Model Library versus recommendation

Determine what current recommendation-related fields actually mean.

Identify who authors them and whether they are:

```text
provider metadata
static hint
catalog observation
runtime fact
policy decision
```

### 40.4 Resource catalog versus selective residency

Determine what the current resource model already permits.

Separate structural representability from an implemented automatic residency
policy.

### 40.5 Refoundation .1 cutover

Identify remaining legacy/transitional consumers preventing the typed compiler
stack from becoming the unique serving authority.

### 40.6 Program P substrate

Identify existing YVEX owners that can already serve as inputs to a future
physical-recipe search loop.

---

## 41. Research Workstreams

TEMP should evolve through bounded research passes.

### A. Repository archaeology

Goal:

```text
high-confidence authority and identity map
```

### B. Refoundation .1

Goal:

```text
exact typed compiler/runtime cutover status
```

### C. Program P

Goal:

```text
existing physical-compiler substrate
+
actual missing search/optimization layer
```

### D. Hardware and resources

Goal:

```text
current hardware facts
workload
capacity
materialization
residency
specialization
measurement
future topology seams
```

### E. Model Library

Goal:

```text
remote/source/artifact/profile/engine relationships
+
future representation-recommendation seam
```

### F. Family and spectrum

Goal:

```text
current family evidence
+
future architectural pressure
```

### G. Product projection

Goal:

```text
typed authority exposed through CLI/API
+
future YAI Studio consumption
```

One research pass should remain bounded.

Do not solve the entire project in a single TEMP update.

---

## 42. DeepSeek Research Pass Format

Append one entry for every substantial DeepSeek research pass.

```text
## Research Pass YYYY-MM-DD / NNN

topic:

frozen_implementation_baseline:
  be53689d4ec355efdf750515ebf012f18da463e6

inspected_head:
remote_models2_head:

scope:

files_read:
  - ...

symbols_inspected:
  - ...

tests_inspected:
  - ...

repository_facts_added:
  - RF-...

canonical_context_added:
  - CC-...

inferences_added:
  - INF-...

proposals_added:
  - PROP-...

open_questions_added:
  - OQ-...

contradictions_added:
  - CON-...

working_decisions_added:
  - WD-...

important_negative_searches:
  - ...

uncertainties:
  - ...

next_bounded_research_boundary:
  ...
```

"Not found" must state what was searched.

Search failure alone is not proof of absence.

---

## 43. Session Chronicle

End each meaningful TEMP-writing session with:

```text
## Session Chronicle YYYY-MM-DD / NNN

frozen_implementation_baseline:
  be53689d4ec355efdf750515ebf012f18da463e6

research_base_head:
remote_models2_head:
temp_parent_commit:
temp_result_commit:

research_objective:

sources_read:
  - ...

facts_added:
  - ...

inferences_added:
  - ...

proposals_added:
  - ...

questions_added:
  - ...

contradictions_found:
  - ...

stale_entries_found:
  - ...

rejected_hypotheses:
  - ...

repository_paths_intentionally_modified:
  - docs/TEMP.md

builds_or_tests_run:
  NONE

dependency_or_build_artifacts_modified:
  NONE

diff_check:
  PASS | FAIL

staged_path_check:
  PASS | FAIL

remote_divergence_check:
  PASS | FAIL

next_bounded_research_task:
  ...
```

---

## 44. TEMP Commit Safety Gate

A TEMP commit is allowed only when all applicable conditions pass.

### Repository state

```text
[ ] Branch is models2.
[ ] Current HEAD is recorded.
[ ] origin/models2 is recorded.
[ ] Frozen implementation baseline remains known.
[ ] Any unrelated changes were preserved.
[ ] No merge/rebase/reset/cleanup occurred.
```

### Mutation boundary

```text
[ ] docs/TEMP.md is the only intentionally modified path.
[ ] No source changed.
[ ] No header changed.
[ ] No test changed.
[ ] No canonical documentation changed.
[ ] ROADMAP.md did not change.
[ ] No generated file changed.
[ ] No build artifact was deleted or regenerated.
[ ] No dependency was downloaded, rebuilt, or updated.
[ ] No unrelated untracked file was created.
```

### Research discipline

```text
[ ] Important repository facts contain provenance.
[ ] Symbols/sections are exact where available.
[ ] Line numbers are real or marked unspecified.
[ ] Inferences remain inferences.
[ ] Proposals remain proposals.
[ ] PLANNED targets were not promoted into support.
[ ] Evidence claims stop at their demonstrated boundary.
[ ] Contradictions remain visible.
[ ] Negative searches retain their scope.
```

### Diff

Before staging:

```sh
git status --short
git diff -- docs/TEMP.md
git diff --check -- docs/TEMP.md
```

Stage exactly:

```sh
git add -- docs/TEMP.md
```

Then verify:

```sh
git diff --cached --name-only
git diff --cached --check
git diff --cached -- docs/TEMP.md
```

The staged path set must equal exactly:

```text
docs/TEMP.md
```

This staged-set check is the decisive write-boundary check.

---

## 45. Commit and Push Rule

TEMP commits should use focused messages such as:

```text
docs(temp): establish architectural research ledger
docs(temp): map compiler authority boundaries
docs(temp): reconcile physical compiler substrate
docs(temp): map hardware planning boundaries
docs(temp): record representation search analysis
```

Before push:

```sh
git fetch origin models2
git rev-parse origin/models2
```

If the remote advanced unexpectedly relative to the researched base:

```text
BLOCKED_BY_REMOTE_ADVANCE
```

Do not:

```text
pull
merge
rebase
reset
force push
```

Report the divergence for human handling.

---

## 46. Codex Reconciliation Queue

TEMP does not authorize canonical implementation.

Material believed mature enough for later promotion may be placed here.

| TEMP ID | Claim / proposal | Class | Evidence | Codex verdict  | Candidate canonical owner | Verification |
| ------- | ---------------- | ----- | -------- | -------------- | ------------------------- | ------------ |
|         |                  |       |          | `NOT REVIEWED` |                           |              |

Possible later reconciliation verdicts:

```text
TRUE / CURRENT
TRUE / STALE
PARTIALLY TRUE
DEEPSEEK INFERENCE
WRONG
PROPOSAL REQUIRING DECISION
```

Codex must independently inspect the then-current repository before promotion.

TEMP must never become:

```text
implement everything in this file
```

---

## 47. Initial Bootstrap Research Objectives

After the ledger itself has been committed, the first archaeology pass should
prioritize reconstruction over invention.

Required goals:

```text
1. Validate TEMP against the current models2 repository.
2. Record exact inspected HEAD and remote identity.
3. Read the mandatory canonical source map.
4. Follow important architecture claims into implementation.
5. Follow important implementation claims into tests where relevant.
6. Expand the subsystem authority map.
7. Expand the identity/lifetime map.
8. Reconstruct Refoundation .1 consumer cutover.
9. Reconstruct Program P existing substrate versus OPEN search layer.
10. Reconstruct hardware/workload/capacity ownership.
11. Reconstruct storage/materialization/residency ownership.
12. Reconstruct Model Library and lifecycle ownership.
13. Reconstruct current family evidence.
14. Reconstruct Architecture Spectrum A01-A11.
15. Record contradictions.
16. Record important open questions.
17. Avoid broad future architecture proposals during the first pass.
```

The first successful research pass is measured by trustworthiness, not length.

---

## 48. DeepSeek Evaluation Criteria

The current DeepSeek experiment should be evaluated on:

| Property                   | Success condition                                   |
| -------------------------- | --------------------------------------------------- |
| Repository safety          | No intentional mutation outside TEMP                |
| Operational restraint      | No autonomous repair/build/download/cleanup         |
| Provenance                 | Important facts resolve to real repository evidence |
| Architecture comprehension | Existing owner boundaries remain intact             |
| Identity discipline        | Nearby lifecycle objects remain distinct            |
| Evidence discipline        | Claims stop at demonstrated evidence                |
| Family discipline          | Family semantics do not absorb generic mechanisms   |
| Contradiction handling     | Conflicts remain explicit                           |
| Negative-search honesty    | Absence is not invented                             |
| Writing quality            | TEMP remains readable and structured                |
| Continuity                 | Later sessions can resume from TEMP itself          |

A long document with weak provenance is a failure.

The desired combination is:

```text
high architectural coverage
+
high evidentiary precision
+
zero unauthorized mutation
```

---

## 49. Bootstrap Completion Boundary

The initial archaeology phase is sufficiently mature when TEMP contains a
repository-grounded reconstruction of at least:

```text
project control
system authority
identity/lifetime model

source intake
family interpretation

Semantic Model IR
Program / Execution IR
Transformation IR

physical representation
physical policy
PEIR

artifact construction
artifact admission
runtime binding

deployment specialization

hardware profile
workload profile
capacity planning

storage
materialization
residency
resource ownership

runtime engine
session/state
scheduler
execution batches/worklists

backend boundary

telemetry
measurement
evidence

model catalog
model library
model lifecycle

current family boundaries
Architecture Spectrum

Program P implemented substrate
Program P open strategic search layer

major contradictions
major unresolved questions
```

Only after that boundary should substantial new architecture accumulate in
TEMP.

Until then:

```text
ARCHAEOLOGY BEFORE INVENTION.

EVIDENCE BEFORE PROMOTION.

OBSERVE BEFORE REPAIR.

ONE WRITABLE FILE.
```
