# YVEX Failure Taxonomy

This document extracts the failure taxonomy from `docs/spine.md`. The spine
remains authoritative.

## Failure Classes

```text
filesystem
artifact
format
metadata
tensor
dtype/qtype
architecture
tokenizer
prompt
graph
memory plan
backend init
backend memory
kernel
KV
prefill
decode
sampler
server
provider protocol
user interrupt
```

## Error Shape

Every serious failure includes:

```text
status code
where
human message
optional tensor name
optional backend
optional model path
optional run id
```

## Bad Errors

```text
invalid model
unsupported tensor
parse error
```

## Good Errors

```text
GGUF metadata value for key tokenizer.chat_template has unsupported type array at offset X
tensor blk.17.attn_q.weight offset exceeds file bounds
tensor data base is not aligned to expected boundary
unsupported qtype IQ2_XXS for backend cpu
```

## Parser Failures

```text
gguf.bad_magic
gguf.unsupported_version
gguf.truncated_header
gguf.truncated_metadata
gguf.unsupported_metadata_type
gguf.metadata_count_overflow
gguf.tensor_count_overflow
gguf.string_out_of_bounds
gguf.array_out_of_bounds
gguf.array_depth_unsupported
gguf.tensor_rank_unsupported
gguf.tensor_shape_overflow
gguf.tensor_offset_overflow
gguf.tensor_out_of_bounds
gguf.tensor_alignment
gguf.qtype_unsupported
```

## Backend Failures

```text
allocation_failed
copy_failed
op_unsupported
op_failed
sync_failed
device_lost
invalid_tensor
```

Failures must name the exact missing or failed capability.
