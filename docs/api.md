# YVEX API

This file describes the public C API surface. It is a compact map of stable
headers and capability boundaries, not an implementation diary.

## 1. Public Header Surface

Use the umbrella header for consumers that want the implemented public surface:

```c
#include <yvex/yvex.h>
```

Header groups:

```text
core:
  version.h status.h error.h log.h

artifact and GGUF:
  artifact.h artifact_naming.h gguf.h gguf_emit.h gguf_template.h

model and tensors:
  dtype.h tensor.h model.h weights.h

tokenizer and prompt:
  tokenizer.h prompt.h

graph and planning:
  graph.h op.h planner.h memory_plan.h

backend:
  backend.h

runtime/session:
  engine.h session.h kv.h logits.h

metrics and traces:
  metrics.h trace.h profile.h

server:
  server.h

tool APIs:
  source_manifest.h native_weights.h conversion.h weight_mapping.h
  qtype_support.h quant_policy.h quant_job.h imatrix.h
  model_registry.h model_ref.h model_gate.h materialize_gate.h

filesystem:
  fs.h
```

## 2. Error Model

Public functions return integer status codes. `YVEX_OK` means success.
Failures are reported as explicit error/status values and, where supported,
write details into `yvex_error`.

Rules:

```text
invalid arguments fail explicitly
missing files fail explicitly
parse failures fail explicitly
unsupported dtype/qtype fails explicitly
backend availability/allocation/copy failures remain distinguishable
```

## 3. Ownership Rules

```text
public option structs borrow input pointers
returned opaque handles must be closed by the matching close/free function
registry and resolver objects own copied strings until cleared/closed
backend tensors are released through backend/weight table close paths
engine-attached weights are released by engine close
sessions observe engine attachment state and do not own engine weights
model artifacts remain outside the repository
```

## 4. Capability Status

| Area | Status |
| --- | --- |
| GGUF metadata/tensor parse | implemented |
| Tensor/model descriptor | implemented |
| Tokenizer fixture path | implemented |
| Prompt rendering diagnostics | implemented |
| Graph/planner substrate | implemented |
| CPU backend | implemented |
| CUDA tensor movement/parity subset | implemented when CUDA is available |
| Selected tensor materialization | implemented |
| Engine-owned selected weight attachment | implemented |
| Local model registry | implemented |
| Alias-or-path model resolver | implemented for one-shot commands |
| Model gate | implemented |
| Materialization gate | implemented |
| Server/provider status shell | implemented |
| KV cache runtime | unavailable skeleton |
| Logits runtime | unavailable skeleton |
| Full model execution | unsupported |
| Prefill/decode | unsupported |
| Generation | unsupported |
| Benchmarks | unsupported |

## 5. Public API Boundary

Public headers expose only implemented contracts or explicit unavailable
skeletons. Do not add future APIs to public headers before there is backing
implementation, tests, command-visible behavior where applicable, and documented
failure behavior.

The C API does not promise inference, generation, OpenAI-compatible generation,
or benchmark performance.
