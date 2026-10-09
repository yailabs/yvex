#!/usr/bin/env python3
"""Differential native operator contracts; never use real credentials or a live host."""

from __future__ import annotations

import argparse
import errno
import hashlib
import json
import os
import pty
from pathlib import Path
import re
import runpy
import select
import shutil
import subprocess
import struct
import sys
import tempfile
import termios
import time


ROOT = Path(__file__).resolve().parents[1]


def invoke(binary: Path, words: list[str], environment: dict[str, str]):
    result = subprocess.run([str(binary), *words], cwd=ROOT, env=environment,
                            capture_output=True, text=True, timeout=15)
    assert "\x1b" not in result.stdout, (words, result.stdout)
    return result


def normalized(value):
    if isinstance(value, dict):
        return {key: normalized(child) for key, child in value.items()
                if key != "last_checked_at"}
    if isinstance(value, list):
        return [normalized(child) for child in value]
    return value


def terminal_output(binary: Path, words: list[str], environment: dict[str, str]) -> str:
    """Capture bounded catalog output through a real terminal, without a host."""
    master, slave = pty.openpty()
    attributes = termios.tcgetattr(slave)
    attributes[1] &= ~termios.OPOST
    termios.tcsetattr(slave, termios.TCSANOW, attributes)
    process = None
    try:
        os.set_blocking(master, False)
        process = subprocess.Popen([str(binary), *words], cwd=ROOT, env=environment,
                                   stdout=slave, stderr=subprocess.PIPE,
                                   stdin=subprocess.DEVNULL)
        chunks = []
        deadline = time.monotonic() + 15
        while True:
            assert time.monotonic() < deadline, ("terminal output timeout", words)
            while select.select([master], [], [], 0.02 if process.poll() is None else 0)[0]:
                try:
                    chunk = os.read(master, 4096)
                except OSError as error:
                    if error.errno in (errno.EIO, errno.EAGAIN):
                        break
                    raise
                if not chunk:
                    break
                chunks.append(chunk)
            if process.poll() is not None:
                # Drain before closing the caller-owned slave: Darwin may
                # discard pending output when the last slave disappears.
                while select.select([master], [], [], 0)[0]:
                    try:
                        chunk = os.read(master, 4096)
                    except OSError as error:
                        if error.errno in (errno.EIO, errno.EAGAIN):
                            break
                        raise
                    if not chunk:
                        break
                    chunks.append(chunk)
                break
        _, error = process.communicate(timeout=1)
        assert process.returncode == 0, error
        return b"".join(chunks).decode().replace("\r\n", "\n")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if slave >= 0:
            os.close(slave)
        os.close(master)


def accounts(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-accounts-") as temporary:
        directory = Path(temporary).resolve()
        environment = dict(os.environ)
        for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN"):
            environment.pop(name, None)
        environment.update({
            "YVEX_CONFIG_DIR": str(directory / "config"),
            "YVEX_HF_CLI": str(ROOT / "tests/fixtures/bin/fake-hf"),
            "YVEX_GH_CLI": str(ROOT / "tests/fixtures/bin/fake-gh"),
            "YVEX_FAKE_HF_LOG": str(directory / "hf.log"),
            "YVEX_FAKE_GH_LOG": str(directory / "gh.log"),
            "YVEX_FAKE_HF_STATE": str(directory / "hf.auth"),
            "YVEX_FAKE_GH_STATE": str(directory / "gh.auth"),
            "YVEX_FAKE_HF_AUTH": "0", "YVEX_FAKE_GH_AUTH": "0",
            "NO_COLOR": "1",
        })
        cases = [
            (["providers"], {}, 0),
            (["status"], {}, 0),
            (["whoami", "huggingface"], {}, 5),
            (["whoami", "github"], {}, 5),
            (["whoami", "hf"], {"YVEX_FAKE_HF_AUTH": "1"}, 0),
            (["whoami", "gh"], {"YVEX_FAKE_GH_AUTH": "1"}, 0),
            (["ensure", "hf", "--interactive", "never"], {}, 5),
            (["ensure", "gh", "--required"], {}, 5),
            (["ensure", "hf"], {"YVEX_FAKE_HF_AUTH": "1"}, 0),
            (["login", "huggingface"], {"YVEX_FAKE_HF_LOGIN_FAIL": "1"}, 1),
            (["login", "github"], {"YVEX_FAKE_GH_LOGIN_FAIL": "1"}, 1),
            (["logout", "hf"], {}, 0),
            (["logout", "gh"], {}, 0),
            (["whoami", "gh", "--token-env", "YVEX_FIXTURE_TOKEN"],
             {"YVEX_FIXTURE_TOKEN": "synthetic-secret-do-not-print"}, 0),
        ]
        for arguments, overrides, expected_exit in cases:
            words = ["source", "accounts", *arguments, "--json"]
            current_environment = {**environment, **overrides}
            result = invoke(binary, words, current_environment)
            assert result.returncode == expected_exit, (words, result)
            projected = json.loads(result.stdout)
            assert "synthetic-secret-do-not-print" not in result.stdout + result.stderr
            if reference:
                canonical = ["huggingface" if word == "hf" else "github" if word == "gh" else word
                             for word in words]
                # The old registry omitted aliases admitted by the native provider
                # parser. Compare canonical spellings, and qualify the aliases above.
                previous = invoke(reference, canonical, current_environment)
                assert previous.returncode == expected_exit, (words, previous)
                # The legacy login/logout renderer inherited provider stdout. Its
                # final schema is the differential oracle, not that contamination.
                lines = [line for line in previous.stdout.splitlines() if line.startswith('{')]
                assert lines, (words, previous)
                assert normalized(projected) == normalized(json.loads(lines[-1])), words
            count += 1
        for arguments in (["whoami", "not-a-provider"], ["ensure", "hf", "--interactive", "sometimes"]):
            result = invoke(binary, ["source", "accounts", *arguments, "--json"], environment)
            assert result.returncode == 2 and not result.stdout, (arguments, result)
            count += 1
        state = (directory / "config/accounts.local.json").read_text()
        assert "synthetic-secret-do-not-print" not in state
        assert json.loads(state)["providers"][0]["raw_token_stored_by_yvex"] is False
    return count


def tokenizers(binary: Path, reference: Path | None) -> int:
    fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
    environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "180"}
    count = 0
    for text in ("hello world", "hello", "hello\x1b[2J world"):
        words = ["inspect", "tokenizer", "encode", str(fixture), "--text", text, "--pieces"]
        result = invoke(binary, words, environment)
        assert result.returncode == 0, result
        ids = re.search(r"^\s*ids\s+([0-9 ]+)$", result.stdout, re.M)
        assert ids, result.stdout
        if reference:
            previous = invoke(reference, words, environment)
            assert previous.returncode == 0, previous
            reference_ids = re.search(r"^ids:\s*([0-9 ]+)$", previous.stdout, re.M)
            assert reference_ids and ids[1].strip() == reference_ids[1].strip()
        count += 1
    for arguments in (["decode", "--ids", "3,4,5"], ["decode", "--ids", "-1"],
                      ["encode", "--text", "hello", "--bos"]):
        result = invoke(binary, ["inspect", "tokenizer", arguments[0], str(fixture), *arguments[1:]], environment)
        if arguments[-1] == "3,4,5":
            assert result.returncode == 0 and '"hello world"' in result.stdout, result
        else:
            assert result.returncode != 0, result
        count += 1
    result = invoke(binary, ["inspect", "tokenizer", "prompt", str(fixture),
                            "--system", "fixture system", "--user", "hello",
                            "--assistant", "world", "--user", "hello again"], environment)
    assert result.returncode == 0, result
    assert result.stdout.index("fixture system") < result.stdout.index("hello") < result.stdout.index("world")
    assert result.stdout.index("world") < result.stdout.index("hello again")
    # The fixture is not an exact source tokenizer and must not advertise runtime qualification.
    result = invoke(binary, ["inspect", "tokenizer", str(fixture)], environment)
    assert result.returncode == 0, result
    assert re.search(r"tokenizer_runtime_ready\s+false", result.stdout)
    assert re.search(r"generation_ready\s+false", result.stdout)
    return count + 2


def paths(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-paths-") as temporary:
        directory = Path(temporary).resolve()
        environment = dict(os.environ)
        environment.pop("YVEX_MODELS_ROOT", None)
        environment.update({
            "YVEX_CONFIG_DIR": str(directory / "config"),
            "YVEX_CACHE_DIR": str(directory / "cache"),
            "YVEX_STATE_DIR": str(directory / "state"),
            "YVEX_DATA_DIR": str(directory / "data"), "NO_COLOR": "1", "COLUMNS": "180",
        })
        models = directory / "models"
        configured = invoke(binary, ["inspect", "paths", "configure", "--models-root", str(models), "--create"], environment)
        assert configured.returncode == 0, configured
        assert models.is_dir()
        count += 1
        for arguments in ([], ["--audit"], ["--output", "audit"],
                          ["resolve", "--family", "deepseek", "--kind", "source"],
                          ["resolve", "--family", "qwen", "--kind", "gguf"],
                          ["resolve", "--family", "mamba2", "--kind", "reference"]):
            result = invoke(binary, ["inspect", "paths", *arguments], environment)
            assert result.returncode == 0 and str(models) in result.stdout, result
            if reference:
                previous = invoke(reference, ["inspect", "paths", *arguments], environment)
                if arguments and arguments[0] == "resolve" and "expected 0 positional arguments" in previous.stderr:
                    # The original registry hid an existing parser/domain action.
                    # These resolve controls qualify the repaired generated grammar;
                    # the stale product reference cannot dispatch that owner.
                    assert previous.returncode == 2
                    count += 1
                    continue
                assert previous.returncode == 0, previous
                for key in ("models_root", "hf_root", "gguf_root", "registry_root", "path", "exists"):
                    fact = re.search(rf"^\s*{key}(?::|\s)\s*(.+)$", previous.stdout, re.M)
                    if fact:
                        assert fact[1] in result.stdout, (key, result.stdout)
            count += 1
        result = invoke(binary, ["inspect", "paths", "--run", "--create"], environment)
        assert result.returncode == 0, result
        run_root = re.search(r"^\s*root\s+(.+)$", result.stdout, re.M)
        assert run_root and Path(run_root[1]).is_dir(), result
        count += 1
        for arguments in (["configure"], ["configure", "--reset", "--create"],
                          ["resolve", "--family", "qwen"], ["--models-root", str(models)],
                          ["--output", "nope"],
                          ["resolve", "--family", "invalid", "--kind", "source"]):
            result = invoke(binary, ["inspect", "paths", *arguments], environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
        reset = invoke(binary, ["inspect", "paths", "configure", "--reset"], environment)
        assert reset.returncode == 0 and re.search(r"removed\s+true", reset.stdout), reset
        assert models.is_dir(), "reset must remove only configuration, never payload"
        count += 1
    return count


def catalogs(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-catalog-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
        registry = directory / "models.json"
        binding = directory / "invalid.binding"
        binding.write_bytes(b"malformed synthetic binding")
        alias = "deepseek4-v4-flash-dspark-selected-embed"
        entry = dict(alias=alias, family="deepseek4", model="v4-flash",
                     path=str(fixture), sha256=hashlib.sha256(fixture.read_bytes()).hexdigest(),
                     file_size=fixture.stat().st_size, format="gguf", runtime_profile="single-artifact",
                     runtime_binding=str(binding), runtime_target="deepseek4-v4-flash-dspark",
                     runtime_backend="cpu", runtime_engine_kind="text", runtime_execution_strategy="speculative",
                     runtime_context=128)
        registry.write_text(json.dumps(dict(schema="yvex.models.local.v8", models=[entry],
                                            working_set=[], publications=[])))
        before = registry.read_bytes()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"), "NO_COLOR": "1"}
        common = ["--models-root", str(models), "--registry", str(registry)]
        for arguments in (["source", "list"], ["artifact", "list"], ["profile", "list"],
                          ["profile", "show", alias]):
            words = [*arguments, *common, "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0, result
            current = json.loads(result.stdout)
            if reference and arguments != ["profile", "show", alias]:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and current == json.loads(previous.stdout), (current, previous)
            if arguments[0] == "profile":
                profile = current["profile"] if arguments[1] == "show" else current["profiles"][0]
                assert profile["launchable"] is False
                assert profile["blocker"].startswith("malformed-binding:"), profile
            count += 1
        for width in (40, 80, 180):
            result = invoke(binary, ["profile", "list", *common], {**environment, "COLUMNS": str(width)})
            assert result.returncode == 0 and "malformed-binding:" in result.stdout, result
            count += 1
        detailed = invoke(binary, ["model", "show", "v4-flash", *common],
                          {**environment, "COLUMNS": "220"})
        assert detailed.returncode == 0, detailed
        assert all(section in detailed.stdout for section in ("MODEL", "REPRESENTATION", "RUNTIME")), detailed
        assert str(fixture) in detailed.stdout and "not launchable" in detailed.stdout, detailed
        assert "malformed-binding:" in detailed.stdout and alias in detailed.stdout, detailed
        assert registry.read_bytes() == before
        count += 1
        for domain in ("source", "profile"):
            result = invoke(binary, [domain, "show", "missing", *common, "--json"], environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
        assert registry.read_bytes() == before, "read projections must not mutate deployment authority"
        scanned = invoke(binary, ["profile", "scan", "--root", str(models), "--registry", str(registry)], environment)
        assert scanned.returncode == 0 and "models-scan" in scanned.stdout, scanned
        assert registry.read_bytes() == before, "scan reports candidates, it does not register them"
        count += 1
        removed = invoke(binary, ["profile", "remove", alias, "--registry", str(registry)], environment)
        assert removed.returncode == 0 and "models-removed" in removed.stdout, removed
        assert json.loads(registry.read_text())["models"] == []
        count += 1
    return count


def artifacts(binary: Path, reference: Path | None) -> int:
    count = 0
    environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "180"}
    fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
    for command in (["artifact", "show"], ["inspect", "artifact", "metadata"], ["inspect", "artifact", "tensors"]):
        result = invoke(binary, [*command, str(fixture)], environment)
        assert result.returncode == 0, result
        if reference:
            previous = invoke(reference, [*command, str(fixture)], environment)
            assert previous.returncode == 0, previous
            for key in ("version", "metadata_count", "tensor_count", "alignment", "architecture", "model_name", "known_tensor_bytes", "status"):
                fact = re.search(rf"^\s*{key}(?::|\s)\s*(.+)$", previous.stdout, re.M)
                if fact:
                    assert re.search(rf"^\s*{key}\s+{re.escape(fact[1])}$", result.stdout, re.M), (key, result)
        if command[-1] == "metadata":
            assert "tokenizer.ggml.tokens" in result.stdout and "array<string>" in result.stdout, result
        elif command[-1] == "tensors":
            assert "token_embd.weight" in result.stdout and re.search(r"range_status\s+valid", result.stdout), result
        else:
            assert "descriptor-only" in result.stdout and "execution-ready" not in result.stdout, result
        count += 1
        for malformed in ("does-not-exist.gguf", "bad-magic.gguf", "short-header.gguf"):
            path = ROOT / "tests/fixtures/gguf" / malformed
            failure = invoke(binary, [*command, str(path)], environment)
            assert failure.returncode != 0 and not failure.stdout, failure
            assert "YVEX_ERR_" in failure.stderr, failure
            count += 1
    return count


def profiles(binary: Path, reference: Path | None) -> int:
    count = 0
    fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
    with tempfile.TemporaryDirectory(prefix="yvex-rust-profile-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(root / "config"), "NO_COLOR": "1", "COLUMNS": "180"}
        registry = root / "models.json"
        alias = "deepseek4-v4-flash-dspark-selected-embed"
        create = ["profile", "create", "--path", str(fixture), "--alias", alias,
                  "--family", "deepseek4", "--model", "v4-flash-dspark", "--scope", "selected",
                  "--class", "embed", "--support-level", "selected-tensor-materialized"]
        result = invoke(binary, [*create, "--registry", str(registry)], environment)
        assert result.returncode == 0 and "models-added" in result.stdout, result
        current = json.loads(registry.read_text())["models"][0]
        assert current["sha256"] == hashlib.sha256(fixture.read_bytes()).hexdigest(), current
        assert current["execution_ready"] is False, "registration must not imply runtime readiness"
        if reference:
            previous_registry = root / "previous.json"
            previous = invoke(reference, [*create, "--registry", str(previous_registry)], environment)
            assert previous.returncode == 0, previous
            assert current == json.loads(previous_registry.read_text())["models"][0]
        count += 1
        before = registry.read_bytes()
        for arguments in ([], ["--audit"], ["--output", "audit"], ["--json"]):
            result = invoke(binary, ["profile", "verify", alias, "--registry", str(registry), *arguments], environment)
            assert result.returncode == 0 and "models-identity-pass" in result.stdout, result
            assert re.search(r"identity_status\s+pass", result.stdout), result
            if reference:
                previous = invoke(reference, ["profile", "verify", alias, "--registry", str(registry), *arguments], environment)
                assert previous.returncode == 0 and "models-identity-pass" in previous.stdout, previous
            count += 1
        integrity_words = ["artifact", "verify", "integrity", alias, "--audit"]
        integrity_env = {**environment, "YVEX_MODELS_REGISTRY": str(registry)}
        result = invoke(binary, integrity_words, integrity_env)
        if reference:
            previous = invoke(reference, integrity_words, integrity_env)
            assert result.returncode == previous.returncode, (result, previous)
            for key in ("identity_status", "metadata_status", "readiness_status", "report_status", "status"):
                fact = re.search(rf"^\s*{key}(?::|\s)\s*(.+)$", previous.stdout, re.M)
                assert fact and re.search(rf"^\s*{key}\s+{re.escape(fact[1])}$", result.stdout, re.M), (
                    key, result, previous)
        count += 1
        for arguments in (["--ctx", "128"], ["--startup-profile", "composite"],
                          ["--runtime-binding", "fixture.binding", "--target", "fixture", "--backend", "cpu"],
                          ["--sha256", "0" * 64], []):
            result = invoke(binary, [*create, "--registry", str(registry), *arguments], environment)
            assert result.returncode != 0 and not result.stdout, result
            assert registry.read_bytes() == before, "refusal must not mutate saved deployment authority"
            count += 1
        drift = json.loads(before)
        drift["models"][0]["sha256"] = "0" * 64
        registry.write_text(json.dumps(drift))
        result = invoke(binary, ["profile", "verify", alias, "--registry", str(registry), "--audit"], environment)
        assert result.returncode == 4 and "models-identity-fail" in result.stdout, result
        assert re.search(r"identity_status\s+fail", result.stdout), result
        count += 1
        for registry_value, options in (
                (drift, []), (json.loads(before), ["--expect-sha256", "0" * 64])):
            registry.write_text(json.dumps(registry_value))
            result = invoke(binary, [*integrity_words, "--backend", "cpu", *options], integrity_env)
            assert result.returncode != 0 and re.search(r"backend_status\s+not-opened", result.stdout), result
            if reference:
                previous = invoke(reference, [*integrity_words, "--backend", "cpu", *options], integrity_env)
                assert result.returncode == previous.returncode, (result, previous)
                for key in ("identity_status", "metadata_status", "readiness_status"):
                    fact = re.search(rf"^\s*{key}(?::|\s)\s*(.+)$", previous.stdout, re.M)
                    assert fact and re.search(rf"^\s*{key}\s+{re.escape(fact[1])}$", result.stdout, re.M), (
                        key, result, previous)
            count += 1
        drift["models"][0]["sha256"] = current["sha256"]
        drift["models"][0]["primary_tensor_dtype"] = "Q2_K"
        registry.write_text(json.dumps(drift))
        result = invoke(binary, ["profile", "verify", alias, "--registry", str(registry), "--audit"], environment)
        assert result.returncode == 4 and "models-metadata-drift" in result.stdout, result
        assert "primary-tensor-dtype-mismatch" in result.stdout, result
        count += 1
        result = invoke(binary, [*integrity_words, "--backend", "cpu"], integrity_env)
        assert result.returncode != 0 and re.search(r"metadata_status\s+fail", result.stdout), result
        assert re.search(r"backend_status\s+not-opened", result.stdout), result
        count += 1
    return count


def sources(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-source-") as temporary:
        root = Path(temporary).resolve()
        source = root / "source"
        source.mkdir()
        (source / "config.json").write_text('{}\n')
        (source / "tokenizer.json").write_text('{}\n')
        (source / "model.safetensors").write_bytes(b"not a valid tensor source")
        manifest = root / "manifest.json"
        environment = {**os.environ, "NO_COLOR": "1", "YVEX_CONFIG_DIR": str(root / "config")}
        create = ["compile", "source", "manifest", "create", "--hf-repo", "test-org/test-model",
                  "--revision", "test-revision", "--local-path", str(source), "--status", "in-progress",
                  "--dry-run-log", str(root / "dry-run.log")]
        result = invoke(binary, [*create, "--out", str(manifest)], environment)
        assert result.returncode == 0 and manifest.exists(), result
        content = json.loads(manifest.read_text())
        assert content["schema"] == "yvex.source_manifest.v1" and content["status"] == "in-progress", content
        if reference:
            previous_manifest = root / "previous-manifest.json"
            previous = invoke(reference, [*create, "--out", str(previous_manifest)], environment)
            assert previous.returncode == 0 and content == json.loads(previous_manifest.read_text()), previous
        count += 1
        refused_path = root / "must-not-publish.json"
        refused = invoke(binary, [*create[:-4], "--status", "complete", "--out", str(refused_path)], environment)
        assert refused.returncode == 2 and not refused_path.exists() and "verifier-owned" in refused.stderr, refused
        count += 1
        for family in ("deepseek", "qwen", "gemma"):
            for location in ([], ["--source", str(source)]):
                words = ["compile", "source", "manifest", "report", "--family", family, "--release", "v0.1.0",
                         "--models-root", str(root / "models"), *location]
                for mode in (["--json"], ["--output", "json"], ["--audit"], ["--output", "table"]):
                    result = invoke(binary, [*words, *mode], environment)
                    if result.returncode != 0:
                        # Intentionally malformed local tensor material remains a refusal,
                        # not an apparently successful metadata report after migration.
                        assert location and "YVEX_ERR_" in result.stderr and not result.stdout, result
                        if reference:
                            previous = invoke(reference, [*words, *mode], environment)
                            assert result.returncode == previous.returncode and not previous.stdout, previous
                        count += 1
                        continue
                    if mode[-1] in ("json", "--json"):
                        current = json.loads(result.stdout)
                        assert current["tensor_payload_loaded"] is False and current["generation"] == "unsupported"
                        if reference:
                            previous = invoke(reference, [*words, *mode], environment)
                            assert previous.returncode == 0 and current == json.loads(previous.stdout), (current, previous)
                    else:
                        assert "top_blocker" in result.stdout and "source report only" in result.stdout, result
                    count += 1
        strict = invoke(binary, ["compile", "source", "manifest", "report", "--family", "deepseek",
                                 "--release", "v0.1.0", "--models-root", str(root / "models"), "--strict"], environment)
        assert strict.returncode == 5 and "exact-source-blocked" in strict.stdout, strict
        count += 1
        # A family profile's default target must not replace the resolved source
        # identity in audit output when the catalog has another family member.
        dynamic = root / "dynamic-source"
        dynamic.mkdir()
        result = invoke(binary, ["compile", "source", "manifest", "report", "--family", "qwen",
                                "--release", "v0.1.0", "--target", "qwen3-6-35b-a3b",
                                "--source", str(dynamic), "--audit"],
                        {**environment, "COLUMNS": "180"})
        assert result.returncode == 0, result
        assert re.search(r'(?m)^\s*target_id\s{2,}qwen3-6-35b-a3b$', result.stdout), result
        assert not re.search(r'(?m)^\s*target_id\s{2,}qwen3-8b$', result.stdout), result
        count += 1
        for words in (["source", "verify", "--source", str(source)],
                      ["source", "verify", "--source", str(source), "--models-root", str(root),
                       "--source-manifest", str(manifest), "--target", "deepseek4-v4-flash"],
                      ["compile", "source", "manifest", "report", "--family", "qwen", "--release", "v0.2.0"],
                      ["compile", "source", "manifest", "report", "--family", "qwen", "--release", "v0.1.0", "--strict"]):
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
        before = manifest.read_bytes()
        verify = ["source", "verify", "--source", str(source), "--models-root", str(root),
                  "--source-manifest", str(manifest)]
        result = invoke(binary, verify, environment)
        assert result.returncode != 0 and "YVEX_ERR_" in result.stderr and manifest.read_bytes() == before, result
        if reference:
            previous = invoke(reference, verify, environment)
            assert result.returncode == previous.returncode and manifest.read_bytes() == before, previous
        count += 1
    return count


def integrity(binary: Path, reference: Path | None) -> int:
    count = 0
    environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
    directory = ROOT / "tests/fixtures/gguf"
    fixture = directory / "valid-tokenizer-simple.gguf"
    digest = hashlib.sha256(fixture.read_bytes()).hexdigest()
    generated = tempfile.TemporaryDirectory(prefix="yvex-rust-integrity-")
    vector = runpy.run_path(str(directory / "make_fixtures.py"))
    selected = Path(generated.name) / "selected-f16.gguf"
    selected.write_bytes(vector["file_bytes"](
        [vector["kv"]("general.architecture", vector["STRING"], "llama")],
        [vector["tensor_info"]("token_embd.weight", [4, 8], 1, 0)], b"\x00" * 64))
    controls = [
        (fixture, [], 0),
        (fixture, ["--expect-sha256", digest], 0),
        (fixture, ["--expect-sha256", "0" * 64], None),
        (fixture, ["--require-token-embedding"], None),
        (selected, ["--require-token-embedding"], 0),
        (selected, ["--partial-token", "1"], 0),
        (selected, ["--partial-token", "4294967295"], None),
        (directory / "valid-minimal.gguf", ["--require-token-embedding"], None),
        (directory / "bad-magic.gguf", [], None),
        (directory / "tensor-offset-out-of-bounds.gguf", [], None),
        (directory / "tensor-dim-zero.gguf", [], None),
        (directory / "tensor-dim-overflow.gguf", [], None),
    ]
    keys = ("digest_status", "identity_status", "metadata_status", "readiness_status",
            "selected_embedding_ready", "selected_embedding_shape", "tensor_count",
            "tensor_ranges_invalid", "tensor_shapes_invalid", "materialization_preflight",
            "backend_status", "report_status", "status")
    for detailed in (False, True):
        for path, options, expected in controls:
            words = ["artifact", "verify", *(["integrity"] if detailed else []), str(path),
                     *options, *(["--audit", "--backend", "cpu"] if detailed else [])]
            result = invoke(binary, words, environment)
            assert (result.returncode == 0) == (expected == 0), (words, result)
            if reference:
                previous = invoke(reference, words, environment)
                assert result.returncode == previous.returncode, (words, result, previous)
                for key in keys:
                    fact = re.search(rf"^\s*{key}(?::|\s)\s*(.+)$", previous.stdout, re.M)
                    if fact:
                        assert re.search(rf"^\s*{key}\s+{re.escape(fact[1])}$", result.stdout, re.M), (
                            words, key, fact[1], result)
            if detailed and "--partial-token" in options and options[-1] == "4294967295":
                assert re.search(r"selected_embedding_ready\s+false", result.stdout), result
                assert re.search(r"readiness_status\s+fail", result.stdout), result
                assert re.search(r"backend_status\s+not-opened", result.stdout), result
            count += 1
    for options in (["--partial-token", "-1"], ["--backend", "cpu"],
                    ["integrity", str(fixture), "--backend", "unknown"]):
        words = (["artifact", "verify", *options] if options[0] == "integrity" else
                 ["artifact", "verify", str(fixture), *options])
        failure = invoke(binary, words, environment)
        assert failure.returncode == 2 and not failure.stdout, failure
        count += 1
    generated.cleanup()
    return count


def native_weights(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-native-") as temporary:
        root = Path(temporary).resolve()
        source = root / "sorgente_日本語"
        source.mkdir()
        header = {
            "tiny.weight": {"dtype": "F16", "shape": [2, 2], "data_offsets": [0, 8]},
            "日本語.weight": {"dtype": "BF16", "shape": [2], "data_offsets": [8, 12]},
        }
        encoded = json.dumps(header, ensure_ascii=False, separators=(",", ":")).encode()
        (source / "model.safetensors").write_bytes(struct.pack("<Q", len(encoded)) + encoded + b"\0" * 12)
        empty = root / "empty"
        empty.mkdir()
        malformed = root / "malformed"
        malformed.mkdir()
        (malformed / "model.safetensors").write_bytes(b"invalid")
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
        controls = [
            (source, [], 0), (source, ["--tensor", "日本語.weight"], 0),
            (source, ["--limit", "0"], 0), (source, ["--limit", "1"], 0),
            (source, ["--json"], 0), (source, ["--tensor", "missing", "--json"], 0),
            (source, ["--tensor", "missing"], 4), (empty, [], 0), (empty, ["--json"], 0),
            (malformed, [], None), (malformed, ["--json"], None),
            (root / "absent", [], None),
        ]
        for location, options, expected in controls:
            words = ["inspect", "source", "tensors", "--source", str(location), *options]
            result = invoke(binary, words, environment)
            assert (result.returncode == 0) == (expected == 0), (words, result)
            if expected is not None:
                assert result.returncode == expected, result
            if "--json" in options and result.returncode == 0:
                value = json.loads(result.stdout)
                assert value["schema"] == "yvex.native_weights.v1", value
                assert value["summary"]["tensor_count"] == (2 if location == source else 0), value
            elif result.returncode == 0:
                assert re.search(r"status\s+native-weights", result.stdout), result
                if "--tensor" in options:
                    assert "日本語.weight" in result.stdout and "TENSOR  tiny.weight" not in result.stdout, result
            else:
                assert not result.stdout and "YVEX_ERR_" in result.stderr, result
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == result.returncode, (words, result, previous)
                if "--json" in options and result.returncode == 0:
                    assert json.loads(previous.stdout) == json.loads(result.stdout), (result, previous)
            count += 1
        for options in ([], ["--source", str(source), "--limit", "-1"],
                        ["--source", str(source), "--limit", "18446744073709551616"]):
            result = invoke(binary, ["inspect", "source", "tensors", *options], environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
    return count


def artifact_construction(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-emission-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
        for qtype, expected_bytes in (("F32", 128), ("F16", 64)):
            output = root / f"owned-{qtype}.gguf"
            words = ["compile", "artifact", "emit", "--out", str(output), "--target-qtype", qtype,
                     "--model-name", "bounded-owned-fixture", "--arch", "llama"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0 and output.exists(), result
            assert re.search(rf"tensor_payload_bytes\s+{expected_bytes}$", result.stdout, re.M), result
            assert re.search(r"roundtrip_validated\s+yes$", result.stdout, re.M), result
            content = output.read_bytes()
            if reference:
                previous_output = root / f"previous-{qtype}.gguf"
                previous = invoke(reference, [*words[:4], str(previous_output), *words[5:]], environment)
                assert previous.returncode == 0 and content == previous_output.read_bytes(), previous
            count += 1
            refusal = invoke(binary, words, environment)
            assert refusal.returncode != 0 and output.read_bytes() == content, refusal
            count += 1
            overwrite = invoke(binary, [*words, "--overwrite"], environment)
            assert overwrite.returncode == 0 and output.read_bytes() == content, overwrite
            count += 1
            for action in ("inspect", "validate", "compare"):
                extra = ["--native-source", str(root)] if action == "compare" else []
                template = ["compile", "artifact", "template", action, "--template", str(output), *extra]
                result = invoke(binary, template, environment)
                assert result.returncode == 0 and "template-" in result.stdout, result
                if reference:
                    previous = invoke(reference, template, environment)
                    assert previous.returncode == result.returncode, previous
                    fact = re.search(r"^status:\s*(.+)$", previous.stdout, re.M)
                    assert fact and re.search(rf"status\s+{re.escape(fact[1])}$", result.stdout, re.M), (result, previous)
                count += 1
        for fixture in ("valid-minimal.gguf", "valid-tokenizer-simple.gguf", "bad-magic.gguf"):
            for action in ("inspect", "validate"):
                words = ["compile", "artifact", "template", action, "--template",
                         str(ROOT / "tests/fixtures/gguf" / fixture)]
                result = invoke(binary, words, environment)
                if reference:
                    previous = invoke(reference, words, environment)
                    assert result.returncode == previous.returncode, (result, previous)
                if result.returncode == 0:
                    assert "template-" in result.stdout, result
                else:
                    assert "YVEX_ERR_" in result.stderr and not result.stdout, result
                count += 1
        for words in (["compile", "artifact", "emit"],
                      ["compile", "artifact", "template", "compare", "--template", str(output)],
                      ["compile", "artifact", "emit", "--out", str(root / "invalid.gguf"),
                       "--target-qtype", "not-a-qtype"],
                      ["compile", "artifact", "template", "inspect",
                       "--template", str(root / "absent-template")]):
            result = invoke(binary, words, environment)
            assert result.returncode != 0 and not result.stdout, result
            count += 1
        assert not (root / "invalid.gguf").exists() and not (root / "missing.gguf").exists()
    return count


def conversion(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-conversion-") as temporary:
        root = Path(temporary).resolve()
        source = root / "native"
        source.mkdir()
        payload = struct.pack("<32e", *(index / 16 for index in range(32)))
        header = {"model.embed_tokens.weight": {"dtype": "F16", "shape": [8, 4],
                                               "data_offsets": [0, len(payload)]}}
        encoded = json.dumps(header, separators=(",", ":")).encode()
        (source / "model.safetensors").write_bytes(struct.pack("<Q", len(encoded)) + encoded + payload)
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
        prefix = ["compile", "quant", "convert"]
        for limit in ([], ["--limit", "1"], ["--limit", "0", "--require-all"]):
            plan = root / "plan.json"
            words = [*prefix, "plan", "--arch", "qwen3", "--native-source", str(source),
                     "--out-plan", str(plan), *limit]
            result = invoke(binary, words, environment)
            assert result.returncode == 0 and "conversion-plan-written" in result.stdout, result
            current = json.loads(plan.read_text())
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and current == json.loads(plan.read_text()), previous
            count += 1
        for qtype in ("F32", "F16"):
            output = root / f"converted-{qtype}.gguf"
            words = [*prefix, "emit", "--arch", "qwen3", "--native-source", str(source),
                     "--tensor", "model.embed_tokens.weight", "--target-qtype", qtype,
                     "--out", str(output)]
            result = invoke(binary, words, environment)
            assert result.returncode == 0 and output.exists(), result
            assert re.search(r"execution_ready\s+false", result.stdout), result
            current = output.read_bytes()
            if reference:
                previous = invoke(reference, [*words, "--overwrite"], environment)
                assert previous.returncode == 0 and current == output.read_bytes(), previous
            count += 1
            failed = invoke(binary, words, environment)
            assert failed.returncode != 0 and output.read_bytes() == current, failed
            count += 1
        for words in ([*prefix, "plan", "--arch", "qwen3"],
                      [*prefix, "emit", "--arch", "qwen3", "--native-source", str(source)],
                      [*prefix, "plan", "--arch", "qwen3", "--native-source", str(source),
                       "--out-plan", str(root / "refused.json"), "--limit", "-1"],
                      [*prefix, "emit", "--arch", "qwen3", "--native-source", str(source),
                       "--tensor", "missing", "--target-qtype", "F32", "--out", str(root / "missing.gguf")]):
            result = invoke(binary, words, environment)
            assert result.returncode != 0 and not (root / "missing.gguf").exists(), result
            count += 1
        result = invoke(binary, ["inspect", "qtype"], environment)
        assert result.returncode == 0 and "qtype-support" in result.stdout, result
        if reference:
            previous = invoke(reference, ["inspect", "qtype"], environment)
            for name, policy, storage, emit, quantize, compute in re.findall(
                    r"^\s+(\S+) policy=(\S+) storage=(\S+) emit=(\S+) quantize=(\S+) compute=(\S+)",
                    previous.stdout, re.M):
                assert re.search(rf"^\s*{name}\s+{policy}\s+{storage}\s+{emit}\s+{quantize}\s+{compute}\b",
                                 result.stdout, re.M), (name, result)
        count += 1
    return count


def materialization(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-materialization-") as temporary:
        environment = {**os.environ, "YVEX_MODELS_REGISTRY": str(Path(temporary).resolve() / "models.json"),
                       "NO_COLOR": "1", "COLUMNS": "220"}
        fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
        words = ["artifact", "materialize", "--model", str(fixture), "--backend", "cpu"]
        def fields(result):
            return {name: value.strip() for name, value in re.findall(
                r"^\s*([a-z_]+):?\s+(.+)$", result.stdout, re.M)}
        for options in ([], ["--require-all"], ["--allow-unsupported-dtype"]):
            result = invoke(binary, [*words, *options], environment)
            assert result.returncode == 0, result
            facts = fields(result)
            for key, value in {"materialization_gate": "pass", "tensors_materialized": "1",
                               "bytes_materialized": "128", "bytes_allocated": "128",
                               "bytes_transferred": "128", "backend_allocated_bytes": "128",
                               "execution_ready": "false", "resource_retirement": "pass",
                               "status": "weights-materialized"}.items():
                assert facts[key] == value, (key, result)
            if reference:
                previous = invoke(reference, [*words, *options], environment)
                assert previous.returncode == 0, previous
                previous_facts = fields(previous)
                for key in facts.keys() & previous_facts.keys():
                    assert facts[key] == previous_facts[key], (key, result, previous)
            count += 1
        for hook, phase, transferred in (("ALLOC", "allocation", "0"), ("TRANSFER", "transfer", "128")):
            result = invoke(binary, words, {**environment, f"YVEX_TEST_FAIL_MATERIALIZE_AFTER_{hook}": "1"})
            assert result.returncode != 0, result
            facts = fields(result)
            for key, value in {"materialization_gate": "fail", "materialization_phase": phase,
                               "bytes_planned": "128", "bytes_allocated": "128",
                               "bytes_transferred": transferred, "cleanup_attempted": "true",
                               "cleanup_status": "pass", "resource_retirement": "pass",
                               "backend_allocated_bytes": "0", "execution_ready": "false",
                               "status": "materialization-failed-cleaned"}.items():
                assert facts[key] == value, (key, result)
            recovery = invoke(binary, words, environment)
            assert recovery.returncode == 0 and fields(recovery)["resource_retirement"] == "pass"
            count += 2
        for path in ("bad-magic.gguf", "tensor-offset-out-of-bounds.gguf", "tensor-dim-zero.gguf"):
            result = invoke(binary, [*words[:3], str(fixture.parent / path), *words[4:]], environment)
            assert result.returncode != 0, result
            facts = fields(result)
            assert facts["materialization_gate"] == "fail" and facts["backend_status"] == "not-opened", result
            assert facts["allocation_attempted"] == "false", result
            count += 1
        result = invoke(binary, [*words[:3], str(fixture.parent / "valid-minimal.gguf"), *words[4:]], environment)
        assert result.returncode == 0 and fields(result)["status"] == "weights-partial", result
        count += 1
        for words in (["artifact", "materialize"], ["artifact", "materialize", "--model", str(fixture)],
                      ["artifact", "materialize", "--model", str(fixture), "--backend", "metal"]):
            assert invoke(binary, words, environment).returncode == 2
            count += 1
    return count


def artifact_gates(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-artifact-gates-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220",
                       "YVEX_MODELS_REGISTRY": str(root / "models.json")}
        fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
        expected = ["--expect-tensor", "token_embd.weight", "--expect-rank", "2",
                    "--expect-dims", "4,8", "--expect-dtype", "F32", "--expect-bytes", "128"]
        def fields(content):
            return {name: value.strip() for name, value in re.findall(r"^([a-z_]+): (.*)$", content, re.M)}
        for gate in ("model", "materialization"):
            words = ["artifact", "verify", gate, "--model", str(fixture), "--label", "fixture",
                     "--family", "test", *expected, "--backend", "cpu", "--require-cpu"]
            if gate == "materialization":
                words += ["--scope", "selected-tensor", "--check-cleanup", "--repeat", "2"]
            variants = ([], ["--expect-dims", "8,4"], ["--sha256", "0" * 64])
            for index, arguments in enumerate(variants):
                # Replace a scalar option rather than duplicating registry-one flags.
                current = list(words)
                for key, value in zip(arguments[::2], arguments[1::2]):
                    if key in current:
                        current[current.index(key) + 1] = value
                    else:
                        current.extend([key, value])
                destination = root / f"{gate}-{index}.txt"
                result = invoke(binary, [*current, "--report-out", str(destination)], environment)
                assert (result.returncode == 0) == (index == 0), result
                assert destination.is_file(), result
                facts = fields(destination.read_text())
                assert facts["execution_ready"] == "false", facts
                if gate == "materialization" and index == 0:
                    assert facts["repeat_count"] == "2" and facts["cleanup_verified"] == "yes", facts
                if reference:
                    previous = invoke(reference, [*current, "--report-out", str(destination)], environment)
                    assert previous.returncode == result.returncode, (result, previous)
                    previous_facts = fields(destination.read_text())
                    for key in facts.keys() & previous_facts.keys() - {"reason"}:
                        assert facts[key] == previous_facts[key], (key, facts, previous_facts)
                count += 1
            if gate == "materialization":
                result = invoke(binary, [*words, "--report-out", str(root / "failure.txt")],
                                {**environment, "YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER": "1"})
                assert result.returncode != 0, result
                facts = fields((root / "failure.txt").read_text())
                assert facts["materialization_gate"] == "fail" and facts["cleanup_status"] == "pass", facts
                assert facts["transfer_attempted"] == "true" and facts["execution_ready"] == "false", facts
                count += 1
            malformed = [expected[:2]]
            for rank, dims in (("5", "1,2,3,4,5"), ("2", "4,0"), ("2", "4"),
                               ("0", "4,8"), ("2", "4,18446744073709551616")):
                complete = list(expected)
                complete[complete.index("--expect-rank") + 1] = rank
                complete[complete.index("--expect-dims") + 1] = dims
                malformed.append(complete)
            for incomplete in malformed:
                basic = ["artifact", "verify", gate, "--model", str(fixture), "--label", "fixture", "--family", "test"]
                if gate == "materialization":
                    basic += ["--scope", "selected-tensor"]
                assert invoke(binary, [*basic, *incomplete], environment).returncode == 2
                count += 1
        result = invoke(binary, ["artifact", "verify", "materialization", "--model", str(root / "missing.gguf"),
                                  "--label", "missing", "--family", "test", "--scope", "selected-tensor",
                                  "--report-out", str(root / "missing.txt")], environment)
        assert result.returncode != 0 and fields((root / "missing.txt").read_text())["failure_class"] == "missing_file", result
        count += 1
    return count


def tensor_mapping(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-mapping-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
        template = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
        for fixture, name, shape in (("known", "embed.weight", [8, 4]),
                                     ("unknown", "unknown.weight", [8, 4]),
                                     ("mismatch", "embed.weight", [3, 4])):
            source = root / fixture
            source.mkdir()
            elements = shape[0] * shape[1]
            header = json.dumps({name: {"dtype": "F16", "shape": shape,
                                        "data_offsets": [0, elements * 2]}}).encode()
            (source / "weights.safetensors").write_bytes(struct.pack("<Q", len(header)) + header + bytes(elements * 2))
            base = ["compile", "tensor", "map", "--arch", "deepseek4", "--native-source", str(source)]
            for extra in ([], ["--template", str(template)], ["--limit", "0"],
                          ["--tensor", "absent"], ["--require-all-native-mapped"],
                          ["--template", str(template), "--require-all-template-matched"]):
                words = [*base, *extra, "--json"]
                result = invoke(binary, words, environment)
                if reference:
                    previous = invoke(reference, words, environment)
                    assert result.returncode == previous.returncode, (result, previous)
                    if result.returncode == 0:
                        assert json.loads(result.stdout) == json.loads(previous.stdout), (result, previous)
                if result.returncode == 0:
                    facts = json.loads(result.stdout)
                    assert facts["schema"] == "yvex.tensor_map.v1" and facts["native_tensors"] == 1, facts
                    assert facts["mapped"] + facts["unmapped"] + facts["shape_mismatch"] == 1, facts
                else:
                    assert not result.stdout and "YVEX_ERR_" in result.stderr, result
                count += 1
            if fixture == "known":
                for width in (32, 80, 180):
                    result = invoke(binary, [*base, "--template", str(template), "--tensor", name],
                                    {**environment, "COLUMNS": str(width)})
                    assert result.returncode == 0, result
                    reflowed = "".join(result.stdout.split())
                    assert name in reflowed and "token_embd.weight" in reflowed, result
                    assert "transpose" in result.stdout and "[4,8]" in result.stdout, result
                    count += 1
                absent = invoke(binary, [*base, "--tensor", "absent"], environment)
                assert absent.returncode == 4 and "YVEX_ERR_FORMAT" in absent.stderr, absent
                assert invoke(binary, [*base, "--limit", "-1"], environment).returncode == 2
                count += 2
        for extra in ([], ["--arch", "deepseek4"],
                      ["--arch", "deepseek4", "--native-source", str(root / "absent")]):
            result = invoke(binary, ["compile", "tensor", "map", *extra], environment)
            assert result.returncode != 0 and not result.stdout, result
            count += 1
    return count


def quant_documents(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-quant-documents-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220"}
        template = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
        policy = root / "policy.json"
        policy.write_text(json.dumps({"schema": "yvex.quant_policy.v1", "name": "test-policy",
                                      "architecture": "deepseek4", "rules": [
            {"selector_kind": "role", "selector": "token_embedding", "qtype": "Q8_0", "requires_imatrix": False},
            {"selector_kind": "pattern", "selector": "blk.*.ffn.experts.*", "qtype": "Q2_K", "requires_imatrix": True}]}))
        def compare(words, output=None):
            result = invoke(binary, words, environment)
            document = output.read_bytes() if result.returncode == 0 and output else None
            if reference:
                previous = invoke(reference, words, environment)
                assert result.returncode == previous.returncode, (result, previous)
                if document is not None:
                    assert document == output.read_bytes(), (result, previous)
            return result
        for action in ("inspect", "validate"):
            result = compare(["compile", "quant", "policy", action, "--policy", str(policy)])
            assert result.returncode == 0 and "QUANT POLICY" in result.stdout, result
            assert re.search(r"requires_imatrix\s+1$", result.stdout, re.M), result
            if action == "inspect":
                assert "role:token_embedding" in result.stdout and "Q8_0" in result.stdout, result
            count += 1
        derived = root / "derived.json"
        result = compare(["compile", "quant", "policy", "derive", "--template", str(template),
                          "--arch", "llama", "--out", str(derived)], derived)
        assert result.returncode == 0 and "quant-policy-written" in result.stdout, result
        assert compare(["compile", "quant", "policy", "validate", "--policy", str(derived),
                        "--template", str(template)]).returncode == 0
        count += 2
        for bad in ({"schema": "wrong", "rules": []}, {"schema": "yvex.quant_policy.v1", "rules": "bad"}):
            broken = root / "broken-policy.json"
            broken.write_text(json.dumps(bad))
            result = compare(["compile", "quant", "policy", "validate", "--policy", str(broken)])
            assert result.returncode != 0 and not result.stdout, result
            count += 1
        listing = compare(["compile", "quant", "preset", "list"])
        assert listing.returncode == 0 and listing.stdout.strip(), listing
        names = listing.stdout.splitlines()
        if reference:
            assert names == invoke(reference, ["compile", "quant", "preset", "list"], environment).stdout.splitlines()
        count += 1
        for name in names:
            shown = compare(["compile", "quant", "preset", "show", name])
            assert shown.returncode == 0 and re.search(r"identity\s+[0-9a-f]{64}$", shown.stdout, re.M), shown
            exported = root / "preset.json"
            result = compare(["compile", "quant", "preset", "export", name, "--out", str(exported)], exported)
            assert result.returncode == 0, result
            count += 2
        for words in (["compile", "quant", "preset", "show", "not-a-preset"],
                      ["compile", "quant", "policy", "inspect", "--policy", str(root / "absent")]):
            result = compare(words)
            assert result.returncode != 0 and not result.stdout, result
            count += 1
        calibration = root / "calibration.dat"
        calibration.write_bytes(b"fixture-declaration-only")
        manifest = root / "imatrix.json"
        base = ["compile", "quant", "imatrix", "create", "--name", "test-imatrix", "--arch", "deepseek4",
                "--imatrix", str(calibration), "--format", "routed_moe_dat", "--status", "present",
                "--dataset", "fixture", "--producer", "test", "--command", "not executed", "--out", str(manifest)]
        result = compare(base, manifest)
        assert result.returncode == 0 and "imatrix-manifest-written" in result.stdout, result
        count += 1
        for action in ("inspect", "validate"):
            result = compare(["compile", "quant", "imatrix", action, "--manifest", str(manifest)])
            assert result.returncode == 0 and re.search(r"file_exists\s+yes$", result.stdout, re.M), result
            count += 1
        for key, value in (("--format", "not-a-format"), ("--status", "not-a-status")):
            words = list(base)
            words[words.index(key) + 1] = value
            original = manifest.read_bytes()
            result = compare(words)
            assert result.returncode != 0 and manifest.read_bytes() == original and not result.stdout, result
            count += 1
        native = root / "native"
        native.mkdir()
        tool = root / "tool"
        sentinel = root / "must-not-run"
        tool.write_text(f"#!/bin/sh\ntouch '{sentinel}'\n")
        tool.chmod(0o700)
        job = root / "job.json"
        base = ["compile", "quant", "job", "create", "--name", "test-job", "--arch", "deepseek4",
                "--tool", "external", "--tool-path", str(tool), "--native-source", str(native),
                "--template", str(template), "--out-gguf", str(root / "out.gguf"), "--log", str(root / "job.log"),
                "--status", "ready", "--command", "not executed", "--out", str(job), "--imatrix", str(calibration)]
        result = compare(base, job)
        assert result.returncode == 0 and not sentinel.exists(), result
        for field in ("tool_exists", "source_exists", "template_exists", "imatrix_exists"):
            assert re.search(rf"{field}\s+yes$", result.stdout, re.M), result
        count += 1
        for action in ("inspect", "validate"):
            result = compare(["compile", "quant", "job", action, "--manifest", str(job)])
            assert result.returncode == 0 and f"quant-job-{'manifest' if action == 'inspect' else 'valid'}" in result.stdout, result
            count += 1
        succeeded = json.loads(job.read_text())
        succeeded["status"] = "succeeded"
        job.write_text(json.dumps(succeeded))
        result = compare(["compile", "quant", "job", "validate", "--manifest", str(job)])
        assert result.returncode != 0 and not result.stdout and not sentinel.exists(), result
        count += 1
        for words in (["compile", "quant", "imatrix", "create"], ["compile", "quant", "job", "create"],
                      ["compile", "quant", "policy", "derive"], ["compile", "quant", "preset", "show"],
                      ["compile", "quant", "preset", "export", names[0]]):
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
    return count


def physical_variants(binary: Path, reference: Path | None) -> int:
    """Native source/plan admission refuses before publication; no model fixtures fabricated."""
    with tempfile.TemporaryDirectory(prefix="yvex-rust-variant-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "180"}
        base = ["--target", "minimax-h3-fl2va", "--source", "/does/not/exist"]
        controls = [
            (["compile", "quant", "plan", *base, "--component", "audio_vae",
              "--out-plan", str(directory / "refused.plan")], 1,
             "immutable source acquisition admission failed"),
            (["compile", "quant", "plan", *base, "--component", "pipeline",
              "--out-plan", str(directory / "refused.plan")], 1,
             "component must be text_encoder, transformer, video_vae, or audio_vae"),
            (["compile", "quant", "plan", *base, "--component", "audio_vae",
              "--preset", "minimax-h3-transformer-q8_0-v1",
              "--out-plan", str(directory / "refused.plan")], 1,
             "component alternate profile targets another component"),
            (["compile", "quant", "plan", "--target", "unknown-physical-target",
              "--source", "/does/not/exist", "--component", "transformer",
              "--out-plan", str(directory / "refused.plan")], 1,
             "target has no physical-variant compiler adapter"),
        ]
        for path in (["compile", "quant", "emit"], ["compile", "quant", "probe"],
                     ["inspect", "quant", "summary"], ["inspect", "quant", "decision"]):
            words = [*path, *base, "--component", "audio_vae", "--plan", str(directory / "missing.plan")]
            if path[-1] == "emit":
                words += ["--out", str(directory / "refused.gguf")]
            elif path[-1] in ("probe", "decision"):
                words += ["--tensor", "missing.tensor"]
            controls.append((words, 1, "immutable source acquisition admission failed"))
        for words, expected, reason in controls:
            current = invoke(binary, words, environment)
            assert current.returncode == expected and not current.stdout, (words, current)
            assert reason in current.stderr, (words, current)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == expected and reason in previous.stderr, (words, previous)
        grammar = [
            ["compile", "quant", "plan", *base],
            ["compile", "quant", "plan", *base, "--component", "audio_vae",
             "--backend", "unknown", "--out-plan", str(directory / "refused.plan")],
            ["compile", "quant", "probe", *base, "--component", "audio_vae",
             "--plan", "missing", "--role", "token_embedding"],
            ["inspect", "quant", "decision", *base, "--component", "audio_vae",
             "--plan", "missing", "--tensor", "one", "--role", "two"],
        ]
        for words in grammar:
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, (words, result)
        assert not list(directory.iterdir()), "refused native work published a plan/artifact"
        return len(controls) + len(grammar)


def provider_catalog(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-discovery-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        registry = directory / "empty.json"
        registry.write_text(json.dumps({"schema": "yvex.models.local.v8", "models": [],
                                        "working_set": [], "publications": []}))
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_MODELS_REGISTRY": str(registry),
                       "YVEX_HF_CLI": str(ROOT / "tests/fixtures/bin/fake-hf"),
                       "YVEX_FAKE_HF_LOG": str(directory / "hf.log"),
                       "YVEX_FAKE_HF_STATE": str(directory / "auth"),
                       "NO_COLOR": "1", "COLUMNS": "240"}
        for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN"):
            environment.pop(name, None)
        common = ["--models-root", str(models), "--json"]
        controls = [
            ["model", "search", "MiniMax H3", *common],
            ["model", "search", "MiniMax H3", "--all", *common],
            ["model", "search", "MiniMax H3", "--provider", "hf", "--page", "2", "--limit", "2", *common],
            ["model", "search", "MiniMax H3", "--author", "MiniMaxAI", "--filter", "safetensors", *common],
            ["source", "inspect", "MiniMaxAI/MiniMax-H3", *common],
            ["source", "inspect", "unsloth/MiniMax-H3-GGUF", "--revision", "main", *common],
            ["model", "search", "unknown", "--provider", "local", *common],
        ]
        for words in controls:
            result = invoke(binary, words, environment)
            assert result.returncode == 0, (words, result)
            data = json.loads(result.stdout)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and data == json.loads(previous.stdout), (words, data, previous)
            count += 1
        first = json.loads(invoke(binary, controls[1], environment).stdout)
        assert first["models"][0]["kind"] == "full model"
        assert first["models"][0]["engine_state"] == "not-observed"
        assert all(not row["local_source"] and not row["local_package"] for row in first["models"])
        for mode, words, expected in [
            ("model-not-found", ["source", "inspect", "missing/model"], 1),
            ("revision-not-found", ["source", "inspect", "MiniMaxAI/MiniMax-H3", "--revision", "missing"], 1),
            ("unsafe-file", ["source", "inspect", "MiniMaxAI/MiniMax-H3"], 4),
            ("malformed", ["model", "search", "malformed"], 4),
            ("empty", ["model", "search", "none"], 0),
        ]:
            env = {**environment, "YVEX_FAKE_HF_DISCOVERY_MODE": mode}
            result = invoke(binary, [*words, *common], env)
            assert result.returncode == expected, (mode, result)
            if reference:
                previous = invoke(reference, [*words, *common], env)
                assert previous.returncode == expected, (mode, previous)
                if expected == 0:
                    assert json.loads(result.stdout) == json.loads(previous.stdout)
            count += 1
        for width in (32, 80, 180):
            env = {**environment, "COLUMNS": str(width)}
            for words in (["model", "search", "MiniMax H3"],
                          ["source", "inspect", "MiniMaxAI/MiniMax-H3", "--audit"]):
                result = invoke(binary, [*words, "--models-root", str(models)], env)
                assert result.returncode == 0 and "MiniMax" in result.stdout, (words, result)
                assert "\x1b" not in result.stdout
                count += 1
        grammar = [
            ["model", "search", "MiniMax", "--limit", "0"],
            ["model", "search", "MiniMax", "--page", "21"],
            ["model", "search", "MiniMax", "--all", "--limit", "1"],
            ["model", "search", "MiniMax", "--provider", "local", "--filter", "gguf"],
            ["source", "inspect", "MiniMaxAI/MiniMax-H3", "--provider", "local"],
            ["source", "inspect", "MiniMaxAI/MiniMax-H3", "--output", "nope"],
            ["source", "inspect", "MiniMaxAI/MiniMax-H3", "--cli", "not-supported"],
        ]
        for words in grammar:
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, (words, result)
            count += 1
        assert not models.exists() or not list(models.iterdir()), "discovery acquired payload"
        assert "  download\n" not in (directory / "hf.log").read_text()
    return count


def model_distribution(binary: Path, reference: Path | None) -> int:
    """Storage/export use native receipts; unique or external bytes are never evicted."""
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-distribution-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        registry = directory / "registry.json"
        artifact = directory / "model.gguf"
        alias = "deepseek4-v4-flash-dspark-selected-embed"
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"),
                       "YVEX_MODELS_REGISTRY": str(registry), "HF_HOME": str(directory / "hf"),
                       "HF_HUB_CACHE": str(directory / "hf/hub"), "HF_XET_CACHE": str(directory / "hf/xet"),
                       "XDG_CACHE_HOME": str(directory / "cache"), "NO_COLOR": "1", "COLUMNS": "240"}
        emit = invoke(binary, ["compile", "artifact", "emit", "--out", str(artifact),
                               "--model-name", "distribution-controlled", "--arch", "deepseek",
                               "--target-qtype", "F16"], environment)
        assert emit.returncode == 0, emit
        created = invoke(binary, ["profile", "create", "--path", str(artifact), "--alias", alias,
                                  "--registry", str(registry),
                                  "--support-level", "selected-tensor-materialized"], environment)
        assert created.returncode == 0, created
        original = artifact.read_bytes()
        digest = hashlib.sha256(original).hexdigest()
        cache = models / "cache/hf"
        cache.mkdir(parents=True)
        os.link(artifact, cache / "shared.gguf")
        (directory / "hf").mkdir()
        (directory / "hf/token").write_text("fixture-auth-not-storage")
        common = ["--models-root", str(models), "--registry", str(registry)]
        for extra in ([], ["--include-caches"]):
            words = ["model", "storage", alias, *common, *extra, "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0, result
            data = json.loads(result.stdout)
            assert data["schema"] == "yvex.model.storage.v1"
            assert data["reflink_shared_extents"] is None and data["historical_peak_bytes"] is None
            assert data["rows"][0]["logical_bytes"] == len(original)
            assert data["rows"][0]["attributable_to_model"] is True
            if extra:
                shared = next(row for row in data["rows"] if row["path"] == str(cache))
                assert shared["shared_files"] == 1 and shared["attributable_to_model"] is False
                assert all(row["path"] != str(directory / "hf") for row in data["rows"])
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and data == json.loads(previous.stdout), (data, previous)
            count += 1
        for width in (32, 80, 180):
            result = invoke(binary, ["model", "storage", alias, *common, "--include-caches"],
                            {**environment, "COLUMNS": str(width)})
            assert result.returncode == 0 and "unknown" in result.stdout and "cache" in result.stdout, result
            count += 1
        for uri in (False, True):
            destination = directory / "export.gguf"
            path = f"file://{destination}" if uri else str(destination)
            words = ["model", "push", alias, path, *common, "--variant", digest, "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0 and destination.read_bytes() == original, result
            data = json.loads(result.stdout)
            assert data["schema"] == "yvex.model.push.v1" and data["representation_identity"] == digest
            assert data["bytes"] == len(original) and data["destination"] == str(destination)
            destination.unlink()
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and data == json.loads(previous.stdout), (data, previous)
                assert destination.read_bytes() == original
                destination.unlink()
            destination.write_bytes(b"existing recipient bytes")
            refused = invoke(binary, words, environment)
            assert refused.returncode != 0 and not refused.stdout, refused
            assert destination.read_bytes() == b"existing recipient bytes"
            destination.unlink()
            count += 2
        for scheme in ("hf", "ssh", "oci"):
            words = ["model", "push", alias, f"{scheme}://fixture/output", *common]
            result = invoke(binary, words, environment)
            assert result.returncode == 3 and "unavailable" in result.stderr, result
            if reference:
                assert invoke(reference, words, environment).returncode == 3
            count += 1
        for extra in ([], ["--dry-run"], ["--variant", digest]):
            words = ["model", "evict", alias, *common, *extra, "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 1 and not result.stdout and "retained" in result.stderr, result
            if reference:
                assert invoke(reference, words, environment).returncode == 1
            assert artifact.read_bytes() == original
            count += 1
        for words in (["model", "storage", "missing", *common, "--json"],
                      ["model", "evict", alias, *common, "--variant", "unknown", "--json"],
                      ["model", "evict", alias, *common, "--representation", "source", "--json"],
                      ["model", "push", alias, str(directory / "refused"), *common,
                       "--variant", "unknown", "--json"]):
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, (words, result)
            if reference:
                assert invoke(reference, words, environment).returncode == 2, words
            assert artifact.read_bytes() == original and not (directory / "refused").exists()
            count += 1
        changed = bytearray(original)
        changed[-1] ^= 1
        artifact.write_bytes(changed)
        result = invoke(binary, ["model", "push", alias, str(directory / "refused"),
                                 *common, "--json"], environment)
        assert result.returncode != 0 and not result.stdout and not (directory / "refused").exists(), result
        assert artifact.read_bytes() == changed
        count += 1
    return count


def local_acquisition(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-acquisition-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        input_file = directory / "tiny-unqualified.gguf"
        previous_models = directory / "previous-models"
        previous_models.mkdir()
        input_file.write_bytes(b"tiny distribution bytes; not an admitted model")
        registry = directory / "empty.json"
        registry.write_text(json.dumps({"schema": "yvex.models.local.v8", "models": [],
                                        "working_set": [], "publications": []}))
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"),
                       "YVEX_MODELS_REGISTRY": str(registry), "NO_COLOR": "1", "COLUMNS": "240"}
        original = input_file.read_bytes()
        digest = hashlib.sha256(original).hexdigest()
        for uri, storage, dry in [(False, "reference", True), (True, "reference", False),
                                  (False, "managed", True), (False, "managed", False)]:
            source = f"file://{input_file}" if uri else str(input_file)
            words = ["model", "pull", source, f"--{storage}", "--models-root", str(models),
                     "--name", f"tiny-{storage}", "--family", "qwen", "--json"]
            if dry:
                words.append("--dry-run")
            result = invoke(binary, words, environment)
            assert result.returncode == 0, (words, result)
            data = json.loads(result.stdout)
            assert data["schema"] == "yvex.model.pull.v1" and data["digest"] == digest
            assert data["size_bytes"] == len(original) and data["format"] == "gguf"
            if dry:
                assert data["dry_run"] is True
            else:
                assert data["storage"] == ("external" if storage == "reference" else "managed")
                assert Path(data["location"]).read_bytes() == original
            if reference:
                reference_words = [str(previous_models) if word == str(models) else word for word in words]
                previous = invoke(reference, reference_words, environment)
                projection = json.loads(previous.stdout.replace(str(previous_models), str(models)))
                assert previous.returncode == 0 and data == projection, (data, previous)
            assert input_file.read_bytes() == original
            count += 1
        # Source-only entries outside the working set must remain visible in
        # the ordinary catalog, just as in the canonical machine projection.
        listing = ["model", "list", "--models-root", str(models)]
        catalog = json.loads(invoke(binary, [*listing, "--json"], environment).stdout)["models"]
        assert any(not model["profiles"] and not model["working_set"] for model in catalog), catalog
        for width in (40, 80, 180):
            env = {**environment, "COLUMNS": str(width), "TERM": "xterm-256color"}
            plain = invoke(binary, listing, env)
            assert plain.returncode == 0, plain
            for model in catalog:
                assert model["selector"] in plain.stdout, (model, plain.stdout)
            styled_env = dict(env)
            styled_env.pop("NO_COLOR", None)
            styled = terminal_output(binary, listing, styled_env)
            assert "\x1b[" in styled, styled
            assert re.sub(r"\x1b\[[0-9;]*m", "", styled) == plain.stdout
            for disabled in ({"NO_COLOR": ""}, {"TERM": "dumb"}):
                assert terminal_output(binary, listing, {**styled_env, **disabled}) == plain.stdout
            machine = terminal_output(binary, [*listing, "--json"], styled_env)
            assert "\x1b" not in machine and json.loads(machine)["models"] == catalog
            count += 5
        for words, expected in [
            ([str(input_file)], 2),
            ([str(input_file), "--managed", "--resume"], 3),
            ([str(input_file), "--reference", "--format", "safetensors"], 2),
            ([str(input_file), "--reference", "--variant", "wrong"], 2),
            ([str(directory / "missing.gguf"), "--managed"], 3),
            (["ssh://fixture/model"], 3), (["oci://fixture/model"], 3),
            (["yvex://fixture/model"], 3),
            (["ssh://fixture/model", "--stream", "--prepare"], 3),
        ]:
            arguments = ["model", "pull", *words, "--models-root", str(models)]
            result = invoke(binary, arguments, environment)
            assert result.returncode == expected and not result.stdout, (arguments, result)
            if reference:
                previous = invoke(reference, arguments, environment)
                assert previous.returncode == expected, (arguments, previous)
            count += 1
        prepared = invoke(binary, ["model", "pull", str(input_file), "--managed", "--prepare",
                                   "--dry-run", "--models-root", str(models)], environment)
        assert prepared.returncode == 0 and "no source or build state changed" in prepared.stdout, prepared
        count += 1
        destination = directory / "export.gguf"
        result = invoke(binary, ["model", "push", "tiny-reference", str(destination),
                                 "--representation", "source", "--models-root", str(models), "--json"], environment)
        assert result.returncode == 0 and destination.read_bytes() == original, result
        count += 1
        before = sorted(str(path.relative_to(models)) for path in models.rglob("*"))
        result = invoke(binary, ["model", "pull", str(input_file), "--reference", "--name", "other",
                                 "--family", ".", "--models-root", str(models), "--json"], environment)
        assert result.returncode != 0 and not result.stdout, result
        assert before == sorted(str(path.relative_to(models)) for path in models.rglob("*"))
        assert input_file.read_bytes() == original
        count += 1
    return count


def attention_operations(binary: Path, reference: Path | None) -> int:
    """Qualify every attention leaf without opening a real model or accelerator."""
    count = 0
    registry = json.loads((ROOT / "config/operator/registry.json").read_text())
    operations = [operation for operation in registry["operations"]
                  if operation["operation_id"].startswith("execute.graph.attention.")]
    with tempfile.TemporaryDirectory(prefix="yvex-rust-attention-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "NO_COLOR": "1"}
        missing = str(directory / "missing.yvex-runtime-binding")
        for operation in operations:
            action = operation["operation_id"].removeprefix("execute.graph.attention.")
            path = operation["command_path"]
            if action == "benchmark.compare":
                words = [*path, "--baseline", str(directory / "missing-baseline"),
                         "--current", str(directory / "missing-current"), "--output", "json"]
            elif action == "prepare":
                words = [*path, "--target", "unknown-attention-fixture", "--models-root", str(models),
                         "--output", "json"]
            else:
                words = [*path, "--target", "deepseek4-v4-flash-dspark", "--models-root", str(models),
                         "--runtime-binding", missing, "--output", "json", "--progress", "off"]
                if action != "compare":
                    words.extend(["--backend", "cuda" if action.startswith("cuda_graph.")
                                  or action in {"capture", "replay"} else "cpu"])
            result = invoke(binary, words, environment)
            assert result.returncode != 0, (words, result)
            assert "Rust migration" not in result.stdout + result.stderr, (words, result)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == result.returncode, (words, result, previous)
                if previous.stdout:
                    current = json.loads(result.stdout)
                    original = json.loads(previous.stdout)
                    # Preflight is shell policy, not native computation. Its owner
                    # and wording intentionally move; the error code stays exact.
                    if action == "benchmark.compare":
                        for value in (current, original):
                            value.pop("failure_where", None)
                            value.pop("reason", None)
                    assert current == original, (words, current, original)
                else:
                    assert not result.stdout, (words, result)
            elif result.stdout:
                projected = json.loads(result.stdout)
                assert projected["command"] == " ".join(path)
                assert projected["status"] == "refused"
                assert projected["runtime_generation_ready"] is False
            count += 1
        base = ["bench", "attention", "execute", "--target", "deepseek4-v4-flash-dspark",
                "--models-root", str(models), "--backend", "cpu", "--output", "json"]
        malformed = [
            ["--probe", "fixture"], ["--scope", "reduced"], ["--phase", "generation"],
            ["--mode", "fallback"], ["--operation-scope", "transformer"],
            ["--operation-scope", "release-attention-set"], ["--trace-level", "everything"],
            ["--input-file", str(directory / "input")], ["--input", "tensor-file"],
            ["--layer-start", "0", "--layer-count", "2"], ["--local-capacity", "0"],
            ["--tokens", "0"], ["--tokens", "18446744073709551616"], ["--warmup", "-1"],
            ["--layer", "0", "--layer-start", "0", "--layer-count", "1"],
            ["--layer-start", "0"], ["--class", "csa"], ["--max-device-bytes", "1"],
            ["--baseline", str(directory / "baseline")], ["--current", str(directory / "current")],
            ["--chunk-tokens", "1"], ["--capture-bucket", "test"],
        ]
        for suffix in malformed:
            words = [*base, *suffix]
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, (words, result)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 2 and not previous.stdout, (words, previous)
            count += 1
        stale = directory / "stale.yvex-runtime-binding"
        stale.write_bytes(b"stale-runtime-binding\n")
        for output in ("json", "csv"):
            words = [*base[:-2], "--runtime-binding", str(stale), "--output", output]
            result = invoke(binary, words, environment)
            assert result.returncode == 4, (words, result)
            if output == "json":
                projected = json.loads(result.stdout)
                assert projected["failure_code"] == "YVEX_ERR_BOUNDS"
                assert projected["status"] == "refused"
            else:
                import csv
                import io
                rows = list(csv.reader(io.StringIO(result.stdout)))
                assert rows[0] == ["field", "value"] and len(rows) > 20
            if reference:
                previous = invoke(reference, words, environment)
                assert result.returncode == previous.returncode
                assert (json.loads(result.stdout) == json.loads(previous.stdout) if output == "json"
                        else result.stdout == previous.stdout), (result, previous)
            count += 1
        ambiguous = directory / "bindings"
        ambiguous.mkdir()
        for name in ("one", "two"):
            (ambiguous / f"{name}.yvex-runtime-binding").write_bytes(b"stale")
        for selected, expected in [(ambiguous, 1), (directory / "linked-bindings", 3)]:
            if selected != ambiguous:
                selected.symlink_to(ambiguous, target_is_directory=True)
            words = ["inspect", "attention", "describe", "--target", "deepseek4-v4-flash-dspark",
                     "--models-root", str(models), "--runtime-binding-dir", str(selected), "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == expected and not result.stdout, (words, result)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == expected and not previous.stdout, (words, previous)
            count += 1
        for width in (40, 80, 180):
            result = invoke(binary, [*base[:-2], "--runtime-binding", missing],
                            {**environment, "COLUMNS": str(width)})
            assert result.returncode == 3 and "ATTENTION" in result.stdout
            assert "runtime binding" in result.stdout and len(result.stdout.splitlines()) <= 20, result
            count += 1
        assert not list(models.rglob("*")), "refusal must not create source/model state"
    return count


def benchmark_publication(binary: Path, reference: Path | None, fixture: Path) -> int:
    """Real native storage/compatibility mechanics over explicit synthetic timing records."""
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-benchmark-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1"}
        for name, mode in [("baseline", "same"), ("current", "same"), ("regressed", "regressed")]:
            result = subprocess.run([str(fixture), str(directory / name), mode],
                                    capture_output=True, text=True, timeout=15)
            assert result.returncode == 0, result
        original = {name: (directory / name).read_bytes() for name in ("baseline", "current", "regressed")}
        for name, threshold, expected in [("current", None, 0), ("current", "0", 0),
                                           ("regressed", None, 0), ("regressed", "0", 1)]:
            words = ["bench", "attention", "benchmark", "compare", "--baseline", str(directory / "baseline"),
                     "--current", str(directory / name), "--output", "json"]
            if threshold is not None:
                words.extend(["--max-regression-bps", threshold])
            result = invoke(binary, words, environment)
            assert result.returncode == expected, (words, result)
            data = json.loads(result.stdout)
            assert data["benchmark_baseline_compatible"] and data["benchmark_comparison_identity"]
            assert data["benchmark_regression_policy_enabled"] == (threshold is not None)
            assert data["runtime_generation_ready"] is False
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == expected and data == json.loads(previous.stdout), (data, previous)
            count += 1
        chart = directory / "comparison.svg"
        words = ["bench", "attention", "benchmark", "compare", "--baseline", str(directory / "baseline"),
                 "--current", str(directory / "current"), "--chart", str(chart), "--output", "json"]
        result = invoke(binary, words, environment)
        data = json.loads(result.stdout)
        assert result.returncode == 0 and data["benchmark_chart_generated"] and chart.is_file(), result
        assert b"&lt;&amp;" in chart.read_bytes() and data["benchmark_chart_identity"]
        if reference:
            # Retire only this owned test output before a second no-clobber producer.
            chart.unlink()
            previous = invoke(reference, words, environment)
            assert previous.returncode == 0 and data == json.loads(previous.stdout), (data, previous)
        existing = chart.read_bytes()
        count += 1
        result = invoke(binary, words, environment)
        assert result.returncode == 1 and chart.read_bytes() == existing, result
        assert json.loads(result.stdout)["failure_code"] == "YVEX_ERR_STATE"
        count += 1
        for name, contents in original.items():
            assert (directory / name).read_bytes() == contents
        corrupt = directory / "corrupt"
        corrupt.write_bytes(b"not a benchmark record")
        result = invoke(binary, ["bench", "attention", "benchmark", "compare",
                                 "--baseline", str(directory / "baseline"), "--current", str(corrupt),
                                 "--output", "json"], environment)
        assert result.returncode != 0 and json.loads(result.stdout)["status"] == "refused", result
        count += 1
    return count


def graph_pipeline_operations(binary: Path, reference: Path | None) -> int:
    """Native computational refusals must not become a legacy dispatcher or fixture success."""
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-pipeline-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "NO_COLOR": "1"}
        artifact = directory / "missing.gguf"
        binding = directory / "binding"
        input_file = directory / "input"
        common = ["--target", "deepseek4-v4-flash-dspark", "--artifact", str(artifact),
                  "--runtime-binding", str(binding), "--backend", "cpu", "--input-file", str(input_file)]
        for action in ["moe", "execute", "decode", "logits", "sample"]:
            path = ["bench", "moe"] if action == "moe" else ["bench", "transformer", action]
            required = (["--input", "tensor-file"] if action == "moe" else
                        ["--context-capacity", "8", "--chunk-tokens", "2"] if action == "execute" else
                        ["--context-capacity", "8", "--prefill-tokens", "2", "--prefill-chunk-tokens", "2"])
            for output in ["json", "csv"]:
                for artifact_exists in [False, True]:
                    if artifact_exists:
                        artifact.write_bytes(b"not an admitted model artifact")
                        binding.write_bytes(b"obsolete runtime binding")
                        input_file.write_bytes(b"invalid numerical input")
                    words = [*path, *common, *required, "--output", output]
                    result = invoke(binary, words, environment)
                    assert result.returncode != 0 and "Rust migration" not in result.stdout + result.stderr, (words, result)
                    assert "\x1b" not in result.stdout, result
                    if reference:
                        previous = invoke(reference, words, environment)
                        assert result.returncode == previous.returncode, (words, result, previous)
                        assert (json.loads(result.stdout) == json.loads(previous.stdout) if output == "json"
                                else result.stdout == previous.stdout), (words, result, previous)
                    if artifact_exists:
                        assert artifact.read_bytes() == b"not an admitted model artifact"
                        artifact.unlink()
                    count += 1
            for additional in [["--backend", "unknown"], ["--progress", "plain"], ["--input", "unknown"],
                               ["--max-host-bytes", "0"], ["--max-device-bytes", "1"],
                               ["--max-host-bytes", "18446744073709551616"]]:
                result = invoke(binary, [*path, *common, *required, *additional], environment)
                assert result.returncode == 2 and not result.stdout, (action, additional, result)
                count += 1
            result = invoke(binary, path, environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
            if action != "moe":
                for additional in [["--phase", "decode"], ["--text", "no prompt dispatch"], ["--context-capacity", "0"],
                                   ["--temperature", "nan"], ["--strategy", "stochastic"], ["--seed", "0"],
                                   ["--top-p", "0.5"], ["--generation-mode", "dspark"]]:
                    result = invoke(binary, [*path, *common, *required, *additional], environment)
                    assert result.returncode == 2 and not result.stdout, (action, additional, result)
                    count += 1
            for width in [40, 80, 180]:
                result = invoke(binary, [*path, *common, *required], {**environment, "COLUMNS": str(width)})
                assert result.returncode != 0 and "refused" in result.stdout and "reason" in result.stdout, result
                assert "kernel_launches" not in result.stdout and "\x1b" not in result.stdout, result
                count += 1
        words = ["bench", "transformer", "sample", *common, "--context-capacity", "8", "--prefill-tokens", "2",
                 "--prefill-chunk-tokens", "2", "--strategy", "stochastic", "--seed", "0", "--top-p", "0.1", "--output", "csv"]
        result = invoke(binary, words, environment)
        assert result.returncode != 0 and result.stdout, result
        if reference:
            previous = invoke(reference, words, environment)
            assert result.returncode == previous.returncode and result.stdout == previous.stdout, (result, previous)
        count += 1
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def generation_operations(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-generation-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "NO_COLOR": "1"}
        artifact = directory / "missing.gguf"
        binding = directory / "binding"
        common = ["bench", "transformer", "generate", "--target", "deepseek4-v4-flash-dspark",
                  "--artifact", str(artifact), "--runtime-binding", str(binding), "--backend", "cpu",
                  "--context-capacity", "8", "--prefill-chunk-tokens", "1", "--max-new-tokens", "3"]
        for prompt in [["--text", "a"], ["--user", "synthetic"],
                       ["--system", "synthetic", "--user", "synthetic"]]:
            for mode in ["target-only", "dspark"]:
                words = [*common, *prompt, "--generation-mode", mode, "--output", "json"]
                result = invoke(binary, words, environment)
                assert result.returncode != 0, result
                projected = json.loads(result.stdout)
                assert projected["status"] == "refused" and not projected["generation_ready"], result
                assert projected["generated_tokens"] == [] and projected["model_committed_tokens"] == 0
                if reference:
                    previous = invoke(reference, words, environment)
                    assert result.returncode == previous.returncode, (result, previous)
                    assert projected == json.loads(previous.stdout), (result, previous)
                count += 1
        for prompt, extra in [([], []), (["--system", "a"], []),
                              (["--text", "a", "--user", "b"], []),
                              (["--text", "a", "--system", "b"], [])] + [
                (["--text", "a"], flags) for flags in [
                    ["--context-capacity", "0"], ["--max-new-tokens", "0"],
                    ["--prefill-chunk-tokens", "0"], ["--phase", "decode"],
                    ["--progress", "plain"], ["--generation-mode", "unknown"],
                    ["--max-output-bytes", "0"], ["--max-host-bytes", "18446744073709551616"],
                    ["--max-device-bytes", "1"], ["--strategy", "stochastic"],
                    ["--temperature", "nan"], ["--seed", "0"], ["--top-p", "0.5"]]]:
            result = invoke(binary, [*common, *prompt, *extra], environment)
            assert result.returncode == 2 and not result.stdout, (prompt, extra, result)
            count += 1
        for width in [40, 80, 180]:
            result = invoke(binary, [*common, "--text", "a"], {**environment, "COLUMNS": str(width)})
            assert result.returncode != 0 and "refused" in result.stdout and "reason" in result.stdout, result
            count += 1
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def computational_semantics(value):
    """Only observation timing varies; computational identities and counters must agree."""
    if isinstance(value, dict):
        if "phases_ns" in value and "counters" in value:
            return {key: computational_semantics(child) for key, child in value.items()
                    if key not in {"identity", "phases_ns"}}
        return {key: computational_semantics(child) for key, child in value.items()
                if not key.endswith("_seconds") and key != "effective_committed_tokens_per_second"}
    if isinstance(value, list):
        return [computational_semantics(child) for child in value]
    return value


def native_pipeline_capacity(environment: dict[str, str]) -> dict[str, str]:
    """Declare fixture capacity only on explicit hosted opt-in, never host admission."""
    result = environment.copy()
    if result.get("YVEX_TEST_FIXTURE_CAPACITY") != "1":
        return result
    variables = ("YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES",
                 "YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES",
                 "YVEX_TEST_RUNTIME_CGROUP_AVAILABLE_MEMORY_BYTES")
    assert not any(name in result for name in variables), (
        "native pipeline fixture cannot replace caller-injected capacity facts")
    result[variables[0]] = result[variables[1]] = "137438953472"
    return result


def native_pipeline_capacity_controls() -> None:
    ordinary = {"NO_COLOR": "1", "YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES": "1"}
    assert native_pipeline_capacity(ordinary) == ordinary
    hosted = {"NO_COLOR": "1", "YVEX_TEST_FIXTURE_CAPACITY": "1"}
    declared = native_pipeline_capacity(hosted)
    assert declared["YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES"] == "137438953472"
    assert declared["YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES"] == "137438953472"
    assert "YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES" not in hosted
    for name in ("YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES",
                 "YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES",
                 "YVEX_TEST_RUNTIME_CGROUP_AVAILABLE_MEMORY_BYTES"):
        try:
            native_pipeline_capacity({**hosted, name: "1"})
        except AssertionError as error:
            assert "cannot replace" in str(error)
        else:
            raise AssertionError(f"declared fixture capacity overwrote {name}")


def native_pipeline(binary: Path, reference: Path | None, compiler: Path) -> int:
    """Real compiled CPU computation, not a producer fixture response or model qualification."""
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-native-pipeline-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "NO_COLOR": "1"}
        native_pipeline_capacity_controls()
        environment = native_pipeline_capacity(environment)
        if environment.get("YVEX_TEST_FIXTURE_CAPACITY") == "1":
            print("native pipeline fixture: declared 128 GiB admission capacity; not host memory evidence",
                  flush=True)
        artifact = directory / "tiny.gguf"
        (directory / "bindings").mkdir()
        subprocess.run(["python3", str(ROOT / "tests/integration/tiny_model.py"), str(artifact)],
                       check=True, capture_output=True, text=True, timeout=15)
        compiled = subprocess.run([str(compiler), str(artifact), str(directory / "bindings"),
                                   str(directory / "input")], capture_output=True,
                                  text=True, env=environment, timeout=15)
        assert compiled.returncode == 0, ("native fixture compilation failed", compiled)
        binding = dict(line.split("=", 1) for line in compiled.stdout.splitlines())["binding_path"]
        common = ["--target", "tiny-executable", "--artifact", str(artifact), "--runtime-binding", binding,
                  "--backend", "cpu"]
        cases = [
            (["bench", "moe"], ["--input", "tensor-file", "--input-file", str(directory / "input-moe")]),
            (["bench", "transformer", "execute"], ["--input-file", str(directory / "input-transformer"),
                                                    "--context-capacity", "8", "--chunk-tokens", "1"]),
            *[(["bench", "transformer", action], ["--input-file", str(directory / "input-transformer"),
                                                  "--context-capacity", "8", "--prefill-tokens", "1",
                                                  "--prefill-chunk-tokens", "1"])
              for action in ["decode", "logits", "sample"]],
            (["bench", "transformer", "generate"], ["--text", "a", "--context-capacity", "8",
                                                     "--prefill-chunk-tokens", "1", "--max-new-tokens", "3"]),
        ]
        before = {path.relative_to(directory): hashlib.sha256(path.read_bytes()).hexdigest()
                  for path in directory.rglob("*") if path.is_file()}
        for path, arguments in cases:
            words = [*path, *common, *arguments, "--output", "json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0, (words, result)
            projected = json.loads(result.stdout)
            assert projected["status"] == "complete", projected
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0, (words, previous)
                assert computational_semantics(projected) == computational_semantics(json.loads(previous.stdout)), (
                    words, projected, previous)
            if path[-1] == "generate":
                assert projected["model_committed_tokens"] == 3 and projected["stop_reason"] == "max-new-tokens"
                assert [row["token_id"] for row in projected["generated_tokens"]] == [0, 0, 0]
            elif path[-1] == "decode":
                assert len(projected["steps"]) == 2
            elif path[-1] == "logits":
                assert len(projected["rows"]) == 3
                assert all(row["finite_count"] == 261 and row["minimum_logit"] == row["maximum_logit"] == 0
                           for row in projected["rows"])
            elif path[-1] == "sample":
                assert len(projected["selected_tokens"]) == 3
                assert all(row["token_id"] == 0 for row in projected["selected_tokens"])
            count += 1
            csv = invoke(binary, [*path, *common, *arguments, "--output", "csv"], environment)
            assert csv.returncode == 0 and '"status","complete"\n' in csv.stdout, (path, csv)
            count += 1
            refused = invoke(binary, [*path, *common, *arguments, "--max-host-bytes", "1", "--output", "json"], environment)
            assert refused.returncode != 0 and json.loads(refused.stdout)["status"] == "refused", (path, refused)
            recovery = invoke(binary, words, environment)
            assert recovery.returncode == 0 and computational_semantics(json.loads(recovery.stdout)) == computational_semantics(projected)
            count += 2
        after = {path.relative_to(directory): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in directory.rglob("*") if path.is_file()}
        assert before == after, "computational commands must not change persisted fixture state"
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def runtime_inputs(binary: Path, reference: Path | None) -> int:
    count = 0
    fixture = ROOT / "tests/fixtures/gguf/valid-tokenizer-simple.gguf"
    environment = {**os.environ, "NO_COLOR": "1"}
    for action, flags in [("tokens", ["--tokens", "0,1,7"]),
                          ("tokens", ["--tokens", "0, 1 ,7"]),
                          ("prompt", ["--text", "hello world"]),
                          ("tokens", ["--tokens", "8"]),
                          ("tokens", ["--tokens", "4294967296"]),
                          ("tokens", ["--tokens", "1,,2"]),
                          ("tokens", ["--tokens", "-1"]),
                          ("tokens", ["--tokens", ",".join(["1"] * 1025)])]:
        words = ["inspect", "input", action, "--model", str(fixture), *flags]
        result = invoke(binary, words, environment)
        expected = action == "prompt" or flags[-1] in {"0,1,7", "0, 1 ,7"}
        assert (result.returncode == 0) == expected, (words, result)
        if expected:
            assert "validated" in result.stdout and "generation_ready" in result.stdout, result
            if action == "tokens":
                assert "0 1 7" in result.stdout and "vocab_size" in result.stdout, result
        if reference:
            previous = invoke(reference, words, environment)
            assert result.returncode == previous.returncode, (words, result, previous)
        count += 1
    for words in [["inspect", "input", "tokens"],
                  ["inspect", "input", "prompt", "--model", str(fixture)],
                  ["inspect", "input", "tokens", "--model", str(fixture), "--text", "a"],
                  ["inspect", "input", "prompt", "--model", str(fixture), "--tokens", "1"]]:
        result = invoke(binary, words, environment)
        assert result.returncode == 2 and not result.stdout, (words, result)
        count += 1
    return count


def native_media(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-media-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data")}
        video = directory / "video.f32"
        audio = directory / "audio.f32"
        video.write_bytes(bytes(144))
        audio.write_bytes(bytes(32040))
        common = ["bench", "media", "publish", "--video-file", str(video), "--audio-file", str(audio),
                  "--frames", "3", "--width", "2", "--height", "2", "--audio-samples", "4005",
                  "--max-host-bytes", "1048576", "--max-output-bytes", "1048576"]
        expected = None
        for mode in ["normal", "table", "audit", "json", "csv"]:
            output = directory / f"{mode}.avi"
            words = [*common, "--out", str(output), "--output", mode]
            result = invoke(binary, words, environment)
            assert result.returncode == 0 and output.stat().st_size == 16524, result
            content = output.read_bytes()
            assert content[:4] == b"RIFF"
            assert expected is None or content == expected, "presentation must not affect media bytes"
            expected = content
            if mode == "json":
                value = json.loads(result.stdout)
                assert value["audio_samples_used"] == 4000 and value["audio_samples_trimmed"] == 5
                assert value["end_user_path_available"] is False
            if reference:
                previous_path = directory / f"reference-{mode}.avi"
                previous = invoke(reference, [*common, "--out", str(previous_path), "--output", mode], environment)
                assert previous.returncode == 0 and previous_path.read_bytes() == content, previous
            collision = invoke(binary, words, environment)
            assert collision.returncode != 0 and output.read_bytes() == content
            count += 2
        for flags in [["--frames", "0"], ["--audio-channels", "3"], ["--fps-denominator", "0"],
                      ["--max-host-bytes", "1"], ["--max-output-bytes", "1"],
                      ["--frames", "18446744073709551615"], ["--output", "unknown"]]:
            output = directory / "refused.avi"
            result = invoke(binary, [*common, "--out", str(output), *flags], environment)
            assert result.returncode != 0 and not output.exists(), (flags, result)
            count += 1
        # Native publication rejects non-finite samples; no partial package is emitted.
        video.write_bytes(struct.pack("f", float("nan")) + bytes(140))
        output = directory / "nonfinite.avi"
        result = invoke(binary, [*common, "--out", str(output)], environment)
        assert result.returncode != 0 and not output.exists(), result
        video.write_bytes(bytes(144))
        recovery = invoke(binary, [*common, "--out", str(directory / "recovery.avi")], environment)
        assert recovery.returncode == 0 and (directory / "recovery.avi").read_bytes() == expected
        count += 2
        for action, geometry in [("audio-vae", ["--latent-steps", "1"]),
                                 ("video-vae", ["--latent-frames", "1", "--latent-height", "1", "--latent-width", "1"])]:
            for target, backend, source in [("wrong", "cpu", str(video)),
                                             ("minimax-h3-fl2va", "cuda", str(video)),
                                             ("minimax-h3-fl2va", "cpu", "/dev/null")]:
                output = directory / "refused-component.f32"
                words = ["bench", "component", action, "--target", target, "--backend", backend,
                         "--artifact", str(directory / "missing.gguf"), "--input-file", source, *geometry,
                         "--out", str(output)]
                result = invoke(binary, words, environment)
                assert result.returncode != 0 and not output.exists(), result
                if reference:
                    previous = invoke(reference, words, environment)
                    assert result.returncode == previous.returncode, (result, previous)
                count += 1
        output = directory / "refused-generation.avi"
        common_generate = ["bench", "media", "generate", "--target", "wrong", "--prompt", "synthetic",
                           "--text-artifact", "missing", "--transformer-artifact", "missing",
                           "--video-artifact", "missing", "--audio-artifact", "missing", "--frames", "124",
                           "--width", "32", "--height", "32", "--out", str(output)]
        for additional in [[], ["--steps", "4294967296"], ["--seed", "-1"], ["--max-device-bytes", "0"]]:
            result = invoke(binary, [*common_generate, *additional], environment)
            assert result.returncode != 0 and not output.exists(), result
            assert "Rust migration" not in result.stderr + result.stdout
            count += 1
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def diagnostic_fixture(path: Path) -> None:
    """A complete role inventory, not an executable model or upstream oracle."""
    native = runpy.run_path(str(ROOT / "tests/integration/tiny_model.py"))
    tensor = native["Tensor"]
    names = ["token_embd.weight", "output_norm.weight", "output.weight",
             "blk.0.attn_norm.weight", "blk.0.attn_q.weight", "blk.0.attn_k.weight",
             "blk.0.attn_v.weight", "blk.0.attn_output.weight", "blk.0.ffn_norm.weight",
             "blk.0.ffn_gate.weight", "blk.0.ffn_up.weight", "blk.0.ffn_down.weight"]
    tensors = [tensor(name, (2,) if "norm" in name else (2, 2)) for name in names]
    metadata, array = native["metadata"], native["array"]
    text, u32, u64, align = [native[name] for name in ("text", "u32", "u64", "align")]
    facts = [metadata("general.architecture", 8, "llama"),
             metadata("general.name", 8, "bounded allocation inventory"),
             metadata("general.file_type", 4, 0), metadata("general.alignment", 4, 32),
             metadata("llama.context_length", 4, 8),
             metadata("tokenizer.ggml.model", 8, "yvex-fixture-simple"),
             array("tokenizer.ggml.tokens", 8, ["a", "b"])]
    payload, directory = b"", b""
    for item in tensors:
        payload = align(payload)
        directory += text(item.name) + u32(len(item.dims))
        directory += b"".join(u64(dim) for dim in item.dims) + u32(item.qtype) + u64(len(payload))
        payload += item.payload
    header = b"GGUF" + u32(3) + u64(len(tensors)) + u64(len(facts))
    path.write_bytes(align(header + b"".join(facts) + directory) + align(payload))


def artifact_diagnostics(binary: Path) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-diagnostics-") as temporary:
        directory = Path(temporary).resolve()
        model = directory / "inventory.gguf"
        diagnostic_fixture(model)
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "180",
                       "YVEX_CONFIG_DIR": str(directory / "config"), "YVEX_DATA_DIR": str(directory / "data"),
                       "YVEX_MODELS_ROOT": str(directory / "models")}
        before = hashlib.sha256(model.read_bytes()).hexdigest()

        def field(result, name, value):
            assert re.search(rf"(?m)^\s*{re.escape(name)}\s+{re.escape(str(value))}\s*$", result.stdout), (name, value, result)

        common = ["--model", str(model), "--audit"]
        for action in ["report", "descriptor", "materialization-plan", "family-runtime"]:
            result = invoke(binary, ["inspect", "model", "full", action, *common], environment)
            assert result.returncode == 0, result
            field(result, "tensor_count", 12)
            field(result, "generation_ready", "false")
            field(result, "runtime_qualification", "not-established-by-inventory")
            assert "real DeepSeek decode unsupported" not in result.stdout
            count += 1
        words = ["inspect", "model", "full", "descriptor", *common]
        for requirement, present in [("attention-q-projection", True), ("moe-router", False)]:
            result = invoke(binary, [*words, "--require-role", requirement], environment)
            assert result.returncode == 0, result
            field(result, "required_role_present", str(present).lower())
            count += 1
        for requirement, present in [("attention", True), ("moe", False), ("tokenizer", True)]:
            result = invoke(binary, [*words, "--require-collection", requirement], environment)
            assert result.returncode == 0, result
            field(result, "required_collection_present", str(present).lower())
            count += 1
        result = invoke(binary, ["inspect", "context", "report", *common, "--context-length", "2", "--chunk-size", "2",
                                 "--tokens", "0,1,4294967295"], environment)
        assert result.returncode == 0, result
        for key, value in [("token_count", 3), ("chunk_count", 2), ("last_chunk_size", 1),
                           ("context_overflow", "true"), ("overflow_mutates_state", "false")]:
            field(result, key, value)
        count += 1
        for flags in [["--context-length", "0"], ["--chunk-size", "-1"], ["--tokens", "1,,2"],
                      ["--tokens", "4294967296"], ["--tokens", ",".join(["1"] * 1025)]]:
            result = invoke(binary, ["inspect", "context", "report", *common, *flags], environment)
            assert result.returncode != 0, result
            count += 1
        for words in [["inspect", "model", "full", "report", *common, "--family", "llama"],
                      ["inspect", "model", "full", "family-runtime", *common, "--family", "deepseek2"],
                      ["inspect", "model", "full", "report", *common, "--plan-only"],
                      ["inspect", "model", "full", "descriptor", *common, "--format", "json"],
                      ["inspect", "model", "full", "report", *common, "--output", "json"],
                      ["inspect", "model", "full", "report", *common, "--limit-tensors", "0"]]:
            result = invoke(binary, words, environment)
            assert result.returncode != 0, result
            count += 1
        for words in [["inspect", "moe", "report", *common], ["inspect", "tensor", "collection", "report", "--collection", "moe", *common]]:
            result = invoke(binary, words, environment)
            assert result.returncode == 0, result
            field(result, "status", "moe-inventory-report")
            assert "expert_count" not in result.stdout, "tensor inventory cannot invent expert populations"
            count += 1
        proof = ["artifact", "materialize", "model", *common, "--backend", "cpu"]
        result = invoke(binary, proof, environment)
        assert result.returncode == 0, result
        for key, value in [("status", "fullmodel-materialize-pass"), ("required_tensor_count", 12),
                           ("required_tensor_bytes", 168), ("materialized_tensor_count", 12),
                           ("materialized_tensor_bytes", 168), ("allocation_attempted", "true"),
                           ("payload_transferred", "false"), ("execution_ready", "false"), ("cleanup_status", "pass")]:
            field(result, key, value)
        assert result.stdout.count("allocation_attempted") == result.stdout.count("cleanup_status") == 1
        count += 1
        for flag in ["--dry-run", "--plan-only"]:
            result = invoke(binary, [*proof, flag], environment)
            assert result.returncode == 0, result
            field(result, "allocation_attempted", "false")
            field(result, "cleanup_status", "not-needed")
            count += 1
        refused = invoke(binary, [*proof, "--limit-bytes", "1"], environment)
        assert refused.returncode != 0, refused
        field(refused, "failed_reason", "byte-limit")
        field(refused, "allocation_attempted", "false")
        count += 1
        phases = ["preflight", "resolve-model", "artifact-identity", "tensor-inventory", "role-coverage",
                  "placement-plan", "memory-budget", "backend-preflight", "materialize-embedding",
                  "materialize-normalization", "materialize-attention", "materialize-mlp", "materialize-moe",
                  "materialize-output", "materialize-tokenizer", "cleanup"]
        for phase in phases:
            result = invoke(binary, [*proof, "--fail-after-phase", phase], environment)
            assert result.returncode != 0, result
            field(result, "failed_phase", phase)
            if phase.startswith("materialize-") or phase == "cleanup":
                field(result, "cleanup_status", "pass")
            count += 1
        recovery = invoke(binary, proof, environment)
        assert recovery.returncode == 0 and recovery.stdout == invoke(binary, proof, environment).stdout, recovery
        count += 2
        missing_roles = invoke(binary, [*proof, "--require-role", "moe-router"], environment)
        assert missing_roles.returncode == 0, missing_roles
        field(missing_roles, "allocation_attempted", "false")
        field(missing_roles, "status", "fullmodel-materialize-refused")
        count += 1
        for action, expected in [("report", 0), ("descriptor", 5), ("family-runtime", 5), ("materialization-plan", 5)]:
            result = invoke(binary, ["inspect", "model", "full", action, "--model", "glm-5.2-official-safetensors",
                                     "--audit"], environment)
            assert result.returncode == expected, result
            field(result, "target_class", "official-source-huge-model")
            field(result, "allocation_attempted", "false")
            count += 1
        result = invoke(binary, ["artifact", "materialize", "model", "--model", "glm-5.2-official-safetensors"], environment)
        assert result.returncode == 5, result
        field(result, "payload_transferred", "false")
        count += 1
        assert hashlib.sha256(model.read_bytes()).hexdigest() == before
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def model_preparation(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-preparation-") as temporary:
        directory = Path(temporary).resolve()
        models = directory / "models"
        models.mkdir()
        registry = directory / "models.local.json"
        registry.write_text('{"schema":"yvex.models.local.v6","models":[]}\n', encoding="utf-8")
        environment = {**os.environ, "NO_COLOR": "1", "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "YVEX_MODELS_ROOT": str(models)}
        fixture = directory / "local.gguf"
        diagnostic_fixture(fixture)
        original = fixture.read_bytes()
        imported = invoke(binary, ["model", "pull", str(fixture), "--reference", "--name", "local",
                                  "--family", "qwen", "--models-root", str(models), "--json"], environment)
        assert imported.returncode == 0, imported
        common = ["--models-root", str(models), "--registry", str(registry)]
        for extra in [[], ["--dry-run"], ["--quant", "unknown"], ["--imatrix", "missing"]]:
            words = ["model", "prepare", "local", *common, *extra, "--json"]
            result = invoke(binary, words, environment)
            value = json.loads(result.stdout)
            assert result.returncode == 3 and value["state"] == "BLOCKED" and not value["changed"], result
            assert "preserved without requantization" in value["blocker"], value
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 3 and value == json.loads(previous.stdout), previous
            assert fixture.read_bytes() == original
            count += 1
        for words in [["model", "prepare"], ["model", "prepare", "missing", *common, "--json"],
                      ["model", "prepare", "local", "other", *common],
                      ["model", "prepare", "local", *common, "--unknown"]]:
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
        # The acquired-record fixture intentionally contains no model weights.
        # Dry-run selects native source/compiler/deployment authority, not source
        # authenticity or numerical execution. It must not create a physical plan.
        revision = "62af8fffb2f7030cac4de2f0169f5b8d1101b646"
        source = models / "source/hf/deepseek-ai/DeepSeek-V4-Flash-DSpark" / revision
        source.mkdir(parents=True)
        records = models / "registry/sources"
        records.mkdir(parents=True, exist_ok=True)
        record = records / "dspark.source.json"
        record.write_text(json.dumps({"schema": "yvex.model-source.registry.v1", "name": "dspark",
            "family": "deepseek", "provider": "huggingface",
            "repository": "deepseek-ai/DeepSeek-V4-Flash-DSpark", "revision": revision,
            "origin_uri": f"hf://deepseek-ai/DeepSeek-V4-Flash-DSpark@{revision}",
            "source_path": str(source), "storage": "managed", "format": "safetensors",
            "precision": "BF16", "digest": hashlib.sha256(b"").hexdigest(), "size_bytes": 0, "file_count": 0,
            "directory": True, "status": "complete", "verification": "revision-verified"}), encoding="utf-8")
        before = record.read_bytes()
        words = ["model", "prepare", "dspark", *common, "--dry-run", "--json"]
        result = invoke(binary, words, environment)
        assert result.returncode == 0, result
        value = json.loads(result.stdout)
        assert value["schema"] == "yvex.model.prepare.v1" and value["state"] == "PLANNED"
        assert not value["changed"] and value["creation_reproducible"]
        assert value["source"] == str(source) and value["revision"] == revision
        assert value["target"] == "deepseek4-v4-flash-dspark" and value["backend"] == "cuda"
        assert value["action"] == "materialize" and "<physical-variant-identity>" in value["artifact"]
        if reference:
            previous = invoke(reference, words, environment)
            assert previous.returncode == 0 and value == json.loads(previous.stdout), previous
        count += 1
        for extra in [["--quant", "not-a-preset"], ["--quant", "deepseek-v4-flash-mixed-iq2xxs-q2k-mxfp4-v1"]]:
            result = invoke(binary, ["model", "prepare", "dspark", *common, *extra,
                                     "--dry-run", "--json"], environment)
            assert result.returncode != 0 and json.loads(result.stdout)["state"] == "BLOCKED", result
            count += 1
        result = invoke(binary, ["model", "prepare", "dspark", *common, "--json"], environment)
        assert result.returncode != 0 and not result.stdout, result
        assert not list(models.rglob("*.quant-plan")) and not list(models.rglob("model.gguf"))
        assert not list(models.rglob("physical.plan")) and not list(models.rglob("*.binding"))
        assert record.read_bytes() == before and fixture.read_bytes() == original
        assert json.loads(registry.read_text(encoding="utf-8"))["models"] == []
        followup = invoke(binary, ["model", "pull", str(fixture), "--managed", "--prepare",
            "--name", "followup", "--family", "qwen", "--models-root", str(models)], environment)
        assert followup.returncode == 3 and "preserved without requantization" in followup.stderr, followup
        assert fixture.read_bytes() == original
        catalog = invoke(binary, ["model", "list", *common, "--json"], environment)
        assert catalog.returncode == 0 and any(item["selector"] == "followup"
            for item in json.loads(catalog.stdout)["models"]), catalog
        # Refusal releases the acquisition lease; a subsequent independent plan
        # succeeds without retrying any operator Case or touching the live daemon.
        recovery = invoke(binary, words, environment)
        assert recovery.returncode == 0 and json.loads(recovery.stdout) == value, recovery
        count += 3
    return count


def artifact_preparation(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-artifact-preparation-") as temporary:
        root = Path(temporary).resolve()
        source = root / "source"
        source.mkdir()
        payload = struct.pack("<32e", *(index / 16 for index in range(32)))
        header = {"embed.weight": {"dtype": "F16", "shape": [8, 4],
                                    "data_offsets": [0, len(payload)]}}
        encoded = json.dumps(header, separators=(",", ":")).encode()
        (source / "model.safetensors").write_bytes(struct.pack("<Q", len(encoded)) + encoded + payload)
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220",
                       "YVEX_CONFIG_DIR": str(root / "config"), "YVEX_DATA_DIR": str(root / "data")}
        target = "deepseek4-v4-flash-dspark-selected-embed"
        output = root / "output" / f"{target}-F16-noimatrix-yvex-v1.gguf"
        registry = root / "registry" / "models.local.json"
        words = ["compile", target, "--source", str(source), "--out", str(output),
                 "--registry", str(registry), "--models-root", str(root / "models")]
        result = invoke(binary, [*words, "--dry-run"], environment)
        assert result.returncode == 0 and "model-prepare-dry-run" in result.stdout, result
        assert re.search(r"registration_planned\s+true", result.stdout), result
        assert not re.search(r"\bregistered\s+true", result.stdout), result
        assert not output.parent.exists() and not registry.exists(), result
        count += 1
        result = invoke(binary, [*words, "--no-register"], environment)
        assert result.returncode == 0 and "model-prepare" in result.stdout, result
        assert "unsupported" in result.stdout and "not-performed" in result.stdout, result
        emitted = output.read_bytes()
        assert emitted.endswith(payload) and not registry.exists(), result
        if reference:
            previous = invoke(reference, [*words, "--overwrite", "--no-register"], environment)
            assert previous.returncode == 0 and output.read_bytes() == emitted, previous
        count += 1
        result = invoke(binary, words, environment)
        assert result.returncode == 1 and "overwrite" in result.stderr, result
        assert output.read_bytes() == emitted and not registry.exists()
        count += 1
        for _ in range(2):
            result = invoke(binary, [*words, "--overwrite"], environment)
            assert result.returncode == 0 and registry.exists(), result
            registered = json.loads(registry.read_text())
            record = next(entry for entry in registered["models"] if entry["alias"] == target)
            assert record["sha256"] == hashlib.sha256(emitted).hexdigest(), record
            assert record["support_level"] == "selected-tensor-materialized", record
            assert not record.get("runtime_binding") and not record.get("runtime_profile"), record
            verified = invoke(binary, ["profile", "verify", target, "--registry", str(registry), "--audit"],
                              environment)
            assert verified.returncode == 0 and "models-identity-pass" in verified.stdout, verified
            count += 1
        for arguments, exit in [(["compile", "unknown"], 2),
                                (["compile", "glm-5.2-official-safetensors", "--dry-run"], 5),
                                (["compile", f"{target}-rmsnorm", "--dry-run"], 5),
                                ([*words, "--json"], 2),
                                ([*words, "--output", "bad"], 2),
                                ([*words, "--out-dir", str(root)], 2),
                                ([*words, "--no-use"], 2),
                                (["compile", target, "--source", str(root / "missing"),
                                  "--out", str(root / "absent.gguf"), "--no-register"], 3)]:
            result = invoke(binary, arguments, environment)
            assert result.returncode == exit, (arguments, result)
            count += 1
        bad_source = root / "bad-source"
        bad_source.mkdir()
        header = {"unrelated.weight": {"dtype": "F16", "shape": [8, 4],
                                       "data_offsets": [0, len(payload)]}}
        encoded = json.dumps(header, separators=(",", ":")).encode()
        (bad_source / "model.safetensors").write_bytes(struct.pack("<Q", len(encoded)) + encoded + payload)
        before = registry.read_bytes()
        failure = invoke(binary, ["compile", target, "--source", str(bad_source), "--registry", str(registry),
                                  "--out", str(output), "--overwrite"], environment)
        assert failure.returncode == 1 and "embed.weight" in failure.stderr, failure
        assert registry.read_bytes() == before and output.read_bytes() == emitted
        recovery = invoke(binary, [*words, "--overwrite"], environment)
        assert recovery.returncode == 0, recovery
        count += 2
    return count


def artifact_check(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-artifact-check-") as temporary:
        root = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "220",
                       "YVEX_CONFIG_DIR": str(root / "config"), "YVEX_DATA_DIR": str(root / "data")}
        artifact = root / "slice.gguf"
        emitted = invoke(binary, ["compile", "artifact", "emit", "--out", str(artifact),
                                  "--model-name", "isolated-check", "--arch", "llama"], environment)
        assert emitted.returncode == 0, emitted
        for level in ["quick", "runtime", "full"]:
            for extra in [[], ["--no-materialize"], ["--no-graph"]]:
                words = ["artifact", "status", str(artifact), "--level", level, "--audit", *extra]
                result = invoke(binary, words, environment)
                assert result.returncode == 0 and "model-check-pass" in result.stdout, result
                assert "not-performed" in result.stdout and "not-executed-by-this-check" in result.stdout, result
                if reference:
                    previous = invoke(reference, words, environment)
                    assert previous.returncode == result.returncode and "model-check-pass" in previous.stdout, previous
                if level != "quick" and not extra == ["--no-materialize"]:
                    assert re.search(r"materialize\s+pass", result.stdout), result
                    assert re.search(r"plan\s+pass", result.stdout), result
                count += 1
        registry = root / "models.local.json"
        target = "deepseek4-v4-flash-dspark-selected-embed"
        created = invoke(binary, ["profile", "create", "--path", str(artifact), "--alias", target,
                                  "--registry", str(registry), "--support-level", "selected-tensor-materialized"],
                         environment)
        assert created.returncode == 0, created
        words = ["artifact", "status", target, "--registry", str(registry), "--audit"]
        result = invoke(binary, words, environment)
        assert result.returncode == 0 and re.search(r"registry-identity\s+pass", result.stdout), result
        if reference:
            previous = invoke(reference, words, environment)
            assert previous.returncode == result.returncode, (result, previous)
        count += 1
        before = registry.read_bytes()
        altered = json.loads(before)
        altered["models"][0]["sha256"] = "0" * 64
        registry.write_text(json.dumps(altered))
        failed = invoke(binary, words, environment)
        assert failed.returncode != 0 and "model-check-fail" in failed.stdout, failed
        assert "materialize              pass" not in failed.stdout, failed
        registry.write_bytes(before)
        report = root / "report"
        recovery = invoke(binary, [*words, "--level", "full", "--report-dir", str(report)], environment)
        assert recovery.returncode == 0 and "model-check-pass" in recovery.stdout, recovery
        saved = (report / f"model-check-{target}-cpu-full.txt").read_text()
        assert "model-check-pass" in saved and "\x1b" not in saved and str(artifact) in saved, saved
        count += 2
        malformed = root / "invalid.gguf"
        malformed.write_bytes(b"not a GGUF")
        for arguments, expected in [(["artifact", "status", "unknown", "--registry", str(registry)], None),
                                    (["artifact", "status", str(malformed)], 4),
                                    (["artifact", "status", str(root / "absent.gguf")], 3),
                                    (["artifact", "status", target, "--level", "unknown"], 2),
                                    (["artifact", "status", target, "--output", "bad"], 2),
                                    (["artifact", "status", target, "--backend", "bad"], 2),
                                    (["artifact", "status", target, "--json"], 2),
                                    (["artifact", "status"], 2),
                                    (["artifact", "status", "glm-5.2-official-safetensors"], 5),
                                    (["artifact", "status", f"{target}-rmsnorm"], 5)]:
            failure = invoke(binary, arguments, environment)
            assert failure.returncode != 0 and (expected is None or failure.returncode == expected), (arguments, failure)
            count += 1
        assert invoke(binary, ["artifact", "status", str(artifact), "--level", "runtime"],
                      environment).returncode == 0
        count += 1
    return count


def target_catalog(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-targets-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data"), "YVEX_MODELS_ROOT": str(directory / "models")}
        words = ["inspect", "target", "list", "--json"]
        result = invoke(binary, words, environment)
        assert result.returncode == 0, result
        catalog = json.loads(result.stdout)
        assert catalog["status"] == "model-target-list" and catalog["targets"], catalog
        if reference:
            assert catalog == json.loads(invoke(reference, words, environment).stdout)
        count += 1
        for record in catalog["targets"]:
            words = ["inspect", "target", "inspect", record["target_id"], "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode == 0, result
            value = json.loads(result.stdout)
            assert value["target_id"] == record["target_id"] and value["class"] == record["class"]
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == 0 and value == json.loads(previous.stdout), (result, previous)
            for mode in ["normal", "audit"]:
                human = invoke(binary, [*words[:-1], "--output", mode], environment)
                assert human.returncode == 0 and record["target_id"] in human.stdout, human
            count += 3
        for words in [["inspect", "target", "inspect", "unknown", "--json"],
                      ["inspect", "target", "inspect"], ["inspect", "target", "list", "--output", "invalid"],
                      ["inspect", "target", "classes", "--json"]]:
            result = invoke(binary, words, environment)
            assert result.returncode == 2 and not result.stdout, (words, result)
            count += 1
        for mode in ["normal", "table", "audit"]:
            result = invoke(binary, ["inspect", "target", "classes", "--output", mode], environment)
            assert result.returncode == 0 and "composite-source-model" in result.stdout, result
            count += 1
        assert not (directory / "config").exists() and not (directory / "data").exists()
    return count


def artifact_inventory(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-artifact-inventory-") as temporary:
        directory = Path(temporary).resolve()
        root = directory / "models"
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "80",
                       "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data")}

        def qualify(arguments, expected=0, compare=True):
            nonlocal count
            words = ["inspect", "artifact", *arguments, "--models-root", str(root)]
            result = invoke(binary, words, environment)
            assert result.returncode == expected, (words, result)
            if expected == 0 and "--json" in words:
                current = json.loads(result.stdout)
                if reference and compare:
                    previous = invoke(reference, words, environment)
                    assert previous.returncode == expected, (words, previous)
                    assert current == json.loads(previous.stdout), (words, current, previous.stdout)
            count += 1
            return result

        qualify(["registry", "--json"])
        for family, leaf in [("deepseek", "selected"), ("qwen", "controlled"), ("gemma", "unknown")]:
            fixture = root / "evidence/fixtures" / family / f"{leaf}.gguf"
            fixture.parent.mkdir(parents=True, exist_ok=True)
            fixture.write_bytes(b"not an admitted GGUF; discovery never proves admission")
        qualify(["registry", "--json"])
        for family, leaf in [("deepseek", "selected"), ("qwen", "controlled"), ("gemma", "unknown")]:
            qualify(["registry", "--family", family, "--json"])
            qualify(["status", leaf, "--json"])
        source = directory / "source"
        source.mkdir()
        registry = root / "registry/deepseek/dynamic.download.json"
        registry.parent.mkdir(parents=True)
        registry.write_text(json.dumps({"target_id": "dynamic", "family": "deepseek",
            "repo_id": "fixture/model", "revision": "a" * 40, "local_source_dir": str(source)}))
        qualify(["status", "dynamic", "--json"])
        reports = root / "evidence/build/deepseek"
        reports.mkdir(parents=True)
        for name, value in [("output-head-map", {"output_head_status": "present"}),
                            ("tensor-map", {"required_role_coverage_status": "required-groups-present"}),
                            ("tokenizer-map", {"status": "fixture"})]:
            (reports / f"dynamic.{name}.json").write_text(json.dumps(value))
            qualify(["status", "dynamic", "--json"])
        for mode in ["normal", "table", "audit"]:
            rendered = qualify(["registry", "--output", mode])
            assert "dynamic" in rendered.stdout and "Discovery only" in rendered.stdout
        explicit_audit = qualify(["registry", "--output", "audit"])
        alias_audit = qualify(["registry", "--audit"])
        assert explicit_audit.stdout == alias_audit.stdout, (explicit_audit, alias_audit)
        for arguments in [["status", "absent", "--json"], ["registry", "--family", "../bad"],
                          ["registry", "--output", "bad"]]:
            qualify(arguments, 2)
        duplicate = root / "evidence/fixtures/qwen/selected.gguf"
        duplicate.write_bytes(b"another observation with same target name")
        qualify(["status", "selected", "--json"], 2, False)
        qualify(["status", "selected", "--family", "deepseek", "--json"])
        blocked = invoke(binary, ["compile", "dynamic", "--models-root", str(root), "--dry-run"], environment)
        assert blocked.returncode == 5 and "complete-artifact-admission-required" in blocked.stderr, blocked
        assert not (root / "evidence/fixtures/deepseek/dynamic.gguf").exists()
        count += 1
        audit = invoke(binary, ["compile", "dynamic", "--models-root", str(root),
                                "--dry-run", "--audit"], {**environment, "COLUMNS": "180"})
        assert audit.returncode == 5, audit
        for label in ("tensor_map_status", "output_head_map_status", "tokenizer_map_status"):
            assert re.search(r'(?m)^\s*' + label + r'\s{2,}present-report-only$', audit.stderr), audit
        assert re.search(r'(?m)^\s*prepare_blocker_count\s{2,}2$', audit.stderr), audit
        assert "unsupported" in audit.stderr and not list(root.rglob("dynamic.gguf")), audit
        count += 1
        # Finite population is fail-closed, never a silently truncated success.
        for index in range(257):
            (root / f"evidence/fixtures/gemma/overflow-{index}.gguf").write_bytes(b"x")
        qualify(["registry", "--family", "gemma", "--json"], 4, False)
    return count


def target_engineering(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-target-engineering-") as temporary:
        directory = Path(temporary).resolve()
        source = directory / "source"
        source.mkdir()
        header = {"model.embed_tokens.weight": {"dtype": "F16", "shape": [4], "data_offsets": [0, 8]},
                  "lm_head.weight": {"dtype": "F16", "shape": [4], "data_offsets": [8, 16]}}
        encoded = json.dumps(header, separators=(",", ":")).encode()
        (source / "model.safetensors").write_bytes(struct.pack("<Q", len(encoded)) + encoded + bytes(16))
        for name in ["config.json", "tokenizer.json", "tokenizer_config.json", "generation_config.json", "special_tokens_map.json"]:
            (source / name).write_text('{"vocab_size":16,"model_type":"fixture"}')
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "80",
                       "YVEX_MODELS_ROOT": str(directory / "models"),
                       "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_DATA_DIR": str(directory / "data")}
        actions = [
            ["decision", "--release", "v0.1.0"],
            ["candidate", "--release", "v0.1.0"],
            ["dense-candidate", "--release", "v0.1.0", "--target", "qwen3-8b"],
            ["qwen-metal", "--release", "v0.1.0", "--target", "qwen3-8b"],
            ["class-profile", "qwen3-8b", "--source", str(source)],
            ["tensor-collection", "qwen3-8b", "--source", str(source)],
            ["tensor-map", "qwen3-8b", "--source", str(source)],
            ["tokenizer-map", "qwen3-8b", "--source", str(source)],
            ["missing-roles", "qwen3-8b", "--source", str(source)],
            ["quant-policy", "qwen3-8b", "--source", str(source)],
        ]
        for action in actions:
            for mode in ["normal", "table", "audit"]:
                words = ["inspect", "target", *action, "--output", mode]
                result = invoke(binary, words, environment)
                assert result.returncode == 0 and "Source/target evidence only" in result.stdout, (words, result)
                if reference:
                    previous = invoke(reference, words, environment)
                    assert previous.returncode == result.returncode, (words, previous, result)
                count += 1
        # Native report facts must survive independently of legacy C text rows.
        # These assertions deliberately retain report-only/non-claim boundaries.
        retained_facts = [
            (["decision", "--release", "v0.1.0"],
             {"report": "target-decision", "top_blocker": "source payload trust"}),
            (["decision", "--release", "v0.1.0", "--audit"],
             {"decision_state": "selected", "candidate.0.id": "deepseek4-v4-flash-dspark",
              "release_ready": "false", "runtime_claim": "unsupported"}),
            (["candidate", "--release", "v0.1.0"],
             {"report": "model-target candidate", "top_blocker": "source payload trust"}),
            (["dense-candidate", "--release", "v0.1.0"],
             {"report": "model-target dense-candidate", "selected": "none"}),
            (["qwen-metal", "--release", "v0.1.0", "--audit"],
             {"lane": "qwen-metal / apple-silicon-metal", "qwen_candidate_count": "3",
              "metal_allocation_status": "unsupported", "blocker_16": "missing-real-prefill"}),
        ]
        for action, expected in retained_facts:
            result = invoke(binary, ["inspect", "target", *action],
                            {**environment, "COLUMNS": "240"})
            assert result.returncode == 0, result
            for key, value in expected.items():
                assert re.search(r'(?m)^\s*' + re.escape(key) + r'\s{2,}' +
                                 re.escape(value) + r'$', result.stdout), (key, value, result)
            count += 1
        machine_cases = [
            ["decision", "--release", "v0.1.0"],
            ["class-profile", "deepseek4-v4-flash-dspark"],
            ["tokenizer-map", "qwen3-8b", "--source", str(source)],
            ["missing-roles", "qwen3-8b"],
            ["tensor-map", "qwen3-8b", "--source", str(source), "--gate", "v0.1.0"],
        ]
        # Preserve candidate names through typed facts, not the retired C audit
        # rows. These remain naming examples, never authenticated role admission.
        result = invoke(binary, ["inspect", "target", "tensor-map", "qwen3-8b",
                                 "--source", str(source), "--audit"],
                        {**environment, "COLUMNS": "180"})
        for label, value in [
            ("tensor_map.entry.0.native_name", "model.embed_tokens.weight"),
            ("tensor_map.entry.0.canonical_name", "model.embedding.token.weight"),
            ("tensor_map.entry.11.native_name", "lm_head.weight"),
            ("tensor_map.entry.11.canonical_name", "model.output_head.weight"),
        ]:
            assert re.search(r'(?m)^\s*' + re.escape(label) + r'\s{2,}' + re.escape(value) + r'$', result.stdout), result
        assert "unsupported-full-model" in result.stdout and result.returncode == 0, result
        count += 1
        for action in machine_cases:
            words = ["inspect", "target", *action, "--json"]
            result = invoke(binary, words, environment)
            assert result.returncode in [0, 5], (words, result)
            current = json.loads(result.stdout)
            if reference:
                previous = invoke(reference, words, environment)
                assert previous.returncode == result.returncode, (words, previous, result)
                try:
                    previous_json = json.loads(previous.stdout)
                except json.JSONDecodeError as error:
                    raise AssertionError((words, previous.stdout, previous.stderr)) from error
                assert current == previous_json, (words, current, previous.stdout)
            count += 1
        for action in ["candidate", "dense-candidate", "qwen-metal"]:
            result = invoke(binary, ["inspect", "target", action, "--release", "v0.1.0", "--json"], environment)
            assert result.returncode == 2 and not result.stdout, result
            count += 1
        result = invoke(binary, ["inspect", "target", "quant-policy", "qwen3-8b",
                                 "--role-support", "--json"], environment)
        assert result.returncode == 2 and not result.stdout, result
        assert "JSON output is unsupported" in result.stderr, result
        count += 1
        # The C shell printed human rows for --source --json: it never
        # established a machine contract. Qualify the repaired typed projection
        # against the native source-role observations, not those human bytes.
        result = invoke(binary, ["inspect", "target", "missing-roles", "qwen3-8b",
                                 "--source", str(source), "--json"], environment)
        observation = json.loads(result.stdout)
        assert result.returncode == 0 and observation["status"] == "missing-role-source-report", result
        assert observation["source_roles"] == {"required": 12, "observed": 11, "missing": 1, "ambiguous": 0}, observation
        assert observation["metadata"] == {"required": 4, "observed": 4, "missing": 0}, observation
        assert observation["evidence_basis"] == "header-and-sidecar-metadata-only", observation
        assert observation["runtime"] == "unsupported" and observation["generation"] == "unsupported-full-model", observation
        count += 1
        for words in [
            ["candidate"], ["candidate", "--release", "v9.9.9"],
            ["tensor-map", "qwen3-8b", "--role", "tokenizer", "--gate", "v0.1.0"],
            ["class-profile", "unknown"], ["tensor-map", "qwen3-8b", "--role", "bad"],
            ["inspect", "qwen3-8b", "--models-root", str(directory / "models")],
        ]:
            result = invoke(binary, ["inspect", "target", *words], environment)
            assert result.returncode == 2, (words, result)
            count += 1
        for width in [24, 80, 180]:
            result = invoke(binary, ["inspect", "target", "inspect", "qwen3-8b", "--paths", "--audit"],
                            {**environment, "COLUMNS": str(width)})
            assert result.returncode == 0 and "registry" in result.stdout and "source_path" in result.stdout, result
            count += 1
        # Read-only reports may write their explicit engineering sidecar, never an executable artifact.
        assert not list(directory.rglob("*.gguf"))
    return count


def remote_acquisition(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-remote-pull-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1", "COLUMNS": "240",
                       "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_HF_CLI": str(ROOT / "tests/fixtures/bin/fake-hf"),
                       "YVEX_FAKE_HF_LOG": str(directory / "hf.log"),
                       "YVEX_FAKE_HF_STATE": str(directory / "hf.auth"),
                       "YVEX_FAKE_HF_AUTH": "1"}
        for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN",
                     "YVEX_SOURCE_ACQUISITION_WORKER", "YVEX_SOURCE_ACQUISITION_OPERATION",
                     "YVEX_SOURCE_ACQUISITION_ID"):
            environment.pop(name, None)
        uri = "hf://MiniMaxAI/MiniMax-H3"
        root = directory / "references"
        common = ["model", "pull", uri, "--models-root", str(root), "--json"]
        for flags in [[], ["--format", "unknown"], ["--format", "gguf", "--variant", "missing"]]:
            result = invoke(binary, [*common, *flags], environment)
            assert result.returncode == 2 and not result.stdout, (flags, result)
            count += 1
        assert not root.exists()
        flags = ["--reference", "--format", "gguf", "--variant", "Q4_K_M"]
        result = invoke(binary, [*common, *flags, "--dry-run"], environment)
        assert result.returncode == 0 and not root.exists(), result
        planned = json.loads(result.stdout)
        assert planned["state"] == "REMOTE" and planned["resolved_revision"] == "b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08"
        count += 1
        result = invoke(binary, [*common, *flags], environment)
        assert result.returncode == 0, result
        value = json.loads(result.stdout)
        assert value["schema"] == "yvex.model.pull.v1" and value["state"] == "REMOTE"
        assert Path(value["record"]).is_file() and value["revision"] in value["origin"]
        count += 1
        result = invoke(binary, [*common[:-1], *flags, "--prepare"], environment)
        assert result.returncode == 3, result
        count += 1
        acquired = directory / "acquired"
        result = invoke(binary, ["model", "pull", uri, "--format", "safetensors", "--auth", "required",
                                 "--models-root", str(acquired), "--json"], environment)
        assert result.returncode == 0, result
        value = json.loads(result.stdout)
        assert value["status"] == "model-download-pass" and value["safetensors_count"] == 2
        assert value["precision"] == "F16" and value["representation_precision"] == "F16"
        source = Path(value["local_source_dir"])
        assert source.is_dir() and value["boundary"]["generation"] == "unsupported"
        count += 1
        log_before = Path(environment["YVEX_FAKE_HF_LOG"]).read_bytes()
        result = invoke(binary, ["model", "pull", uri, "--format", "safetensors", "--auth", "required",
                                 "--models-root", str(acquired), "--json"], environment)
        assert result.returncode == 0, result
        reused = json.loads(result.stdout)
        assert reused["schema"] == "yvex.model.pull.v1" and reused["changed"] is False
        assert reused["digest"] == value["source_payload_digest"] and Path(reused["location"]) == source
        assert Path(environment["YVEX_FAKE_HF_LOG"]).read_bytes() == log_before, "retained identity must not resolve mutable upstream"
        count += 1
        member = next(source.glob("*.safetensors"))
        original = member.read_bytes()
        member.write_bytes(original + b"changed after retained receipt")
        result = invoke(binary, ["model", "pull", uri, "--format", "safetensors", "--auth", "required",
                                 "--models-root", str(acquired), "--json"], environment)
        assert result.returncode != 0 and not result.stdout, result
        assert Path(environment["YVEX_FAKE_HF_LOG"]).read_bytes() == log_before
        assert member.read_bytes() != original, "integrity refusal must not repair or redownload silently"
        member.write_bytes(original)
        count += 1
        # Real byte identity through a bounded provider fixture, not a model/GPU claim.
        payload = directory / "tiny.gguf"
        payload.write_bytes(b"GGUF bounded source distribution fixture")
        digest = hashlib.sha256(payload.read_bytes()).hexdigest()
        tiny_environment = {**environment, "YVEX_FAKE_HF_DISCOVERY_MODE": "tiny",
                            "YVEX_FAKE_HF_TINY_BYTES": str(payload.stat().st_size),
                            "YVEX_FAKE_HF_TINY_SHA": digest,
                            "YVEX_FAKE_HF_RELEASE_FILE": str(payload),
                            "YVEX_FAKE_HF_RELEASE_NAME": "model-Q4_K_M.gguf"}
        tiny = directory / "tiny"
        result = invoke(binary, ["model", "pull", "hf://yvex-fixtures/tiny-executable",
                                 "--models-root", str(tiny), "--auth", "required", "--json"], tiny_environment)
        assert result.returncode == 0, result
        value = json.loads(result.stdout)
        assert len(value["source_payload_digest"]) == 64 and value["representation_format"] == "gguf", value
        assert hashlib.sha256((Path(value["local_source_dir"]) / "model-Q4_K_M.gguf").read_bytes()).hexdigest() == digest
        retained_digest = value["source_payload_digest"]  # Native tree identity is not an upstream file SHA.
        count += 1
        result = invoke(binary, ["model", "pull", "hf://yvex-fixtures/tiny-executable",
                                 "--models-root", str(tiny), "--auth", "required", "--json"], tiny_environment)
        assert result.returncode == 0, result
        value = json.loads(result.stdout)
        assert value["digest"] == retained_digest, value
        count += 1
    return count


def acquisition_loaded_image(binary: Path) -> int:
    if not Path("/proc/self/exe").exists():
        return 0  # Linux loaded-image guarantee; macOS is qualified separately.
    with tempfile.TemporaryDirectory(prefix="yvex-rust-loaded-image-") as temporary:
        directory = Path(temporary).resolve()
        executable = directory / "yvex"
        shutil.copy2(binary, executable)
        marker = directory / "metadata-entered"
        root = directory / "models"
        environment = {**os.environ, "NO_COLOR": "1",
                       "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_HF_CLI": str(ROOT / "tests/fixtures/bin/fake-hf"),
                       "YVEX_FAKE_HF_LOG": str(directory / "hf.log"),
                       "YVEX_FAKE_HF_STATE": str(directory / "hf.auth"),
                       "YVEX_FAKE_HF_AUTH": "1", "YVEX_FAKE_HF_INFO_MARKER": str(marker),
                       "YVEX_FAKE_HF_INFO_DELAY": "1"}
        for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN",
                     "YVEX_SOURCE_ACQUISITION_WORKER", "YVEX_SOURCE_ACQUISITION_OPERATION",
                     "YVEX_SOURCE_ACQUISITION_ID"):
            environment.pop(name, None)
        process = subprocess.Popen([str(executable), "source", "acquire", "gemma-4-12b-it",
                                    "--models-root", str(root), "--auth", "required", "--json"],
                                   cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 10
            while not marker.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(0.02)
            assert marker.exists(), "fixture did not enter pre-worker metadata lookup"
            executable.unlink()  # Only this test's private copy, never a build/service executable.
            stdout, stderr = process.communicate(timeout=15)
            assert process.returncode == 0, (stdout, stderr)
            value = json.loads(stdout)
            assert value["status"] == "model-download-pass" and Path(value["local_source_dir"]).is_dir()
        finally:
            if process.poll() is None:
                invoke(binary, ["source", "stop", "gemma-4-12b-it", "--models-root", str(root),
                                "--timeout-seconds", "5", "--force"], environment)
                process.communicate(timeout=10)
    return 1


def supervised_acquisition(binary: Path, reference: Path | None) -> int:
    count = 0
    with tempfile.TemporaryDirectory(prefix="yvex-rust-acquisition-") as temporary:
        directory = Path(temporary).resolve()
        environment = {**os.environ, "NO_COLOR": "1",
                       "YVEX_CONFIG_DIR": str(directory / "config"),
                       "YVEX_HF_CLI": str(ROOT / "tests/fixtures/bin/fake-hf"),
                       "YVEX_FAKE_HF_LOG": str(directory / "hf.log"),
                       "YVEX_FAKE_HF_STATE": str(directory / "hf.auth"),
                       "YVEX_FAKE_HF_AUTH": "1"}
        for name in ("HF_TOKEN", "HUGGING_FACE_HUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN",
                     "YVEX_SOURCE_ACQUISITION_WORKER", "YVEX_SOURCE_ACQUISITION_OPERATION",
                     "YVEX_SOURCE_ACQUISITION_ID"):
            environment.pop(name, None)
        def command(action, root, *options):
            return ["source", action, "gemma-4-12b-it", "--models-root", str(root),
                    "--output", "json", *options]
        def checked(action, root, *options, expected=0, extra=None):
            nonlocal count
            result = invoke(binary, command(action, root, *options), {**environment, **(extra or {})})
            assert result.returncode == expected, (action, options, result)
            value = json.loads(result.stdout) if result.stdout else None
            count += 1
            return value
        dry = directory / "dry"
        value = checked("acquire", dry, "--dry-run", "--auth", "never",
                        extra={"HF_TOKEN": "synthetic-secret", "YVEX_FAKE_HF_REQUIRE_ANONYMOUS": "1"})
        assert value["revision"] == "b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08" and not dry.exists(), value
        assert value["account_provider_stage"] == "skipped", value
        count += 1
        root = directory / "normal"
        value = checked("acquire", root, "--auth", "required")
        assert value["status"] == "model-download-pass" and value["payload_hash_verified"] is False
        assert value["account_provider_stage"] == "pass" and value["top_blocker"] == "none", value
        assert value["safetensors_count"] == 2 and value["boundary"]["runtime_ready"] is False
        source = Path(value["local_source_dir"])
        assert source.is_dir() and len(value["source_payload_digest"]) == 64
        status = checked("status", root)
        assert status["lifecycle"] == "complete" and status["committed_bytes"] == value["total_regular_file_bytes"]
        assert status["inflight_selected_bytes"] is None and status["current_rate_bytes_per_second"] is None
        count += 2
        if reference:
            previous = invoke(reference, command("status", root), environment)
            assert previous.returncode == 0, previous
            assert json.loads(previous.stdout) == status, (json.loads(previous.stdout), status)
            count += 1
        transfers_before = Path(environment["YVEX_FAKE_HF_LOG"]).read_text().count("  --local-dir\n")
        operation_before = next(root.rglob("*.acquisition.operation.json")).read_bytes()
        value = checked("resume", root, "--auth", "required")
        assert value["status"] == "model-download-resume-pass" and Path(value["local_source_dir"]) == source
        assert checked("status", root)["generation"] == 1
        assert Path(environment["YVEX_FAKE_HF_LOG"]).read_text().count("  --local-dir\n") == transfers_before
        assert next(root.rglob("*.acquisition.operation.json")).read_bytes() == operation_before
        assert checked("stop", root)["lifecycle"] == "complete"  # Terminal stop is idempotent.
        count += 1
        selected = directory / "selection"
        value = checked("acquire", selected, "--include", "*.json", "--exclude", "*.md", "--auth", "required")
        selected_source = value["local_source_dir"]
        value = checked("resume", selected, "--auth", "required")
        assert value["local_source_dir"] == selected_source and value["include_patterns"] == ["*.json"]
        assert value["exclude_patterns"] == ["*.md"]
        count += 1
        # Metadata-only then full selection of the SAME source: the worker
        # must reopen its exact operation, not resolve an ambiguous alias.
        multiple = directory / "multiple-selections"
        common = ["source", "acquire", "--repo", "test-org/test-model", "--family", "gemma",
                  "--name", "selected-model", "--revision",
                  "b8b09e34f8d2b9d1b7a51982ccb26ae2b8b9ef08", "--models-root", str(multiple),
                  "--auth", "required", "--json", "--stall-seconds", "2"]
        selected_paths = []
        for pattern in ("*.json", "*", "*.safetensors"):
            result = invoke(binary, [*common, "--include", pattern], environment)
            assert result.returncode == 0, result
            receipt = json.loads(result.stdout)
            assert receipt["status"] == "model-download-pass", receipt
            selected_paths.append(receipt["local_source_dir"])
            count += 1
        assert len(set(selected_paths)) == 3 and all(Path(p).is_dir() for p in selected_paths)
        result = invoke(binary, ["source", "status", "--repo", "test-org/test-model",
                                 "--family", "gemma", "--name", "selected-model",
                                 "--models-root", str(multiple), "--json"], environment)
        assert result.returncode != 0 and "ambiguous" in result.stderr, result
        count += 1
        failed = directory / "failed"
        value = checked("acquire", failed, "--auth", "required", expected=1,
                        extra={"YVEX_FAKE_HF_FAIL_AT_STEP": "2"})
        assert value["status"] == "model-download-fail" and Path(value["local_source_dir"]).is_dir()
        assert checked("status", failed)["lifecycle"] == "failed"
        assert checked("resume", failed, "--auth", "required")["status"] == "model-download-resume-pass"
        count += 1
        stopped = directory / "stopped"
        running = subprocess.Popen([str(binary), *command("acquire", stopped, "--auth", "required", "--tick-seconds", "1")],
                                   cwd=ROOT, env={**environment, "YVEX_FAKE_HF_STEP_DELAY": "1", "YVEX_FAKE_HF_STEPS": "8"},
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 8
            operation_path = stopped / "evidence/build/gemma/gemma-4-12b-it.acquisition.operation.json"
            while time.monotonic() < deadline:
                if operation_path.exists():
                    operation = json.loads(operation_path.read_text())
                    if operation.get("provider_process_present"):
                        break
                time.sleep(0.05)
            else:
                raise AssertionError("fixture provider did not become authenticated")
            status = checked("status", stopped)
            assert status["active"] is True and status["stop_available"] is True
            checked("cleanup", stopped, "--failed-partials", "--yes", expected=1)
            value = checked("stop", stopped, "--timeout-seconds", "5")
            assert value["lifecycle"] == "stopped" and value["reason"] == "operator-stop", value
            stdout, stderr = running.communicate(timeout=8)
            assert running.returncode == 1 and json.loads(stdout)["status"] == "model-download-interrupted", (stdout, stderr)
            assert not value["provider_pid"] and value["resume_available"] is True
            assert checked("resume", stopped, "--auth", "required")["status"] == "model-download-resume-pass"
            count += 3
        finally:
            if running.poll() is None:
                invoke(binary, command("stop", stopped, "--timeout-seconds", "5", "--force"), environment)
                running.communicate(timeout=8)
        detached = directory / "detached"
        observer = subprocess.Popen([str(binary), *command("acquire", detached, "--auth", "required")],
                                    cwd=ROOT, env={**environment, "YVEX_FAKE_HF_STEP_DELAY": "2", "YVEX_FAKE_HF_STEPS": "8"},
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 8
            operation_path = detached / "evidence/build/gemma/gemma-4-12b-it.acquisition.operation.json"
            while time.monotonic() < deadline:
                if operation_path.exists() and json.loads(operation_path.read_text()).get("provider_process_present"):
                    break
                time.sleep(0.05)
            else:
                raise AssertionError("detached fixture provider did not become authenticated")
            observer.terminate()
            stdout, stderr = observer.communicate(timeout=8)
            assert observer.returncode == 143 and not stdout and "observation detached" in stderr
            value = checked("status", detached)
            assert value["active"] and value["lifecycle"] == "downloading", value
            value = checked("stop", detached, "--timeout-seconds", "5")
            assert value["lifecycle"] == "stopped" and value["reason"] == "operator-stop", value
            count += 1
        finally:
            invoke(binary, command("stop", detached, "--timeout-seconds", "5", "--force"), environment)
            if observer.poll() is None:
                observer.communicate(timeout=8)
        provider_before = (directory / "hf.log").read_bytes()
        for flags in [["--auth", "invalid"], ["--max-workers", "0"], ["--progress", "invalid"],
                      ["--include", ""], ["--repo", "../escape", "--family", "../bad"]]:
            result = invoke(binary, ["source", "acquire", "gemma-4-12b-it", "--models-root", str(directory / "negative"), *flags], environment)
            assert result.returncode == 2, (flags, result)
            count += 1
        result = invoke(binary, ["source", "acquire", "--repo", "test-org/test-model", "--family", "llama/unsafe",
                                 "--models-root", str(directory / "negative")], environment)
        assert result.returncode == 2 and "source.acquisition.provenance" in result.stderr, result
        assert (directory / "hf.log").read_bytes() == provider_before, "invalid grammar contacted the provider"
        count += 1
        assert not (directory / "negative").exists()
        cache = source / ".cache" / "huggingface/download"
        cache.mkdir(parents=True, exist_ok=True)
        lock = cache / "fixture.lock"
        partial = cache / "fixture.incomplete"
        lock.write_bytes(b"stale lock")
        partial.write_bytes(b"preserved provider partial")
        retained_partial = partial.resolve()
        checked("resume", root, "--auth", "required", expected=1)
        plan = checked("cleanup", root, "--stale-locks", "--dry-run")
        assert plan["deleted_paths"] == 0 and lock.exists() and partial.exists(), plan
        machine_audit = checked("cleanup", root, "--stale-locks", "--dry-run", "--audit")
        assert machine_audit == plan, "human audit must not contaminate cleanup JSON"
        checked("resume", root, "--auth", "required", "--clear-stale-locks")
        assert not lock.exists() and partial.exists()
        lock.write_bytes(b"stale lock")
        removed = checked("cleanup", root, "--stale-locks", "--yes")
        assert removed["deleted_paths"] == 1 and not lock.exists() and partial.exists(), removed
        count += 3
        # Exact argv matching blocks cleanup while another owner uses this source.
        # A shell can exec sleep and lose its source argv. Use an explicit
        # process that retains the exact argument, and observe its readiness.
        owner = subprocess.Popen([sys.executable, "-c",
                                  "import time; print('ready', flush=True); time.sleep(30)",
                                  str(source)], stdout=subprocess.PIPE, text=True)
        try:
            assert select.select([owner.stdout], [], [], 5)[0], "source owner did not become ready"
            assert owner.stdout.readline() == "ready\n" and owner.poll() is None
            checked("cleanup", root, "--failed-partials", "--yes", expected=1)
            assert owner.poll() is None and source.exists() and partial.exists()
        finally:
            owner.terminate()
            owner.wait(timeout=6)
            owner.stdout.close()
        record_path = root / "registry/gemma/gemma-4-12b-it.download.json"
        record = record_path.read_bytes()
        victim = directory / "foreign-data"
        victim.mkdir()
        (victim / "preserve").write_bytes(b"not acquisition state")
        forged = json.loads(record)
        forged["local_source_dir"] = str(victim)
        record_path.write_text(json.dumps(forged))
        checked("cleanup", root, "--failed-partials", "--yes", expected=2)
        assert (victim / "preserve").read_bytes() == b"not acquisition state"
        record_path.write_bytes(record)
        # A symlinked parent is refused before any deletion.
        (root / "source/hf/google").rename(root / "source/hf/google-owned")
        (root / "source/hf/google").symlink_to(root / "source/hf/google-owned", target_is_directory=True)
        checked("cleanup", root, "--failed-partials", "--yes", expected=2)
        (root / "source/hf/google").unlink()
        (root / "source/hf/google-owned").rename(root / "source/hf/google")
        removed = checked("cleanup", root, "--failed-partials", "--yes")
        assert removed["deleted_paths"] > 0 and not source.exists() and not record_path.exists()
        assert retained_partial.exists(), "failed-partials cleanup must not follow the provider-cache alias"
        count += 3
    return count


def qualification(binary: Path) -> int:
    """Python authority / Rust consumer parity; fixtures never become model evidence."""
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    import qualification as q
    environment = dict(os.environ, NO_COLOR="1")
    prefix = ["model", "qualification"]
    catalog = invoke(binary, [*prefix, "list", "--json"], environment)
    assert catalog.returncode == 0, catalog.stderr
    records = json.loads(catalog.stdout)["targets"]
    for record in records:
        q.validate(record)
    suite = invoke(binary, [*prefix, "suite", "--json"], environment)
    assert suite.returncode == 0
    suites = json.loads(suite.stdout)["suites"]
    assert all(c["reasoning_modes"] == ["none", "high", "maximum"] for s in suites for c in s["cases"])
    target = {"schema":q.TARGET_SCHEMA, **{k:1 if t=="integer" else "fixture" for k,t in q.TARGET_FIELDS.items()}}
    metric = "decode.post-first.committed"; plane, unit, definition = q.METRICS[metric]
    m = dict(metric=metric,unit=unit,definition=definition,case="fixture",prompt_identity="fixture",
             reference_identity=None,session_state="fresh",warm_state="warm",output_bound=256,
             samples=[1,2,3],statistics=q.statistics_for([1,2,3]),scope="fixture",evidence="fixture")
    receipt = dict(schema=q.RECEIPT_SCHEMA,id="fixture",title="Software fixture only",target=target,
                   target_identity=q.target_id(target),origin="local",measurements=[m],
                   claims={p:dict(state="UNQUALIFIED",scope="fixture",required_evidence=[],evidence={},blockers=[]) for p in q.PLANES},
                   provenance={"source_stability":"fixture","evidence_class":"fixture","profiled":False},limitations=["Not model evidence"])
    q.validate(receipt)
    count = 2
    with tempfile.TemporaryDirectory(prefix="yvex-qualification-contract-") as directory:
        left, right = Path(directory)/"left.json", Path(directory)/"right.json"
        left.write_text(json.dumps(receipt));right.write_text(json.dumps(receipt))
        command = [*prefix,"compare",str(left),str(right),"--metric",metric,"--case","fixture","--json"]
        result = invoke(binary,command,environment)
        assert result.returncode == 0, result.stderr
        assert json.loads(result.stdout)["kind"] == "direct"
        count += 1
        for value in (True, None, "false"):
            candidate = json.loads(json.dumps(receipt))
            candidate["provenance"]["profiled"] = value
            try:
                q.comparison(receipt, m, candidate, candidate['measurements'][0])
            except ValueError:
                pass
            else:
                raise AssertionError('profiled/unknown fixture was admitted for performance comparison')
            right.write_text(json.dumps(candidate))
            assert invoke(binary,command,environment).returncode != 0
            count += 1
        for outcomes in (None, {}, [{"case":"fixture", "result":"QUALIFIED", "reason":"fixture", "evidence":"fixture"}],
                         [{"case":"fixture", "result":"FAIL", "reason":"bounded refusal", "evidence":"fixture"}]):
            candidate = json.loads(json.dumps(receipt))
            candidate["provenance"]["case_outcomes"] = outcomes
            try:
                q.validate(candidate)
                expected_valid = True
            except ValueError:
                expected_valid = False
            right.write_text(json.dumps(candidate))
            result = invoke(binary,[*prefix,"show",str(right),"--json"],environment)
            assert (result.returncode == 0) == expected_valid, result.stderr
            count += 1
        fact = dict(id="mapped-rss", case="fixture", value=0, unit="byte",
                    definition="Linux mapping RSS; not a timed benchmark", evidence="fixture")
        for facts in ([fact], [dict(fact, value=None)], [dict(fact, value=True)],
                      [dict(fact, value=-1)], [dict(fact, unit="token/s")], [fact, fact], None):
            candidate = json.loads(json.dumps(receipt))
            candidate["provenance"]["diagnostics"] = facts
            try:
                q.validate(candidate)
                expected_valid = True
            except ValueError:
                expected_valid = False
            right.write_text(json.dumps(candidate))
            result = invoke(binary,[*prefix,"show",str(right),"--json"],environment)
            assert (result.returncode == 0) == expected_valid, result.stderr
            if expected_valid:
                human = invoke(binary,[*prefix,"show",str(right)],environment)
                assert human.returncode == 0 and "not timed benchmark samples" in human.stdout
                assert ("NOT MEASURED" in human.stdout) == (facts[0]["value"] is None)
            count += 1
        for key, kind in q.TARGET_FIELDS.items():
            candidate = json.loads(json.dumps(receipt))
            candidate["target"][key] = 2 if kind == "integer" else "different"
            candidate["target_identity"] = q.target_id(candidate["target"])
            right.write_text(json.dumps(candidate))
            refused = invoke(binary,command,environment)
            assert refused.returncode != 0, key
            admitted = invoke(binary,[*command,"--vary",key],environment)
            assert admitted.returncode == 0, (key,admitted.stderr)
            expected = q.comparison(receipt,m,candidate,candidate["measurements"][0],(key,))
            assert json.loads(admitted.stdout)["varying"] == expected["varying"]
            count += 2
        candidate = json.loads(json.dumps(receipt));candidate["measurements"][0]["statistics"]["median"]=900
        right.write_text(json.dumps(candidate))
        assert invoke(binary,command,environment).returncode != 0
        # Unknown suite refuses before connecting to any Host or creating a receipt.
        refused = invoke(binary,[*prefix,"run","fixture","--suite","absent","--case","absent",
                                 "--reasoning","maximum","--receipt-dir",str(Path(directory)/"new"),"--json"],environment)
        assert refused.returncode != 0 and not (Path(directory)/"new").exists()
        count += 2
        candidate = json.loads(json.dumps(receipt))
        metric = "same-top-token"
        _, unit, definition = q.METRICS[metric]
        candidate["measurements"][0].update(metric=metric, unit=unit, definition=definition)
        command = [*prefix,"compare",str(left),str(right),"--metric",metric,"--case","fixture","--json"]
        for identity in (None, "", " ", "independent-fixture"):
            candidate["measurements"][0]["reference_identity"] = identity
            left.write_text(json.dumps(candidate)); right.write_text(json.dumps(candidate))
            result = invoke(binary, command, environment)
            assert (result.returncode == 0) == (identity == "independent-fixture"), result.stderr
            count += 1
    return count


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--reference", type=Path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--benchmark-fixture", type=Path)
    parser.add_argument("--tiny-compiler", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    reference = args.reference.resolve() if args.reference else None
    console_count = empty_host_console(binary)
    print(f"PASS isolated empty-host console: {console_count} modes; no operator host or model")
    qualification_count = qualification(binary)
    print(f"PASS qualification contracts: {qualification_count} controls; software evidence only")
    account_count = accounts(binary, reference)
    tokenizer_count = tokenizers(binary, reference)
    path_count = paths(binary, reference)
    catalog_count = catalogs(binary, reference)
    artifact_count = artifacts(binary, reference)
    profile_count = profiles(binary, reference)
    integrity_count = integrity(binary, reference)
    source_count = sources(binary, reference)
    native_count = native_weights(binary, reference)
    construction_count = artifact_construction(binary, reference)
    conversion_count = conversion(binary, reference)
    materialization_count = materialization(binary, reference)
    gate_count = artifact_gates(binary, reference)
    mapping_count = tensor_mapping(binary, reference)
    document_count = quant_documents(binary, reference)
    variant_count = physical_variants(binary, reference)
    discovery_count = provider_catalog(binary, reference)
    distribution_count = model_distribution(binary, reference)
    acquisition_count = local_acquisition(binary, reference)
    attention_count = attention_operations(binary, reference)
    benchmark_count = benchmark_publication(binary, reference, args.benchmark_fixture.resolve()) if args.benchmark_fixture else 0
    pipeline_count = graph_pipeline_operations(binary, reference)
    generation_count = generation_operations(binary, reference)
    native_pipeline_count = native_pipeline(binary, reference, args.tiny_compiler.resolve()) if args.tiny_compiler else 0
    input_count = runtime_inputs(binary, reference)
    media_count = native_media(binary, reference)
    diagnostic_count = artifact_diagnostics(binary)
    target_count = target_catalog(binary, reference)
    preparation_count = model_preparation(binary, reference)
    artifact_preparation_count = artifact_preparation(binary, reference)
    check_count = artifact_check(binary, reference)
    inventory_count = artifact_inventory(binary, reference)
    engineering_count = target_engineering(binary, reference)
    supervised_count = supervised_acquisition(binary, reference)
    remote_count = remote_acquisition(binary, reference)
    loaded_image_count = acquisition_loaded_image(binary)
    print(f"PASS Rust CLI contracts: providers={account_count} tokenizer={tokenizer_count} paths={path_count} catalog={catalog_count} artifacts={artifact_count} profiles={profile_count} integrity={integrity_count} source={source_count} native_weights={native_count} construction={construction_count} conversion={conversion_count} materialization={materialization_count} gates={gate_count} mapping={mapping_count}; "
          f"quant_documents={document_count}; physical_variants={variant_count}; discovery={discovery_count}; distribution={distribution_count}; local_acquisition={acquisition_count}; attention={attention_count}; benchmark_publication={benchmark_count}; pipeline={pipeline_count}; generation={generation_count}; native_pipeline={native_pipeline_count}; runtime_input={input_count}; media={media_count}; artifact_diagnostics={diagnostic_count}; target_catalog={target_count}; model_preparation={preparation_count}; artifact_preparation={artifact_preparation_count}; artifact_check={check_count}; artifact_inventory={inventory_count}; target_engineering={engineering_count}; supervised_acquisition={supervised_count}; remote_acquisition={remote_count}; loaded_image={loaded_image_count}; isolated projections/refusals, pure JSON, redaction, native IDs and ordered prompt roles")


def empty_host_console(binary: Path) -> int:
    """Real owned empty hosts: RAW is JSONL, OFF is silent, human has a header."""
    for mode in ("json", "off", "human"):
        with tempfile.TemporaryDirectory(prefix="yvex-console-") as temporary:
            runtime = Path(temporary).resolve()
            env = dict(os.environ, XDG_RUNTIME_DIR=str(runtime), NO_COLOR="1")
            process = subprocess.Popen([str(binary), "serve", "--openai", "off", "--logs", mode],
                                       cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 15
                while not (runtime / "yvex/yvexd.sock").exists():
                    assert process.poll() is None and time.monotonic() < deadline
                    time.sleep(.02)
                status = invoke(binary, ["host", "status", "--json"], env)
                assert status.returncode == 0 and json.loads(status.stdout)["loaded_engine_count"] == 0
                stopped = invoke(binary, ["host", "stop"], env)
                assert stopped.returncode == 0, stopped.stderr
                out, err = process.communicate(timeout=15)
                assert process.returncode == 0 and not err, (mode, err)
                assert not (runtime / "yvex/yvexd.sock").exists()
                if mode == "off":
                    assert not out
                elif mode == "json":
                    rows = [json.loads(line) for line in out.splitlines()]
                    assert rows and all(isinstance(row, dict) and "kind" in row for row in rows)
                    assert "\x1b" not in out
                else:
                    assert "HOST ready" in out and "Ctrl-C to stop" in out
            finally:
                if process.poll() is None:
                    process.terminate()  # Only this test-owned empty process.
                    process.communicate(timeout=15)
    return 3


if __name__ == "__main__":
    main()
