<!-- docs:metadata
title: "Finite CPU latency / finite-cpu-ordered-neon-native-20261008"
id: yvex.evaluation.qualification.finite-cpu-ordered-neon-native-20261008
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, evaluator, agent]
publication: {html: true, pdf: true, index: true}
generated: true
source: ../qualification/finite-cpu-ordered-neon-native-20261008.json
-->

# Finite CPU latency / finite-cpu-ordered-neon-native-20261008

[Benchmarks](../README.md) · [Methodology](../methodology.md)

[All targets](qualification-index.md) · [Machine receipt](../qualification/finite-cpu-ordered-neon-native-20261008.json)

Target identity: `e74c16659f1b05927c6f715360743550847a1fc72e5443054b3f0ef9ab415bb1`. Origin: **yvex-published**.

## Measurements

| Metric / exact case | Median | Unit | N | Min–max | MAD |
| --- | ---: | --- | ---: | --- | ---: |
| compute.model-forward / upstream.reference | 2.22527 | s | 3 | 2.22247–2.28974 | 0.00280855 |
| request.client-complete / upstream.reference | 2.22591 | s | 3 | 2.22311–2.29029 | 0.00280593 |
| compute.model-forward / product.context64 | 4.2076 | s | 3 | 4.18629–4.22639 | 0.018793 |
| request.client-complete / product.context64 | 4.20828 | s | 3 | 4.18696–4.22719 | 0.0189068 |
| compute.model-forward / product.action | 2.51746 | s | 3 | 2.50427–2.51787 | 0.000414068 |
| request.client-complete / product.action | 2.51793 | s | 3 | 2.505–2.5185 | 0.000573491 |
| compute.model-forward / product.population8 | 3.2066 | s | 3 | 3.19749–3.21395 | 0.00734851 |
| request.client-complete / product.population8 | 3.20703 | s | 3 | 3.19808–3.21452 | 0.0074836 |

## Request outcomes (not performance samples)

| Case | Result | Reason |
| --- | --- | --- |
| upstream.reference | PASS | Full result, ordered IDs, exact lineage, zero generation/sampling |
| product.context64 | PASS | Full result, ordered IDs, exact lineage, zero generation/sampling |
| product.action | PASS | Full result, ordered IDs, exact lineage, zero generation/sampling |
| product.population8 | PASS | Full result, ordered IDs, exact lineage, zero generation/sampling |

## Claim boundaries

| Plane | State | Exact scope / missing gate |
| --- | --- | --- |
| backend-execution | UNQUALIFIED | Not established by this timing capture;  |
| checkpoint-reference | CHARACTERIZED | One retained independent upstream three-candidate input/logit control, absolute tolerance 1e-4; not checkpoint-wide quality;  |
| deployment-performance | CHARACTERIZED | Repeated isolated resident CPU complete-model execution; not installed Exon SDK latency;  |
| family-conformance | UNQUALIFIED | Not established by this timing capture;  |
| product-path | CHARACTERIZED | Repeated isolated resident CPU complete-model execution; not installed Exon SDK latency;  |
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
| source_commit | f1c47701528dc5d4f8a2ad5f8dd04daea3230939 |
| source_tree | d7f2ee106fdf85e1ddc137793c66cddb70b6b30b |
| source_delta | fbb0d53f496f96848e7c384f8420c6af10bd3e6f3541c2de6d9f2ab1bcb9cfa0 |
| build | 29c8747b3c748f5992fd8ded2b93173cad25882f188d8bb18050ddd6a73e4686 |
| executable | 23f7ef8ba0192f76667db9d0f13b217ee306193c9f91e4da72f6bd432683bbc8 |
| backend | cpu |
| backend_implementation | sha256:fbb0d53f496f96848e7c384f8420c6af10bd3e6f3541c2de6d9f2ab1bcb9cfa0 |
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
| product_path | public C local producer / native-v25 |
| suite | 4250adc32a22c5af6219d18db255859a7158117d1078c783feb44427db31af46 |

## Definitions and reproducibility

- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **compute.model-forward**: server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.
- **request.client-complete**: client dispatch including connect through terminal response. Scope: Full model forward or local C caller as named; no remote qualification. Session: stateless finite request; stable engine generation; warm/cold: resident engine; no hidden warmup or sample-time reload; output bound: 65536. Evidence: /home/dgmothx/lab/models/evidence/deepseek-competitive-20261005.oO7lAU/full-system-20261008.k2BN6u/finite-candidate-neon-native-01/samples.json.

No confidence interval is inferred from small sample counts. The machine receipt retains samples, prompt/reference identities and provenance.

## Non-claims

- Isolated native local CPU characterization, not installed Exon/YAI SDK or Fast Search acceptance.
- No Case content or indeterminate replay. Three samples per workload are not a universal 5 s SLA.
- Source and result identities are unchanged; scores remain uncalibrated. One upstream fixture does not establish general model quality.
- Transformation IR identity is not separately projected by the source/binding capture and remains unknown.
- No CPU affinity or uninterrupted hardware reservation. File mapping, parameter execution and workspace bytes overlap and must not be added blindly.
