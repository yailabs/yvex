#!/usr/bin/env python3
"""Native product measurement adapter for manifest-driven qualification suites.

Uses the native test client, never a second implementation of the private wire.
Does not load/unload engines, retry turns, or select a faster product profile.
Raw outputs belong outside the repository. Full-model references are separate.
"""
import argparse
import csv
import hashlib
import http.client
import json
import math
import os
from pathlib import Path
import signal
import shutil
import subprocess
import tarfile
import tempfile
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with open(path, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def canonical(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                    ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def measurement_instrumentation(declared_profiled=False, environment=None):
    """Retain declared/observed profiling state, never infer absence of attachment.

    Only marker names cross into evidence. Environment values may contain local
    paths and are neither disclosed nor hashed into the qualification target.
    External/attached tools must be declared explicitly by the caller.
    """
    if type(declared_profiled) is not bool:
        raise ValueError("profiling declaration must be boolean")
    environment = os.environ if environment is None else environment
    markers = [name for name in ("CUDA_INJECTION64_PATH", "YVEX_DIAGNOSTIC_CUPTI_OUTPUT", "LD_PRELOAD")
               if environment.get(name)]
    return dict(schema="yvex.qualification.instrumentation.v1",
                profiled=declared_profiled or bool(markers), declared_profiled=declared_profiled,
                observed_environment_markers=markers,
                scope="caller declaration and invoking-process markers; not proof against external profiler attachment")


def cuda_processes():
    """Read device/PID facts only; no CLI human output or operator content."""
    result = subprocess.run(["nvidia-smi", "--query-compute-apps=gpu_uuid,pid",
                             "--format=csv,noheader,nounits"], check=True,
                            capture_output=True, text=True, timeout=5)
    if len(result.stdout) > 65536:
        raise ValueError("accelerator process observation exceeds bounded extent")
    rows, seen = [], set()
    for row in csv.reader(result.stdout.splitlines()):
        if len(row) != 2:
            raise ValueError("accelerator process identity unavailable")
        device, pid = (v.strip() for v in row)
        if not device.startswith(("GPU-", "MIG-")) or not pid.isdecimal() or int(pid) <= 0:
            raise ValueError("accelerator process identity unavailable")
        key = (device, int(pid))
        if key in seen:
            raise ValueError("duplicate accelerator process identity")
        seen.add(key)
        rows.append(dict(device=device, pid=int(pid)))
    return rows


class AcceleratorObservation:
    """Sampled interference witness, NOT proof of exclusive hardware ownership.

    Advisory QA locks do not arbitrate ordinary operator sessions. A foreign
    compute process or failed observation poisons this entire measured interval,
    even if it disappears before cleanup. Never signal a foreign process. The
    caller decides whether its own supported cancellation path should be used.
    """
    def __init__(self, path, allowed_pids, devices=(), interval=1, probe=cuda_processes):
        if (not allowed_pids or any(type(p) is not int or p <= 0 for p in allowed_pids)
                or not .1 <= interval <= 10 or any(not d for d in devices)):
            raise ValueError("invalid accelerator observation selection")
        self.path, self.allowed_pids, self.devices = Path(path), set(allowed_pids), set(devices)
        self.interval, self.probe = interval, probe
        self.rows, self.stop, self.lock = [], threading.Event(), threading.Lock()
        self.thread = None

    def sample(self):
        with self.lock:
            started = time.monotonic()
            try:
                processes = self.probe()
                foreign = [p for p in processes if p["pid"] not in self.allowed_pids
                           and (not self.devices or p["device"] in self.devices)]
                row = dict(state="CONTENDED" if foreign else "OBSERVED_CLEAR",
                           processes=processes, foreign=foreign, error=None)
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                row = dict(state="UNKNOWN", processes=None, foreign=None, error=type(error).__name__)
            row.update(started_monotonic=started, completed_monotonic=time.monotonic())
            self.rows.append(row)
            self.stream.write(json.dumps(row, sort_keys=True, allow_nan=False) + "\n")
            self.stream.flush()

    def summary(self):
        with self.lock:
            states = {row["state"] for row in self.rows}
            return dict(schema="yvex.qualification.resource-observation.v1", adapter="cuda-process-list-v1",
                        state="CONTENDED" if "CONTENDED" in states else "UNKNOWN" if
                        "UNKNOWN" in states or not states else "OBSERVED_CLEAR",
                        evidence=str(self.path), allowed_pids=sorted(self.allowed_pids), devices=sorted(self.devices),
                        interval_seconds=self.interval, observations=len(self.rows),
                        probe_seconds=sum(row["completed_monotonic"] - row["started_monotonic"] for row in self.rows),
                        scope="sampled compute-process witness; not exclusive reservation, profiler or numerical evidence")

    def require_clear(self):
        if self.summary()["state"] != "OBSERVED_CLEAR":
            raise ValueError("accelerator measurement contended/unavailable; retain raw evidence, no performance admission")

    def _watch(self):
        while not self.stop.wait(self.interval):
            self.sample()

    def __enter__(self):
        self.stream = self.path.open("x")
        self.sample()
        try:
            self.require_clear()
        except ValueError:
            self.stream.close()
            raise
        self.thread = threading.Thread(target=self._watch, daemon=True)
        self.thread.start()
        return self

    def __exit__(self, kind, value, traceback):
        self.stop.set()
        self.thread.join()
        try:
            self.sample()
        finally:
            self.stream.close()


def watch_owned_command(command, path, allowed_pids, *, env=None, cwd=ROOT,
                        stdout=None, stderr=None, observer_factory=AcceleratorObservation,
                        include_child=False):
    """Wrap only a child with a qualified SIGINT cancellation/cleanup contract.

    This is intended for the Rust qualification command, not the legacy raw
    measurement client. A numerical/transport failure is not retried. If owned
    cancellation cannot settle in 30 seconds, leave its identity for explicit
    reconciliation; never kill the producer or signal a foreign GPU process.
    """
    process, cancelled_at = None, None
    try:
        with observer_factory(path, allowed_pids) as resources:
            process = subprocess.Popen(command, env=env, cwd=cwd, stdout=stdout, stderr=stderr)
            if include_child:
                # Only this exact spawned child, never an unowned GPU process.
                resources.allowed_pids.add(process.pid)
            while True:
                try:
                    returncode = process.wait(timeout=.1)
                    break
                except subprocess.TimeoutExpired:
                    if resources.summary()["state"] != "OBSERVED_CLEAR" and cancelled_at is None:
                        process.send_signal(signal.SIGINT)
                        cancelled_at = time.monotonic()
                    if cancelled_at is not None and time.monotonic() - cancelled_at >= 30:
                        raise RuntimeError(f"owned measurement PID {process.pid} cancellation unsettled; no kill/retry")
    finally:
        if process is not None:
            record = dict(resources.summary(), owned_child_pid=process.pid,
                          cancellation_requested=cancelled_at is not None,
                          owned_child_returncode=process.poll())
            with Path(str(path) + ".summary.json").open("x") as stream:
                json.dump(record, stream, indent=2, allow_nan=False)
    resources.require_clear()
    return returncode


def generation_command(args, row):
    # Native greedy is an explicit strategy with neutral temperature=1; the
    # reference adapter's temperature=0 also selects argmax. Do not pretend
    # that these are identical sampling parameter records or use a rejected
    # native temperature=0 spelling. Neither path performs stochastic draws.
    return [str(args.binary), "bench", "transformer", "generate", "--target", args.target,
            "--artifact", str(args.artifact), "--runtime-binding", str(args.binding),
            "--backend", "cuda", "--text", row["rendered_prompt"],
            "--context-capacity", str(args.context), "--prefill-chunk-tokens", str(args.chunk),
            "--max-new-tokens", str(row["maximum_output"]), "--generation-mode", args.strategy,
            "--strategy", "greedy", "--temperature", "1", "--output", "json"]


def bound_product_build(binary, snapshot):
    """Bind Make/Cargo-authored product provenance, not just captured bytes."""
    value = json.loads(subprocess.check_output([str(binary), "version", "--json"], cwd=ROOT))
    return validate_product_build(value, snapshot)


def validate_product_build(value, snapshot):
    """The same relationship validates a live build and a retained receipt."""
    expected = dict(build_commit=snapshot["head"], source_tree=snapshot["tree"],
                    source_state=snapshot["state"])
    delta = value.get("source_delta_identity")
    delta_matches = delta == snapshot["delta_identity"] or (
        snapshot["state"] == "clean" and snapshot["delta_identity"] == hashlib.sha256(b"").hexdigest()
        and delta in (None, ""))
    if (value.get("schema") != "yvex.version.v1" or any(value.get(k) != v for k,v in expected.items())
            or not delta_matches
            or not value.get("build_identity") or not value.get("shell_build_identity")):
        raise ValueError("product executable is not bound to the measured source snapshot; rebuild first")
    return value


def reference_generation(args):
    """Controlled-engine comparison through typed Rust/C generation JSON.

    Prepared input/capture manifests own workload and reference. This is not
    native product performance, an official vector producer or a new oracle.
    No resident operator host is modified and no failed request is retried.
    """
    import qa
    import qualification as qualification
    import qualification_reference as reference_adapter
    inputs = reference_adapter.read(args.inputs)
    reference = reference_adapter.read(args.reference)
    if (reference.get("schema") != reference_adapter.SCHEMA
            or reference.get("source") != {k:inputs["authority"][k] for k in ("repository", "revision")}
            or reference.get("implementation", {}).get("independent_of_yvex") is not True):
        raise ValueError("exact independent checkpoint reference required")
    cases = reference_adapter.input_cases(inputs, args.case)
    cases = [c for c in cases if c["reasoning"] in args.reasoning and c["workload_class"] != "synthetic"]
    if not cases or set(args.reasoning) - {c["reasoning"] for c in cases}:
        raise ValueError("requested reference workload/mode absent")
    captures = {(r["case"], r["reasoning"]):r for r in reference["cases"]}
    if len(captures) != len(reference["cases"]):
        raise ValueError("duplicate independent capture")
    for row in cases:
        result = captures.get((row["case"], row["reasoning"]), {})
        raw = reference_adapter.checked_raw(args.reference.parent, result.get("raw_evidence_path", ""),
                                            result.get("raw_evidence_sha256"))
        reference_adapter.validate_capture(row, raw, args.reference.parent)
        if raw.get("output_token_ids") != result.get("output_token_ids"):
            raise ValueError("independent continuation summary differs from raw capture")
        if len(row["prompt_token_ids"]) + row["maximum_output"] > args.context:
            raise ValueError("reference workload exceeds declared context")
    closed = reference_adapter.read(args.reference.parent / "closed.json")
    if closed.get("exit_code") != 0 or closed.get("gpu_processes_after") != "":
        raise ValueError("independent producer teardown not qualified")
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        raise ValueError("raw candidate/reference comparison must remain outside Git")
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    retained = retain_source(output / "source-capture", [args.binary])
    initial, executable = source(), digest(args.binary)
    build = bound_product_build(args.binary, initial)
    configuration = dict(context=args.context, prefill_chunk=args.chunk, strategy=args.strategy,
                         backend="cuda", sampling=dict(strategy="greedy", temperature=1,
                             stochastic=False, seed_present=False, top_k=0, top_p=1, min_p=0, typical_p=1),
                         reference_sampling={"temperature":0, "stochastic":False},
                         session_state="fresh", transport="direct-engineering-generation",
                         concurrency=1, lane="controlled-engine", performance_claim=False)
    reference_adapter.write(output / "identity.json", dict(source=initial, source_capture=retained, build=build,
        executable_sha256=executable, artifact_sha256=digest(args.artifact),
        binding_file_sha256=digest(args.binding), reference_sha256=digest(args.reference),
        inputs_sha256=digest(args.inputs), configuration=configuration))
    registry, _ = qa.load_registry()
    with qa.resource_locks(registry, ["cuda-device", "gb10-live-model", "benchmark-directory"]):
        if cuda_processes():
            raise ValueError("operator/GPU occupied; no controlled reference admission")
        for index,row in enumerate(cases):
            command = generation_command(args, row)
            path = output / f"candidate-{index}.json"
            with path.open("x") as out, (output / f"candidate-{index}.stderr").open("x") as err:
                rc = watch_owned_command(command, output / f"candidate-{index}.resources.jsonl", [os.getpid()],
                                         stdout=out, stderr=err, include_child=True)
            if source() != initial or digest(args.binary) != executable:
                raise ValueError("candidate source/executable changed during comparison")
            if rc:
                raise RuntimeError(f"candidate failure for {row['case']}/{row['reasoning']}; retained, no retry")
            candidate = reference_adapter.read(path)
            qualification.require(all(candidate.get(k) == v for k,v in dict(context_capacity=args.context,
                prefill_chunk_tokens=args.chunk, maximum_new_tokens=row["maximum_output"]).items()),
                "native admitted generation geometry differs from requested comparison")
            result = qualification.continuation_agreement(row, captures[(row["case"], row["reasoning"])],
                candidate, args.strategy)
            observation = dict(schema="yvex.qualification.continuation-observation.v1",
                case=row["case"], reasoning=row["reasoning"], input_identity=row["input_identity"],
                source_stable=True, candidate_sha256=digest(path),
                reference_capture_sha256=captures[(row["case"], row["reasoning"])]["raw_evidence_sha256"],
                configuration=configuration, comparison=result)
            with (output / "observations.jsonl").open("a") as stream:
                stream.write(json.dumps(observation, sort_keys=True, allow_nan=False) + "\n")
            print(json.dumps(dict(case=row["case"], reasoning=row["reasoning"], comparison=result)), flush=True)
    reference_adapter.write(output / "closed.json", dict(source_stable=source() == initial,
        executable_unchanged=digest(args.binary) == executable, gpu_processes_after=cuda_processes()))


def continuation_receipts(args):
    """Authenticate a closed comparison and project generic per-mode receipts.

    No inference, model admission or current-source substitution occurs here.
    Publication preserves the source actually captured, including missing facts.
    """
    import qualification as q
    import qualification_reference as independent
    run = args.run.resolve()
    identity = independent.read(run / "identity.json")
    closed = independent.read(run / "closed.json")
    q.require(closed == dict(source_stable=True, executable_unchanged=True, gpu_processes_after=[]),
              "candidate source/teardown was not qualified")
    inputs, reference = independent.read(args.inputs), independent.read(args.reference)
    suite = corpus(args.suite)
    q.require(digest(args.inputs) == identity["inputs_sha256"]
              and digest(args.reference) == identity["reference_sha256"]
              and digest(args.suite) == inputs["corpus_sha256"], "comparison input/reference/suite identity differs")
    authority = {k:inputs["authority"][k] for k in ("repository", "revision")}
    q.require(reference.get("schema") == independent.SCHEMA and reference.get("source") == authority
              and reference.get("implementation", {}).get("independent_of_yvex") is True,
              "exact independent producer required")
    reference_closed = independent.read(args.reference.parent / "closed.json")
    q.require(reference_closed.get("exit_code") == 0 and reference_closed.get("gpu_processes_after") == "",
              "independent producer teardown was not qualified")
    capture = independent.read(run / "source-capture/manifest.json")
    q.require(capture == identity["source_capture"]
              and dict(capture["source"], tree=capture["tree"]) == identity["source"], "source capture identity differs")
    for name, expected in capture["files"].items():
        path = (run / "source-capture" / name).resolve()
        q.require(not Path(name).is_absolute() and path.is_relative_to((run / "source-capture").resolve())
                  and path.is_file() and digest(path) == expected, "captured source/executable bytes differ")
    build, snapshot = identity["build"], identity["source"]
    q.require(all(build.get(k) == snapshot[v] for k,v in dict(build_commit="head", source_tree="tree",
        source_state="state", source_delta_identity="delta_identity").items())
        and build.get("build_identity") and build.get("shell_build_identity"), "captured build/source relationship differs")
    q.require(capture["files"]["executable-0"] == identity["executable_sha256"], "captured executable differs")
    requests = {(r["case"], r["reasoning"]):r for r in independent.input_cases(inputs, ())}
    references = {(r["case"], r["reasoning"]):r for r in reference["cases"]}
    rows = [json.loads(line) for line in (run / "observations.jsonl").read_text().splitlines()]
    q.require(0 < len(rows) <= 64 and len({(r["case"], r["reasoning"]) for r in rows}) == len(rows),
              "invalid comparison population")
    groups = {}
    for index, row in enumerate(rows):
        key = (row["case"], row["reasoning"])
        request, baseline = requests[key], references[key]
        candidate = independent.checked_raw(run, f"candidate-{index}.json", row["candidate_sha256"])
        resource = independent.read(run / f"candidate-{index}.resources.jsonl.summary.json")
        q.require(resource.get("state") == "OBSERVED_CLEAR" and resource.get("owned_child_returncode") == 0
                  and resource.get("cancellation_requested") is False, "candidate resource interval unavailable/contended")
        raw = independent.checked_raw(args.reference.parent, baseline["raw_evidence_path"], baseline["raw_evidence_sha256"])
        actual_reference = independent.validate_capture(request, raw, args.reference.parent)
        q.require(baseline["output_token_ids"] == actual_reference["output_token_ids"],
                  "reference summary differs from authenticated producer response")
        result = q.continuation_agreement(request, baseline, candidate, identity["configuration"]["strategy"])
        q.require(row["configuration"] == identity["configuration"] and row["source_stable"] is True
                  and row["input_identity"] == request["input_identity"]
                  and row["reference_capture_sha256"] == baseline["raw_evidence_sha256"]
                  and row["comparison"] == result, "recorded comparison differs from authenticated raw execution")
        groups.setdefault(row["reasoning"], []).append((dict(row, comparison=result,
            prompt_tokens=len(request["prompt_token_ids"]), maximum_output=request["maximum_output"],
            evidence=str(run / f"candidate-{index}.json")), candidate))
    output = args.output.resolve()
    q.require(not output.is_relative_to(ROOT), "generated receipts must be retained outside Git before publication")
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    config = identity["configuration"]
    for mode, values in groups.items():
        target = dict(schema=q.TARGET_SCHEMA, **{key:None for key in q.TARGET_FIELDS})
        candidate = values[0][1]
        target.update(family_contract=candidate["family"], upstream_repository=authority["repository"],
            checkpoint=authority["revision"], artifact_set=identity["artifact_sha256"],
            representation="artifact-sha256:" + identity["artifact_sha256"],
            source_commit=snapshot["head"], source_tree=snapshot["tree"], source_delta=snapshot["delta_identity"],
            build=build["build_identity"], executable=identity["executable_sha256"], backend=config["backend"],
            context=config["context"], prefill_geometry=f"chunk={config['prefill_chunk']}", sequence_geometry="width=1",
            concurrency=config["concurrency"], strategy="speculative" if config["strategy"] == "dspark" else "target-only",
            reasoning=mode, sampling=q.canonical(config["sampling"]), product_path="controlled-engine",
            suite=q.identity(suite))
        for field, key in dict(binding="runtime_binding_identity", tokenizer_conversation="prompt_policy_identity",
                               kernel_bundle="kernel_bundle_identity", runtime_configuration="generation_plan_identity").items():
            value = candidate.get(key)
            q.require(all(other.get(key) == value for _,other in values), "plan identity changed within target")
            # Generation plan includes the output bound, so heterogeneous cases
            # may need independent targets rather than an invented shared plan.
            if value:
                target[field] = value
        if "execution_class" in candidate and "evidence_profile" in candidate:
            target["backend_implementation"] = f"execution-class={candidate['execution_class']}; evidence-profile={candidate['evidence_profile']}"
        producer = reference["implementation"]
        compact_producer = {key:producer[key] for key in ("name", "revision", "executable_sha256", "command", "independent_of_yvex")}
        compact_producer.update(environment_identity=q.identity(producer["environment"]),
                                representation_identity=q.identity(producer["physical_representation"]))
        provenance = dict(source_stability="frozen", evidence_class="real-independent-continuation-characterization",
            raw_root=str(run), source_capture=capture, build=build, configuration=config,
            reference_sha256=identity["reference_sha256"], inputs_sha256=identity["inputs_sha256"],
            independent_implementation=compact_producer,
            binding_file_sha256=identity["binding_file_sha256"], profiled=False,
            resource_scope="sampled process witness; not uninterrupted exclusive hardware reservation",
            closed_sha256=digest(run / "closed.json"))
        receipt = q.continuation_receipt(target, [row for row,_ in values], provenance,
            f"{args.id}-{mode}", f"{args.title} — {mode}", args.origin)
        independent.write(output / (receipt["id"] + ".json"), receipt)
        print(json.dumps(dict(id=receipt["id"], target_identity=receipt["target_identity"],
                             cases=len(values), state="CHARACTERIZED")), flush=True)


def authenticate_source_capture(run, directory, expected, snapshot, executables):
    """Authenticate replayable captured bytes without consulting today's source."""
    import qualification as q
    import qualification_reference as independent
    root = (run / directory).resolve()
    q.require(not (run / directory).is_symlink() and root.is_relative_to(run.resolve())
              and not (root / "manifest.json").is_symlink(), "source capture escaped evidence directory")
    capture = independent.read(root / "manifest.json")
    q.require(capture.get("schema") == "yvex.qualification.source-capture.v1" and capture == expected
              and dict(capture["source"], tree=capture["tree"]) == snapshot and isinstance(capture["files"], dict)
              and all(k in capture["files"] for k in ("base.tar", "delta.patch", "untracked.tar")),
              "source capture differs from recorded producer/adapter")
    for name, value in capture["files"].items():
        path = (root / name).resolve()
        q.require(Path(name).name == name and not Path(name).is_absolute() and path.is_relative_to(root)
                  and not (root / name).is_symlink() and path.is_file() and digest(path) == value,
                  "captured source/executable bytes differ")
    q.require(all(capture["files"].get(name) == value for name,value in executables.items()),
              "captured executable identity differs")


def evidence_rows(path):
    """Bounded regular JSONL evidence; malformed rows never become an empty run."""
    import qualification as q
    q.require(path.is_file() and not path.is_symlink() and path.stat().st_size <= 16 * 1024 * 1024,
              "evidence exceeds bounded regular-file extent")
    rows = []
    with path.open() as stream:
        for line in stream:
            q.require(len(line) <= 1024 * 1024 and len(rows) < 65536, "evidence row extent exceeded")
            row = json.loads(line)
            q.require(isinstance(row, dict), "evidence row is not a record")
            rows.append(row)
    q.require(rows, "empty evidence interval")
    return rows


def native_resource_interval(run, repetition, pid):
    """Raw process observations, not a trusted all-clear summary or reservation."""
    import qualification as q
    import qualification_reference as independent
    summary = independent.read(run / f"native-{repetition}.resource-summary.json")
    path = run / f"native-{repetition}.resources.jsonl"
    rows = evidence_rows(path)
    q.require(summary.get("schema") == "yvex.qualification.resource-observation.v1"
              and summary.get("adapter") == "cuda-process-list-v1" and type(pid) is int and pid > 0
              and summary.get("state") == "OBSERVED_CLEAR" and summary.get("allowed_pids") == [pid]
              # This native adapter observes every device. Import cannot add a
              # filter that would hide another process after the experiment.
              and summary.get("devices") == [] and summary.get("evidence") == str(path)
              and type(summary.get("observations")) is int and summary["observations"] == len(rows),
              "resource interval unavailable/contended")
    previous = None
    for row in rows:
        start, end = row.get("started_monotonic"), row.get("completed_monotonic")
        q.require(type(start) in (int,float) and type(end) in (int,float)
                  and math.isfinite(start) and math.isfinite(end) and start <= end
                  and (previous is None or previous <= start), "invalid resource observation timing")
        previous = end
        processes = row.get("processes")
        q.require(isinstance(processes, list) and len(processes) <= 4096
                  and all(isinstance(p, dict) and type(p.get("pid")) is int and p["pid"] > 0
                  and isinstance(p.get("device"), str) and p["device"].startswith(("GPU-", "MIG-"))
                  for p in processes), "raw accelerator identity unavailable")
        q.require(len({(p["device"], p["pid"]) for p in processes}) == len(processes),
                  "duplicate accelerator process identity")
        foreign = [p for p in processes if p["pid"] != pid]
        q.require(row.get("state") == "OBSERVED_CLEAR" and row.get("foreign") == foreign == []
                  and row.get("error") is None, "raw resource interval unavailable/contended")
    return summary


def native_measurements(config, rows, scope, evidence, skip_first):
    """Group actual input histories, never pool reused turns from different text."""
    import qualification as q
    groups, populations = {}, []
    for row in rows:
        if row["repetition"] < skip_first:
            continue
        c, m = row["configuration"], row["metrics"]
        key = (c["turn_index"], row["input_identity"])
        groups.setdefault(key, []).append(m)
    fields = {"admission.client":"client_admitted_seconds", "prefill.wall":"prefill_seconds",
              "ttft.server":"server_first_token_seconds", "ttft.client-visible":"client_first_visible_seconds",
              "request.client-complete":"client_complete_seconds",
              "reasoning.first.server":"first_reasoning_seconds", "reasoning.first.client":"client_first_reasoning_seconds",
              "final.first.server":"first_final_seconds", "final.first.client":"client_first_final_seconds"}
    measurements = []
    for (turn, input_identity), values in sorted(groups.items()):
        split = sum(index == turn for index,_ in groups) > 1
        case = f"{config['case']}/turn-{turn}" + ("/input-" + input_identity if split else "")
        for m in values:
            populations.append(dict(m, case=case, input_identity=input_identity))
        for metric in (*fields, "prefill.uncached", "decode.post-first.committed",
                       "reasoning.phase-rate", "final.phase-rate"):
            def value(m):
                if metric.startswith("reasoning.") and not m.get("reasoning_tokens"):
                    return None
                if metric.startswith("final.") and not m.get("final_tokens"):
                    return None
                if metric == "prefill.uncached":
                    return m["prefill_rate"] if not m["reused_tokens"] and m["prefill_tokens"] else None
                if metric == "decode.post-first.committed":
                    return m["post_first_decode_rate"] if m.get("post_first_decode_units", 0) >= 32 else None
                if metric in ("reasoning.phase-rate", "final.phase-rate"):
                    prefix = metric.split(".")[0]
                    return m[prefix + "_tokens"] / m[prefix + "_seconds"] if m.get(prefix + "_seconds", 0) > 0 else None
                return m.get(fields[metric])
            samples = [value(m) for m in values]
            if any(v is None for v in samples):
                continue
            q.require(all(type(v) in (int,float) and math.isfinite(v) and v >= 0 for v in samples),
                      "invalid native metric")
            _, unit, definition = q.METRICS[metric]
            measurements.append(dict(metric=metric, unit=unit, definition=definition, case=case,
                prompt_identity=input_identity, reference_identity=None, session_state="fresh" if turn == 0 else "reused",
                warm_state=config["warm_state"] + f"; first {skip_first} repetitions excluded explicitly",
                output_bound=config["output_bound"], samples=samples, statistics=q.statistics_for(samples),
                scope=scope, evidence=evidence))
    q.require(measurements, "no admitted native measurements")
    return measurements, populations


def native_receipts(args):
    """Import a closed source-bound native capture; never infer missing identities."""
    import qualification as q
    import qualification_reference as independent
    run = args.run.resolve()
    identity, closed = independent.read(run / "identity.json"), independent.read(run / "closed.json")
    instrumentation = identity.get("instrumentation")
    q.require(isinstance(instrumentation, dict)
              and instrumentation.get("schema") == "yvex.qualification.instrumentation.v1"
              and instrumentation.get("profiled") is False
              and instrumentation.get("declared_profiled") is False
              and instrumentation.get("observed_environment_markers") == []
              and closed.get("instrumentation") == instrumentation,
              "profiled/unknown native capture cannot become performance evidence")
    config, engine, snapshot, adapter = (identity[k] for k in ("configuration", "engine", "source", "adapter"))
    q.require(closed.get("schema") == "yvex.qualification.native-closed.v1"
              and all(closed.get(k) is True for k in ("source_unchanged", "adapter_unchanged", "executable_unchanged")),
              "native source/executable closure unavailable")
    current = closed["engine"]
    stable = ("alias", "generation", "model_identity", "artifact_identity", "runtime_binding_identity",
              "specialization_identity", "context_capacity", "prefill_chunk_tokens", "execution_strategy", "sessions")
    q.require(all(current.get(k) == engine.get(k) for k in stable)
              and all(current.get(k) == 0 for k in ("active_work", "attached_clients", "model_leases"))
              and all(closed["host"].get(k) == 0 for k in ("active_requests", "active_http_requests", "queue_depth")),
              "native generation/cleanup closure differs")
    q.require(type(engine.get("generation")) is int and engine["generation"] > 0
              and identity.get("lane") in ("product-native", "controlled-engine")
              and config.get("sampling") in ("greedy", "product") and config.get("concurrency") == 1
              and all(config.get(k) == engine.get(v) and config.get(k) for k,v in
                      dict(model="model_identity", artifact="artifact_identity", binding="runtime_binding_identity",
                           specialization="specialization_identity").items()),
              "native producer identity/configuration differs")
    validate_product_build(identity["build"], snapshot)
    authenticate_source_capture(run, "source-capture", identity["source_capture"], snapshot,
                                {"executable-0":config["executable"], "executable-1":identity["client"]})
    authenticate_source_capture(run, "adapter-source-capture", adapter["source_capture"], adapter["source"],
                                {"executable-0":adapter["client_sha256"]})
    q.require(adapter["client_sha256"] == identity["client"], "measurement client differs")
    suite = corpus(args.suite)
    q.require(digest(args.suite) == config["corpus"] and suite["applicability"] == identity["source_authority"],
              "suite identity/applicability differs")
    selected = next(c for c in suite["cases"] if c["id"] == config["case"])
    q.require(config["strategy"] == engine["execution_strategy"] in selected["execution_strategies"]
              and config["reasoning"] in selected["reasoning_modes"] and config["output_bound"] == selected["maximum_output"]
              and config["context"] == engine["context_capacity"] and config["prefill_chunk"] == engine["prefill_chunk_tokens"],
              "suite/producer configuration differs")
    relationship = q.validate(independent.read(args.relationship))
    lineage = relationship["target"]
    q.require(lineage["artifact_set"] == config["artifact"] == engine["artifact_identity"]
              and lineage["binding"] == config["binding"] == engine["runtime_binding_identity"]
              and lineage["upstream_repository"] == suite["applicability"]["repository"]
              and lineage["checkpoint"] == suite["applicability"]["revision"],
              "no exact artifact/binding/checkpoint relationship")
    observations = evidence_rows(run / "observations.jsonl")
    q.require(all(type(r.get("repetition")) is int and r["repetition"] >= 0 for r in observations)
              and type(args.skip_first) is int and args.skip_first in (0, 1), "invalid repetition selection")
    repetitions = sorted({r["repetition"] for r in observations})
    q.require(repetitions == list(range(len(repetitions))) and 1 <= len(repetitions) <= 20
              and args.skip_first < len(repetitions), "invalid repetition selection")
    prompt_paths = [run / f"prompt-{i}.txt" for i in range(len(prompts(selected)))]
    q.require(all(path.read_text() == text for path,text in zip(prompt_paths, prompts(selected))), "workload text changed")
    verified, raw_files = [], {}
    for repetition in repetitions:
        path = run / f"native-{repetition}.jsonl"
        raw = evidence_rows(path)
        q.require(sum(r.get("kind") == "started" for r in raw) == len(prompt_paths),
                  "native admission acknowledgement missing")
        actual = summarize_native(raw)
        recorded = [r for r in observations if r["repetition"] == repetition]
        q.require(len(actual) == len(recorded) == len(prompt_paths), "missing native turn evidence")
        resources = native_resource_interval(run, repetition, identity["host_pid"])
        for name in (path.name, f"native-{repetition}.resources.jsonl", f"native-{repetition}.resource-summary.json"):
            raw_files[name] = digest(run / name)
        history = []
        for turn, (metrics, row) in enumerate(zip(actual, recorded)):
            expected = dict(config, session_state="fresh" if turn == 0 else "reused", turn_index=turn,
                            prompt_sha256=digest(prompt_paths[turn]))
            q.require(row.get("schema") == "yvex.qualification.measurement.v1"
                      and row["configuration"] == expected and row["source"] == snapshot and row["source_stable"] is True
                      and row.get("instrumentation") == instrumentation
                      and row["resources"] == resources and row["metrics"] == metrics and row["lane"] == identity["lane"]
                      and 0 < metrics["generated_tokens"] <= config["output_bound"], "native observation differs from raw execution")
            input_identity = canonical(dict(prompt=expected["prompt_sha256"], previous_published_history=history,
                prompt_tokens=metrics["prompt_tokens"], reused_tokens=metrics["reused_tokens"]))
            verified.append(dict(row, input_identity=input_identity))
            history.append(metrics["content_sha256"])
    scope = f"Closed {identity['lane']} capture; exact loaded configuration and explicit sampling/output override; CHARACTERIZED only"
    measurements, populations = native_measurements(config, verified, scope, str(run / "observations.jsonl"), args.skip_first)
    target = dict(schema=q.TARGET_SCHEMA, **{key:None for key in q.TARGET_FIELDS})
    for key in ("family_contract", "upstream_repository", "checkpoint", "tokenizer_conversation",
                "transformation_ir", "physical_policy", "representation"):
        target[key] = lineage[key]
    target.update(artifact_set=config["artifact"], binding=config["binding"], specialization=config["specialization"],
        source_commit=snapshot["head"], source_tree=snapshot["tree"], source_delta=snapshot["delta_identity"],
        build=identity["build"]["build_identity"], executable=config["executable"], backend=identity["profile"]["backend"],
        runtime_configuration=engine.get("capacity_plan_identity") or None, context=config["context"],
        prefill_geometry=f"chunk={config['prefill_chunk']}", sequence_geometry=f"width={engine['capacity']['physical_sequence_width']}",
        concurrency=config["concurrency"], strategy=config["strategy"], reasoning=config["reasoning"],
        sampling=config["sampling"], product_path=f"{identity['lane']}/{config['transport']}", suite=q.identity(suite))
    capture_evidence = {"native-capture":dict(sha256=digest(run / "observations.jsonl"),
        locator=str(run / "observations.jsonl"), result="PASS")}
    claims = {plane:dict(state="CHARACTERIZED" if plane in ("deployment-performance", "product-path") else "UNQUALIFIED",
        scope=scope if plane in ("deployment-performance", "product-path") else "Not qualified by timing capture",
        required_evidence=[], evidence=capture_evidence if plane in ("deployment-performance", "product-path") else {},
        blockers=[]) for plane in q.PLANES}
    receipt = dict(schema=q.RECEIPT_SCHEMA, id=args.id, title=args.title, target=target,
        target_identity=q.target_id(target), origin=args.origin, claims=claims, measurements=measurements,
        provenance=dict(source_stability="frozen", profiled=instrumentation["profiled"],
            instrumentation=instrumentation, evidence_class="native-characterization",
            raw_root=str(run), build=identity["build"], adapter=adapter, source_capture=identity["source_capture"],
            identity_sha256=digest(run / "identity.json"), closed_sha256=digest(run / "closed.json"),
            relationship_sha256=digest(args.relationship), relationship_target=relationship["target_identity"],
            raw_files=raw_files, configuration=config, observations=populations,
            skipped_initial_repetitions=args.skip_first, resource_scope="sampled process witness; not uninterrupted reservation"),
        limitations=["Timing capture does not qualify model quality, upstream conformance or another quantization.",
            "Suite applicability is joined to the exact artifact/binding catalog relationship, not inferred from a model name.",
            "Producer and adapter source/build identities remain distinct. Unknown hardware/kernel dimensions refuse full comparison.",
            "Native measurements are neither HTTP timing nor terminal rendering; client TTFT includes dispatch/admission.",
            "Different published histories remain separate input groups. Outputs shorter than 33 committed tokens have no sustained rate.",
            "First publication and unreached reasoning-to-final transition remain NOT MEASURED. No load timing is inferred.",
            "Sampled clear intervals are not uninterrupted hardware reservations; no global model-throughput or release claim."])
    q.validate(receipt)
    output = args.output.resolve()
    q.require(not output.is_relative_to(ROOT), "retain receipt outside Git before publication")
    independent.write(output, receipt)
    print(json.dumps(dict(id=receipt["id"], target_identity=receipt["target_identity"], state="CHARACTERIZED")))


def corpus(path):
    value = json.loads(path.read_text())
    if value["schema"] != "yvex.qualification.suite.v1":
        raise ValueError("unsupported corpus")
    ids = [case["id"] for case in value["cases"]]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate case")
    for case in value["cases"]:
        for axis in ("reasoning_modes", "execution_strategies"):
            if not case.get(axis) or len(set(case[axis])) != len(case[axis]):
                raise ValueError("empty/duplicate workload axis: " + axis)
    return value


def corpora(directory):
    """Discover suite authority by schema, never by family/file-name spelling.

    Unrelated vector schemas are not suites. Unknown suite versions and duplicate
    operator selectors refuse the entire projection instead of hiding or picking
    one workload arbitrarily.
    """
    suites, selectors = [], set()
    for path in sorted(Path(directory).glob("*.json")):
        value = json.loads(path.read_text())
        schema = value.get("schema") if isinstance(value, dict) else None
        if not isinstance(schema, str) or not schema.startswith("yvex.qualification.suite."):
            continue
        suite = corpus(path)
        selector = suite.get("id")
        if not isinstance(selector, str) or not selector:
            raise ValueError("invalid qualification suite selector")
        if selector in selectors:
            raise ValueError("duplicate qualification suite selector: " + selector)
        selectors.add(selector)
        suites.append(suite)
    return suites


def configurations(suite):
    """One logical case, explicit source-authored reasoning/strategy axes."""
    for case in suite["cases"]:
        for reasoning in case["reasoning_modes"]:
            for strategy in case["execution_strategies"]:
                yield dict(case=case["id"], reasoning=reasoning, strategy=strategy,
                           state="UNQUALIFIED", workload_class=case["class"])


def prompts(case):
    values = list(case["turns"])
    if "repeat" in case:
        spec = case["repeat"]
        values[0] += "".join(spec["template"].format(index=i) for i in range(spec["count"]))
    return values


def summarize_native(rows):
    """Fragments are byte deliveries, NOT token counts. Rates come from the server."""
    def elapsed(value):
        return type(value) in (int, float) and math.isfinite(value) and value >= 0
    result, fragments, started = [], [], None
    for row in rows:
        if row["kind"] == "started":
            seconds, request = row.get("seconds"), row.get("request")
            if (started is not None or fragments or type(request) is not int or request <= 0
                    or not elapsed(seconds)):
                raise ValueError("invalid native TURN_STARTED acknowledgement")
            started = row
        elif row["kind"] == "fragment":
            if started is not None and (row.get("request") != started["request"]
                                       or not elapsed(row.get("seconds"))
                                       or row["seconds"] < started["seconds"]):
                raise ValueError("native fragment differs from TURN_STARTED request/timing")
            fragments.append(row)
        elif row["kind"] == "turn":
            if started is not None and (row.get("request") != started["request"]
                                       or not elapsed(row.get("client_complete_seconds"))
                                       or row["client_complete_seconds"] < started["seconds"]):
                raise ValueError("native terminal differs from TURN_STARTED request/timing")
            if row["reused_tokens"] > row["prompt_tokens"]:
                raise ValueError("invalid reused token count")
            if row["prefill_tokens"] != row["prompt_tokens"] - row["reused_tokens"]:
                raise ValueError("prefill population is not newly executed input")
            visible = [f for f in fragments if f["hex"] and f["channel"] in (1, 2)]
            channels = {channel: b"".join(bytes.fromhex(f["hex"]) for f in fragments
                                         if f["channel"] == channel) for channel in (1, 2)}
            observation = dict(row, client_first_visible_seconds=visible[0]["seconds"] if visible else None,
                               client_admitted_seconds=started["seconds"] if started else None,
                               client_first_final_seconds=next((f["seconds"] for f in visible if f["channel"] == 1), None),
                               client_first_reasoning_seconds=next((f["seconds"] for f in visible if f["channel"] == 2), None),
                               first_fragment_publication_seconds=None,
                               first_fragment_publication_status="not-projected-by-terminal-summary",
                               content_sha256={str(k): hashlib.sha256(v).hexdigest() for k, v in channels.items()})
            # A tool/control fragment cannot establish first visible answer/reasoning.
            result.append(observation)
            fragments, started = [], None
    if fragments or started is not None:
        raise ValueError("unterminated native turn; delivery indeterminate")
    return result


def fragment_manifests(rows):
    """Comparable delivery evidence, not token IDs or numerical equivalence.

    The native generated-token identity includes execution/state lineage; it
    is not a hash of token IDs alone. Matching ordered fragment bounds, channel
    and byte digests proves bounded published-content agreement. Different
    packetization remains inconclusive without the underlying bytes.
    """
    turns, pending = [], None
    for row in rows:
        kind = row.get("kind")
        if kind == "dispatch":
            if pending is not None:
                raise ValueError("unterminated fragment delivery")
            pending = []
        elif kind == "fragment":
            if pending is None:
                raise ValueError("fragment without dispatch")
            channel, size, digest_value = row.get("channel"), row.get("bytes"), row.get("sha256")
            if (type(channel) is not int or channel < 0 or type(size) is not int or size < 0
                    or not isinstance(digest_value, str) or len(digest_value) != 64
                    or any(c not in "0123456789abcdef" for c in digest_value)):
                raise ValueError("malformed fragment identity")
            pending.append(dict(channel=channel, bytes=size, sha256=digest_value))
        elif kind == "turn":
            if pending is None:
                raise ValueError("terminal turn without dispatch")
            turns.append(pending)
            pending = None
        elif kind in ("refused", "unsettled"):
            raise ValueError("failed delivery is not completed-content evidence")
    if pending is not None:
        raise ValueError("unterminated fragment delivery")
    if not turns:
        raise ValueError("no completed fragment delivery")
    return turns


def source(root=None):
    # QA owns the source delta algorithm, including untracked source membership.
    import qa
    root = Path(root or ROOT).resolve()
    return dict(qa.source_snapshot(root), tree=qa.git_capture(root, ["rev-parse", "HEAD^{tree}"]))


def retain_source(output, executables, root=None):
    """Retain replayable source, not just an unrecoverable dirty-tree digest.

    This binds observed files; it does not prove a binary was built from them.
    Ignored files, runtime assets and credentials are never swept into the archive.
    """
    import qa
    root, output = Path(root or ROOT).resolve(), output.resolve()
    if output.is_relative_to(root):
        raise ValueError("source evidence must remain outside the repository")
    before = qa.source_snapshot(root)
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    def git(*args):
        return subprocess.check_output(["git", *args], cwd=root)
    with (output / "base.tar").open("xb") as stream:
        subprocess.run(["git", "archive", "--format=tar", before["head"]],
                       cwd=root, stdout=stream, check=True)
    (output / "delta.patch").write_bytes(git("diff", "--binary", "--no-ext-diff", "HEAD", "--", "."))
    untracked = sorted(p for p in git("ls-files", "--others", "--exclude-standard", "-z").decode().split("\0")
                       if p and "__pycache__/" not in p and not p.endswith(".pyc"))
    with tarfile.open(output / "untracked.tar", "x") as archive:
        for name in untracked:
            path = root / name
            if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root):
                raise ValueError("untracked evidence is not a bounded regular source file")
            archive.add(path, arcname=name, recursive=False)
    binaries = {}
    for index, path in enumerate(executables):
        path = Path(path)
        expected = digest(path)
        destination = output / f"executable-{index}"
        with path.open("rb") as src, destination.open("xb") as dst:
            shutil.copyfileobj(src, dst)
        if digest(path) != expected or digest(destination) != expected:
            raise ValueError("executable changed during evidence capture")
        binaries[destination.name] = dict(observed_path=str(path), sha256=expected)
    if qa.source_snapshot(root) != before:
        raise ValueError("source changed during evidence capture")
    record = dict(schema="yvex.qualification.source-capture.v1", source=before,
                  tree=git("rev-parse", "HEAD^{tree}").decode().strip(), executables=binaries,
                  files={p.name:digest(p) for p in sorted(output.iterdir())},
                  scope="observed source and executable bytes; build relationship separately required")
    with (output / "manifest.json").open("x") as stream:
        json.dump(record, stream, indent=2)
    return record


def http_sampling(selection):
    """Explicit adapter request, following the admitted OpenAI sampling contract.

    Native defaults with stochastic=0/temperature=1 are NOT equivalent to omitted
    OpenAI temperature: the compatibility adapter derives stochastic from it.
    A server-generated seed is unavailable, not zero or a reproducible seed.
    """
    if selection not in ("product", "greedy"):
        raise ValueError("unsupported sampling selection")
    # This adapter admits temperature/top_p, not native top_k/min_p/typical_p
    # fields. Their neutral values originate in provider_request_default.
    request = dict(temperature=0 if selection == "greedy" else 1, top_p=1)
    facts = dict(request, top_k=0, min_p=0, typical_p=1,
                 stochastic=selection != "greedy", seed=None,
                 seed_origin="not-applicable" if selection == "greedy" else "server-generated-not-projected")
    return request, facts


def read_http_stream(response, started, journal, clock=time.monotonic):
    """Public SSE adapter only. Native timing is never inferred from this stream."""
    if response.status != 200:
        raise ValueError(f"HTTP refusal {response.status}: " + response.read(65536).decode(errors="replace"))
    channels = {"content": "", "reasoning_content": ""}
    first = {key: None for key in channels}
    usage, metrics, request_id, finish = None, None, None, None
    total = 0
    while True:
        line = response.readline(1024 * 1024 + 1)
        total += len(line)
        if len(line) > 1024 * 1024 or total > 16 * 1024 * 1024:
            raise ValueError("HTTP evidence exceeds bounded stream extent; no retry")
        if not line:
            raise ValueError("HTTP delivery indeterminate: missing DONE; no retry")
        arrived = clock() - started
        journal.write(json.dumps({"seconds": arrived, "line": line.decode("utf-8")}) + "\n")
        journal.flush()
        if not line.startswith(b"data:"):
            continue  # SSE comments are liveness, not model tokens.
        data = line[5:].strip()
        if data == b"[DONE]":
            if not request_id or usage is None or finish is None:
                raise ValueError("HTTP terminal evidence incomplete")
            return dict(client_complete_seconds=arrived, client_first_visible_seconds=min(
                            (t for t in first.values() if t is not None), default=None),
                        client_first_final_seconds=first["content"],
                        client_first_reasoning_seconds=first["reasoning_content"],
                        server_completion_metrics=metrics, usage=usage, request_id=request_id,
                        finish_reason=finish, content=channels,
                        server_first_token_seconds=None, first_fragment_publication_seconds=None)
        value = json.loads(data)
        if value.get("error"):
            raise ValueError("HTTP generation failed: " + json.dumps(value["error"]))
        identity = value.get("id")
        if identity:
            if request_id is not None and request_id != identity:
                raise ValueError("HTTP response identity changed")
            request_id = identity
        if value.get("usage") is not None:
            usage = value["usage"]
        if value.get("yvex_completion_metrics") is not None:
            metrics = value["yvex_completion_metrics"]
        for choice in value.get("choices", []):
            if choice.get("index", 0) != 0:
                raise ValueError("multiple HTTP choices not admitted by this suite adapter")
            finish = choice.get("finish_reason") or finish
            for channel in channels:
                text = choice.get("delta", {}).get(channel)
                if text:
                    if not isinstance(text, str):
                        raise ValueError("non-text HTTP content")
                    if first[channel] is None:
                        first[channel] = arrived
                    channels[channel] += text


def correlate_http_events(events, request_id, pid, after_sequence):
    """Join by producer correlation/sequence, not wall time or neighboring logs."""
    selected = [e for e in events if e.get("external_correlation_id") == request_id
                and e.get("process") == pid and e.get("sequence", 0) > after_sequence]
    owners = {(e["process"], e["session"], e["request"]) for e in selected}
    if len(owners) != 1 or not any(e["kind"] == "generation.completed" for e in selected):
        raise ValueError("HTTP producer correlation unavailable/ambiguous; no inferred compute timing")
    return selected


def http_lane(args):
    """Resident loopback compatibility lane; no load, profile switch or redispatch."""
    import qa
    suite = corpus(args.suite)
    selected = next(c for c in suite["cases"] if c["id"] == args.case)
    if args.reasoning not in selected["reasoning_modes"]:
        raise ValueError("reasoning mode not admitted by suite")
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        raise ValueError("raw measurement must be outside the repository")
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(args.runtime_dir), NO_COLOR="1")
    def read(*words):
        return json.loads(subprocess.check_output([str(args.binary), *words], env=env, cwd=ROOT))
    def events():
        return [json.loads(line) for line in subprocess.check_output(
            [str(args.binary), "host", "logs", "--json"], env=env, cwd=ROOT).decode().splitlines()]
    capture = retain_source(output / "source-capture", [args.binary])
    initial = source()
    if capture["source"] != {k:initial[k] for k in capture["source"]}:
        raise ValueError("source changed after capture")
    binary = digest(args.binary)
    if digest(f"/proc/{args.host_pid}/exe") != binary:
        raise ValueError("HTTP host executable differs; use a separately bound baseline")
    registry, _ = qa.load_registry()
    with qa.resource_locks(registry, ["cuda-device", "gb10-live-model", "benchmark-directory"]):
        before = read("host", "status", "--json")
        engine = next(e for e in read("engine", "list", "--json")["engines"] if e["alias"] == args.model)
        if any(before[k] for k in ("active_requests", "active_http_requests", "queue_depth")) or any(
                engine[k] for k in ("active_work", "attached_clients", "model_leases")):
            raise ValueError("operator activity present; no HTTP benchmark admission")
        if engine["execution_strategy"] not in selected["execution_strategies"]:
            raise ValueError("strategy not admitted by suite")
        profile = next(p for p in read("profile", "list", "--json")["profiles"] if p["identity"] == args.model)
        sampling_request, sampling_facts = http_sampling(args.sampling)
        configuration = dict(transport="http-openai", model=args.model, engine=engine, profile=profile,
                             instrumentation=measurement_instrumentation(getattr(args, "profiled", False)),
                             executable=binary, source=initial, source_capture=capture,
                             suite=canonical(suite), case=args.case,
                             host_version=read("version", "--json"),
                             source_scope="invoking worktree snapshot; executable version owns built source facts",
                             reasoning=args.reasoning, sampling=sampling_facts, sampling_selection=args.sampling, concurrency=1,
                             output_bound=selected["maximum_output"], session_state="ephemeral-per-request",
                             prefix_reuse="not assumed; native retained-session behavior is a different lane")
        (output / "identity.json").write_text(json.dumps(configuration, indent=2))
        for sample in range(args.repeats):
            messages = list(selected.get("messages", []))
            turns = [None] if messages else prompts(selected)
            for turn_index, prompt in enumerate(turns):
                if prompt is not None:
                    messages.append(dict(role="user", content=prompt))
                body = dict(model=args.model, messages=messages, max_tokens=selected["maximum_output"],
                            stream=True, stream_options={"include_usage": True},
                            reasoning_effort={"none":"none", "high":"high", "maximum":"max"}[args.reasoning],
                            yvex_engine_generation=engine["generation"])
                body.update(sampling_request)
                if "tools" in selected:
                    body["tools"] = selected["tools"]
                encoded = json.dumps(body, ensure_ascii=False, separators=(",", ":")).encode()
                sequence = max((e["sequence"] for e in events() if e.get("process") == args.host_pid), default=0)
                stem = f"http-{sample}-{turn_index}"
                (output / (stem + ".request.json")).write_bytes(encoded)
                connection = http.client.HTTPConnection("127.0.0.1", args.port, timeout=1800)
                started = time.monotonic()
                try:
                    with AcceleratorObservation(output / (stem + ".resources.jsonl"), [args.host_pid]) as resources:
                        started = time.monotonic()
                        connection.request("POST", "/v1/chat/completions", encoded, {"Content-Type":"application/json"})
                        with (output / (stem + ".sse.jsonl")).open("x") as log:
                            observation = read_http_stream(connection.getresponse(), started, log)
                finally:
                    connection.close()  # No retry even after a transport loss.
                deadline = time.monotonic() + 10
                while True:
                    current = read("host", "status", "--json")
                    after = next(e for e in read("engine", "list", "--json")["engines"] if e["alias"] == args.model)
                    if after["generation"] != engine["generation"]:
                        raise ValueError("HTTP engine generation changed")
                    if (not any(current[k] for k in ("active_requests", "active_http_requests", "queue_depth"))
                            and after["sessions"] == engine["sessions"] and not after["active_work"]):
                        break
                    if time.monotonic() >= deadline:
                        raise ValueError("HTTP cleanup not settled; reconcile, do not retry")
                    time.sleep(.05)
                if source() != initial or digest(f"/proc/{args.host_pid}/exe") != binary:
                    raise ValueError("moving HTTP source/executable")
                resource_facts = resources.summary()
                (output / (stem + ".resource-summary.json")).write_text(json.dumps(resource_facts, indent=2))
                resources.require_clear()
                observed = correlate_http_events(events(), observation["request_id"], args.host_pid, sequence)
                observation["server_events"] = observed
                for event in observed:
                    if event["kind"] == "generation.first_token":
                        observation["server_first_token_seconds"] = event["seconds"]
                    elif event["kind"] == "prefill.completed":
                        observation.update(prefill_seconds=event["seconds"], prefill_rate=event["rate"],
                                           newly_prefilled_tokens=event["a"])
                record = dict(schema="yvex.qualification.measurement.v1", lane="http-compatibility",
                              configuration=configuration, repetition=sample, turn_index=turn_index,
                              prompt_identity=canonical(messages), request_sha256=hashlib.sha256(encoded).hexdigest(),
                              source_stable=True, resources=resource_facts, metrics=observation)
                with (output / "observations.jsonl").open("a") as stream:
                    stream.write(json.dumps(record, sort_keys=True, allow_nan=False) + "\n")
                messages.append(dict(role="assistant", **observation["content"]))


def native(args):
    import qa
    suite = corpus(args.suite)
    selected = next(c for c in suite["cases"] if c["id"] == args.case)
    if args.reasoning not in selected["reasoning_modes"]:
        raise ValueError("reasoning mode not admitted by suite")
    if "native_disposition" in selected:
        raise ValueError(selected["native_disposition"])
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        raise ValueError("raw measurement must be outside the repository")
    output.mkdir(mode=0o700, parents=True, exist_ok=False)
    producer_root = Path(getattr(args, "producer_source", None) or ROOT).resolve()
    capture = retain_source(output / "source-capture", [args.binary, args.client], root=producer_root)
    initial = source(producer_root)
    if capture["source"] != {k:initial[k] for k in capture["source"]}:
        raise ValueError("source changed after capture")
    build = bound_product_build(args.binary, initial)
    adapter_capture = retain_source(output / "adapter-source-capture", [args.client])
    adapter = dict(source=source(), runner_sha256=digest(__file__), client_sha256=digest(args.client),
                   source_capture=adapter_capture,
                   scope="measurement adapter source; not the installed producer's build source")
    env = dict(os.environ, XDG_RUNTIME_DIR=str(args.runtime_dir), NO_COLOR="1")
    def read(*words):
        return json.loads(subprocess.check_output([str(args.binary), *words], env=env, cwd=ROOT))
    pid = args.host_pid
    executable = digest(f"/proc/{pid}/exe")
    if executable != digest(args.binary):
        raise ValueError("CLI and loaded executable differ")
    registry, _ = qa.load_registry()
    with qa.resource_locks(registry, ["cuda-device", "gb10-live-model", "benchmark-directory"]):
        status = read("host", "status", "--json")
        if any(status[k] for k in ("active_requests", "active_http_requests", "queue_depth")):
            raise ValueError("host busy; no benchmark admission")
        engines = read("engine", "list", "--json")["engines"]
        engine = next(e for e in engines if e["alias"] == args.model)
        if engine["execution_strategy"] not in selected["execution_strategies"]:
            raise ValueError("loaded strategy not admitted by suite")
        if engine["active_work"] or engine["attached_clients"] or engine["model_leases"]:
            raise ValueError("engine has operator ownership; no benchmark admission")
        profiles = read("profile", "list", "--json")["profiles"]
        profile = next(p for p in profiles if p["identity"] == args.model)
        authority = suite["applicability"]
        config = dict(executable=executable, model=engine["model_identity"],
                      artifact=engine["artifact_identity"], binding=engine["runtime_binding_identity"],
                      specialization=engine["specialization_identity"], strategy=engine["execution_strategy"],
                      context=engine["context_capacity"], prefill_chunk=engine["prefill_chunk_tokens"],
                      sampling=args.sampling, transport=f"native-v{status['protocol']}", concurrency=1, corpus=digest(args.suite),
                      case=selected["id"], reasoning=args.reasoning, output_bound=selected["maximum_output"],
                      warm_state="resident-engine; first-request-and-repeats-separated")
        instrumentation = measurement_instrumentation(getattr(args, "profiled", False))
        identity = dict(configuration=config, source=initial, build=build, adapter=adapter,
                        instrumentation=instrumentation,
                        source_capture=capture, engine=engine, profile=profile,
                        source_authority=authority, client=digest(args.client), host_pid=pid,
                        binding_file_sha256=digest(profile["runtime_binding"]),
                        lane=args.lane, workload_class=selected["class"],
                        product_override={"sampling": args.sampling, "output_bound": selected["maximum_output"]})
        (output / "identity.json").write_text(json.dumps(identity, indent=2))
        for sample in range(args.repeats):
            session = "bench-" + uuid.uuid4().hex[:16]
            paths = []
            for i, prompt in enumerate(prompts(selected)):
                path = output / f"prompt-{i}.txt"
                path.write_text(prompt)
                paths.append(str(path))
            command = [str(args.client), str(args.runtime_dir / "yvex/yvexd.sock"), args.model,
                       str(engine["generation"]), session, args.reasoning,
                       str(selected["maximum_output"]), args.sampling, *paths]
            # Retain partial evidence on refusal/loss; never automatically redispatch.
            with AcceleratorObservation(output / f"native-{sample}.resources.jsonl", [pid]) as resources:
                with (output / f"native-{sample}.jsonl").open("x") as out, (output / f"native-{sample}.stderr").open("x") as err:
                    process = subprocess.run(command, stdout=out, stderr=err, env=env, cwd=ROOT)
            resource_facts = resources.summary()
            (output / f"native-{sample}.resource-summary.json").write_text(json.dumps(resource_facts, indent=2))
            if process.returncode:
                raise RuntimeError(f"native failure: reconcile owned {session}; do not retry")
            rows = [json.loads(line) for line in (output / f"native-{sample}.jsonl").read_text().splitlines()]
            observations = summarize_native(rows)
            if len(observations) != len(paths):
                raise ValueError("missing turn evidence")
            if (source(producer_root) != initial or source() != adapter["source"]
                    or digest(f"/proc/{pid}/exe") != executable or digest(args.client) != adapter["client_sha256"]):
                raise ValueError("producer/adapter source or executable moved during sample")
            after = read("engine", "list", "--json")["engines"]
            current = next(e for e in after if e["alias"] == args.model)
            if current["generation"] != engine["generation"] or current["sessions"] != engine["sessions"] or current["active_work"]:
                raise ValueError("engine generation/cleanup mismatch")
            resources.require_clear()
            with (output / "observations.jsonl").open("a") as stream:
                for index, observation in enumerate(observations):
                    record = dict(schema="yvex.qualification.measurement.v1", lane=args.lane,
                                  configuration=dict(config, session_state="fresh" if index == 0 else "reused",
                                                     turn_index=index, prompt_sha256=digest(paths[index])),
                                  source=initial, source_stable=True, repetition=sample,
                                  workload_class=selected["class"], instrumentation=instrumentation,
                                  resources=resource_facts, metrics=observation)
                    stream.write(json.dumps(record, sort_keys=True, allow_nan=False) + "\n")

        closed = dict(schema="yvex.qualification.native-closed.v1", source_unchanged=source(producer_root) == initial,
                      instrumentation=instrumentation,
                      adapter_unchanged=source() == adapter["source"],
                      executable_unchanged=digest(f"/proc/{pid}/exe") == executable,
                      engine=current, host=read("host", "status", "--json"))
        (output / "closed.json").write_text(json.dumps(closed, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inspect = sub.add_parser("corpus")
    inspect.add_argument("--suite", type=Path, required=True)
    compare = sub.add_parser("reference-generation", help="controlled greedy continuation characterization, not product timing")
    for name in ("inputs", "reference", "binary", "artifact", "binding", "output"):
        compare.add_argument("--" + name, type=Path, required=True)
    compare.add_argument("--target", required=True)
    compare.add_argument("--context", type=int, required=True)
    compare.add_argument("--chunk", type=int, required=True)
    compare.add_argument("--strategy", choices=("target-only", "dspark"), required=True)
    compare.add_argument("--case", action="append", default=[])
    compare.add_argument("--reasoning", action="append", choices=("none", "high", "maximum"), required=True)
    publish = sub.add_parser("continuation-receipts", help="authenticate closed independent comparisons; no model execution")
    for name in ("run", "inputs", "reference", "suite", "output"):
        publish.add_argument("--" + name, type=Path, required=True)
    publish.add_argument("--id", required=True)
    publish.add_argument("--title", required=True)
    publish.add_argument("--origin", choices=("local", "yvex-published"), default="local")
    native_import = sub.add_parser("native-receipts", help="authenticate a closed native capture; no inference")
    for name in ("run", "suite", "relationship", "output"):
        native_import.add_argument("--" + name, type=Path, required=True)
    native_import.add_argument("--id", required=True)
    native_import.add_argument("--title", required=True)
    native_import.add_argument("--origin", choices=("local", "yvex-published"), default="local")
    native_import.add_argument("--skip-first", type=int, choices=(0, 1), default=0,
                               help="retain first-request raw evidence but separate it from repeated warm samples")
    for lane in ("native", "http"):
        run = sub.add_parser(lane)
        for name in ("binary", "runtime-dir", "output", "suite"):
            run.add_argument("--" + name, type=Path, required=True)
        run.add_argument("--host-pid", type=int, required=True)
        run.add_argument("--model", required=True)
        run.add_argument("--case", required=True)
        run.add_argument("--reasoning", required=True)
        run.add_argument("--repeats", type=int, default=3)
        run.add_argument("--sampling", choices=("product", "greedy"), default="product")
        run.add_argument("--profiled", action="store_true",
                         help="diagnostic capture only; prohibits native performance-receipt import")
        if lane == "native":
            run.add_argument("--client", type=Path, required=True)
            run.add_argument("--producer-source", type=Path,
                             help="exact installed producer checkout; never substitute the adapter source")
            run.add_argument("--lane", choices=("product-native", "controlled-engine"), required=True)
        else:
            run.add_argument("--port", type=int, required=True)
    args = parser.parse_args()
    if args.command == "corpus":
        print(json.dumps({"sha256": digest(args.suite), "corpus": corpus(args.suite)}, indent=2))
    elif args.command == "reference-generation":
        if not 1 <= args.chunk <= args.context <= 32768:
            parser.error("invalid controlled context/prefill geometry")
        reference_generation(args)
    elif args.command == "continuation-receipts":
        continuation_receipts(args)
    elif args.command == "native-receipts":
        native_receipts(args)
    else:
        if not 1 <= args.repeats <= 20:
            parser.error("repeats outside 1..20")
        if args.command == "http":
            if not 1 <= args.port <= 65535:
                parser.error("port outside 1..65535")
            http_lane(args)
        else:
            native(args)


if __name__ == "__main__":
    main()
