#!/usr/bin/env python3
"""Keep an isolated real YVEX management service available for native GUI qualification.
No model, operator profile, fixed port, or automatic remote approval is used.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', required=True, type=Path)
parser.add_argument('--root', required=True, type=Path)
args = parser.parse_args()
root = args.root.resolve()
root.mkdir(mode=0o700, exist_ok=False)
for name in ['config', 'data', 'models', 'runtime', 'xdg', 'home', 'profiles']:
    (root / name).mkdir(mode=0o700)
env = {**os.environ, 'HOME': str(root / 'home'), 'YVEX_CONFIG_DIR': str(root / 'config'),
       'YVEX_DATA_DIR': str(root / 'data'), 'YVEX_MODELS_ROOT': str(root / 'models'),
       'YVEX_MODELS_REGISTRY': str(root / 'config/models.local.json'),
       'XDG_RUNTIME_DIR': str(root / 'runtime'), 'XDG_DATA_HOME': str(root / 'xdg')}
for key in ['HF_TOKEN', 'HUGGING_FACE_HUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN']:
    env.pop(key, None)
binary = args.binary.resolve()
state = root / 'service'
with socket.socket() as reservation:
    reservation.bind(('127.0.0.1', 0))
    inference_port = reservation.getsockname()[1]
def cli(*argv):
    return subprocess.run([str(binary), 'management', *argv, '--state-dir', str(state)],
                          env=env, check=True, capture_output=True, text=True, timeout=20)
cli('host-configure', str(inference_port))
log = (root / 'management.log').open('w')
service = subprocess.Popen([str(binary), 'management', 'serve', '--bind', '127.0.0.1:0',
                           '--name', 'Disposable Studio lifecycle server', '--state-dir', str(state)],
                          env=env, stdout=subprocess.PIPE, stderr=log, text=True)
stop = False
def finish(signum, frame):
    global stop
    stop = True
signal.signal(signal.SIGINT, finish)
signal.signal(signal.SIGTERM, finish)
try:
    info = json.loads(service.stdout.readline())
    endpoint = 'https://' + info['address']
    invitation = root / 'owner-invitation.json'
    cli('owner-invite', '--endpoint', endpoint, '--output', str(invitation))
    handoff = {**info, 'endpoint': endpoint, 'inference_port': inference_port,
               'owner_invitation_file': str(invitation), 'state_dir': str(state),
               'studio_profile_root': str(root / 'profiles'), 'operator_state_touched': False,
               'scope': 'real zero-engine disposable Host; no model compatibility claim'}
    (root / 'handoff.json').write_text(json.dumps(handoff, indent=2) + '\n')
    print(json.dumps(handoff), flush=True)
    while not stop and service.poll() is None:
        time.sleep(.2)
finally:
    # This process owns the isolated socket namespace; never addresses an operator Host.
    subprocess.run([str(binary), 'host', 'stop'], env=env, capture_output=True, timeout=30)
    service.terminate()
    service.wait(timeout=15)
    log.close()
