# CLORI

CLORI is a standalone Neural Execution Engine.

CLORI is not YAI. It can be used independently by local model users, and it can
also run as a NET-compatible node for YAI.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Status

This repository is a skeleton and documentation bootstrap.

No inference implementation exists in this bootstrap delivery. No server, LAN
discovery, runtime transport or model execution path is implemented here.

## First Target

The first practical target is local LLM/GGUF execution and benchmarking. The
design must not be limited to small models: CLORI is shaped around model
artifacts, descriptors, tensor tables, memory/KV accounting, backend
abstractions, metrics and benchmark receipts.

Future targets may include embeddings, rerankers, classifiers, vision encoders,
audio encoders and larger models.

## Repository Role

CLORI owns neural execution concerns. YAI owns authority. NET owns the runtime
communication substrate in YAI. `interfaces/transports` owns contract vocabulary
and schemas.

Intended canonical remote: `https://github.com/yailabs/clori`.

Temporary fallback remote, if organization access is unavailable:
`https://github.com/francescomaiomascio/clori`.
