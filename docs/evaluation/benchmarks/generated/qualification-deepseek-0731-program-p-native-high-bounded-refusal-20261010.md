<!-- docs:metadata
title: "DeepSeek 0731 Program P: bounded native high-reasoning refusal"
id: yvex.evaluation.qualification.deepseek-0731-program-p-native-high-bounded-refusal-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-native-high-bounded-refusal-20261010.json
-->

# DeepSeek 0731 Program P: bounded native high-reasoning refusal

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-native-high-bounded-refusal-20261010.json)

Target identity: `3f2a7bd6610817055fcfa7047909d048ec2d26f3a1841924b8f5e3fcdebbcd6f`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| turn.committed / coding.hash-table | 256 | count | Server generation.failed committed token population; not successful output; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-thinking-matrix/host.jsonl |
| turn.prompt / coding.hash-table | 110 | count | Rendered high-reasoning input token count, including source-authored policy; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-q2-native-thinking-matrix/host.jsonl |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| coding.hash-table | FAIL | 256-token limit reached before source reasoning delimiter; no retry; final transition not measured |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Native emission and pinned independent GGUF reader establish structure, not checkpoint-matched quality; Independent held-out comparison missing |
| backend-execution | CHARACTERIZED | One owned native high-reasoning turn stopped at output bound before its source delimiter; not successful completion or speed qualification;  |
| deployment-performance | CHARACTERIZED | One owned native high-reasoning turn stopped at output bound before its source delimiter; not successful completion or speed qualification;  |
| product-path | CHARACTERIZED | One owned native high-reasoning turn stopped at output bound before its source delimiter; not successful completion or speed qualification;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | 1e1d143a28c02e50a29deb8aa1123eb4d16866e8e2d348aa1d7081f1a9e18da2 |
| representation | goal-v1-q2_k; family-preserved roles unchanged |
| artifact_set | 7b33f67b79c6ad47c6a0b78aafac4131ad8d6ec27c162ebcf5b0a056670eb38f |
| binding | a4bb99c92e1f8d1fb61f7db3ed0fc5d7a900fcc87b7ecd6fd8c75c486a939737 |
| specialization | 22c47bdb6eebe27e01cb9e40571b52582324274365d4c266ef2034e3c7e5aa6e |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 34aeafbdb4800b7002cd8d464f13044d144e565096e2a70ec2bab659b1b9dab9 |
| build | df8ef064dcacc221bf01ea8f544caa4f1290d647566f2124a65131840736812e |
| executable | 412247b97bc36c8c17e209abccc9e2235415802044690945d29181ac8a9bc89c |
| backend | cuda |
| backend_implementation | CUDA Q2_K routed matrix; shared row matrix candidate |
| kernel_bundle | 43eb78da7f8beb3edfda0ee3cf12e18cc17db96808cafaba4b7d6d192ee4cea1 |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | isolated resident host; fresh session per sample after separate warmup; target-only; no prefix reuse |
| context | 4096 |
| prefill_geometry | chunk=512 |
| sequence_geometry | width=1 |
| concurrency | 1 |
| strategy | target-only |
| reasoning | high |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | product-native-v25 |
| suite | d5d9eb417abe63fc2d2c242317479cfc785ce46688e25cc7e936501111393f9d |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- High reasoning reached the explicit 256-output bound before its source delimiter. This is not the historical attention-state invalidation.
- No successful-turn performance measurement or final transition was earned; maximum was not executed after this refusal.
- No independent checkpoint-matched representation quality; successful component or earlier none-mode tests are not an upstream model oracle.
- The owned isolated host stopped and GPU list was empty; installed Host and services were unchanged.
