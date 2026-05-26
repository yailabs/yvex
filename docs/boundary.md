# CLORI Boundary

CLORI is a standalone Neural Execution Engine, not YAI.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## CLORI Owns

- model artifact inspection
- model descriptors
- tensor tables
- quant profiles
- architecture registry
- family profiles
- execution IR
- backend abstraction
- memory accounting
- KV cache accounting
- decoding and sampler policy
- neural output generation
- runtime metrics
- benchmark receipts
- standalone CLI/server posture
- NET-compatible node adapter posture

## CLORI Does Not Own

- YAI case authority
- YAI policy
- YAI memory
- YAI graph
- YAI facts
- YAI journal
- YAI carrier authority
- YAI action approval
- NET discovery
- NET routing
- NET trust authority

## Public Posture

CLORI can be useful without YAI. It should serve local model users who want to
inspect and measure model-runtime behavior directly.

CLORI can later integrate with YAI through NET-compatible node contracts. That
does not make CLORI a YAI authority source.

No inference implementation exists in this bootstrap delivery.
