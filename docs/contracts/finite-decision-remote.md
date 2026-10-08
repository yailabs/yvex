<!-- docs:metadata
title: Remote finite-decision producer v1
id: yvex.contracts.finite-decision-remote
document: reference
status: current
owner: interfaces
audience: [developer, engineer, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Remote finite-decision producer v1

[Contracts](README.md) · [Public C producer](c-api.md) · [SSH trust](remote-management.md)

Machine layouts: [request schema](schema/finite-request-v1.schema.json) and
[response schema](schema/finite-response-v1.schema.json). Runtime byte bounds,
unique candidate IDs and identity matching supplement JSON Schema validation.

This is a separately versioned public computation boundary, not an extension
of management request v1, OpenAI chat, or the private local protocol. One
`yvex management finite-protocol` forced process uses the installed public
`yvex_finite_producer_execute_local` C client against the same-user resident
host. Model input, execution, cancellation and identity remain C-owned.
The remote consumer does not encode or forward the private Unix wire.

## Transport, trust and explicit grant

Use the restricted dedicated OpenSSH listener configuration in
[remote management](remote-management.md#transport-and-trust): exact approved
Ed25519 host pin, an enrolled client key, no shell, TTY, forwarding or user RC.
SSH encrypts the question/context and authenticates both peers. A self-reported
JSON device identity is not a trust anchor. Do not use `ssh-keyscan` as approval.

Enrollment is local and explicitly selects computation:

```sh
yvex management enroll PEER_PUBLIC_KEY TRUST_FILE HOST_PUBLIC_KEY PEER_HEX \
  --scope finite-decision
```

Prefer a dedicated finite-compute trust file/listener. Omission of `--scope`
retains management-only enrollment. Existing management peers do not acquire
compute permission. The forced entry selects `management finite-protocol`
and rechecks the current trust snapshot and grant on every invocation;
revocation applies to subsequent invocations, including multiplexed SSH.
Revocation does not revoke another already-admitted computation retroactively.
No remote load/unload, host stop/start, model installation or Case action is
authorized. The model must already have an admitted finite engine/input policy.

## Request

One connection carries one UTF-8 JSON line, at most 32,768 bytes including LF.
Objects reject unknown or duplicate fields. `request_id` is 64 lowercase hex
characters, opaque correlation only. The exact schema and operation are:

```json
{"schema":"yvex.finite.request.v1","request_id":"1111111111111111111111111111111111111111111111111111111111111111","operation":"finite.decision.execute","input":{"model_alias":"ADMITTED_ALIAS","expected_generation":1,"question":"Choose one candidate","context":"","candidates":[{"id":"opaque-a","text":"first"},{"id":"opaque-b","text":"second"}]}}
```

The `input` is the public C producer's semantic-neutral contract: nonempty
model alias, nonzero expected generation, bounded question/context, and one to
eight ordered candidates with unique nonempty opaque IDs and nonempty text.
Bounds are UTF-8 bytes excluding the terminator: alias/ID/text less than 128,
question less than 256, context less than 512; NUL is refused, never truncated.
No Case ID, tokenizer IDs, markers, templates or family selector is accepted.
Generation is process-local, not a durable global model identity.

```sh
ssh -T -F /dev/null -o BatchMode=yes -o IdentitiesOnly=yes \
  -o StrictHostKeyChecking=yes -o UserKnownHostsFile=APPROVED_PINS \
  -i ENROLLED_KEY -p FINITE_PORT USER@DGX < request.json
```

Supply no remote command. The enrolled forced command chooses the protocol.
The host account's `XDG_RUNTIME_DIR` must identify the normal host socket;
the remote caller cannot select a socket or filesystem path.

## Response and exact result

Successful JSON has `schema="yvex.finite.response.v1"`, echoed `request_id`,
`operation`, `model_alias`, authenticated `device_identity` and
`authenticated_peer` (`ssh-ed25519:sha256:<64-hex>`), `status="ok"`,
`dispatch_state="completed"`, and one `result` object. No human preamble or ANSI.

`result` preserves the public C result fields:

| Fields | Meaning |
| --- | --- |
| `schema_version=1`, `score_kind="model-logit"`, `engine_generation` | Exact finite producer/result class and resident generation |
| `source_identity`, `logical_model_identity`, `binding_identity`, `tokenizer_identity`, `physical_program_identity`, `input_policy_identity` | Computational/model lineage; every identity is 64 lowercase hex |
| `input_identity`, `candidate_population_identity`, `result_identity` | Exact input/population/result identities, not correlation ID |
| `candidates[]` | Original ordered `id`, finite `raw_score`, `relative_candidate_probability` |
| `calibrated=false` | Relative distribution over this population only; not calibrated confidence or a semantic decision |
| `token_count`, `candidate_count`, `model_forward_count`, `sampling_invocation_count`, `generated_token_count`, `resident_backbone_count` | Exact bounded work: one forward/backbone, zero sampling/generation |
| `elapsed_nanoseconds`, `source_mapped_bytes`, `parameter_execution_bytes`, `workspace_host_bytes`, `workspace_device_bytes` | Producer-authored time/resource facts; do not add overlapping byte classes |

`elapsed_nanoseconds` measures the tensor-engine forward, not the complete
caller operation. Input admission/tokenization, candidate scoring/result sealing
and transport are outside that clock. A caller budget must be measured from
client dispatch through validated response, independently of this compute fact.

Consumers must verify peer pins, schema, correlation, alias, generation, ordered
candidate IDs and the exact model/binding/input-policy identities admitted by
their configuration. Alias/generation alone cannot authenticate a model across
host replacement. The native client verifies generation and candidate population
before publication. A result must not be substituted for a different checkpoint
or promoted into Case authority, calibration, model quality or Fast Search
qualification.

## Refusal, loss and lifecycle

Before dispatch, malformed/schema/bounds/population or unknown operation errors
return `status="refused"`, `dispatch_state="not_dispatched"`, `request_id=null`
and `reason="malformed_request"` or `unsupported_operation`. Failed grant/trust
also refuses without execution. SSH authentication/pin failure may produce no
JSON at all. Trust/read failures are not model results.

A native error returns `status="error"`, `dispatch_state="outcome_unavailable"`,
`reason="producer_error"`, correlation/peer/alias fields, and `error` containing
numeric `code`, typed `name` and native `owner`, never question/context-bearing
messages or partial scores. This conservatively does not claim execution never
started. Success alone carries `result`.

There is no automatic retry, fallback, generation substitution, result cache or
durable remote receipt. Reusing `request_id` does not deduplicate work. Loss after
dispatch is an unknown computational outcome; caller policy controls any new
independent invocation. No durable Case effect is executed by this producer.
Process termination closes the native connection; host-owned finite execution
and lease retirement remain authoritative. Transport loss does not promise
immediate cancellation of an already dispatched computation. Host lifecycle is
not owned by the SSH process. The public C client bounds the handshake to 30 s
and then clears that socket timeout; this surface adds no total-computation
deadline. Consumers must separately bound their SSH call and preserve unknown
outcome semantics on loss.

## Consumer handoff and evidence scope

The independently buildable public Rust `yvex-sdk` supports
`finite::remote::{RemoteClient, ProducerIdentity, Invocation}` over an explicitly
approved `yvex_sdk::SshConnection`. The invocation binds alias, expected
generation, question/context and ordered candidates; the client validates schema,
correlation, server/peer and exact producer lineage before returning a result.
Use `with_timeout(Duration::from_millis(5000))` for a 5,000 ms caller budget;
timeout is outcome-unavailable, not permission to retry. The client's complete
response receive bound is 32,768 bytes, stricter than a 65,536-byte consumer bound.
`finite::execute_local` remains the separate public-C local integration.
YAI retains Participant/Case admission and disclosure. SDK validates/projects
producer facts; neither commercial access nor computational success grants Case
authority. HTTPS management is not this finite-compute endpoint. No SDK, YAI or
Studio changes are required to consume a compatible faster resident realization.

`tests/integration/finite_remote.py` exercises the real restricted SSH producer
and its public C client, with a synthetic native peer for wire/result negatives.
That fixture is not real model quality or Exon→DGX product qualification.
Management v1, private protocol v25 and installed C ABI v1 retain their identities.
Actual installed listener/key approval and real model/Exon integration remain
separate qualification facts, never inferred from a loopback fixture.
