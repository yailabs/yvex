# YVEX Operator Runbook

This runbook is the operator transcript for the current YVEX surface.
It is not a product tutorial and not a generation claim.
It is designed to be rerun after every implementation wave.

YVEX currently separates:

```text
source intake
YVEX-produced artifact creation
artifact identity/integrity
registry selection
backend materialization
graph proof
runtime diagnostics
daemon/status checks
repository validation
```

The future operator path is a short model prepare plus run/chat flow. That path
is not claimed until runtime generation exists.

Run these commands from the `yailabs/yvex` source repository. Keep source
weights, generated GGUFs, reports, local registries, and logs in operator-local
model storage outside this repository.

## 1. Bootstrap Operator Context

Copy this once per shell. Only `YVEX_MODELS_ROOT`, `YVEX_BACKEND`, and registry
mode should normally be changed by hand. Everything else is derived.

```sh
# Run from the YVEX repository root.

: "${YVEX_MODELS_ROOT:=$HOME/lab/models}"
: "${YVEX_FAMILY:=deepseek}"
: "${YVEX_BACKEND:=cpu}"
: "${YVEX_REGISTRY_MODE:=local}"

export YVEX_MODELS_ROOT
export YVEX_FAMILY
export YVEX_BACKEND
export YVEX_REGISTRY_MODE

export YVEX_HF_ROOT="$YVEX_MODELS_ROOT/hf"
export YVEX_GGUF_ROOT="$YVEX_MODELS_ROOT/gguf"
export YVEX_REPORT_ROOT="$YVEX_MODELS_ROOT/reports"
export YVEX_REFERENCE_ROOT="$YVEX_MODELS_ROOT/reference"
export YVEX_REGISTRY_ROOT="$YVEX_MODELS_ROOT/registry"

export DEEPSEEK_SOURCE="$YVEX_HF_ROOT/deepseek/DeepSeek-V4-Flash"
export DEEPSEEK_GGUF_DIR="$YVEX_GGUF_ROOT/deepseek"
export DEEPSEEK_REPORT_DIR="$YVEX_REPORT_ROOT/deepseek"

export SELECTED_ALIAS="deepseek4-v4-flash-selected-embed"
export SEGMENT_ALIAS="deepseek4-v4-flash-selected-embed-rmsnorm"
export GLM_TARGET="glm-5.2-official-safetensors"

export SELECTED_GGUF="$DEEPSEEK_GGUF_DIR/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
export SEGMENT_GGUF="$DEEPSEEK_GGUF_DIR/deepseek4-v4-flash-selected-embed-rmsnorm-F16-noimatrix-yvex-v1.gguf"
export FIXTURE_GGUF="$DEEPSEEK_GGUF_DIR/deepseek4-v4-flash-fixture-embed-F32-noimatrix-yvex-v1.gguf"

export SELECTED_MODEL_REF="$SELECTED_ALIAS"
export SEGMENT_MODEL_REF="$SEGMENT_ALIAS"

export TOKENIZER_FIXTURE="tests/fixtures/gguf/valid-tokenizer-simple.gguf"

mkdir -p \
  "$YVEX_HF_ROOT" \
  "$YVEX_GGUF_ROOT" \
  "$YVEX_REPORT_ROOT" \
  "$YVEX_REFERENCE_ROOT" \
  "$YVEX_REGISTRY_ROOT" \
  "$DEEPSEEK_GGUF_DIR" \
  "$DEEPSEEK_REPORT_DIR"
```

Registry mode is normally local. Use an isolated registry only when you want the
runbook to avoid the default local model registry.

```sh
if [ "$YVEX_REGISTRY_MODE" = "isolated" ]; then
  export YVEX_MODELS_REGISTRY="$YVEX_REGISTRY_ROOT/models.local.json"
else
  unset YVEX_MODELS_REGISTRY
fi
```

## Artifact Storage Policy

Source weights live outside the source repository:

```text
$YVEX_MODELS_ROOT/hf/<family>/<model>/
```

YVEX-produced GGUF artifacts live outside the source repository:

```text
$YVEX_MODELS_ROOT/gguf/<family>/
```

External reference artifacts live outside the source repository:

```text
$YVEX_MODELS_ROOT/reference/<family>/
```

Generated reports live outside the source repository:

```text
$YVEX_MODELS_ROOT/reports/<family>/
```

The source repository is `yailabs/yvex`. The artifact root is operator-local
model storage. Publishing YVEX-produced GGUFs later means publishing to an
artifact distribution target such as Hugging Face Hub, not committing real model
artifacts to the source repository.

No `.safetensors`, `.bin`, `.dat`, or real `.gguf` file belongs in the YVEX
source repository. Tiny parser fixtures under `tests/fixtures/gguf/` are the
only exception.

## 2. Quick Start: Current Runtime Boundary

Use this block to verify the repository and current command surface before
touching local model artifacts.

```sh
make
./yvex commands
./yvex model-target list
./yvex models current || true
./yvex backend "$YVEX_BACKEND"
./yvex help graph
```

If a selected DeepSeek alias is already registered, continue with the selected
artifact validation flow. If it is not registered, run the build-or-register
artifact flow below.

## 3. Operator Modes

Mode A - no model artifacts required:

```text
build/discover
model-target registry
CPU standalone graph ops
controlled fixture graph
tokenizer fixture diagnostics
repository validation
```

Mode B - selected DeepSeek GGUF available:

```text
register alias
inspect/tensors/metadata
integrity
materialize
engine/session
selected graph partial
gates
daemon status
```

Mode C - selected embedding-plus-RMSNorm GGUF available:

```text
register segment alias
selected segment graph
prefill state summary
prefill plus minimal KV binding
```

Mode D - CUDA-capable host:

```text
cuda-info
backend cuda
CUDA graph ops
CUDA materialization
make check-cuda
```

Mode E - source tensor intake:

```text
source-manifest
native-weights
tensor-map
convert plan
convert emit
```

Mode F - future generation path:

```text
model prepare
runtime generation
chat generation
daemon/provider generation
streaming generation
```

Mode F is future-only until decode, logits, sampling, and generation exist.

## 4. Build Or Refresh Selected DeepSeek GGUF

Run this section only when the operator-local DeepSeek source tensors already
exist under `$DEEPSEEK_SOURCE`.

```sh
./yvex source-manifest create \
  --hf-repo "deepseek-ai/DeepSeek-V4-Flash" \
  --revision "main" \
  --local-path "$DEEPSEEK_SOURCE" \
  --status in-progress \
  --out "$DEEPSEEK_GGUF_DIR/deepseek-source-manifest.json"

./yvex native-weights --source "$DEEPSEEK_SOURCE" --limit 20
./yvex tensor-map --arch deepseek4 --native-source "$DEEPSEEK_SOURCE" --limit 20

./yvex convert plan \
  --arch deepseek4 \
  --native-source "$DEEPSEEK_SOURCE" \
  --out-plan "$DEEPSEEK_GGUF_DIR/deepseek-selected-plan.json"

./yvex convert emit \
  --arch deepseek4 \
  --native-source "$DEEPSEEK_SOURCE" \
  --tensor embed.weight \
  --target-qtype F16 \
  --out "$SELECTED_GGUF" \
  --overwrite

./yvex inspect "$SELECTED_GGUF"
./yvex tensors "$SELECTED_GGUF"
./yvex metadata "$SELECTED_GGUF"
```

This selected conversion emits only the selected embedding GGUF. It does not
create the embedding-plus-RMSNorm segment artifact and does not create a full
model artifact.

## 5. Register Or Refresh Aliases

Register the selected artifact idempotently. After this block, the rest of the
runbook can use `$SELECTED_MODEL_REF`.

```sh
./yvex models remove "$SELECTED_ALIAS" || true
./yvex models add \
  --path "$SELECTED_GGUF" \
  --alias "$SELECTED_ALIAS" \
  --support-level selected-tensor-materialized
./yvex models use "$SELECTED_ALIAS"
./yvex models current
./yvex models inspect "$SELECTED_ALIAS"

export SELECTED_MODEL_REF="$SELECTED_ALIAS"
```

Only run the segment alias block if `$SEGMENT_GGUF` already exists. The selected
embedding-only conversion flow does not create this segment artifact.

```sh
if [ -f "$SEGMENT_GGUF" ]; then
  ./yvex models remove "$SEGMENT_ALIAS" || true
  ./yvex models add \
    --path "$SEGMENT_GGUF" \
    --alias "$SEGMENT_ALIAS" \
    --support-level selected-tensor-materialized
  ./yvex models inspect "$SEGMENT_ALIAS"
  export SEGMENT_MODEL_REF="$SEGMENT_ALIAS"
else
  printf 'skip segment alias: %s not found\n' "$SEGMENT_GGUF"
fi
```

Direct path mode remains valid if you do not want to use aliases:

```sh
export SELECTED_MODEL_REF="$SELECTED_GGUF"
export SEGMENT_MODEL_REF="$SEGMENT_GGUF"
```

## 6. Verify Artifact Identity And Integrity

Run the selected artifact chain in this order: descriptor, tensor table,
metadata, integrity check, and operator integrity report.

```sh
./yvex inspect "$SELECTED_MODEL_REF"
./yvex tensors "$SELECTED_MODEL_REF"
./yvex metadata "$SELECTED_MODEL_REF"

./yvex integrity check \
  --model "$SELECTED_MODEL_REF" \
  --require-token-embedding \
  --partial-token 0

./yvex integrity report \
  --model "$SELECTED_MODEL_REF" \
  --backend "$YVEX_BACKEND" \
  --require-token-embedding \
  --partial-token 0
```

If `$SELECTED_MODEL_REF` is an alias, verify registry identity and metadata
drift before materialization or graph execution.

```sh
./yvex models verify "$SELECTED_ALIAS"
```

The report is local operator evidence only. It is not remote provenance, model
quality evidence, malware detection, sandboxing, or a generation claim.

## 7. Materialize And Attach Runtime State

CPU is the default path. `$YVEX_BACKEND` defaults to `cpu`, so this block can be
rerun after every artifact refresh.

```sh
./yvex materialize --model "$SELECTED_MODEL_REF" --backend "$YVEX_BACKEND"
./yvex engine --model "$SELECTED_MODEL_REF" --backend "$YVEX_BACKEND"
./yvex session "$SELECTED_MODEL_REF" --backend "$YVEX_BACKEND"
```

CUDA add-on:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex materialize --model "$SELECTED_MODEL_REF" --backend cuda
./yvex engine --model "$SELECTED_MODEL_REF" --backend cuda
./yvex session "$SELECTED_MODEL_REF" --backend cuda
./yvex graph --model "$SELECTED_MODEL_REF" --backend cuda --execute-partial --partial-token 0
```

Materialization creates backend-owned tensor residency. Engine/session
attachment exposes runtime state. These commands do not execute a full
transformer path.

## 8. Execute Current Graph Proofs

Standalone graph ops do not open a model artifact. They prove bounded primitive
behavior over deterministic F32 inputs.

```sh
./yvex graph --backend "$YVEX_BACKEND" --execute-op --op rope \
  --position 7 \
  --head-dim 8

./yvex graph --backend "$YVEX_BACKEND" --execute-op --op attention \
  --seq-len 4 \
  --position 3 \
  --head-dim 8 \
  --causal

./yvex graph --backend "$YVEX_BACKEND" --execute-op --op matmul \
  --m 1 \
  --k 8 \
  --n 8

./yvex graph --backend "$YVEX_BACKEND" --execute-op --op mlp \
  --hidden-dim 8 \
  --ffn-dim 16 \
  --activation silu \
  --gated
```

Controlled fixture graph creates a tiny local GGUF under operator-local model
storage and executes deterministic embedding fixture tokens.

```sh
./yvex gguf-emit controlled \
  --out "$FIXTURE_GGUF" \
  --model-name yvex-controlled-fixture \
  --arch deepseek \
  --overwrite

./yvex graph \
  --model "$FIXTURE_GGUF" \
  --backend "$YVEX_BACKEND" \
  --execute-fixture \
  --fixture-token 0

./yvex graph \
  --model "$FIXTURE_GGUF" \
  --backend "$YVEX_BACKEND" \
  --execute-fixture \
  --fixture-token 1
```

Controlled block fixture composes the implemented primitive ops into one
bounded transformer-block-shaped proof. It uses deterministic fixture tensors,
not real model block weights.

```sh
./yvex graph \
  --backend "$YVEX_BACKEND" \
  --execute-block \
  --block fixture \
  --seq-len 4 \
  --position 3 \
  --hidden-dim 8 \
  --head-dim 8 \
  --ffn-dim 16
```

Selected artifact graph executes the selected F16 embedding partial over the
selected artifact.

```sh
./yvex graph \
  --model "$SELECTED_MODEL_REF" \
  --backend "$YVEX_BACKEND" \
  --execute-partial \
  --partial-token 0
```

Selected segment graph requires the embedding-plus-RMSNorm segment artifact.

```sh
if [ -f "$SEGMENT_GGUF" ]; then
  ./yvex graph \
    --model "$SEGMENT_MODEL_REF" \
    --backend "$YVEX_BACKEND" \
    --execute-segment \
    --segment embedding-rmsnorm \
    --tokens 0,1 \
    --token-index 1
else
  printf 'skip segment graph: %s not found\n' "$SEGMENT_GGUF"
fi
```

Future layer scheduling will repeat controlled block execution over token
positions with explicit state, scratch reuse, cleanup, and reference comparison.
It is not a current operator command until the command exists.

## 9. Execute Prefill And KV Diagnostics

Minimal KV ownership is always runnable and does not require model artifacts.

```sh
./yvex kv \
  --layers 1 \
  --heads 2 \
  --head-dim 4 \
  --capacity 8 \
  --append-demo \
  --read-position 0
```

Prefill state summaries require the selected embedding-plus-RMSNorm segment
artifact.

```sh
if [ -f "$SEGMENT_GGUF" ]; then
  ./yvex input tokens \
    --model "$SEGMENT_MODEL_REF" \
    --tokens 0,1

  ./yvex prefill \
    --model "$SEGMENT_MODEL_REF" \
    --backend "$YVEX_BACKEND" \
    --segment embedding-rmsnorm \
    --tokens 0,1

  ./yvex prefill \
    --model "$SEGMENT_MODEL_REF" \
    --backend "$YVEX_BACKEND" \
    --segment embedding-rmsnorm \
    --tokens 0,1,2 \
    --attach-kv \
    --kv-layers 1 \
    --kv-heads 2 \
    --kv-head-dim 4 \
    --kv-capacity 8
else
  printf 'skip prefill diagnostics: %s not found\n' "$SEGMENT_GGUF"
fi
```

This is segment-summary prefill state plus minimal diagnostic KV binding. It is
not attention-backed transformer prefill, decode, logits, sampling, or
generation.

## 10. Run Daemon Status Checks

`yvexd` is a provider/status shell. Use one-request mode for repeatable checks.

```sh
./yvexd \
  --model "$SELECTED_MODEL_REF" \
  --backend "$YVEX_BACKEND" \
  --host 127.0.0.1 \
  --port 18080 \
  --one-request &
server_pid=$!
sleep 1
curl -s http://127.0.0.1:18080/v1/models
wait "$server_pid" || true
```

Additional status endpoints:

```sh
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
```

The daemon status surface does not serve generation.

## 11. Run Diagnostic Console And Accepted-Only Paths

YVEX does not currently behave like DS4 when launched with `./yvex`. DS4 can
enter a generation REPL because its runtime path exists. YVEX will move toward a
short prepare/run/chat path only after runtime generation exists.

Current diagnostic console and run commands use tokenizer-bearing fixtures.
They accept text or prompt input and report diagnostics only.

```sh
printf '/status\n/quit\n' | ./yvex chat \
  --model "$TOKENIZER_FIXTURE" \
  --backend "$YVEX_BACKEND"

./yvex run \
  --model "$TOKENIZER_FIXTURE" \
  --backend "$YVEX_BACKEND" \
  --prompt "hello"
```

Selected DeepSeek embedding artifacts do not contain tokenizer metadata for the
diagnostic console.

## 12. Run Repository Validation

Run the standard validation gate before committing changes.

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
```

CUDA-capable hosts:

```sh
make check-cuda
```

`make check` includes the consolidated integrity regression harness. Operators
do not need to run that harness on their own artifacts; use `integrity check`,
`models verify`, `materialize`, and graph command output for local artifacts.

## 13. Artifact And Path Hygiene

Check that generated artifacts and local model state are not tracked.

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected result:

```text
No downloaded source tensors.
No real model GGUFs.
Tiny test fixtures only.
```

Run a public-doc path leak scan before publishing docs:

```sh
grep -R -nE '(/home|/Users|/mnt)/[^[:space:]]+' \
  README.md MODEL_ARTIFACTS.md AGENTS.md docs/contract.md docs/spine.md docs/operator-runbook.md || true
```

## 14. Manual Debug Order

Use this order before assuming a runtime bug:

```text
run ./yvex help <command>
check the path passed to --native-source
check the path passed to --out
check YVEX_MODELS_REGISTRY only if isolated registry mode is intentional
run ./yvex model-target list
run ./yvex models current
run ./yvex inspect <model>
run ./yvex tensors <model>
run ./yvex integrity check --model <model>
test CPU before CUDA
use CUDA only on CUDA-capable hosts
never commit generated GGUFs or local registry files
```

For daemon checks, start with `--one-request` and a status endpoint before
testing longer-lived processes.

## 15. Future One-Command Shape

Future target shape, not current capability:

```sh
./yvex prepare <model-or-alias> --backend cuda
./yvex chat --model <model-or-alias> --backend cuda
./yvexd --model <model-or-alias> --backend cuda
```

The current runbook stays longer because the implemented surface still exposes
the source/artifact/registry/integrity/materialization/graph/runtime boundaries
separately. The short path belongs after decode, logits, sampling, and runtime
generation exist.

## 16. Current Boundary

Current implemented boundaries:

```text
source tensor inventory and selected conversion planning
selected DeepSeek embedding GGUF emission
alias registry and identity checks
artifact integrity and metadata drift checks
CPU/CUDA selected materialization
engine/session attachment
controlled fixture graph execution
selected embedding partial graph execution
selected embedding-plus-RMSNorm segment execution
standalone RoPE, attention, matmul, and MLP primitive proofs
controlled transformer-block fixture proof
segment-summary prefill state diagnostics
minimal KV ownership and diagnostic binding
daemon status endpoints
accepted-only chat/run diagnostics
```

Current unsupported boundaries:

```text
complete model materialization
complete transformer execution
attention-backed transformer prefill
decode
logits
sampling
generation
interactive generation
provider generation
capability evaluation
benchmark: unsupported
generation: unsupported
external reference only
accepted-only diagnostic path
```
