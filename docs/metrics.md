# YVEX Metrics

This document extracts the metrics catalogue from `docs/spine.md`. The spine
remains authoritative.

## Load

```text
file_open_ms
mmap_ms
format_probe_ms
gguf_header_ms
metadata_parse_ms
tensor_dir_ms
tensor_table_ms
arch_detect_ms
model_validate_ms
backend_init_ms
total_load_ms
```

## Tensor

```text
tensor_count
tensor_bytes_total
tensor_count_by_dtype
tensor_bytes_by_dtype
tensor_count_by_role
tensor_bytes_by_role
largest_tensor_name
largest_tensor_bytes
unsupported_tensor_count
```

## Memory

```text
host_rss_bytes
host_available_bytes
gpu_total_bytes
gpu_free_bytes
gpu_used_bytes
weights_mapped_bytes
weights_resident_bytes
kv_allocated_bytes
kv_used_bytes
scratch_allocated_bytes
peak_memory_bytes
```

## Prefill

```text
prompt_tokens
cached_tokens
suffix_tokens
prefill_tokens
prefill_chunks
prefill_chunk_size
prefill_ms
prefill_tps
prefill_gpu_ms
prefill_cpu_ms
kv_write_bytes
backend_sync_count
```

## Decode

```text
ttft_ms
decode_tokens
decode_ms
decode_tps
latency_p50_ms
latency_p90_ms
latency_p99_ms
sampler_ms_total
sampler_ms_avg
logits_ms_avg
token_eval_ms_avg
```

## Sampling

```text
temperature
top_k
top_p
min_p
seed
greedy
selected_token_id
selected_token_text
selected_token_logprob
top_logprobs
```

## KV

```text
kv_dtype
kv_bytes_per_token
kv_total_bytes
kv_used_bytes
kv_blocks
kv_block_size
kv_rewind_count
kv_prefix_reuse_tokens
kv_gpu_bytes
kv_host_bytes
```

## MoE

```text
router_ms
experts_total
experts_active
experts_selected_per_token
expert_cache_hit_rate
expert_cache_miss_count
expert_load_ms
expert_load_bytes
moe_ms
```

## Server

```text
requests_total
requests_active
queue_ms
time_to_first_token_ms
stream_tokens_total
errors_total
cancelled_total
```

Metrics become support evidence only when produced by real commands and stored
in run artifacts.
