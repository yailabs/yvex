# YVEX Operator Runbook

## What this runbook is

This runbook is the current operator transcript for YVEX.

It is designed to be rerun after every implementation wave.

It is not a product tutorial.

It is not a generation claim.

It checks the implemented source, artifact, registry, integrity, backend, graph,
runtime-diagnostic, daemon-status, and validation surfaces.

The future shape is shorter: prepare a model, run/chat, serve.

The current shape is longer because YVEX still exposes its internal boundaries
explicitly: source intake, YVEX-produced artifact creation, artifact identity,
registry selection, backend materialization, graph proof, runtime diagnostics,
daemon status, and repository validation.

Run the transcript from the `yailabs/yvex` source repository root.

Keep real source tensors, generated GGUFs, reports, reference artifacts, local
registries, logs, and caches outside the source repository.

## One-paste transcript

Paste this block from the repository root.

Change only `YVEX_MODELS_ROOT` and `YVEX_BACKEND` when needed.

Default model storage:

```text
$HOME/lab/models
```

Default backend:

```text
cpu
```

The transcript uses shell-local variables. It does not require an export wall.

```sh
set -u

models_root="${YVEX_MODELS_ROOT:-$HOME/lab/models}"
backend="${YVEX_BACKEND:-cpu}"

hf_root="$models_root/hf"
gguf_root="$models_root/gguf"
report_root="$models_root/reports"
reference_root="$models_root/reference"
registry_root="$models_root/registry"

deepseek_source="$hf_root/deepseek/DeepSeek-V4-Flash"
deepseek_gguf_dir="$gguf_root/deepseek"
deepseek_report_dir="$report_root/deepseek"

selected_alias="deepseek4-v4-flash-selected-embed"
segment_alias="deepseek4-v4-flash-selected-embed-rmsnorm"
glm_target="glm-5.2-official-safetensors"

selected_gguf="$deepseek_gguf_dir/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
segment_gguf="$deepseek_gguf_dir/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
fixture_gguf="$deepseek_gguf_dir/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"

tokenizer_fixture="tests/fixtures/gguf/valid-tokenizer-simple.gguf"

policy_json="$deepseek_report_dir/policy.json"
quant_job_json="$deepseek_report_dir/quant-job.json"
imatrix_json="$deepseek_report_dir/imatrix.json"
template_json="$deepseek_report_dir/template.json"

mkdir -p "$hf_root" "$gguf_root" "$report_root" "$reference_root" "$registry_root" "$deepseek_gguf_dir" "$deepseek_report_dir"

selected_ref=""
segment_ref=""

printf '\n== build and discover ==\n'
make
./yvex version
./yvex --version
./yvex commands
./yvex info
./yvexd --help

printf '\n== help surface ==\n'
./yvex help backend
./yvex help chat
./yvex help commands
./yvex help convert
./yvex help cuda-info
./yvex help detokenize
./yvex help engine
./yvex help graph
./yvex help gguf-template
./yvex help gguf-emit
./yvex help help
./yvex help imatrix
./yvex help info
./yvex help inspect
./yvex help input
./yvex help integrity
./yvex help kv
./yvex help materialize
./yvex help materialize-gate
./yvex help metadata
./yvex help model-gate
./yvex help model-target
./yvex help models
./yvex help native-weights
./yvex help paths
./yvex help plan
./yvex help prefill
./yvex help prompt
./yvex help quant-job
./yvex help quant-policy
./yvex help qtype-support
./yvex help run
./yvex help session
./yvex help source-manifest
./yvex help tensor-map
./yvex help tokenize
./yvex help tokenizer
./yvex help tensors
./yvex help version

printf '\n== model targets ==\n'
./yvex model-target classes
./yvex model-target list
./yvex model-target inspect "$selected_alias"
./yvex model-target inspect "$segment_alias"
./yvex model-target inspect "$glm_target"

printf '\n== paths and backend ==\n'
./yvex paths
./yvex paths --run
./yvex paths --run --create
./yvex backend cpu
if ./yvex cuda-info >/tmp/yvex-cuda-info.txt 2>&1; then
  cat /tmp/yvex-cuda-info.txt
  ./yvex backend cuda
else
  cat /tmp/yvex-cuda-info.txt
  printf 'cuda unavailable: continuing CPU transcript\n'
fi
rm -f /tmp/yvex-cuda-info.txt

printf '\n== tokenizer fixture diagnostics ==\n'
./yvex tokenizer "$tokenizer_fixture"
./yvex tokenize "$tokenizer_fixture" --text "hello"
./yvex detokenize "$tokenizer_fixture" --ids 0,1
./yvex prompt "$tokenizer_fixture" --user "hello" --tokens
./yvex input prompt --model "$tokenizer_fixture" --text "hello"
./yvex input tokens --model "$tokenizer_fixture" --tokens 0,1

printf '\n== standalone graph proofs ==\n'
./yvex graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1

printf '\n== controlled tiny GGUF fixture ==\n'
./yvex gguf-emit controlled --out "$fixture_gguf" --model-name yvex-controlled-fixture --arch deepseek --overwrite
./yvex inspect "$fixture_gguf"
./yvex tensors "$fixture_gguf"
./yvex metadata "$fixture_gguf"
./yvex graph --model "$fixture_gguf" --backend cpu --execute-fixture --fixture-token 0
./yvex graph --model "$fixture_gguf" --backend cpu --execute-fixture --fixture-token 1

printf '\n== controlled block fixture ==\n'
./yvex graph --backend cpu --execute-block --block fixture --seq-len 4 --position 3 --hidden-dim 8 --head-dim 8 --ffn-dim 16

printf '\n== source intake and selected GGUF refresh ==\n'
if [ -d "$deepseek_source" ]; then
  ./yvex source-manifest create --hf-repo "deepseek-ai/DeepSeek-V4-Flash" --revision "main" --local-path "$deepseek_source" --status in-progress --out "$deepseek_gguf_dir/deepseek-source-manifest.json"
  ./yvex native-weights --source "$deepseek_source" --limit 20
  ./yvex tensor-map --arch deepseek4 --native-source "$deepseek_source" --limit 20
  ./yvex convert plan --arch deepseek4 --native-source "$deepseek_source" --out-plan "$deepseek_gguf_dir/deepseek-selected-plan.json"
  ./yvex convert emit --arch deepseek4 --native-source "$deepseek_source" --tensor embed.weight --target-qtype F16 --out "$selected_gguf" --overwrite
  ./yvex inspect "$selected_gguf"
  ./yvex tensors "$selected_gguf"
  ./yvex metadata "$selected_gguf"
else
  printf 'skip source intake: %s not found\n' "$deepseek_source"
fi

printf '\n== alias refresh ==\n'
if [ -f "$selected_gguf" ]; then
  ./yvex models remove "$selected_alias" || true
  ./yvex models add --path "$selected_gguf" --alias "$selected_alias" --support-level selected-tensor-materialized
  ./yvex models use "$selected_alias"
  ./yvex models current
  ./yvex models list
  ./yvex models inspect "$selected_alias"
  ./yvex models verify "$selected_alias"
  selected_ref="$selected_alias"
else
  printf 'skip selected alias: %s not found\n' "$selected_gguf"
fi

if [ -f "$segment_gguf" ]; then
  ./yvex models remove "$segment_alias" || true
  ./yvex models add --path "$segment_gguf" --alias "$segment_alias" --support-level selected-tensor-materialized
  ./yvex models inspect "$segment_alias"
  ./yvex models verify "$segment_alias"
  segment_ref="$segment_alias"
else
  printf 'skip segment alias: %s not found\n' "$segment_gguf"
fi

printf '\n== selected artifact validation ==\n'
if [ -n "${selected_ref:-}" ]; then
  ./yvex inspect "$selected_ref"
  ./yvex tensors "$selected_ref"
  ./yvex metadata "$selected_ref"
  ./yvex integrity check --model "$selected_ref"
  ./yvex integrity check --model "$selected_ref" --require-token-embedding --partial-token 0
  ./yvex integrity report --model "$selected_ref" --backend cpu --require-token-embedding --partial-token 0
  ./yvex materialize --model "$selected_ref" --backend cpu
  ./yvex engine --model "$selected_ref" --backend cpu
  ./yvex session "$selected_ref" --backend cpu
  ./yvex plan "$selected_ref" --backend cpu --seq 1 --ctx 16
  ./yvex graph --model "$selected_ref" --backend cpu --execute-partial --partial-token 0
else
  printf 'skip selected artifact validation: no selected_ref\n'
fi

printf '\n== selected segment validation ==\n'
if [ -n "${segment_ref:-}" ]; then
  ./yvex inspect "$segment_ref"
  ./yvex tensors "$segment_ref"
  ./yvex integrity check --model "$segment_ref" --require-token-embedding --partial-token 0
  ./yvex integrity report --model "$segment_ref" --backend cpu --require-token-embedding --partial-token 0
  ./yvex materialize --model "$segment_ref" --backend cpu
  ./yvex engine --model "$segment_ref" --backend cpu
  ./yvex session "$segment_ref" --backend cpu
  ./yvex input tokens --model "$segment_ref" --tokens 0,1
  ./yvex graph --model "$segment_ref" --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 1
  ./yvex prefill --model "$segment_ref" --backend cpu --segment embedding-rmsnorm --tokens 0,1
  ./yvex prefill --model "$segment_ref" --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
else
  printf 'skip selected segment validation: no segment_ref\n'
fi

printf '\n== KV diagnostics ==\n'
./yvex kv --layers 1 --heads 2 --head-dim 4 --capacity 8 --append-demo --read-position 0

printf '\n== gates ==\n'
if [ -n "${selected_ref:-}" ]; then
  ./yvex model-gate check --model "$selected_ref" --label deepseek-v4-flash-selected-embedding --family deepseek4 --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --report-out "$deepseek_report_dir/deepseek-model-gate-cpu.txt"
  ./yvex materialize-gate check --model "$selected_ref" --label deepseek-v4-flash-selected-embedding --family deepseek4 --scope selected-tensor --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4096,129280 --expect-dtype F16 --expect-bytes 1059061760 --backend cpu --require-cpu --repeat 3 --check-cleanup --report-out "$deepseek_report_dir/deepseek-materialize-gate-cpu.txt"
else
  printf 'skip gates: no selected_ref\n'
fi

printf '\n== quant/template command coverage ==\n'
./yvex qtype-support
if [ -f "$policy_json" ]; then ./yvex quant-policy inspect --policy "$policy_json"; ./yvex quant-policy validate --policy "$policy_json"; else printf 'skip quant-policy: %s not found\n' "$policy_json"; fi
if [ -f "$quant_job_json" ]; then ./yvex quant-job inspect --manifest "$quant_job_json"; ./yvex quant-job validate --manifest "$quant_job_json"; else printf 'skip quant-job: %s not found\n' "$quant_job_json"; fi
if [ -f "$imatrix_json" ]; then ./yvex imatrix inspect --manifest "$imatrix_json"; ./yvex imatrix validate --manifest "$imatrix_json"; else printf 'skip imatrix: %s not found\n' "$imatrix_json"; fi
if [ -f "$template_json" ]; then ./yvex gguf-template inspect --template "$template_json"; ./yvex gguf-template validate --template "$template_json"; if [ -d "$deepseek_source" ]; then ./yvex gguf-template compare --template "$template_json" --native-source "$deepseek_source"; fi; else printf 'skip gguf-template: %s not found\n' "$template_json"; fi

printf '\n== daemon status ==\n'
if [ -n "${selected_ref:-}" ]; then
  ./yvexd --model "$selected_ref" --backend cpu --host 127.0.0.1 --port 18080 --one-request &
  server_pid=$!
  sleep 1
  curl -s http://127.0.0.1:18080/v1/models
  wait "$server_pid" || true
else
  printf 'skip daemon models endpoint: no selected_ref\n'
fi

./yvexd --host 127.0.0.1 --port 18081 --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18081/health
wait "$server_pid" || true

./yvexd --host 127.0.0.1 --port 18082 --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18082/metrics
wait "$server_pid" || true

printf '\n== accepted-only chat/run diagnostics ==\n'
printf '/status\n/quit\n' | ./yvex chat --model "$tokenizer_fixture" --backend cpu
./yvex run --model "$tokenizer_fixture" --backend cpu --prompt "hello"

printf '\n== repository validation ==\n'
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
if ./yvex cuda-info >/dev/null 2>&1; then make check-cuda; else printf 'skip make check-cuda: CUDA unavailable\n'; fi

printf '\n== artifact hygiene ==\n'
git status --short
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
grep -R -nE '(/home|/Users|/mnt)/[^[:space:]]+' README.md MODEL_ARTIFACTS.md AGENTS.md docs/contract.md docs/spine.md docs/operator-runbook.md || true

printf '\nYVEX operator transcript complete.\n'
```

## Full command inventory

Every implemented `./yvex` command must appear here.

Each line says where the command appears in the one-paste transcript.

```text
backend — paths and backend
chat — accepted-only chat/run diagnostics
commands — build and discover
convert — source intake and selected GGUF refresh
cuda-info — paths and backend, repository validation
detokenize — tokenizer fixture diagnostics
engine — selected artifact validation, selected segment validation
graph — standalone graph proofs, controlled tiny GGUF fixture, selected artifact validation, selected segment validation, controlled block fixture
gguf-template — quant/template command coverage, conditional
gguf-emit — controlled tiny GGUF fixture
help — help surface
imatrix — quant/template command coverage, conditional
info — build and discover
inspect — controlled tiny GGUF fixture, source intake, selected artifact validation, selected segment validation
input — tokenizer fixture diagnostics, selected segment validation
integrity — selected artifact validation, selected segment validation
kv — KV diagnostics
materialize — selected artifact validation, selected segment validation
materialize-gate — gates, conditional on selected artifact
metadata — controlled tiny GGUF fixture, source intake, selected artifact validation
model-gate — gates, conditional on selected artifact
model-target — model targets
models — alias refresh
native-weights — source intake and selected GGUF refresh, conditional on source tensors
paths — paths and backend
plan — selected artifact validation
prefill — selected segment validation, conditional on segment artifact
prompt — tokenizer fixture diagnostics
quant-job — quant/template command coverage, conditional
quant-policy — quant/template command coverage, conditional
qtype-support — quant/template command coverage
run — accepted-only chat/run diagnostics
session — selected artifact validation, selected segment validation
source-manifest — source intake and selected GGUF refresh, conditional on source tensors
tensor-map — source intake and selected GGUF refresh, conditional on source tensors
tokenize — tokenizer fixture diagnostics
tokenizer — tokenizer fixture diagnostics
tensors — controlled tiny GGUF fixture, source intake, selected artifact validation, selected segment validation
version — build and discover
```

The inventory is not a capability claim. It is a map from implemented command
names to the transcript location that exercises or discovers them.

## Artifact roots and generated files

Source tensors stay in operator-local model storage:

```text
$HOME/lab/models/hf/<family>/<model>/
```

YVEX-produced GGUFs stay in operator-local model storage:

```text
$HOME/lab/models/gguf/<family>/
```

External reference artifacts stay in:

```text
$HOME/lab/models/reference/<family>/
```

Reports stay in:

```text
$HOME/lab/models/reports/<family>/
```

The source repository stays clean.

Publishing YVEX-produced GGUFs later means uploading them to an artifact
distribution target, not committing them to the source repository.

## Optional flows

### CUDA rerun

Run the transcript again with CUDA selected:

```sh
YVEX_BACKEND=cuda sh -c '<paste the one-paste transcript again from the top>'
```

The transcript itself detects CUDA for `make check-cuda`.

### Direct path mode

The transcript uses aliases after registration.

To force direct path mode, run the relevant commands manually with:

```text
$selected_gguf
$segment_gguf
```

Do not set a global direct-path mode unless you are debugging registry behavior.

### Isolated registry mode

Use an isolated registry only for registry experiments:

```sh
YVEX_MODELS_REGISTRY="$HOME/lab/models/registry/models.local.json"
```

The default transcript uses the normal local registry.

### GLM source download status

GLM source tensors are downloaded outside the transcript.

Use this status check:

```sh
models_root="${YVEX_MODELS_ROOT:-$HOME/lab/models}"
pid_file="$models_root/logs/glm52-safetensors-download.pid"
log_file="$models_root/logs/glm52-safetensors-download.log"
[ -f "$pid_file" ] && ps -p "$(cat "$pid_file")" -o pid,stat,etime,cmd || true
[ -f "$log_file" ] && tail -n 40 "$log_file" || true
find "$models_root/hf/glm/GLM-5.2" -maxdepth 1 -type f -name "*.safetensors" | wc -l
du -sh "$models_root/hf/glm/GLM-5.2" 2>/dev/null || true
```

Do not add GLM download commands to the main transcript.

## Current boundary

Current YVEX can inspect, emit selected artifacts, register aliases, validate
integrity, materialize selected tensors, attach engine/session state, run
controlled graph proofs, run selected graph slices, run segment-summary prefill,
run minimal KV diagnostics, run daemon status checks, and run accepted-only
chat/run diagnostics.

Current YVEX does not implement complete transformer execution, full transformer
prefill, decode, logits, sampling, generation, provider generation, capability
evaluation, or benchmarks.

Future shape:

```text
prepare model
run/chat
serve
stream
evaluate
benchmark
```

That future shape is not claimed by the current implementation.

## Manual debug fallback

Use this order before assuming a runtime bug:

```text
1. ./yvex commands
2. ./yvex help <command>
3. ./yvex model-target list
4. ./yvex models current
5. ./yvex models list
6. ./yvex inspect <model-or-alias>
7. ./yvex tensors <model-or-alias>
8. ./yvex integrity check --model <model-or-alias>
9. ./yvex backend cpu
10. ./yvex cuda-info
11. rerun CPU before CUDA
12. check git status before committing
```
