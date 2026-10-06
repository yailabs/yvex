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
