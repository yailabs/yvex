# Remote management bootstrap v1

This contract is independent of model family, accelerator, hostname and Case.
It applies to a YVEX machine with an OpenSSH server and an Ed25519 host key;
DGX Spark is one qualification fixture, not a type or routing rule. The
OpenAI-compatible listener and YVEX's private Unix socket are **not** remote
management transports.

## Transport and trust

One SSH connection carries one UTF-8 JSON request line on standard input and
one JSON response line on standard output. A separate, supervised OpenSSH
listener may remain available when `yvex serve` is stopped. The only admitted
remote command is `yvex management protocol`, installed as a forced command
for each enrolled client public key. The managed `authorized_keys` file must
be the listener's sole client-key authority; password, keyboard-interactive,
agent forwarding, port forwarding, TTY and user RC must be disabled. The
listener must run as the same OS user as the local YVEX host for `host.status`
to inspect that user's private socket. Do not place unrestricted login keys in
this listener's key file or use the normal login listener as a substitute.

The SSH host Ed25519 public-key blob, not an IP address or a reported model
name, is the device identity. Its canonical form is
`ssh-ed25519:sha256:<64 lowercase hex digits>`. The enrolled client public-key
blob uses the same form for its peer identity. The SSH handshake must verify a
previously approved *exact* host key; a JSON `device_identity` cannot
authenticate itself. `ssh-keyscan` may discover an untrusted candidate key but
does not establish identity. The operator must approve the host key through an
independent channel before pinning it. A known pinned device with no reachable
listener is `known_offline`; an endpoint without a pin is `unknown`; a
different presented host key is `identity_changed` and must fail closed.
These are client-side trust states, not claims inferred from an unauthenticated
network scan.

A discovery candidate may be represented by a client as
`yvex.management.discovery.v1` with `transport="ssh"`, `address`, `port`,
`host_key_algorithm="ssh-ed25519"`, `host_public_key` (the exact SSH base64
blob) and derived `candidate_identity`. This object is **untrusted** until an
independent operator-approved pin matches the key observed by the SSH
handshake. It contains no client credential or enrollment secret. YVEX does
not advertise a machine through the inference listener.

OpenSSH's [server configuration](https://man.openbsd.org/sshd_config),
[authorized-key restrictions](https://man.openbsd.org/sshd), and
[key-scan warning](https://man.openbsd.org/ssh-keyscan) define the transport
mechanics. YVEX owns the typed application message and local peer-enrollment
file; it does not implement a second SSH or TLS stack.

## Local enrollment and revocation

An operator on the managed machine explicitly approves the exact peer-key
identity. These local commands do not run over the management protocol:

```text
yvex management identity HOST_ED25519_PUBLIC_KEY
yvex management identity PEER_ED25519_PUBLIC_KEY
yvex management trust-init ABSOLUTE_PRIVATE_AUTHORIZED_KEYS_PATH
yvex management enroll PEER_ED25519_PUBLIC_KEY ABSOLUTE_PRIVATE_AUTHORIZED_KEYS_PATH HOST_ED25519_PUBLIC_KEY EXPECTED_PEER_HEX
yvex management revoke PEER_HEX ABSOLUTE_PRIVATE_AUTHORIZED_KEYS_PATH
```

`enroll` refuses a mismatch with `EXPECTED_PEER_HEX`. The trust file is
created with mode 0600 under an owner-controlled, non-group-writable directory;
YVEX updates it atomically under a local lock. Each entry has OpenSSH
`restrict,command="..."` options and binds the exact enrolled peer identity
to the forced protocol command. Removing an entry revokes subsequent SSH
authentication. The forced command also rechecks the current managed trust
snapshot on each request, so a previously authenticated multiplexed SSH
transport cannot issue a new authorized request after revocation. A new key
is a new peer; a changed host key is a new device
identity, not an automatic rotation. Neither file paths nor IP addresses are
part of the device identity. Protect the management OS account and SSH host
private key as privileged deployment assets.

Use a dedicated OpenSSH listener/port with `AuthorizedKeysFile` set only to
this YVEX-managed file, `PubkeyAuthentication yes`,
`AuthenticationMethods publickey`, `PasswordAuthentication no`,
`KbdInteractiveAuthentication no`, `PermitTTY no`, `DisableForwarding yes`,
`PermitUserRC no`, `PermitUserEnvironment no`, `StrictModes yes`, and an
explicit allowed account. The forced command is in each key entry. The
operator must verify the effective `sshd -T` configuration and keep the
bootstrap listener supervised independently of `yvex serve`. This repository
does not silently install or reconfigure the system SSH service.

A dedicated listener configuration has this shape (replace every placeholder
with a local absolute path or selected network address):

```text
Port MANAGEMENT_PORT
ListenAddress MANAGEMENT_ADDRESS
HostKey HOST_ED25519_PRIVATE_KEY
AuthorizedKeysFile ABSOLUTE_PRIVATE_AUTHORIZED_KEYS_PATH
AllowUsers YVEX_HOST_ACCOUNT
HostKeyAlgorithms ssh-ed25519
PubkeyAcceptedAlgorithms ssh-ed25519
PubkeyAuthentication yes
AuthenticationMethods publickey
PasswordAuthentication no
KbdInteractiveAuthentication no
GSSAPIAuthentication no
UsePAM no
StrictModes yes
PermitTTY no
DisableForwarding yes
PermitUserRC no
PermitUserEnvironment no
MaxSessions 1
SetEnv XDG_RUNTIME_DIR=HOST_RUNTIME_DIRECTORY
```

Validate with `sshd -t -f CONFIG` and inspect the effective settings with
`sshd -T -f CONFIG`; supervise `sshd -D -f CONFIG` separately from the
inference host. `HOST_RUNTIME_DIRECTORY` must resolve to the same private
socket namespace used by that user's `yvex serve`. The test uses isolated
non-privileged listener and runtime directories; no system SSH config is
changed by the fixture.

## Wire contract

Request schema: `yvex.management.request.v1`. Response schema:
`yvex.management.response.v1`. A request is an object with exactly:

```json
{"schema":"yvex.management.request.v1","request_id":"<64-hex>","operation":"device.describe"}
```

`request_id` is an opaque correlation identity. Version 1 exposes only
read-only operations:

```text
ssh -T -o BatchMode=yes -o StrictHostKeyChecking=yes \
  -o UserKnownHostsFile=PINNED_KNOWN_HOSTS \
  -o IdentitiesOnly=yes -i ENROLLED_CLIENT_KEY \
  -p MANAGEMENT_PORT MANAGEMENT_USER@MACHINE < request.json
```

No remote shell command follows the address: the server's enrolled-key entry
selects the forced YVEX protocol command. The client checks the SSH host key,
then checks `device_identity` and `authenticated_peer` against its pins.

| Operation | Result | Host required? |
| --- | --- | --- |
| `device.describe` | `device_identity`, `authenticated_peer` | No |
| `host.status` | `device_identity`, `host_state`; when running, typed engine/session counts | No |

`host_state` is `running`, `stopped` (no private socket), or `unavailable`
(socket exists but current local protocol cannot be read). Malformed requests
return `status=refused`, `reason=malformed_request`, `request_id=null`.
Unknown operations return `reason=unsupported_operation`. SSH authentication
and host-key failures happen before the YVEX wire protocol and have no JSON
response. A revoked peer on an already-authenticated transport receives
`reason=peer_revoked_or_authority_unavailable` without host facts. Read-only
request replay is side-effect-free; the same request ID
does not imply a durable operation receipt.

No remote `start`, `stop`, model catalog, load/unload, deployment identity,
memory, telemetry or logs operation is implemented by this first slice.
Those future operations require an identity-bound durable operation journal,
idempotent retry after reply loss, exact lifecycle delegation to current YVEX
owners and bounded refusal/observation contracts. A request for one now is
explicitly refused as unsupported; no arbitrary shell/SSH command or local
socket forwarding is a fallback. No Case identity enters YVEX.

## Conformance and handoff

`tests/integration/remote_management.sh` starts an isolated OpenSSH listener
with a disposable host key and two enrolled disposable client keys. It checks
host-off/running/stopped status without restarting SSH, both client identities,
repeatable read-only
requests, unknown/revoked peers, changed pinned host key, malformed request,
revocation over an already-authenticated SSH multiplexed transport,
arbitrary-command refusal and bootstrap restart. With
`YVEX_MANAGEMENT_TEST_REMOTE` and `YVEX_MANAGEMENT_TEST_ADDRESS` it also
executes an actual second-machine client against the test listener. The test
prints the temporary address and device identity; its listener and keys are
removed at completion, so that address is **not** a persistent YAI endpoint.

YAI can consume this v1 identity/pairing/status contract after independently
approving and pinning a deployment host key. It cannot yet govern model
operations or infer that an arbitrary machine is remote-ready. Production
qualification still requires a supervised persistent listener, effective
security-configuration audit, real YAI enrollment, operation journal and
mutating lifecycle conformance. The broad remote transport, authentication and
release rows in `ROADMAP.md` are not promoted by the isolated fixture.
