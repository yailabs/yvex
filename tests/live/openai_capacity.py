#!/usr/bin/env python3
"""Public capacity control through a real already-loaded YVEX endpoint.

No private model access, no fallback, no workload truncation. JSON lines retain
request/response digests, exact model identities, expected and observed facts.
Caller owns an exclusive test host/GPU and source-stability receipt.
"""
import argparse
import hashlib
import json
import time
import urllib.error
import urllib.request


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base")
    parser.add_argument("model")
    args = parser.parse_args()

    def exchange(path, body=None):
        req = urllib.request.Request(args.base.rstrip("/") + path, data=body,
                                     headers={"Content-Type": "application/json"})
        started = time.monotonic()
        try:
            with urllib.request.urlopen(req, timeout=180) as response:
                status, raw = response.status, response.read()
        except urllib.error.HTTPError as exc:
            status, raw = exc.code, exc.read()
        if raw.startswith((b"data:", b"event:")):
            value = {"events": [json.loads(line[6:]) for line in raw.splitlines()
                                if line.startswith(b"data: ") and line != b"data: [DONE]"],
                     "done": b"data: [DONE]" in raw}
        else:
            value = json.loads(raw)
        print(json.dumps({"path": path, "body_bytes": len(body or b""),
                          "request_sha256": digest(body or b""), "status": status,
                          "wall_seconds": time.monotonic() - started,
                          "response_sha256": digest(raw), "response": value}, sort_keys=True), flush=True)
        return status, value

    status, catalog = exchange("/v1/models")
    assert status == 200
    model = next(row for row in catalog["data"] if row["id"] == args.model)
    assert model["yvex_profile"] == "yvex.openai.compat.v3"
    capacity = model["yvex_capacity"]
    context = capacity["runtime_sequence_tokens"]
    maximum = capacity["maximum_requested_output_tokens"]
    assert context > 8 and maximum > 0 and not capacity["resource_reservation"]

    def request(content, maximum_output=1, tools=None, generation=None, pad=40277, stream=False):
        value = {"model": args.model, "yvex_engine_generation":
                 model["engine_generation"] if generation is None else generation,
                 "messages": [{"role": "user", "content": content}],
                 "max_tokens": maximum_output, "temperature": 0, "reasoning_effort": "none"}
        if tools is not None:
            value["tools"] = tools
        if stream:
            value["stream"] = True
        body = json.dumps(value, separators=(",", ":"), ensure_ascii=False).encode()
        assert len(body) <= pad
        return body + b" " * (pad - len(body))

    def preflight(body):
        status, value = exchange("/v1/chat/completions/preflight", body)
        assert status == 200
        assert value["engine_generation"] == model["engine_generation"]
        assert value["execution_or_resources_qualified"] is False
        assert value["yvex_capacity"]["resource_reservation"] is False
        assert len(value["tokenizer_identity"]) == 64 and len(value["prompt_identity"]) == 64
        return value

    brief = request("Hello.")
    first, repeated = preflight(brief), preflight(brief)
    assert first == repeated, "greedy exact preflight must be deterministic"
    assert first["token_capacity_compatible"] and first["effective_output_tokens"] == 1
    dense_body = request(" x" * 12000)
    dense = preflight(dense_body)
    assert dense["input_capacity_exceeded"] and not dense["output_capacity_exceeded"]
    assert dense["input_tokens"] > context
    changed_output = preflight(request(" x" * 12000, min(maximum, 8)))
    assert changed_output["input_tokens"] == dense["input_tokens"]
    assert changed_output["prompt_identity"] == dense["prompt_identity"]
    assert changed_output["input_capacity_exceeded"]

    tools = [{"type": "function", "function": {"name": "read_data",
              "description": " x" * 12000,
              "parameters": {"type": "object", "properties": {}}}}]
    tool = preflight(request("Hello.", tools=tools))
    assert tool["input_capacity_exceeded"] and tool["input_tokens"] > first["input_tokens"]

    output = preflight(request("Hello.", maximum + 1))
    assert output["output_capacity_exceeded"] and not output["input_capacity_exceeded"]
    status, error = exchange("/v1/chat/completions", request("Hello.", maximum + 1))
    assert status == 413 and error["error"]["code"] == "output_token_capacity_exceeded"
    for body in (dense_body, request(" x" * 12000, min(maximum, 8))):
        status, error = exchange("/v1/chat/completions", body)
        assert status == 413 and error["error"]["code"] == "input_token_capacity_exceeded"
    status, stream_error = exchange("/v1/chat/completions", request(" x" * 12000, stream=True))
    if status == 200:
        assert not stream_error["done"]
        assert stream_error["events"][-1]["error"]["code"] == "input_token_capacity_exceeded"
    else:
        assert status == 413 and stream_error["error"]["code"] == "input_token_capacity_exceeded"
    response_body = json.dumps({"model": args.model, "input": " x" * 12000,
                               "temperature": 0, "max_output_tokens": 1,
                               "reasoning_effort": "none", "stream": True, "store": False},
                              separators=(",", ":")).encode()
    status, response_error = exchange("/v1/responses", response_body)
    if status == 200:
        failed = response_error["events"][-1]
        assert failed["type"] == "response.failed"
        assert failed["response"]["error"]["code"] == "input_token_capacity_exceeded"
    else:
        assert status == 413 and response_error["error"]["code"] == "input_token_capacity_exceeded"
    status, completion = exchange("/v1/chat/completions", brief)
    assert status == 200 and completion["usage"]["prompt_tokens"] == first["input_tokens"]
    assert completion["usage"]["completion_tokens"] == 1
    status, _ = exchange("/v1/chat/completions/preflight",
                         request("Hello.", generation=model["engine_generation"] + 1))
    assert status == 409

    # A bounded token-density threshold, not a byte threshold or model-name rule.
    low, high = 0, min(context * 2, 12000)
    low_report = preflight(request(" x"))
    for _ in range(15):
        if high - low <= 1:
            break
        middle = (low + high) // 2
        observed = preflight(request(" x" * max(1, middle)))
        if observed["input_capacity_exceeded"]:
            high = middle
        else:
            low, low_report = middle, observed
    high_report = preflight(request(" x" * high))
    assert low_report["input_tokens"] <= context < high_report["input_tokens"]
    print(json.dumps({"result": "PASS", "oracle": "admitted tokenizer and runtime capacity",
                      "scope": "public capacity, not upstream model conformance",
                      "input_boundary": [low_report["input_tokens"], high_report["input_tokens"]],
                      "sequence_capacity": context, "fallback_count": 0}, sort_keys=True))


if __name__ == "__main__":
    main()
