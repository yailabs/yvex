#!/usr/bin/env python3
# Migrated from yailabs/yai-sdk 7a2a19b0 under MIT; see sdk/README.md.
"""SDK management over isolated SSH; synthetic jobs, not YVEX lifecycle qualification."""
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
    run(["cargo", "build", "--locked", "-p", "yvex-sdk@0.2.0", "--example", "management_product"], cwd=SDK)
    target = Path(os.environ.get("CARGO_TARGET_DIR", SDK / "target"))
    executable = target / "debug/examples/management_product"
    # StrictModes verifies ancestors of AuthorizedKeysFile; /tmp is shared.
    with tempfile.TemporaryDirectory(prefix=".yvex-sdk-management-ssh-", dir=Path.home()) as directory:
        tmp = Path(directory)
        for name in ["host", "client", "wrong"]:
            run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-f", str(tmp/name)])
        device, peer = key_identity(tmp/"host.pub"), key_identity(tmp/"client.pub")
        (tmp/"mode").write_text("valid")
        (tmp/"audit").write_text("")
        command = shlex.join([shutil.which("python3"), str(ROOT/"tests/fixtures/sdk/management_peer.py"),str(tmp),device,peer])
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
                    device, peer, str(tmp/"request.json"), "3000"]
                def invoke(operation, input, mode="valid", timeout="3000"):
                    request=dict(schema="yvex.management.request.v2",request_id=os.urandom(32).hex(),operation=operation,input=input)
                    (tmp/"request.json").write_text(json.dumps(request));(tmp/"mode").write_text(mode)
                    before=len((tmp/"audit").read_text().splitlines())
                    result=subprocess.run(args[:-1]+[timeout],capture_output=True,text=True,timeout=6)
                    if len((tmp/"audit").read_text().splitlines()) != before+1:
                        log.seek(0)
                        raise AssertionError((mode, result.stdout, result.stderr, log.read()))
                    assert "SYNTHETIC_PRIVATE_PROMPT" not in result.stderr
                    return request,result,json.loads(result.stdout)
                for mode in ["valid","wrong_correlation","wrong_device","wrong_peer","wrong_job","refused","unsupported","unavailable","lost","oversized","timeout"]:
                    request,result,value=invoke("engine.load",{"profile":"SYNTHETIC_ONLY","host_instance":"c"*64},mode,"500" if mode=="timeout" else "3000")
                    assert (result.returncode==0)==(mode=="valid"),(mode,result.stderr,value)
                    if mode=="valid": assert value["value"]["job_id"]==request["request_id"]
                    else: assert value["dispatch_state"]==("not_dispatched" if mode in ("refused","unsupported") else "outcome_unavailable"),(mode,value)
                # Receipt retained by the fake producer after a lost reply. Recovery
                # is a NEW read, not a second operational submission.
                lost,_,failure=invoke("generation.start",{"host_instance":"c"*64,"model":"synthetic","generation":1,"name":"test","session_identity":"d"*64,"prompt":"SYNTHETIC_PRIVATE_PROMPT","maximum_new_tokens":8,"reasoning":"disabled"},"lost")
                assert failure["dispatch_state"]=="outcome_unavailable"
                _,result,receipt=invoke("job.get",{"job_id":lost["request_id"]})
                assert result.returncode==0 and receipt["value"]["state"]=="succeeded"
                audited=[json.loads(line) for line in (tmp/"audit").read_text().splitlines()]
                assert sum(row["request_id"]==lost["request_id"] for row in audited)==1
                _,_,absent=invoke("job.get",{"job_id":"f"*64})
                assert absent["code"]=="refused" and absent["reason"]=="job_not_found"
                before=len((tmp/"audit").read_text().splitlines())
                wrong_key=" ".join((tmp/"wrong.pub").read_text().split()[:2])
                (tmp/"bad_pins").write_text(f"[127.0.0.1]:{port} {wrong_key}\n")
                for changed in [args[:1]+[str(tmp/"bad_pins")]+args[2:],args[:2]+[str(tmp/"wrong")]+args[3:]]:
                    result=subprocess.run(changed,capture_output=True,text=True,timeout=6)
                    assert result.returncode!=0 and json.loads(result.stdout)["dispatch_state"]=="outcome_unavailable"
                    assert len((tmp/"audit").read_text().splitlines())==before
                print("PASS SDK management v2: 11 real-SSH synthetic controls; lost reply recovered by exact job read without redispatch; wrong pin/key refused. NO YVEX runtime qualification")
            finally:
                server.terminate()
                try: server.wait(timeout=2)
                except subprocess.TimeoutExpired: server.kill(); server.wait(timeout=2)

if __name__ == "__main__": main()
