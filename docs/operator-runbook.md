# YVEX Operator Runbook

This runbook is the command-first path through the current YVEX operator
surface. It uses repository-local binaries, operator-local model storage,
selected GGUF emission, registry aliases, backend materialization, engine
attachment, daemon status, and gates. It does not describe inference or
generation because those paths are not implemented.

The default examples assume this repository sits next to an operator-owned
`../models/` directory:

```text
../models/hf/deepseek/DeepSeek-V4-Flash
../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

That layout is a convenience, not a project rule. If your weights or GGUFs live
elsewhere, replace the path arguments passed to `--native-source`, `--out`,
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
  --local-path ../models/hf/deepseek/DeepSeek-V4-Flash \
  --status in-progress \
  --out ../models/gguf/deepseek/deepseek-source-manifest.json

./yvex native-weights \
  --source ../models/hf/deepseek/DeepSeek-V4-Flash \
  --limit 20

./yvex tensor-map \
  --arch deepseek4 \
  --native-source ../models/hf/deepseek/DeepSeek-V4-Flash \
  --limit 20

./yvex convert plan \
  --arch deepseek4 \
  --native-source ../models/hf/deepseek/DeepSeek-V4-Flash \
  --out-plan ../models/gguf/deepseek/deepseek-selected-plan.json

./yvex convert emit \
  --arch deepseek4 \
  --native-source ../models/hf/deepseek/DeepSeek-V4-Flash \
  --tensor embed.weight \
  --target-qtype F16 \
  --out ../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
  --overwrite

./yvex inspect ../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
./yvex tensors ../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
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
  --path ../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
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

## 5. Materialize and attach DeepSeek selected weights

Materialization copies the selected tensor bytes into backend-owned storage.
Engine attachment then makes that backend-resident tensor engine-owned runtime
state. Neither step executes a transformer graph.

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
weights_attached: true
weights_backend: cpu or cuda
weight_tensor_count: 1
weight_total_bytes: 1059061760
execution_ready: false
graph_execution_ready: false
```

The engine owns the attached backend tensors. The session observes that state;
it does not own or execute the weights.

## 6. Run yvexd with the DeepSeek alias

`yvexd` is a provider/status shell. It accepts a direct path or registered alias
for `--model`, then serves status endpoints. In one-request mode it exits after
serving one HTTP request.

Direct path:

```sh
./yvexd \
  --model ../models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf \
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

## 7. Run model and materialization gates

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
  --report-out ../models/gguf/deepseek/deepseek-model-gate.txt
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
  --report-out ../models/gguf/deepseek/deepseek-materialize-gate.txt
```

Expected outcome: gate status is pass when the selected artifact and CUDA host
are available. The result remains selected-tensor materialization only.

## 8. Use chat / REPL diagnostics only with tokenizer-bearing artifacts

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

## 9. Optional controlled GGUF sanity check

Use the controlled GGUF path only when you want a repository-local sanity check
that does not require external model weights. This is not the DeepSeek pipeline,
and it intentionally reports `architecture: llama`.

```sh
./yvex gguf-emit controlled --out ../models/gguf/deepseek/controlled-yvex-fixture.gguf --overwrite
./yvex inspect ../models/gguf/deepseek/controlled-yvex-fixture.gguf
./yvex metadata ../models/gguf/deepseek/controlled-yvex-fixture.gguf
./yvex tensors ../models/gguf/deepseek/controlled-yvex-fixture.gguf
./yvex materialize --model ../models/gguf/deepseek/controlled-yvex-fixture.gguf --backend cpu
```

Expected outcome: `status: weights-materialized` and `execution_ready: false`.
This proves controlled emission, parsing, tensor table inspection, and selected
materialization, not full model execution.

## 10. Quantization / imatrix / provenance commands

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

## 11. Validate the repository

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

## 12. Artifact and path hygiene

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

## 13. Debugging checklist

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

## 14. Benchmarking and evaluation status

Benchmarking and capability evaluation are not implemented yet. Throughput,
token latency, official-vector evaluation, logits regression, and generation
quality suites belong after the relevant graph/logits/generation runtime exists.

Do not create benchmark claims from materialization, CUDA probing, daemon
status, or diagnostic console behavior.

## 15. What is not implemented yet

The current runtime does not implement full model execution, prefill, decode,
sampling, generation, provider-compatible generation, full DeepSeek support,
full GGUF conversion, KV execution, SSD streaming, distributed inference, or
benchmark performance.

`execution_ready` remains false until scheduled graph operations, required graph
weights, scratch, KV, logits, decode, and sampling form an implemented path.
