# YVEX public client

The canonical MIT client is owned by YVEX. Rust clients use `yvex-sdk` from
`sdk/rust`; TypeScript consumers use `@yvex/sdk`. Neither depends on YAI, Studio,
a private native runtime, or its persistence. The optional finite native adapter
requires only an explicitly installed public YVEX C client.

The Rust crate is an independent workspace: `cargo test --manifest-path
sdk/rust/Cargo.toml` does not build the compiler/runtime. Public HTTPS, local
companion and advanced restricted SSH clients preserve exact identity and recovery.
The CLI and network dispatcher invoke the same producer domain owners.

Clients pin YVEX directly. YAI SDK 0.8 removes the historical YVEX facades in
a breaking SDK release; YAI Application contracts remain owned by
that independent package. TypeScript contains safe public records, not renderer
credential transport. `sdk/tools/generate.py` owns generated projections from the
reviewed public records under `sdk/rust/contract`.

Managed Host clients use `hosts().control`, `prepare_start`, `prepare_stop`, and
`prepare_restart`; Job observation retains exact identity after transport loss.
Remote service control is a separately advertised owner grant. The SDK never
adopts an external Host, provisions a server by shell command, or restores model
and Session state after restart. See the producer product-management contract.

Program P clients discover `optimization.techniques` and prepare the bounded
`optimization.plan` Job. `OptimizationPlanInput` identifies a catalog model and
exact revision, never arbitrary source/output paths. Results retain candidate,
context and build identities, missing quality and null unmeasured performance.
The [producer handoff](../docs/contracts/product-management.md#physical-compilation-consumer-handoff)
defines limits, recovery and the separate local production/deployment boundary.

The independent `optimization` example demonstrates the public companion:

```text
cargo run --manifest-path sdk/rust/Cargo.toml --example optimization -- prepare input.json invocation.json
cargo run --manifest-path sdk/rust/Cargo.toml --example optimization -- invoke invocation.json
cargo run --manifest-path sdk/rust/Cargo.toml --example optimization -- get <retained-request-id>
cargo run --manifest-path sdk/rust/Cargo.toml --example optimization -- evidence <published-record-id>
```

Prepare refuses overwrite. Dispatch happens once; on transport loss use `get`,
not another planning request. The example neither loads a model nor changes
credentials. Independent HTTPS clients use the identical typed operation over
their approved connection. No CLI-output parsing or private socket is involved.
Plan evidence references preserve the published receipt identity and association
scope. `client.qualifications().get(...)` retrieves the complete record by ID;
it cannot read caller-chosen files or grant qualification to another candidate.

## Independent conformance and migration provenance

The SDK has one implementation. Its legacy external conformance harnesses moved
from MIT `yailabs/yai-sdk` revision `7a2a19b00d6e5934f30d6d1d3d11b38a2137c6f5`
to `tests/sdk`, `tests/fixtures/sdk` and `tests/integration/sdk_finite_client.c`;
exact producer parity lives in
`sdk/tools/check_parity.py`. Both repositories retain the same Francesco
Maiomascio copyright and MIT notice. No proprietary Core or Studio source is
included. Historical facade releases remain available in their Git history.

```sh
CARGO_BUILD_JOBS=2 cargo test --manifest-path sdk/rust/Cargo.toml --locked --all-targets
python3 sdk/tools/generate.py --check
python3 sdk/tools/check_parity.py
python3 -m unittest discover -s tests/sdk -p '*contract.py'
python3 tests/sdk/test_management_remote.py
python3 tests/sdk/test_finite_remote.py
python3 tests/sdk/test_finite_native.py --include-dir /path/to/public/include
```

SSH controls use ephemeral private keys and isolated loopback listeners, never
operator connections. Native finite controls link a synthetic ABI peer, not an
inference engine. They require libclang and its matching system headers.
`test_network_native.py` additionally requires an explicit disposable producer
binary and an available native vault; it never substitutes plaintext storage.
Build its examples with `native-credentials` in a separate Cargo target directory
and set `YVEX_SDK_CONNECTIONS_EXAMPLE` (and `YVEX_SDK_PRODUCT_EXAMPLE` for the
product controls) to those exact executables. Default-feature `--all-targets`
tests otherwise rebuild the same example paths without native credentials.
Registered QA distinguishes these environment gates from standalone contract
tests. A passing client fixture is not model, deployment or Studio acceptance.

Consumers migrate Rust dependencies from the facade to this independent
`sdk/rust` package and TypeScript imports from `@yai/sdk/yvex` to `@yvex/sdk`.
Pin a reviewed YVEX revision in both manifests; keep YAI SDK versioning separate.
API changes originate here, consumers adopt independently through supported
schemas, and no SDK dependency grants runtime authority or permission to replay.
