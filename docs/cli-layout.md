# YVEX CLI Layout

This document extracts CLI layout examples and output envelope rules from
`docs/spine.md`. The spine remains authoritative.

## Rich One-Shot Run

```text
YVEX 0.1.0 | backend: cuda | device: NVIDIA GB10 | model: qwen

LOAD
  file        models/qwen.gguf
  size        42.1 GiB
  mmap        yes
  format      GGUF v3
  tensors     812
  tokenizer   loaded
  arch        qwen

MEMORY PLAN
  weights     42.1 GiB mmap
  kv cache    2.4 GiB planned
  scratch     768 MiB planned
  backend     cuda
  residency   weights=mmap, kv=gpu, scratch=gpu

PREFILL
  tokens      1024
  chunks      2 x 512
  speed       382 tok/s
  elapsed     2.68s

ASSISTANT
  ...
```

## Chat Layout

```text
YVEX chat
model   qwen3-coder-next-q4.gguf
backend cuda / NVIDIA GB10 / sm_121
ctx     32768
mode    greedy
trace   off

yvex> Explain mmap in C.

PREFILL
  cached       512 tokens
  suffix       83 tokens
  chunk        1/1
  speed        421 tok/s
  elapsed      0.20s

assistant>
mmap maps a file into virtual memory...

[decode] 47 tok | 18.9 tok/s | last 53ms | ctx 642/32768
```

## Status Lines

Prefill:

```text
[prefill] chunk 2/4 | 1024/2048 tok | 384 tok/s | kv 1.2 GiB | elapsed 2.6s
```

Decode:

```text
[decode] 87 tok | 18.7 tok/s | last 52ms | p50 51ms | ctx 1192/32768
```

Rules:

```text
status line writes to stderr
status line uses carriage return only on TTY or explicit always mode
final status line is followed by newline before exit
status line never writes to stdout
```

## JSON Output Envelope

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "inspect",
  "status": "ok",
  "data": {},
  "error": null
}
```

Error:

```json
{
  "schema": "yvex.cli.result.v1",
  "command": "inspect",
  "status": "error",
  "data": null,
  "error": {
    "code": "YVEX_ERR_FORMAT",
    "where": "yvex_gguf_open",
    "message": "bad GGUF magic at offset 0"
  }
}
```

## JSONL Stream

```jsonl
{"schema":"yvex.event.v1","event":"prefill_start","run_id":"run_...","ts_ns":0,"data":{"tokens":123}}
{"schema":"yvex.event.v1","event":"prefill_done","run_id":"run_...","ts_ns":0,"data":{"ms":292.0,"tok_s":421.0}}
{"schema":"yvex.event.v1","event":"token","run_id":"run_...","ts_ns":0,"data":{"index":0,"id":9906,"text":"Hello","latency_ms":52.1}}
{"schema":"yvex.event.v1","event":"done","run_id":"run_...","ts_ns":0,"data":{"generated":64,"decode_tps":19.1}}
```

JSONL output must remain one valid JSON object per line.
