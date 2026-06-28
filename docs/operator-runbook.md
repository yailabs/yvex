# YVEX Operator Runbook

This runbook is the command-first path through the current YVEX operator
surface. It uses repository-local binaries, operator-local model storage,
selected GGUF emission, registry aliases, backend materialization, engine
attachment, daemon status, and gates. It does not describe inference or
generation because those paths are not implemented.

The default examples use an operator-owned model root outside this repository:

```text
/path/to/models/hf/deepseek/DeepSeek-V4-Flash
/path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

That layout is a placeholder, not a project rule. If your weights or GGUFs live
elsewhere, use those operator-local paths in `--native-source`, `--out`,
`--path`, and `--report-out`. Keep real model artifacts outside this repository.

## 1. Build the repository

YVEX currently runs from repository-local binaries: `./yvex` and `./yvexd`.
Build them before running operator commands.

```sh
make
./yvex version
./yvex commands
./yvex info
```

Expected outcome: the binaries exist in the repository root and `./yvex info`
reports implemented artifact, GGUF, backend, registry, console, and daemon
status surfaces.

## 2. Discover the command surface

Use command help as the source of truth for flags and current behavior.

```sh
./yvex help inspect
./yvex help materialize
./yvex help models
./yvex help chat
./yvex help model-gate
./yvex help materialize-gate
./yvexd --help
```

Expected outcome: help text stays bounded. Commands may inspect, materialize,
open diagnostics, and serve status endpoints; they must not claim generation.

## 3. Build or refresh the DeepSeek selected GGUF

This is the real selected-artifact path. It reads local DeepSeek native
safetensors and overwrites the selected embedding GGUF in the operator-owned
GGUF directory.

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/models/hf/deepseek/DeepSeek-V4-Flash \
  --status in-progress \
  --out /path/to/models/gguf/deepseek/deepseek-source-manifest.json

./yvex native-weights \
  --source /path/to/models/hf/deepseek/DeepSeek-V4-Flash \
  --limit 20

./yvex tensor-map \
  --arch deepseek4 \
  --native-source /path/to/models/hf/deepseek/DeepSeek-V4-Flash \
  --limit 20

./yvex convert plan \
  --arch deepseek4 \
  --native-source /path/to/models/hf/deepseek/DeepSeek-V4-Flash \
  --out-plan /path/to/models/gguf/deepseek/deepseek-selected-plan.json

./yvex convert emit \
  --arch deepseek4 \
  --native-source /path/to/models/hf/deepseek/DeepSeek-V4-Flash \
  --tensor embed.weight \
  --target-qtype F16 \
  --out /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
  --overwrite

./yvex inspect /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
./yvex tensors /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Expected outcome: `inspect` reports `architecture: deepseek`, and `tensors`
shows `token_embd.weight` with shape `[4096,129280]`, dtype `F16`, and
`1059061760` tensor bytes.

If your native source or GGUF directory is somewhere else, change only the path
arguments. Do not commit the real model source or generated selected GGUF.

## 4. Register and inspect the DeepSeek selected GGUF

Register the selected GGUF using the canonical alias.

```sh
./yvex models add \
  --path /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
  --alias deepseek4-v4-flash-selected-embed \
  --support-level selected-tensor-materialized

./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
```

Expected outcome: `models current` prints the DeepSeek alias, `inspect` reports
`architecture: deepseek`, and `tensors` shows the selected F16 embedding tensor.

Alias policy is strict. Use canonical aliases such as
`deepseek4-v4-flash-selected-embed`; simple labels such as `controlled` are
rejected.

## 5. Check artifact integrity

Run the baseline integrity check before materialization or graph execution when
checking operator-local artifacts.

```sh
./yvex integrity check --model deepseek4-v4-flash-selected-embed
```

Expected outcome:

```text
artifact_integrity: check
format: gguf
version: 3
architecture: deepseek
tensor_count: 1
known_tensor_bytes: 1059061760
tensor_shapes_checked: 1
tensor_dtypes_checked: 1
tensor_byte_counts_checked: 1
integrity_status: pass
integrity_errors: 0
status: artifact-integrity-pass
```

The check catches structural corruption, tensor range problems, checked
byte-count failures, duplicate tensor names, and selected embedding readiness
when requested. It is not a supply-chain security audit and does not prove model
quality, author identity, malware absence, or provenance.

The integrity output includes tensor range validation. A pass means each
declared tensor payload range is inside the local file, offset/byte arithmetic
did not overflow, and selected embedding readiness checks can validate the
requested token slice when `--require-token-embedding` is used. It does not prove
the tensor values are correct.

For selected embedding readiness, integrity output reports the interpreted
hidden size, vocabulary size, output count, output byte count, and selected-token
slice byte size. These are shape and dtype accounting facts for
`token_embd.weight`, not inference readiness.

The repository also runs tiny corrupt `GGUF` fixture tests against integrity,
inspect, tensors, materialization, and graph-entry refusal paths. Those fixtures
are repository validation assets, not operator model assets. Operators should
run `integrity check` and `models verify` on their own artifacts; they only need
the corrupt fixture suite when debugging YVEX itself.

For selected embedding readiness, include the partial-token boundary:

```sh
./yvex integrity check \
  --model deepseek4-v4-flash-selected-embed \
  --require-token-embedding \
  --partial-token 0
```

## 6. Verify artifact identity and registry metadata

Registered aliases record local file identity. Verify the alias before
materialization or graph execution when checking that the operator-local file is
still the same file YVEX registered.

```sh
./yvex models verify deepseek4-v4-flash-selected-embed
```

Expected outcome:

```text
models: verify
alias: deepseek4-v4-flash-selected-embed
registered_sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
current_sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
digest_status: pass
identity_status: pass
metadata_status: pass
readiness_status: pass
registered_primary_tensor: token_embd.weight
current_primary_tensor: token_embd.weight
registered_primary_dtype: F16
current_primary_dtype: F16
registered_primary_dims: [4096,129280]
current_primary_dims: [4096,129280]
status: models-identity-pass
```

`models verify` checks both digest identity and registry metadata drift. A digest
match proves only that the current bytes match the local registry record. A
metadata pass proves that the registry's recorded summary still matches current
parsed artifact facts: support level, architecture, tensor count, known tensor
bytes, primary tensor name/role/dtype/rank/dims/bytes, and selected embedding
readiness. This is not remote provenance or model-quality validation.

You can also make an expected digest explicit:

```sh
./yvex integrity check \
  --model deepseek4-v4-flash-selected-embed \
  --expect-sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
```

If an alias points to bytes that changed after registration, safety-critical
paths such as `materialize`, `engine --backend`, `session`, `model-gate`,
`materialize-gate`, and `graph --execute-partial` fail before backend allocation
or graph execution. Digest checks are local identity evidence only. They do not
prove remote provenance, author identity, malware absence, model quality, or
supply-chain security.

## 7. Generate an operator integrity report

Use the integrity report as the normal operator summary before materialization
or real partial graph execution.

```sh
./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
```

Expected outcome:

```text
artifact_integrity_report: summary
identity_status: pass
metadata_status: pass
integrity_status: pass
selected_embedding_ready: true
materialization_preflight: pass
graph_partial_guard: pass
execution_ready: false
generation: unsupported
status: integrity-report-pass
```

The report composes existing checks: artifact structure, local digest identity,
registry metadata drift, shape/dtype accounting, tensor range validation,
selected embedding readiness, materialization preflight, and graph-entry guard
status for implemented graph paths. It does not mean the model can generate
text, and it is not a supply-chain security audit.

If no `--backend` is supplied, backend-dependent sections report
`not-checked`.

## 8. Materialize and attach DeepSeek selected weights

Materialization copies the selected tensor bytes into backend-owned storage.
Engine attachment then makes that backend-resident tensor engine-owned runtime
state. Neither step executes a transformer graph.

Materialization reports an integrity gate. A `preflight` failure means YVEX
refused the artifact before backend allocation. An `allocation` or `transfer`
failure must report whether cleanup was attempted and whether cleanup passed.
This is materialization safety and backend lifecycle accounting, not inference
readiness.

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
```

CUDA-capable hosts can also run:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
```

Expected outcome:

```text
materialization status: materialized
materialization_gate: pass
materialization_phase: complete
integrity_status: pass
shape_status: pass
range_status: pass
allocation_attempted: true
transfer_attempted: true
cleanup_status: not-needed
weights_attached: true
weights_backend: cpu or cuda
weight_tensor_count: 1
weight_total_bytes: 1059061760
execution_ready: false
graph_execution_ready: false
```

The engine owns the attached backend tensors. The session observes that state;
it does not own or execute the weights.

## 9. Execute a real selected embedding segment

The real partial graph path uses the selected `F16` DeepSeek embedding tensor. It
executes a constrained token-embedding graph segment over engine-attached
`token_embd.weight`, converts the selected row to `F32`, compares the backend
output to a raw-artifact reference slice, and reports a checksum plus sample
values.

Graph commands report `graph_integrity_guard` and `graph_execution_phase`. A
`preflight` failure means no backend dispatch happened. This is graph-entry
safety, not full model execution or inference readiness.

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --execute-partial \
  --partial-token 0
```

Expected outcome:

```text
graph_integrity_guard: pass
graph_execution_phase: complete
graph_kind: selected-embedding-partial
dispatch_attempted: true
reference_read_attempted: true
output_allocation_attempted: true
real_partial_graph_executed: true
partial_graph_kind: token-embedding
partial_backend: cpu
partial_weight: token_embd.weight
partial_weight_dtype: F16
partial_output_dtype: F32
partial_output_count: 4096
partial_output_bytes: 16384
partial_max_abs_diff: 0
execution_ready: false
graph_execution_ready: false
status: real-partial-graph-executed
```

CUDA-capable hosts can run the same boundary on CUDA:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cuda \
  --execute-partial \
  --partial-token 0
```

Expected outcome: CUDA reports the same output checksum, reference checksum, and
sample values as CPU for the selected token. This is the first real-model partial
graph segment. It is not prefill, KV runtime, decode, logits, sampling,
generation, or a CUDA transformer backend.

## 10. Execute a deterministic fixture graph

M4 fixture execution uses a tiny controlled GGUF so output can be checked
exactly. The example keeps the file under the operator-owned DeepSeek GGUF
directory and marks the fixture architecture as `deepseek`, but it is still a
controlled F32 fixture, not the large selected F16 DeepSeek artifact.

The fixture graph uses the same graph integrity guard. A `preflight` failure
still means no backend dispatch happened.

```sh
./yvex gguf-emit controlled \
  --out /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --model-name yvex-m4-deepseek-fixture \
  --arch deepseek \
  --overwrite

./yvex graph \
  --model /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --backend cpu \
  --execute-fixture \
  --fixture-token 0

./yvex graph \
  --model /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --backend cpu \
  --execute-fixture \
  --fixture-token 1
```

Expected outcome:

```text
graph_integrity_guard: pass
graph_execution_phase: complete
graph_kind: fixture-embedding
dispatch_attempted: true
reference_read_attempted: false
output_allocation_attempted: true
fixture_graph_executed: true
fixture_backend: cpu
fixture_op: embed
fixture_weight: token_embd.weight
fixture_output_values: 0,4,8,12
execution_ready: false
graph_execution_ready: false
status: fixture-graph-executed
```

Token `1` should produce `fixture_output_values: 16,20,24,28`. That proves the
fixture graph reads attached tensor bytes, dispatches the embed node through the
backend, and writes a real output buffer. It does not execute a real DeepSeek
model graph, produce logits, or generate text.

CUDA-capable hosts can run the same fixture graph on CUDA:

```sh
./yvex graph \
  --model /path/to/models/gguf/deepseek/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf \
  --backend cuda \
  --execute-fixture \
  --fixture-token 0
```

Expected outcome: the CUDA output values match the CPU fixture output. This is
fixture graph parity only; it is not a CUDA transformer backend.

## 11. Run yvexd with the DeepSeek alias

`yvexd` is a provider/status shell. It accepts a direct path or registered alias
for `--model`, then serves status endpoints. In one-request mode it exits after
serving one HTTP request.

Direct path:

```sh
./yvexd \
  --model /path/to/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
  --backend cpu \
  --host 127.0.0.1 \
  --port 18080 \
  --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18080/v1/models
wait "$server_pid" || true
```

Alias:

```sh
./yvexd \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --host 127.0.0.1 \
  --port 18081 \
  --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18081/v1/models
wait "$server_pid" || true
```

Expected outcome: `/v1/models` reports provider-shell model status with
`generation_available: false`. The daemon is not a generation server.

Status endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

## 12. Run model and materialization gates

Use gates for repeatable selected-artifact checks. These commands encode
expected file identity and tensor facts.

```sh
./yvex model-gate check \
  --model deepseek4-v4-flash-selected-embed \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4096,129280 \
  --expect-dtype F16 \
  --expect-bytes 1059061760 \
  --backend cpu \
  --backend cuda \
  --require-cpu \
  --require-cuda \
  --report-out /path/to/models/gguf/deepseek/deepseek-model-gate.txt
```

```sh
./yvex materialize-gate check \
  --model deepseek4-v4-flash-selected-embed \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
  --scope selected-tensor \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4096,129280 \
  --expect-dtype F16 \
  --expect-bytes 1059061760 \
  --backend cpu \
  --backend cuda \
  --require-cpu \
  --require-cuda \
  --repeat 3 \
  --check-cleanup \
  --report-out /path/to/models/gguf/deepseek/deepseek-materialize-gate.txt
```

Expected outcome: gate status is pass when the selected artifact and CUDA host
are available. The result remains selected-tensor materialization only.

## 13. Use chat / REPL diagnostics only with tokenizer-bearing artifacts

The selected DeepSeek embedding artifact does not include the tokenizer metadata
needed by `chat`. Use `chat` with a tokenizer-bearing fixture or future
tokenizer-bearing model artifact. The diagnostic console accepts text and slash
commands, but it does not generate model responses.

```sh
printf '/status\n/quit\n' | ./yvex chat --model tests/fixtures/gguf/valid-tokenizer-simple.gguf --backend cpu
```

Expected outcome: the console opens against the explicit tokenizer fixture and
exits on `/quit`. Plain user text produces the diagnostic unsupported-generation
placeholder; it is not inference. Do not use the selected DeepSeek embedding
artifact as a chat model.

## 14. Quantization / imatrix / provenance commands

Keep qtype storage, policy, provenance, and compute boundaries separate.

```sh
./yvex qtype-support
```

Manifest-dependent commands:

```sh
./yvex quant-policy validate --policy /path/to/policy.json
./yvex quant-job inspect --manifest /path/to/quant-job.json
./yvex imatrix inspect --manifest /path/to/imatrix.json
```

Expected outcome: these surfaces report policy or provenance state. They do not
run native quantization, calibration, or model execution.

## 15. Validate the repository

Run the standard validation gate before committing changes.

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

`make check` runs the consolidated integrity regression harness. It exercises
structural corruption, digest drift, metadata drift, materialization preflight,
graph guard refusal, cleanup, and repeat behavior across repository fixtures.
Operators do not need to run that harness on their model artifacts; they should
use `integrity check`, `models verify`, `materialize`, and graph command output.

CUDA-capable hosts:

```sh
make check-cuda
```

Expected outcome: baseline tests, CLI smoke, docs surface, repository surface,
and CUDA validation pass. If CUDA is unavailable, report that explicitly.

## 16. Artifact and path hygiene

Check that generated artifacts and local model state are not tracked.

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected outcome: no tracked large model artifacts. Tracked GGUF files are tiny
parser fixtures only.

Run a public-doc path leak scan before publishing docs:

```sh
grep -R -nE '(/home|/Users|/mnt)/[^[:space:]]+' README.md MODEL_ARTIFACTS.md AGENTS.md docs/contract.md docs/spine.md docs/operator-runbook.md || true
```

Expected outcome: no personal or machine-specific absolute paths in public
documentation.

## 17. Debugging checklist

Use this order before assuming a runtime bug:

```text
run ./yvex help <command>
check the path passed to --native-source
check the path passed to --out
check YVEX_MODELS_REGISTRY only if you intentionally use an isolated registry
run ./yvex models current
run ./yvex inspect <model>
run ./yvex tensors <model>
test CPU before CUDA
use CUDA only on CUDA-capable hosts
never commit generated GGUFs or local registry files
```

For daemon checks, start with `--one-request` and a status endpoint before
testing longer-lived processes.

## 18. Benchmarking and evaluation status

Benchmarking and capability evaluation are not implemented yet. Throughput,
token latency, official-vector evaluation, logits regression, and generation
quality suites belong after the relevant graph/logits/generation runtime exists.

Do not create benchmark claims from materialization, CUDA probing, daemon
status, or diagnostic console behavior.

## 19. What is not implemented yet

The current runtime does not implement full model execution, prefill, decode,
sampling, generation, provider-compatible generation, full DeepSeek support,
full GGUF conversion, KV execution, SSD streaming, distributed inference, or
benchmark performance.

`execution_ready` remains false until scheduled graph operations, required graph
weights, scratch, KV, logits, decode, and sampling form an implemented path.
