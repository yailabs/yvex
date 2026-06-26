# YVEX Operator Runbook

This runbook is the command-first path through the current YVEX operator
surface. It uses repository-local binaries, temporary state, controlled fixtures,
and explicit boundary checks. It does not describe inference or generation
because those paths are not implemented.

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

## 3. Create a temporary workspace

Use temporary output and a temporary registry while testing workflows. This keeps
operator state out of the repository.

```sh
tmpdir="$(mktemp -d)"
export YVEX_MODELS_REGISTRY="$tmpdir/models.local.json"
echo "$tmpdir"
```

Expected outcome: generated GGUFs, local registries, reports, and logs stay
under `$tmpdir` unless you deliberately choose another external location.

## 4. Emit and inspect a controlled GGUF

Start with the controlled GGUF path because it is repository-local and does not
require external model weights.

```sh
./yvex gguf-emit controlled --out "$tmpdir/controlled.gguf" --overwrite
./yvex inspect "$tmpdir/controlled.gguf"
./yvex metadata "$tmpdir/controlled.gguf"
./yvex tensors "$tmpdir/controlled.gguf"
```

Expected outcome: `inspect` reports GGUF v3 descriptor facts, `metadata` prints
parsed metadata, and `tensors` shows the controlled `token_embd.weight` tensor.

## 5. Materialize controlled weights on CPU and CUDA

Materialization copies tensor bytes into backend-owned storage and reports
residency. It does not execute a transformer graph.

```sh
./yvex materialize --model "$tmpdir/controlled.gguf" --backend cpu
```

CUDA-capable hosts can also run:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model "$tmpdir/controlled.gguf" --backend cuda
```

Expected outcome: `status: weights-materialized` and `execution_ready: false`.
This proves controlled emission, parsing, tensor table inspection, and selected
materialization, not full model execution.

Attach the same selected materialized weights to the engine lifecycle:

```sh
./yvex engine --model "$tmpdir/controlled.gguf" --backend cpu
./yvex session "$tmpdir/controlled.gguf" --backend cpu
```

CUDA, when available:

```sh
./yvex engine --model "$tmpdir/controlled.gguf" --backend cuda
./yvex session "$tmpdir/controlled.gguf" --backend cuda
```

Expected outcome: `weights_attached: true`, `weight_tensor_count: 1`,
`weights_backend: cpu` or `cuda`, and `graph_execution_ready: false`. The
engine owns the attached backend tensors. The session observes that state; it
does not own or execute the weights.

## 6. Register a local model artifact

The local model registry maps an alias to an external GGUF path. With the
temporary registry from section 3, registration is safe to repeat.

```sh
./yvex models add --path "$tmpdir/controlled.gguf" --alias controlled
./yvex models list
./yvex inspect controlled
./yvex tensors controlled
```

Expected outcome: `controlled` resolves through the registry. The alias is local
state; it does not change runtime capability.

## 7. Select the current model

Select one registered alias as the current model for console diagnostics.

```sh
./yvex models use controlled
./yvex models current
```

Expected outcome: `models current` prints `selected: controlled`. No command may
silently choose the first model in the registry.

## 8. Use the selected model in chat / REPL diagnostics

The diagnostic console uses the current selected registry model when `--model`
is omitted. It accepts text and slash commands, but it does not generate model
responses.

```sh
printf '/status\n/quit\n' | ./yvex chat --backend cpu
printf '/quit\n' | ./yvex chat --model controlled --backend cpu
```

Expected outcome: the console opens against the selected or explicit model and
exits on `/quit`. Plain user text produces the diagnostic unsupported-generation
placeholder; it is not inference.

## 9. Run yvexd with a model path or alias

`yvexd` is a provider/status shell. It accepts a direct path or registered alias
for `--model`, then serves status endpoints. In one-request mode it exits after
serving one HTTP request.

Direct path:

```sh
./yvexd --model "$tmpdir/controlled.gguf" --backend cpu \
  --host 127.0.0.1 --port 18080 --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18080/v1/models
wait "$server_pid" || true
```

Alias:

```sh
./yvexd --model controlled --backend cpu \
  --host 127.0.0.1 --port 18081 --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18081/v1/models
wait "$server_pid" || true
```

Status endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

Expected outcome: `/v1/models` reports provider-shell model status with
`generation_available: false`. The daemon is not a generation server.

## 10. Inspect the active DeepSeek selected artifact

The active DeepSeek selected embedding artifact is external operator state. If
it is registered locally, inspect it by alias.

```sh
./yvex models list
./yvex models current
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cuda
```

Expected facts:

```text
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
weights_attached: true
execution_ready: false
graph_execution_ready: false
```

If the artifact is absent on this host, register the operator-local copy:

```sh
./yvex models add --path /path/to/deepseek-selected-embed.gguf --alias deepseek4-v4-flash-selected-embed
./yvex models use deepseek4-v4-flash-selected-embed
```

Do not commit the artifact or the local registry.

## 11. Run model and materialization gates

Use gates for repeatable selected-artifact checks. These commands are long
because they encode expected file identity and tensor facts.

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
  --report-out "$tmpdir/model-gate.txt"
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
  --report-out "$tmpdir/materialize-gate.txt"
```

Expected outcome: gate status is pass when the external artifact and CUDA host
are available. The result remains selected-tensor materialization only.

## 12. Open-weight intake commands

Use these commands for source provenance, native inventory, mapping, planning,
and selected emission. They do not perform full-model conversion.

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/native/source \
  --status in-progress \
  --out "$tmpdir/source-manifest.json"

./yvex native-weights --source /path/to/native/source --limit 20
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source --limit 20
./yvex convert plan --arch deepseek4 --native-source /path/to/native/source --out-plan "$tmpdir/yvex-plan.json"
./yvex convert emit --arch deepseek4 --native-source /path/to/native/source --tensor model.embed_tokens.weight --target-qtype F16 --out "$tmpdir/yvex-selection.gguf" --overwrite
```

Expected outcome: source facts, tensor mapping, a conversion plan, or a selected
GGUF. Full native quantization and full-model GGUF conversion are not current
runtime capabilities.

## 13. Quantization / imatrix / provenance commands

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

## 14. Validate the repository

Run the standard validation gate before committing changes.

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

CUDA-capable hosts:

```sh
make check-cuda
```

Expected outcome: baseline tests, CLI smoke, docs surface, repository surface,
and CUDA validation pass. If CUDA is unavailable, report that explicitly.

## 15. Artifact and path hygiene

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

## 16. Debugging checklist

Use this order before assuming a runtime bug:

```text
run ./yvex help <command>
check YVEX_MODELS_REGISTRY
run ./yvex models current
run ./yvex inspect <model>
run ./yvex tensors <model>
test CPU before CUDA
use CUDA only on CUDA-capable hosts
never commit generated GGUFs or local registry files
```

For daemon checks, start with `--one-request` and a status endpoint before
testing longer-lived processes.

## 17. Benchmarking and evaluation status

Benchmarking and capability evaluation are not implemented yet. Throughput,
token latency, official-vector evaluation, logits regression, and generation
quality suites belong after the relevant graph/logits/generation runtime exists.

Do not create benchmark claims from materialization, CUDA probing, daemon
status, or diagnostic console behavior.

## 18. What is not implemented yet

The current runtime does not implement full model execution, prefill, decode,
sampling, generation, provider-compatible generation, full DeepSeek support,
full GGUF conversion, KV execution, SSD streaming, distributed inference, or
benchmark performance.

`execution_ready` remains false until scheduled graph operations, required graph
weights, scratch, KV, logits, decode, and sampling form an implemented path.
