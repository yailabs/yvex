# Architecture

CLORI is a standalone Neural Execution Engine. It is separate from YAI because
neural execution and YAI authority are different responsibilities.

CLORI may still become NET-compatible later: YAI controls authority, NET moves
streams, and CLORI executes neural computation.

Runtime control and measurement matter because model execution needs visible
artifact metadata, descriptors, tensor layout, memory pressure, KV behavior,
decode surfaces, metrics and receipts.

Small local models are the first practical target. They are not an architectural
limit.
