# CLORI

CLORI is a Neural Execution Engine for local AI models and runtime control.

CLORI is early. This repository is a scaffold for the external CLORI project
under YAI Labs, and it is not production-ready.

CLORI is not YAI. CLORI can be used independently, and it may later become a
NET-compatible external node for YAI.

YAI controls authority.
NET moves streams.
CLORI executes neural computation.

## Status

- Inference is not implemented.
- Server behavior is not implemented.
- NET compatibility is planned, not implemented.
- No benchmark results exist yet.
- No model support is claimed in this scaffold.

## Repository Layout

```text
docs/        architecture, boundary, spine, integration and benchmark notes
src/         source placeholder
protocols/   protocol placeholder
benches/     benchmark placeholder
examples/    example placeholder
tests/       test placeholder
```

## Validation

```sh
make info
make check
```

## License

CLORI is licensed under the MIT license. See [LICENSE](LICENSE).
