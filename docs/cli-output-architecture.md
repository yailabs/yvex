# CLI Output Architecture

This document records the completed `CLI.ARCH.AUDIT.0` inventory and doctrine.
It is an architecture audit only: it does not change command behavior, add a
renderer, remove output flags, or implement JSON output.

The production inventory below counts direct C/CUDA output forms in the source
tree, excluding `tests/`, `build/`, and documentation. The counted forms are
`printf`, `fprintf`, `puts`, `fputs`, `putchar`, `perror`, `vprintf`, and
`vfprintf`. No direct-print macros were found in the scanned production tree.
Function recovery and surface classification are heuristic, so the file and
call-kind totals are the authoritative part of this audit.

## Print Inventory Summary

Manual production print sites: 7319.

| File group | Count |
| --- | ---: |
| Root CLI/domain files, `yvex_*.c` | 6901 |
| GGUF files, `gguf/*.c` | 404 |
| Daemon/server, `yvexd.c` and `yvex_server.c` | 14 |
| CUDA files, `cuda/*.c` and `cuda/*.cu` | 0 |
| Public headers, `include/yvex/*.h` | 0 |

| Print kind | Count |
| --- | ---: |
| `printf` | 5616 |
| `fprintf` | 1645 |
| `fputs` | 46 |
| `putchar` | 11 |
| `vprintf` | 1 |
| `puts` | 0 |
| `perror` | 0 |
| `vfprintf` | 0 |

| Stream class | Count |
| --- | ---: |
| stdout | 5624 |
| file pointer | 902 |
| stderr | 762 |
| trace-like helper | 21 |
| indent helper | 6 |
| comma helper | 3 |
| argv-like text | 1 |

| Surface class | Count |
| --- | ---: |
| porcelain/human output | 5143 |
| error/refusal output | 879 |
| diagnostic/evidence output | 827 |
| plumbing/raw-like output | 339 |
| log/daemon output | 131 |

## Top Files by Manual Print Count

| File | Count | Current pressure |
| --- | ---: | --- |
| `yvex_model_artifacts.c` | 2581 | model/download/fullmodel report walls and audit records |
| `yvex_model.c` | 1409 | model-target, tensor-map, mapping gate, qtype policy, list/table output |
| `yvex_graph.c` | 690 | graph reports, primitive proofs, execution diagnostics |
| `yvex_runtime.c` | 432 | runtime reports and operator status surfaces |
| `yvex_artifact.c` | 365 | artifact inspect/metadata/tensor surfaces |
| `yvex_kv.c` | 314 | KV reports and diagnostic surfaces |
| `yvex_source.c` | 308 | source manifest, native inventory, provenance reports |
| `gguf/tools.c` | 305 | conversion/tooling output and raw-ish evidence |
| `yvex_generation.c` | 208 | diagnostic generation and trace output |
| `yvex_accounts.c` | 95 | account/provider status output |
| `gguf/quant.c` | 80 | quant tooling output |
| `yvex_fs.c` | 75 | path and filesystem status output |
| `yvex_decode.c` | 69 | diagnostic decode output |
| `yvex_chat.c` | 65 | interactive shell/status output |
| `yvex_sampling.c` | 65 | diagnostic sampling output |
| `yvex_logits.c` | 64 | diagnostic logits output |
| `yvex_backend.c` | 45 | backend status/probe output |
| `yvex_tokenizer.c` | 42 | tokenizer diagnostic output |
| `yvex_metrics.c` | 34 | metrics output |
| `yvex_profile.c` | 23 | profile output |
| `gguf/conversion.c` | 19 | conversion diagnostics |
| `yvex_cli.c` | 17 | top-level dispatch/help only |
| `yvexd.c` | 14 | daemon process output |

The immediate concentration is clear: the first four files contain 5112 print
sites. The next migration should reduce those owner-file walls before touching
lower-pressure modules.

## Print Kinds by Category

Most output is still hardcoded in command/report branches. That was acceptable
while YVEX needed proof text quickly, but it now creates an architectural
problem: command bodies own both semantic decisions and layout. The future split
should be report construction first, rendering second.

The rough categories are:

| Category | Current shape | Candidate migration |
| --- | --- | --- |
| Porcelain | Human command output, often flat key-value walls. | Renderer-owned compact reports, lists, and tables. |
| Diagnostic/evidence | `--audit`, report evidence, contract checks, trace text. | Structured audit/raw path, eventually JSON-backed where useful. |
| Plumbing/raw-like | Existing ad hoc `--json` and GGUF/tooling output. | One stable raw renderer; no layout promises. |
| Error/refusal | `stderr`, usage, unsupported mode, invalid input. | Error helper with stable classes and concise text. |
| Log/daemon | Daemon/status and runtime log-like output. | Log renderer or log helper, separate from command output. |

## Porcelain vs Plumbing Violations

The current violation is not that YVEX has many report fields. Evidence is
necessary. The violation is that many porcelain commands expose evidence as a
default human layout. A user running a normal command gets a flat diagnostic
wall, while the code grows compensating flags such as `--output table`,
`--audit`, `--include-*`, and internal contract-check flags.

Porcelain commands are human operator commands. Their default output should be
chosen by the command, not by a long-term user mode. If the natural shape is a
table, default output should be a table. If the natural shape is a compact
report, default output should be a compact report. If the natural shape is a
list, default output should be a list. The default must not contain giant tensor
entries, path walls unless paths are the point, repeated unsupported/runtime
status fields, or benchmark/release disclaimers copied into every row.

Plumbing output is stable machine-readable data or complete evidence. It must
be requested explicitly. JSON is the target raw/plumbing surface, but this audit
does not implement JSON output and does not claim that JSON is uniform today.
`--audit` remains a transitional evidence path until a structured raw surface
exists for the reports that need it.

## Existing Flag Surface

The production source contains 256 unique `--...` strings. Not all are output
flags; many are selectors, paths, or domain options. The output-related pressure
is concentrated here:

| Flag family | Count | Classification | Notes |
| --- | ---: | --- | --- |
| `--output` | 118 | transitional | Existing normal/table/audit grammar remains, but table should become renderer layout instead of a long-term UX crutch. |
| `--audit` | 112 | transitional | Keep for evidence while structured raw output is absent. |
| `--json` | 23 | replace-with-json target | Some ad hoc raw paths exist; do not claim one uniform JSON interface yet. |
| `--check-output-contract` | 4 | diagnostic/internal | Useful for native checks, but should not spread as an operator-facing pattern. |
| `--trace-level` | 5 | diagnostic | Trace belongs to diagnostic/log surfaces, not normal porcelain. |
| `--include-*` | 158 | transitional evidence toggles | These should not multiply to compensate for missing report/raw separation. |
| `--dry-run` | 15 | not output-related | Domain behavior flag. |
| `--models-root` | 57 | not output-related | Path selection. |
| `--out-dir` | 4 | not output-related | Path selection. |
| `--role` | 10 | not output-related | Domain selector. |
| `--gate` | 6 | not output-related | Domain selector. |

Important `--include-*` families include blockers, next, candidates, KV, graph,
residency, tensors, context, attention, and pressure targets. These are useful
evidence selectors today, but their presence is a symptom that normal output,
audit output, and raw output are not sharply separated yet.

## Output Doctrine

Default output is porcelain: short, human-readable, and selected by the command.
Normal output should carry one compact status, one top blocker where relevant,
one next step where relevant, and one short boundary where the command could be
misread as a runtime capability claim.

Table is a layout, not a long-term user-selected mode. Existing `--output table`
must stay during migration, but list-like commands should print aligned tables
by default when that is the clearest human layout.

Raw/plumbing output is explicit. The target raw surface is `--json`, with stable
field names and no human layout promises.
JSON output is not implemented uniformly in this audit.

Audit is transitional. It preserves evidence while report objects and raw
renderers are not complete. It should not become the permanent answer for every
operator wanting data.

Renderer ownership should move toward this shape:

| Layer | Responsibility |
| --- | --- |
| Domain owner | Build semantic result/report structs and precise failure classes. |
| Porcelain renderer | Choose compact human layout: summary, table, list, or short report. |
| Plumbing renderer | Serialize raw stable fields, with JSON as the target. |
| Diagnostic renderer | Preserve evidence during the transition from audit walls to structured raw output. |
| Error/log helper | Keep stderr/errors/logs separate from command output. |

`yvex_cli.c` remains top-level dispatch/help only. Domain files may call render
helpers, but command branches should stop owning full report walls.

## Renderer Foundation

`V010.CLI.25` adds the first private renderer ownership boundary. The
implementation is header-only in `yvex_render_private.h` so it does not create a
new root implementation unit or a public API. The primitives cover stream/mode
setup, report titles, key-value fields, numeric fields, compact status,
top-blocker, next-row, boundary, short sections, inline field groups, and table
row/header text.

The pilot migration is `models prepare --dry-run` for downloaded/source-backed
Qwen/Gemma targets. `yvex_model_artifacts.c` now builds a semantic prepare
source report, renders compact porcelain output by default, and keeps the full
diagnostic evidence under `--audit`.

This is only the foundation. It does not remove all print sites, complete
`yvex_model_artifacts.c`, migrate `yvex_model.c`, implement uniform JSON output,
remove existing flags, or add runtime, quantization, artifact, generation, eval,
benchmark, throughput, or release capability.

## Base CLI Grammar

`V010.CLI.26` adds the base command grammar and command catalog. The
implementation keeps `yvex_cli.c` as the top-level grammar/catalog/dispatch
owner only: it records command group, surface class, purpose, normal usage,
common example, option-class summary, and boundary metadata for each top-level
command. Domain files still own behavior, detailed help, semantic reports, and
failure classes.

The command shape is:

```text
yvex <family> <action> [object] [selectors] [behavior flags] [diagnostic flags]
```

The grammar is conceptual and help-facing. It does not force every command into
the same number of words. Examples:

```text
yvex models prepare TARGET
yvex models check TARGET
yvex model-target inspect TARGET
yvex model-target tensor-map TARGET
yvex graph check --suite primitives
yvex generate --model TARGET
```

Top-level command families are:

```text
core
operator
model
source
artifact
graph
runtime
diagnostic
server
research/future
```

Known top-level commands are classified as one of:

```text
porcelain
diagnostic
plumbing
mixed-transitional
future/planned
```

The current catalog does not claim a completed plumbing command surface just
because some commands have ad hoc JSON paths. Uniform JSON remains future work.

Normal output shape:

```text
<title>: <object> [<status>]
<identity line>
<summary line>
top_blocker: <blocker>
next: <row-or-action>
boundary: <short boundary>
```

List/table output shape:

```text
<TITLE>

COLUMN  COLUMN  COLUMN
value   value   value
```

Status/refusal shapes:

```text
<thing>: <status>
scope: <short scope>
next: <next action>
boundary: <short boundary>
```

```text
error: <class>
reason: <human reason>
hint: <short next action, if useful>
```

Audit remains verbose and stable:

```text
<stable key>: <value>
<stable key>: <value>
boundary: <full evidence boundary>
```

The base operator status vocabulary is:

```text
ok
ready
selected
present
missing
blocked
unsupported
planned
diagnostic
report-only
not-measured
failed
```

Top-level help now follows the grammar: thesis, usage, command shape, common
commands, command groups, option classes, raw/evidence note, and boundary.
`yvex commands` is the grouped command catalog with command, group, surface,
and purpose columns. `yvex help COMMAND` prints command grammar metadata before
the domain-owned help text.

Option classes are:

```text
selector
path
behavior
diagnostic
plumbing
transitional layout
```

This row does not migrate every command output, remove flags, implement JSON,
or change domain behavior.

## Full CLI Track Rebase

`SPINE.CLI.REBASE.1` reconciles the Operator CLI track after the executed
`V010.CLI.26` grammar row. The next CLI work is no longer a direct jump into
one owner file. The track is now layered:

```text
inventory/doctrine
-> renderer foundation
-> base command grammar
-> shared status/refusal/error grammar
-> diagnostic/log/error split
-> command-family porcelain migrations
-> raw/plumbing JSON where report objects exist
-> flag demotion
-> final CLI gate
```

The immediate missing substrate is status and refusal grammar. YVEX needs one
shared shape for `status`, `top_blocker`, `next`, `boundary`, refusals, and
errors before `models`, `model-target`, `graph`, and runtime command families
are migrated broadly. Otherwise each namespace will keep inventing its own CLI
language while still using the same renderer primitives.

The old local migrations are preserved as planned command-family work:

```text
models artifacts porcelain -> V010.CLI.MODELS.4
model-target porcelain -> V010.CLI.TARGET.*
graph/runtime diagnostic migration -> V010.CLI.GRAPH.0 and V010.CLI.RUNTIME.0
```

This rebase does not change command behavior, remove print sites, implement
JSON, remove existing flags, migrate command families, add runtime behavior, or
claim qtype, artifact, generation, eval, benchmark, throughput, or release
readiness.

## Migration Plan

Phase 0 is complete as `CLI.ARCH.AUDIT.0`: print inventory, flag inventory,
porcelain/plumbing doctrine, and renderer ownership doctrine.

Phase 1 is complete as `V010.CLI.25`: introduce a narrow internal report/render
boundary and one `models prepare --dry-run` pilot without broad behavior
changes, runtime claims, JSON claims, or a new command forest.

Phase 2 is complete as `V010.CLI.26`: define the base CLI grammar and grouped
command catalog in `yvex_cli.c` without moving domain behavior there.

Phase 3 is complete as `SPINE.CLI.REBASE.1`: rebuild the Operator CLI track as
shared substrate plus command-family migrations.

Phase 4 is planned as `V010.CLI.27`: define base status and refusal grammar
before broad namespace migrations.

Phase 5 is planned as `V010.CLI.28`: split error, log, diagnostic, and trace
surfaces from normal porcelain.

Phase 6 is command-family porcelain: migrate models, model-target, source,
graph, runtime, generate, chat, and related operator families after shared
status/refusal/error grammar exists.

Phase 7 maps to `V010.CLI.20`: add JSON/raw plumbing only after a report object
exists and stable raw fields can be tested for a command family.

Phase 8 is flag demotion: once porcelain and JSON/raw cover a command family,
demote `--output table`, reduce `--include-*`, and keep audit only where
promotion evidence still needs it.

Existing flags stay in place during migration. `yvex_cli.c` stays dispatch-only.

## Non-Goals

This audit does not remove all print sites, implement JSON output, remove
existing flags, add CLI command surfaces, create renderer source files, refactor
the whole CLI, add runtime behavior, add tensor mapping semantics, add qtype
role support, quantize tensors, emit artifacts, construct runtime descriptors,
attach backend residency, feed graph consumers, execute
prefill/decode/logits/tokenizer/sampling/generation, evaluate, benchmark, claim
throughput, or mark release readiness.

## Current Sequencing Boundary

CLI output cleanup is not the current product blocker. The priority-blocking
Active Next is:

```text
V010.REBASE.DEEPSEEK.0
```

The ordered foundation sequence lives in
`docs/repair/v010-foundation-closure.md`. `V010.CLI.27 - base status and refusal
grammar` remains a deferred CLI substrate row, not an active product row. CLI
migrations resume only when an owning repair or runtime row needs the
corresponding command contract. Output architecture work cannot promote
artifact or runtime capability.
