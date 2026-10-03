<!-- docs:metadata
title: Quick Start
id: yvex.guides.quickstart
document: guide
status: current
owner: yvex
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Quick Start

**Build the product, select admitted material and open a retained host session.**

[Up](README.md)

[Build details](build.md) · [Source preparation](source-preparation.md) · [Recovery](recovery.md)

## Build for your platform

Install the [native prerequisites](build.md#developer-and-packager-entry-points).
On Apple Silicon, follow the [macOS toolchain setup](build.md#macos-native-cpu-build)
first, including modern GNU Make. Then, from the checkout root:

```sh
make -j4
./yvex version --json
./yvex help
./yvex model list
```

The repository does not include model weights. Catalog entries can describe
source-only material; use [model lifecycle](model-lifecycle.md) to acquire and
prepare an exact admitted representation and inspect its readiness.

## Choose an admitted execution path

Linux CPU/CUDA model paths and macOS CPU model paths have separate evidence.
Apple Silicon also has an early Metal backend, inspectable with
`./yvex inspect backend metal`; its current GPU scope is shared storage and
F32 embedding primitives. A successful device inspection does not admit a model
engine. Use the [Metal procedure](build.md#apple-silicon-metal-foundation) for
that foundation and the [Qwen 0.8B CPU record](../evaluation/macos-small-model.md)
for the exact bounded Mac model result.

For an admitted model with qualified conversation support, start `./yvex serve`
in one terminal if no host is already running. In another:

```sh
./yvex model show MODEL
./yvex model load MODEL
./yvex chat --model MODEL
```

Replace `MODEL` with its catalog identity. Backend and conversation support must
both be admitted; the Mac Qwen CPU continuation proof does not establish chat.
The [interactive client](interactive-client.md) owns chat procedures and
[Status](../project-control/STATUS.md#model-family-and-hardware-scope) owns the
current evidence limits.
