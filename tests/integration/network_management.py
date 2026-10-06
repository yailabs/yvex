#!/usr/bin/env python3
"""Generated loopback TLS identities and same-user UDS; no operator deployment claim."""
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import ssl
import subprocess
import tempfile
import time

BINARY = Path(os.environ.get('YVEX_BIN', 'yvex')).resolve()


def main():
    with tempfile.TemporaryDirectory(prefix='yvex-net-') as directory:
        root = Path(directory)
        root.chmod(0o700)
        for name in ['config', 'data', 'models', 'runtime', 'xdg', 'home']:
            (root / name).mkdir(mode=0o700)
        env = {**os.environ, 'HOME': str(root / 'home'),
               'YVEX_CONFIG_DIR': str(root / 'config'), 'YVEX_DATA_DIR': str(root / 'data'),
               'YVEX_MODELS_ROOT': str(root / 'models'),
               'YVEX_MODELS_REGISTRY': str(root / 'config/models.local.json'),
               'XDG_RUNTIME_DIR': str(root / 'runtime'), 'XDG_DATA_HOME': str(root / 'xdg')}
        for key in ['HF_TOKEN', 'HUGGING_FACE_HUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN']:
            env.pop(key, None)
        state = root / 'service'
        process = None
        host_process = None
        certificate = None
        server = None
        service_socket = None

        def command(action, *args, expected=0):
            result = subprocess.run([str(BINARY), 'management', action, *args,
                                     '--state-dir', str(state)], env=env,
                                    text=True, capture_output=True, timeout=20)
            assert result.returncode == expected, (action, result.returncode, result.stderr)
            return json.loads(result.stdout) if expected == 0 else result

        def start():
            nonlocal process, certificate, server, service_socket
            process = subprocess.Popen([str(BINARY), 'management', 'serve', '--bind',
                                        '127.0.0.1:0', '--name', 'Generated fixture YVEX',
                                        '--state-dir', str(state)], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            line = process.stdout.readline()
            assert line, process.stderr.read()
            info = json.loads(line)
            server = ('127.0.0.1', int(info['address'].rsplit(':', 1)[1]))
            service_socket = info['local_socket']
            certificate = info['device_identity']
            assert info['discovery_advertised'] is False
            assert Path(service_socket).stat().st_mode & 0o777 == 0o600
            return info

        def stop():
            nonlocal process
            if process is not None:
                process.terminate()
                process.wait(timeout=15)
                assert not process.stderr.read(), 'No request or credential diagnostics permitted'
                process = None

        def connect(local=False):
            if local:
                client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                client.settimeout(15)
                client.connect(service_socket)
                return client
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            client = context.wrap_socket(socket.create_connection(server, 5), server_hostname='localhost')
            client.settimeout(15)
            # Fixture oracle explicitly pins before any bearer/request is sent.
            observed = 'tls-sha256:' + hashlib.sha256(client.getpeercert(binary_form=True)).hexdigest()
            assert observed == certificate
            return client

        def exchange(method, path, data=None, credential=None, local=False, extra=''):
            body = json.dumps(data).encode() if data is not None else b''
            auth = f'Authorization: Bearer {credential}\r\n' if credential else ''
            media = 'Content-Type: application/json\r\n' if method == 'POST' else ''
            if local and path == '/v1/management':
                media += f'X-Yvex-Device-Identity: {certificate}\r\n'
            request = (f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n{auth}{media}'
                       f'Content-Length: {len(body)}\r\n{extra}\r\n').encode() + body
            with connect(local) as client:
                client.sendall(request)
                chunks = []
                while True:
                    value = client.recv(8192)
                    if not value:
                        break
                    chunks.append(value)
            header, result = b''.join(chunks).split(b'\r\n\r\n', 1)
            return int(header.split(b' ', 2)[1]), json.loads(result)

        def operation(name, data=None, credential=None, local=False, identity=None):
            request = {'schema': 'yvex.management.request.v2',
                       'request_id': identity or secrets.token_hex(32),
                       'operation': name, 'input': data or {}}
            return exchange('POST', '/v1/management', request, credential, local)

        try:
            start()
            status, public = exchange('GET', '/v1/identity')
            assert status == 200 and public['pairing_available'] is False
            assert set(public) == {'schema', 'device_identity', 'display_name', 'protocol', 'pairing_available'}
            assert operation('management.capabilities')[0] == 403
            raw = secrets.token_bytes(32)
            token = raw.hex()
            digest = hashlib.sha256(raw).hexdigest()
            pairing = {'schema': 'yvex.management.pairing.request.v1',
                       'credential_hash': digest, 'client_name': 'Generated test client'}
            assert exchange('POST', '/v1/pairing', pairing)[1]['reason'] == 'pairing_window_closed'
            command('pairing-open')
            assert exchange('POST', '/v1/pairing', pairing)[1]['posture'] == 'pending'
            assert exchange('POST', '/v1/pairing', pairing)[1]['request_id'] == digest
            assert operation('management.capabilities', credential=token)[0] == 403
            assert exchange('GET', '/v1/pairing/status', credential=token)[1]['posture'] == 'pending'
            assert len(command('pairing-list')['peers']) == 1
            command('pairing-approve', digest)
            assert exchange('GET', '/v1/pairing/status', credential=token)[1]['posture'] == 'approved'
            status, response = operation('management.capabilities', credential=token)
            assert status == 200 and response['status'] == 'ok'
            assert response['device_identity'] == certificate
            assert response['authenticated_peer'] == 'credential-sha256:' + digest
            assert len(response['data']['operations']) == 36
            assert operation('management.capabilities', credential=secrets.token_hex(32))[0] == 403
            status, response = operation('management.capabilities', local=True)
            assert status == 200 and response['authenticated_peer'] == f'local-user:{os.geteuid()}'
            assert len(response['data']['operations']) == 36
            assert exchange('POST', '/v1/management', {}, token, extra='X-Yvex-Device-Identity: tls-sha256:'+'0'*64+'\r\n')[1]['reason'] == 'stale_management_device_identity'
            for extra in ['Origin: http://untrusted.invalid\r\n', 'Transfer-Encoding: chunked\r\n',
                          'Authorization: Bearer duplicate\r\n', 'Host: duplicate\r\n',
                          'Content-Length: 0\r\n']:
                assert exchange('GET', '/v1/identity', credential=token, extra=extra)[0] == 403
            assert operation('model.list', {'token': 'not-an-input'}, credential=token)[1]['status'] == 'refused'
            # Same persistent management service keeps historical Host truth separately from admission.
            assert operation('host.get', credential=token)[1]['data']['last_known'] is None
            with socket.socket() as reservation:
                reservation.bind(('127.0.0.1', 0))
                host_port = reservation.getsockname()[1]
            host_process = subprocess.Popen([str(BINARY), 'serve', '--workers', '1', '--openai-port', str(host_port)],
                                            env=env, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                live = operation('host.get', credential=token)[1]['data']
                if live['state'] == 'running':
                    break
                assert host_process.poll() is None, host_process.stderr.read()
                time.sleep(.03)
            assert live['state'] == 'running' and live['host_instance'] is not None
            assert live['last_known'] is None
            stopped = subprocess.run([str(BINARY), 'host', 'stop'], env=env, text=True,
                                     capture_output=True, timeout=20)
            assert stopped.returncode == 0, stopped.stderr
            host_process.wait(timeout=15)
            host_process = None
            historical = operation('host.get', credential=token)[1]['data']
            assert historical['state'] == 'stopped'
            assert historical['host_instance'] is None and historical['status'] is None
            assert historical['last_known']['host_instance'] == live['host_instance']
            assert historical['last_known']['status'] == live['status']
            assert historical['last_known']['observed_at_unix_ms'] > 0
            assert operation('engine.list', credential=token)[1]['status'] != 'ok'
            _, refused_load = operation('engine.load', {'host_instance': live['host_instance'],
                'profile': 'missing-fixture'}, credential=token)
            for _ in range(100):
                _, load_receipt = operation('job.get', {'job_id': refused_load['data']['job_id']}, token)
                if load_receipt['data']['state'] not in ['accepted', 'running']:
                    break
                time.sleep(.02)
            assert load_receipt['data']['state'] == 'failed'
            assert operation('host.get', credential=token)[1]['data']['last_known'] == historical['last_known']
            job_identity = secrets.token_hex(32)
            # Existing source owner refuses a missing exact model; receipt still provides recovery.
            _, submitted = operation('source.verify', {'source': 'missing-fixture'}, token,
                                     identity=job_identity)
            assert submitted['status'] == 'ok', submitted
            for _ in range(100):
                _, receipt = operation('job.get', {'job_id': job_identity}, token)
                if receipt['data']['state'] not in ['accepted', 'running']:
                    break
                time.sleep(.05)
            assert receipt['data']['state'] == 'failed', receipt
            assert operation('job.get', {'job_id': job_identity}, local=True)[1]['status'] != 'ok'
            old_certificate = certificate
            stop()
            start()
            assert certificate == old_certificate
            assert operation('job.get', {'job_id': job_identity}, token)[1]['data']['job_id'] == job_identity
            assert operation('management.capabilities', credential=token)[1]['status'] == 'ok'
            command('pairing-revoke', digest)
            assert operation('management.capabilities', credential=token)[0] == 403
            assert exchange('GET', '/v1/pairing/status', credential=token)[1]['posture'] == 'revoked'
            assert operation('management.capabilities', local=True)[1]['status'] == 'ok'
            for path in state.rglob('*'):
                if path.is_file():
                    assert token.encode() not in path.read_bytes()
            # Pending TLS connections consume at most8 workers and expire at the absolute deadline.
            idle = [socket.create_connection(server, 5) for _ in range(8)]
            time.sleep(.3)
            overflow = socket.create_connection(server, 5)
            overflow.settimeout(2)
            assert overflow.recv(1) == b''
            overflow.close()
            time.sleep(10.1)
            for client in idle:
                client.settimeout(2)
                assert client.recv(1) == b''
                client.close()
            assert exchange('GET', '/v1/identity')[0] == 200
            stop()
            assert not Path(service_socket).exists()
            # Unsafe persisted identity cannot be loaded or silently replaced.
            (state / 'identity.json').chmod(0o644)
            command('network-identity', expected=2)
            print(json.dumps({'result': 'PASS', 'evidence_class': 'generated_loopback_TLS_and_same_user_UDS',
                              'controls': ['public identity only', 'closed/pending/approved pairing',
                                           'strict framing', 'wrong credential', 'private stable TLS identity',
                                           '36 shared operations', 'last-known Host is historical, never admission', 'durable receipt restart',
                                           'transport-separated receipt access', 'remote revoke/local continuity',
                                           '8-connection bound', 'absolute TLS deadline', 'unsafe storage refusal'],
                              'operator_state_touched': False}))
        finally:
            if host_process is not None:
                host_process.terminate()
                host_process.wait(timeout=15)
            stop()


if __name__ == '__main__':
    main()
