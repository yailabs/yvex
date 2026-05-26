# Architecture

CLORI is a standalone Neural Execution Engine organized around transparent
model-runtime evidence.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Shape

The planned architecture separates:

- artifact inspection
- normalized model descriptors
- tensor tables
- quantization profiles
- memory and KV cache accounting
- execution IR
- backend adapter boundaries
- prefill and decode surfaces
- streaming surfaces
- metrics and receipts
- benchmark reports

The first practical target is local LLM/GGUF inspection, benchmarking and
execution, but that target does not imply implemented support in the current
repository state.

## Scale Constraint

CLORI must not be designed only for small models. The architecture should scale
from small local models to larger models and future neural workloads such as
embeddings, rerankers, classifiers, vision encoders and audio encoders.

## Current State

No inference implementation exists in this bootstrap delivery. No serving,
runtime transport, NET adapter or benchmark execution path is implemented.
