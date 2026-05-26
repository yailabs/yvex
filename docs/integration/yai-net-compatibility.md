# YAI NET Compatibility

Status: Boundary foundation
Authority: CLORI repository

CLORI integrates with YAI through NET-compatible node contracts. The
authoritative NET spine lives in the YAI repository at
`docs/spines/net-spine.md`; CLORI keeps only a copied compatibility reference at
`docs/spines/yai-net-spine-reference.md`.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Compatibility Commitments

- CLORI advertises neural capabilities through a NET-compatible node adapter.
- CLORI accepts invocation envelopes defined by NET compatibility work.
- CLORI streams token, metric, error and receipt outputs through NET-compatible
  envelopes.
- CLORI reports health/readiness/liveness in a shape NET can consume.
- CLORI does not perform NET discovery, NET routing or NET trust decisions.

## Non-Authority Rule

CLORI can execute neural computation only after YAI has authorized an
invocation. CLORI receipts may be imported by YAI, but they do not become YAI
policy, memory, graph, fact or journal authority by themselves.
