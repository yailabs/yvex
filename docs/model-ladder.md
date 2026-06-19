# YVEX Model Ladder

This document extracts the model ladder from `docs/spine.md`. The spine remains
authoritative.

YVEX uses a ladder, not a jump straight to the largest model.

## M0 - Fixture Models

Purpose:

```text
test parser
test tensor table
test tokenizer mock
test graph executor
test sampler
test run artifacts
```

Goal:

```text
one-token deterministic inference on toy graph
```

## M1 - Small Real GGUF Models

Purpose:

```text
real GGUF parser
real tokenizer
real prompt template
CPU reference path
first real token generation
```

Candidate size:

```text
0.5B to 8B
```

## M2 - Medium Coder/Instruct Models

Purpose:

```text
benchmark practical local use
coding prompt behavior
CUDA early value
longer prompts
tokenization stress
moderate GPU pressure
```

## M3 - Medium/Large MoE Models

Purpose:

```text
routing
expert residency
MoE memory behavior
expert cache
CUDA specialization
```

## M4 - DeepSeek V4 Flash-Class Research Target

Initial tasks:

```text
inspect artifact
parse metadata
classify tensors
classify expert tensors
produce memory plan
estimate KV cost
produce unsupported report
then move toward execution
```

No execution claim exists until commands prove execution.

## M5 - Trillion-Class Research Target

Initial deliverable:

```text
research report only
no execution claim
```

Study:

```text
file size
metadata
tensor count
expert count
active parameter estimate
context memory
KV size
required quantization
possible offload strategy
DGX Spark feasibility
```

## M6 - Future Semi-Frontier Watch

Families:

```text
Qwen
DeepSeek
Kimi
GLM
Llama
Gemma
Mistral
Phi
```

## Support Status Values

```text
watch
documented
inspectable
tokenizable
planned
cpu-reference
cpu-one-token
cuda-partial
cuda-run
server-ready
unsupported
```

## Model Page Template

```text
status:
source:
license:
architecture:
format:
tokenizer:
context:
qtypes:
memory expectation:
YVEX support status:
current blockers:
next experiment:
DGX Spark feasibility:
```

Large model pages must distinguish inspectability, tokenization, planning, CPU
execution, CUDA partial execution, and full run support.
