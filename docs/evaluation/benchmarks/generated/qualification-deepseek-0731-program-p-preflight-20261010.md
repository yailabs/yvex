<!-- docs:metadata
title: "Program P: 0731 post-reboot full preflight refuses; no inference result"
id: yvex.evaluation.qualification.deepseek-0731-program-p-preflight-20261010
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/deepseek-0731-program-p-preflight-20261010.json
-->

# Program P: 0731 post-reboot full preflight refuses; no inference result

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/deepseek-0731-program-p-preflight-20261010.json)

Target identity: `7ac82ea30ffa328646938969b0db79b437c48c94c980240f5e846b89a29f92aa`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |

No quality or performance metric is published for this target.

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| admission.initial-lower-bound / postboot-512 | 1.24458152e+11 | byte | Encoded payload plus largest tensor transient and reserve only; excludes full workspace/state; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/deepseek-search.json |
| admission.full-required / postboot-512 | 1.26708988e+11 | byte | Canonical runtime pre-residency requirement for requested context 4096 and chunk 512; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-postboot-admission/stderr.log |
| admission.available / postboot-512 | 1.25732471e+11 | byte | Available capacity observed at the failed full preflight; not a reservation; /home/dgmothx/lab/models/evidence/physical-compiler-20261010.sQYLQOVq/0731-postboot-admission/stderr.log |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| family-conformance | UNQUALIFIED | Not rerun in this resource probe; prior exact-checkpoint encoding evidence remains separate;  |
| checkpoint-reference | BLOCKED | No independent full-model 0731 reference supplied by this probe; Checkpoint-matched independent inference evidence missing |
| representation-quality | BLOCKED | Artifact identity and byte feasibility are not numerical quality; Independent held-out comparison missing |
| backend-execution | BLOCKED | Full capacity check refuses before residency; no model forward; Full pre-residency envelope exceeds observed available memory; no reserve relaxation |
| deployment-performance | BLOCKED | Refusal is not a zero-rate performance sample; No admitted model or committed generation |
| product-path | UNQUALIFIED | No native protocol or HTTP inference measurement; installed Host untouched;  |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | deepseek-v4-flash-dspark |
| upstream_repository | deepseek-ai/DeepSeek-V4-Flash-0731 |
| checkpoint | 7872f01b1d1fe23eabc4c98b48bffcef5a386062 |
| tokenizer_conversation | bd23dc1b6acfa3f74426cf6832be8a33f4375f1538c96214947ea645ddfd3819 |
| transformation_ir | ad1dffd6da2e85126811eb74f0c02665cc5a40c781fdb51dd3d7b1da59ec064d |
| physical_policy | d66294b5e016806f2e283e8b0160e25385302fa6c9db4ac24a49e94140fa0ea9 |
| representation | deepseek-v4-flash-0731-q8_0-q2_k-v1 |
| artifact_set | 4dc4265a92d77c874c82aa16c1358688b71bb7b10267cc80911c3ef42d2fa11a |
| binding | 29676295361a8c99b86b3e1837a64e23791337f862d01fbeaeb2b5361399cc6d |
| specialization | NOT RETAINED |
| source_commit | 0f5a54b2aeccce0a67f98eb4e898072bb35cca95 |
| source_tree | bf4ee5f5f8f757b50bcedcba19e6d82f16faafd6 |
| source_delta | 169496be38fa8e1e8dd187d859613705919ea4db2ca96064ff7bd705aad8bb73 |
| build | dd88d1a5ec632d5f9365920a515b079baf77e607be00eb9c42b90f98846b9294 |
| executable | 4368d9461e4b243868d2fa92059d2141574571173424cb296c955c2d70a3987f |
| backend | cuda |
| backend_implementation | NOT RETAINED |
| kernel_bundle | NOT RETAINED |
| hardware_model | NVIDIA DGX Spark GB10 |
| device_count | 1 |
| topology | single GB10 coherent unified memory |
| driver | 580.159.03 |
| runtime_toolkit | CUDA 13.0; nvcc 13.0.88 |
| memory_configuration | 130663165952 system bytes; mandatory capacity/8 reserve |
| runtime_configuration | requested 4096 context and 8 output tokens; pre-residency refusal |
| context | 4096 |
| prefill_geometry | requested chunk 512; no input positions executed |
| sequence_geometry | requested width 1; fresh isolated execution |
| concurrency | 1 |
| strategy | target-only |
| reasoning | none |
| sampling | greedy; temperature 1; no stochastic draws |
| product_path | direct native engineering generation; not resident-host/native-protocol performance |
| suite | NOT RETAINED |

## Definitions and reproducibility


No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- No successful 0731 native forward, timing, model-quality claim or 20/700 exit.
- Post-reboot service population differs from historical pre-reboot observations; memory recovery is not a model optimization.
- No installed model, listener or service was restarted or replaced; Host remained ready with zero engines.
- The independent 64-token chunk diagnostic has a separate raw receipt and is not combined into this target.
- Static search records unknown workspace/state explicitly; passing its initial lower bound does not promise full runtime fit.
- Candidate preparation and future measured ranking remain separate from this refused baseline. No hardware throughput ceiling is established.
