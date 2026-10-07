#!/usr/bin/env python3
"""Prepare exact upstream inputs; admit independently captured inference evidence.

Preparation is NOT full-model inference. Missing independent results fail closed.
No YVEX output is accepted as an independent reference and no cross-precision
bit-exact or tolerance claim is invented by this harness.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import qualification_run as measurement
import qualification_reference as independent

CORPUS = ROOT / "tests/vectors/deepseek_product.json"


def prepare(source):
    from tokenizers import Tokenizer
    import tokenizers
    if tokenizers.__version__ != "0.20.3":
        raise ValueError("reference tokenizer must be tokenizers==0.20.3")
    authority = json.loads((ROOT / "tests/vectors/manifest.json").read_text())["deepseek_official_encoding"]
    for name, expected in authority["files"].items():
        if measurement.digest(source / name) != expected:
            raise ValueError("upstream byte identity differs: " + name)
    spec = importlib.util.spec_from_file_location("upstream_dsv4", source / "encoding/encoding_dsv4.py")
    encoding = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(encoding)
    tokenizer = Tokenizer.from_file(str(source / "tokenizer.json"))
    records = []
    for base in measurement.corpus(CORPUS)["cases"]:
        for reasoning in base["reasoning_modes"]:
            records.append(prepare_case(dict(base, reasoning=reasoning), encoding, tokenizer, authority))
    return dict(schema="yvex.deepseek.reference-inputs.v1", authority=authority,
                corpus_sha256=measurement.digest(CORPUS),
                upstream_inference_files={str(p.relative_to(source)):measurement.digest(p)
                                          for p in sorted((source / "inference").glob("*.py"))},
                evidence_class="independent-input-encoding; full-model NOT RUN", cases=records)


def prepare_case(case, encoding, tokenizer, authority):
    # Multi-turn product continuations depend on actual generated replies;
    # only their first input can be prepared without inventing assistant state.
    messages = case.get("messages") or [{"role":"user", "content":measurement.prompts(case)[0]}]
    if "tools" in case:
        messages = [{"role":"system", "content":"", "tools":case["tools"]}, *messages]
    thinking = "chat" if case["reasoning"] == "none" else "thinking"
    effort = "max" if case["reasoning"] == "maximum" else "high" if case["reasoning"] == "high" else None
    rendered = encoding.encode_messages(messages, thinking, reasoning_effort=effort)
    ids = tokenizer.encode(rendered, add_special_tokens=False).ids
    record = dict(case=case["id"], workload_class=case["class"], messages=messages,
                  reasoning=case["reasoning"], conversation_mode=thinking,
                  sampling={"temperature":0, "stochastic":False}, maximum_output=case["maximum_output"],
                  rendered_prompt=rendered, rendered_prompt_sha256=hashlib.sha256(rendered.encode()).hexdigest(),
                  prompt_token_ids=ids, prompt_token_count=len(ids),
                  tokenizer_sha256=authority["files"]["tokenizer.json"],
                  inference_reference_status="MISSING", reference_output=None,
                  multi_turn_status="requires independent generated continuation" if len(case.get("turns", [])) > 1 else "not-applicable")
    record["input_identity"] = measurement.canonical({k:v for k,v in record.items()
                                                    if k not in ("inference_reference_status", "reference_output")})
    return record


def prepare_continuations(inputs, reference, evidence_root, source):
    """Continue only independently completed, source-parseable conversations.

    The generic producer still consumes prepared token IDs. Family grammar owns
    the assistant history, including the source's reasoning-drop policy; neither
    arbitrary prose nor a length-truncated reply becomes a completed assistant.
    """
    from tokenizers import Tokenizer
    validate_reference(inputs, reference, evidence_root)
    spec = importlib.util.spec_from_file_location("upstream_dsv4_history", source / "encoding/encoding_dsv4.py")
    encoding = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(encoding)
    tokenizer = Tokenizer.from_file(str(source / "tokenizer.json"))
    requests = {(r["case"], r["reasoning"]):r for r in inputs["cases"]}
    captures = {(r["case"], r["reasoning"]):r for r in reference["cases"]}
    cases, dispositions = [], []
    for base in measurement.corpus(CORPUS)["cases"]:
        if len(base.get("turns", [])) < 2:
            continue
        for reasoning in base["reasoning_modes"]:
            result = captures.get((base["id"], reasoning))
            if result is None:
                continue
            request = requests[(base["id"], reasoning)]
            grammar = output_grammar(result["output_token_ids"], result["finish_reason"], reasoning,
                                     encoding, tokenizer)
            disposition = dict(case=base["id"], reasoning=reasoning, turn_index=1,
                               prior_capture_sha256=result["raw_evidence_sha256"],
                               prior_grammar=grammar["state"])
            if grammar["state"] != "PASS":
                dispositions.append(dict(disposition, state="UNQUALIFIED",
                    reason="prior assistant did not complete source-authored grammar; no fabricated history"))
                continue
            assistant = encoding.parse_message_from_completion_text(
                tokenizer.decode(result["output_token_ids"], skip_special_tokens=False),
                request["conversation_mode"])
            messages = [*request["messages"], assistant, {"role":"user", "content":base["turns"][1]}]
            row = prepare_case(dict(base, id=base["id"] + "/turn-1", messages=messages,
                                    reasoning=reasoning), encoding, tokenizer, inputs["authority"])
            row.update(turn_index=1, prior_input_identity=request["input_identity"],
                       prior_capture_sha256=result["raw_evidence_sha256"],
                       multi_turn_status="actual independent assistant history; second request")
            row["input_identity"] = measurement.canonical({k:v for k,v in row.items()
                if k not in ("inference_reference_status", "reference_output", "input_identity")})
            cases.append(row)
            dispositions.append(dict(disposition, state="PREPARED", reason="source-encoded actual independent history"))
    if not cases:
        raise ValueError("no source-parseable completed history for independent continuation")
    return dict(inputs, cases=cases, continuation_dispositions=dispositions,
                evidence_class="independent continuation input encoding; second request NOT RUN")


def validate_reference(inputs, reference, evidence_root=None):
    generic = reference.get("schema") == independent.SCHEMA
    if not generic and reference.get("schema") != "yvex.deepseek.independent-inference.v1":
        raise ValueError("unsupported inference evidence")
    if reference.get("source") != {k:inputs["authority"][k] for k in ("repository", "revision")}:
        raise ValueError("source checkpoint not exact; a hosted model name is insufficient")
    implementation = reference.get("implementation", {})
    if implementation.get("independent_of_yvex") is not True or "yvex" in implementation.get("name", "").lower():
        raise ValueError("not an independent inference owner")
    for key in ("name", "revision", "executable_sha256", "command", "environment", "physical_representation"):
        if not implementation.get(key):
            raise ValueError("missing reproducibility: " + key)
    if not reference.get("checkpoint_manifest_sha256"):
        raise ValueError("missing exact weight manifest")
    expected = {(r["case"], r["reasoning"]):r for r in inputs["cases"]
                if r["workload_class"] != "synthetic"}
    if not expected:
        raise ValueError("empty independent inference suite")
    by_id = {(r["case"], r["reasoning"]):r for r in reference.get("cases", [])}
    if len(by_id) != len(reference.get("cases", [])):
        raise ValueError("duplicate reference cases")
    if set(by_id) != set(expected):
        raise ValueError("independent inference suite coverage differs")
    for request in expected.values():
        result = by_id.get((request["case"], request["reasoning"]), {})
        if result.get("input_identity") != request["input_identity"]:
            raise ValueError("missing/mismatched independent inference: " + request["case"])
        for key in ("output_token_ids", "output_sha256", "raw_evidence_sha256", "finish_reason"):
            if not result.get(key):
                raise ValueError("missing inference result: " + key)
        if not all(type(t) is int and t >= 0 for t in result["output_token_ids"]):
            raise ValueError("malformed reference tokens")
        if len(result["output_token_ids"]) > request["maximum_output"]:
            raise ValueError("reference output exceeds admitted bound")
        if result["finish_reason"] not in ("eos", "stop", "length"):
            raise ValueError("reference inference did not complete successfully")
        if (result["finish_reason"] == "length"
                and len(result["output_token_ids"]) != request["maximum_output"]):
            raise ValueError("length-limited reference did not execute the declared output bound")
        if evidence_root is None:
            raise ValueError("independent raw evidence root required; metadata alone is not inference evidence")
        relative = Path(result.get("raw_evidence_path", ""))
        root = evidence_root.resolve()
        raw = (root / relative).resolve()
        if relative.is_absolute() or not raw.is_relative_to(root) or raw == root:
            raise ValueError("raw evidence must be a bounded relative path")
        if raw.stat().st_size > 16 * 1024 * 1024:
            raise ValueError("reference capture exceeds bounded extent")
        if measurement.digest(raw) != result["raw_evidence_sha256"]:
            raise ValueError("raw inference evidence digest mismatch")
        payload = json.loads(raw.read_text())
        if generic:
            independent.validate_capture(request, payload, root)
            closed = independent.read(root / "closed.json")
            if closed.get("exit_code") != 0 or closed.get("gpu_processes_after") != "":
                raise ValueError("independent producer teardown not qualified")
        if (payload.get("input_identity") != request["input_identity"]
                or payload.get("output_token_ids") != result["output_token_ids"]
                or not isinstance(payload.get("text"), str)
                or hashlib.sha256(payload["text"].encode()).hexdigest() != result["output_sha256"]):
            raise ValueError("reference continuation differs from raw capture")
        # An eight-token admission probe must not masquerade as the corpus's
        # full bounded request. Output identity alone cannot prove execution policy.
        if (payload.get("sampling") != request["sampling"]
                or payload.get("maximum_output") != request["maximum_output"]
                or payload.get("prompt_token_ids") != request["prompt_token_ids"]
                or payload.get("finish_reason") != result["finish_reason"]):
            raise ValueError("reference execution policy differs from prepared input")
    return {"status":"REFERENCE_CAPTURE_PRESENT", "yvex_conformance":"NOT_RUN",
            "comparison_policy":"exact input identity; numerical/continuation comparison must separately admit representation differences"}


def output_grammar(tokens, finish, reasoning, encoding, tokenizer):
    """Observe source-authored structure, never classify generated prose.

    A bounded truncation is not a malformed completed message. Conversely an
    EOS without the required reasoning terminator must remain a visible failure.
    This checks the independent producer output, not YVEX model conformance.
    """
    end = tokenizer.token_to_id(encoding.thinking_end_token)
    eos = tokenizer.token_to_id(encoding.eos_token)
    if end is None or eos is None:
        raise ValueError("source grammar token missing")
    result = dict(state="NOT_MEASURED_TRUNCATED", reason=None,
                  reasoning_boundary_positions=[i for i, token in enumerate(tokens) if token == end])
    if finish != "eos":
        return result
    if not tokens or tokens[-1] != eos:
        return dict(result, state="FAIL", reason="natural EOS differs from token evidence")
    try:
        encoding.parse_message_from_completion_text(
            tokenizer.decode(tokens, skip_special_tokens=False),
            "chat" if reasoning == "none" else "thinking")
    except (ValueError, AssertionError):
        # Do not retain parser error prose: it can contain the generated message.
        return dict(result, state="FAIL", reason="source-authored completion parser refused output")
    return dict(result, state="PASS")


def reference_summary(inputs, reference, source):
    from tokenizers import Tokenizer
    # prepare() has already authenticated this exact source and tokenizer.
    spec = importlib.util.spec_from_file_location("upstream_dsv4_result", source / "encoding/encoding_dsv4.py")
    encoding = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(encoding)
    tokenizer = Tokenizer.from_file(str(source / "tokenizer.json"))
    requests = {(r["case"], r["reasoning"]):r for r in inputs["cases"]}
    rows = []
    for result in reference["cases"]:
        request = requests[(result["case"], result["reasoning"])]
        rows.append(dict(case=result["case"], reasoning=result["reasoning"],
            input_identity=request["input_identity"], prompt_tokens=request["prompt_token_count"],
            maximum_output=request["maximum_output"], output_tokens=len(result["output_token_ids"]),
            finish=result["finish_reason"], capture_sha256=result["raw_evidence_sha256"],
            output_sha256=result["output_sha256"], multi_turn=request["multi_turn_status"],
            grammar=output_grammar(result["output_token_ids"], result["finish_reason"],
                                   result["reasoning"], encoding, tokenizer)))
        if "prior_capture_sha256" in request:
            parent = next(row["case"] for row in inputs["continuation_dispositions"]
                          if row["prior_capture_sha256"] == request["prior_capture_sha256"]
                          and row["reasoning"] == request["reasoning"])
            rows[-1].update(prior_capture_sha256=request["prior_capture_sha256"],
                            prior_input_identity=request["prior_input_identity"], turn_index=request["turn_index"],
                            parent_case=parent)
    producer = reference["implementation"]
    implementation = {key:producer[key] for key in ("name", "revision", "executable_sha256", "command")}
    implementation["independent_of_yvex"] = producer["independent_of_yvex"]
    # Large per-tensor manifests remain external and are linked by identity.
    implementation["environment_identity"] = measurement.canonical(producer["environment"])
    implementation["representation_identity"] = measurement.canonical(producer["physical_representation"])
    report = dict(schema="yvex.qualification.reference-observation.v1",
                source=reference["source"], implementation=implementation,
                suite_sha256=inputs["corpus_sha256"],
                checkpoint_manifest_sha256=reference["checkpoint_manifest_sha256"],
                status="CHARACTERIZED", yvex_conformance="NOT_RUN", cases=rows,
                limitations=reference["limitations"])
    if inputs.get("continuation_dispositions"):
        captured = {(r["case"], r["reasoning"]) for r in rows}
        report["continuation_dispositions"] = [dict(row, state="CHARACTERIZED"
            if (row["case"] + "/turn-1", row["reasoning"]) in captured else "UNQUALIFIED")
            for row in inputs["continuation_dispositions"]]
        # Keep the producer plan's original warnings auditable, but label their
        # provenance: they may describe its earlier first-request-only capture.
        report["limitations"] = ["Inherited producer-plan note: " + item for item in reference["limitations"]]
        report["limitations"].append(
            "Actual coverage here is the authenticated second request listed below, with independent generated history; not native prefix reuse or complete multi-turn qualification in all modes.")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--continue-from", type=Path,
                        help="prepare second requests from authenticated, completed independent assistant history")
    parser.add_argument("--report", type=Path, help="exclusive compact reference observation, without generated prose")
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("raw prompt/token vectors must be outside Git")
    inputs = prepare(args.source)
    if args.continue_from:
        prior = independent.read(args.continue_from)
        inputs = prepare_continuations(inputs, prior, args.continue_from.parent, args.source)
    with args.output.open("x") as stream:
        json.dump(inputs, stream, ensure_ascii=False, indent=2, allow_nan=False)
    if not args.reference:
        print("BLOCKED independent full-model inference: inputs prepared, outputs absent")
        return 2
    reference = json.loads(args.reference.read_text())
    result = validate_reference(inputs, reference, args.reference.parent)
    if args.report:
        if args.report.resolve().is_relative_to(ROOT):
            parser.error("capture report outside Git first; review before publication")
        report = reference_summary(inputs, reference, args.source)
        report["reference_sha256"] = measurement.digest(args.reference)
        with args.report.open("x") as stream:
            json.dump(report, stream, indent=2, allow_nan=False)
        result["output_grammar"] = {state:sum(r["grammar"]["state"] == state for r in report["cases"])
                                    for state in ("PASS", "FAIL", "NOT_MEASURED_TRUNCATED")}
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
