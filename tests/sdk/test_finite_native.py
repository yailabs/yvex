#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""Explicit native client conformance using public installed headers, NOT YVEX inference."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"
import sys
GUARD = str(SDK/"build_support/check_finite_abi.py")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--include-dir", required=True, type=Path)
    args = p.parse_args()
    with tempfile.TemporaryDirectory(prefix="yvex-sdk-native-conformance-") as directory:
        prefix = Path(directory)
        (prefix / "include/yvex").mkdir(parents=True)
        (prefix / "lib").mkdir()
        for name in ("core.h", "finite_decision_producer.h"):
            shutil.copyfile(args.include_dir / "yvex" / name, prefix / "include/yvex" / name)
        subprocess.run(["python3", GUARD, "--include-dir", str(prefix / "include"), "--check"], cwd=SDK, check=True)
        obj = prefix / "fixture.o"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(prefix / "include"),
            "-c", str(ROOT / "tests/integration/sdk_finite_client.c"), "-o", str(obj)], check=True)
        subprocess.run(["ar", "rcs", str(prefix / "lib/libyvex.a"), str(obj)], check=True)
        env = dict(os.environ, YVEX_CLIENT_PREFIX=str(prefix), YVEX_CLIENT_LINK_LIBS="")
        command = ["cargo", "test", "--locked", "-p", "yvex-sdk@0.2.0", "--features", "finite-decision-native"]
        subprocess.run(command, cwd=SDK, env=env, check=True)
        executable = SDK / "target/debug/examples/finite"
        subprocess.run(["cargo", "build", "--locked", "-p", "yvex-sdk@0.2.0", "--features", "finite-decision-native", "--example", "finite"], cwd=SDK, env=env, check=True)
        request = {"model_alias": "sdk-conformance-only", "expected_generation": 7, "question": "valid", "context": "SDK fixture, not Case content",
            "candidates": [{"id": "opaque:one", "text": "First"}, {"id": "opaque:two", "text": "Second"}]}
        path = prefix / "request.json"
        for question in ["valid", "stale", "foreign", "nan", "calibrated", "generated", "oversized", "unterminated", "refuse"]:
            request["question"] = question
            path.write_text(json.dumps(request))
            result = subprocess.run([str(executable), str(prefix / "unused.socket"), str(path)], capture_output=True, text=True)
            assert (result.returncode == 0) == (question == "valid"), (question, result.stderr)
            assert "CONTEXT_MESSAGE_MUST_NOT_BE_RENDERED" not in result.stderr
            if question == "valid":
                value = json.loads(result.stdout)
                assert value["engine_generation"] == 7 and not value["calibrated"]
                assert [c["id"] for c in value["candidates"]] == [c["id"] for c in request["candidates"]]
        # Same schema/size with changed field identity cannot bypass the public
        # declaration guard. Only this disposable header is changed.
        header = prefix / "include/yvex/finite_decision_producer.h"
        original_header = header.read_text()
        header.write_text(original_header.replace("engine_generation, token_count", "token_count, engine_generation"))
        refused = subprocess.run(["python3", GUARD, "--include-dir", str(prefix / "include"), "--check"], cwd=SDK, capture_output=True, text=True)
        assert refused.returncode != 0 and "declaration drift" in refused.stderr
        header.write_text(original_header.replace("double raw_score;", "double different_score;"))
        refused = subprocess.run(["python3", GUARD, "--include-dir", str(prefix / "include"), "--check"], cwd=SDK, capture_output=True, text=True)
        assert refused.returncode != 0 and "declaration drift" in refused.stderr
        header.write_text(original_header)
        core = prefix / "include/yvex/core.h"
        core.write_text(core.read_text().replace("YVEX_ERR_STATE = -8", "YVEX_ERR_STATE = -80"))
        refused = subprocess.run(["python3", GUARD, "--include-dir", str(prefix / "include"), "--check"], cwd=SDK, capture_output=True, text=True)
        assert refused.returncode != 0 and "declaration drift" in refused.stderr
        print("PASS native SDK C-client conformance: exact ABI, copied identities, eight result/refusal negatives, record/nested-field/status drift negatives; NO MODEL RUNTIME QUALIFICATION")


if __name__ == "__main__":
    main()
