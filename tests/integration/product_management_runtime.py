#!/usr/bin/env python3
"""Real isolated SSH + producer + SDK + native zero-engine Host. No real model claim."""
import base64
import hashlib
import json
import os
from pathlib import Path
import pwd
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get('YVEX_BIN', ROOT / 'yvex')).resolve()
SDK = Path(os.environ['YVEX_SDK_PRODUCT_EXAMPLE']).resolve()


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, capture_output=True, text=True, timeout=30, **kwargs)


def identity(path):
    return 'ssh-ed25519:sha256:' + hashlib.sha256(base64.b64decode(path.read_text().split()[1])).hexdigest()


def wait(predicate):
    for _ in range(100):
        if predicate():
            return
        time.sleep(.05)
    raise AssertionError('isolated owner did not become ready')


def main():
    with tempfile.TemporaryDirectory(prefix='yvex-product-runtime-') as directory:
        tmp = Path(directory)
        for name in ['runtime', 'data', 'config', 'models', 'xdg']:
            (tmp / name).mkdir(mode=0o700)
        env = {**os.environ, 'XDG_RUNTIME_DIR': str(tmp / 'runtime'), 'XDG_DATA_HOME': str(tmp / 'xdg'),
               'YVEX_DATA_DIR': str(tmp / 'data'), 'YVEX_CONFIG_DIR': str(tmp / 'config'),
               'YVEX_MODELS_ROOT': str(tmp / 'models')}
        for name in ['host', 'product', 'readonly']:
            run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', tmp / name])
        device, peer = identity(tmp / 'host.pub'), identity(tmp / 'product.pub')
        trust = tmp / 'authorized_keys'
        run([BINARY, 'management', 'trust-init', trust], env=env)
        for name, scope in [('product', 'product-management'), ('readonly', 'management')]:
            run([BINARY, 'management', 'enroll', tmp / f'{name}.pub', trust, tmp / 'host.pub',
                 identity(tmp / f'{name}.pub').split(':')[-1], '--scope', scope], env=env)
        with socket.socket() as reserved:
            reserved.bind(('127.0.0.1', 0))
            port = reserved.getsockname()[1]
        user = pwd.getpwuid(os.getuid()).pw_name
        config = [f'Port {port}', 'ListenAddress 127.0.0.1', f'HostKey {tmp / "host"}',
                  f'AuthorizedKeysFile {trust}', f'PidFile {tmp / "sshd.pid"}', f'AllowUsers {user}',
                  'AuthenticationMethods publickey', 'PubkeyAuthentication yes', 'PasswordAuthentication no',
                  'KbdInteractiveAuthentication no', 'UsePAM no', 'GSSAPIAuthentication no', 'StrictModes yes',
                  'PermitTTY no', 'DisableForwarding yes', 'PermitUserRC no', 'PermitUserEnvironment no']
        owned_env = ['XDG_RUNTIME_DIR', 'XDG_DATA_HOME', 'YVEX_DATA_DIR', 'YVEX_CONFIG_DIR', 'YVEX_MODELS_ROOT']
        config.append('SetEnv ' + ' '.join(f'{key}={env[key]}' for key in owned_env))
        (tmp / 'sshd.conf').write_text('\n'.join(config) + '\n')
        (tmp / 'pins').write_text(f'[127.0.0.1]:{port} ' + ' '.join((tmp / 'host.pub').read_text().split()[:2]) + '\n')
        run([shutil.which('sshd'), '-t', '-f', tmp / 'sshd.conf'])
        effective = run([shutil.which('sshd'), '-T', '-f', tmp / 'sshd.conf']).stdout
        forwarded = [item for line in effective.splitlines() if line.lower().startswith('setenv ')
                         for item in line.split()[1:]]
        assert all(f'{key}={env[key]}' in forwarded for key in owned_env), 'incomplete fixture isolation'
        server = subprocess.Popen([shutil.which('sshd'), '-D', '-f', str(tmp / 'sshd.conf'), '-E', str(tmp / 'ssh.log')])
        host = None
        try:
            def reachable():
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=.1): return True
                except OSError: return False
            wait(reachable)
            sequence = 0

            def request(operation, data, expected=True, key='product'):
                nonlocal sequence
                sequence += 1
                req = {'schema': 'yvex.management.request.v2', 'request_id': hashlib.sha256(f'runtime-{sequence}'.encode()).hexdigest(), 'operation': operation, 'input': data}
                (tmp / 'request.json').write_text(json.dumps(req))
                result = subprocess.run([str(SDK), str(tmp / 'pins'), str(tmp / key), '127.0.0.1', str(port), user,
                                         device, identity(tmp / f'{key}.pub'), str(tmp / 'request.json'), '5000'], capture_output=True, text=True, timeout=8)
                value = json.loads(result.stdout)
                assert (result.returncode == 0) == expected, (operation, value, result.stderr)
                if expected:
                    assert value['request_id'] == req['request_id']
                    assert value['authenticated_peer'] == identity(tmp / f'{key}.pub')
                    return value['value']
                return value

            assert request('host.get', {})['state'] == 'stopped'
            capabilities = request('management.capabilities', {})
            assert len(capabilities['operations']) >= 31 and not capabilities.get('training', True)
            assert request('model.list', {})['models'] == []
            assert request('job.list', {})['jobs'] == []
            assert request('host.get', {}, False, 'readonly')['code'] != ''

            def start_host():
                proc = subprocess.Popen([str(BINARY), 'serve', '--openai', 'off', '--logs', 'off'], env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                wait(lambda: (tmp / 'runtime/yvex/yvexd.sock').exists())
                assert proc.poll() is None
                return proc

            host = start_host()
            first = request('host.get', {})
            assert first['state'] == 'running' and len(first['host_instance']) == 64
            assert request('host.get', {})['host_instance'] == first['host_instance']
            assert request('engine.list', {})['engines'] == []
            events = request('observe.events', {'limit': 32})
            assert events['host_instance'] == first['host_instance']
            host.terminate(); host.wait(timeout=5); host = None
            wait(lambda: not (tmp / 'runtime/yvex/yvexd.sock').exists())
            host = start_host()
            second = request('host.get', {})
            assert second['host_instance'] != first['host_instance']
            accepted = request('engine.load', {'host_instance': first['host_instance'], 'profile': 'nonexistent-fixture'})
            def settled():
                receipt = request('job.get', {'job_id': accepted['job_id']})
                return receipt if receipt['state'] not in ['accepted', 'running'] else None
            receipt = None
            for _ in range(50):
                receipt = settled()
                if receipt: break
                time.sleep(.05)
            assert receipt and receipt['state'] == 'failed' and 'stale_host_instance' in receipt['reason'], receipt
            receipt_path = tmp / 'xdg/yvex/management-jobs-v2' / peer.split(':')[-1] / f'{accepted["job_id"]}.json'
            assert receipt_path.is_file(), 'receipt escaped the declared fixture data root'
            assert request('engine.list', {})['engines'] == []
            assert 'input' not in request('job.list', {})['jobs'][0]
            run([BINARY, 'management', 'revoke', peer.split(':')[-1], trust], env=env)
            assert request('host.get', {}, False)['code'] != ''
            print(json.dumps({'evidence': 'real-isolated-SSH-producer-SDK-native-zero-engine-Host', 'passed': True,
                'controls': ['explicit management grant', 'v1 cannot mutate', 'empty model library', 'zero-engine Host',
                             'stable live Host identity', 'new identity after restart', 'stale mutation has zero engine effects',
                             'exact job recovery', 'bounded redacted job list', 'peer revoke'],
                'not_claimed': ['real model inference', 'Spark', 'human acceptance']}))
        finally:
            if host and host.poll() is None: host.terminate(); host.wait(timeout=5)
            server.terminate(); server.wait(timeout=5)


if __name__ == '__main__':
    main()
