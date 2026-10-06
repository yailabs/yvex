<!-- docs:metadata
title: Native network management
id: yvex.contracts.network-management
document: reference
status: mixed
owner: interfaces
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Native network management

[Up](README.md) · [Product operations](product-management.md)

## Selected boundary

The selected Studio compute-control-plane program includes a standalone YVEX
management service, explicit remote pairing, local automatic same-user discovery
and optional LAN discovery. Local native and isolated cross-machine HTTPS paths
are qualified. The [operator rollout](../evaluation/product-management-control-plane.md#operator-spark-lan-rollout)
also establishes deployed HTTPS identity and physical-LAN discovery, not complete
runtime administration or an enrolled Studio consumer.

The single `yvex` executable owns this service independently of Studio and YAI.
A persistent management service can observe a stopped or zero-engine computational
Host. Starting management never starts, replaces or stops a computational Host.
The existing 36 versioned product operations and durable receipt owner remain
unchanged. No new YAI ProviderTarget, trust, Principal, Case or inference grant is
created by management enrollment. SSH read/finite/product grants retain their
existing meaning and remain an advanced compatibility transport.

The management service and computational Host must use the same admitted private
protocol for runtime operations. A reachable management listener does not make
an older Host compatible: protocol 25's Host and Session lifetime fences cannot
be synthesized for protocol 24. With an incompatible preserved Host, `host.get`
reports `unavailable`, engine/session operations refuse, and model catalog reads
retain `runtime_observation: unavailable`. This is not a stopped or zero-engine
Host claim. Upgrade the Host only through a coordinated supported lifecycle;
management startup never performs that upgrade implicitly.

## HTTPS identity and discovery

`yvex management serve` defaults to loopback `127.0.0.1:18080`. Remote use requires
an explicit IP/port bind. An optional `--advertise` flag publishes the service as
`_yvex-management._tcp.local.` using mDNS. Advertised addresses, labels, version
and certificate fingerprints are discovery hints only; discovery never enrolls
or trusts a service. Inference addresses are not management identity.

The service creates an owner-only persistent TLS certificate/private key pair in
its private native service directory. Rustls owns TLS and rcgen certificate
creation; Studio never creates producer keys. Identity is
`tls-sha256:<lowercase SHA-256 of certificate DER>`. It is independent of address,
port, computational Host instance nonce, package and Engine generation. Clients
inspect public identity and require explicit operator trust of the fingerprint
before sending any pairing credential or authenticated management request.
A changed certificate fails closed; no automatic replacement or downgrade.

The private service directory defaults to
`$XDG_CONFIG_HOME/yvex/management-network-v1`, or
`$HOME/.config/yvex/management-network-v1` without an absolute XDG config root.
`--state-dir` is an explicit local service option for isolated installations and
tests. The directory is owner-only 0700 and regular identity/ledger files 0600.
Unsafe permissions, symlinks, malformed or partially missing identity refuse;
they never trigger silent key regeneration. Secrets are never printed in status,
ordinary logs, public identity, discovery records, receipts or CLI arguments.

## Public HTTP records

All endpoints use HTTP/1.1 over TLS, one request per connection, JSON responses,
no redirects, no cookies and no CORS. Origin and Transfer-Encoding headers refuse.
Duplicate Authorization, Host or Content-Length fields refuse. Requests have
bounded headers/body and an absolute handshake/read deadline; slow clients cannot
reset the deadline by sending individual bytes. Connections and worker count are
bounded before TLS admission. The product body remains bounded to 128 KiB and
response to 1 MiB. Unknown fields and unsupported operations retain strict refusal.

`GET /v1/identity` is public and returns only:

```json
{"schema":"yvex.management.identity.v1","device_identity":"tls-sha256:<64hex>","display_name":"YVEX","protocol":"yvex.management.v2","pairing_available":false}
```

No Host, model, device, engine, session or job inventory is returned anonymously.

`POST /v1/pairing` accepts:

```json
{"schema":"yvex.management.pairing.request.v1","credential_hash":"<64 lowercase hex>","client_name":"Studio"}
```

The client generates 32 cryptographically random bytes and stores them through its
native protected credential owner. The wire bearer is their 64 lowercase hex
encoding. `credential_hash` is SHA-256 of the decoded 32 bytes, not their text
encoding. The producer stores the digest only. Labels are bounded user-facing
text and never an authorization decision.

Pairing status is:

```json
{"schema":"yvex.management.pairing.v1","request_id":"<credential_hash>","posture":"pending","scope":"product-management","expires_at_unix_ms":0,"reason":null}
```

Postures are `pending`, `approved`, `revoked`, `expired` and `refused`. Expiry is
the approval-request deadline, not a silently expiring approved grant. Approved
grants remain until explicit local revocation. A lost pairing response is recovered
by authenticated `GET /v1/pairing/status`; submitting the same digest while its
request exists is idempotent. Status requires `Authorization: Bearer <64hex>`.
The producer never returns the bearer or infers its ownership from a request ID.

A local `management pairing-open` action opens a 120-second request window.
Startup never opens it. Pending requests are bounded to 32; approved/revoked
records to 128. `pairing-list` shows exact request identity and client label.
`pairing-approve REQUEST_ID` approves one unexpired pending request;
`pairing-revoke REQUEST_ID` revokes one exact record. These actions are local
operator actions and have no remote self-approval endpoint. A remote label,
LAN origin or discovery result never authorizes itself. Revocation is reobserved
for every new request; already admitted owner work retains normal recovery rules.

`POST /v1/management` requires an approved bearer and carries the existing
`yvex.management.request.v2` envelope. The producer dispatches the exact same
product operation owner and produces `yvex.management.response.v2` with
`device_identity` equal to its TLS identity and `authenticated_peer` equal to
`credential-sha256:<credential_hash>`. The peer's private job directory uses a
hash with a network-domain prefix, separate from any equal SSH identity bytes.
Lost responses never authorize blind mutation retries: retain the exact request
identity and observe the existing job receipt.

## Same-machine public companion

The service also binds a public management Unix socket, distinct from the private
computational Host protocol. Its canonical path is
`$XDG_RUNTIME_DIR/yvex/product-management.sock` when the absolute runtime directory
is current-user owned with mode 0700. Without it, use
`/tmp/yvex-<effective uid>/product-management.sock` under an owner-only 0700 directory.
The socket is 0600. Both client and server verify ownership and the connected peer
UID through the OS; a same-path symlink or foreign owner refuses. An already live
socket is never replaced blindly. Only a dead same-user socket can be recovered
under the service startup lock.

The same bounded HTTP router and 36 product operation bodies run over this local
socket; TLS and bearer credentials are unnecessary after OS same-user validation.
Local management requests must send `X-Yvex-Device-Identity` with the previously
observed TLS service identity. The server checks it before dispatch; a recreated
service cannot accept a stale local mutation and only reveal the mismatch after
the effect. Duplicate headers refuse. The stable service TLS identity is still
returned, while `authenticated_peer` is
`local-user:<uid>`. Job ownership has a distinct local-user domain prefix. This
public local connection requires no SSH setup, manual pairing or fake account.
Discovery never auto-starts the service or the computational Host. Missing/stopped
management remains unavailable; no alternate hidden CLI or persistence reader.

## Evidence classes and remaining boundaries

Qualification uses disposable private directories, generated test identities and
loopback listeners. Required controls include TLS fingerprint mismatch, missing
and wrong credentials, pending/unapproved peer, exact approval, expiry, revocation,
strict framing, bounded slow clients, malformed private storage, local foreign
peer refusal, same-user automatic discovery, and durable mutation observation
after a lost response. A real SDK -> TLS -> tiny compiled CPU model path is separate
from synthetic router tests and from operator Spark/hardware acceptance.

The management grant does not authorize remote OpenAI inference. A future remote
inference credential owner and YAI provider integration must be separately
qualified. No UI may call a management-only connection a qualified Case provider.

## Standalone software candidate

The supported `make package` / `make install` path includes this service in the
single `yvex` executable; it requires neither YAI nor Studio to run. A disposable
local candidate can be installed using `make install DESTDIR=/absolute/staging/root
prefix=/usr` and invoked as `/absolute/staging/root/usr/bin/yvex management serve`.
The package's existing legal/distribution marker remains `UNQUALIFIED`; functional
management tests do not grant release or legal readiness.

For an explicitly selected LAN service, use `management serve --bind IP:18080
--name NAME --advertise`. On that host, `management pairing-open` opens the short
approval window; `pairing-list` and `pairing-approve REQUEST_ID` authorize one
identified client. `pairing-revoke REQUEST_ID` prevents its future requests.
No pairing command installs a service, changes a firewall, starts an Engine or
modifies a YAI Case. A deployment supervisor may own process startup separately.
