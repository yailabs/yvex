# YVEX / YAI Provider Boundary

This document extracts the provider boundary from `docs/spine.md`. The spine
remains authoritative.

## YAI Owns

```text
case
subject
provider boundary
operation attempt
policy gate
control decision
effect boundary
observation boundary
receipt authority
record authority
projection
memory
actor context
tool/action approval
operator authority
workflow authority
case-bound durable state
```

## YVEX Owns

```text
model file open/stat/mmap
format probe
GGUF parser
metadata table
tensor directory
tensor table
dtype/qtype registry
architecture profile
model validation
tokenizer
chat template
prompt rendering
execution graph
memory plan
backend ABI
CPU reference backend
CUDA backend
KV cache
prefill
decode
logits buffer
sampler
token streaming
CLI rendering
local server response
execution-local trace
execution-local metrics
execution-local receipt
```

## Boundary Flow

```text
YAI case/control layer
  -> provider request
  -> YVEX execution
  -> token stream / response / metrics / trace / receipt
  -> YAI provider observation
  -> YAI receipt/record/projection/memory decision
```

## YVEX May Expose

```text
CLI output
JSON output
JSONL stream
OpenAI-compatible HTTP subset
native C API
execution receipt
trace/profile/metrics files
```

## YVEX Must Not

```text
call YAI internally in the initial architecture
mutate YAI case state
decide case authority
approve actions
own actor memory
own tool execution policy
claim YAI integration without command-visible behavior
```

## Receipt Rule

YVEX receipts are execution-local evidence. They become YAI records only if YAI
imports them through its own authority path.
