# YVEX Operator Runbook

This runbook is the command-first path through the current YVEX operator
surface. It uses repository-local binaries, temporary state, optional controlled
fixtures, operator-local DeepSeek selected artifacts, and explicit boundary
checks. It does not describe inference or generation because those paths are not
implemented.

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

## 3. Create a temporary workspace and choose registry state

Use temporary output for reports and disposable fixtures. Do not export a
temporary registry if you want to use an alias that is already registered in
your normal `.yvex/models.local.json`; exporting `YVEX_MODELS_REGISTRY` points
all model-ref commands at that isolated registry.

```sh
tmpdir="$(mktemp -d)"
echo "$tmpdir"
```

Optional isolated registry for repeatable tests:

```sh
export YVEX_MODELS_REGISTRY="$tmpdir/models.local.json"
```

Expected outcome: generated GGUFs, reports, and logs stay under `$tmpdir`
unless you deliberately choose another external location. If you use the
isolated registry, register the DeepSeek alias into that registry before running
alias-based commands.

## 4. Register and inspect the DeepSeek selected GGUF

The main operator path uses the external DeepSeek selected embedding artifact,
not the controlled `llama` fixture. Set the local path for this machine, then
register the canonical alias.

```sh
DEEPSEEK_ALIAS=deepseek4-v4-flash-selected-embed
DEEPSEEK_GGUF=/path/to/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf

./yvex models add \
  --path "$DEEPSEEK_GGUF" \
  --alias "$DEEPSEEK_ALIAS" \
  --support-level selected-tensor-materialized

./yvex models use "$DEEPSEEK_ALIAS"
./yvex models current
./yvex inspect "$DEEPSEEK_ALIAS"
./yvex tensors "$DEEPSEEK_ALIAS"
```

Expected outcome: `models current` prints the DeepSeek alias, `inspect` reports
`architecture: deepseek`, and `tensors` shows `token_embd.weight` with shape
`[4096,129280]`, dtype `F16`, and `1059061760` tensor bytes.

Alias policy is strict. Use canonical aliases such as
`deepseek4-v4-flash-selected-embed`; simple labels such as `controlled` are
rejected.

## 5. Materialize and attach DeepSeek selected weights

Materialization copies the selected tensor bytes into backend-owned storage.
Engine attachment then makes that backend-resident tensor engine-owned runtime
state. Neither step executes a transformer graph.

```sh
./yvex materialize --model "$DEEPSEEK_ALIAS" --backend cpu
./yvex engine --model "$DEEPSEEK_ALIAS" --backend cpu
./yvex session "$DEEPSEEK_ALIAS" --backend cpu
```

CUDA-capable hosts can also run:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model "$DEEPSEEK_ALIAS" --backend cuda
./yvex engine --model "$DEEPSEEK_ALIAS" --backend cuda
./yvex session "$DEEPSEEK_ALIAS" --backend cuda
```

Expected outcome:

```text
materialization status: materialized
weights_attached: true
weights_backend: cpu or cuda
weight_tensor_count: 1
weight_total_bytes: 1059061760
execution_ready: false
graph_execution_ready: false
```

The engine owns the attached backend tensors. The session observes that state;
it does not own or execute the weights.

## 6. Optional controlled GGUF sanity check

Use the controlled GGUF path only when you want a repository-local sanity check
that does not require external model weights. This is not the DeepSeek pipeline,
and it intentionally reports `architecture: llama`.

```sh
./yvex gguf-emit controlled --out "$tmpdir/controlled.gguf" --overwrite
./yvex inspect "$tmpdir/controlled.gguf"
./yvex metadata "$tmpdir/controlled.gguf"
./yvex tensors "$tmpdir/controlled.gguf"
```

Expected outcome: `inspect` reports GGUF v3 descriptor facts, `metadata` prints
parsed metadata, and `tensors` shows the controlled `token_embd.weight` tensor.

## 7. Materialize controlled weights on CPU and CUDA

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

## 8. Local registry notes

The local model registry maps canonical aliases to external GGUF paths. When the
artifact filename follows the YVEX naming grammar, `models add` can derive the
alias from the filename and `--alias` is optional.

```sh
./yvex models add --path "$DEEPSEEK_GGUF"
./yvex models list
./yvex inspect "$DEEPSEEK_ALIAS"
./yvex tensors "$DEEPSEEK_ALIAS"
```

Expected outcome: the DeepSeek alias resolves through the registry. The alias
is local state; it does not change runtime capability.

## 9. Select the current model

Select one registered alias as the current model for console diagnostics.

```sh
./yvex models use "$DEEPSEEK_ALIAS"
./yvex models current
```

Expected outcome: `models current` prints the DeepSeek alias. No command may
silently choose the first model in the registry.

## 10. Use chat / REPL diagnostics only with tokenizer-bearing artifacts

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

## 11. Run yvexd with the DeepSeek alias

`yvexd` is a provider/status shell. It accepts a direct path or registered alias
for `--model`, then serves status endpoints. In one-request mode it exits after
serving one HTTP request.

Direct path:

```sh
./yvexd --model "$DEEPSEEK_GGUF" --backend cpu \
  --host 127.0.0.1 --port 18080 --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18080/v1/models
wait "$server_pid" || true
```

Alias:

```sh
./yvexd --model "$DEEPSEEK_ALIAS" --backend cpu \
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

## 12. Active DeepSeek selected artifact facts

The active DeepSeek selected embedding artifact is external operator state. The
commands below should pass after section 4 registration.

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

If the artifact is absent on this host, set `DEEPSEEK_GGUF` to the
operator-local copy and register it:

```sh
DEEPSEEK_GGUF=/path/to/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
./yvex models add --path "$DEEPSEEK_GGUF" --alias "$DEEPSEEK_ALIAS"
./yvex models use "$DEEPSEEK_ALIAS"
```

Do not commit the artifact or the local registry.

## 13. Run model and materialization gates

Use gates for repeatable selected-artifact checks. These commands are long
because they encode expected file identity and tensor facts.

```sh
./yvex model-gate check \
  --model "$DEEPSEEK_ALIAS" \
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
  --model "$DEEPSEEK_ALIAS" \
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

## 14. Open-weight intake commands

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

## 15. Quantization / imatrix / provenance commands

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

## 16. Validate the repository

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

## 17. Artifact and path hygiene

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

## 18. Debugging checklist

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

## 19. Benchmarking and evaluation status

Benchmarking and capability evaluation are not implemented yet. Throughput,
token latency, official-vector evaluation, logits regression, and generation
quality suites belong after the relevant graph/logits/generation runtime exists.

Do not create benchmark claims from materialization, CUDA probing, daemon
status, or diagnostic console behavior.

## 20. What is not implemented yet

The current runtime does not implement full model execution, prefill, decode,
sampling, generation, provider-compatible generation, full DeepSeek support,
full GGUF conversion, KV execution, SSD streaming, distributed inference, or
benchmark performance.

`execution_ready` remains false until scheduled graph operations, required graph
weights, scratch, KV, logits, decode, and sampling form an implemented path.
