#!/usr/bin/env python3
"""Real restricted SSH + public C client; synthetic native peer, not model QA."""
import copy
import json
import os
from pathlib import Path
import pwd
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
BIN = Path(os.environ.get("YVEX_BIN", ROOT / "yvex")).resolve()
PEER = Path(os.environ.get("YVEX_FINITE_REMOTE_PEER", ROOT / "build/tests/finite-remote-peer")).resolve()
COUNT = 0
RESPONSE = json.loads((ROOT / "docs/contracts/schema/finite-response-v1.schema.json").read_text())


def checked(*command, **kwargs):
    return subprocess.run(command, check=True, text=True, capture_output=True, timeout=30, **kwargs).stdout.strip()


def wait_for(predicate):
    deadline = time.monotonic() + 8
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("isolated fixture not ready")


def main():
    global COUNT
    cache = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="yvex-finite-remote-", dir=cache) as temporary:
        folder = Path(temporary)
        folder.chmod(0o700)
        runtime = folder / "runtime"
        (runtime / "yvex").mkdir(parents=True, mode=0o700)
        native_socket = runtime / "yvex/yvexd.sock"
        for name in ("host", "compute", "management", "unknown"):
            checked("ssh-keygen", "-q", "-N", "", "-t", "ed25519", "-f", str(folder / name))
        trust = folder / "authorized_keys"
        checked(str(BIN), "management", "trust-init", str(trust))
        identities = {name: checked(str(BIN), "management", "identity", str(folder / (name + ".pub")))
                      for name in ("host", "compute", "management")}

        def enroll(name, scope):
            checked(str(BIN), "management", "enroll", str(folder / (name + ".pub")),
                    str(trust), str(folder / "host.pub"), identities[name].split(":")[-1], "--scope", scope)

        enroll("compute", "finite-decision")
        enroll("management", "management")
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        known = folder / "known_hosts"
        known.write_text(f"[127.0.0.1]:{port} {(folder / 'host.pub').read_text()}")
        known.chmod(0o600)
        config = folder / "sshd_config"
        user = pwd.getpwuid(os.getuid()).pw_name
        config.write_text("\n".join((f"Port {port}", "ListenAddress 127.0.0.1",
            f"HostKey {folder / 'host'}", f"AuthorizedKeysFile {trust}", f"PidFile {folder / 'pid'}",
            "PubkeyAuthentication yes", "PasswordAuthentication no", "KbdInteractiveAuthentication no",
            "UsePAM no", "StrictModes yes", "PermitTTY no", "DisableForwarding yes",
            "PermitUserRC no", f"SetEnv XDG_RUNTIME_DIR={runtime}", f"AllowUsers {user}")) + "\n")
        checked("/usr/sbin/sshd", "-t", "-f", str(config))
        server = subprocess.Popen(["/usr/sbin/sshd", "-D", "-f", str(config), "-E", str(folder / "sshd.log")])
        audit = folder / "peer.log"
        audit_file = audit.open("w")
        peer = subprocess.Popen([str(PEER), str(native_socket)], stdout=audit_file, stderr=audit_file)
        control = folder / "control"
        base = ["ssh", "-T", "-F", "/dev/null", "-o", "BatchMode=yes", "-o", "IdentitiesOnly=yes",
                "-o", "StrictHostKeyChecking=yes", "-o", f"UserKnownHostsFile={known}",
                "-o", "ConnectTimeout=4", "-i", str(folder / "compute"), "-p", str(port)]

        def exchange(request, key="compute", extra=()):
            command = base.copy()
            command[command.index("-i") + 1] = str(folder / key)
            if extra:
                command[1:1] = list(extra)
            wire = request if isinstance(request, str) else json.dumps(request, ensure_ascii=False) + "\n"
            run = subprocess.run(command + [f"{user}@127.0.0.1"], input=wire,
                                 capture_output=True, text=True, timeout=15)
            assert run.returncode == 0, run.stderr
            assert "\x1b" not in run.stdout
            response = json.loads(run.stdout)
            if key == "management":
                assert response["schema"] == "yvex.management.response.v1" and response["status"] == "refused"
                return response
            layout = next(x for x in RESPONSE["oneOf"] if x["properties"]["status"]["const"] == response["status"])
            assert set(response) == set(layout["required"]), response
            if response["status"] == "ok":
                assert set(response["result"]) == set(RESPONSE["$defs"]["result"]["required"])
                for candidate in response["result"]["candidates"]:
                    assert set(candidate) == {"id", "raw_score", "relative_candidate_probability"}
            return response

        def expect(condition):
            global COUNT
            assert condition
            COUNT += 1

        request = {"schema": "yvex.finite.request.v1", "request_id": "1" * 64,
                   "operation": "finite.decision.execute", "input": {"model_alias": "finite",
                   "expected_generation": 9, "question": "Quale?", "context": "π",
                   "candidates": [{"id": "a", "text": "Uno"}, {"id": "b", "text": "Due"}]}}
        try:
            wait_for(lambda: native_socket.exists())
            wait_for(lambda: subprocess.run(["ssh-keyscan", "-T", "1", "-p", str(port), "127.0.0.1"],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0)
            result = exchange(request)
            assert result["status"] == "ok", result
            expect(result["status"] == "ok" and result["dispatch_state"] == "completed")
            expect(result["request_id"] == request["request_id"] and result["model_alias"] == "finite")
            expect(result["device_identity"] == identities["host"] and result["authenticated_peer"] == identities["compute"])
            body = result["result"]
            expect(body["engine_generation"] == 9 and [c["id"] for c in body["candidates"]] == ["a", "b"])
            expect(body["model_forward_count"] == 1 and body["sampling_invocation_count"] == 0
                   and body["generated_token_count"] == 0 and body["calibrated"] is False)
            expect(body["result_identity"] == "a" * 64 and body["candidates"][1]["raw_score"] == 1.0)
            wait_for(lambda: "CLOSED" in audit.read_text())
            dispatch_before = audit.read_text().count("DISPATCH")
            malformed = []
            for mutation in ("unknown", "schema", "generation", "population", "length", "nul", "operation"):
                bad = copy.deepcopy(request)
                if mutation == "unknown": bad["input"]["extra"] = True
                if mutation == "schema": bad["schema"] = "other"
                if mutation == "generation": bad["input"]["expected_generation"] = 0
                if mutation == "population": bad["input"]["candidates"][1]["id"] = "a"
                if mutation == "length": bad["input"]["question"] = "x" * 256
                if mutation == "nul": bad["input"]["context"] = "\0"
                if mutation == "operation": bad["operation"] = "model.load"
                malformed.append(bad)
            malformed += [json.dumps(request).replace('"schema":', '"schema":"duplicate","schema":') + "\n",
                          " " * 32769 + "\n", json.dumps(request)]
            # A missing LF requires EOF; SSH's stdin relay supplies it here.
            for bad in malformed:
                refused = exchange(bad)
                expect(refused["status"] == "refused" and refused["dispatch_state"] == "not_dispatched"
                       and "result" not in refused)
            expect(audit.read_text().count("DISPATCH") == dispatch_before)
            for question, generation, name in (("refuse", 9, "YVEX_ERR_STATE"),
                    ("Quale?", 8, "YVEX_ERR_STATE"), ("foreign-generation", 9, "YVEX_ERR_STATE"),
                    ("foreign-population", 9, "YVEX_ERR_FORMAT")):
                bad = copy.deepcopy(request)
                bad["input"].update(question=question, expected_generation=generation)
                observed = exchange(bad)
                expect(observed["status"] == "error" and "result" not in observed
                       and observed["error"]["name"] == name and observed["dispatch_state"] == "outcome_unavailable")
            # Lose a dispatched transport; never retry or manufacture a result.
            before = audit.read_text().count("DISPATCH")
            closed = audit.read_text().count("CLOSED")
            delayed = copy.deepcopy(request)
            delayed["input"]["question"] = "delay"
            child = subprocess.Popen(base + [f"{user}@127.0.0.1"], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                child.stdin.write(json.dumps(delayed) + "\n")
                child.stdin.flush()
                wait_for(lambda: audit.read_text().count("DISPATCH") > before)
            finally:
                child.terminate()
                child.communicate(timeout=5)
            wait_for(lambda: audit.read_text().count("CLOSED") > closed)
            expect(audit.read_text().count("DISPATCH") == before + 1)
            expect(exchange(request)["status"] == "ok")
            expect(exchange(request, key="management")["status"] == "refused")
            expect(exchange({"schema": "yvex.management.request.v1", "request_id": "2" * 64,
                             "operation": "host.status"})["status"] == "refused")
            command = subprocess.run(base + [f"{user}@127.0.0.1", "uname"],
                                     input=json.dumps(request) + "\n", text=True, capture_output=True, timeout=10)
            expect(json.loads(command.stdout)["reason"] == "restricted_ssh_required")
            unknown = base.copy()
            unknown[unknown.index("-i") + 1] = str(folder / "unknown")
            expect(subprocess.run(unknown + [f"{user}@127.0.0.1"], input="\n", text=True,
                   capture_output=True, timeout=10).returncode != 0)
            expect(exchange(request, extra=("-o", "RequestTTY=force"))["status"] == "ok")
            # PermitTTY=no prevents a TTY; it does not change the typed computation.
            checked(*(base + ["-M", "-N", "-f", "-S", str(control), f"{user}@127.0.0.1"]))
            checked(str(BIN), "management", "revoke", identities["compute"].split(":")[-1], str(trust))
            revoked = exchange(request, extra=("-S", str(control), "-o", "ControlMaster=no"))
            expect(revoked["status"] == "refused" and revoked["dispatch_state"] == "not_dispatched")
            enroll("compute", "management")
            revoked = exchange(request, extra=("-S", str(control), "-o", "ControlMaster=no"))
            expect(revoked["status"] == "refused" and revoked["dispatch_state"] == "not_dispatched")
            checked(str(BIN), "management", "revoke", identities["compute"].split(":")[-1], str(trust))
            enroll("compute", "finite-decision")
            expect(exchange(request)["status"] == "ok")
            wrong_host = known.read_text().replace((folder / "host.pub").read_text().strip(),
                                                   (folder / "unknown.pub").read_text().strip())
            known.write_text(wrong_host)
            expect(subprocess.run(base + [f"{user}@127.0.0.1"], input="\n", text=True,
                   capture_output=True, timeout=10).returncode != 0)
        except Exception:
            print((folder / "sshd.log").read_text()[-4000:])
            print(audit.read_text()[-4000:])
            raise
        finally:
            if control.exists():
                subprocess.run(base + ["-O", "exit", "-S", str(control), f"{user}@127.0.0.1"],
                               capture_output=True, timeout=5)
            for child in (peer, server):
                child.terminate()
                child.wait(timeout=5)
            audit_file.close()
        print(f"PASS finite_remote controls={COUNT} transport=restricted-ssh native=public-C-client oracle=synthetic-peer")


if __name__ == "__main__":
    main()
