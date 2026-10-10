<!-- docs:metadata
title: Compiler Evidence
id: yvex.evaluation.compiler-evidence
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Compiler Evidence

**How executable-composition evidence bounds a compilation claim.**

[Up](README.md)

## Executable composition oracle

The CPU-only tiny vertical is the fast composition oracle for this pipeline. Its focused test
owner deterministically generates an untracked GGUF, admits it through the production artifact
contract, compiles the semantic model, operator graph, Physical Execution IR and runtime binding,
then launches the real foreground host with zero engines, loads the fixture,
serves it, unloads it without stopping the host, reloads it as a new generation,
and admits two fitting engines concurrently. The production `host status`,
`engine list`, `engine load`, native generation, `engine unload`, `host logs`, and
`host stop` paths must return the expected context, text, identities, typed
completion event, routing refusals, and clean lifecycle. A second build must
reproduce artifact and binding identities, while a corrupted artifact must
refuse without terminating the host.

The fixture adds no production model family and does not establish support, quality, CUDA or
performance for a real model. Its generated artifact and binding remain temporary build evidence,
never repository authority.

## Program P: profile resolution and bounded allocation

The profile-driven continuation is **partial implementation**, not the competitive
exit of `V010.PHYSICAL.MODEL.COMPILER.GOAL.DRIVEN.0`. The canonical records below
bind the observed source delta, binary, requests, planned representations and
refusals. They contain no new inference throughput measurements.

| Subject | Earned evidence | Explicit boundary |
| --- | --- | --- |
| [DeepSeek 0731 allocation](benchmarks/generated/qualification-deepseek-0731-program-p-resolved-profiles-20261010.md) | Source-derived profile, bounded role frontier, exported policy, canonical plan and one internal source-preserving tensor probe | No new complete artifact, binding, model execution or independent quality claim |
| [Mamba2 profile/CLI](benchmarks/generated/qualification-mamba2-program-p-resolved-profiles-20261010.md) | Real catalog-guided terminal journey and replay; sequence-mixer geometry retained; unavailable allocation and Metal model execution refused | Prior Mamba execution proof remains separate; no new quantization or performance claim |
| Exact allocation algorithm | Exhaustive enumeration independently checks every frontier of 64 bounded test problems; ties, overflow, insufficient budget and state/output exhaustion are controlled | The optimized objective is source-retained elements under encoded bytes, not an estimated quality score |
| Receipt admission | Exact embedded-publication equality precedes automatic comparison; forged external numbers remain untrusted inspection | Missing kernel identity, changed workload/strategy and incompatible measurement bases refuse; no automatic qualified recommendation |
| Guided refusal/recovery | Real PTY cancellation, unsupported schema, source verification failure and request replay controls | A compiler target is not assumed to be a catalog alias; failed verification cannot publish an artifact or saved request |

The test owners are [optimization](../../tests/unit/optimization.c),
[Rust CLI contracts](../../tests/rust_cli_contracts.py),
[installed ABI](../../tests/test_public_abi.py) and the existing official
[tokenizer oracle](../../tests/reference/tokenizer.py). Source-stable raw controls,
source archives and software logs are referenced by the structured records.
Official encoding/BPE parity does not establish full-model logits or continuation.

### Reproduce the product path

```sh
./yvex compile optimize --list-techniques
./yvex compile optimize --guided --out-request goals.json
./yvex compile optimize --request goals.json --json
./yvex compile optimize --request goals.json --select <candidate-id> --out-policy recipe.json
```

The guided surface selects an acquired source through the native catalog. If its
canonical authenticated manifest is absent, it asks before invoking native source
verification; that operation may read all shards and retains its verified
manifest. Cancellation before verification changes no source state. A saved
request is reproducible intent, not proof of feasibility or a production receipt.

Request v2 may select `source-retention` with an explicit `weight_budget` and
bounded `search_states`. The exported policy feeds the existing `compile quant
plan`, `probe` and `emit --binding-directory` owners. See the
[compiler contract and examples](../architecture/compiler-ir.md#reproducible-engineering-workflow).
No step installs an engine. The ordinary guided surface currently stops at
candidate assessment; integrated guided production/qualification, pre-production
full workspace/state costing and evidence-backed recommendation remain open.

### Computational non-claim

The operator's previous DSpark remained resident during these offline controls.
The new role-allocation recipe was **not** run as a complete model. Existing
[DeepSeek performance records](benchmarks/generated/qualification-families.md)
remain historical, separately identified observations. In particular, the retained
MXFP4 publication records lack a kernel-bundle identity: the new comparison gate
does not fill that gap from the current build or rank those rows automatically.
Independent held-out quality and the 20/700 performance gates remain unearned.
