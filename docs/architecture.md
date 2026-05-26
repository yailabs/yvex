# Architecture

CLORI is organized around model artifacts, normalized descriptors, tensor
metadata, backend contracts, execution IR, memory/KV accounting, decode surfaces
and metrics.

The first implementation target is GGUF inspection and local execution
benchmarking, but the architecture must remain open to larger models and
non-LLM neural outputs.

No inference implementation exists in this bootstrap delivery.
