# YVEX

YVEX is a native C inference engine for local open-weight models.

It is built below chat interfaces, provider APIs, sampling loops, and agent
shells. Its job is to turn local model files into runtime-owned state: parsed
model facts, tensor roles, backend-resident weights, engine/session ownership,
scheduled graph work, and eventually token production by code that can explain
what happened.

That last word, "eventually", is important. YVEX does not yet run a complete
text-generation path. It does not prefill a prompt, own a live `KV` cache,
decode tokens, produce logits, sample text, or expose provider generation. What
exists today is narrower and real: controlled graph execution over fixture
weights, selected `F16` embedding execution over real model bytes, a selected
embedding-plus-RMSNorm segment over multiple real tensors, explicit token
sequence input into those graph paths, and integrity gates that stop unsafe
artifacts before allocation or dispatch.

That is the point of the project. Local inference is easy to make theatrical.
A prompt box can hide a weak runtime. A server can answer `/v1/models` while no
lower path can produce logits. A model can be "loaded" in half a dozen
incompatible senses: a path exists, a file opened, a header parsed, tensor names
were listed, weights moved to a device, graph work was scheduled, or tokens
actually came out. YVEX is built for the space between those words.

The project is opinionated: a local inference engine should earn every claim in
the order the machine actually needs it. First the artifact has to be readable.
Then the tensor directory has to make sense. Then tensor names must map to
runtime roles. Then selected bytes must be safe to read. Then memory has to
belong to a backend. Then the engine has to own the attachment. Then a graph
has to dispatch and write an output that can be checked. Later, and only later,
prompt prefill, `KV`, decode, logits, sampling, and generation should sit on top
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
but which boundary failed: file identity, GGUF parse, metadata, tensor mapping,
dtype accounting, byte range, registry drift, allocation, transfer, engine
attachment, graph preflight, backend op support, output allocation, reference
read, token input, future prefill, future decode, future logits.

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
kind ran, did cleanup happen after failure, and why is full execution still
false? Those questions should not require reading a profiler trace or guessing
from a chat transcript.

YVEX is therefore not just a file inspector. It is also not a complete
inference system. It is the lower runtime being assembled in public, with each
new boundary tested before the next claim is made.

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

The hardware lane is intentionally split:

- `CPU` is the correctness and diagnostics lane. It is where parser behavior,
  tensor accounting, reference values, cleanup, and small graph execution should
  be easiest to reason about.
- `CUDA` on Linux is the current accelerated lane. It is used for selected
  materialization and the implemented graph paths where CUDA parity exists.
- DGX Spark / GB10 is a CUDA-class pressure target for larger local runtime
  work. It matters because memory pressure changes what can be attempted.
- Metal on macOS is an important future local-machine lane. YVEX does not claim
  it today.
- ROCm on Strix Halo-class systems is also an important future lane. YVEX does
  not claim it today.

This is deliberately different from saying "the project runs everywhere." It
does not. The current implementation is compact and C-native. It has CPU and
CUDA behavior for the lower runtime work that exists today. Other lanes are
named because they are part of the local inference bet, not because the code has
earned them.

## What YVEX Is

YVEX is the engine layer between local model artifacts and future generation.
It is concerned with states that many tools collapse into one word:

- a model path;
- a parsed `GGUF`;
- a model descriptor;
- a tensor with a runtime role;
- a selected tensor planned for materialization;
- backend-owned storage;
- engine-owned weight attachment;
- session visibility into engine state;
- graph preflight;
- backend dispatch;
- output allocation;
- reference comparison;
- token input;
- future prompt prefill;
- future `KV` ownership;
- future logits and sampling.

Those are different states. They fail differently. They also need different
tests. YVEX keeps them separate so the runtime can refuse precisely instead of
collapsing everything into a vague "load failed" or "inference failed."

The public command surface reflects that choice. `inspect`, `metadata`, and
`tensors` show artifact facts. `models` gives operator-local artifacts stable
names without committing weights. `integrity report` summarizes local artifact
state. `materialize`, `engine`, and `session` expose residency and ownership.
`graph` runs the implemented fixture and selected real-tensor graph paths.
`run`, `chat`, and `yvexd` are diagnostic/provider-shaped surfaces until the
engine can produce model output for real.

The project is not trying to make the incomplete part look complete. If a path
is below generation, it says so. If an artifact is structurally valid but lacks
selected embedding readiness, it says so. If a registered alias points to bytes
that changed, it says so. If a backend cannot run an op, graph dispatch should
not happen.

## What YVEX Is Not

YVEX is not a generic `GGUF` runner. It parses and emits GGUF because that is
the artifact boundary YVEX currently uses, but arbitrary GGUF execution is not
the project claim.

YVEX is not a wrapper around another inference runtime. The runtime state it
does own is native: descriptors, selected weights, backend allocation summaries,
engine/session ownership, graph result structs, and CLI reports.

YVEX is not a chat UI. The `chat` and REPL surfaces are diagnostics until a real
generation path exists under them.

YVEX is not a provider generation server. `yvexd` is useful for status, metrics,
model listing, registry/direct-path handling, and provider-shaped diagnostics.
It must keep reporting generation as unavailable until the runtime can back it.

YVEX is not a benchmark leaderboard. It has correctness and regression
measurements today: exact fixture values, checksums, max-diff comparisons, token
selection, and guard/refusal behavior. Token/sec numbers wait for prefill,
decode, logits, sampling, and generation in one reproducible path.

YVEX is not a model artifact repository. Real weights, generated GGUFs,
operator registries, reports, logs, and build output stay outside git. The repo
contains source, headers, docs, tiny fixtures, and tests.

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

`MODEL_ARTIFACTS.md` is the public artifact card surface. It records external
artifact posture, selected pressure artifacts, digest facts, tensor facts, and
validation posture. It should describe operator-local artifacts without leaking
personal filesystem paths.

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
pretending to prefill.

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
  --model deepseek4-v4-flash-selected-segment \
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
It proves that memory planning, graph scheduling, backend dispatch, reference
comparison, and cleanup can cover more than one tensor and more than one op. It
is the first larger real-model graph segment, but it is still a segment. It is
not a transformer block. It is not prompt prefill. It does not own `KV`. It does
not produce logits.

The command is intentionally explicit:

```sh
./yvex graph \
  --model deepseek4-v4-flash-selected-segment \
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

## Backend Lanes

YVEX keeps backend lanes conceptually separate because each lane answers a
different engineering question.

`CPU` is the reference lane. It should be boring, inspectable, and stable. It is
where tiny fixtures, raw reference comparisons, shape/range failures, cleanup,
and graph result accounting are easiest to reason about.

`CUDA` is the active acceleration lane on Linux. The current CUDA path covers
backend diagnostics, selected materialization, fixture parity, selected
embedding execution, and the selected embedding-plus-RMSNorm segment where the
host has CUDA available.

GB10 / DGX Spark matters as a pressure target because high-memory local CUDA
machines change the scale of artifacts that can be exercised. It does not turn
YVEX into a complete model runner by itself.

Metal and ROCm are future lanes. They matter because local inference is not only
about one vendor stack, and high-memory local machines are a central part of the
project bet. But the README should not blur design pressure into support. The
current public claim remains CPU plus CUDA behavior for the runtime boundaries
that exist today.

## Provider Boundary

`./yvexd` is an early provider/status boundary. It can report process health,
metrics, model listing, direct paths, registry aliases, and generation
unavailability. That is useful groundwork because provider surfaces need to
know which runtime state exists before they can responsibly expose model output.

The provider boundary must stay honest. A server that can answer status is not
a server that can generate. A provider-shaped API is not provider-compatible
generation. Until the runtime owns prefill, `KV`, decode, logits, sampling, and
generation, `yvexd` should remain a status and diagnostics surface.

Typical daemon exploration starts here:

```sh
./yvexd --help
./yvexd --once
```

The runbook has the current daemon examples. The README deliberately does not
turn those examples into a generation story.

## Measurement Posture

YVEX has measurements, but not benchmark numbers. That distinction matters.

Valid measurements today are correctness and regression measurements at
boundaries the runtime owns:

- exact output values for controlled fixtures;
- output checksums for selected embedding;
- raw-artifact reference checksums;
- max absolute diff for embedding-plus-RMSNorm;
- token parsing and bounds behavior;
- materialization planned/allocated/transferred byte accounting;
- graph guard phase and cleanup behavior;
- CPU/CUDA parity for implemented paths on CUDA hosts.

Invalid claims today include token/sec, model quality, long-context throughput,
provider latency, sampling behavior, and generation speed. Those require a
runtime path that does not exist yet. When prefill, decode, logits, sampling,
and generation are implemented in one reproducible path, benchmark reporting
will need artifact identity, backend, qtype, context length, machine, command,
and trace/profile context.

Until then, benchmark results: none.

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

## More Documentation

If you want the command transcript, start with the runbook. If you want the C
surface, read the API document. If you want the rules for what the CLI may
claim, read the contract.

- `docs/operator-runbook.md`: command-first workflow for local artifacts,
  artifact integrity, materialization, engine/session attachment, graph proofs,
  daemon status, token input, and validation.
- `docs/api.md`: public C API, ownership rules, report structs, backend
  capabilities, token input, materialization summaries, graph results, and
  integrity report surfaces.
- `docs/contract.md`: CLI, filesystem, registry, backend, server, console,
  output, validation, and claim contract.
- `MODEL_ARTIFACTS.md`: external artifact cards, selected pressure artifacts,
  digest facts, tensor facts, and validation posture.
- `AGENTS.md`: operating rules for humans and coding agents working in this
  repository.
- `docs/spine.md`: internal delivery map. It is not product documentation, but
  it is the source of truth for implementation order inside this repository.

## Current Reading Order

For a new contributor, the shortest useful path is:

1. Read this README for project posture and public boundaries.
2. Read `docs/operator-runbook.md` and run the normal validation commands.
3. Read `MODEL_ARTIFACTS.md` before touching real artifacts.
4. Read `docs/contract.md` before changing CLI output or public claims.
5. Read `docs/api.md` before changing public headers or ownership behavior.
6. Read `docs/spine.md` before choosing implementation work.

That order matters because YVEX is not only code; it is code plus a claim
discipline. The project should become more capable without becoming vague.

## License

YVEX is licensed under the MIT license.
