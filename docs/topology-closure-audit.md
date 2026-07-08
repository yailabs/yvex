# Topology Closure Audit

Date: 2026-07-08

Base commit: `d41439c refactor: remove model target runner`

## Scope

This audit covers the repository after the source, generation, KV, sampling,
graph, and model-target cell extraction rows. It checks ownership drift between
CLI input, command adapters, domain/report builders, renderers, explicit file
writers, runtime/core modules, and `libyvex.a`.

This is an audit-only artifact. It does not change command behavior, implement
runtime capability, implement quantization, emit artifacts, or close global
topology.

## Summary

| Area | Result | Count | Notes |
| --- | ---: | ---: | --- |
| Direct output matches | blocked | 4397 | 47 allowed writer matches, 4350 outside final topology writer owners |
| CLI-shaped input matches | blocked | 678 | 147 outside CLI input/command/router/catalog owners |
| CLI/operator include matches | blocked | 38 | 12 outside CLI/core owners |
| Renderer leakage grep matches | advisory | 34 | source renderer typed-field/string false positives; no file IO found |
| Command adapters over 350 lines | pass | 0 | largest adapter is source at 221 lines |
| Report text-buffer matches | blocked | 9 | no model-target debt; residual account capture/log sink wording |
| Compatibility shell matches | blocked | 29 | graph dump compatibility and render boundary helpers remain |
| Large C files over 1000 lines | blocked | 18 | 5 over 2500; 2 over 5000 |
| `libyvex.a` CLI leakage | pass | 0 | CLI input/command/render/io are not in `CORE_SRCS` |
| Model-target hard guard | pass | 0 | runner/text-buffer/argc/argv debt absent |
| Spine alignment | pass | 10 | required topology rows present; closure remains planned |

Conclusion: `closure-blocked-by-major-residue`

## Direct Output Inventory

Command:

```sh
git grep -nE '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|fputc|putchar|perror|fwrite)\s*\(' -- src include || true
```

Classification:

| Class | Count | Status |
| --- | ---: | --- |
| allowed CLI IO | 3 | allowed |
| allowed explicit file writers | 14 | allowed |
| allowed model-target sidecar writer | 30 | allowed explicit local file writer |
| outside final writer owners | 4350 | legacy pending / violation for closure |

Largest outside-writer files:

| Path | Matches | Classification |
| --- | ---: | --- |
| `src/model/yvex_model_artifacts.c` | 2558 | major legacy pending |
| `src/runtime/yvex_runtime.c` | 432 | major legacy pending |
| `src/artifact/yvex_artifact.c` | 365 | major legacy pending |
| `src/gguf/tools.c` | 286 | major legacy pending |
| `src/accounts/yvex_accounts.c` | 100 | legacy pending |
| `src/gguf/quant.c` | 85 | legacy pending |
| `src/core/yvex_fs.c` | 75 | legacy pending |
| `src/runtime/yvex_chat.c` | 70 | legacy pending |
| `src/generation/yvex_decode.c` | 69 | legacy pending |
| `src/generation/yvex_logits.c` | 64 | legacy pending |
| `src/backend/yvex_backend.c` | 45 | legacy pending |
| `src/tokenizer/yvex_tokenizer.c` | 42 | legacy pending |
| `src/metrics/yvex_metrics.c` | 41 | legacy pending |
| `src/source/yvex_source_write.c` | 26 | explicit source writer candidate; needs owner review |
| `src/graph/yvex_graph_report.c` | 1 | graph dump compatibility residue |

## CLI-Shaped Input Inventory

Command:

```sh
git grep -nE '\b(argc|argv)\b|usage:|--help|--output|--audit' -- src include || true
```

Classification:

| Class | Count | Status |
| --- | ---: | --- |
| total matches | 678 | audit surface |
| outside CLI input/commands/router/catalog | 147 | legacy pending / needs cleanup |

Top non-CLI owners:

| Path | Matches | Classification |
| --- | ---: | --- |
| `src/model/yvex_model_artifacts.c` | 34 | major legacy pending |
| `src/gguf/tools.c` | 21 | legacy pending |
| `src/runtime/yvex_runtime.c` | 19 | legacy pending |
| `src/artifact/yvex_artifact.c` | 19 | legacy pending |
| `src/tokenizer/yvex_tokenizer.c` | 9 | legacy pending |
| `src/backend/yvex_backend.c` | 6 | legacy pending |
| `src/generation/yvex_logits.c` | 3 | legacy pending |
| `src/generation/yvex_decode.c` | 3 | legacy pending |

Some model-target report modules contain output-contract field values such as
`--output` in row text or test-facing strings; those are not CLI parsers, but
they should be reviewed when CLOSURE.2 turns advisory findings into hard guards.

## CLI/Operator Include Inventory

Command:

```sh
git grep -nE '#include "yvex_(cli|operator|console)|#include <.*cli.*>' -- src include || true
```

Non-CLI/core hits:

| Path | Count | Classification |
| --- | ---: | --- |
| `src/model/yvex_model_artifacts.c` | 2 | major legacy pending |
| `src/accounts/yvex_accounts.c` | 1 | legacy pending |
| `src/artifact/yvex_artifact.c` | 1 | legacy pending |
| `src/backend/yvex_backend.c` | 1 | legacy pending |
| `src/generation/yvex_decode.c` | 1 | legacy pending |
| `src/generation/yvex_logits.c` | 1 | legacy pending |
| `src/generation/yvex_prefill.c` | 1 | legacy pending |
| `src/gguf/tools.c` | 1 | legacy pending |
| `src/runtime/yvex_chat.c` | 1 | legacy pending |
| `src/runtime/yvex_runtime.c` | 1 | legacy pending |
| `src/tokenizer/yvex_tokenizer.c` | 1 | legacy pending |

## Renderer Leakage Inventory

Command:

```sh
git grep -nE 'yvex_backend|yvex_graph_build|yvex_model_ref|native_weight|safetensors|fopen|open\(|read\(|write\(' -- src/cli/render || true
```

Result: 34 matches, all in `src/cli/render/yvex_source_render.c`.

Classification: advisory false positives. The matches are typed source report
fields and literal output labels containing `safetensors`; no renderer file IO,
backend open, graph construction, or report building was found by this scan.

## Command Adapter Inventory

Threshold: 350 lines.

| Adapter | Lines | Status |
| --- | ---: | --- |
| `src/cli/commands/yvex_source_cli.c` | 221 | pass |
| `src/cli/commands/yvex_kv_cli.c` | 156 | pass |
| `src/cli/commands/yvex_graph_cli.c` | 129 | pass |
| `src/cli/commands/yvex_sampling_cli.c` | 115 | pass |
| `src/cli/commands/yvex_model_target_cli.c` | 112 | pass |
| `src/cli/commands/yvex_generate_cli.c` | 87 | pass |

All remaining command adapters are 18-line forwarding adapters.

## Report Text-Buffer Inventory

Command:

```sh
git grep -nE 'primary_text|diagnostic_text|raw_output|report_text|captured_text|line_buffer|capture|sink' -- src include || true
```

Findings:

| Path | Classification |
| --- | --- |
| `src/accounts/yvex_accounts.c` | provider subprocess capture; legacy pending, not report blob |
| `include/yvex/log.h` | log sink wording; allowed text description |
| `include/yvex/status.h` | log sink wording; allowed text description |
| `src/generation/yvex_kv_report.c` | comment wording only |

No model-target `primary_text`, `diagnostic_text`, `raw_output`,
`report_text`, `captured_text`, or `line_buffer` debt remains.

## Compatibility Shell Inventory

Command:

```sh
git grep -nE 'compat|legacy|shim|bridge|runner|not-bound|renderer-only|_render_boundary|boundary anchor' -- src include || true
```

Important findings:

| Path | Classification |
| --- | --- |
| `src/graph/yvex_graph_report.c` | graph dump compatibility; pending cleanup |
| `src/cli/yvex_render_private.h` | render boundary helper; pending CLI renderer cleanup |
| `src/core/yvex_operator_render_private.h` | legacy operator render helper; pending cleanup |
| `src/accounts/yvex_accounts.c` | legacy provider binary discovery; likely real behavior |
| `src/runtime/yvex_runtime.c` | direct conversion bridge output; pending cleanup |
| `include/yvex/conversion.h`, `include/yvex/weights.h` | public bridge terminology; not automatically invalid |
| `src/model/target/*` | semantic compatibility/status field values only; no runner/backend shell |

## Monolith Inventory

Thresholds:

- warning: over 1000 lines
- strong warning: over 2500 lines
- critical: over 5000 lines

| Path | Lines | Severity | Mixes layers |
| --- | ---: | --- | --- |
| `src/model/yvex_model_artifacts.c` | 17223 | critical | yes; next row should split model artifacts/control-plane output |
| `src/runtime/yvex_runtime.c` | 4698 | strong | yes; runtime CLI/report/output residue remains |
| `src/graph/yvex_graph_primitive.c` | 4538 | strong | likely primitive proof density; needs focused review |
| `src/gguf/quant.c` | 2513 | strong | likely policy/tooling mix; needs focused review |
| `src/gguf/tools.c` | 2184 | warning | command surface/tooling mix remains |
| `src/backend/yvex_backend.c` | 2103 | warning | backend report/output residue remains |
| `src/artifact/yvex_artifact.c` | 1995 | warning | artifact command/report/output residue remains |
| `src/gguf/conversion.c` | 1678 | warning | conversion bridge/map facts need review |
| `src/source/yvex_source_report.c` | 1466 | warning | source report is large but cell-owned |
| `src/backend/cuda/cuda_ops.c` | 1352 | warning | CUDA primitive implementation density likely justified |
| `src/model/yvex_model.c` | 1297 | warning | model registry/materialization facts need review |
| `src/accounts/yvex_accounts.c` | 1247 | warning | provider execution/capture mix remains |
| `src/core/yvex_fs.c` | 1217 | warning | filesystem utility/output residue needs review |
| `src/generation/yvex_generation.c` | 1169 | warning | closed cell but still large enough to monitor |
| `src/tokenizer/yvex_tokenizer.c` | 1158 | warning | tokenizer command/report residue remains |
| `src/artifact/yvex_artifact_integrity.c` | 1037 | warning | integrity report/output residue likely |
| `src/gguf/gguf.c` | 1007 | warning | parser density likely acceptable pending review |

Recommended next row should start with the critical `src/model/yvex_model_artifacts.c`
residue, not quantization implementation.

## libyvex Leakage Inventory

Command:

```sh
grep -nE 'src/cli/(input|commands|render|io)' Makefile || true
```

Result: Makefile references CLI sources only in CLI source variables and include
paths. `CORE_SRCS` does not include `src/cli/input`, `src/cli/commands`,
`src/cli/render`, or `src/cli/io`.

`libyvex.a` CLI leakage: 0.

## Spine Alignment

Required rows present:

- `TOPOLOGY.CELL.SOURCE.0`
- `TOPOLOGY.CELL.GENERATION.0`
- `TOPOLOGY.CELL.KV.0`
- `TOPOLOGY.CELL.SAMPLING.0`
- `TOPOLOGY.CELL.GRAPH.0`
- `TOPOLOGY.CELL.MODEL_TARGET.0`
- `TOPOLOGY.CELL.MODEL_TARGET.1`
- `TOPOLOGY.CELL.MODEL_TARGET.2`
- `TOPOLOGY.CELL.MODEL_TARGET.3`
- `TOPOLOGY.CELL.CLOSURE.1`

`TOPOLOGY.CELL.CLOSURE.0` remains planned/open. `V010.QUANT.2` remains open.
This audit does not change Active Next.

## Recommended Cleanup Rows

| Row | Purpose |
| --- | --- |
| `TOPOLOGY.CELL.CLOSURE.2` | remove or hard-classify the residual direct-output and CLI-shaped domain debt found here |
| `TOPOLOGY.CELL.MODEL_ARTIFACTS.0` | split `src/model/yvex_model_artifacts.c` into domain/report/input/command/render/write ownership |
| `TOPOLOGY.CELL.RUNTIME.0` | split runtime report/command/output residue from runtime coordination |
| `TOPOLOGY.CELL.ARTIFACT.0` | extract artifact inspect/integrity command and rendering surfaces |
| `TOPOLOGY.CELL.GGUF_TOOLS.0` | split GGUF command/tool output from GGUF parsing/conversion/quant internals |
| `TOPOLOGY.CELL.BACKEND.0` | split backend reports/rendering from backend abstraction |
| `TOPOLOGY.CELL.TOKENIZER.0` | split tokenizer command/report/render ownership |
| `TOPOLOGY.CELL.GRAPH.COMPAT.0` | remove FILE-based graph dump compatibility or move it to an explicit writer |

## Explicit Conclusion

`closure-blocked-by-major-residue`

The completed cell extractions removed the model-target runner debt and the
major source/generation/KV/sampling/graph/model-target mixed-cell surfaces, but
global topology closure is still blocked. The highest-risk residue is direct
operator output and CLI-shaped command/report ownership in `src/model`,
`src/runtime`, `src/artifact`, `src/gguf`, `src/backend`, `src/tokenizer`, and
provider/account modules. The next cleanup pass should use this audit as the
cleanup map before returning to `V010.QUANT.2`.
