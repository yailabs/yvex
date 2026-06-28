# YVEX

YVEX is a native C inference engine for local open-weight models.

It is built below chat interfaces, provider APIs, sampling loops, and agent
shells. Its job is to turn local model files into runtime-owned state: parsed
model facts, tensor roles, backend-resident weights, engine/session ownership,
scheduled graph work, and eventually token production by code that can explain
what happened.

YVEX is being built toward the whole local transformer path, not toward a
permanent demo slice. Today the engine owns the lower part of that path:
controlled graph execution, selected `F16` embedding over real model bytes,
embedding plus RMSNorm over multiple real tensors, explicit token input, and a
prefill state foundation. Those stages are deliberately small because they are
the load-bearing floor for the later parts of the run: `KV` ownership, decode,
logits, sampling, and generation. The current boundary is not the final shape of
YVEX; it is the part of the engine that has already been made accountable.

That is the point of the project. Local inference is easy to make theatrical.
A prompt box can hide a weak runtime. A server can answer `/v1/models` while no
lower path can produce logits. A model can be "loaded" in half a dozen
incompatible senses: a path exists, a file opened, a header parsed, tensor names
were listed, weights moved to a device, graph work was scheduled, a state
summary was recorded, or tokens actually came out. YVEX is built for the space
between those words.

The project is opinionated: a local inference engine should earn every claim in
the order the machine actually needs it. First the artifact has to be readable.
Then the tensor directory has to make sense. Then tensor names must map to
runtime roles. Then selected bytes must be safe to read. Then memory has to
belong to a backend. Then the engine has to own the attachment. Then a graph
has to dispatch and write an output that can be checked. Then token input has
to be explicit. Then a prefill state can be summarized. Later, and only later,
real `KV` ownership, decode, logits, sampling, and generation should sit on top
of that lower path.

The `GGUF`, registry, integrity, qtype, and model-building machinery exists
because local inference needs those things close to the engine. They are not
decorations around a future chat box. They are how a native runtime learns what
the file says, which tensors mean what, how many bytes may be read, which dtypes
are storage-only, which backend owns a buffer, and where a run stopped when
something refuses to continue.

## Motivations

Very capable open-weight models finally exist. They are large enough, useful
enough, and available enough that local inference deserves runtime work that is
more serious than a thin wrapper and more inspectable than a magic prompt box.
At the same time, high-memory local machines are becoming interesting again:
desktop CUDA systems, DGX Spark / GB10-class machines, large-memory workstations,
and future local lanes where unified memory or fast storage change what can be
attempted outside a hosted provider.

YVEX exists because that world needs engines that can be debugged at the right
depth. When a local model fails, the useful question is not "did inference fail?"
The useful question is which boundary failed: file identity, GGUF parse,
metadata, tensor mapping, dtype accounting, byte range, registry drift,
allocation, transfer, engine attachment, graph preflight, backend op support,
output allocation, reference read, token input, prefill state, future `KV`,
future decode, future logits. Those are different locations in the runtime, and
they deserve different failure modes.

There are excellent local inference projects already. YVEX is not trying to be
all of them. It is taking a narrower route: native C, explicit ownership, local
artifacts, small command surfaces that reveal runtime state, and a refusal bias
when the engine cannot prove the next step. The narrowness is deliberate. A
system that refuses honestly is easier to grow than a system that accepts
everything and blurs what happened.

DeepSeek-class models are useful pressure targets because they force real
choices early. Their tensor sizes, qtype boundaries, memory movement, tokenizer
facts, and hardware pressure make weak abstractions show themselves. They are
not branding. If another open-weight model becomes the better target for local
runtime work, YVEX should move. The rule stays the same: support is earned
through source evidence, tensor mapping, artifact identity, backend residency,
scheduled graph execution, and tests.

The project also exists because local runtime work should be inspectable by
operators. A developer should be able to ask: which artifact am I using, which
digest did I record, did the alias drift, which primary tensor did the registry
remember, which backend was selected, how many bytes were planned, which graph
kind ran, how many tokens were processed, did cleanup happen after failure, and
why is full execution still false? Those questions should not require reading a
profiler trace or guessing from a chat transcript.

YVEX is therefore growing the inference system from the lower runtime upward,
with each stage made inspectable before the next one is added.

## The Local Inference Bet

The bet is simple: open-weight models plus serious local machines will keep
mattering, and the engines that run them should be understandable all the way
down to artifact bytes and backend ownership.

Hosted inference is convenient, but it hides the machine. Local inference puts
the machine back into the story: how much memory exists, which backend owns
which buffer, which qtype is storage-only, which graph op is actually available,
which context length will later be possible, and what happens when a local file
changes under a registered alias. YVEX treats those details as first-class
runtime concerns instead of operator trivia.

The hardware story follows the same rule as the runtime story: use the lanes
that have evidence, name future pressure directions clearly, and do not blur a
machine class into a support claim.

## Machines and backend lanes

YVEX keeps machine classes and backend lanes visible because local inference is
not abstract. Memory size, backend ownership, copy behavior, and graph op
coverage change what the engine can prove.

| Machine class | Backend lane | Why it matters |
| --- | --- | --- |
| CPU-only developer machine | CPU reference | Parser, descriptor, token input, integrity, cleanup, and small graph correctness stay debuggable. |
| CUDA workstation | CUDA | Current accelerated lane for selected materialization and selected graph execution. |
| DGX Spark / GB10-class CUDA machine | CUDA pressure target | Useful for larger local artifacts and future memory-pressure work. |
| High-memory MacBook / Mac Studio | Metal direction | Important local inference class for future unified-memory work. |
| Strix Halo-class system | ROCm direction | Important local inference class for future unified-memory AMD work. |

The current public claim remains CPU plus CUDA behavior for the runtime
boundaries that exist today. Metal and ROCm are named as local inference
directions, not current support.

## What YVEX Is

YVEX is the engine layer for local transformer execution. It cares about model
files, tensor roles, backend memory, graph scheduling, token input, prefill
state, future `KV`, future decode, future logits, and future sampling because
all of those belong to one run. They are not separate side quests; they are the
layers of the same inference engine.

That engine starts before a prompt exists. It starts when a local artifact can
be identified, parsed, bounded, mapped into tensor roles, and attached to
runtime ownership. It continues when selected weights become backend-resident,
when graph preflight can prove the next read is legal, when a backend op
dispatches, when output bytes can be checked against a reference path, and when
token sequences become runtime state rather than loose command arguments.

The public command surface reflects those layers. `inspect`, `metadata`, and
`tensors` show artifact facts. `models` gives operator-local artifacts stable
names without committing weights. `integrity report` summarizes local artifact
state. `materialize`, `engine`, and `session` expose residency and ownership.
`graph` runs the implemented fixture and selected real-tensor graph paths.
`input` makes token sequences explicit. `prefill` creates the current prefill
state summary. `run`, `chat`, and `yvexd` keep user and provider shapes wired
while the lower runtime grows into the generation path.

## What YVEX Is Not

YVEX is intentionally not a generic `GGUF` runner, a wrapper around another
runtime, a chat UI, a benchmark leaderboard, or a model-weight repository. Those
choices are not limitations of ambition; they are boundary choices. The project
is focused on owning the native inference path rather than hiding missing
runtime state behind a broad surface.

`yvexd` is currently a provider/status boundary. It is there so server shape can
be wired early, but provider-backed generation should only arrive when the same
runtime path can actually prefill, decode, produce logits, sample, and stream
tokens.

Real weights, generated GGUFs, operator registries, reports, logs, and build
output stay outside git. The repo contains source, headers, docs, tiny fixtures,
and tests.

## Model Artifacts

Real model artifacts are operator-local. They are documented, registered, and
checked, but not committed. This is a hard boundary: public source control must
not become a model-weight dump, a local registry dump, or a collection of
developer machine paths.

The active pressure artifact is a selected DeepSeek embedding GGUF containing
`token_embd.weight`. The selected tensor has shape `[4096,129280]`, dtype `F16`,
and about one billion tensor bytes. That is large enough to make the engine
care about file identity, range math, backend allocation, transfer, cleanup,
engine attachment, graph dispatch, and output comparison. It is not enough to
claim the model runs.

There is also a selected segment artifact for the current real graph segment.
It contains the real token embedding tensor and the first RMSNorm weight, such
as `blk.0.attn_norm.weight`, along with the metadata needed to run embedding
lookup followed by RMSNorm. That segment is useful because it makes graph work
multi-tensor and multi-op while remaining below transformer prefill.

[MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md) is the public artifact card surface. It
records external artifact posture, selected pressure artifacts, digest facts,
tensor facts, and validation posture. It should describe operator-local
artifacts without leaking personal filesystem paths.

A typical local registration looks like this:

```sh
./yvex models add \
  --path /path/to/operator/model.gguf \
  --alias deepseek4-v4-flash-selected-embed \
  --support-level selected-tensor-materialized

./yvex models use deepseek4-v4-flash-selected-embed
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
```

The alias is not a claim that a complete DeepSeek model is executable. It is an
operator-local handle for the specific artifact and support level that YVEX has
evidence for.

## GGUF and Model-Building Work

A local inference engine cannot treat `GGUF` as a file extension only. The
container carries model architecture, metadata, tensor names, tensor ranks,
dimensions, dtypes, byte offsets, qtypes, alignment, and tokenizer information
where present. Those facts are not just displayed by the CLI; they become
runtime preconditions.

YVEX keeps model-building tools near the runtime because the engine needs to
know why an emitted artifact looks the way it does. `source-manifest` records
source evidence. `native-weights` inventories native tensors. `tensor-map`
connects source tensor names to runtime roles. `gguf-template` states what an
artifact is expected to contain. Narrow `convert` paths emit selected artifacts
that the current runtime can validate and use.

Quantization is treated with the same separation. `qtype-support` describes
what storage formats are recognized. `quant-policy` records policy choices.
`quant-job` records external quantization work. `imatrix` keeps calibration
evidence separate from runtime compute support. Recognizing a storage format is
not the same as having a kernel for a graph op. YVEX should not blur those two
facts.

That discipline can feel heavy when the graph is still narrow. It is there
because the next stages will be harder, not easier. Prefill and decode will not
be made more trustworthy by pretending tensor layout, dtype policy, artifact
identity, and backend ownership are small details.

## Integrity and Refusal Gates

The integrity layer is local correctness and corruption safety. It is not
supply-chain security, malware detection, sandboxing, author authenticity,
model-quality validation, or remote provenance. A passing report means the
implemented local checks passed. It does not mean the model is good, safe,
complete, or capable of generation.

The current integrity stack covers several boundaries:

- structural GGUF checks;
- tensor directory parsing;
- duplicate and empty tensor names;
- tensor rank and dimension accounting;
- dtype storage recognition and byte-count math;
- tensor byte-range validation;
- selected token slice validation;
- file identity and SHA-256 digest comparison;
- registry alias metadata drift;
- selected embedding readiness;
- materialization preflight before backend allocation;
- graph preflight before dispatch and raw reference reads;
- cleanup and repeat behavior after injected failures;
- a consolidated regression harness for corrupt and drifted artifacts.

The normal operator summary is:

```sh
./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
```

For a registered alias, the report can include digest identity, registry
metadata drift, selected tensor readiness, materialization preflight, and graph
guard status. For a raw path, it reports the current digest and local artifact
state without pretending registry metadata exists. If no backend is supplied,
backend readiness is not checked.

The important thing is the refusal boundary. If structural integrity fails,
materialization should not allocate. If a registered alias has a digest
mismatch, the engine should not silently trust it. If metadata drift invalidates
the registered tensor facts, graph execution should not dispatch. If a selected
token is out of range, the reference slice should not be read. The value of the
integrity module is not that it says "pass" often; it is that it says "stop"
early and specifically.

## What Runs Today

YVEX currently has two executable proof lanes. They are both real, but they are
not the same kind of proof.

The controlled lane uses tiny `F32` fixtures. It is there to prove executor and
backend mechanics under conditions where every value can be known. The runtime
opens the fixture, attaches selected tensor data, dispatches an embed node,
allocates output, reads values back, and checks stable output. This lane is not
real-model inference. It is the exact, deterministic lane where backend and
graph mechanics can be caught without model complexity hiding the bug.

The selected-artifact lane uses real model bytes. It starts with the selected
DeepSeek-class embedding tensor and moves that artifact through identity,
integrity, materialization, engine-owned attachment, graph preflight, backend
dispatch, output readback, and raw-artifact reference comparison. This lane is
real model tensor participation in scheduled graph work. It is still not a
complete transformer.

The first real graph segment reads `token_embd.weight`, interprets the tensor as
`[hidden_size, vocab_size]`, selects a token, converts the `F16` embedding row to
an `F32` output vector, and compares the result with an independent raw-artifact
reference slice. The larger selected segment runs embedding lookup followed by
RMSNorm using a real first RMSNorm weight. The output is checked by checksum and
max-diff against the reference path.

Explicit token input is part of the current boundary. `yvex input tokens`
parses bounded comma-separated token IDs, validates them, and graph commands can
select a token by index. Prompt text is different: it only becomes executable
token input when tokenizer metadata is present and executable for the artifact.
Selected artifacts without tokenizer metadata fail cleanly instead of
pretending to run a full prompt prefill.

YVEX can also build a prefill state summary from explicit token input by running
the implemented selected segment over the token sequence. This is a foundation
state: it records positions, per-token segment execution aggregation, checksums,
and byte accounting. It is not attention `KV`, decode, logits, sampling, or
generation.

The short CPU path for the selected embedding proof is:

```sh
make
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
```

CUDA-capable hosts can run the same lower boundary on the CUDA lane:

```sh
./yvex cuda-info
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cuda --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cuda --execute-partial --partial-token 0
```

The segment path uses the selected segment artifact and the segment graph mode:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --execute-segment \
  --segment embedding-rmsnorm \
  --tokens 0,1 \
  --token-index 0
```

Those commands are examples of the runtime boundary, not a general recipe for
running a full model. The operator runbook keeps the longer expected outputs,
CUDA variants, fixture proof, failure modes, and gate commands.

## Operator Path

The normal operator path is intentionally repetitive. That repetition is useful:
each command crosses a different runtime boundary, and each boundary can fail
with a different reason.

First, build and identify the command surface:

```sh
make
./yvex commands
./yvex help integrity
./yvex help graph
```

Then make sure the local artifact is registered and still matches its recorded
identity:

```sh
./yvex models current
./yvex models verify deepseek4-v4-flash-selected-embed
```

Ask for the aggregate integrity view before moving bytes:

```sh
./yvex integrity report \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --require-token-embedding \
  --partial-token 0
```

Materialize only after the artifact passes the relevant checks:

```sh
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
```

Attach the selected backend-resident weights to engine-owned state:

```sh
./yvex engine --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex session deepseek4-v4-flash-selected-embed --backend cpu
```

Then run the bounded graph path:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --execute-partial \
  --partial-token 0
```

The graph result reports the graph guard, selected tensor, backend, output
shape, checksum, reference checksum, max diff, and readiness boundaries. The
important success is not "the model generated text"; it is that a real selected
tensor participated in guarded scheduled graph work and matched the independent
reference path.

For explicit token lists, the same boundary can be driven by token input:

```sh
./yvex input tokens --tokens 0,1,2

./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --execute-partial \
  --tokens 0,1,2 \
  --token-index 1
```

That selects token `1` from the explicit token sequence. It does not prefill a
prompt and does not create a `KV` cache.

## Controlled Fixture Graph

The fixture graph is deliberately small. It uses controlled `F32` data so graph
execution can be checked exactly. That is useful because runtime mechanics need
a stable place to fail: descriptor construction, weight attachment, backend
dispatch, memory planning, output allocation, cleanup, and checksums.

The fixture command shape is:

```sh
./yvex graph \
  --model tests/fixtures/gguf/valid-metadata-tensors.gguf \
  --backend cpu \
  --execute-fixture \
  --fixture-token 0
```

The exact fixture paths used by tests are intentionally tiny. Tracked GGUFs in
this repository are fixtures only. Real model artifacts remain external.

CUDA parity for the fixture lane belongs in the validation suite. If a backend
cannot run the fixture op, the graph guard should fail before dispatch instead
of pretending the graph ran.

## Selected Embedding Segment

The selected embedding segment is the first real-model graph proof. It uses the
operator-local selected artifact, the real `F16 token_embd.weight` tensor, and a
selected token ID. The tensor is interpreted according to the runtime convention
as `[hidden_size, vocab_size]`. For the active pressure artifact that means a
hidden size of `4096` and a vocabulary size of `129280`.

The graph path validates the artifact, identity, metadata, shape, dtype, tensor
range, selected token slice, backend availability, backend op support, output
allocation size, and raw reference read range before completing the run. The
output is an `F32` vector. The reference path independently reads the raw
artifact slice and compares it with the backend output.

This path matters because it crosses a real threshold: YVEX is not only parsing
metadata and not only copying bytes. A real selected model tensor participates
in scheduled graph computation. It still does not imply attention, MLP, routed
experts, logits, sampling, or text generation.

## Selected Embedding Plus RMSNorm Segment

The next real segment adds one more operation and one more real tensor. The
segment reads `token_embd.weight`, performs embedding lookup, then uses the
first RMSNorm weight such as `blk.0.attn_norm.weight` with explicit epsilon to
produce a normalized `F32` vector.

That path is important for a different reason than the embedding-only segment.
It is the first larger real-model graph slice on the road to transformer block
execution. It proves multi-tensor scheduling, intermediate memory planning,
backend RMSNorm support, cleanup, and reference comparison before attention and
MLP are added.

The command is intentionally explicit:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --execute-segment \
  --segment embedding-rmsnorm \
  --partial-token 0
```

The runbook documents the registered alias, integrity report, CPU run, CUDA
variant, and expected result fields. README keeps the concept visible without
becoming the full operator transcript.

## Token Input Boundary

Token input is now a runtime boundary. This is not the same thing as prompt
prefill. Explicit token sequences are parsed, bounded, and routed into the
implemented graph paths. Prompt strings only become tokens when executable
tokenizer metadata is available.

The small command is:

```sh
./yvex input tokens --tokens 0,1,2 --token-index 1
```

Graph commands can consume the selected token:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-embed \
  --backend cpu \
  --execute-partial \
  --tokens 0,1,2 \
  --token-index 1
```

This makes the graph input explicit. It also keeps the refusal honest: if the
token is out of range, graph dispatch should not happen. If prompt text lacks an
executable tokenizer path for the artifact, it should not be quietly treated as
prefill.

## Prefill State Foundation

The prefill state foundation is the first place where a token sequence becomes
runtime state instead of a command argument. It records positions, processed
tokens, per-token segment summaries, checksums, output byte accounting, and
cleanup status. It is still below attention `KV` and logits, but it is no
longer just token parsing.

The current state uses the implemented selected embedding-plus-RMSNorm segment
as the per-token graph slice. That gives the next runtime boundary something
concrete to attach to: position accounting, per-token graph work, failure
cleanup, checksum aggregation, and a state object that can be inspected before
minimal `KV` ownership arrives.

The command shape is:

```sh
./yvex prefill \
  --model deepseek4-v4-flash-selected-embed-rmsnorm \
  --backend cpu \
  --segment embedding-rmsnorm \
  --tokens 0,1,2
```

The next runtime boundary is minimal `KV` ownership. The prefill state
foundation makes that boundary less vague.

## Provider Boundary

`./yvexd` is an early provider/status boundary. It can report process health,
metrics, model listing, direct paths, registry aliases, and generation
unavailability. That is useful groundwork because provider surfaces need to
know which runtime state exists before they can responsibly expose model output.

The provider boundary must stay honest. A server that can answer status is not
a server that can generate. A provider-shaped API is not provider-compatible
generation. Until the runtime owns real `KV`, decode, logits, sampling, and
generation, `yvexd` should remain a status and diagnostics surface.

Typical daemon exploration starts here:

```sh
./yvexd --help
./yvexd --once
```

The runbook has the current daemon examples. The README deliberately does not
turn those examples into a generation story.

## Speed and evaluation shape

YVEX should publish speed numbers only for runtime paths it actually owns. Today
that means correctness and regression evidence, not token/sec claims. When the
engine owns real prefill, decode, logits, and generation in one path, speed
tables should include machine, backend, artifact identity, qtype, context
length, command, and reproducibility notes.

| Measurement | What will be measured | When it becomes meaningful |
| --- | --- | --- |
| Fixture graph correctness | exact values and checksums | already useful for executor regression |
| Selected segment correctness | checksum and max-diff against raw-artifact reference | useful for graph/kernel regression |
| Prefill throughput | token interval throughput over real prefill | once prefill owns real transformer state |
| Decode throughput | generated-token decode speed | once decode and logits exist |
| Generation throughput | end-to-end prompt plus generated tokens | once sampling and generation exist |
| Capability regression | task-level output stability | once the same generation path users run exists |

## Build and Validation

The normal build is small:

```sh
make
```

The standard validation gate is:

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

CUDA-capable hosts should also run:

```sh
make check-cuda
```

The artifact guardrail is equally important:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is that no real model weights are tracked. Tracked GGUF files
are tiny fixtures under `tests/`.

## Repository Shape

The repository is intentionally compact. It is not split into many top-level
packages because the runtime boundaries are still close together and the tests
need to exercise them as one system.

Important areas:

- `yvex_cli.c` wires the command surface.
- `yvexd.c` is the provider/status daemon entrypoint.
- `yvex_*.c` files hold the compact runtime modules.
- `include/yvex/` contains the public C API.
- `gguf/` contains the parser, emission, family mapping, conversion helpers,
  qtype policy, and artifact tooling.
- `cuda/` contains the CUDA host bridge and kernel unit.
- `tests/` contains unit coverage, CLI coverage, tiny fixtures, vectors, and
  regression harnesses.
- `docs/` contains the API map, contract, operator runbook, and internal spine.

As the runtime grows, source boundaries should follow ownership boundaries.
CLI code should not own backend logic. Server code should not duplicate runtime
wiring. Parser fixtures, runtime fixtures, evaluation vectors, and future
benchmarks should remain distinct. Build output, reports, local registries, and
operator artifacts should stay out of git.

## Documentation

The public documentation is intentionally small:
[docs/operator-runbook.md](docs/operator-runbook.md) is the command-first
workflow for artifacts, integrity, materialization, graph proofs, token input,
prefill state, daemon status, and validation; [docs/api.md](docs/api.md)
documents the C API, ownership rules, report structs, backend capabilities,
token input, prefill state summaries, materialization summaries, graph results,
and integrity surfaces; [docs/contract.md](docs/contract.md) is the behavior
and claim contract for CLI output, filesystem state, registry, backend, server,
validation, and public boundaries; [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md)
records external artifact cards, selected pressure artifacts, digest facts,
tensor facts, and validation posture; [AGENTS.md](AGENTS.md) gives repository
operating rules for humans and coding agents; and
[docs/spine.md](docs/spine.md) is the internal delivery map, not product
documentation.

## License

YVEX is licensed under the MIT license.
