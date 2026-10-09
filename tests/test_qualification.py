#!/usr/bin/env python3
"""Fail-closed measurement contracts; no model or GPU is used by these fixtures."""
import copy
import hashlib
import io
import json
import importlib.util
from pathlib import Path
import unittest
import sys
import tempfile
import subprocess
import tarfile
from types import SimpleNamespace
from unittest.mock import Mock, patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import qualification as q
import qualification_reference as independent
import qualification_gguf as projection

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("measurement", ROOT / "tools/qualification_run.py")
measurement = importlib.util.module_from_spec(spec)
spec.loader.exec_module(measurement)


class NativeReceiptTests(unittest.TestCase):
    """Closed capture import is consistency evidence, never a numerical oracle."""
    def fixture(self, root):
        run = root / "run"
        run.mkdir()
        suite_path = ROOT / "tests/vectors/deepseek_competitive.json"
        suite = measurement.corpus(suite_path)
        case = next(c for c in suite["cases"] if c["id"] == "coding.hash-table")
        relationship = ROOT / "docs/evaluation/benchmarks/qualification/deepseek-published-native-coding-16.json"
        lineage = independent.read(relationship)["target"]
        snapshot = dict(head="fixture-head", tree="fixture-tree", state="clean",
                        delta_identity=hashlib.sha256(b"").hexdigest())
        def capture(name, binaries):
            directory = run / name
            directory.mkdir()
            for filename in ("base.tar", "delta.patch", "untracked.tar"):
                (directory / filename).write_bytes(b"fixture archive, never executed")
            for index, value in enumerate(binaries):
                (directory / f"executable-{index}").write_bytes(value)
            record = dict(schema="yvex.qualification.source-capture.v1", tree=snapshot["tree"],
                source={k:v for k,v in snapshot.items() if k != "tree"},
                files={p.name:measurement.digest(p) for p in directory.iterdir()})
            independent.write(directory / "manifest.json", record)
            return record
        producer = capture("source-capture", [b"producer fixture", b"measurement fixture"])
        adapter = capture("adapter-source-capture", [b"measurement fixture"])
        engine = dict(alias="fixture", generation=7, model_identity="fixture-model", sessions=0,
            artifact_identity=lineage["artifact_set"], runtime_binding_identity=lineage["binding"],
            specialization_identity="fixture-specialization", context_capacity=32768, prefill_chunk_tokens=64,
            execution_strategy="speculative", active_work=0, attached_clients=0, model_leases=0,
            capacity={"physical_sequence_width":1}, capacity_plan_identity="fixture-capacity")
        config = dict(executable=producer["files"]["executable-0"], model=engine["model_identity"],
            artifact=lineage["artifact_set"], binding=lineage["binding"], specialization=engine["specialization_identity"],
            context=32768, prefill_chunk=64, concurrency=1, sampling="greedy", transport="native-v25",
            corpus=measurement.digest(suite_path), case=case["id"], reasoning="none", strategy="speculative",
            output_bound=case["maximum_output"], warm_state="resident-engine; first-request-and-repeats-separated")
        instrumentation = measurement.measurement_instrumentation(False, {})
        identity = dict(configuration=config, source=snapshot, source_capture=producer, engine=engine,
            instrumentation=instrumentation,
            build=dict(schema="yvex.version.v1", build_commit=snapshot["head"], source_tree=snapshot["tree"],
                source_state="clean", build_identity="fixture-native-build", shell_build_identity="fixture-shell"),
            adapter=dict(source=snapshot, source_capture=adapter, client_sha256=adapter["files"]["executable-0"]),
            source_authority=suite["applicability"], client=adapter["files"]["executable-0"],
            host_pid=42, lane="product-native", profile={"backend":"cuda"})
        independent.write(run / "identity.json", identity)
        independent.write(run / "closed.json", dict(schema="yvex.qualification.native-closed.v1",
            instrumentation=instrumentation,
            source_unchanged=True, adapter_unchanged=True, executable_unchanged=True, engine=engine,
            host=dict(active_requests=0, active_http_requests=0, queue_depth=0)))
        prompt = run / "prompt-0.txt"
        prompt.write_text(measurement.prompts(case)[0])
        observations = []
        for repetition in range(3):
            events = [dict(kind="started", request=2, seconds=1),
                dict(kind="fragment", request=2, seconds=3, channel=1, hex="66697874757265"),
                dict(kind="turn", request=2, prompt_tokens=31, reused_tokens=0, prefill_tokens=31,
                    prefill_seconds=2, prefill_rate=15.5, client_complete_seconds=8 + repetition,
                    server_first_token_seconds=2, generated_tokens=33, post_first_decode_units=32,
                    post_first_decode_rate=8, reasoning_tokens=0, final_tokens=33, final_seconds=5,
                    first_reasoning_seconds=None, first_final_seconds=2)]
            (run / f"native-{repetition}.jsonl").write_text("".join(json.dumps(e) + "\n" for e in events))
            raw = dict(state="OBSERVED_CLEAR", started_monotonic=1, completed_monotonic=1.1,
                processes=[dict(device="GPU-fixture", pid=42)], foreign=[], error=None)
            path = run / f"native-{repetition}.resources.jsonl"
            path.write_text(json.dumps(raw) + "\n")
            resources = dict(schema="yvex.qualification.resource-observation.v1", adapter="cuda-process-list-v1",
                state="OBSERVED_CLEAR", allowed_pids=[42], devices=[], observations=1, evidence=str(path))
            independent.write(run / f"native-{repetition}.resource-summary.json", resources)
            observations.append(dict(schema="yvex.qualification.measurement.v1", repetition=repetition,
                instrumentation=instrumentation,
                source=snapshot, source_stable=True, lane="product-native", resources=resources,
                configuration=dict(config, session_state="fresh", turn_index=0, prompt_sha256=measurement.digest(prompt)),
                metrics=measurement.summarize_native(events)[0]))
        (run / "observations.jsonl").write_text("".join(json.dumps(row) + "\n" for row in observations))
        return SimpleNamespace(run=run, suite=suite_path, relationship=relationship,
            output=root / "receipt.json", id="fixture-native", title="Fixture native only", origin="local", skip_first=1)

    def test_native_import_keeps_producer_adapter_planes_and_first_run_separate(self):
        with tempfile.TemporaryDirectory() as directory:
            args = self.fixture(Path(directory))
            with patch("builtins.print"):
                measurement.native_receipts(args)
            receipt = q.validate(independent.read(args.output))
            self.assertEqual(receipt["target"]["product_path"], "product-native/native-v25")
            self.assertEqual(receipt["target"]["prefill_geometry"], "chunk=64")
            self.assertEqual(receipt["target"]["source_commit"], "fixture-head")
            self.assertIsNone(receipt["target"]["kernel_bundle"])
            self.assertEqual(receipt["provenance"]["skipped_initial_repetitions"], 1)
            for plane in q.PLANES:
                self.assertEqual(receipt["claims"][plane]["state"],
                    "CHARACTERIZED" if plane in ("deployment-performance", "product-path") else "UNQUALIFIED")
            metrics = {row["metric"]:row for row in receipt["measurements"]}
            self.assertEqual(metrics["request.client-complete"]["samples"], [9,10])
            self.assertNotIn("reasoning.phase-rate", metrics)
            self.assertNotIn("load.wall", metrics)
            with self.assertRaises(FileExistsError):
                measurement.native_receipts(args)

    def test_native_import_refuses_tampering_without_writing_a_receipt(self):
        changes = [
            ("observations.jsonl", lambda rows: rows[0]["metrics"].update(prefill_rate=999)),
            ("observations.jsonl", lambda rows: rows[0].update(repetition=True)),
            ("closed.json", lambda row: row["engine"].update(generation=8)),
            ("closed.json", lambda row: row["engine"].update(model_leases=1)),
            ("closed.json", lambda row: row["host"].update(active_http_requests=1)),
            ("identity.json", lambda row: row["configuration"].update(model="another-model")),
            ("identity.json", lambda row: row["configuration"].update(specialization="another-specialization")),
            ("identity.json", lambda row: row["build"].update(source_tree="another-tree")),
            ("identity.json", lambda row: row["source_authority"].update(revision="another-checkpoint")),
            ("identity.json", lambda row: row["instrumentation"].update(profiled=True)),
            ("identity.json", lambda row: row.pop("instrumentation")),
            ("closed.json", lambda row: row.pop("instrumentation")),
            ("observations.jsonl", lambda rows: rows[0]["instrumentation"].update(profiled=True)),
            ("native-0.resource-summary.json", lambda row: row.update(state="CONTENDED")),
            ("native-0.resources.jsonl", lambda rows: rows[0]["processes"].append(dict(device="GPU-fixture", pid=77))),
            ("native-0.resources.jsonl", lambda rows: rows[0].update(state="UNKNOWN", error="TimeoutExpired")),
        ]
        for filename, change in changes:
            with self.subTest(filename=filename), tempfile.TemporaryDirectory() as directory:
                args = self.fixture(Path(directory))
                path = args.run / filename
                value = measurement.evidence_rows(path) if filename.endswith("jsonl") else independent.read(path)
                change(value)
                path.write_text("".join(json.dumps(row) + "\n" for row in value)
                    if filename.endswith("jsonl") else json.dumps(value))
                with self.assertRaises(ValueError):
                    measurement.native_receipts(args)
                self.assertFalse(args.output.exists())

    def test_profiling_declaration_and_injection_markers_never_become_timing(self):
        self.assertFalse(measurement.measurement_instrumentation(False, {})["profiled"])
        self.assertTrue(measurement.measurement_instrumentation(True, {})["profiled"])
        for marker in ("CUDA_INJECTION64_PATH", "YVEX_DIAGNOSTIC_CUPTI_OUTPUT", "LD_PRELOAD"):
            with self.subTest(marker=marker):
                observation = measurement.measurement_instrumentation(False, {marker: "/private/local/path"})
                self.assertTrue(observation["profiled"])
                self.assertEqual(observation["observed_environment_markers"], [marker])
                self.assertNotIn("/private/local/path", json.dumps(observation))
        for value in (0, 1, None, "false"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                measurement.measurement_instrumentation(value, {})

    def test_source_capture_refuses_changed_bytes_and_symlink(self):
        for symlink in (False, True):
            with self.subTest(symlink=symlink), tempfile.TemporaryDirectory() as directory:
                args = self.fixture(Path(directory))
                path = args.run / "source-capture/executable-0"
                if symlink:
                    path.unlink()
                    path.symlink_to(args.run / "adapter-source-capture/executable-0")
                else:
                    path.write_bytes(b"different executable")
                with self.assertRaises(ValueError):
                    measurement.native_receipts(args)
                self.assertFalse(args.output.exists())

    def test_resource_import_refuses_hidden_filter_duplicate_pid_and_backwards_clock(self):
        for kind in ("filtered", "duplicate", "backwards", "boolean-count", "wrong-evidence"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                args = self.fixture(Path(directory))
                summary_path = args.run / "native-0.resource-summary.json"
                summary = independent.read(summary_path)
                raw_path = args.run / "native-0.resources.jsonl"
                rows = measurement.evidence_rows(raw_path)
                if kind == "filtered": summary["devices"] = ["GPU-selected"]
                elif kind == "boolean-count": summary["observations"] = True
                elif kind == "wrong-evidence": summary["evidence"] = "some-other-observation"
                elif kind == "duplicate": rows[0]["processes"] *= 2
                else: rows[0]["completed_monotonic"] = 0
                summary_path.write_text(json.dumps(summary))
                raw_path.write_text("".join(json.dumps(row) + "\n" for row in rows))
                with self.assertRaises(ValueError):
                    measurement.native_resource_interval(args.run, 0, 42)

    def test_evidence_reader_is_bounded_and_refuses_nonrecords(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "raw.jsonl"
            for text in ("", "[]\n", "true\n", "{}" + " " * (1024*1024) + "\n"):
                path.write_text(text)
                with self.assertRaises(ValueError):
                    measurement.evidence_rows(path)
            with path.open("wb") as stream:
                stream.truncate(16 * 1024 * 1024 + 1)
            with self.assertRaises(ValueError):
                measurement.evidence_rows(path)

    def test_history_groups_and_short_outputs_never_manufacture_sustained_or_reasoning_rates(self):
        config = dict(case="fixture-history", output_bound=256, warm_state="resident")
        metrics = dict(client_complete_seconds=3, prefill_tokens=5, reused_tokens=10, prefill_rate=5,
            post_first_decode_units=9, post_first_decode_rate=9, reasoning_tokens=0, final_tokens=10)
        rows = [dict(repetition=i, input_identity="history-" + str(i),
                     configuration={"turn_index":1}, metrics=metrics) for i in range(2)]
        observations, _ = measurement.native_measurements(config, rows, "fixture only", "fixture", 0)
        self.assertEqual(len(observations), 2)
        self.assertEqual({r["case"] for r in observations},
                         {"fixture-history/turn-1/input-history-0", "fixture-history/turn-1/input-history-1"})
        self.assertTrue(all(r["metric"] == "request.client-complete" and r["session_state"] == "reused"
                            and r["statistics"]["count"] == 1 for r in observations))

    def test_shared_metric_population_rules_refuse_missing_malformed_or_short_counts(self):
        for name in ("delivery.client-gap.maximum", "delivery.client-gap.mean"):
            for count, expected in ((0, False), (1, False), (2, True), (12, True)):
                self.assertEqual(q.metric_observation_admitted(name,
                    dict(client_visible_fragments=count)), expected)
            for count in (None, True, -1, 2.0, "2"):
                self.assertFalse(q.metric_observation_admitted(name,
                    dict(client_visible_fragments=count)))
            self.assertFalse(q.metric_observation_admitted(name, {}))
        for units, admitted in ((0, False), (9, False), (31, False), (32, True), (255, True)):
            self.assertEqual(q.metric_observation_admitted("decode.post-first.committed",
                dict(post_first_decode_units=units)), admitted)
        for units in (None, True, -1, 32.0, "32"):
            self.assertFalse(q.metric_observation_admitted("decode.post-first.committed",
                dict(post_first_decode_units=units)))
        self.assertFalse(q.metric_observation_admitted("decode.post-first.committed", {}))
        self.assertTrue(q.metric_observation_admitted("prefill.uncached",
            dict(prefill_tokens=6, reused_tokens=0)))
        for tokens, reused in ((6, 16), (0, 0), (6, True), (6, -1), (6, None)):
            self.assertFalse(q.metric_observation_admitted("prefill.uncached",
                dict(prefill_tokens=tokens, reused_tokens=reused)))
        self.assertFalse(q.metric_observation_admitted("unknown", {}))

    def test_native_delivery_gap_projection_preserves_absent_values(self):
        config = dict(case="fixture-delivery", output_bound=32, warm_state="fixture")
        for count in (None, 0, 1, 2, 4):
            metrics = dict(client_complete_seconds=3, client_visible_fragments=count,
                client_visible_gap_max_seconds=1.25, client_visible_gap_mean_seconds=0.5)
            rows = [dict(repetition=0, input_identity="fixture",
                configuration={"turn_index":0}, metrics=metrics)]
            observations, _ = measurement.native_measurements(config, rows, "fixture", "fixture", 0)
            gaps = {r["metric"]:r for r in observations if r["metric"].startswith("delivery.")}
            self.assertEqual(len(gaps), 2 if count in (2, 4) else 0)
            if gaps:
                self.assertEqual(gaps["delivery.client-gap.mean"]["samples"], [0.5])
                metrics["client_visible_gap_mean_seconds"] = None
                observations, _ = measurement.native_measurements(config, rows, "fixture", "fixture", 0)
                self.assertFalse(any(r["metric"] == "delivery.client-gap.mean" for r in observations))


class MeasurementTests(unittest.TestCase):
    def continuation_fixture(self, actual=(7, 8, 9), expected=(7, 8, 9), stop="max-new-tokens"):
        request = dict(input_identity="fixture-input", prompt_token_ids=[1, 2],
                       sampling={"temperature":0, "stochastic":False}, maximum_output=3)
        reference = dict(input_identity=request["input_identity"], output_token_ids=list(expected), finish_reason="length")
        candidate = dict(status="complete", generation_ready=True, execution_mode="target-only",
                         prompt_tokens=2, prompt_token_identity=q.token_ids_identity([1, 2]),
                         sampling_draws=0,
                         sampled_tokens=len(actual), model_committed_tokens=len(actual), stop_reason=stop,
                         generated_tokens=[dict(ordinal=i, token_id=t, model_committed=True, terminal=False)
                                           for i,t in enumerate(actual)])
        return request, reference, candidate

    def test_continuation_observation_is_not_cross_realization_conformance(self):
        request, reference, candidate = self.continuation_fixture()
        result = q.continuation_agreement(request, reference, candidate, "target-only")
        self.assertEqual(result["state"], "CHARACTERIZED")
        self.assertEqual(result["greedy_prefix_tokens"], 3)
        self.assertTrue(result["exact_bounded_continuation"])
        self.assertIsNone(result["first_divergence"])
        self.assertIn("PPL", result["unavailable_metrics"])
        self.assertNotIn("same_top_token_rate", result)

    def test_continuation_after_divergence_is_not_teacher_forced_accuracy(self):
        request, reference, candidate = self.continuation_fixture(actual=(7, 77, 9))
        result = q.continuation_agreement(request, reference, candidate, "target-only")
        self.assertTrue(result["first_token_agreement"])
        self.assertEqual(result["greedy_prefix_tokens"], 1)
        self.assertEqual(result["first_divergence"], 1)
        self.assertEqual(result["candidate_token_at_divergence"], 77)
        request, reference, candidate = self.continuation_fixture(actual=(7,), stop="eos")
        candidate["generated_tokens"][0].update(terminal=True, model_committed=False)
        candidate["model_committed_tokens"] = 0
        result = q.continuation_agreement(request, reference, candidate, "target-only")
        self.assertEqual(result["candidate_sampled_tokens"], 1)
        self.assertEqual(result["candidate_committed_tokens"], 0)
        self.assertIsNone(result["candidate_token_at_divergence"])

    def test_continuation_receipt_keeps_planes_and_missing_metrics_independent(self):
        request, reference, candidate = self.continuation_fixture(actual=(7, 77, 9))
        target = dict(schema=q.TARGET_SCHEMA, **{key:None for key in q.TARGET_FIELDS})
        target.update(reasoning="maximum", strategy="target-only", product_path="controlled-engine")
        row = dict(case="fixture", input_identity=request["input_identity"], prompt_tokens=2, maximum_output=3,
                   candidate_sha256="a" * 64, reference_capture_sha256="b" * 64, evidence="fixture-raw",
                   comparison=q.continuation_agreement(request, reference, candidate, "target-only"))
        receipt = q.continuation_receipt(target, [row], dict(source_stability="fixture", evidence_class="fixture"),
                                       "fixture-continuation", "Fixture only")
        self.assertEqual(receipt["origin"], "local")
        self.assertEqual(receipt["claims"]["checkpoint-reference"]["state"], "CHARACTERIZED")
        self.assertEqual(receipt["claims"]["representation-quality"]["state"], "BLOCKED")
        self.assertEqual(receipt["claims"]["deployment-performance"]["state"], "UNQUALIFIED")
        self.assertEqual([m["metric"] for m in receipt["measurements"]], ["greedy-prefix"])
        self.assertEqual(receipt["measurements"][0]["samples"], [1])
        self.assertIsNone(receipt["target"]["hardware_model"])
        for key,value in (("state", "QUALIFIED"), ("greedy_prefix_tokens", 4),
                          ("first_token_agreement", 0), ("candidate_committed_tokens", 4)):
            bad = copy.deepcopy(receipt)
            bad["provenance"]["continuation_comparisons"][0]["comparison"][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                q.validate(bad)
        bad = copy.deepcopy(receipt)
        bad["provenance"]["continuation_comparisons"].append(copy.deepcopy(row))
        with self.assertRaisesRegex(ValueError, "duplicate"):
            q.validate(bad)

    def test_continuation_refuses_missing_input_policy_or_publication(self):
        request, reference, candidate = self.continuation_fixture()
        for key, value in (("prompt_token_identity", None), ("prompt_tokens", 3), ("generation_ready", False),
                           ("status", "partial"), ("execution_mode", "dspark"), ("stop_reason", "cancelled"),
                           ("sampled_tokens", 2), ("model_committed_tokens", 2), ("sampling_draws", 1)):
            with self.subTest(key=key), self.assertRaises(ValueError):
                q.continuation_agreement(request, reference, dict(candidate, **{key:value}), "target-only")
        for field,value in (("ordinal", 1), ("token_id", True), ("model_committed", 1), ("terminal", None)):
            bad = copy.deepcopy(candidate)
            bad["generated_tokens"][0][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                q.continuation_agreement(request, reference, bad, "target-only")
        with self.assertRaises(ValueError):
            q.continuation_agreement(dict(request, sampling={"temperature":1}), reference, candidate, "target-only")
        with self.assertRaises(ValueError):
            q.continuation_agreement(request, dict(reference, input_identity="other"), candidate, "target-only")
        request, reference, candidate = self.continuation_fixture(actual=(7,), stop="max-new-tokens")
        with self.assertRaisesRegex(ValueError, "below declared output bound"):
            q.continuation_agreement(request, reference, candidate, "target-only")

    def test_token_identity_refuses_native_width_overflow_and_boolean(self):
        self.assertNotEqual(q.token_ids_identity([1, 2]), q.token_ids_identity([2, 1]))
        for tokens in ([], [True], [-1], [2**32], [1.0], None):
            with self.subTest(tokens=tokens), self.assertRaises(ValueError):
                q.token_ids_identity(tokens)

    def test_closed_comparison_projection_authenticates_raw_bytes_and_source(self):
        request, reference, candidate = self.continuation_fixture()
        request.update(case="fixture", reasoning="maximum", workload_class="representative",
                       prompt_token_count=2, rendered_prompt="fixture")
        candidate.update(family="fixture-family", generation_plan_identity="plan-fixture")
        suite_path = ROOT / "tests/vectors/deepseek_product.json"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run, oracle, capture_dir = root / "run", root / "oracle", root / "run/source-capture"
            capture_dir.mkdir(parents=True)
            oracle.mkdir()
            (capture_dir / "executable-0").write_bytes(b"fixture executable, never run")
            snapshot = dict(head="head-fixture", tree="tree-fixture", state="dirty", delta_identity="delta-fixture")
            capture = dict(schema="yvex.qualification.source-capture.v1",
                source={k:v for k,v in snapshot.items() if k != "tree"}, tree=snapshot["tree"],
                files={"executable-0":measurement.digest(capture_dir / "executable-0")})
            independent.write(capture_dir / "manifest.json", capture)
            inputs = dict(authority={"repository":"fixture-checkpoint", "revision":"fixture-revision"},
                          corpus_sha256=measurement.digest(suite_path), cases=[request])
            independent.write(root / "inputs.json", inputs)
            body = independent.request_body(request)
            response = dict(tokens=[7,8,9], content="fixture", stop_type="limit", tokens_evaluated=2, tokens_predicted=3)
            independent.write(oracle / "request.json", body)
            independent.write(oracle / "response.json", response)
            raw = independent.continuation(request, body, response)
            raw.update(raw_request_path="request.json", raw_response_path="response.json",
                raw_request_sha256=measurement.digest(oracle / "request.json"),
                raw_response_sha256=measurement.digest(oracle / "response.json"))
            independent.write(oracle / "capture.json", raw)
            reference.update(case="fixture", reasoning="maximum", raw_evidence_path="capture.json",
                             raw_evidence_sha256=measurement.digest(oracle / "capture.json"))
            evidence = dict(schema=independent.SCHEMA, source=inputs["authority"], cases=[reference],
                implementation=dict(independent_of_yvex=True, name="fixture-reference", revision="revision",
                    executable_sha256="a"*64, command=["fixture"], environment={"fixture":True}, physical_representation={"fixture":True}))
            independent.write(oracle / "reference.json", evidence)
            independent.write(oracle / "closed.json", dict(exit_code=0, gpu_processes_after=""))
            config = dict(strategy="target-only", backend="cuda", context=4096, prefill_chunk=64,
                          concurrency=1, sampling={"strategy":"greedy", "stochastic":False})
            identity = dict(source=snapshot, source_capture=capture,
                build=dict(build_commit=snapshot["head"], source_tree=snapshot["tree"], source_state="dirty",
                    source_delta_identity=snapshot["delta_identity"], build_identity="native", shell_build_identity="shell"),
                executable_sha256=capture["files"]["executable-0"], artifact_sha256="b"*64,
                binding_file_sha256="c"*64, inputs_sha256=measurement.digest(root / "inputs.json"),
                reference_sha256=measurement.digest(oracle / "reference.json"), configuration=config)
            independent.write(run / "identity.json", identity)
            independent.write(run / "closed.json", dict(source_stable=True, executable_unchanged=True, gpu_processes_after=[]))
            independent.write(run / "candidate-0.json", candidate)
            resources = dict(state="OBSERVED_CLEAR", owned_child_returncode=0, cancellation_requested=False)
            independent.write(run / "candidate-0.resources.jsonl.summary.json", resources)
            row = dict(case="fixture", reasoning="maximum", input_identity=request["input_identity"], configuration=config,
                       source_stable=True, candidate_sha256=measurement.digest(run / "candidate-0.json"),
                       reference_capture_sha256=reference["raw_evidence_sha256"],
                       comparison=q.continuation_agreement(request, reference, candidate, "target-only"))
            (run / "observations.jsonl").write_text(json.dumps(row) + "\n")
            args = SimpleNamespace(run=run, inputs=root / "inputs.json", reference=oracle / "reference.json",
                suite=suite_path, output=root / "receipts", id="fixture-closed", title="Fixture only", origin="local")
            with patch("builtins.print"):
                measurement.continuation_receipts(args)
            receipt = q.validate(independent.read(args.output / "fixture-closed-maximum.json"))
            self.assertEqual(receipt["target"]["source_delta"], "delta-fixture")
            self.assertEqual(receipt["measurements"][0]["samples"], [3])
            for name, change in (("resources", lambda: (run / "candidate-0.resources.jsonl.summary.json").write_text(
                    json.dumps(dict(resources, state="CONTENDED")))),
                ("candidate", lambda: (run / "candidate-0.json").write_text(json.dumps(dict(candidate, sampled_tokens=2)))),
                ("source", lambda: (capture_dir / "executable-0").write_bytes(b"different executable"))):
                with self.subTest(name=name):
                    change()
                    args.output = root / ("refused-" + name)
                    with self.assertRaises(ValueError):
                        measurement.continuation_receipts(args)
                    self.assertFalse(args.output.exists())
                    (run / "candidate-0.resources.jsonl.summary.json").write_text(json.dumps(resources))
                    (run / "candidate-0.json").write_text(json.dumps(candidate, indent=2, ensure_ascii=False))

    def test_controlled_generation_can_admit_only_its_exact_spawned_child(self):
        child = Mock(pid=1234)
        child.wait.return_value = 0
        child.poll.return_value = 0
        observer = Mock(allowed_pids={42})
        observer.__enter__ = Mock(return_value=observer)
        observer.__exit__ = Mock(return_value=False)
        observer.summary.return_value = dict(state="OBSERVED_CLEAR")
        with tempfile.TemporaryDirectory() as directory, patch.object(measurement.subprocess, "Popen", return_value=child):
            result = measurement.watch_owned_command(["generation-fixture"], Path(directory) / "resources.jsonl", [42],
                observer_factory=Mock(return_value=observer), include_child=True)
        self.assertEqual(result, 0)
        self.assertEqual(observer.allowed_pids, {42, 1234})
        child.send_signal.assert_not_called()

    def test_reference_command_preserves_native_greedy_parameter_contract(self):
        args = SimpleNamespace(binary="product", target="target", artifact="artifact", binding="binding",
                               context=4096, chunk=64, strategy="dspark")
        command = measurement.generation_command(args, dict(rendered_prompt="immutable prefix", maximum_output=256))
        self.assertEqual(command[command.index("--temperature") + 1], "1")
        self.assertEqual(command[command.index("--strategy") + 1], "greedy")
        self.assertEqual(command[command.index("--text") + 1], "immutable prefix")
        self.assertNotIn("--seed", command)

    def test_product_build_must_bind_exact_source_not_just_binary_digest(self):
        snapshot = dict(head="head", tree="tree", state="dirty", delta_identity="delta")
        version = dict(schema="yvex.version.v1", build_commit="head", source_tree="tree", source_state="dirty",
                       source_delta_identity="delta", build_identity="native", shell_build_identity="rust")
        with patch.object(measurement.subprocess, "check_output", return_value=json.dumps(version)):
            self.assertEqual(measurement.bound_product_build(Path("product"), snapshot), version)
        for key in ("build_commit", "source_tree", "source_state", "source_delta_identity", "build_identity", "shell_build_identity"):
            bad = dict(version, **{key:None})
            with patch.object(measurement.subprocess, "check_output", return_value=json.dumps(bad)), \
                    self.subTest(key=key), self.assertRaisesRegex(ValueError, "rebuild first"):
                measurement.bound_product_build(Path("product"), snapshot)

    def test_clean_product_build_may_omit_only_the_empty_source_delta(self):
        snapshot = dict(head="head", tree="tree", state="clean",
                        delta_identity=hashlib.sha256(b"").hexdigest())
        version = dict(schema="yvex.version.v1", build_commit="head", source_tree="tree",
                       source_state="clean", build_identity="native", shell_build_identity="rust")
        for delta in (None, "", snapshot["delta_identity"]):
            value = dict(version) if delta is None else dict(version, source_delta_identity=delta)
            with patch.object(measurement.subprocess, "check_output", return_value=json.dumps(value)):
                self.assertEqual(measurement.bound_product_build(Path("product"), snapshot), value)
        for bad in (dict(version, source_delta_identity="foreign-delta"),
                    dict(version, build_commit="other-producer"),
                    dict(version, source_tree="other-tree"),
                    dict(version, source_state="dirty")):
            with patch.object(measurement.subprocess, "check_output", return_value=json.dumps(bad)), \
                    self.subTest(version=bad), self.assertRaisesRegex(ValueError, "rebuild first"):
                measurement.bound_product_build(Path("product"), snapshot)
        for bad in (dict(snapshot, state="dirty"), dict(snapshot, delta_identity="unknown")):
            with patch.object(measurement.subprocess, "check_output", return_value=json.dumps(version)), \
                    self.subTest(snapshot=bad), self.assertRaisesRegex(ValueError, "rebuild first"):
                measurement.bound_product_build(Path("product"), bad)

    def test_accelerator_observation_retains_transient_contention(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Mock(side_effect=[[dict(device="GPU-fixture", pid=42)],
                                      [dict(device="GPU-fixture", pid=42), dict(device="GPU-fixture", pid=77)],
                                      [dict(device="GPU-fixture", pid=42)]])
            path = Path(directory) / "resource.jsonl"
            with measurement.AcceleratorObservation(path, [42], interval=10, probe=probe) as observer:
                observer.sample()
            self.assertEqual(observer.summary()["state"], "CONTENDED")
            self.assertEqual(observer.summary()["observations"], 3)
            with self.assertRaisesRegex(ValueError, "no performance admission"):
                observer.require_clear()
            self.assertEqual([r["state"] for r in map(json.loads, path.read_text().splitlines())],
                             ["OBSERVED_CLEAR", "CONTENDED", "OBSERVED_CLEAR"])

    def test_accelerator_observation_unknown_never_becomes_zero_contention(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Mock(side_effect=[[], subprocess.TimeoutExpired("fixture", 5), []])
            path = Path(directory) / "resource.jsonl"
            with measurement.AcceleratorObservation(path, [42], interval=10, probe=probe) as observer:
                observer.sample()
            self.assertEqual(observer.summary()["state"], "UNKNOWN")
            with self.assertRaises(ValueError): observer.require_clear()
            unknown = json.loads(path.read_text().splitlines()[1])
            self.assertIsNone(unknown["processes"])
            self.assertIsNone(unknown["foreign"])

    def test_accelerator_observation_refuses_busy_entry_without_signalling(self):
        with tempfile.TemporaryDirectory() as directory, patch("os.kill") as signal:
            path = Path(directory) / "resource.jsonl"
            probe = Mock(return_value=[dict(device="GPU-fixture", pid=77)])
            with self.assertRaisesRegex(ValueError, "contended"):
                with measurement.AcceleratorObservation(path, [42], probe=probe):
                    self.fail("contended measurement must not be admitted")
            signal.assert_not_called()
            self.assertEqual(len(path.read_text().splitlines()), 1)
            with self.assertRaises(FileExistsError):
                with measurement.AcceleratorObservation(path, [42], probe=probe): pass

    def test_accelerator_observation_scope_is_explicit_not_exclusive_guarantee(self):
        with tempfile.TemporaryDirectory() as directory:
            probe = Mock(return_value=[dict(device="GPU-other", pid=77)])
            with measurement.AcceleratorObservation(Path(directory) / "resources.jsonl", [42],
                    devices=["GPU-fixture"], interval=10, probe=probe) as observer:
                pass
            observer.require_clear()
            self.assertEqual(observer.summary()["state"], "OBSERVED_CLEAR")
            self.assertIn("not exclusive reservation", observer.summary()["scope"])
        for args in (([], (), 1), ([0], (), 1), ([True], (), 1), ([42], (), 0), ([42], ("",), 1)):
            with self.assertRaises(ValueError):
                measurement.AcceleratorObservation(Path("unused"), *args)

    def test_cuda_process_csv_is_typed_and_bounded(self):
        with patch.object(measurement.subprocess, "run") as run:
            run.return_value.stdout = "GPU-fixture, 42\nMIG-fixture, 77\n"
            self.assertEqual(measurement.cuda_processes(),
                             [dict(device="GPU-fixture", pid=42), dict(device="MIG-fixture", pid=77)])
            for bad in ("GPU-fixture, N/A\n", "GPU-fixture, -1\n", "human output\n",
                        "GPU-fixture, 42\nGPU-fixture, 42\n", "x" * 65537):
                run.return_value.stdout = bad
                with self.assertRaises(ValueError): measurement.cuda_processes()
            run.return_value.stdout = ""
            self.assertEqual(measurement.cuda_processes(), [])

    def test_interference_cancels_only_owned_qualified_child_and_keeps_evidence(self):
        child = Mock(pid=1234)
        child.wait.side_effect = [subprocess.TimeoutExpired("fixture", .1), 0]
        child.poll.return_value = 0
        observer = Mock()
        observer.__enter__ = Mock(return_value=observer)
        observer.__exit__ = Mock(return_value=False)
        observer.summary.return_value = dict(state="CONTENDED", evidence="fixture")
        observer.require_clear.side_effect = ValueError("no performance admission")
        with tempfile.TemporaryDirectory() as directory, patch.object(measurement.subprocess, "Popen", return_value=child):
            path = Path(directory) / "resources.jsonl"
            with self.assertRaisesRegex(ValueError, "no performance admission"):
                measurement.watch_owned_command(["qualified-child-fixture"], path, [42],
                                                observer_factory=Mock(return_value=observer))
            child.send_signal.assert_called_once_with(measurement.signal.SIGINT)
            child.kill.assert_not_called()
            summary = json.loads(Path(str(path) + ".summary.json").read_text())
            self.assertEqual(summary["owned_child_pid"], 1234)
            self.assertTrue(summary["cancellation_requested"])
            self.assertEqual(summary["owned_child_returncode"], 0)

    def test_clear_resource_wrapper_does_not_hide_child_failure_or_redispatch(self):
        child = Mock(pid=1234)
        child.wait.return_value = 1
        child.poll.return_value = 1
        observer = Mock()
        observer.__enter__ = Mock(return_value=observer)
        observer.__exit__ = Mock(return_value=False)
        observer.summary.return_value = dict(state="OBSERVED_CLEAR")
        with tempfile.TemporaryDirectory() as directory, patch.object(measurement.subprocess, "Popen", return_value=child) as start:
            result = measurement.watch_owned_command(["qualified-child-fixture"], Path(directory) / "resources.jsonl", [42],
                                                    observer_factory=Mock(return_value=observer))
            self.assertEqual(result, 1)
            start.assert_called_once()
            child.send_signal.assert_not_called()

    def test_reference_history_publication_cannot_promote_absent_modes(self):
        value = json.loads((ROOT / "docs/evaluation/benchmarks/references/deepseek-independent-history.json").read_text())
        q.validate_reference_observation(value)
        self.assertEqual([r["state"] for r in value["continuation_dispositions"]],
                         ["CHARACTERIZED", "UNQUALIFIED", "UNQUALIFIED"])
        for state, parent, lineage in (("CHARACTERIZED", "absent", True),
                                      ("CHARACTERIZED", "conversation.coding", False)):
            bad = copy.deepcopy(value)
            bad["cases"][0]["parent_case"] = parent
            if not lineage:
                del bad["cases"][0]["prior_input_identity"]
            with self.assertRaises(ValueError):
                q.validate_reference_observation(bad)
        for index in (1, 2):
            bad = copy.deepcopy(value)
            bad["continuation_dispositions"][index]["state"] = "CHARACTERIZED"
            with self.assertRaisesRegex(ValueError, "continuation capture missing"):
                q.validate_reference_observation(bad)

    def test_reference_multiturn_uses_only_completed_independent_history(self):
        spec = importlib.util.spec_from_file_location("continuation_fixture", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        modes = ("none", "high", "maximum")
        base = dict(id="conversation.fixture", turns=["hello", "write code"], reasoning_modes=modes)
        suite = measurement.corpus(reference.CORPUS)
        inputs = dict(authority=reference.source_authority(suite),
                      corpus_sha256=measurement.digest(reference.CORPUS),
                      cases=[dict(case=base["id"], reasoning=mode,
                      conversation_mode="chat" if mode == "none" else "thinking",
                      messages=[{"role":"user", "content":"hello"}], input_identity="input-" + mode)
                      for mode in modes])
        evidence = dict(cases=[dict(case=base["id"], reasoning=mode, output_token_ids=[7,8],
                        finish_reason="eos", raw_evidence_sha256="capture-" + mode) for mode in modes])
        tokenizer = Mock()
        tokenizer.decode.return_value = "independently generated greeting"
        assistant = dict(role="assistant", content="actual greeting", reasoning_content="", tool_calls=[])
        encoding = SimpleNamespace(parse_message_from_completion_text=Mock(return_value=assistant))
        def prepared(case, *args):
            return dict(case=case["id"], messages=case["messages"], reasoning=case["reasoning"],
                        input_identity="replace", inference_reference_status="MISSING", reference_output=None)
        def grammar(tokens, finish, mode, *args):
            return {"state":{"none":"PASS", "high":"FAIL", "maximum":"NOT_MEASURED_TRUNCATED"}[mode]}
        with patch.dict(sys.modules, {"tokenizers":SimpleNamespace(Tokenizer=SimpleNamespace(from_file=lambda p: tokenizer))}), \
                patch.object(reference, "validate_reference") as authenticate, \
                patch.object(reference.measurement, "corpus", return_value=dict(suite, cases=[base])), \
                patch.object(reference, "load_encoding", return_value=encoding), \
                patch.object(reference, "prepare_case", side_effect=prepared), \
                patch.object(reference, "output_grammar", side_effect=grammar):
            result = reference.prepare_continuations(inputs, evidence, Path("/fixture/captures"), Path("/fixture/source"))
            authenticate.assert_called_once_with(inputs, evidence, Path("/fixture/captures"))
            row = result["cases"][0]
            self.assertEqual(len(result["cases"]), 1)
            self.assertEqual(row["messages"], [inputs["cases"][0]["messages"][0], assistant,
                             {"role":"user", "content":"write code"}])
            self.assertEqual(row["prior_capture_sha256"], "capture-none")
            self.assertEqual([r["state"] for r in result["continuation_dispositions"]],
                             ["PREPARED", "UNQUALIFIED", "UNQUALIFIED"])
            encoding.parse_message_from_completion_text.assert_called_once()
            with patch.object(reference, "output_grammar", return_value={"state":"FAIL"}):
                with self.assertRaisesRegex(ValueError, "no source-parseable"):
                    reference.prepare_continuations(inputs, evidence, Path("/fixture/captures"), Path("/fixture/source"))

    def test_reference_offsets_do_not_inherit_reader_scalar_overflow(self):
        class ReaderScalar:
            def __init__(self, value):
                self.value = value

            def __index__(self):
                return self.value

            def __add__(self, other):
                raise AssertionError("reader scalar arithmetic must not own file offsets")

        for value in (0, 31, 32, 2**32 - 1, 2**32 + 31, 95_050_210_272):
            result = projection.aligned(ReaderScalar(value), ReaderScalar(32))
            self.assertEqual(result, ((value + 31) // 32) * 32)
            self.assertIs(type(result), int)
        for value, alignment in ((-1, 32), (1, 0), (1, 3), (1, -32)):
            with self.assertRaises(ValueError):
                projection.aligned(value, alignment)
        with self.assertRaises(TypeError):
            projection.aligned(1.5, 32)

    def test_reference_bf16_embedding_exhausts_all_bit_patterns(self):
        import struct
        source = b"".join(struct.pack("<H", n) for n in range(65536))
        oracle = b"".join(struct.pack("<I", n << 16) for n in range(65536))
        self.assertEqual(projection.widen(source), oracle)
        with self.assertRaisesRegex(ValueError,"partial BF16"):
            projection.widen(b"x")

    def test_reference_projection_recipe_is_immutable_not_an_admitted_variant(self):
        recipe = json.loads((ROOT / "tests/vectors/deepseek_reference.json").read_text())
        self.assertEqual(projection.projection_recipe(recipe), recipe)
        self.assertIn("not the higher-precision", " ".join(recipe["limitations"]))
        for key, value in (("source_sha256","bad"),("reader_revision","floating-main"),
                           ("widen_bf16",1),("exclude_prefixes",[""]),
                           ("metadata_aliases",{"a":"same","b":"same"}),
                           ("expected_target_tensors",-1),("limitations",[])):
            with self.assertRaises(ValueError): projection.projection_recipe(dict(recipe,**{key:value}))

    def test_independent_adapter_uses_prepared_tokens_not_authored_prompts(self):
        row = dict(case="fixture", reasoning="maximum", prompt_token_ids=[1,2], prompt_token_count=2,
                   maximum_output=3, rendered_prompt="source-authored maximum", input_identity="fixture",
                   sampling={"temperature":0,"stochastic":False})
        self.assertEqual(independent.input_cases({"cases":[row]}), [row])
        self.assertEqual(independent.request_body(row)["prompt"], [1,2])
        for bad in ({"cases":[]}, {"cases":[row,row]},
                    {"cases":[dict(row, sampling={"temperature":1})]},
                    {"cases":[dict(row, prompt_token_ids=[True,2])]},
                    {"cases":[dict(row, prompt_token_count=3)]}):
            with self.assertRaises(ValueError): independent.input_cases(bad)
        with self.assertRaisesRegex(ValueError, "absent"):
            independent.input_cases({"cases":[row]}, ["other"])

    def test_independent_capture_binds_raw_response_and_execution_policy(self):
        row = dict(prompt_token_ids=[1,2], maximum_output=3, input_identity="fixture",
                   sampling={"temperature":0,"stochastic":False})
        body = independent.request_body(row)
        response = dict(tokens=[3,4,5], content="fixture", stop_type="limit", tokens_evaluated=2,
                        tokens_predicted=3)
        capture = independent.continuation(row, body, response)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            independent.write(root / "request.json", body)
            independent.write(root / "response.json", response)
            capture.update(raw_request_path="request.json", raw_response_path="response.json",
                           raw_request_sha256=measurement.digest(root / "request.json"),
                           raw_response_sha256=measurement.digest(root / "response.json"))
            self.assertEqual(independent.validate_capture(row,capture,root)["output_token_ids"], [3,4,5])
            for bad in (dict(capture,text="forged"), dict(capture,output_token_ids=[3]),
                        dict(capture,raw_response_path="../response.json"),
                        dict(capture,raw_response_sha256="a"*64)):
                with self.assertRaises(ValueError): independent.validate_capture(row,bad,root)
        for bad in (dict(body,cache_prompt=True),dict(body,n_predict=1),dict(body,temperature=1)):
            with self.assertRaisesRegex(ValueError,"request policy"):
                independent.continuation(row,bad,response)
        for bad in (dict(response,tokens=[3]),dict(response,tokens_predicted=1),
                    dict(response,tokens_evaluated=1),dict(response,stop_type="cancelled"),
                    dict(response,tokens=[True,4,5])):
            with self.assertRaises(ValueError): independent.continuation(row,body,bad)

    def test_fragment_identity_does_not_compare_execution_lineage(self):
        rows = [{"kind":"dispatch"},
                {"kind":"fragment", "channel":2, "bytes":3, "sha256":"a" * 64},
                {"kind":"turn", "token_identity":"execution-A"}]
        other = copy.deepcopy(rows)
        other[-1]["token_identity"] = "execution-B"
        self.assertEqual(measurement.fragment_manifests(rows), measurement.fragment_manifests(other))
        other[1]["channel"] = 1
        self.assertNotEqual(measurement.fragment_manifests(rows), measurement.fragment_manifests(other))
        for bad in (rows[:-1], rows[1:], [], [*rows[:-1], {"kind":"refused"}]):
            with self.assertRaises(ValueError): measurement.fragment_manifests(bad)
        for field, value in (("sha256", "bad"), ("bytes", -1), ("channel", True)):
            bad = copy.deepcopy(rows)
            bad[1][field] = value
            with self.assertRaisesRegex(ValueError, "malformed"):
                measurement.fragment_manifests(bad)

    def test_source_capture_retains_dirty_source_without_ignored_assets(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            root = base / "repo"
            root.mkdir()
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=root, stderr=subprocess.DEVNULL)
            git("init")
            (root / ".gitignore").write_text("weights.bin\n")
            (root / "source.c").write_text("original\n")
            git("add", ".")
            git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                "commit", "-m", "fixture")
            (root / "source.c").write_text("changed\n")
            (root / "new.rs").write_text("new source\n")
            (root / "weights.bin").write_bytes(b"ignored payload")
            binary = base / "binary"
            binary.write_bytes(b"fixture executable")
            output = base / "capture"
            record = measurement.retain_source(output, [binary], root)
            self.assertEqual(record["source"]["state"], "dirty")
            self.assertIn(b"+changed", (output / "delta.patch").read_bytes())
            with tarfile.open(output / "base.tar") as archive:
                self.assertEqual(archive.extractfile("source.c").read(), b"original\n")
                self.assertNotIn("weights.bin", archive.getnames())
            with tarfile.open(output / "untracked.tar") as archive:
                self.assertEqual(archive.getnames(), ["new.rs"])
                self.assertEqual(archive.extractfile("new.rs").read(), b"new source\n")
            self.assertEqual((output / "executable-0").read_bytes(), binary.read_bytes())
            for name, expected in record["files"].items():
                self.assertEqual(measurement.digest(output / name), expected)
            with self.assertRaises(FileExistsError):
                measurement.retain_source(output, [binary], root)
            with self.assertRaisesRegex(ValueError, "outside"):
                measurement.retain_source(root / "capture", [binary], root)
            (root / "foreign").symlink_to(binary)
            with self.assertRaisesRegex(ValueError, "bounded regular"):
                measurement.retain_source(base / "bad-capture", [binary], root)

    def test_http_sampling_resolves_adapter_not_native_defaults(self):
        request, facts = measurement.http_sampling("product")
        self.assertEqual(request, {"temperature":1, "top_p":1})
        self.assertTrue(facts["stochastic"])
        self.assertIsNone(facts["seed"])
        self.assertEqual(facts["seed_origin"], "server-generated-not-projected")
        request, facts = measurement.http_sampling("greedy")
        self.assertEqual(request, {"temperature":0, "top_p":1})
        self.assertEqual((facts["top_k"], facts["min_p"], facts["typical_p"]), (0, 0, 1))
        self.assertFalse(facts["stochastic"])
        with self.assertRaises(ValueError): measurement.http_sampling("native-product")

    def http(self, lines):
        response = io.BytesIO(b"".join(lines))
        response.status = 200
        ticks = iter(range(1, 100))
        return measurement.read_http_stream(response, 0, io.StringIO(), lambda: next(ticks))

    def test_http_comments_not_content_or_compute_timing(self):
        value = self.http([
            b": yvex execution progress\n",
            b'data: {"id":"r1","choices":[{"delta":{"content":"hello"}}]}\n',
            b'data: {"id":"r1","choices":[{"finish_reason":"length"}],"usage":{"completion_tokens":7},"yvex_completion_metrics":{"first_token_seconds":0.1}}\n',
            b"data: [DONE]\n"])
        self.assertEqual(value["client_first_visible_seconds"], 2)
        self.assertEqual(value["usage"]["completion_tokens"], 7)
        self.assertIsNone(value["server_first_token_seconds"])
        self.assertIsNone(value["first_fragment_publication_seconds"])

    def test_http_truncated_or_incomplete_terminal_refuses(self):
        with self.assertRaisesRegex(ValueError, "indeterminate"):
            self.http([b'data: {"id":"r1","choices":[]}\n'])
        with self.assertRaisesRegex(ValueError, "incomplete"):
            self.http([b"data: [DONE]\n"])

    def test_http_error_and_extent_refuse(self):
        with self.assertRaisesRegex(ValueError, "runtime_unavailable"):
            self.http([b'data: {"error":{"code":"runtime_unavailable"}}\n'])
        with self.assertRaisesRegex(ValueError, "bounded stream"):
            self.http([b"x" * (1024 * 1024 + 1)])

    def test_http_correlation_requires_exact_unique_producer(self):
        event = dict(external_correlation_id="r1", process=42, sequence=8,
                     session="owned", request=7, kind="generation.completed")
        self.assertEqual(measurement.correlate_http_events([event], "r1", 42, 7), [event])
        for identity, pid, sequence in [("other",42,7), ("r1",43,7), ("r1",42,8)]:
            with self.assertRaisesRegex(ValueError, "unavailable/ambiguous"):
                measurement.correlate_http_events([event], identity, pid, sequence)
        with self.assertRaisesRegex(ValueError, "unavailable/ambiguous"):
            measurement.correlate_http_events([event, dict(event, session="another")], "r1",42,7)

    def test_reuse_denominator(self):
        rows = [{"kind":"turn", "prompt_tokens":26,"reused_tokens":16,"prefill_tokens":10}]
        self.assertEqual(measurement.summarize_native(rows)[0]["prefill_tokens"], 10)
        rows[0]["prefill_tokens"] = 26
        with self.assertRaisesRegex(ValueError, "newly executed"):
            measurement.summarize_native(rows)

    def test_fragments_not_tokens_or_server_ttft(self):
        rows = [{"kind":"fragment","seconds":1.1,"channel":5,"hex":"6e"},
                {"kind":"fragment","seconds":2.8,"channel":1,"hex":"68656c6c6f"},
                {"kind":"turn","prompt_tokens":6,"reused_tokens":0,"prefill_tokens":6,
                 "server_first_token_seconds":1.37,"generated_tokens":10}]
        value = measurement.summarize_native(rows)[0]
        self.assertEqual(value["client_first_visible_seconds"], 2.8)
        self.assertEqual(value["generated_tokens"], 10)
        self.assertEqual(value["server_first_token_seconds"], 1.37)
        self.assertIsNone(value["first_fragment_publication_seconds"])
        self.assertIsNone(value["client_admitted_seconds"])

    def test_native_started_is_distinct_from_server_and_visible_ttft(self):
        rows = [{"kind":"started", "seconds":1.2, "request":2},
                {"kind":"fragment", "seconds":2.8, "request":2, "channel":1, "hex":"6869"},
                {"kind":"turn", "request":2, "client_complete_seconds":4,
                 "prompt_tokens":6, "reused_tokens":0, "prefill_tokens":6,
                 "server_first_token_seconds":1.37, "generated_tokens":10}]
        value = measurement.summarize_native(rows)[0]
        self.assertEqual(value["client_admitted_seconds"], 1.2)
        self.assertEqual(value["server_first_token_seconds"], 1.37)
        self.assertEqual(value["client_first_visible_seconds"], 2.8)
        # Reused-turn observations must not inherit the previous acknowledgement.
        following = dict(rows[-1], request=3)
        self.assertIsNone(measurement.summarize_native(rows + [following])[1]["client_admitted_seconds"])
        for index, field, bad in [(0,"seconds",-1), (0,"seconds",float("nan")),
                                  (0,"request",True), (1,"request",3), (1,"seconds",1),
                                  (1,"seconds",float("nan")), (1,"seconds",None),
                                  (2,"request",3), (2,"client_complete_seconds",1),
                                  (2,"client_complete_seconds",float("inf"))]:
            changed = copy.deepcopy(rows)
            changed[index][field] = bad
            with self.assertRaises(ValueError):
                measurement.summarize_native(changed)
        for changed in ([rows[0], rows[0]], rows[:1], rows[:2], [rows[1], rows[0]]):
            with self.assertRaises(ValueError):
                measurement.summarize_native(changed)

    def test_unterminated_delivery(self):
        with self.assertRaisesRegex(ValueError, "indeterminate"):
            measurement.summarize_native([{"kind":"fragment","seconds":1,"channel":1,"hex":"61"}])

    def test_corpus_scope(self):
        c = measurement.corpus(ROOT / "tests/vectors/deepseek_product.json"); rows = {r["id"]:r for r in c["cases"]}
        self.assertEqual(rows["stress.count"]["class"], "synthetic")
        self.assertEqual(rows["coding.metal"]["class"], "representative")
        self.assertEqual(rows["reasoning.schedule"]["reasoning_modes"], ["none", "high", "maximum"])
        cells = list(measurement.configurations(c))
        self.assertEqual(len(cells), 6 * len(rows))
        self.assertEqual(len({(r["case"], r["reasoning"], r["strategy"]) for r in cells}), len(cells))
        self.assertIn("NOT official", c["provenance"])
        self.assertEqual(len(measurement.prompts(rows["conversation.coding"])), 3)

    def test_external_competitive_control_is_separate(self):
        suite = measurement.corpus(ROOT / "tests/vectors/deepseek_competitive.json")
        case, = suite["cases"]
        self.assertEqual(suite["id"], "deepseek-competitive")
        self.assertIn("9139e2ae58a41503968a500f36f75895c1ba63fc", suite["provenance"])
        self.assertIn("NOT official", suite["provenance"])
        self.assertEqual(case["id"], "coding.hash-table")
        self.assertEqual(measurement.prompts(case), [
            "Write a complete C hash table implementation with string keys, insert, find, delete, and a test main. Output only C code."
        ])
        self.assertEqual(case["maximum_output"], 256)
        self.assertEqual(len(list(measurement.configurations(suite))), 6)
        discovered = measurement.corpora(ROOT / "tests/vectors")
        self.assertIn(suite, discovered)

    def test_long_turn_suite_keeps_prompts_and_separates_output_bound(self):
        base = measurement.corpus(ROOT / "tests/vectors/deepseek_product.json")
        suite = measurement.corpus(ROOT / "tests/vectors/deepseek_turn_lifecycle.json")
        original = {case["id"]: case for case in base["cases"]}
        self.assertEqual(suite["id"], "deepseek-turn-lifecycle")
        self.assertNotEqual(q.identity(suite), q.identity(base))
        self.assertEqual(suite["applicability"]["revision"], base["applicability"]["revision"])
        self.assertIn("NOT official", suite["provenance"])
        self.assertEqual({case["id"] for case in suite["cases"]},
                         {"coding.metal", "conversation.coding", "reasoning.schedule"})
        for case in suite["cases"]:
            previous = original[case["id"]]
            self.assertEqual(case["turns"], previous["turns"])
            self.assertEqual(case["class"], "representative")
            self.assertEqual(case["reasoning_modes"], previous["reasoning_modes"])
            self.assertEqual(case["execution_strategies"], previous["execution_strategies"])
            self.assertEqual(previous["maximum_output"], 256)
            self.assertEqual(case["maximum_output"], 4096)
        cells = list(measurement.configurations(suite))
        self.assertEqual(len(cells), 18)
        self.assertTrue(all(cell["state"] == "UNQUALIFIED" for cell in cells))
        self.assertIn(suite, measurement.corpora(ROOT / "tests/vectors"))

    def test_suite_discovery_uses_schema_not_filename(self):
        suite = measurement.corpus(ROOT / "tests/vectors/deepseek_competitive.json")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "neutral.json").write_text(json.dumps(suite))
            (path / "unrelated_product.json").write_text(json.dumps({"schema":"other.vector.v1"}))
            (path / "array.json").write_text("[]")
            self.assertEqual(measurement.corpora(path), [suite])

    def test_suite_discovery_refuses_unknown_version_and_duplicate_selector(self):
        suite = measurement.corpus(ROOT / "tests/vectors/deepseek_competitive.json")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "a.json").write_text(json.dumps(suite))
            unsupported = dict(suite, schema="yvex.qualification.suite.v2")
            (path / "b.json").write_text(json.dumps(unsupported))
            with self.assertRaisesRegex(ValueError, "unsupported corpus"):
                measurement.corpora(path)
            (path / "b.json").write_text(json.dumps(dict(suite, revision="another")))
            with self.assertRaisesRegex(ValueError, "duplicate qualification suite selector"):
                measurement.corpora(path)

    def test_prefill_suite_binds_shared_literal_corpus(self):
        suite = measurement.corpus(ROOT / "tests/vectors/deepseek_prefill.json")
        self.assertEqual(suite["id"], "deepseek-prefill")
        self.assertIn("NOT official", suite["provenance"])
        self.assertIn("f53e0d80cb2d4492d24ebd63c7000c397b16ae70f9bf09b3763e5d8323ec209f", suite["provenance"])
        expected = {
            "prefill.promessi-512": "597dc7ecff70b1d634e596af71068213746c29d0924f9555443e8b87a6d08e6c",
            "prefill.promessi-2048": "a6120db0c07806165d7866d0e972caacfd1af0bea0e3c666f9b98c4d7d7bae64",
            "prefill.promessi-8192": "02f4f56643530dcf0b66d128cbe921b39a700bb3f13ea1f02f3bbeaf47080ff9",
        }
        import hashlib
        for case in suite["cases"]:
            prompt, = measurement.prompts(case)
            self.assertEqual(hashlib.sha256(prompt.encode()).hexdigest(), expected[case["id"]])
            self.assertEqual(case["reasoning_modes"], ["none", "high", "maximum"])
            self.assertEqual(case["execution_strategies"], ["target-only", "speculative"])
        self.assertEqual(len(list(measurement.configurations(suite))), 18)


class QualificationTests(unittest.TestCase):
    def test_reference_encoder_neither_writes_nor_reads_bytecode_cache(self):
        import py_compile
        spec = importlib.util.spec_from_file_location("reference", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "encoding/encoding_dsv4.py"
            source.parent.mkdir()
            source.write_text("value = 1\n")
            self.assertEqual(reference.load_encoding(root).value, 1)
            self.assertEqual(list(source.parent.iterdir()), [source])
            # An unchecked cache is a negative control: the source bytes, not
            # previously compiled code, must remain the reference authority.
            py_compile.compile(str(source), invalidation_mode=py_compile.PycInvalidationMode.UNCHECKED_HASH)
            source.write_text("value = 2\n")
            before = {str(p):p.read_bytes() for p in root.rglob("*") if p.is_file()}
            self.assertEqual(reference.load_encoding(root).value, 2)
            self.assertEqual(before, {str(p):p.read_bytes() for p in root.rglob("*") if p.is_file()})

    def test_checkpoint_specific_encoding_authority(self):
        spec = importlib.util.spec_from_file_location("reference", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        old = measurement.corpus(reference.CORPUS)
        new = measurement.corpus(ROOT / "tests/vectors/deepseek_0731_product.json")
        old_authority = reference.source_authority(old)
        new_authority = reference.source_authority(new)
        self.assertEqual(old["cases"], new["cases"])
        self.assertNotEqual(old_authority["revision"], new_authority["revision"])
        self.assertEqual(old_authority["files"]["tokenizer.json"], new_authority["files"]["tokenizer.json"])
        self.assertNotEqual(old_authority["files"]["encoding/encoding_dsv4.py"],
                            new_authority["files"]["encoding/encoding_dsv4.py"])
        for kind in ("prefill", "competitive"):
            previous = measurement.corpus(ROOT / f"tests/vectors/deepseek_{kind}.json")
            selected = measurement.corpus(ROOT / f"tests/vectors/deepseek_0731_{kind}.json")
            self.assertEqual(previous["cases"], selected["cases"])
            self.assertNotEqual(q.identity(previous), q.identity(selected))
            self.assertEqual(reference.source_authority(selected), new_authority)
        bad = copy.deepcopy(new)
        bad["source_authority"] = old["source_authority"]
        with self.assertRaisesRegex(ValueError, "checkpoint differs"):
            reference.source_authority(bad)
        for selector in ("../foreign.json#authority", "tests/vectors/manifest.json#missing"):
            bad["source_authority"] = selector
            with self.assertRaises(ValueError):
                reference.source_authority(bad)

    def test_independent_output_grammar_is_not_capture_or_yvex_pass(self):
        spec = importlib.util.spec_from_file_location("reference", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        class Source:
            thinking_end_token = "end"
            eos_token = "eos"
            @staticmethod
            def parse_message_from_completion_text(text, mode):
                if mode == "thinking" and "2" not in text:
                    raise ValueError("private generated text must not enter report")
        class Tokenizer:
            def token_to_id(self, text): return {"end":2,"eos":3}[text]
            def decode(self, tokens, skip_special_tokens): return str(tokens)
        def check(tokens, finish="eos", reasoning="high"):
            return reference.output_grammar(tokens, finish, reasoning, Source, Tokenizer())
        self.assertEqual(check([1,2,3])["state"], "PASS")
        self.assertEqual(check([1,3], reasoning="none")["state"], "PASS")
        self.assertEqual(check([1,3])["state"], "FAIL")
        self.assertNotIn("private", check([1,3])["reason"])
        self.assertEqual(check([1,2])["state"], "FAIL")
        self.assertEqual(check([1], finish="length")["state"], "NOT_MEASURED_TRUNCATED")

    def test_backend_plane_cannot_disappear_into_performance(self):
        value = self.receipt()
        value["claims"]["deployment-performance"]["state"] = "CHARACTERIZED"
        self.assertEqual(q.validate(value)["claims"]["backend-execution"]["state"], "UNQUALIFIED")
        del value["claims"]["backend-execution"]
        with self.assertRaisesRegex(ValueError, "independent evidence planes"):
            q.validate(value)

    def test_reference_publication_preserves_capture_not_conformance(self):
        value = json.loads((ROOT / "docs/evaluation/benchmarks/references/deepseek-independent-target.json").read_text())
        q.validate_reference_observation(value)
        self.assertEqual(len(value["cases"]), 24)
        self.assertEqual(sum(r["grammar"]["state"] == "FAIL" for r in value["cases"]), 1)
        bad = copy.deepcopy(value)
        bad["yvex_conformance"] = "PASS"
        with self.assertRaisesRegex(ValueError, "cannot promote"):
            q.validate_reference_observation(bad)
        bad = copy.deepcopy(value)
        row = next(r for r in bad["cases"] if r["finish"] == "length")
        row["grammar"]["state"] = "PASS"
        with self.assertRaisesRegex(ValueError, "truncated output"):
            q.validate_reference_observation(bad)
        bad = copy.deepcopy(value)
        bad["cases"].append(bad["cases"][0])
        with self.assertRaisesRegex(ValueError, "duplicate/invalid"):
            q.validate_reference_observation(bad)

    def test_failed_workload_is_visible_without_performance_samples(self):
        value = json.loads((ROOT / "docs/evaluation/benchmarks/qualification/deepseek-native-coding-high-bounded-refusal.json").read_text())
        q.validate(value)
        self.assertEqual(value["measurements"], [])
        self.assertEqual(value["provenance"]["case_outcomes"][0]["result"], "FAIL")
        value["provenance"]["case_outcomes"][0]["result"] = "QUALIFIED"
        with self.assertRaisesRegex(ValueError, "invalid case result"):
            q.validate(value)

    def receipt(self):
        target = {"schema":q.TARGET_SCHEMA, **{key:1 if kind == "integer" else "fixture"
                                               for key,kind in q.TARGET_FIELDS.items()}}
        return {"schema":q.RECEIPT_SCHEMA,"id":"fixture","title":"Fixture only",
                "target":target,"target_identity":q.target_id(target),"origin":"local",
                "claims":{p:{"state":"UNQUALIFIED","scope":"fixture","required_evidence":[],
                             "evidence":{},"blockers":[]} for p in q.PLANES},
                "measurements":[],"provenance":{"source_stability":"fixture","evidence_class":"fixture","profiled":False},
                "limitations":["Not model evidence"]}

    def test_reference_checkpoint_and_independence(self):
        spec = importlib.util.spec_from_file_location("reference", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        inputs = {"authority":{"repository":"fixture", "revision":"a"}, "cases":[]}
        value = {"schema":"yvex.deepseek.independent-inference.v1", "source":{"repository":"fixture", "revision":"b"}}
        with self.assertRaisesRegex(ValueError, "checkpoint not exact"):
            reference.validate_reference(inputs, value)
        value["source"]["revision"] = "a"
        value["implementation"] = {"independent_of_yvex":False, "name":"YVEX"}
        with self.assertRaisesRegex(ValueError, "independent inference owner"):
            reference.validate_reference(inputs, value)

    def test_reference_requires_complete_policy_bound_raw_capture(self):
        spec = importlib.util.spec_from_file_location("reference", ROOT / "tests/reference/deepseek_inference.py")
        reference = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(reference)
        request = dict(case="fixture", reasoning="none", workload_class="representative",
                       input_identity="fixture-input", maximum_output=32,
                       sampling={"temperature":0, "stochastic":False}, prompt_token_ids=[1, 2])
        inputs = dict(authority={"repository":"fixture", "revision":"a"}, cases=[request])
        payload = dict(input_identity="fixture-input", output_token_ids=[3], text="fixture",
                       maximum_output=32, sampling=request["sampling"], prompt_token_ids=[1, 2],
                       finish_reason="eos")
        record = dict(case="fixture", reasoning="none", input_identity="fixture-input",
                      output_token_ids=[3], output_sha256=measurement.hashlib.sha256(b"fixture").hexdigest(),
                      raw_evidence_path="capture.json", finish_reason="eos")
        value = dict(schema="yvex.deepseek.independent-inference.v1", source=inputs["authority"],
                     checkpoint_manifest_sha256="a"*64,
                     implementation=dict(independent_of_yvex=True, name="fixture-oracle",
                       revision="a", executable_sha256="b"*64, command=["fixture"],
                       environment={"scope":"fixture"}, physical_representation="fixture"), cases=[record])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def write(raw):
                (root / "capture.json").write_text(json.dumps(raw))
                record["raw_evidence_sha256"] = measurement.digest(root / "capture.json")
            write(payload)
            self.assertEqual(reference.validate_reference(inputs, value, root)["yvex_conformance"], "NOT_RUN")
            for key, changed in [("maximum_output",8), ("sampling",{"temperature":1}),
                                 ("prompt_token_ids",[1]), ("finish_reason","cancelled")]:
                write(dict(payload, **{key:changed}))
                with self.assertRaisesRegex(ValueError, "execution policy"):
                    reference.validate_reference(inputs, value, root)
            write(payload)
            with self.assertRaisesRegex(ValueError, "coverage differs"):
                reference.validate_reference(inputs, dict(value, cases=[]), root)
            with self.assertRaisesRegex(ValueError, "empty independent"):
                reference.validate_reference(dict(inputs, cases=[]), value, root)
            record["finish_reason"] = "length"
            write(dict(payload, finish_reason="length"))
            with self.assertRaisesRegex(ValueError, "declared output bound"):
                reference.validate_reference(inputs, value, root)
            record["finish_reason"] = "cancelled"
            with self.assertRaisesRegex(ValueError, "complete successfully"):
                reference.validate_reference(inputs, value, root)

    def metric(self, name="decode.post-first.committed"):
        _, unit, definition = q.METRICS[name]
        return dict(metric=name, definition=definition, unit=unit, case="fixture", prompt_identity="fixture",
                    reference_identity="fixture", session_state="fresh", warm_state="warm",
                    output_bound=256, samples=[1,2,3], statistics=q.statistics_for([1,2,3]),
                    scope="fixture", evidence="fixture")

    def test_finite_metrics_do_not_relabel_forward_as_complete_caller_time(self):
        receipt = self.receipt()
        receipt["measurements"] = [self.metric("compute.model-forward"),
                                   self.metric("request.client-complete"),
                                   self.metric("result.encoded-bytes")]
        q.validate(receipt)
        self.assertIn("excludes input admission", receipt["measurements"][0]["definition"])
        self.assertEqual(q.METRICS["result.encoded-bytes"][:2], ("product-path", "byte"))
        receipt["measurements"][0]["definition"] = q.METRICS["request.client-complete"][2]
        with self.assertRaisesRegex(ValueError, "denominator/definition changed"):
            q.validate(receipt)

    def test_diagnostics_are_bounded_nullable_facts_not_performance_metrics(self):
        receipt = self.receipt()
        fact = dict(id="mapped-rss", case="fixture", value=0, unit="byte",
                    definition="Observed Linux mapping RSS; not GPU residency", evidence="fixture")
        receipt["provenance"]["diagnostics"] = [fact]
        q.validate(receipt)
        receipt["provenance"]["diagnostics"][0]["value"] = None
        q.validate(receipt)
        for key, value in (("value", True), ("value", -1), ("value", float("nan")),
                           ("unit", "token/s"), ("evidence", ""), ("extra", "unknown")):
            bad = copy.deepcopy(receipt)
            bad["provenance"]["diagnostics"][0][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError): q.validate(bad)
        receipt["provenance"]["diagnostics"] *= 2
        with self.assertRaisesRegex(ValueError, "duplicate diagnostic"): q.validate(receipt)
        for bad in (None, {}, [fact] * 257):
            receipt["provenance"]["diagnostics"] = bad
            with self.assertRaisesRegex(ValueError, "diagnostic extent"): q.validate(receipt)

    def test_profiled_and_unknown_runs_never_become_performance_comparisons(self):
        left = self.receipt()
        left['measurements'] = [self.metric()]
        right = copy.deepcopy(left)
        q.comparison(left, left['measurements'][0], right, right['measurements'][0])
        for value in (True, None, 'false'):
            right['provenance']['profiled'] = value
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, 'explicitly unprofiled'):
                q.comparison(left, left['measurements'][0], right, right['measurements'][0])

    def test_target_all_axes_change_identity(self):
        target = self.receipt()["target"]
        initial = q.target_id(target)
        for key,kind in q.TARGET_FIELDS.items():
            candidate = dict(target); candidate[key] = 2 if kind == "integer" else "changed"
            self.assertNotEqual(initial, q.target_id(candidate), key)

    def test_no_ephemeral_identity_fields(self):
        for key in ("pid","time","path","device_uuid","observed_clock"):
            target = self.receipt()["target"]; target[key] = "not-semantic"
            with self.assertRaisesRegex(ValueError,"target fields"): q.target_id(target)

    def test_fixture_cannot_qualify(self):
        r=self.receipt(); c=r["claims"]["deployment-performance"]
        c.update(state="QUALIFIED", required_evidence=["runtime"],
                 evidence={"runtime":{"sha256":"a"*64,"locator":"fixture","result":"PASS"}})
        with self.assertRaisesRegex(ValueError,"stable source"): q.validate(r)
        r["provenance"]["source_stability"]="frozen"
        with self.assertRaisesRegex(ValueError,"fixture"): q.validate(r)

    def test_no_plane_promotion(self):
        r=self.receipt(); r["claims"]["family-conformance"]["state"]="CHARACTERIZED"
        q.validate(r)
        self.assertEqual(r["claims"]["representation-quality"]["state"],"UNQUALIFIED")
        self.assertEqual(r["origin"],"local")

    def test_blocked_requires_prerequisite(self):
        r=self.receipt(); r["claims"]["checkpoint-reference"]["state"]="BLOCKED"
        with self.assertRaisesRegex(ValueError,"prerequisite"): q.validate(r)

    def test_statistics_cannot_be_typed_independently(self):
        r=self.receipt(); m=self.metric(); r["measurements"]=[m]; m["statistics"]["median"]=200
        with self.assertRaisesRegex(ValueError,"statistics"): q.validate(r)

    def test_performance_comparison_refuses_topology_transport_strategy(self):
        for key, kind in q.TARGET_FIELDS.items():
            a=self.receipt();a["measurements"]=[self.metric()];b=copy.deepcopy(a)
            b["target"][key]=2 if kind=="integer" else "other"
            b["target_identity"]=q.target_id(b["target"])
            with self.assertRaisesRegex(ValueError,"incompatible targets"):
                q.comparison(a,a["measurements"][0],b,b["measurements"][0])

    def test_quantization_comparison_checkpoint_locked(self):
        a=self.receipt(); a["measurements"]=[self.metric("same-top-token")]; b=copy.deepcopy(a)
        b["target"]["representation"]="q4";b["target_identity"]=q.target_id(b["target"])
        self.assertEqual(q.comparison(a,a["measurements"][0],b,b["measurements"][0],("representation",))["kind"],"explicit-experiment")
        b["target"]["checkpoint"]="another";b["target_identity"]=q.target_id(b["target"])
        with self.assertRaisesRegex(ValueError,"checkpoint/reference"):
            q.comparison(a,a["measurements"][0],b,b["measurements"][0],("representation","checkpoint"))

    def test_quality_oracle(self):
        r=q.probability_quality([[.25,.75]],[[.25,.75]],[1])
        self.assertEqual(r["kl"],0); self.assertEqual(r["probability_delta_rms"],0)
        self.assertEqual(r["same_top_token_rate"],1)
        self.assertAlmostEqual(r["perplexity"],1/.75)

    def test_quality_comparison_refuses_two_missing_references(self):
        for missing in (None, "", " "):
            a=self.receipt(); a["measurements"]=[self.metric("same-top-token")]
            a["measurements"][0]["reference_identity"]=missing
            with self.assertRaisesRegex(ValueError,"independent reference identity"):
                q.comparison(a,a["measurements"][0],a,a["measurements"][0])

    def test_partial_logprobs_and_zero_support_refuse(self):
        with self.assertRaisesRegex(ValueError,"full normalized"):
            q.probability_quality([[.1,.2]],[[.1,.2]],[1])
        with self.assertRaisesRegex(ValueError,"unbounded"):
            q.probability_quality([[.5,.5]],[[0.,1.]],[1])


if __name__ == "__main__":
    unittest.main()
