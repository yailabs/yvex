#!/usr/bin/env python3
"""Disposable pinned TLS ownership; optional real SDK/desktop vault, never operator grants."""
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
SDK = os.environ.get('YVEX_SDK_CONNECTIONS_EXAMPLE')


def main():
    with tempfile.TemporaryDirectory(prefix='yvex-owner-') as directory:
        root = Path(directory)
        root.chmod(0o700)
        for name in ['config', 'data', 'models', 'runtime', 'xdg', 'home', 'profiles']:
            (root / name).mkdir(mode=0o700)
        env = {**os.environ, 'HOME': str(root / 'home'),
               'YVEX_CONFIG_DIR': str(root / 'config'), 'YVEX_DATA_DIR': str(root / 'data'),
               'YVEX_MODELS_ROOT': str(root / 'models'),
               'YVEX_MODELS_REGISTRY': str(root / 'config/models.local.json'),
               'XDG_RUNTIME_DIR': str(root / 'runtime'), 'XDG_DATA_HOME': str(root / 'xdg')}
        for key in ['HF_TOKEN', 'HUGGING_FACE_HUB_TOKEN', 'GH_TOKEN', 'GITHUB_TOKEN']:
            env.pop(key, None)
        # Desktop credential service is the existing OS service; only generated test keys are written.
        sdk_env = {**os.environ, 'YVEX_SDK_CONNECTION_PROFILE_ROOT': str(root / 'profiles')}
        state = root / 'service'
        process = None
        server = None
        certificate = None
        owner_profiles, client_profiles = [], []

        def command(action, *args, expected=0):
            result = subprocess.run([str(BINARY), 'management', action, *args,
                                     '--state-dir', str(state)], env=env,
                                    text=True, capture_output=True, timeout=20)
            assert result.returncode == expected, (action, result.returncode, result.stderr)
            return json.loads(result.stdout) if result.stdout else None

        def sdk(action, *args, expected=0):
            result = subprocess.run([SDK, action, *args], env=sdk_env, text=True,
                                    capture_output=True, timeout=25)
            assert result.returncode == expected, (action, result.returncode, result.stdout, result.stderr)
            return json.loads(result.stdout)

        def start():
            nonlocal process, server, certificate
            process = subprocess.Popen([str(BINARY), 'management', 'serve', '--bind',
                                        '127.0.0.1:0', '--name', 'Headless ownership fixture',
                                        '--state-dir', str(state)], env=env,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            line = process.stdout.readline()
            assert line, process.stderr.read()
            info = json.loads(line)
            server = ('127.0.0.1', int(info['address'].rsplit(':', 1)[1]))
            certificate = info['device_identity']
            return 'https://' + info['address']

        def stop():
            nonlocal process
            if process:
                process.terminate()
                process.wait(timeout=15)
                assert not process.stderr.read(), 'No secret/request logs'
                process = None

        def exchange(method, path, data=None, credential=None, discard=False):
            body = json.dumps(data).encode() if data is not None else b''
            auth = f'Authorization: Bearer {credential}\r\n' if credential else ''
            media = 'Content-Type: application/json\r\n' if method == 'POST' else ''
            request = (f'{method} {path} HTTP/1.1\r\nHost: localhost\r\n{auth}{media}'
                       f'Content-Length: {len(body)}\r\n\r\n').encode() + body
            context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
            context.check_hostname = False
            context.verify_mode = ssl.CERT_NONE
            with context.wrap_socket(socket.create_connection(server, 5), server_hostname='localhost') as client:
                client.settimeout(15)
                assert 'tls-sha256:' + hashlib.sha256(client.getpeercert(binary_form=True)).hexdigest() == certificate
                client.sendall(request)
                # Lost caller acknowledgement: drain TLS only to guarantee producer completion,
                # deliberately discard outcome and recover by the exact retained request ID.
                chunks = []
                while True:
                    value = client.recv(8192)
                    if not value:
                        break
                    chunks.append(value)
            if discard:
                return None
            header, result = b''.join(chunks).split(b'\r\n\r\n', 1)
            return int(header.split(b' ', 2)[1]), json.loads(result)

        def operation(token):
            return exchange('POST', '/v1/management', {'schema': 'yvex.management.request.v2',
                'request_id': secrets.token_hex(32), 'operation': 'management.capabilities', 'input': {}}, token)

        try:
            endpoint = start()
            invitation = root / 'owner-invite.json'
            created = command('owner-invite', '--endpoint', endpoint, '--output', str(invitation))
            assert created['device_identity'] == certificate
            assert invitation.stat().st_mode & 0o777 == 0o600
            bundle = json.loads(invitation.read_text())
            assert bundle['invitation_secret'] not in json.dumps(created)
            owner_token = secrets.token_hex(32)
            owner_hash = hashlib.sha256(bytes.fromhex(owner_token)).hexdigest()
            bootstrap = {'schema': 'yvex.management.owner.bootstrap.v1',
                'invitation_secret': bundle['invitation_secret'], 'credential_hash': owner_hash,
                'client_name': 'Generated owner fixture'}
            assert exchange('POST', '/v1/owner/bootstrap', {**bootstrap, 'invitation_secret': secrets.token_hex(32)})[0] == 403
            if SDK:
                inspected = sdk('owner-inspect', str(invitation))
                assert 'invitation_secret' not in inspected
                profile = sdk('owner-prepare', str(invitation), 'Generated SDK owner', '--invitation-source-verified')
                owner_profiles.append(profile['profile_ref'])
                assert profile['posture'] == 'prepared'
                owner = sdk('owner-claim', profile['profile_ref'])
                assert owner['posture'] == 'approved' and owner['scope'] == 'pairing-administration'
                assert sdk('owner-status', profile['profile_ref'])['owner_ref'] == owner['owner_ref']
                assert sdk('list') == []
                assert len(sdk('owner-list')) == 1
                def read_admin():
                    return sdk('owner-connections', profile['profile_ref'])
                def action(kind, target=None, revision=None, request_id=None):
                    request = {'schema': 'yvex.management.owner.action.v1',
                        'request_id': request_id or secrets.token_hex(32), 'expected_revision':
                        read_admin()['revision'] if revision is None else revision,
                        'action': kind, 'target_ref': target}
                    request_file = root / 'action.json'
                    request_file.write_text(json.dumps(request))
                    result = sdk('owner-action', profile['profile_ref'], str(request_file))
                    assert sdk('owner-action-status', profile['profile_ref'], request['request_id']) == result
                    assert sdk('owner-action', profile['profile_ref'], str(request_file)) == result
                    return result
            else:
                _, owner = exchange('POST', '/v1/owner/bootstrap', bootstrap)
                assert owner['posture'] == 'approved' and owner['scope'] == 'pairing-administration'
                assert operation(owner_token)[0] == 403, 'Owner does not imply product grant'
                assert exchange('POST', '/v1/owner/bootstrap', {**bootstrap, 'credential_hash': 'f'*64})[0] == 403
                def read_admin():
                    status, result = exchange('GET', '/v1/owner/connections', credential=owner_token)
                    assert status == 200
                    return result
                def action(kind, target=None, revision=None, request_id=None):
                    request = {'schema': 'yvex.management.owner.action.v1',
                        'request_id': request_id or secrets.token_hex(32), 'expected_revision':
                        read_admin()['revision'] if revision is None else revision,
                        'action': kind, 'target_ref': target}
                    exchange('POST', '/v1/owner/actions', request, owner_token, discard=True)
                    status, result = exchange('GET', '/v1/owner/actions/' + request['request_id'], credential=owner_token)
                    assert status == 200
                    if kind != 'owner_revoke':
                        assert exchange('POST', '/v1/owner/actions', request, owner_token)[1] == result
                    return result
            assert action('pairing_open')['posture'] == 'applied'
            stale = action('pairing_open', revision=0)
            assert stale['posture'] == 'refused' and stale['reason'] == 'stale_owner_revision'
            raw = secrets.token_bytes(32)
            client_token = raw.hex()
            client_hash = hashlib.sha256(raw).hexdigest()
            pairing = {'schema': 'yvex.management.pairing.request.v1',
                'credential_hash': client_hash, 'client_name': 'Generated product client'}
            assert exchange('POST', '/v1/pairing', pairing)[1]['posture'] == 'pending'
            assert operation(client_token)[0] == 403
            assert read_admin()['peers'][0]['request_id'] == client_hash
            assert action('pairing_approve', client_hash)['posture'] == 'applied'
            assert operation(client_token)[1]['status'] == 'ok'
            assert exchange('GET', '/v1/owner/connections', credential=client_token)[0] == 403
            assert action('pairing_revoke', client_hash)['posture'] == 'applied'
            assert operation(client_token)[0] == 403
            assert exchange('GET', '/v1/pairing/status', credential=client_token)[1]['posture'] == 'revoked'
            assert action('owner_revoke', owner['owner_ref'])['posture'] == 'applied'
            if SDK:
                assert sdk('owner-status', profile['profile_ref'])['posture'] == 'revoked'
                assert sdk('owner-connections', profile['profile_ref'], expected=2)['code'] == 'owner_request_refused'
                assert sdk('owner-get', profile['profile_ref'])['pending_requests'] == []
            else:
                assert exchange('GET', '/v1/owner/status', credential=owner_token)[1]['posture'] == 'revoked'
                assert exchange('GET', '/v1/owner/connections', credential=owner_token)[0] == 403
                assert exchange('POST', '/v1/owner/bootstrap', bootstrap)[1]['posture'] == 'revoked'
            for path in [*state.rglob('*'), *(root / 'profiles').rglob('*')]:
                if path.is_file():
                    content = path.read_bytes()
                    assert bundle['invitation_secret'].encode() not in content
                    assert owner_token.encode() not in content and client_token.encode() not in content
            stop()
            print(json.dumps({'result': 'PASS', 'evidence_class': 'generated_loopback_TLS_headless_owner' +
                ('_canonical_SDK_native_vault_process_restore' if SDK else '_producer_contract'),
                'controls': ['private expiring invitation', 'exact TLS identity', 'invalid invitation refusal',
                    'administration and product scopes separate', 'explicit remote approval', 'stale revision',
                    'durable exact receipt no redispatch', 'client revoke', 'owner revoke', 'no plaintext profile secrets'],
                'operator_state_touched': False}))
        finally:
            for ref in client_profiles:
                sdk('forget', ref)
            for ref in owner_profiles:
                sdk('owner-forget', ref)
            stop()


if __name__ == '__main__':
    main()
