<!-- docs:metadata
title: Build YVEX
id: yvex.guides.build
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Build YVEX

**Prepare the native toolchain and build the supported local product.**

[Up](README.md)

## Developer and packager entry points

GNU Make 4.3 or newer, a C11 compiler, Python 3 and the pinned REPLAI build
toolchain are required. CUDA is optional at build time; an explicitly requested
CUDA execution still refuses without an admitted kernel bundle. Start with
`make help`, then `make -j4` (library plus the single `yvex` executable).
`make check` and registered `make qa-*` lanes are software qualification, not
release or model-quality claims. [QA](../evaluation/qa.md) owns lane selection.

The root Makefile is the entry point. `config/make/config.mk` owns toolchain
configuration; `rules.mk` owns source-relative compilation, generated inputs and
linking; `qa.mk` supplies existing qualification adapters; `docs.mk` owns
publication targets; `distribution.mk` owns software staging and legal gates.
Source and test membership still come only from their existing manifests and
registries. `make print-build-inputs` exposes the parsed authored Make inputs.

`CC`, `AR`, `CPPFLAGS`, `CFLAGS`, `LDFLAGS`, `LDLIBS`, `NVCC`, `NVCCFLAGS`,
`BUILD_DIR` and `YVEX_CUDA_ARCH` are explicit overrides. Packager `CPPFLAGS`
augment mandatory project includes/feature definitions. Material compiler and
link flags invalidate their consumers; C and CUDA transitive header dependencies
are generated, including PTX and native CUBIN. Concurrent image/archive publication
uses complete staged files. Use an independent build directory for separate
toolchain variants; this is not an out-of-source configure interface.

For an executable **software candidate**, including its current manifest and
license receipts:

```sh
make package
make install DESTDIR=/absolute/staging/root prefix=/usr
```

GNU `exec_prefix`, `bindir`, `datarootdir`, `datadir`, `INSTALL_PROGRAM` and
`INSTALL_DATA` overrides are supported. Installation derives file membership
from `config/package_manifest.tsv`; it does not install model payloads or invent
a separately qualified SDK package. It preserves the `UNQUALIFIED` legal marker.
Neither `package` nor `install` grants release/distribution readiness:
`qualify-distribution DISTRIBUTION_PACKAGE=/absolute/legal-bundle` remains the
fail-closed gate for an exact recipient package. Unknown dependency/legal closure
remains blocked. `clean` removes only its validated build tree and, for the
default tree, `./yvex`; it neither follows symlink ancestors nor deletes arbitrary
root objects or historical executables.

## Prerequisites

Builds provide one executable product. `yvex serve` owns the private Unix
listener and bounded loopback OpenAI-compatible listener in the foreground.
`yvex model load` resolves a launchable logical model to an exact deployment
and asks that host to create an engine generation. Several generations may
coexist within the admitted host bound. Other `yvex` modes own native clients
and finite offline engineering operations.

Loading a model requires one complete registry startup profile. A text runtime
binds one admitted GGUF to its exact runtime binding, target, backend, and
default context capacity. A text load may request a different bounded context
with `--ctx N`; the server checks the compiled model limit and current resource
envelope before creating a new engine generation. A composite runtime instead
binds an installed component root
to its target, backend, and capability mode without inventing a singular
artifact or text-runtime binding. Inspect the product catalog first:

```sh
./yvex model list --wide
./yvex model show MODEL
```

The table contains one row per proven logical model and exposes its selected
format, quantization or precision, representation size, state, backend,
location, and number of alternatives. `model show` expands the exact lineage.
Only READY models can cross the engine boundary. If none is ready, complete the
pull and preparation path or the one-time advanced
[registration procedure](source-preparation.md#registering-an-existing-model). Backend selection is
part of the resolved deployment and never falls back silently.

Provider credentials remain owned by the installed provider CLI; YVEX records
only redacted observations and never persists a raw token. Discover the exact
implemented account operations and inspect current state with:

```sh
./yvex help source accounts
./yvex source accounts providers --output table
./yvex source accounts status --output table
./yvex source accounts whoami huggingface --output table
```

Authentication and removal remain explicit:

```sh
./yvex source accounts login huggingface
./yvex source accounts logout huggingface
```

The Hugging Face integration delegates credentials to the installed `hf`
client; the GitHub integration delegates them to `gh`. `--json` provides the
complete redacted machine projection. Account commands never print a token.
