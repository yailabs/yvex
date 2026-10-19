#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""SDK against real isolated SSH, synthetic scores. NEVER real YVEX qualification."""
import base64
import hashlib
import json
import os
from pathlib import Path
import pwd
import shlex
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
SDK = ROOT / "sdk/rust"

def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, **kwargs)

def key_identity(path):
    blob = path.read_text().split()[1]
    return "ssh-ed25519:sha256:" + hashlib.sha256(base64.b64decode(blob)).hexdigest()

def main():
    sshd = shutil.which("sshd")
    assert sshd and shutil.which("ssh-keygen"), "Explicit SSH lane requires installed OpenSSH client/server"
    run(["cargo", "build", "--locked", "-p", "yvex-sdk@0.2.0", "--example", "finite_remote"], cwd=SDK)
    target = Path(os.environ.get("CARGO_TARGET_DIR", SDK / "target"))
    executable = target / "debug/examples/finite_remote"
    # Keep the forced-command key under a private user-owned ancestry.
    with tempfile.TemporaryDirectory(prefix=".yvex-sdk-finite-ssh-", dir=Path.home()) as directory:
        tmp = Path(directory)
        for name in ["host", "client", "wrong"]:
            run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-f", str(tmp/name)])
        device, peer = key_identity(tmp/"host.pub"), key_identity(tmp/"client.pub")
        identity = {field: char*64 for field,char in zip([
            "source_identity", "logical_model_identity", "binding_identity", "tokenizer_identity",
            "physical_program_identity", "input_policy_identity"], "cdef12")}
        (tmp/"identity.json").write_text(json.dumps(identity))
        (tmp/"mode").write_text("valid")
        (tmp/"audit").write_text("")
        request = {"model_alias":"SDK_SYNTHETIC_ONLY", "expected_generation":7, "question":"bounded choice",
            "context":"No Case content or model execution", "candidates":[{"id":"one", "text":"First"}, {"id":"two", "text":"Second"}]}
        (tmp/"request.json").write_text(json.dumps(request))
        command = shlex.join([shutil.which("python3"), str(ROOT/"tests/fixtures/sdk/finite_remote_peer.py"),
            str(tmp/"identity.json"), str(tmp/"mode"), str(tmp/"audit"), device, peer])
        assert '"' not in command, "Fixture path cannot be encoded safely in forced command"
        (tmp/"authorized_keys").write_text(f'restrict,command="{command}" ' + (tmp/"client.pub").read_text())
        (tmp/"authorized_keys").chmod(0o600)
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1",0))
            port = reservation.getsockname()[1]
        user = pwd.getpwuid(os.getuid()).pw_name
        config = f"""Port {port}
ListenAddress 127.0.0.1
HostKey {tmp/'host'}
PidFile {tmp/'pid'}
AuthorizedKeysFile {tmp/'authorized_keys'}
AllowUsers {user}
HostKeyAlgorithms ssh-ed25519
PubkeyAcceptedAlgorithms ssh-ed25519
PubkeyAuthentication yes
AuthenticationMethods publickey
PasswordAuthentication no
KbdInteractiveAuthentication no
GSSAPIAuthentication no
UsePAM no
StrictModes yes
PermitTTY no
DisableForwarding yes
PermitUserRC no
PermitUserEnvironment no
MaxSessions 1
"""
        (tmp/"sshd.conf").write_text(config)
        run([sshd, "-t", "-f", str(tmp/"sshd.conf")])
        host_key = " ".join((tmp/"host.pub").read_text().split()[:2])
        (tmp/"pins").write_text(f"[127.0.0.1]:{port} {host_key}\n")
        with (tmp/"sshd.log").open("w+") as log:
            server = subprocess.Popen([sshd,"-D","-e","-f",str(tmp/"sshd.conf")], stdout=log, stderr=log)
            try:
                for _ in range(50):
                    if server.poll() is not None:
                        log.seek(0); raise AssertionError(log.read())
                    try:
                        with socket.create_connection(("127.0.0.1",port), timeout=0.1): break
                    except OSError: time.sleep(0.02)
                else: raise AssertionError("isolated listener did not start")
                args = [str(executable), str(tmp/"pins"), str(tmp/"client"), "127.0.0.1", str(port), user,
                    device, peer, str(tmp/"identity.json"), str(tmp/"request.json"), "3000"]
                seen = set()
                for mode in ["valid", "stale", "foreign_model", "foreign_candidate", "reordered", "unsupported",
                    "unknown_field", "wrong_correlation", "wrong_device", "wrong_peer", "refused", "producer_error", "lost", "oversized", "timeout"]:
                    (tmp/"mode").write_text(mode)
                    before = len((tmp/"audit").read_text().splitlines())
                    command_args = args if mode != "timeout" else args[:-1] + ["500"]
                    result = subprocess.run(command_args, capture_output=True, text=True, timeout=5)
                    after = (tmp/"audit").read_text().splitlines()
                    assert len(after) == before+1, (mode, "zero/multiple dispatch", result.stderr)
                    assert after[-1] not in seen; seen.add(after[-1])
                    assert "DO_NOT_LOG_THIS_SYNTHETIC_CONTEXT" not in result.stderr
                    assert (result.returncode == 0) == (mode == "valid"), (mode, result.stderr)
                    if mode == "valid":
                        observed = json.loads(result.stdout)
                        assert observed["request_id"] == after[-1]
                        assert observed["result"]["engine_generation"] == 7
                        assert [c["id"] for c in observed["result"]["candidates"]] == ["one","two"]
                        assert not observed["result"]["calibrated"]
                    elif mode == "refused": assert "NotDispatched" in result.stderr
                    else: assert "OutcomeUnavailable" in result.stderr
                # Wrong pin/unenrolled key must fail SSH with no peer invocation.
                before = len((tmp/"audit").read_text().splitlines())
                wrong_key = " ".join((tmp/"wrong.pub").read_text().split()[:2])
                (tmp/"bad_pins").write_text(f"[127.0.0.1]:{port} {wrong_key}\n")
                for changed in [args[:1] + [str(tmp/"bad_pins")] + args[2:], args[:2] + [str(tmp/"wrong")] + args[3:]]:
                    refused = subprocess.run(changed, capture_output=True, text=True, timeout=5)
                    assert refused.returncode != 0 and "TransportUnavailable" in refused.stderr
                    assert len((tmp/"audit").read_text().splitlines()) == before
                print("PASS SDK remote finite: 15 real-SSH synthetic protocol controls, wrong pin/unenrolled key refused, one dispatch per invocation; NO YVEX MODEL/Fast Search qualification")
            finally:
                server.terminate()
                try: server.wait(timeout=2)
                except subprocess.TimeoutExpired: server.kill(); server.wait(timeout=2)

if __name__ == "__main__": main()
