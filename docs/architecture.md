# Architecture

CLORI is a standalone Neural Execution Engine. It is not a chatbot framework, a
model provider wrapper or a YAI subsystem.

The engine is designed around a runtime evidence pipeline:

```text
artifact -> descriptor -> tensor map -> runtime plan -> backend -> decode -> metrics -> receipt
```

The first implementation direction is local model execution foundations:
artifact inspection, normalized descriptors, tensor layout, memory and KV
accounting, backend boundaries, decode surfaces, metrics and receipts.

Small local models are the first practical target. They are not an architectural
limit. Larger models and additional neural workloads remain design targets, not
current support claims.

CLORI is separate from YAI because neural execution and external authority are
different responsibilities. CLORI may later become NET-compatible, but NET
compatibility is not implemented in this scaffold.
