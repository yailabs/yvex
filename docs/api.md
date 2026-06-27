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
  artifact.h artifact_identity.h artifact_integrity.h artifact_naming.h
  gguf.h gguf_emit.h gguf_template.h

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
| Deterministic fixture graph execution | implemented for controlled fixtures |
| Real selected embedding partial graph | implemented for `F16` `token_embd.weight` |
| Artifact integrity baseline | implemented for `GGUF` structural/range checks |
| Local artifact identity | implemented with file size and SHA-256 digest |
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

`yvex_engine_execute_partial_graph` borrows engine-attached weights owned by
`yvex_engine` and returns a copied output summary only. The executor does not
expose backend pointers, does not transfer ownership to sessions, and keeps
`execution_ready` and broad graph readiness false. The implemented partial graph
boundary is the selected token-embedding segment only; it is not prefill, decode,
logits, sampling, generation, or full model execution.

`yvex_artifact_identity_read` and `yvex_artifact_compute_sha256` provide local
file identity evidence: current byte size plus lowercase SHA-256 digest. The
hashing path streams the file and does not read large model artifacts into
memory. A digest match proves only that the bytes match a recorded or expected
local value; it does not prove author identity, remote provenance, malware
absence, or supply-chain trust.

`yvex_artifact_integrity_check_path` opens a local artifact path and returns a
caller-owned `yvex_artifact_integrity_report`. `yvex_artifact_integrity_validate`
borrows an already opened artifact, parsed `GGUF`, and tensor table; it does not
take ownership of those objects. The report contains copied summary fields,
local identity/digest status for path checks, and a bounded copied issue list.
The integrity API checks structural `GGUF` bounds, tensor range math, selected
embedding readiness, and optional expected/registered SHA-256 matching; it is
not a supply-chain security, malware, sandboxing, or remote provenance API.
The repository test suite includes tiny corrupt `GGUF` fixtures that exercise
the bounded issue list and parser-to-integrity error mapping; that corpus is
structural regression coverage, not fuzzing.

`yvex_tensor_range_validate` is the canonical byte-range calculation for a
parsed tensor directory row. It computes element count, dtype size, tensor byte
count, `tensor_data_offset + tensor_relative_offset`, end offset, file bounds,
and alignment status before payload-reading paths use tensor bytes.
`yvex_tensor_shape_accounting_validate` is the canonical shape/dtype accounting
step that feeds that range calculation. It reports rank, dims, element count,
storage byte accounting, storage support, and the narrow compute-support flags
for the controlled `F32` fixture embedding path and the selected `F16`
embedding path. Storage accounting support is not full runtime compute support.
`yvex_selected_embedding_shape_validate` interprets `token_embd.weight` as
`dims[0] = hidden_size` and `dims[1] = vocab_size`, validates the token id, and
reports output count, output bytes, and selected-token slice bytes for the real
partial graph path.
`yvex_tensor_embedding_slice_range_validate` narrows that validated tensor range
to the selected embedding token slice used by the real partial graph path. These
helpers do not prove tensor values are correct; they prove the byte ranges are
bounded before reads, materialization, or graph reference extraction.
