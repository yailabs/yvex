# YVEX public client

The canonical MIT client is owned by YVEX. Rust clients use `yvex-sdk` from
`sdk/rust`; TypeScript consumers use `@yvex/sdk`. Neither depends on YAI, Studio,
a private native runtime, or its persistence. The optional finite native adapter
requires only an explicitly installed public YVEX C client.

The Rust crate is an independent workspace: `cargo test --manifest-path
sdk/rust/Cargo.toml` does not build the compiler/runtime. Public HTTPS, local
companion and advanced restricted SSH clients preserve exact identity and recovery.
The CLI and network dispatcher invoke the same producer domain owners.

`yai-sdk` remains a compatibility/composition consumer. New independent clients
should pin YVEX directly. TypeScript contains safe public records, not renderer
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
