#!/usr/bin/env python3
"""Real CPU chat on an owned host; synchronize through terminal state and typed facts."""
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import pty
import select
import struct
import subprocess
import sys
import tempfile
import termios
import time

ARTIFACT_SHA = "0c5776eb6b1f2abb3a35f2324aabc4d8b7693856650b799e88161f7167feded6"
PROFILE = "qwen3-5-0-8b-qwen3-5-0-8b-source-faithful-cpu"
SESSION = "qwen-conversation-proof"


def main():
    binary, artifact, binding, output = map(lambda p: Path(p).resolve(), sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    assert hashlib.file_digest(artifact.open("rb"), "sha256").hexdigest() == ARTIFACT_SHA
    env = os.environ.copy()
    trace_path = output / "host.jsonl"
    terminal = bytearray()
    host = chat = None
    master = slave = None

    def command(*args, ok=True):
        result = subprocess.run([str(binary), *args], env=env, capture_output=True, timeout=120)
        assert (result.returncode == 0) == ok, (args, result.stderr.decode())
        return result

    def typed(name, *args):
        result = command(*args, "--json")
        (output / (name + ".json")).write_bytes(result.stdout)
        return json.loads(result.stdout)

    def drain():
        while master is not None and select.select([master], [], [], 0)[0]:
            try:
                data = os.read(master, 65536)
            except OSError as error:
                if error.errno == errno.EIO:
                    break
                raise
            if not data:
                break
            terminal.extend(data)

    def events():
        result = []
        for line in trace_path.read_bytes().splitlines():
            try:
                result.append(json.loads(line))
            except ValueError:
                pass  # Progress/banner or an as-yet incomplete final log record.
        return result

    def until(predicate, seconds=180):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            drain()
            if predicate():
                return
            assert host.poll() is None, "owned host exited"
            assert chat is None or chat.poll() is None, "chat exited before completion"
            time.sleep(0.02)
        raise AssertionError("bounded product qualification timed out")

    runtime_base = "/private/tmp" if sys.platform == "darwin" else "/tmp"
    with tempfile.TemporaryDirectory(prefix="yvex-qwen-chat-", dir=runtime_base) as runtime:
        env["XDG_RUNTIME_DIR"] = runtime
        env["YVEX_MODELS_REGISTRY"] = str(Path(runtime) / "profiles.json")
        try:
            command("profile", "create", "--path", str(artifact), "--runtime-binding", str(binding),
                    "--registry", env["YVEX_MODELS_REGISTRY"], "--alias", PROFILE + "-" + binding.stem[:16],
                    "--target", "qwen3.5-0.8b", "--backend", "cpu", "--ctx", "256",
                    "--execution-strategy", "target-only")
            with trace_path.open("wb") as trace:
                host = subprocess.Popen([str(binary), "serve", "--openai", "off", "--logs", "json",
                                         "--trace-content", "--trace-level", "full"], env=env,
                                        stdout=trace, stderr=trace)
                until(lambda: subprocess.run([str(binary), "host", "status", "--json"], env=env,
                                             capture_output=True).returncode == 0, 30)
                load = typed("load", "model", "load", "qwen3.5-0.8b", "--ctx", "256")
                assert load["backend"] == "cpu" and load["artifact_identity"] == ARTIFACT_SHA
                master, slave = pty.openpty()
                fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
                before = termios.tcgetattr(slave)
                chat = subprocess.Popen([str(binary), "chat",
                                         "--session", SESSION, "--max-new-tokens", "16"],
                                        env=env, stdin=slave, stdout=slave, stderr=slave)
                until(lambda: not (termios.tcgetattr(slave)[3] & termios.ICANON), 30)
                summaries = []
                for index, prompt in enumerate(("My name is Ada. Reply with my name only.",
                                                "What is my name? Reply with my name only."), 1):
                    os.write(master, prompt.encode() + b"\r")
                    until(lambda: len([e for e in events() if e.get("kind") == "generation.completed"
                                       and e.get("session") == SESSION]) == index)
                    state = typed("session-turn-" + str(index), "session", "show", SESSION)["session"]
                    assert state["turns"] == index and state["state"] == "ready"
                    assert state["position"] == (24 if index == 1 else 46)
                    summaries.append(state)
                assert summaries[0]["identity"] == summaries[1]["identity"]
                records = [e for e in events() if e.get("session") == SESSION]
                first = [e["b"] for e in records if e["kind"] == "generation.first_token"]
                counts = [e["a"] for e in records if e["kind"] == "tokenizer.completed"]
                completed = [e for e in records if e["kind"] == "generation.completed"]
                assert first == [92055, 92055] and counts == [23, 45]
                assert all(e["a"] == 1 and e["c"] == 1 and e["artifact_identity"] == ARTIFACT_SHA
                           for e in completed)
                os.write(master, b"/quit\r")
                chat.wait(timeout=15)
                drain()
                assert chat.returncode == 0 and termios.tcgetattr(slave) == before
                command("session", "close", SESSION)
                unload = typed("unload", "model", "unload", "qwen3.5-0.8b")
                assert unload["state"] == "unloaded"
                typed("host-after-unload", "host", "status")
                command("host", "stop")
                assert host.wait(timeout=15) == 0
            proof = {"schema": "yvex.evidence.qwen-small-chat.v1", "backend": "cpu",
                     "artifact_identity": ARTIFACT_SHA, "generated_token_ids": first,
                     "prompt_token_counts": counts, "sessions": summaries,
                     "terminal_restored": True, "owned_host_exit": host.returncode}
            (output / "proof.json").write_text(json.dumps(proof, indent=2) + "\n")
            print(json.dumps(proof))
        finally:
            (output / "terminal.bin").write_bytes(terminal)
            if chat is not None and chat.poll() is None:
                chat.terminate()
                chat.wait(timeout=15)
            if host is not None and host.poll() is None:
                command("host", "stop")
                host.wait(timeout=30)
            for descriptor in (master, slave):
                if descriptor is not None:
                    os.close(descriptor)


if __name__ == "__main__":
    assert len(sys.argv) == 5, "usage: qwen_small_chat.py BINARY ARTIFACT BINDING OUTPUT"
    main()
