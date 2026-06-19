# YVEX Logging and Tracing

This document extracts logging, tracing, and profiling rules from
`docs/spine.md`. The spine remains authoritative.

## Domains

```text
fs
loader
gguf
tensor
model
arch
tokenizer
prompt
graph
planner
memory
backend
cpu
cuda
metal
rocm
kv
prefill
decode
sampler
server
openai
yai
```

## Levels

```text
error
warn
info
debug
trace
```

## Flags

```text
--quiet
--verbose
--log-level info
--trace gguf,tensor,backend
--trace all
--trace-prefill
--trace-decode
--trace-kv
--trace-sampler
--trace-out trace.jsonl
--metrics-out metrics.json
--profile-out profile.json
```

## Trace Event

```json
{
  "schema": "yvex.trace.v1",
  "ts_ns": 123456789,
  "run_id": "run_20260619_153001_ab12",
  "domain": "prefill",
  "event": "chunk_done",
  "data": {
    "chunk_index": 1,
    "tokens": 512,
    "ms": 1304.2,
    "tok_s": 392.5
  }
}
```

## Profile

```json
{
  "schema": "yvex.profile.v1",
  "load_ms": 1200.4,
  "mmap_ms": 12.1,
  "gguf_parse_ms": 42.0,
  "tensor_table_ms": 8.4,
  "tokenize_ms": 3.1,
  "prompt_render_ms": 0.8,
  "prefill_ms": 1413.0,
  "decode_ms": 6720.0,
  "sampler_ms": 4.2
}
```

## stdout/stderr

Logs, warnings, progress, and timing go to stderr. Generated text and
machine-readable command output go to stdout.
