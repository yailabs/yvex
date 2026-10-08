#!/usr/bin/env python3
"""Canonical qualification identity, claims and comparability (not runtime admission).

Runtime benchmark v5 and evaluation observations remain producer evidence. This
envelope references them; it never turns execution, publication or a local receipt
into a broader support claim. No family-specific implementation belongs here.
"""
import hashlib
import json
import math
import statistics

TARGET_SCHEMA = "yvex.qualification.target.v1"
RECEIPT_SCHEMA = "yvex.qualification.receipt.v1"
RECEIPT_FIELDS = ("schema", "id", "title", "target", "target_identity", "origin", "claims", "measurements", "provenance", "limitations")
CLAIM_FIELDS = ("state", "scope", "required_evidence", "evidence", "blockers")
OUTCOME_FIELDS = ("case", "result", "reason", "evidence")
OUTCOME_RESULTS = ("PASS", "FAIL", "CANCELLED", "INDETERMINATE")
METRIC_FIELDS = ("metric", "definition", "unit", "case", "prompt_identity", "reference_identity", "session_state", "warm_state", "output_bound", "samples", "statistics", "scope", "evidence")
DIAGNOSTIC_FIELDS = ("id", "case", "value", "unit", "definition", "evidence")
DIAGNOSTIC_UNITS = ("byte", "count", "s")
PLANES = ("family-conformance", "checkpoint-reference", "representation-quality",
          "backend-execution", "deployment-performance", "product-path")
STATES = ("QUALIFIED", "CHARACTERIZED", "BLOCKED", "UNQUALIFIED", "UNSUPPORTED")
# These reference canonical producer identities; they do not reconstruct lineage.
# Null explicitly means missing provenance and prevents qualification/comparison.
TARGET_FIELDS = {
    "family_contract": "string", "upstream_repository": "string", "checkpoint": "string",
    "tokenizer_conversation": "string", "transformation_ir": "string", "physical_policy": "string",
    "representation": "string", "artifact_set": "string", "binding": "string", "specialization": "string",
    "source_commit": "string", "source_tree": "string", "source_delta": "string", "build": "string",
    "executable": "string", "backend": "string", "backend_implementation": "string",
    "kernel_bundle": "string", "hardware_model": "string", "device_count": "integer",
    "topology": "string", "driver": "string", "runtime_toolkit": "string", "memory_configuration": "string",
    "runtime_configuration": "string", "context": "integer", "prefill_geometry": "string",
    "sequence_geometry": "string", "concurrency": "integer", "strategy": "string",
    "reasoning": "string", "sampling": "string", "product_path": "string", "suite": "string",
}
QUALITY_KEY = ("family_contract", "upstream_repository", "checkpoint", "tokenizer_conversation", "suite")
# Changes require an explicit experiment. Checkpoint/workload/metric changes never
# become a representation regression test, even if the caller asks to ignore them.
QUALITY_VARIANTS = set(TARGET_FIELDS) - set(QUALITY_KEY)
METRICS = {
    "compute.model-forward": ("deployment-performance", "s", "server tensor-engine forward execution wall; excludes input admission/tokenization, result scoring/sealing and transport"),
    "result.encoded-bytes": ("product-path", "byte", "complete successful public JSONL response including LF, observed before SDK projection"),
    "admission.client": ("product-path", "s", "client dispatch including connect to native TURN_STARTED acknowledgement; not pure server setup time"),
    "prefill.wall": ("deployment-performance", "s", "server-authored complete newly executed prefill wall time"),
    "decode.post-first.committed": ("deployment-performance", "token/s", "committed tokens after first / elapsed decode wall after first"),
    "prefill.uncached": ("deployment-performance", "token/s", "newly committed uncached input positions / complete prefill wall"),
    "ttft.server": ("deployment-performance", "s", "turn start to first committed model-token callback"),
    "ttft.client-visible": ("product-path", "s", "client dispatch including connect to first nonempty final/reasoning content"),
    "reasoning.first.server": ("deployment-performance", "s", "server turn start to first source-classified reasoning token"),
    "reasoning.first.client": ("product-path", "s", "client dispatch including connect to first nonempty reasoning fragment"),
    "final.first.server": ("deployment-performance", "s", "server turn start to first source-classified final token"),
    "final.first.client": ("product-path", "s", "client dispatch including connect to first nonempty final fragment"),
    "reasoning.phase-rate": ("deployment-performance", "token/s", "source-classified reasoning tokens / server phase from prefill completion to reasoning boundary or decode end; not post-first sustained rate"),
    "final.phase-rate": ("deployment-performance", "token/s", "source-classified final tokens / server phase from reasoning boundary or prefill completion to decode end; not post-first sustained rate"),
    "request.client-complete": ("product-path", "s", "client dispatch including connect through terminal response"),
    "load.complete": ("deployment-performance", "s", "load admission to execution-ready engine"),
    "load.residency": ("deployment-performance", "s", "server-authored residency phase wall"),
    "memory.rss-peak": ("deployment-performance", "byte", "observed process peak RSS; not device residency"),
    "nll.teacher-forced": ("representation-quality", "nat/token", "mean negative log probability of the same teacher-forced continuation"),
    "perplexity": ("representation-quality", "ratio", "exp(mean teacher-forced NLL) on exact matching positions"),
    "kl.reference-candidate": ("representation-quality", "nat", "mean full-vocabulary KL(reference || candidate) on the same contexts"),
    "probability-delta.rms": ("representation-quality", "probability", "RMS candidate minus reference probability over full vocabulary and positions"),
    "same-top-token": ("representation-quality", "fraction", "fraction of identical full-vocabulary argmax tokens, same contexts"),
    "greedy-prefix": ("checkpoint-reference", "token", "matching continuation prefix under the declared exact comparison policy"),
}
# Publication eligibility, not runtime admission or a universal sustained-rate
# guarantee. Both native consumers use this generated population contract.
METRIC_ADMISSION = {
    "decode.post-first.committed": {"post_first_decode_units": {"minimum": 32}},
    "prefill.uncached": {"reused_tokens": {"maximum": 0}, "prefill_tokens": {"minimum": 1}},
}


def metric_observation_admitted(metric, observation):
    """Missing/invalid populations cannot become sustained or uncached samples."""
    for field, bounds in METRIC_ADMISSION.get(metric, {}).items():
        value = observation.get(field)
        if type(value) is not int or value < 0:
            return False
        if "minimum" in bounds and value < bounds["minimum"]:
            return False
        if "maximum" in bounds and value > bounds["maximum"]:
            return False
    return metric in METRICS


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False, allow_nan=False)


def identity(value):
    return hashlib.sha256(canonical(value).encode()).hexdigest()


def require(test, message):
    if not test:
        raise ValueError(message)


def target_id(target):
    require(set(target) == {"schema", *TARGET_FIELDS}, "unknown/missing target fields")
    require(target["schema"] == TARGET_SCHEMA, "unsupported target schema")
    for key, kind in TARGET_FIELDS.items():
        value = target[key]
        if value is None:
            continue
        require((type(value) is int and value > 0) if kind == "integer"
                else isinstance(value, str) and bool(value.strip()), "invalid target field: " + key)
    # Local paths, PIDs, timestamps and observed clock readings have no fields
    # here. They belong to receipt provenance, not semantic target identity.
    return identity(target)


def statistics_for(samples):
    require(isinstance(samples, list) and bool(samples), "samples unavailable")
    require(all(type(x) in (int, float) and math.isfinite(x) for x in samples), "non-finite sample")
    median = statistics.median(samples)
    return {"count": len(samples), "median": median, "minimum": min(samples), "maximum": max(samples),
            "median_absolute_deviation": statistics.median(abs(x - median) for x in samples)}


def validate(receipt):
    require(set(receipt) == set(RECEIPT_FIELDS), "invalid receipt fields")
    require(receipt["schema"] == RECEIPT_SCHEMA, "unsupported receipt schema")
    target = receipt["target"]
    require(receipt["target_identity"] == target_id(target), "target identity mismatch")
    require(receipt["origin"] in ("local", "yvex-published"), "invalid publication origin")
    import re
    require(re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", receipt["id"]) is not None, "invalid receipt ID")
    require(isinstance(receipt["title"], str) and bool(receipt["title"]), "missing title")
    require(set(receipt["claims"]) == set(PLANES), "independent evidence planes required")
    require(isinstance(receipt["limitations"], list) and bool(receipt["limitations"]), "non-claims required")
    provenance = receipt["provenance"]
    require(isinstance(provenance, dict) and provenance.get("source_stability") in ("frozen", "unknown", "fixture"), "source stability required")
    diagnostics = provenance.get("diagnostics", [])
    require(isinstance(diagnostics, list) and len(diagnostics) <= 256, "invalid diagnostic extent")
    diagnostic_ids = set()
    for fact in diagnostics:
        require(isinstance(fact, dict) and set(fact) == set(DIAGNOSTIC_FIELDS), "invalid diagnostic fields")
        require(all(isinstance(fact[k], str) and fact[k].strip() for k in ("id", "case", "definition", "evidence")),
                "diagnostic context missing")
        require(fact["id"] not in diagnostic_ids, "duplicate diagnostic identity")
        diagnostic_ids.add(fact["id"])
        require(fact["unit"] in DIAGNOSTIC_UNITS, "unregistered diagnostic unit")
        value = fact["value"]
        require(value is None or type(value) in (int, float) and math.isfinite(value) and value >= 0,
                "invalid diagnostic value")
    outcomes = provenance.get("case_outcomes", [])
    require(isinstance(outcomes, list), "case outcomes must be a list")
    for outcome in outcomes:
        require(set(outcome) == set(OUTCOME_FIELDS), "invalid case outcome")
        require(outcome["result"] in OUTCOME_RESULTS, "invalid case result")
        require(all(isinstance(outcome[k], str) and outcome[k] for k in ("case", "reason", "evidence")),
                "case outcome context missing")
    continuations = provenance.get("continuation_comparisons", [])
    require(isinstance(continuations, list) and len(continuations) <= 64, "invalid continuation extent")
    seen = set()
    for row in continuations:
        key = (row.get("case"), row.get("input_identity"))
        require(all(isinstance(v, str) and v for v in key) and key not in seen, "duplicate/invalid continuation")
        seen.add(key)
        result = row.get("comparison", {})
        require(type(row.get("prompt_tokens")) is int and row["prompt_tokens"] > 0
                and type(row.get("maximum_output")) is int and 0 < row["maximum_output"] <= 4096,
                "continuation input/output bounds missing")
        require(result.get("state") == "CHARACTERIZED" and result.get("exact_input_tokens") is True,
                "continuation observation cannot promote numerical conformance")
        require(type(result.get("first_token_agreement")) is bool
                and type(result.get("exact_bounded_continuation")) is bool, "invalid continuation agreement")
        for field in ("reference_sampled_tokens", "candidate_sampled_tokens", "candidate_committed_tokens", "greedy_prefix_tokens"):
            require(type(result.get(field)) is int and result[field] >= 0, "invalid continuation population")
        require(0 <= result["greedy_prefix_tokens"] <= min(result["reference_sampled_tokens"], result["candidate_sampled_tokens"])
                and 0 < result["reference_sampled_tokens"] <= row["maximum_output"]
                and 0 < result["candidate_sampled_tokens"] <= row["maximum_output"]
                and result["candidate_committed_tokens"] <= result["candidate_sampled_tokens"],
                "continuation population exceeds executed work")
        require(result.get("comparison_policy") and result.get("unavailable_metrics"), "continuation authority/non-claims missing")
        require(all(isinstance(row.get(k), str) and re.fullmatch(r"[a-f0-9]{64}", row[k])
                    for k in ("candidate_sha256", "reference_capture_sha256")), "continuation raw identity missing")
        require(row.get("evidence"), "continuation evidence missing")
    for plane, claim in receipt["claims"].items():
        require(set(claim) == set(CLAIM_FIELDS), "invalid claim shape")
        require(claim["state"] in STATES and bool(claim["scope"]), "invalid claim state/scope")
        require(isinstance(claim["required_evidence"], list) and isinstance(claim["evidence"], dict), "invalid evidence set")
        for evidence in claim["evidence"].values():
            require(set(evidence) == {"sha256", "locator", "result"}, "invalid evidence reference")
            require(re.fullmatch(r"[a-f0-9]{64}", evidence["sha256"]) is not None, "invalid evidence digest")
            require(evidence["result"] in ("PASS", "FAIL", "BLOCKED", "SKIP", "ERROR"), "invalid evidence result")
            require(bool(evidence["locator"]), "missing evidence locator")
        if claim["state"] == "QUALIFIED":
            require(bool(claim["required_evidence"]) and not claim["blockers"], "qualification gate missing/blocked")
            require(all(claim["evidence"].get(k, {}).get("result") == "PASS" for k in claim["required_evidence"]), "required evidence not PASS")
            keys = QUALITY_KEY if plane == "family-conformance" else TARGET_FIELDS
            require(all(target[k] is not None for k in keys), "qualified claim lacks target identity")
            require(provenance["source_stability"] == "frozen", "qualification requires stable source")
            require(provenance.get("evidence_class") != "fixture", "fixture is not qualification")
        if claim["state"] == "BLOCKED":
            require(bool(claim["blockers"]), "BLOCKED requires exact missing prerequisite")
        if claim["state"] == "QUALIFIED" and plane in ("deployment-performance", "product-path"):
            require(any(METRICS.get(m.get("metric"), (None,))[0] == plane
                        for m in receipt["measurements"]), "qualified performance requires measurements")
    for metric in receipt["measurements"]:
        require(set(metric) == set(METRIC_FIELDS), "invalid metric fields")
        require(metric["metric"] in METRICS, "unregistered metric definition")
        plane, unit, definition = METRICS[metric["metric"]]
        require((metric["unit"], metric["definition"]) == (unit, definition), "metric denominator/definition changed")
        require(metric["statistics"] == statistics_for(metric["samples"]), "statistics differ from samples")
        require(all(metric[k] for k in ("case", "prompt_identity", "session_state", "warm_state", "scope", "evidence")), "measurement context missing")
        if receipt["claims"][plane]["state"] == "QUALIFIED":
            require(len(metric["samples"]) >= 3 if plane in ("deployment-performance", "product-path") else True, "qualification needs repeated samples")
            if plane in ("deployment-performance", "product-path"):
                require(provenance.get("profiled") is False, "profiled/unknown runs cannot qualify performance")
    return receipt


def comparison(left, lm, right, rm, varying=()):
    validate(left); validate(right)
    require(lm in left["measurements"] and rm in right["measurements"], "metric not owned by receipt")
    metric_fields = ("metric", "definition", "unit", "case", "prompt_identity", "reference_identity",
                     "session_state", "warm_state", "output_bound")
    require(all(lm[k] == rm[k] for k in metric_fields), "incompatible metric/workload/reference/denominator")
    quality = METRICS[lm["metric"]][0] in ("representation-quality", "checkpoint-reference")
    require(quality or left["provenance"].get("profiled") is False and right["provenance"].get("profiled") is False,
            "performance comparison requires explicitly unprofiled samples")
    require(not quality or isinstance(lm["reference_identity"], str) and bool(lm["reference_identity"].strip()),
            "quality comparison lacks independent reference identity")
    required = QUALITY_KEY if quality else TARGET_FIELDS
    for key in required:
        require(left["target"][key] is not None and right["target"][key] is not None, "comparison lacks " + key)
    differences = [key for key in TARGET_FIELDS if left["target"][key] != right["target"][key]]
    require(set(varying) <= set(TARGET_FIELDS), "unknown experiment axis")
    require(not quality or not (set(varying) & set(QUALITY_KEY)), "checkpoint/reference suite may not vary in quality regression")
    require(not (set(differences) - set(varying)), "incompatible targets: " + ", ".join(differences))
    return {"kind": "explicit-experiment" if differences else "direct", "varying": differences,
            "left": left["target_identity"], "right": right["target_identity"],
            "automatic_ranking": False}


def probability_quality(reference, candidate, expected):
    """Full distributions only; top-k slices are not renormalized into fake KL.

    All rows must be teacher-forced on identical contexts by the caller's exact
    checkpoint/suite gate. These statistics are not family-independent thresholds.
    """
    require(len(reference) == len(candidate) == len(expected) and bool(expected), "quality position mismatch")
    kl, deltas, nll, same, square, cells = [], [], [], [], 0.0, 0
    for r, c, token in zip(reference, candidate, expected):
        require(len(r) == len(c) and len(r) > 1 and type(token) is int and 0 <= token < len(r), "vocabulary mismatch")
        for row in (r, c):
            require(all(type(x) in (float, int) and math.isfinite(x) and x >= 0 for x in row), "invalid probability")
            require(abs(sum(row) - 1) <= 1e-8, "full normalized distribution required")
        # Zero candidate support cannot be hidden by epsilon smoothing.
        require(all(q > 0 for p, q in zip(r, c) if p > 0) and c[token] > 0, "zero support: KL/NLL unbounded")
        kl.append(sum(p * math.log(p / q) for p, q in zip(r, c) if p))
        deltas.append(c[token] - r[token]); nll.append(-math.log(c[token]))
        same.append(max(range(len(r)), key=r.__getitem__) == max(range(len(c)), key=c.__getitem__))
        square += sum((q - p) ** 2 for p, q in zip(r, c)); cells += len(r)
    mean_nll = statistics.mean(nll)
    ordered = sorted(deltas)
    return {"positions":len(expected), "nll":mean_nll,
            "perplexity":math.exp(mean_nll) if mean_nll < 709 else None,
            "kl":statistics.mean(kl), "probability_delta_rms":math.sqrt(square / cells),
            "correct_token_probability_delta":statistics_for(deltas),
            "probability_delta_percentiles_nearest_rank":{str(p):ordered[max(0, math.ceil(p * len(ordered) / 100) - 1)] for p in (5,50,95)},
            "same_top_token_rate":statistics.mean(same)}


def token_ids_identity(tokens):
    """Reproduce the native tokenizer's existing identity, not a new lineage.

    src/tokenizer/execution.c owns this encoding: length-delimited namespace
    (little-endian length), big-endian population and big-endian token IDs.
    A candidate's C-authored identity must match this prepared reference input.
    """
    require(isinstance(tokens, list) and bool(tokens) and len(tokens) <= 1048576,
            "token population unavailable/out of bounds")
    require(all(type(t) is int and 0 <= t <= 0xffffffff for t in tokens), "invalid token ID")
    namespace = b"yvex.tokenizer.token-ids.v1"
    digest = hashlib.sha256(len(namespace).to_bytes(8, "little") + namespace)
    digest.update(len(tokens).to_bytes(8, "big"))
    for token in tokens:
        digest.update(token.to_bytes(8, "big"))
    return digest.hexdigest()


def continuation_agreement(request, reference, candidate, strategy):
    """Observe exact greedy prefix; do not invent cross-realization tolerances.

    The caller authenticates the independent capture and exact checkpoint.
    Only positions before the first divergence share a causal context; later
    free-running token equality is NOT teacher-forced same-top-token accuracy.
    EOS is a sampled token, not necessarily a model-committed decode position.
    """
    require(strategy in ("target-only", "dspark"), "unsupported execution strategy")
    require(request.get("sampling") == {"temperature":0, "stochastic":False},
            "greedy reference policy required")
    require(reference.get("input_identity") == request.get("input_identity")
            and bool(request.get("input_identity")), "reference input mismatch")
    ids = request.get("prompt_token_ids")
    require(candidate.get("prompt_token_identity") == token_ids_identity(ids)
            and candidate.get("prompt_tokens") == len(ids), "candidate prompt token identity mismatch")
    require(candidate.get("status") == "complete" and candidate.get("generation_ready") is True,
            "candidate generation did not complete")
    require(candidate.get("execution_mode") == strategy, "candidate execution strategy mismatch")
    require(type(candidate.get("sampling_draws")) is int and candidate["sampling_draws"] == 0,
            "candidate greedy comparison contains stochastic draws or missing policy evidence")
    stop = candidate.get("stop_reason")
    require(stop in ("eos", "tokenizer-stop-token", "max-new-tokens"), "candidate did not reach bounded completion")
    rows = candidate.get("generated_tokens")
    bound = request.get("maximum_output")
    require(type(bound) is int and 0 < bound <= 4096 and isinstance(rows, list)
            and 0 < len(rows) <= bound and candidate.get("sampled_tokens") == len(rows),
            "candidate output population mismatch")
    require(all(type(row.get("ordinal")) is int and row["ordinal"] == i
                and type(row.get("model_committed")) is bool
                and type(row.get("terminal")) is bool for i,row in enumerate(rows)),
            "candidate token ordinals/commit facts invalid")
    actual = [row.get("token_id") for row in rows]
    token_ids_identity(actual)
    committed = sum(row["model_committed"] for row in rows)
    require(type(candidate.get("model_committed_tokens")) is int
            and candidate["model_committed_tokens"] == committed, "candidate committed population mismatch")
    require(stop != "max-new-tokens" or len(rows) == bound, "candidate truncated below declared output bound")
    expected = reference.get("output_token_ids")
    token_ids_identity(expected)
    require(len(expected) <= bound, "reference exceeds declared output bound")
    prefix = 0
    for left,right in zip(expected, actual):
        if left != right:
            break
        prefix += 1
    same = expected == actual
    return dict(state="CHARACTERIZED", exact_input_tokens=True,
                first_token_agreement=expected[0] == actual[0], greedy_prefix_tokens=prefix,
                exact_bounded_continuation=same, reference_sampled_tokens=len(expected),
                candidate_sampled_tokens=len(actual), candidate_committed_tokens=committed,
                first_divergence=None if same else prefix,
                reference_token_at_divergence=expected[prefix] if prefix < len(expected) else None,
                candidate_token_at_divergence=actual[prefix] if prefix < len(actual) else None,
                candidate_finish=stop, reference_finish=reference.get("finish_reason"),
                comparison_policy="exact greedy prefix on the same input; cross-realization characterization, not an admitted quality tolerance",
                unavailable_metrics=["teacher-forced NLL", "PPL", "full-distribution KL", "RMS probability delta", "teacher-forced same-top-token rate"])


def continuation_receipt(target, rows, provenance, identifier, title, origin="local"):
    """Publish bounded independent comparison without inventing a quality gate.

    Raw-capture authentication belongs to the adapter. This envelope deliberately
    earns neither numerical equivalence nor a timing claim, even for an exact
    token match. Unknown plan dimensions remain unknown in historical captures.
    """
    require(bool(rows), "continuation comparisons missing")
    scopes = {
        "family-conformance": "Independent continuation comparison does not run the family conformance gate",
        "checkpoint-reference": "Exact-input greedy-prefix characterization against the declared independent realization",
        "representation-quality": "No admitted higher-precision checkpoint or full-distribution comparison/tolerance",
        "backend-execution": "Bounded completion does not replace backend numerical and lifecycle qualification",
        "deployment-performance": "No performance claim; preparation and diagnostic work are not resident-host timing",
        "product-path": "Direct engineering generation, not the native local product or HTTP compatibility path",
    }
    claims = {plane:dict(state="CHARACTERIZED" if plane == "checkpoint-reference" else
                        "BLOCKED" if plane == "representation-quality" else "UNQUALIFIED",
                        scope=scope, required_evidence=[], evidence={},
                        blockers=[scope] if plane == "representation-quality" else [])
              for plane,scope in scopes.items()}
    plane, unit, definition = METRICS["greedy-prefix"]
    measurements = [dict(metric="greedy-prefix", definition=definition, unit=unit, case=row["case"],
        prompt_identity=row["input_identity"], reference_identity=row["reference_capture_sha256"],
        session_state="fresh", warm_state="not a timing measurement; standalone generation includes preparation",
        output_bound=row["maximum_output"], samples=[row["comparison"]["greedy_prefix_tokens"]],
        statistics=statistics_for([row["comparison"]["greedy_prefix_tokens"]]),
        scope=claims[plane]["scope"], evidence=row["evidence"]) for row in rows]
    receipt = dict(schema=RECEIPT_SCHEMA, id=identifier, title=title, target=target,
        target_identity=target_id(target), origin=origin, claims=claims, measurements=measurements,
        provenance=dict(provenance, continuation_comparisons=rows), limitations=[
            "Representative suite inputs are not official upstream inference vectors; the four official encoding cases remain a separate gate.",
            "Exact-input free-running continuations use the declared independent arithmetic/weight/KV realization; no cross-realization tolerance is invented.",
            "Only positions before first divergence share a causal context; later token equality is not teacher-forced same-top-token accuracy.",
            "Missing full logits, teacher-forced NLL/PPL, KL and probability deltas are unavailable, never zero error.",
            "One bounded observation per case is not a statistical quality guarantee, model-quality score or sustained-throughput result.",
            "Standalone preparation/teardown is not warm product performance; native and HTTP timings require their own targets.",
            "Historical unprojected plan identities remain null; neither newer source nor another receipt fills them retroactively.",
        ])
    return validate(receipt)


def schema():
    """Deterministic JSON Schema projection; field authority is TARGET_FIELDS."""
    return {"$schema":"https://json-schema.org/draft/2020-12/schema", "title":"YVEX qualification target",
            "type":"object", "additionalProperties":False,
            "required":["schema", *TARGET_FIELDS],
            "properties":{"schema":{"const":TARGET_SCHEMA}, **{name:{"type":[kind,"null"],
                **({"minimum":1} if kind == "integer" else {"minLength":1})} for name,kind in TARGET_FIELDS.items()}}}


def validate_reference_observation(record):
    """Validate a compact independent capture view, not numerical conformance.

    Family-specific capture owners authenticate raw outputs and source grammar.
    This generic layer preserves those findings without inventing quality gates.
    """
    import re
    require(record.get("schema") == "yvex.qualification.reference-observation.v1", "invalid reference observation")
    require(record.get("status") == "CHARACTERIZED" and record.get("yvex_conformance") == "NOT_RUN",
            "reference capture cannot promote YVEX conformance")
    require(record.get("source", {}).get("repository") and record["source"].get("revision"), "reference source required")
    implementation = record.get("implementation", {})
    require(implementation.get("independent_of_yvex") is True, "independent producer required")
    for key in ("name", "revision", "command"):
        require(bool(implementation.get(key)), "reference producer lacks " + key)
    for value in [record.get(k) for k in ("suite_sha256", "checkpoint_manifest_sha256", "reference_sha256")] + [
            implementation.get(k) for k in ("executable_sha256", "environment_identity", "representation_identity")]:
        require(isinstance(value, str) and re.fullmatch(r"[a-f0-9]{64}", value), "invalid reference identity")
    require(isinstance(record.get("limitations"), list) and bool(record["limitations"]), "reference limits required")
    rows = record.get("cases")
    require(isinstance(rows, list) and bool(rows), "reference cases required")
    seen = set()
    for row in rows:
        key = (row.get("case"), row.get("reasoning"))
        require(all(isinstance(x, str) and x for x in key) and key not in seen, "duplicate/invalid reference case")
        seen.add(key)
        for field in ("input_identity", "capture_sha256", "output_sha256"):
            require(isinstance(row.get(field), str) and re.fullmatch(r"[a-f0-9]{64}", row[field]), "invalid capture identity")
        for field in ("prompt_tokens", "maximum_output", "output_tokens"):
            require(type(row.get(field)) is int and row[field] > 0, "invalid reference population")
        require(row["output_tokens"] <= row["maximum_output"], "reference exceeds output bound")
        require(row.get("finish") in ("eos", "stop", "length"), "reference not completed")
        if row["finish"] == "length":
            require(row["output_tokens"] == row["maximum_output"], "reference length differs from bound")
        grammar = row.get("grammar", {})
        require(grammar.get("state") in ("PASS", "FAIL", "NOT_MEASURED_TRUNCATED"), "invalid source grammar result")
        require(row["finish"] == "eos" or grammar["state"] == "NOT_MEASURED_TRUNCATED", "truncated output cannot pass full grammar")
        require(grammar["state"] != "FAIL" or bool(grammar.get("reason")), "grammar failure reason required")
        positions = grammar.get("reasoning_boundary_positions")
        require(isinstance(positions, list) and all(type(p) is int and 0 <= p < row["output_tokens"] for p in positions),
                "invalid source boundary positions")
        lineage = ("prior_capture_sha256", "prior_input_identity", "turn_index", "parent_case")
        if any(key in row for key in lineage):
            require(all(key in row for key in lineage), "incomplete reference history lineage")
            for key in lineage[:2]:
                require(isinstance(row[key], str) and re.fullmatch(r"[a-f0-9]{64}", row[key]),
                        "invalid reference history identity")
            require(type(row["turn_index"]) is int and row["turn_index"] > 0,
                    "invalid reference history turn")
            require(isinstance(row["parent_case"], str) and bool(row["parent_case"]),
                    "reference parent case missing")
    dispositions = record.get("continuation_dispositions", [])
    require(isinstance(dispositions, list), "invalid reference continuation dispositions")
    seen = set()
    for row in dispositions:
        key = (row.get("case"), row.get("reasoning"), row.get("turn_index"))
        require(all(isinstance(x, str) and x for x in key[:2]) and type(key[2]) is int
                and key[2] > 0 and key not in seen, "invalid/duplicate reference continuation disposition")
        seen.add(key)
        require(row.get("state") in ("CHARACTERIZED", "UNQUALIFIED") and bool(row.get("reason")),
                "invalid continuation evidence state")
        require(row.get("prior_grammar") in ("PASS", "FAIL", "NOT_MEASURED_TRUNCATED"),
                "invalid prior source grammar")
        require(isinstance(row.get("prior_capture_sha256"), str)
                and re.fullmatch(r"[a-f0-9]{64}", row["prior_capture_sha256"]), "invalid prior capture")
        if row["state"] == "CHARACTERIZED":
            require(row["prior_grammar"] == "PASS" and any(
                case.get("prior_capture_sha256") == row["prior_capture_sha256"]
                and case.get("parent_case") == row["case"]
                and case.get("turn_index") == row["turn_index"] and case["reasoning"] == row["reasoning"]
                for case in rows), "continuation capture missing or prior source grammar failed")
    return record
