#!/usr/bin/env python3
"""Tiny compiled CPU model over local protocol or optional real SSH/public SDK.

YVEX_SESSION_MANAGEMENT_TRANSPORT=ssh-sdk requires YVEX_SDK_PRODUCT_EXAMPLE.
https-sdk requires YVEX_SDK_CONNECTIONS_EXAMPLE and a native protected vault.
Neither mode establishes real model quality, Spark compatibility, or human acceptance.
"""
import base64
from contextlib import contextmanager, ExitStack
import hashlib
import json
import os
from pathlib import Path
import pwd
import shutil
import select
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
BINARY = Path(os.environ.get('YVEX_BIN', ROOT / 'yvex')).resolve()
COMPILER = Path(os.environ.get('TINY_COMPILER', ROOT / 'build/tests/tiny_compile')).resolve()


@contextmanager
def ssh_sdk_transport(root, env, trust):
    """Authenticated, loopback-only test transport with no operator credential use."""
    executable = Path(os.environ['YVEX_SDK_PRODUCT_EXAMPLE']).resolve()
    assert executable.is_file() and os.access(executable, os.X_OK), 'SDK example unavailable'
    sshd = shutil.which('sshd')
    assert sshd, 'sshd unavailable; SDK lane cannot be qualified'
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    user = pwd.getpwuid(os.getuid()).pw_name
    config = [f'Port {port}', 'ListenAddress 127.0.0.1', f'HostKey {root / "host"}',
              f'AuthorizedKeysFile {trust}', f'PidFile {root / "sshd.pid"}', f'AllowUsers {user}',
              'AuthenticationMethods publickey', 'PubkeyAuthentication yes', 'PasswordAuthentication no',
              'KbdInteractiveAuthentication no', 'UsePAM no', 'GSSAPIAuthentication no', 'StrictModes yes',
              'PermitTTY no', 'DisableForwarding yes', 'PermitUserRC no', 'PermitUserEnvironment no']
    owned_env = ['YVEX_CONFIG_DIR', 'YVEX_DATA_DIR', 'YVEX_MODELS_ROOT', 'YVEX_MODELS_REGISTRY',
                 'XDG_RUNTIME_DIR', 'XDG_DATA_HOME', 'YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES',
                 'YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES', 'YVEX_TEST_RUNTIME_CGROUP_AVAILABLE_MEMORY_BYTES']
    config.append('SetEnv ' + ' '.join(f'{key}={env[key]}' for key in owned_env if key in env))
    (root / 'sshd.conf').write_text('\n'.join(config) + '\n')
    public = (root / 'host.pub').read_text().split()
    (root / 'pins').write_text(f'[127.0.0.1]:{port} ' + ' '.join(public[:2]) + '\n')
    subprocess.run([sshd, '-t', '-f', str(root / 'sshd.conf')], check=True)
    effective = subprocess.run([sshd, '-T', '-f', str(root / 'sshd.conf')], check=True,
                               capture_output=True, text=True).stdout
    forwarded = [item for line in effective.splitlines() if line.lower().startswith('setenv ')
                     for item in line.split()[1:]]
    assert all(f'{key}={env[key]}' in forwarded for key in owned_env if key in env), 'incomplete fixture isolation'
    process = subprocess.Popen([sshd, '-D', '-f', str(root / 'sshd.conf'), '-E', str(root / 'ssh.log')])
    try:
        deadline = time.monotonic() + 5
        while True:
            assert process.poll() is None, 'isolated sshd stopped'
            try:
                with socket.create_connection(('127.0.0.1', port), timeout=.1):
                    break
            except OSError:
                assert time.monotonic() < deadline, 'isolated sshd did not become ready'
                time.sleep(.03)
        def identity(path):
            return 'ssh-ed25519:sha256:' + hashlib.sha256(base64.b64decode(path.read_text().split()[1])).hexdigest()
        yield [str(executable), str(root / 'pins'), str(root / 'peer'), '127.0.0.1', str(port), user,
               identity(root / 'host.pub'), identity(root / 'peer.pub')]
    finally:
        process.terminate()
        process.wait(timeout=5)


@contextmanager
def https_sdk_transport(root,env):
    """Actual native vault, unique generated credential, isolated TLS producer."""
    executable=Path(os.environ['YVEX_SDK_CONNECTIONS_EXAMPLE']).resolve()
    assert executable.is_file(), 'native SDK connections example unavailable'
    env['YVEX_SDK_CONNECTION_PROFILE_ROOT']=str(root/'sdk-profiles')
    process=None;profile_ref=None
    def sdk(*args):
        result=subprocess.run([str(executable),*args],env=env,capture_output=True,text=True,timeout=25)
        assert result.returncode==0,(args[0],result.stdout,result.stderr)
        return json.loads(result.stdout)
    def owner(action,*args):
        result=subprocess.run([str(BINARY),'management',action,*args,'--state-dir',str(root/'network-service')],env=env,capture_output=True,text=True,timeout=25)
        assert result.returncode==0,(action,result.stderr)
        return json.loads(result.stdout)
    try:
        process=subprocess.Popen([str(BINARY),'management','serve','--bind','127.0.0.1:0','--state-dir',str(root/'network-service')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        assert select.select([process.stdout],[],[],10)[0], 'network service startup timeout'
        line=process.stdout.readline();assert line,process.stderr.read();service=json.loads(line)
        candidate=sdk('probe','https://'+service['address'])
        assert candidate['identity']['device_identity']==service['device_identity']
        path=root/'network-candidate.json';path.write_text(json.dumps(candidate))
        profile=sdk('trust',str(path),'SDK tiny CPU fixture','--fingerprint-verified');profile_ref=profile['profile_ref']
        owner('pairing-open');pending=sdk('request',profile_ref);assert pending['posture']=='pending'
        owner('pairing-approve',pending['request_id']);assert sdk('status',profile_ref)['posture']=='approved'
        peer=pending['request_id'];journal=hashlib.sha256(f'yvex.management.peer.v1:credential-sha256:{peer}'.encode()).hexdigest()
        yield [str(executable),'invoke',profile_ref],peer,journal
    finally:
        try:
            if profile_ref:sdk('forget',profile_ref)
        finally:
            if process is not None:process.terminate();process.wait(timeout=15)


def main():
    transport = os.environ.get('YVEX_SESSION_MANAGEMENT_TRANSPORT', 'local')
    assert transport in ['local', 'ssh-sdk', 'https-sdk'], 'unknown qualification transport'
    # The full native socket path must fit Darwin's AF_UNIX extent too.
    with tempfile.TemporaryDirectory(prefix='yvex-management-sessions-', dir=Path('/tmp').resolve()) as directory, ExitStack() as stack:
        root = Path(directory)
        root.chmod(0o700)
        for name in ['config', 'data', 'models', 'runtime', 'xdg', 'bindings']:
            (root / name).mkdir(mode=0o700)
        env = {**os.environ, 'YVEX_CONFIG_DIR': str(root / 'config'), 'YVEX_DATA_DIR': str(root / 'data'),
               'YVEX_MODELS_ROOT': str(root / 'models'), 'XDG_RUNTIME_DIR': str(root / 'runtime'),
               'XDG_DATA_HOME': str(root / 'xdg'), 'YVEX_MODELS_REGISTRY': str(root / 'config/models.json'),
               'NO_COLOR': '1'}
        if env.get('YVEX_TEST_FIXTURE_CAPACITY') == '1':
            for key in ['YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES', 'YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES',
                        'YVEX_TEST_RUNTIME_CGROUP_AVAILABLE_MEMORY_BYTES']:
                assert key not in env, 'Declared test capacity cannot override caller facts.'
            env['YVEX_TEST_RUNTIME_TOTAL_MEMORY_BYTES'] = '137438953472'
            env['YVEX_TEST_RUNTIME_AVAILABLE_MEMORY_BYTES'] = '137438953472'
            print('Declared 128 GiB admission capacity fixture; not physical host memory evidence.', flush=True)
        for name in ['SSH_TTY', 'SSH_ORIGINAL_COMMAND']:
            env.pop(name, None)

        def run(*args, **kwargs):
            result = subprocess.run([str(BINARY), *map(str, args)], env=env,
                                    text=True, capture_output=True, timeout=30, **kwargs)
            assert result.returncode == 0, (args, result.returncode, result.stdout, result.stderr)
            return result.stdout

        artifact = root / 'models/tiny.gguf'
        subprocess.run(['python3', str(ROOT / 'tests/integration/tiny_model.py'), str(artifact)], check=True)
        compiled = subprocess.run([str(COMPILER), str(artifact), str(root / 'bindings')],
                                  env=env, capture_output=True, text=True, timeout=30)
        assert compiled.returncode == 0, compiled.stderr
        # This output belongs only to the test fixture compiler; production management never parses CLI output.
        facts = dict(line.split('=', 1) for line in compiled.stdout.splitlines())
        profile = 'tiny-executable-cpu-complete'
        run('profile', 'create', '--path', artifact, '--alias', profile, '--family', 'tiny',
            '--model', 'executable', '--scope', 'cpu', '--class', 'complete',
            '--runtime-binding', facts['binding_path'], '--target', 'tiny-executable',
            '--backend', 'cpu', '--execution-strategy', 'target-only', '--ctx', '8')
        if transport == 'https-sdk':
            sdk_command,peer,journal=stack.enter_context(https_sdk_transport(root,env))
        else:
            for name in ['host', 'peer']:
                subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', str(root / name)], check=True)
            peer = run('management', 'identity', root / 'peer.pub').strip().rsplit(':', 1)[1]
            trust = root / 'authorized_keys'
            run('management', 'trust-init', trust)
            run('management', 'enroll', root / 'peer.pub', trust, root / 'host.pub', peer,
                '--scope', 'product-management')
            sdk_command = (stack.enter_context(ssh_sdk_transport(root, env, trust))
                           if transport == 'ssh-sdk' else None)
            journal=peer
        if transport == 'local':
            env['SSH_CONNECTION'] = '127.0.0.1 40000 127.0.0.1 40001'
        sequence = 0

        def request(operation, data, identity=None, discard_response=False):
            nonlocal sequence
            sequence += 1
            identity = identity or hashlib.sha256(f'session-control-{sequence}'.encode()).hexdigest()
            message = {'schema': 'yvex.management.request.v2', 'request_id': identity,
                       'operation': operation, 'input': data}
            if sdk_command:
                path = root / 'request.json'
                path.write_text(json.dumps(message))
                result = subprocess.run([*sdk_command,str(path),*(['10000'] if transport=='ssh-sdk' else [])], env=env, text=True,
                                        stdout=subprocess.DEVNULL if discard_response else subprocess.PIPE,
                                        stderr=subprocess.PIPE, timeout=15)
                assert result.returncode == 0, (operation, result.stdout, result.stderr)
                if discard_response:
                    return identity
                reply = json.loads(result.stdout)
                assert reply['request_id'] == identity and reply['authenticated_peer'].endswith(peer), reply
                return reply['value']
            words = ['management', 'product-protocol', root / 'host.pub', peer, trust]
            if discard_response:
                result = subprocess.run([str(BINARY), *map(str, words)], env=env, text=True,
                                        input=json.dumps(message) + '\n', stdout=subprocess.DEVNULL,
                                        stderr=subprocess.PIPE, timeout=30)
                assert result.returncode == 0, result.stderr
                return identity
            reply = json.loads(run(*words, input=json.dumps(message) + '\n'))
            assert reply['status'] == 'ok', reply
            return reply['data']

        def wait_job(identity):
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                observed = request('job.get', {'job_id': identity})
                if observed['state'] not in ['accepted', 'running']:
                    return observed
                time.sleep(.03)
            raise AssertionError(('job timeout', observed))

        def job(operation, data, expected='succeeded'):
            observed = wait_job(request(operation, data)['job_id'])
            assert observed['state'] == expected, observed
            return observed

        host_process = None
        host_log = (root / 'host.log').open('w+')

        def start_host():
            nonlocal host_process
            with socket.socket() as reservation:
                reservation.bind(('127.0.0.1', 0))
                port = reservation.getsockname()[1]
            host_process = subprocess.Popen([str(BINARY), 'serve', '--workers', '2', '--openai-port', str(port)],
                                            env=env, stdout=host_log, stderr=host_log)
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                posture = request('host.get', {})
                if posture['state'] == 'running':
                    return posture['host_instance']
                assert host_process.poll() is None, (host_process.returncode, (root / 'host.log').read_text())
                time.sleep(.05)
            raise AssertionError(('host timeout', posture, (root / 'host.log').read_text()))

        def stop_host():
            nonlocal host_process
            if host_process is not None and host_process.poll() is None:
                run('host', 'stop')
                host_process.wait(timeout=15)
            host_process = None

        try:
            instance = start_host()
            assert request('engine.list', {})['engines'] == []
            engine = job('engine.load', {'host_instance': instance, 'profile': profile, 'context_capacity': 8})['result']['engine']
            assert engine['execution_ready'] and engine['engine_kind'] == 'text', engine
            routing = {'host_instance': instance, 'model': engine['alias'], 'generation': engine['generation']}
            old = job('session.create', {**routing, 'name': 'test'})['result']['session']
            exact = {**routing, 'name': 'test', 'session_identity': old['identity']}
            generated_input = {**exact, 'prompt': 'a', 'maximum_new_tokens': 1, 'reasoning': 'disabled'}
            lost_id = hashlib.sha256(b'lost-generation-reply').hexdigest()
            request('generation.start', generated_input, identity=lost_id, discard_response=True)
            generated = wait_job(lost_id)
            assert (root / 'xdg/yvex/management-jobs-v2' / journal / f'{lost_id}.json').is_file(), \
                'receipt escaped the declared fixture data root'
            assert generated['state'] == 'succeeded', generated
            assert generated['result']['complete'] and generated['result']['metrics']['generated_tokens'] == 1, generated
            assert generated['result']['host_instance'] == instance, generated
            assert generated['result']['model'] == engine['alias'], generated
            assert generated['result']['generation'] == engine['generation'], generated
            assert isinstance(generated['result']['channels'], list), generated
            for channel in generated['result']['channels']:
                assert channel['channel'] in ['final_text', 'public_reasoning', 'tool_call', 'tool_result', 'control', 'error']
                assert isinstance(channel['text'], str)
            print('Typed engine, Session and generation receipt/result accepted.', flush=True)
            assert request('generation.start', generated_input, identity=lost_id)['job_id'] == lost_id
            current = request('session.get', {'model': engine['alias'], 'generation': engine['generation'], 'name': 'test'})['session']
            assert current['turns'] == 1 and current['identity'] == old['identity'], current
            forked = job('session.fork', {**exact, 'fork_name': 'forked', 'maximum_prefix_bytes': 16 * 1024 * 1024})['result']['session']
            assert forked['identity'] != old['identity'], forked
            job('session.reset', exact)
            reset = request('session.get', {'model': engine['alias'], 'generation': engine['generation'], 'name': 'test'})['session']
            assert reset['turns'] == 0, reset
            # Idle cancellation reports its real owner result; active cancellation is qualified in native unit.server.
            cancellation = wait_job(request('generation.cancel', exact)['job_id'])
            assert cancellation['state'] in ['succeeded', 'failed'], cancellation
            job('session.close', exact)
            replacement = job('session.create', {**routing, 'name': 'test'})['result']['session']
            assert replacement['identity'] != old['identity'], replacement
            for operation in ['session.close', 'session.reset', 'generation.cancel']:
                stale = job(operation, exact, expected='failed')
                assert 'identity' in stale['reason'] or 'stale' in stale['reason'] or (operation == 'generation.cancel' and 'no active turn' in stale['reason']), stale
            fresh = {**routing, 'name': 'test', 'session_identity': replacement['identity']}
            same = request('session.get', {'model': engine['alias'], 'generation': engine['generation'], 'name': 'test'})['session']
            assert same['identity'] == replacement['identity'] and same['turns'] == 0, same
            job('session.close', fresh)
            job('session.close', {**routing, 'name': 'forked', 'session_identity': forked['identity']})
            events = request('observe.events', {'limit': 32})
            assert isinstance(events['events'], list)
            job('engine.unload', routing)
            stop_host()
            restarted = start_host()
            assert restarted != instance
            stale_host = job('engine.load', {'host_instance': instance, 'profile': profile}, expected='failed')
            assert 'stale_host_instance' in stale_host['reason'], stale_host
            assert request('engine.list', {})['engines'] == []
            evidence = {'https-sdk':'real-isolated-TLS-public-SDK-native-vault-tiny-CPU','ssh-sdk':'real-isolated-SSH-public-SDK-native-tiny-CPU','local':'real-native-tiny-CPU-over-controlled-local-management-protocol'}[transport]
            print(json.dumps({'evidence': evidence,
                              'load_generate_fork_reset_close_unload': 'pass',
                              'lost_acknowledgement': 'same receipt; exactly one turn',
                              'session_lifetime_fence': 'close-recreate then stale reset/close/cancel refused',
                              'host_restart_fence': 'old instance refused',
                              'idle_cancel_posture': cancellation['state'],
                              'real_ssh': transport=='ssh-sdk', 'real_tls':transport=='https-sdk', 'public_sdk_typed_validation': bool(sdk_command),
                              'model_fixture': 'tiny compiled CPU; declared admission capacity when requested',
                              'response_loss': 'caller deliberately discards one submission response; observes exact receipt',
                              'real_model_quality': False, 'operator_state_modified': False}))
        finally:
            try:
                stop_host()
            finally:
                if host_process is not None and host_process.poll() is None:
                    host_process.terminate()
                    host_process.wait(timeout=10)
                host_log.close()


if __name__ == '__main__':
    main()
