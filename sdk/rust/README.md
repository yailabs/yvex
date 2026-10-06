# YVEX public client

Typed product management, separate finite computation and explicit public transport boundaries. See the repository documentation for qualification limits.

### Native network connections

Use `management::network::probe` to observe an untrusted HTTPS candidate. After
explicit out-of-band fingerprint confirmation, `ConnectionManager::trust` stores
an immutable safe profile and a random native-vault credential; it sends nothing.
`request_pairing` requests producer approval. After lost acknowledgement use
`pairing_status`, never redispatch. A terminal expired/refused/revoked observation
may use explicit `new_pairing` to create a fresh unpaired identity. It does not
revive the old grant. `client(profile_ref)` uses the pinned identity and approved
credential. `detect_local` observes the documented same-user public companion
without starting services or creating a commercial account.

Enable `native-credentials` for Linux Secret Service/macOS Keychain. The shared
`connections` example supports probe, trust, request, status, list, local, get,
new-request, invoke and forget. It takes retained invocation files, never secret
arguments. An explicit `YVEX_SDK_CONNECTION_PROFILE_ROOT` selects isolated safe
metadata for qualification; credentials still use the real native store. No
plaintext fallback exists. DNS-SD `management::discovery::browse` returns untrusted
hints; it never pairs automatically. See SDK architecture/qualification records.
