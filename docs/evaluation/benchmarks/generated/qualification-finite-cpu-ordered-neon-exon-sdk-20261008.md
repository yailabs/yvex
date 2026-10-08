<!-- docs:metadata
title: "Finite CPU / installed Exon public SDK latency"
id: yvex.evaluation.qualification.finite-cpu-ordered-neon-exon-sdk-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/finite-cpu-ordered-neon-exon-sdk-20261008.json
-->

# Finite CPU / installed Exon public SDK latency

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/finite-cpu-ordered-neon-exon-sdk-20261008.json)

Target identity: `212587e5e2e8098fc00c32787e5577a1e7ac7d4bf45b497865f874511684e90e`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| compute.model-forward / upstream.reference | 2.24834 | s | 3 | 2.24358–2.3048 | 0.00475746 |
| request.client-complete / upstream.reference | 2.46192 | s | 3 | 2.45399–2.59244 | 0.00792944 |
| result.encoded-bytes / upstream.reference | 1978 | byte | 1 | 1978–1978 | 0 |
| compute.model-forward / product.context64 | 4.22626 | s | 3 | 4.2239–4.23231 | 0.00235792 |
| request.client-complete / product.context64 | 4.70242 | s | 3 | 4.49437–4.72862 | 0.0262056 |
| result.encoded-bytes / product.context64 | 1977 | byte | 1 | 1977–1977 | 0 |
| compute.model-forward / product.action | 2.52394 | s | 3 | 2.51017–2.53508 | 0.011134 |
| request.client-complete / product.action | 2.75274 | s | 3 | 2.74688–2.9683 | 0.00586348 |
| result.encoded-bytes / product.action | 1987 | byte | 1 | 1987–1987 | 0 |
| compute.model-forward / product.population8 | 3.19925 | s | 3 | 3.193–3.2514 | 0.00624368 |
| request.client-complete / product.population8 | 3.42476 | s | 3 | 3.41941–3.48934 | 0.00534862 |
| result.encoded-bytes / product.population8 | 2496 | byte | 1 | 2496–2496 | 0 |

## Diagnostic facts (not timed benchmark samples)

These observations explain this exact run; they do not qualify a throughput or quality claim.

| Fact / case | Value | Unit | Definition / evidence |
| --- | ---: | --- | --- |
| input-positions-upstream.reference / upstream.reference | 29 | count | source-policy rendered finite forward positions; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json |
| input-positions-product.context64 / product.context64 | 64 | count | source-policy rendered finite forward positions; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json |
| input-positions-product.action / product.action | 36 | count | source-policy rendered finite forward positions; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json |
| input-positions-product.population8 / product.population8 | 46 | count | source-policy rendered finite forward positions; /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| upstream.reference | PASS | All three SDK calls complete within 5 s, preserve lineage/generation/ordered IDs; fresh raw response <=65,536 bytes |
| product.context64 | PASS | All three SDK calls complete within 5 s, preserve lineage/generation/ordered IDs; fresh raw response <=65,536 bytes |
| product.action | PASS | All three SDK calls complete within 5 s, preserve lineage/generation/ordered IDs; fresh raw response <=65,536 bytes |
| product.population8 | PASS | All three SDK calls complete within 5 s, preserve lineage/generation/ordered IDs; fresh raw response <=65,536 bytes |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | CHARACTERIZED | Real complete CPU forward, one backbone, no sampling; generic independent ordered-F64 numerical and cancellation/refusal/cleanup controls qualified separately;  |
| checkpoint-reference | CHARACTERIZED | Retained independent upstream input/logit control, 1e-4 absolute tolerance; exact baseline score/result identity retained; not checkpoint-wide quality;  |
| deployment-performance | CHARACTERIZED | Twelve representative completed installed public SDK calls, each compute and Exon caller <=5 s; unchanged generation, no reload/retry; not a universal latency SLA or YAI semantic acceptance;  |
| family-conformance | UNQUALIFIED | Not established by this timing capture;  |
| product-path | CHARACTERIZED | Twelve representative completed installed public SDK calls, each compute and Exon caller <=5 s; unchanged generation, no reload/retry; not a universal latency SLA or YAI semantic acceptance;  |
| representation-quality | BLOCKED | Not established by this timing capture; No independent checkpoint-wide logits/NLL/quality corpus in this latency capture |

## Exact configuration

| Identity / setting | Value |
| --- | --- |
| family_contract | laya-typed-decisions / tensor-input token-type-dual-rope-v1 |
| upstream_repository | convaiinnovations/laya-typed-decisions |
| checkpoint | 1a793eb568e6718f15941d08f85432581df534e3 |
| tokenizer_conversation | 6c8aaa9a542084f2457eab775d4eeb51f92a70c0fd9de28d5edb0ddec3c08d30/8c3ecc1adba6928c3255b24f2d1eb0dd1a6843795473b8f3521cb25094e4c042 |
| transformation_ir | NOT RETAINED |
| physical_policy | tensor-binding-v1; no quantization transformation; mapped immutable F16 source |
| representation | F16 source / ordered F64 CPU F32 publication |
| artifact_set | 4fa56de72383a9d3efa9cfa78955733c81b9fc8067a587ca4beb82c78107a24e |
| binding | 5d1d4598c32c8317efbacfa3097cc4e630485cb9722e1f25e3fc3fe802fa1c41 |
| specialization | ff6f2d5c61e15657abbee3762fa2f0bdb71191f20437cce71afd7cc5d73b1228 |
| source_commit | 7fc562d5da05bba104d383266e72e3f8e075e404 |
| source_tree | 0f1794b4c783a6d10d455930f5dea3484fa180bf |
| source_delta | e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 |
| build | d22503df96b3ee7b0317cb0abc9cd7ce29891b8c5e1af4c8f2fc935344dc947b |
| executable | fce0ebc0f1e370d9dad12a635134c0d5857883cd935bbc54956f2eeea0bbc19d |
| backend | cpu |
| backend_implementation | source:7fc562d5da05bba104d383266e72e3f8e075e404/ordered-encoded-F32-CPU |
| kernel_bundle | not-applicable: CPU operations linked in authenticated executable |
| hardware_model | NVIDIA DGX Spark GB10 CPU; Cortex-X925/Cortex-A725 |
| device_count | 1 |
| topology | 1 host / one CPU forward at a time; no GPU execution |
| driver | Linux 6.17.0-1021-nvidia aarch64 |
| runtime_toolkit | native C CPU / ARM64 NEON; no GPU toolkit used |
| memory_configuration | shared GB10 system memory; OS scheduler; no exclusive machine reservation |
| runtime_configuration | resident finite engine; one forward/backbone, zero sampling/generation; generation=1 |
| context | 64 |
| prefill_geometry | not-applicable: complete bidirectional finite forward |
| sequence_geometry | one admitted input; 1..8 ordered candidates; rendered input <=64 positions |
| concurrency | 1 |
| strategy | not-applicable: finite-decision |
| reasoning | not-applicable |
| sampling | none |
| product_path | Exon public Rust SDK / restricted SSH JSONL v1 / DGX resident host native-v25 |
| suite | 4250adc32a22c5af6219d18db255859a7158117d1078c783feb44427db31af46 |

## Definitions and reproducibility

- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **result.encoded-bytes**: complete successful public JSONL response including LF, observed before SDK projection. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/public-controls-01/upstream.reference.capture.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **result.encoded-bytes**: complete successful public JSONL response including LF, observed before SDK projection. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/public-controls-01/product.context64.capture.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **result.encoded-bytes**: complete successful public JSONL response including LF, observed before SDK projection. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/public-controls-01/product.action.capture.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/sdk-window-01/samples.json.
- **result.encoded-bytes**: complete successful public JSONL response including LF, observed before SDK projection. Scope: Exon SDK process wall includes startup, SSH, response validation/projection; bytes measured on a separate fresh public JSONL call, not SDK reserialization. Session: stateless finite request; stable resident generation; warm/cold: resident CPU engine; DeepSeek resident idle; no sample-time reload or hidden warmup; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/public-controls-01/product.population8.capture.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Producer-boundary budget qualification for this exact checkpoint/CPU/corpus and approved Exon SDK path; not YAI Fast Search semantic acceptance or a universal 5-second SLA.
- Three timed SDK samples per case; one separately measured fresh public JSONL byte control per case. SDK receive bound 32,768 bytes is stricter than the 65,536-byte consumer limit.
- No Case prompts, indeterminate retry or sample-time reload. No transport/trust/grant/schema/numerical precision change.
- Only the retained upstream three-candidate control has independent logit evidence. No logits-wide/NLL/PPL/calibration or general model-quality claim.
- Transformation IR identity is not separately projected; remains unknown and prevents broad target qualification.
- Load was measured with cached source/pages; not cold loading. DeepSeek idle and resident; no simultaneous inference guarantee.
- These CPU changes do not establish the separate DeepSeek CUDA 20/700 throughput exit.
