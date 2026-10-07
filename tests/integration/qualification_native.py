#!/usr/bin/env python3
"""Qualify the typed native measurement observer; no model or speed claim."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import qualification_run as measurement


def main():
    client, fixture = (Path(path).resolve() for path in sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="yvex-native-observer-") as directory:
        root = Path(directory)
        root.chmod(0o700)
        socket, prompt = root / "host.sock", root / "input.txt"
        prompt.write_text("hello")
        with (root / "host.log").open("wb") as log:
            host = subprocess.Popen([str(fixture), str(socket)], stdout=log, stderr=log)
            try:
                deadline = time.monotonic() + 10
                while not socket.exists():
                    assert host.poll() is None and time.monotonic() < deadline
                    time.sleep(0.01)
                command = [str(client), str(socket), "deepseek4-v4-flash-dspark",
                           "7", "bench-observer", "none", "3", "greedy",
                           str(prompt), str(prompt)]
                for index, invalid in [(3, "0"), (6, "-1"), (7, "unsupported")]:
                    refused = command.copy()
                    refused[index] = invalid
                    result = subprocess.run(refused, capture_output=True, timeout=10)
                    assert result.returncode != 0 and not result.stdout, result
                result = subprocess.run(command, capture_output=True, timeout=10)
                assert result.returncode == 0, result.stderr
                rows = [json.loads(line) for line in result.stdout.splitlines()]
                observations = measurement.summarize_native(rows)
                assert len(observations) == 2, observations
                for index, value in enumerate(observations):
                    assert value["request"] == index + 2, value
                    assert (0 <= value["client_admitted_seconds"]
                            <= value["client_first_visible_seconds"]
                            <= value["client_complete_seconds"]), value
                    assert value["first_fragment_publication_seconds"] is None
                    assert value["generated_tokens"] == 3, value
                    assert value["server_first_token_seconds"] == 2.5, value
                    assert value["prefill_tokens"] == 4 and value["reused_tokens"] == 1, value
                assert host.poll() is None and socket.exists()
            finally:
                if host.poll() is None:
                    host.terminate()
                assert host.wait(timeout=5) == 0
        log = (root / "host.log").read_text()
        assert "session.summary created=1 closed=1" in log, log
        print("native observer: typed TURN_STARTED/fragment/terminal correlation; "
              "two turns; malformed arguments refused before session mutation; "
              "session cleanup exact; no model or performance claim")


if __name__ == "__main__":
    main()
