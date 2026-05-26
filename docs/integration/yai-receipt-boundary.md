# YAI Receipt Boundary

Status: Boundary foundation
Authority: CLORI repository

CLORI emits execution receipts for neural computation. YAI imports and
reconciles receipts according to YAI authority.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## CLORI Receipt Role

CLORI receipts describe what CLORI attempted, executed, measured and returned.
They may include model descriptor refs, backend refs, prompt/workload refs,
token counts, timing, memory estimates, error states and benchmark metadata.

## YAI Receipt Role

YAI decides whether a CLORI receipt is accepted, reconciled, quarantined or
linked to case material. Importing a CLORI receipt must not grant CLORI policy
authority or action approval.

## Boundary

NET may transport receipt chunks and final receipts. NET does not make the
receipt authoritative. YAI does.
