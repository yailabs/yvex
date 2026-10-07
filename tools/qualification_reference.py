#!/usr/bin/env python3
"""Capture independent continuations from manifest inputs, never YVEX output.

This is an independent-producer adapter, not a benchmark or a family oracle.
Family owners authenticate/prepare inputs and interpret captured output grammar.
Only an explicitly owned loopback llama.cpp process is started or stopped.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time

import qualification_run as measurement

ROOT = measurement.ROOT
SCHEMA = "yvex.qualification.reference-captures.v1"
PLAN_SCHEMA = "yvex.qualification.reference-producer.v1"
MAX_BYTES = 16 * 1024 * 1024


def write(path, value):
    with path.open("x") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False, allow_nan=False)


def read(path):
    if path.stat().st_size > MAX_BYTES:
        raise ValueError("reference manifest exceeds bounded extent")
    return json.loads(path.read_text())


def input_cases(inputs, selected=()):
    """Prepared token IDs are authoritative; no prompt is authored by this adapter."""
    rows = inputs.get("cases", [])
    if not rows:
        raise ValueError("empty reference inputs")
    seen = set()
    result = []
    for row in rows:
        key = (row.get("case"), row.get("reasoning"))
        if not all(isinstance(k, str) and k for k in key) or key in seen:
            raise ValueError("duplicate/invalid reference input")
        seen.add(key)
        ids = row.get("prompt_token_ids")
        if (not isinstance(ids, list) or not ids
                or not all(type(t) is int and t >= 0 for t in ids)
                or row.get("prompt_token_count") != len(ids)
                or type(row.get("maximum_output")) is not int or not 0 < row["maximum_output"] <= 4096
                or not isinstance(row.get("rendered_prompt"), str)
                or not isinstance(row.get("input_identity"), str)):
            raise ValueError("malformed prepared reference input")
        if row.get("sampling") != {"temperature": 0, "stochastic": False}:
            raise ValueError("independent adapter admits explicit greedy sampling only")
        if not selected or row["case"] in selected:
            result.append(row)
    if not result or set(selected) - {r["case"] for r in result}:
        raise ValueError("requested reference case absent")
    return result


def request_body(row):
    return dict(prompt=row["prompt_token_ids"], n_predict=row["maximum_output"],
                temperature=0, top_k=0, top_p=1, min_p=0, repeat_penalty=1,
                repeat_last_n=0, cache_prompt=False, n_probs=20, return_tokens=True, stream=False)


def continuation(row, body, response):
    """Policy and token provenance are checked before publishing a raw capture."""
    if body != request_body(row):
        raise ValueError("reference request policy differs from prepared input")
    finish = {"limit": "length", "eos": "eos", "word": "stop"}.get(response.get("stop_type"))
    tokens = response.get("tokens")
    if (finish is None or not isinstance(tokens, list) or not tokens
            or not all(type(t) is int and t >= 0 for t in tokens)
            or len(tokens) > row["maximum_output"]
            or finish == "length" and len(tokens) != row["maximum_output"]
            or not isinstance(response.get("content"), str)):
        raise ValueError("independent response lacks bounded continuation evidence")
    if (response.get("tokens_evaluated") != len(row["prompt_token_ids"])
            or response.get("tokens_predicted") != len(tokens)):
        raise ValueError("independent prompt population differs")
    return dict(input_identity=row["input_identity"], prompt_token_ids=row["prompt_token_ids"],
                sampling=row["sampling"], maximum_output=row["maximum_output"],
                output_token_ids=tokens, text=response["content"], finish_reason=finish)


def checked_raw(root, path, expected):
    relative = Path(path)
    root = root.resolve()
    resolved = (root / relative).resolve()
    if (relative.is_absolute() or not resolved.is_relative_to(root) or resolved == root
            or not resolved.is_file() or resolved.stat().st_size > MAX_BYTES):
        raise ValueError("reference evidence must be a bounded relative file")
    if measurement.digest(resolved) != expected:
        raise ValueError("reference raw evidence digest mismatch")
    return read(resolved)


def validate_capture(row, capture, root):
    """Raw producer request/response, not caller summaries, anchor continuation."""
    body = checked_raw(root, capture["raw_request_path"], capture["raw_request_sha256"])
    response = checked_raw(root, capture["raw_response_path"], capture["raw_response_sha256"])
    expected = continuation(row, body, response)
    if any(capture.get(k) != v for k, v in expected.items()):
        raise ValueError("reference capture differs from independent producer response")
    return expected


def source_state(path):
    def git(*args):
        return subprocess.check_output(["git", "-C", str(path), *args], text=True).strip()
    return dict(revision=git("rev-parse", "HEAD"), tree=git("rev-parse", "HEAD^{tree}"),
                status=git("status", "--porcelain"), repository=git("remote", "get-url", "origin"))


def loaded_producer(pid, implementation):
    if measurement.digest(Path(f"/proc/{pid}/exe")) != implementation["executable_sha256"]:
        raise ValueError("independent loaded executable identity differs")
    maps = Path(f"/proc/{pid}/maps").read_text().splitlines()
    paths = {line.split(maxsplit=5)[5] for line in maps if len(line.split(maxsplit=5)) == 6}
    for name, expected in implementation["environment"]["libraries"].items():
        resolved = Path(name).resolve()
        if str(resolved) not in paths or measurement.digest(resolved) != expected:
            raise ValueError("independent loaded library identity differs")


def producer(plan):
    if plan.get("schema") != PLAN_SCHEMA or plan.get("adapter") != "llama.cpp-completion-v1":
        raise ValueError("unsupported independent producer plan")
    source = source_state(Path(plan["checkout"]))
    if (source["status"] or source["revision"] != plan["revision"] or source["tree"] != plan["tree"]
            or source["repository"] != plan["repository"]):
        raise ValueError("independent source is not the exact clean pinned producer")
    if "yvex" in plan["name"].lower():
        raise ValueError("YVEX is not an independent reference")
    for field in ("binary", "artifact", "checkpoint_manifest", "representation_receipt"):
        if measurement.digest(Path(plan[field])) != plan[field + "_sha256"]:
            raise ValueError("independent producer identity differs: " + field)
    libraries = {}
    for name, expected in plan["libraries"].items():
        if measurement.digest(Path(name)) != expected:
            raise ValueError("independent library identity differs")
        libraries[name] = expected
    if not libraries or not plan.get("limitations"):
        raise ValueError("independent runtime libraries and non-claims required")
    if (type(plan.get("context")) is not int or not 1 <= plan["context"] <= 32768
            or type(plan.get("chunk")) is not int or not 1 <= plan["chunk"] <= plan["context"]
            or type(plan.get("port")) is not int or not 1024 <= plan["port"] <= 65535):
        raise ValueError("invalid independent execution bounds")
    # Explicit F32 KV is an independent numerical realization, not YVEX identity.
    argv = [plan["binary"], "-m", plan["artifact"], "-c", str(plan["context"]),
            "-b", str(plan["chunk"]), "-ub", str(plan["chunk"]), "-ngl", "99", "-np", "1",
            "--no-warmup", "--cache-type-k", "f32", "--cache-type-v", "f32",
            "--host", "127.0.0.1", "--port", str(plan["port"])]
    return dict(independent_of_yvex=True, name=plan["name"], revision=source["revision"],
                executable_sha256=plan["binary_sha256"], command=argv,
                environment=dict(source=source, libraries=libraries, plan_identity=measurement.canonical(plan)),
                physical_representation=read(Path(plan["representation_receipt"])))


def exchange(port, method, path, body=None):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=600)
    try:
        connection.request(method, path, None if body is None else json.dumps(body),
                           {"Content-Type": "application/json"})
        response = connection.getresponse()
        raw = response.read(MAX_BYTES + 1)
        if len(raw) > MAX_BYTES:
            raise ValueError("independent response exceeds bounded extent")
        return response.status, raw
    finally:
        connection.close()


def capture(inputs_path, plan_path, output, selected=()):
    import qa
    inputs, plan = read(inputs_path), read(plan_path)
    cases = input_cases(inputs, selected)
    if output.resolve().is_relative_to(ROOT):
        raise ValueError("raw reference evidence must remain outside Git")
    registry, _ = qa.load_registry()
    with qa.resource_locks(registry, ["cuda-device", "gb10-live-model", "benchmark-directory"]):
        gpu = lambda: subprocess.check_output(["nvidia-smi", "--query-compute-apps=pid",
                                               "--format=csv,noheader"], text=True).strip()
        if gpu() or Path(f"/run/user/{os.getuid()}/yvex/yvexd.sock").exists():
            raise ValueError("operator/GPU occupied; no independent reference admission")
        implementation = producer(plan)
        checkpoint = read(Path(plan["checkpoint_manifest"]))
        authority = inputs["authority"]
        if (checkpoint["source"]["repo"] != authority["repository"]
                or checkpoint["source"]["revision"] != authority["revision"]
                or checkpoint["verification"]["payload_digest_status"] != "upstream_payload_verified"):
            raise ValueError("reference weights do not bind the prepared checkpoint")
        if any(len(c["prompt_token_ids"]) + c["maximum_output"] > plan["context"] for c in cases):
            raise ValueError("reference workload exceeds declared context")
        # An occupied port is refused, never adopted as an independent producer.
        with socket.socket() as port:
            port.bind(("127.0.0.1", plan["port"]))
        output.mkdir(mode=0o700, parents=True, exist_ok=False)
        write(output / "plan.json", plan)
        write(output / "inputs.json", inputs)
        record = dict(schema=SCHEMA, source={k:authority[k] for k in ("repository", "revision")},
                      checkpoint_manifest_sha256=plan["checkpoint_manifest_sha256"],
                      input_manifest_sha256=measurement.digest(inputs_path), implementation=implementation,
                      limitations=plan["limitations"], cases=[])
        record["implementation"]["environment"]["capture_adapter_sha256"] = measurement.digest(Path(__file__))
        write(output / "identity.json", record)
        with (output / "server.log").open("x") as log:
            server = subprocess.Popen(implementation["command"], stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 900
                while server.poll() is None:
                    try:
                        if exchange(plan["port"], "GET", "/health")[0] == 200:
                            break
                    except (OSError, http.client.HTTPException):
                        pass
                    if time.monotonic() >= deadline:
                        raise ValueError("independent load deadline; no generation retry")
                    time.sleep(.1)
                if server.poll() is not None:
                    raise ValueError("independent producer failed admission")
                loaded_producer(server.pid, implementation)
                print(json.dumps(dict(stage="reference-ready", process=server.pid)), flush=True)
                for index, row in enumerate(cases):
                    status, raw = exchange(plan["port"], "POST", "/tokenize", dict(
                        content=row["rendered_prompt"], add_special=False, parse_special=True))
                    if status != 200 or json.loads(raw)["tokens"] != row["prompt_token_ids"]:
                        raise ValueError("independent prompt token identity differs")
                    stem = f"{index:03d}"
                    body = request_body(row)
                    write(output / (stem + ".request.json"), body)
                    status, raw = exchange(plan["port"], "POST", "/completion", body)
                    with (output / (stem + ".response.json")).open("xb") as stream:
                        stream.write(raw)
                    if status != 200:
                        raise ValueError(f"independent generation refused HTTP {status}; no retry")
                    result = continuation(row, body, json.loads(raw))
                    result.update(raw_request_path=stem + ".request.json",
                                  raw_response_path=stem + ".response.json",
                                  raw_request_sha256=measurement.digest(output / (stem + ".request.json")),
                                  raw_response_sha256=measurement.digest(output / (stem + ".response.json")))
                    write(output / (stem + ".capture.json"), result)
                    record["cases"].append(dict(case=row["case"], reasoning=row["reasoning"],
                        input_identity=row["input_identity"], output_token_ids=result["output_token_ids"],
                        output_sha256=hashlib.sha256(result["text"].encode()).hexdigest(),
                        raw_evidence_path=stem + ".capture.json",
                        raw_evidence_sha256=measurement.digest(output / (stem + ".capture.json")),
                        finish_reason=result["finish_reason"]))
                    print(json.dumps(dict(stage="reference-case", case=row["case"], reasoning=row["reasoning"],
                                          output_tokens=len(result["output_token_ids"]), finish=result["finish_reason"])), flush=True)
                if (source_state(Path(plan["checkout"])) != implementation["environment"]["source"]
                        or measurement.digest(Path(__file__)) != implementation["environment"]["capture_adapter_sha256"]):
                    raise ValueError("independent producer changed during capture")
                loaded_producer(server.pid, implementation)
                write(output / "reference.json", record)
            finally:
                if server.poll() is None:
                    server.send_signal(signal.SIGINT)  # Only our owned independent process.
                    server.wait(timeout=60)
                write(output / "closed.json", dict(exit_code=server.returncode, gpu_processes_after=gpu(),
                                                   scope="independent producer teardown, not YVEX cleanup"))
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--producer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", action="append", default=[])
    args = parser.parse_args()
    capture(args.inputs, args.producer, args.output, args.case)


if __name__ == "__main__":
    main()
