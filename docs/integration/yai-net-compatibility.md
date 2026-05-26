# YAI NET Compatibility

Status: Boundary foundation
Authority: CLORI repository

CLORI can later integrate with YAI through NET-compatible node contracts. This
repository does not implement NET integration in its bootstrap state.

The authoritative NET spine lives in the YAI repository at
`docs/spines/net-spine.md`. CLORI keeps only a copied compatibility reference at
`docs/spines/yai-net-spine-reference.md`.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Planned Compatibility Surface

A future CLORI NET-compatible node may:

- advertise neural capabilities in a NET-compatible shape
- accept invocation envelopes defined by NET compatibility work
- stream chunks, metrics, errors and receipts through NET-compatible envelopes
- report health, readiness and liveness in a shape NET can consume
- expose benchmark and receipt material without becoming a YAI authority source

## Non-Authority Rule

CLORI can execute neural computation only after YAI has authorized an
invocation. CLORI receipts may be imported by YAI, but they do not become YAI
policy, memory, graph, fact or journal authority by themselves.

CLORI does not perform NET discovery, NET routing or NET trust decisions.

No NET integration is implemented in this bootstrap delivery.
No inference implementation exists in this bootstrap delivery.
