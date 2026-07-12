# YVEX

**YVEX is a native C inference engine for local open-weight models.** It owns
the transformation from upstream weight shards to generated token IDs. A 70B
dense checkpoint contains approximately **140 GB** of FP16 weights; an ideal
4-bit representation lowers the weight payload toward **35 GB**, but neither
figure is a residency estimate. Prefill and decode add persistent KV state,
transient dequantization buffers, backend scratch and allocator fragmentation.
The relevant constraints are therefore peak live bytes, memory traffic and
synchronization frequency rather than whether the checkpoint can be opened.

YVEX represents that transformation as a native C ownership graph. File-backed
readers retain 64-bit offsets until a checked host access is admitted. Tensor
storage follows qtype row geometry instead of flattened element counts.
Materialization records which allocator owns each byte range, and a backend
marks output state only after launch and synchronization have both succeeded.
The first accelerated target is **CUDA on the 128 GB NVIDIA GB10 in DGX
Spark**. Metal and ROCm remain separate backend programs because unified-memory
placement and device-memory placement do not have interchangeable lifetimes.

The v0.1.0 reference workload is the pinned
[DeepSeek-V4-Flash](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash)
snapshot. Its local source occupies **159,629,046,930 bytes** across **46
safetensors shards** and exposes **69,187 tensor records**. The model contains
**284B total parameters and activates 13B per token**; 256 routed experts
contribute six selected experts while one shared expert remains active. Expert
weights use FP4, most other parameters use FP8, and the declared context limit
is **1,048,576 tokens**. Hybrid CSA/HCA attention and
manifold-constrained hyper-connections make artifact capacity, active FLOPs,
expert traffic and attention-state storage four distinct resource equations.

DeepSeek-V4-Flash is the release workload, not the engine architecture. Common
owners are also exercised against dense Qwen, hybrid Qwen MoE and Gemma decoder
configurations. Family-specific execution enters through typed architecture
adapters rather than target-name branches. [`PROJECT.md`](PROJECT.md) owns the
current implementation state and release gates.

## Typed State Across the Inference Pipeline

Each inference phase consumes a typed result from the preceding owner. Source
verification establishes repository identity and an immutable shard inventory.
The architecture adapter converts structured configuration into layer and state
semantics. Tensor mapping binds those semantics to names, shapes and transforms;
the artifact encoder then assigns qtypes and byte-addressable storage. Runtime
execution is admissible only when every projection preserves its predecessor's
identity and failure state.

```text
pinned upstream snapshot
  -> typed architecture specification
  -> source-role-layout tensor map
  -> qtype assignment and complete GGUF encoding
  -> container, layout, and payload admission
  -> owned host/device tensor residency
  -> execution-complete runtime descriptor
  -> full-layer prefill and attention-derived KV
  -> KV-consuming decode, vocabulary logits, sampling
  -> tokenizer-defined text
```

These transitions are deliberately non-equivalent. Source identity does not
establish tensor-role completeness. Container addressability does not establish
payload correctness. Numerical agreement for an isolated CUDA kernel does not
establish that the operation consumes the release model's layout. The consumer
must re-evaluate the exact preconditions it owns instead of projecting a generic
ready bit from a lower stage.

C exposes these transitions at the representation boundary. File positions are
checked before conversion into address-space widths. Block qtypes require
`ne[0]` to divide the canonical block width, after which checked products derive
row count and tensor bytes. Device outputs remain unwritten until final context
synchronization, and failed cleanup preserves allocation state rather than
pretending that ownership was released.

Performance measurements inherit the same identity chain. Prefill latency is
meaningless without prompt geometry and the resident tensor set; decode latency
is meaningless without KV length and placement; kernel timing is meaningless
without dtype, layout and synchronization scope. YVEX therefore records the
state that defines a measurement before treating it as comparative evidence.

## Architecture-Specific Execution Semantics

Parameter count alone does not determine the runtime graph. Dense decoders fix
the FFN path but still vary head grouping, rotary subspace, normalization,
activation and output-head tying. Sparse decoders add a token-dependent expert
set. Hybrid attention introduces recurrent or compressed state whose geometry
cannot be represented as a conventional K/V tensor with a different length.
These differences alter required tensor roles, scratch lifetime and the state
carried between prefill and decode.

The repository uses several concrete configurations to keep those distinctions
visible. The values below come from the corresponding upstream model cards and
local structured configs; they are architecture inputs, not YVEX support claims.

| Decoder topology | Concrete configuration | Execution semantics | YVEX boundary |
| --- | --- | --- | --- |
| Dense GQA transformer | [Qwen3-8B](https://huggingface.co/Qwen/Qwen3-8B): 36 layers, `d_model=4096`, 32 query heads, 8 KV heads, `d_ff=12288`, 40,960-token context. | A fixed SwiGLU-style FFN path isolates GQA projection geometry, RoPE, residual ownership and an untied output head. | Source and tensor-profile evidence only. |
| Dense hybrid-attention decoder | [Gemma-4-31B](https://huggingface.co/google/gemma-4-31B): 30.7B parameters, 60 layers, five 1,024-token sliding layers per global layer, 256K context, 262K vocabulary. | Local attention and global attention use different RoPE and KV rules; the text decoder also ties token embeddings to the output head. | Dense/common mapping evidence only. |
| Hybrid MoE decoder | [Qwen3.6-35B-A3B](https://huggingface.co/Qwen/Qwen3.6-35B-A3B): 35B total, 3B active, 40 layers, three Gated DeltaNet blocks per full-attention block, 256 experts, top-8 plus one shared expert, 262,144-token native context. | Recurrent linear-attention state and conventional KV coexist with routed expert state; neither can be lowered through a dense-transformer compatibility path. | Header, naming-map and role-coverage evidence only. |
| Compressed-attention MoE decoder | [DeepSeek-V4-Flash](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash): 284B total, 13B active, 43 layers, 256 routed experts, top-6 plus one shared expert, 1M context. | CSA/HCA, mHC and mixed FP4/FP8 storage jointly determine position state, expert residency, dequantization and residual topology. | Exact source verified; execution-complete architecture IR active. |

The adapter boundary also has to accommodate topologies that are not current
YVEX targets. [Llama-style GQA](https://arxiv.org/abs/2407.21783),
[Mixtral top-2 sparse FFNs](https://arxiv.org/abs/2401.04088) and
[Mamba selective state-space blocks](https://arxiv.org/abs/2312.00752) have
different state and dispatch contracts even when their weight tensors fit
familiar matrix shapes. A future adapter must express those differences as
typed layer semantics; tensor-name aliases cannot make the common runtime
correct.

CPU remains the independent numerical reference. CUDA on GB10 is the first
accelerated implementation lane. Metal unified memory and ROCm device memory
must each establish allocation, synchronization and failure semantics against
the same operation contracts rather than inherit CUDA capability through an
abstract backend label.

## Artifacts as Executable Contracts

YVEX parses GGUF v3 against ggml commit
`af97976c7810cdabb1863172f31c432dab767de7`. The qtype registry at that revision
defines scalar width or block width plus bytes per block. For block storage,
`ne[0]` is the logical row width and must divide the block width exactly. The
checked product of all remaining dimensions gives row count; row bytes and row
count then give the raw tensor extent.

Global layout admission consumes that extent in directory order. The first
relative tensor offset is zero, each successor begins at the previous
power-of-two-aligned padded end, and required padding is zero-filled. The
validator derives the data-section span with checked 64-bit arithmetic and
rejects gaps, overlap, truncation or snapshot drift before any tensor payload is
read.

The file-backed reader stores only decoded metadata and tensor-directory state,
so structural memory is `O(metadata + tensor_count)` rather than `O(file_size)`.
Its resource budgets admit the current DeepSeek scale of 69,187 tensor entries
and a tokenizer vocabulary of 129,280 entries without quadratic duplicate
detection. Payload streaming later consumes the same immutable ranges instead
of reparsing the container through a second ownership path.

A tensor proof artifact closes a named parser, layout or primitive property over
a bounded tensor set. A complete model artifact additionally satisfies every
tensor role in one architecture specification. A supported model artifact has
also passed materialization, execution, generation and release gates. Valid
container bytes therefore establish only the lowest of these three contracts.

Weight sources, emitted GGUFs and registry identities remain operator-owned
state. Git contains only the engine and bounded synthetic fixtures needed to
reproduce structural and failure behavior. The complete ownership contract is
defined in
[`MODEL_ARTIFACTS.md`](MODEL_ARTIFACTS.md).

## Operational Projections of Internal State

CLI reports project typed domain state; they do not establish a second
capability model. Each command below terminates at one explicit admission
boundary.

| Boundary | Admitted invariant | Operational projection |
| --- | --- | --- |
| Source | Repository, revision, structured config, tokenizer, shard index and all referenced headers agree. | Strict verification returns a pinned inventory while leaving tensor payload bytes unread. |
| Container | GGUF records decode under bounded resource limits and every qtype-sized interval follows canonical padded order. | `inspect` and `integrity` project the same immutable reader and layout result. |
| Backend | Context creation, allocator operations, kernel-bundle admission, function resolution and exact dtype variant are distinct states. | `backend` reports the admitted matrix; an absent variant refuses before dispatch. |
| Primitive | Device output survives synchronization and agrees with an independent reference under the declared tolerance. | `graph check` exposes dispatch, checksums, maximum absolute difference and cleanup status. |
| Runtime | A complete descriptor binds admitted tensors to every family-specific layer and persistent state transition. | This projection remains unavailable until full prefill, attention-derived KV and KV-consuming decode compose. |

### Container and Primitive Proof

The bounded repository path requires no external model source:

```sh
make

./yvex inspect tests/fixtures/gguf/valid-metadata-tensors.gguf
./yvex integrity check \
  --model tests/fixtures/gguf/valid-metadata-tensors.gguf

./yvex backend cpu
./yvex graph check --suite primitives --backend cpu

make check
```

`inspect` and `integrity` consume one GGUF parse and one global layout result.
The CPU backend report projects its operation matrix, after which `graph check`
executes RoPE, causal attention, matrix multiplication and gated MLP fixtures
against independent references. These facts do not constitute a model graph.

### Pinned Source and CUDA Capability Admission

The release-source path operates over the pinned local snapshot:

```sh
SOURCE="$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash"

./yvex source-manifest report \
  --family deepseek \
  --release v0.1.0 \
  --target deepseek4-v4-flash \
  --source "$SOURCE" \
  --strict \
  --include-config \
  --include-blockers

./yvex native-weights --source "$SOURCE" --limit 20
./yvex backend cuda
```

Strict verification binds the source to repository
`deepseek-ai/DeepSeek-V4-Flash` at revision
`60d8d70770c6776ff598c94bb586a859a38244f1`, then reconciles 69,187 indexed
tensors against 46 readable headers in one scan. `native-weights` exposes the
header records without reading payload ranges. `backend cuda` independently
reports GB10 context state, generated PTX bundle admission and the exact F32/F16
primitive variants currently proven.

The complete DeepSeek GGUF, execution descriptor and transformer remain
unimplemented. [`PROJECT.md`](PROJECT.md) owns that dependency state.

## Build and Validation

The baseline target builds `libyvex.a`, `yvex` and `yvexd`, then runs CLI and
documentation contract checks:

```sh
make
make smoke
make check-docs
```

On the GB10 path, `nvcc` compiles `cuda_kernels.cu` into generated PTX and the
Driver API suite verifies module admission, symbol resolution, numerical parity
and failure rollback:

```sh
make check-cuda
```

The source ownership and validation contract lives in
[`AGENTS.md`](AGENTS.md). Executable operator procedures live in
[`docs/operator-runbook.md`](docs/operator-runbook.md).

## Specifications and Reference Implementations

The [GGUF specification](https://github.com/ggml-org/ggml/blob/af97976c7810cdabb1863172f31c432dab767de7/docs/gguf.md),
ggml type traits and llama.cpp conversion code define independent container,
qtype and transformation references. YVEX pins those sources at the behavior
being tested; production code does not link against ggml to calculate storage
geometry.

vLLM and SGLang provide comparative decompositions for loader, model-runner,
attention, KV and MoE ownership. TensorRT-LLM and CUTLASS provide Blackwell
kernel and low-precision layout references. The CUDA Driver API remains the
authority for context, module, function, launch and synchronization lifecycle.
None of their process models or support matrices is inherited by YVEX.

Model semantics come from the [DeepSeek-V4 technical
report](https://arxiv.org/abs/2606.19348), the [Qwen3 technical
report](https://arxiv.org/abs/2505.09388), the [Gemma-4 model
card](https://huggingface.co/google/gemma-4-31B), and their exact structured
configs. Llama, Mixtral and Mamba remain comparative architecture references
until an owned YVEX adapter consumes their semantics. The complete
reference-to-owner map is maintained in
[`docs/reference-architecture.md`](docs/reference-architecture.md).

## Documentation

[`PROJECT.md`](PROJECT.md) owns current state, dependencies and release gates.
[`docs/system-target.md`](docs/system-target.md) maps those responsibilities to
the source tree. [`docs/model-families.md`](docs/model-families.md) defines how
family adapters enter the common engine, while [`docs/api.md`](docs/api.md)
records the public C lifetime and error contracts. Release meaning belongs to
[`docs/v010-release-doctrine.md`](docs/v010-release-doctrine.md); none of these
documents duplicates the live project ledger.

## License

YVEX is licensed under the MIT license.
