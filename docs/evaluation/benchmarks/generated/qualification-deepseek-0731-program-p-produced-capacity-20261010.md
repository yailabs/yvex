<!-- docs:metadata
title: "Program P: exact produced-binding capacity inspection without engine loading"
id: yvex.evaluation.qualification.deepseek-0731-program-p-produced-capacity-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-produced-capacity-20261010.json
-->

# Program P: exact produced-binding capacity inspection without engine loading

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-produced-capacity-20261010.json)

Target identity: `01bfb254a9b8c984f0a748d5c9113686d35d2618a866da8872e5cf9a31acc76a`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| capacity.q2-target.required_peak_bytes / q2-target | 1.22079459e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q2-target.available_bytes / q2-target | 1.25351649e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q8-target.required_peak_bytes / q8-target | 1.26708988e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q8-target.available_bytes / q8-target | 1.25276873e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q2-speculative.required_peak_bytes / q2-speculative | 1.22312931e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q2-speculative.available_bytes / q2-speculative | 1.25123256e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |
| capacity.q2-small-budget.available_bytes / q2-small-budget | 1.25465371e+11 | byte | Runtime-authored capacity inspection; requirement is planned, availability sampled, not physical working memory; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/produced-capacity-controls/receipt.json |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not executed by this capacity inspection;  |
| checkpoint-reference | BLOCKED | Capacity inspection supplies no independent model or quality oracle; Independent checkpoint-matched evidence unavailable in this inspection |
| representation-quality | BLOCKED | Capacity inspection supplies no independent model or quality oracle; Independent checkpoint-matched evidence unavailable in this inspection |
| backend-execution | UNQUALIFIED | Not executed by this capacity inspection;  |
| deployment-performance | UNQUALIFIED | Not executed by this capacity inspection;  |
| product-path | CHARACTERIZED | Compiler CLI projects canonical runtime capacity with exact variant matching; no model loaded;  |

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
| specialization | NOT RETAINED |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | e2814df8a1309941578fce89d5fa461c7f43ac40bde2ac0a773a40968c70853b |
| build | 06bc0577bc4c8ad7f3eaf85ad6816c778a495d427ec646d7d4267947f111cf0a |
| executable | fcccb138f4de3fc7d4746b10cf4b9f1cfbfeaee545dbb18bea6ba32b6a3d5c54 |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | capacity preflight; no weights or session; canonical runtime reserve |
| context | 4096 |
| prefill_geometry | requested chunk 512; no prefill execution |
| sequence_geometry | requested width 1; no session created |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | {"min_p":0.0,"seed":null,"seed_present":0,"stochastic":0,"temperature":0.0,"top_k":0,"top_p":1.0,"typical_p":1.0} |
| product_path | native Rust compiler CLI; not inference |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Passing capacity does not authenticate current artifact bytes, reserve memory, create an engine or qualify execution.
- No throughput or load-latency measurement; concurrent offline artifact emission was observed.
- The Q8 control is a different representation, explicitly identified in the raw control record; not a same-representation speed comparison.
- An early insufficient-budget refusal has no sealed plan or required-peak value; these remain null, not zero.
- Detailed plan categories may overlap; the canonical required total, not a sum of diagnostic fields, owns admission.
- Custom reserve inspection is explicitly refused; normal runtime reserve applies. Selection and independent quality remain unearned.
